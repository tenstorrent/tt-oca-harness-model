// SPDX-License-Identifier: Apache-2.0
/**
 * @file telemetry_receiver_tb.cpp
 * @brief Primary self-checking test bench for the SMC Telemetry Receiver model.
 *
 * Exercises: reset values, 32-bit register RW/RO/masking semantics, ATB beat
 * assembly and message decode (against a hand-derived beat stream, so the
 * decoder is not validated by its own encoder), per-counter valid bits,
 * multi-packet messages, message-queue FIFO ordering, drop-oldest overflow,
 * BUFFER_POP, threshold and missing-last interrupts, INTR_TEST forcing, RX/TX
 * flush, reset behaviour, transport_dbg peeks, and CCI introspection.
 */

#include "telemetry_receiver.h"

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {

int g_failures = 0;

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        auto _e = (expected);                                                  \
        auto _a = (actual);                                                    \
        if (_e != _a) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " #actual " expected=0x" << std::hex               \
                      << static_cast<uint64_t>(_e) << " got=0x"                \
                      << static_cast<uint64_t>(_a) << std::dec << "\n";        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

using cfg = smc::telemetry_receiver_cfg;

// Register offsets (telemetry_receiver.rdl).
constexpr uint64_t CTRL = 0x00, STATUS = 0x04, INTR_STATUS = 0x08;
constexpr uint64_t INTR_ENABLE = 0x0C, INTR_TEST = 0x10;
constexpr uint64_t PROBE_ID = 0x14, COUNTER_VLDS = 0x18, COUNTER0 = 0x80;

/// Counter register @p i.
constexpr uint64_t counter_reg(unsigned i) { return COUNTER0 + 4u * i; }

// Geometry chosen by the CCI presets in sc_main().
constexpr unsigned DEPTH        = 4; // tb.tel.buffer_depth
constexpr unsigned MAX_COUNTERS = 4; // default max_counters_per_message

// Tiny 32-bit TLM driver exercising the LT path through a quantum keeper.
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver, 32> sock;
    tlm_utils::tlm_quantumkeeper qk;

    explicit driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEFu;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write32(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << addr << ", 0x" << value
                      << ") rsp=" << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    unsigned dbg_read(uint64_t addr, uint32_t& out) {
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&out));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

struct tb : sc_core::sc_module {
    // Main DUT: 4 counters per message (3 ATB packets), 4-deep queue.
    driver                  drv;
    smc::telemetry_receiver dut;
    // Second DUT: 1 counter per message => exactly one ATB packet, so a beat
    // stream can be derived by hand and hard-coded as an oracle.
    driver                  drv1;
    smc::telemetry_receiver dut1;

    sc_core::sc_signal<bool>     rstn{"rstn"}, afready{"afready"};
    sc_core::sc_signal<bool>     irq{"irq"}, afvalid{"afvalid"}, atready{"atready"};
    sc_core::sc_signal<uint32_t> debug{"debug"};

    sc_core::sc_signal<bool>     rstn1{"rstn1"}, afready1{"afready1"};
    sc_core::sc_signal<bool>     irq1{"irq1"}, afvalid1{"afvalid1"}, atready1{"atready1"};
    sc_core::sc_signal<uint32_t> debug1{"debug1"};

    SC_HAS_PROCESS(tb);

    explicit tb(sc_core::sc_module_name n)
        : sc_module(n), drv("drv"), dut("tel"), drv1("drv1"), dut1("tel1")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rstn);
        dut.afready_i(afready);
        dut.irq_o(irq);
        dut.afvalid_o(afvalid);
        dut.atready_o(atready);
        dut.debug_o(debug);

        drv1.sock.bind(dut1.reg_socket);
        dut1.rst_n_i(rstn1);
        dut1.afready_i(afready1);
        dut1.irq_o(irq1);
        dut1.afvalid_o(afvalid1);
        dut1.atready_o(atready1);
        dut1.debug_o(debug1);

        SC_THREAD(run);
    }

    // Let SC_METHODs (reset / recompute) run and signal updates propagate.
    void settle() { sc_core::wait(sc_time(1, SC_NS)); }

    void do_reset() {
        rstn.write(false);
        rstn1.write(false);
        settle();
        rstn.write(true);
        rstn1.write(true);
        settle();
    }

    /// Queue one message on `dut` via the reference encoder.
    void send(uint8_t probe_id, const std::vector<uint32_t>& values,
              bool last = true) {
        std::vector<smc::telemetry_counter_value> c;
        for (uint32_t v : values) c.push_back({true, v});
        dut.push_atb_beats(
            smc::telemetry_encode_message(probe_id, c, MAX_COUNTERS, last));
        settle();
    }

    void run();
};

void tb::run()
{
    // ----------------------------------------------------------------------
    // 1. Reset values
    // ----------------------------------------------------------------------
    afready.write(false);
    afready1.write(false);
    do_reset();
    EXPECT_EQ(0u,   drv.read32(CTRL));
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, drv.read32(STATUS)); // empty at reset
    EXPECT_EQ(0u,   drv.read32(INTR_STATUS));
    EXPECT_EQ(0u,   drv.read32(INTR_ENABLE));
    EXPECT_EQ(0u,   drv.read32(INTR_TEST));
    EXPECT_EQ(0u,   drv.read32(PROBE_ID));
    EXPECT_EQ(0u,   drv.read32(COUNTER_VLDS));
    EXPECT_EQ(0u,   drv.read32(counter_reg(0)));
    EXPECT_TRUE(!irq.read());
    EXPECT_TRUE(!afvalid.read());
    EXPECT_TRUE(atready.read());                       // ready once out of reset
    EXPECT_EQ(cfg::DBG_BUFFER_EMPTY, debug.read());
    EXPECT_EQ(0u, dut.fill_level());
    std::cout << "  [PASS] reset values\n";

    // ----------------------------------------------------------------------
    // 2. Register RW / RO / masking
    // ----------------------------------------------------------------------
    drv.write32(CTRL, cfg::CTRL_THRESHOLD_MASK);       // all threshold bits
    EXPECT_EQ(cfg::CTRL_THRESHOLD_MASK, drv.read32(CTRL));
    drv.write32(CTRL, 0u);
    // The two pulse bits are write-only: they never read back.
    drv.write32(CTRL, cfg::CTRL_BUFFER_POP | cfg::CTRL_RX_FLUSH);
    EXPECT_EQ(0u, drv.read32(CTRL));

    drv.write32(INTR_ENABLE, 0xFFFFFFFFu);
    EXPECT_EQ(cfg::INTR_MASK, drv.read32(INTR_ENABLE)); // reserved bits masked
    drv.write32(INTR_ENABLE, 0u);
    // INTR_TEST holds only the level bit; MISSING_LAST is a write pulse.
    drv.write32(INTR_TEST, 0xFFFFFFFFu);
    EXPECT_EQ(cfg::INTR_BUFFER_THRESHOLD, drv.read32(INTR_TEST));
    drv.write32(INTR_TEST, 0u);

    // Hardware-driven registers ignore writes.
    drv.write32(STATUS, 0xFFFFFFFFu);
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, drv.read32(STATUS));
    drv.write32(PROBE_ID, 0x1Fu);
    EXPECT_EQ(0u, drv.read32(PROBE_ID));
    drv.write32(COUNTER_VLDS, 0xFFFFFFFFu);
    EXPECT_EQ(0u, drv.read32(COUNTER_VLDS));
    drv.write32(counter_reg(2), 0xFFFFFFFFu);
    EXPECT_EQ(0u, drv.read32(counter_reg(2)));
    std::cout << "  [PASS] register RW/RO/masking\n";

    // ----------------------------------------------------------------------
    // 3. Hand-derived beat stream (independent decode oracle) on dut1
    //
    // One counter per message => one 64-bit packet => 8 beats.  The expected
    // beats below were derived by hand from telemetry_receiver_pkg.sv for
    // probe_id=0x15, counter0=0xDEADBEEF (valid):
    //   bit63 last_packet=1; blocks[0].data = probe_id<<2 (packet[60:56]);
    //   blocks[1..4] = 0xDE,0xAD,0xBE,0xEF (MSB byte first), all valid.
    //   => packet word 0xD53BDADDF7BC0000
    // ----------------------------------------------------------------------
    const std::vector<uint8_t> oracle_beats = {0x00, 0x00, 0xBC, 0xF7,
                                               0xDD, 0xDA, 0x3B, 0xD5};

    // The reference encoder must reproduce the hand-derived stream exactly.
    {
        std::vector<smc::telemetry_counter_value> c{{true, 0xDEADBEEFu}};
        const std::vector<uint8_t> enc =
            smc::telemetry_encode_message(0x15, c, /*max_counters=*/1);
        EXPECT_EQ(oracle_beats.size(), enc.size());
        for (size_t i = 0; i < oracle_beats.size() && i < enc.size(); ++i)
            EXPECT_EQ(oracle_beats[i], enc[i]);
    }

    // Decode the hand-derived stream (not the encoder's output).
    EXPECT_EQ(8u, dut1.push_atb_beats(oracle_beats));
    settle();
    EXPECT_EQ(1u,    dut1.fill_level());
    EXPECT_EQ(0u,    drv1.read32(STATUS));             // no longer empty
    EXPECT_EQ(0x15u, drv1.read32(PROBE_ID));
    EXPECT_EQ(0x1u,  drv1.read32(COUNTER_VLDS));       // one counter, valid
    EXPECT_EQ(0xDEADBEEFu, drv1.read32(counter_reg(0)));
    EXPECT_EQ(0u,    drv1.read32(counter_reg(1)));     // above max_counters
    std::cout << "  [PASS] hand-derived beat stream decode + encoder agreement\n";

    // ----------------------------------------------------------------------
    // 4. Multi-packet message (4 counters => 3 packets) on dut
    // ----------------------------------------------------------------------
    do_reset();
    EXPECT_EQ(24u, smc::telemetry_encode_message(
                       0, {}, MAX_COUNTERS).size());   // 3 packets x 8 beats
    send(0x0A, {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u});
    EXPECT_EQ(1u,    dut.fill_level());
    EXPECT_EQ(0x0Au, drv.read32(PROBE_ID));
    EXPECT_EQ(0xFu,  drv.read32(COUNTER_VLDS));
    EXPECT_EQ(0x11111111u, drv.read32(counter_reg(0)));
    EXPECT_EQ(0x22222222u, drv.read32(counter_reg(1)));
    EXPECT_EQ(0x33333333u, drv.read32(counter_reg(2)));
    EXPECT_EQ(0x44444444u, drv.read32(counter_reg(3)));
    // Counter registers above max_counters_per_message are tied off.
    EXPECT_EQ(0u, drv.read32(counter_reg(4)));
    EXPECT_EQ(0u, drv.read32(counter_reg(cfg::NUM_COUNTER_REGS - 1)));
    std::cout << "  [PASS] multi-packet message decode\n";

    // ----------------------------------------------------------------------
    // 5. Per-counter valid bits: an incomplete counter reads back as zero
    // ----------------------------------------------------------------------
    do_reset();
    {
        std::vector<smc::telemetry_counter_value> c{{true,  0xAAAAAAAAu},
                                                    {false, 0xBBBBBBBBu},
                                                    {true,  0xCCCCCCCCu},
                                                    {false, 0xDDDDDDDDu}};
        dut.push_atb_beats(smc::telemetry_encode_message(0x1F, c, MAX_COUNTERS));
        settle();
    }
    EXPECT_EQ(0x1Fu, drv.read32(PROBE_ID));            // full 5-bit probe id
    EXPECT_EQ(0x5u,  drv.read32(COUNTER_VLDS));        // counters 0 and 2 only
    EXPECT_EQ(0xAAAAAAAAu, drv.read32(counter_reg(0)));
    EXPECT_EQ(0u,          drv.read32(counter_reg(1))); // invalid -> zeroed
    EXPECT_EQ(0xCCCCCCCCu, drv.read32(counter_reg(2)));
    EXPECT_EQ(0u,          drv.read32(counter_reg(3)));
    std::cout << "  [PASS] per-counter valid bits\n";

    // ----------------------------------------------------------------------
    // 6. Queue FIFO order, BUFFER_POP, STATUS empty/full
    // ----------------------------------------------------------------------
    do_reset();
    for (unsigned m = 1; m <= DEPTH; ++m)
        send(static_cast<uint8_t>(m), {0x1000u + m, 0u, 0u, 0u});
    EXPECT_EQ(DEPTH, dut.fill_level());
    EXPECT_EQ(cfg::STATUS_BUFFER_FULL, drv.read32(STATUS));
    EXPECT_EQ(cfg::DBG_BUFFER_FULL, debug.read());
    // Messages come out oldest-first.
    for (unsigned m = 1; m <= DEPTH; ++m) {
        EXPECT_EQ(m, drv.read32(PROBE_ID));
        EXPECT_EQ(0x1000u + m, drv.read32(counter_reg(0)));
        drv.write32(CTRL, cfg::CTRL_BUFFER_POP);
        settle();
    }
    EXPECT_EQ(0u, dut.fill_level());
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, drv.read32(STATUS));
    EXPECT_EQ(0u, drv.read32(PROBE_ID));               // empty view reads zero
    EXPECT_EQ(0u, drv.read32(COUNTER_VLDS));
    std::cout << "  [PASS] FIFO order / BUFFER_POP / STATUS\n";

    // ----------------------------------------------------------------------
    // 7. Overflow drops the OLDEST message (newest telemetry survives)
    // ----------------------------------------------------------------------
    do_reset();
    for (unsigned m = 1; m <= DEPTH + 2; ++m)
        send(static_cast<uint8_t>(m), {0x2000u + m, 0u, 0u, 0u});
    EXPECT_EQ(DEPTH, dut.fill_level());                // capped at depth
    EXPECT_EQ(3u,    drv.read32(PROBE_ID));            // 1 and 2 were dropped
    EXPECT_EQ(0x2003u, drv.read32(counter_reg(0)));
    std::cout << "  [PASS] drop-oldest overflow\n";

    // ----------------------------------------------------------------------
    // 8. BUFFER_THRESHOLD interrupt (level) + INTR_STATUS mirror
    // ----------------------------------------------------------------------
    do_reset();
    drv.write32(CTRL, 1u << cfg::CTRL_THRESHOLD_SHIFT); // threshold = 1
    drv.write32(INTR_ENABLE, cfg::INTR_BUFFER_THRESHOLD);
    settle();
    EXPECT_TRUE(!irq.read());
    send(0x01, {1u, 0u, 0u, 0u});                       // fill = 1, not > 1
    EXPECT_TRUE(!irq.read());
    send(0x02, {2u, 0u, 0u, 0u});                       // fill = 2 > 1
    EXPECT_TRUE(irq.read());
    EXPECT_EQ(cfg::INTR_BUFFER_THRESHOLD, drv.read32(INTR_STATUS));
    // The status bit is read-only: W1C does not clear a level source.
    drv.write32(INTR_STATUS, cfg::INTR_BUFFER_THRESHOLD);
    settle();
    EXPECT_TRUE(irq.read());
    // Popping must re-write the threshold field: CTRL is one register, so a
    // bare BUFFER_POP write would also zero the threshold and keep the
    // interrupt asserted (fill=1 > 0).
    drv.write32(CTRL, cfg::CTRL_BUFFER_POP);            // clobbers threshold
    settle();
    EXPECT_TRUE(irq.read());                            // still on: threshold=0
    drv.write32(CTRL, (1u << cfg::CTRL_THRESHOLD_SHIFT) | cfg::CTRL_BUFFER_POP);
    settle();
    EXPECT_EQ(0u, dut.fill_level());                    // both pops took effect
    EXPECT_TRUE(!irq.read());
    EXPECT_EQ(0u, drv.read32(INTR_STATUS));
    send(0x03, {3u, 0u, 0u, 0u});                       // fill = 1, threshold 1
    EXPECT_TRUE(!irq.read());
    // Disabling the enable masks the interrupt even while over threshold.
    send(0x04, {4u, 0u, 0u, 0u});                       // fill = 2 > 1
    EXPECT_TRUE(irq.read());
    drv.write32(INTR_ENABLE, 0u);
    settle();
    EXPECT_TRUE(!irq.read());
    std::cout << "  [PASS] BUFFER_THRESHOLD interrupt\n";

    // ----------------------------------------------------------------------
    // 9. INTR_TEST.BUFFER_THRESHOLD forces the interrupt (level, releasable)
    // ----------------------------------------------------------------------
    do_reset();
    drv.write32(INTR_ENABLE, cfg::INTR_BUFFER_THRESHOLD);
    drv.write32(INTR_TEST, cfg::INTR_BUFFER_THRESHOLD);
    settle();
    EXPECT_TRUE(irq.read());                            // forced with empty queue
    EXPECT_EQ(cfg::INTR_BUFFER_THRESHOLD, drv.read32(INTR_STATUS));
    drv.write32(INTR_TEST, 0u);                         // writing 0 releases it
    settle();
    EXPECT_TRUE(!irq.read());
    std::cout << "  [PASS] INTR_TEST.BUFFER_THRESHOLD\n";

    // ----------------------------------------------------------------------
    // 10. Missing-last event: assembly buffer fills with no last_packet marker
    // ----------------------------------------------------------------------
    do_reset();
    drv.write32(INTR_ENABLE, cfg::INTR_MISSING_LAST);
    settle();
    send(0x07, {0xF00Du, 0u, 0u, 0u}, /*last=*/false);
    EXPECT_EQ(0u, dut.fill_level());                    // partial msg discarded
    EXPECT_TRUE(irq.read());
    EXPECT_EQ(cfg::INTR_MISSING_LAST, drv.read32(INTR_STATUS));
    EXPECT_TRUE((debug.read() & cfg::DBG_MISSING_LAST) != 0);
    EXPECT_TRUE((debug.read() & cfg::DBG_ASSEMBLY_FULL) != 0);
    // Sticky: W1C clears it.
    drv.write32(INTR_STATUS, cfg::INTR_MISSING_LAST);
    settle();
    EXPECT_TRUE(!irq.read());
    EXPECT_EQ(0u, drv.read32(INTR_STATUS));
    // The receiver recovers: the next well-formed message decodes normally.
    send(0x08, {0x1234u, 0u, 0u, 0u});
    EXPECT_EQ(1u,    dut.fill_level());
    EXPECT_EQ(0x08u, drv.read32(PROBE_ID));
    EXPECT_EQ(0x1234u, drv.read32(counter_reg(0)));
    std::cout << "  [PASS] missing-last event + recovery\n";

    // ----------------------------------------------------------------------
    // 11. Missing-last is gated by INTR_ENABLE at the moment of the event
    // ----------------------------------------------------------------------
    do_reset();
    drv.write32(INTR_ENABLE, 0u);
    send(0x09, {1u, 0u, 0u, 0u}, /*last=*/false);
    EXPECT_TRUE(!irq.read());
    EXPECT_EQ(0u, drv.read32(INTR_STATUS));             // status gated off
    EXPECT_TRUE((debug.read() & cfg::DBG_MISSING_LAST) != 0); // event still seen
    std::cout << "  [PASS] missing-last enable gating\n";

    // ----------------------------------------------------------------------
    // 12. INTR_TEST.MISSING_LAST pulse forces the sticky status
    // ----------------------------------------------------------------------
    do_reset();
    drv.write32(INTR_TEST, cfg::INTR_MISSING_LAST);      // enable still clear
    settle();
    EXPECT_EQ(0u, drv.read32(INTR_STATUS));
    drv.write32(INTR_ENABLE, cfg::INTR_MISSING_LAST);
    drv.write32(INTR_TEST, cfg::INTR_MISSING_LAST);
    settle();
    EXPECT_EQ(cfg::INTR_MISSING_LAST, drv.read32(INTR_STATUS));
    EXPECT_TRUE(irq.read());
    EXPECT_EQ(0u, drv.read32(INTR_TEST));                // pulse does not latch
    drv.write32(INTR_STATUS, cfg::INTR_MISSING_LAST);
    settle();
    EXPECT_TRUE(!irq.read());
    std::cout << "  [PASS] INTR_TEST.MISSING_LAST\n";

    // ----------------------------------------------------------------------
    // 13. RX flush discards the queue AND a partially assembled message
    // ----------------------------------------------------------------------
    do_reset();
    send(0x11, {0xAAAAu, 0u, 0u, 0u});
    send(0x12, {0xBBBBu, 0u, 0u, 0u});
    EXPECT_EQ(2u, dut.fill_level());
    // Leave one packet of a 3-packet message in the assembly buffer.
    {
        const std::vector<uint8_t> beats =
            smc::telemetry_encode_message(0x13, {}, MAX_COUNTERS);
        for (unsigned i = 0; i < cfg::BEATS_PER_PACKET; ++i)
            dut.push_atb_beat(beats[i]);
        settle();
    }
    drv.write32(CTRL, cfg::CTRL_RX_FLUSH);
    settle();
    EXPECT_EQ(0u, dut.fill_level());
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, drv.read32(STATUS));
    // The stale beats are gone, so the next message decodes cleanly.
    send(0x14, {0x5555u, 0u, 0u, 0u});
    EXPECT_EQ(1u,    dut.fill_level());
    EXPECT_EQ(0x14u, drv.read32(PROBE_ID));
    EXPECT_EQ(0x5555u, drv.read32(counter_reg(0)));
    std::cout << "  [PASS] RX flush\n";

    // ----------------------------------------------------------------------
    // 14. TX flush handshake over the ATB AF channel
    // ----------------------------------------------------------------------
    do_reset();
    afready.write(false);
    settle();
    drv.write32(CTRL, cfg::CTRL_TX_FLUSH);
    settle();
    EXPECT_TRUE(afvalid.read());                         // request raised
    EXPECT_EQ(cfg::CTRL_TX_FLUSH, drv.read32(CTRL));     // bit is readable
    afready.write(true);                                 // transmitter acks
    settle();
    EXPECT_TRUE(!afvalid.read());                        // request retired
    EXPECT_EQ(0u, drv.read32(CTRL));                     // self-cleared
    // With afready already high the flush retires immediately.
    drv.write32(CTRL, cfg::CTRL_TX_FLUSH);
    settle();
    EXPECT_TRUE(!afvalid.read());
    EXPECT_EQ(0u, drv.read32(CTRL));
    afready.write(false);
    settle();
    std::cout << "  [PASS] TX flush handshake\n";

    // ----------------------------------------------------------------------
    // 15. Reset clears loaded state; beats are refused while in reset
    // ----------------------------------------------------------------------
    send(0x15, {0x9999u, 0u, 0u, 0u});
    drv.write32(INTR_ENABLE, cfg::INTR_MASK);
    EXPECT_EQ(1u, dut.fill_level());
    rstn.write(false);
    settle();
    EXPECT_TRUE(!atready.read());                        // not ready in reset
    EXPECT_TRUE(!dut.push_atb_beat(0xFF));               // beat refused
    EXPECT_EQ(0u, dut.push_atb_beats({0x01, 0x02}));
    rstn.write(true);
    settle();
    EXPECT_TRUE(atready.read());
    EXPECT_EQ(0u, dut.fill_level());
    EXPECT_EQ(0u, drv.read32(INTR_ENABLE));              // registers cleared
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, drv.read32(STATUS));
    std::cout << "  [PASS] reset clears state / refuses beats\n";

    // ----------------------------------------------------------------------
    // 16. transport_dbg and dbg_reg peeks are side-effect-free
    // ----------------------------------------------------------------------
    do_reset();
    send(0x16, {0x4242u, 0u, 0u, 0u});
    {
        uint32_t v = 0;
        EXPECT_EQ(4u, drv.dbg_read(PROBE_ID, v));
        EXPECT_EQ(0x16u, v);
        EXPECT_EQ(4u, drv.dbg_read(counter_reg(0), v));
        EXPECT_EQ(0x4242u, v);
        // Peeking must not disturb the queue.
        EXPECT_EQ(1u,    dut.fill_level());
        EXPECT_EQ(0x16u, drv.read32(PROBE_ID));
        EXPECT_EQ(0x16u, dut.dbg_reg(PROBE_ID));
        EXPECT_EQ(0u,    dut.dbg_reg(0x1C)); // unmapped offset -> 0
    }
    std::cout << "  [PASS] transport_dbg / dbg_reg peek\n";

    // ----------------------------------------------------------------------
    // 17. CCI introspection
    // ----------------------------------------------------------------------
    {
        auto broker = cci::cci_get_broker();
        auto h_depth = broker.get_param_handle("tb.tel.buffer_depth");
        auto h_delay = broker.get_param_handle("tb.tel.access_delay_ns");
        auto h_cnt1  = broker.get_param_handle("tb.tel1.max_counters_per_message");
        EXPECT_TRUE(h_depth.is_valid());
        EXPECT_TRUE(h_delay.is_valid());
        EXPECT_TRUE(h_cnt1.is_valid());
        EXPECT_EQ(DEPTH, h_depth.get_cci_value().get_uint());
        EXPECT_TRUE(h_depth.is_preset_value());
        EXPECT_EQ(1u, h_cnt1.get_cci_value().get_uint());
        EXPECT_TRUE(h_delay.get_cci_value().get_double() == 3.0); // preset
        h_delay.set_cci_value(cci::cci_value(5.0));               // mutable
        EXPECT_TRUE(h_delay.get_cci_value().get_double() == 5.0);

        std::cout << "  CCI parameters:\n";
        for (auto& h : broker.get_param_handles()) {
            std::cout << "    " << std::left << std::setw(38) << h.name()
                      << " = " << std::setw(8) << h.get_cci_value().to_json()
                      << (h.is_preset_value() ? " [preset]" : " [default]") << "\n";
        }
        std::cout << "  [PASS] CCI introspection\n";
    }

    dut.dump_state(std::cout);

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    // Static, not heap: the broker must outlive every cci_param (so it cannot
    // be a plain local), but a `new` that is never deleted is a LeakSanitizer
    // finding under --asan, along with everything libcci allocates behind it.
    static cci_utils::consuming_broker broker_impl("GlobalBroker");
    cci::cci_register_broker(&broker_impl);

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.tel.buffer_depth",
                                       cci::cci_value(DEPTH));
    global_broker.set_preset_cci_value("tb.tel.access_delay_ns",
                                       cci::cci_value(3.0));
    // Single-counter geometry => one ATB packet per message.
    global_broker.set_preset_cci_value("tb.tel1.max_counters_per_message",
                                       cci::cci_value(1u));
    global_broker.set_preset_cci_value("tb.tel1.buffer_depth",
                                       cci::cci_value(2u));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
