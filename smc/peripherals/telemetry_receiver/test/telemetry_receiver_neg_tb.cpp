// SPDX-License-Identifier: Apache-2.0
/**
 * @file telemetry_receiver_neg_tb.cpp
 * @brief Negative-path / edge-case bench for the SMC Telemetry Receiver model.
 *
 * Exercises: TLM error responses (command / burst / address / in-window decode
 * miss), transport_dbg accept and reject paths, read-only write-ignore,
 * BUFFER_POP on an empty queue, BUFFER_THRESHOLD field truncation at the
 * message-buffer pointer width, and the extreme CCI geometries (minimum
 * 2-message queue, maximum 32 counters per message => 19 ATB packets).
 */

#include "telemetry_receiver.h"

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <iostream>
#include <vector>

using sc_core::sc_time;
using sc_core::SC_NS;

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

constexpr uint64_t CTRL = 0x00, STATUS = 0x04, INTR_STATUS = 0x08;
constexpr uint64_t INTR_ENABLE = 0x0C, PROBE_ID = 0x14, COUNTER_VLDS = 0x18;
constexpr uint64_t COUNTER0 = 0x80;

constexpr uint64_t counter_reg(unsigned i) { return COUNTER0 + 4u * i; }

/// Raw driver: returns the TLM response instead of failing the test, so error
/// paths can be asserted on.
struct raw_driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<raw_driver, 32> sock;

    explicit raw_driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {}

    tlm::tlm_response_status access(tlm::tlm_command cmd, uint64_t addr,
                                    uint32_t len, uint32_t& data) {
        tlm::tlm_generic_payload gp;
        sc_time t = sc_core::SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    tlm::tlm_response_status read (uint64_t a, uint32_t& d) {
        return access(tlm::TLM_READ_COMMAND, a, 4, d);
    }
    tlm::tlm_response_status write(uint64_t a, uint32_t d) {
        return access(tlm::TLM_WRITE_COMMAND, a, 4, d);
    }

    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, uint32_t len, uint32_t& data) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

struct tb : sc_core::sc_module {
    // Minimum queue depth (2) with a single counter per message.
    raw_driver              drv;
    smc::telemetry_receiver dut;
    // Maximum counters per message (32) => 19 ATB packets per message.
    raw_driver              drvB;
    smc::telemetry_receiver dutB;

    sc_core::sc_signal<bool>     rstn{"rstn"}, afready{"afready"};
    sc_core::sc_signal<bool>     irq{"irq"}, afvalid{"afvalid"}, atready{"atready"};
    sc_core::sc_signal<uint32_t> debug{"debug"};

    sc_core::sc_signal<bool>     rstnB{"rstnB"}, afreadyB{"afreadyB"};
    sc_core::sc_signal<bool>     irqB{"irqB"}, afvalidB{"afvalidB"}, atreadyB{"atreadyB"};
    sc_core::sc_signal<uint32_t> debugB{"debugB"};

    SC_HAS_PROCESS(tb);

    explicit tb(sc_core::sc_module_name n)
        : sc_module(n), drv("drv"), dut("telmin"), drvB("drvB"), dutB("telmax")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rstn);
        dut.afready_i(afready);
        dut.irq_o(irq);
        dut.afvalid_o(afvalid);
        dut.atready_o(atready);
        dut.debug_o(debug);

        drvB.sock.bind(dutB.reg_socket);
        dutB.rst_n_i(rstnB);
        dutB.afready_i(afreadyB);
        dutB.irq_o(irqB);
        dutB.afvalid_o(afvalidB);
        dutB.atready_o(atreadyB);
        dutB.debug_o(debugB);

        SC_THREAD(run);
    }

    void settle() { sc_core::wait(sc_time(1, SC_NS)); }

    void do_reset() {
        rstn.write(false);
        rstnB.write(false);
        settle();
        rstn.write(true);
        rstnB.write(true);
        settle();
    }

    void run();
};

void tb::run()
{
    afready.write(false);
    afreadyB.write(false);
    do_reset();

    // ----------------------------------------------------------------------
    // 1. TLM command error: neither read nor write
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        EXPECT_TRUE(drv.access(tlm::TLM_IGNORE_COMMAND, CTRL, 4, d) ==
                    tlm::TLM_COMMAND_ERROR_RESPONSE);
    }
    std::cout << "  [PASS] TLM command error\n";

    // ----------------------------------------------------------------------
    // 2. Burst error: registers are 32-bit only
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        EXPECT_TRUE(drv.access(tlm::TLM_READ_COMMAND,  CTRL, 8, d) ==
                    tlm::TLM_BURST_ERROR_RESPONSE);
        EXPECT_TRUE(drv.access(tlm::TLM_WRITE_COMMAND, CTRL, 2, d) ==
                    tlm::TLM_BURST_ERROR_RESPONSE);
        EXPECT_TRUE(drv.access(tlm::TLM_READ_COMMAND,  CTRL, 1, d) ==
                    tlm::TLM_BURST_ERROR_RESPONSE);
    }
    std::cout << "  [PASS] TLM burst error\n";

    // ----------------------------------------------------------------------
    // 3. Address error: outside the 0x100 window, or misaligned
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        EXPECT_TRUE(drv.read (cfg::WINDOW_SIZE, d) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_TRUE(drv.read (0x1000u, d)          == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_TRUE(drv.read (0x02u, d)            == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        EXPECT_TRUE(drv.write(0x0Au, 0u)           == tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
    std::cout << "  [PASS] TLM address error\n";

    // ----------------------------------------------------------------------
    // 4. In-window decode miss: the gap between COUNTER_VLDS and COUNTER[0]
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        for (uint64_t off : {uint64_t{0x1C}, uint64_t{0x20}, uint64_t{0x40},
                             uint64_t{0x7C}}) {
            EXPECT_TRUE(drv.read (off, d) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
            EXPECT_TRUE(drv.write(off, 0u) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        // ... while the first and last counter registers do decode.
        EXPECT_TRUE(drv.read(counter_reg(0), d) == tlm::TLM_OK_RESPONSE);
        EXPECT_TRUE(drv.read(counter_reg(cfg::NUM_COUNTER_REGS - 1), d) ==
                    tlm::TLM_OK_RESPONSE);
    }
    std::cout << "  [PASS] in-window decode miss\n";

    // ----------------------------------------------------------------------
    // 5. transport_dbg: accepted and rejected forms
    // ----------------------------------------------------------------------
    {
        uint32_t d = 0;
        // Rejected: bad length, misaligned, out of window, bad command.
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND,   CTRL, 8, d));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND,   0x06, 4, d));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND,   0x200, 4, d));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_IGNORE_COMMAND, CTRL, 4, d));
        // Rejected: debug write to an unmapped in-window offset.
        d = 0xFFFFFFFFu;
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_WRITE_COMMAND, 0x1C, 4, d));
        // Accepted: debug write then debug read back through the model.
        d = cfg::INTR_MASK;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND, INTR_ENABLE, 4, d));
        d = 0;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_READ_COMMAND, INTR_ENABLE, 4, d));
        EXPECT_EQ(cfg::INTR_MASK, d);
        // Accepted but ignored: a debug write to a read-only register.
        d = 0xFFFFFFFFu;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND, counter_reg(1), 4, d));
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND, STATUS, 4, d));
        d = 0;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_READ_COMMAND, counter_reg(1), 4, d));
        EXPECT_EQ(0u, d);
        drv.write(INTR_ENABLE, 0u);
    }
    // dbg_reg() on an offset past the counter array returns 0 rather than
    // indexing out of range.
    EXPECT_EQ(0u, dut.dbg_reg(cfg::WINDOW_SIZE));
    EXPECT_EQ(0u, dut.dbg_reg(0x1C));
    std::cout << "  [PASS] transport_dbg accept/reject + dbg_reg guards\n";

    // ----------------------------------------------------------------------
    // 6. BUFFER_POP on an empty queue is ignored (no pointer underflow)
    // ----------------------------------------------------------------------
    do_reset();
    EXPECT_EQ(0u, dut.fill_level());
    for (int i = 0; i < 3; ++i) drv.write(CTRL, cfg::CTRL_BUFFER_POP);
    settle();
    EXPECT_EQ(0u, dut.fill_level());
    EXPECT_EQ(cfg::STATUS_BUFFER_EMPTY, [&]{ uint32_t d = 0;
                                             drv.read(STATUS, d); return d; }());
    // The queue still works afterwards.
    dut.push_atb_beats(smc::telemetry_encode_message(
        3, std::vector<smc::telemetry_counter_value>{{true, 0x77u}}, 1));
    settle();
    EXPECT_EQ(1u, dut.fill_level());
    {
        uint32_t d = 0;
        drv.read(PROBE_ID, d);
        EXPECT_EQ(3u, d);
        drv.read(counter_reg(0), d);
        EXPECT_EQ(0x77u, d);
    }
    std::cout << "  [PASS] BUFFER_POP on empty queue\n";

    // ----------------------------------------------------------------------
    // 7. Flush precedence: a write setting both RX_FLUSH and BUFFER_POP
    //    flushes (the RTL gives the flush precedence over the pop)
    // ----------------------------------------------------------------------
    dut.push_atb_beats(smc::telemetry_encode_message(
        4, std::vector<smc::telemetry_counter_value>{{true, 0x88u}}, 1));
    settle();
    EXPECT_EQ(2u, dut.fill_level());
    drv.write(CTRL, cfg::CTRL_RX_FLUSH | cfg::CTRL_BUFFER_POP);
    settle();
    EXPECT_EQ(0u, dut.fill_level());   // whole queue gone, not just one entry
    std::cout << "  [PASS] RX_FLUSH takes precedence over BUFFER_POP\n";

    // ----------------------------------------------------------------------
    // 8. BUFFER_THRESHOLD truncates to the message-buffer pointer width
    //
    // buffer_depth = 2 => pointer width clog2(2)+1 = 2 bits, so a threshold
    // field of 0x5 is compared as 0x5 & 0x3 = 1.
    // ----------------------------------------------------------------------
    do_reset();
    drv.write(INTR_ENABLE, cfg::INTR_BUFFER_THRESHOLD);
    drv.write(CTRL, 0x5u << cfg::CTRL_THRESHOLD_SHIFT);
    settle();
    EXPECT_TRUE(!irq.read());
    dut.push_atb_beats(smc::telemetry_encode_message(
        5, std::vector<smc::telemetry_counter_value>{{true, 1u}}, 1));
    settle();
    EXPECT_EQ(1u, dut.fill_level());
    EXPECT_TRUE(!irq.read());                       // 1 > 1 is false
    dut.push_atb_beats(smc::telemetry_encode_message(
        6, std::vector<smc::telemetry_counter_value>{{true, 2u}}, 1));
    settle();
    EXPECT_EQ(2u, dut.fill_level());
    EXPECT_TRUE(irq.read());                        // 2 > 1 => truncated to 1
    std::cout << "  [PASS] BUFFER_THRESHOLD truncation\n";

    // ----------------------------------------------------------------------
    // 9. Maximum geometry: 32 counters per message (19 ATB packets)
    // ----------------------------------------------------------------------
    {
        const unsigned packets = smc::telemetry_packets_per_message(
            cfg::NUM_COUNTER_REGS);
        EXPECT_EQ(19u, packets);                    // ceil((1 + 4*32) / 7)

        std::vector<smc::telemetry_counter_value> c;
        for (unsigned i = 0; i < cfg::NUM_COUNTER_REGS; ++i)
            c.push_back({true, 0xC0000000u | i});

        const std::vector<uint8_t> beats = smc::telemetry_encode_message(
            0x1E, c, cfg::NUM_COUNTER_REGS);
        EXPECT_EQ(packets * cfg::BEATS_PER_PACKET, beats.size());
        EXPECT_EQ(beats.size(), dutB.push_atb_beats(beats));
        settle();

        EXPECT_EQ(1u, dutB.fill_level());
        uint32_t d = 0;
        drvB.read(PROBE_ID, d);
        EXPECT_EQ(0x1Eu, d);
        drvB.read(COUNTER_VLDS, d);
        EXPECT_EQ(0xFFFFFFFFu, d);                  // all 32 counters valid
        for (unsigned i = 0; i < cfg::NUM_COUNTER_REGS; ++i) {
            drvB.read(counter_reg(i), d);
            EXPECT_EQ(0xC0000000u | i, d);
        }
    }
    std::cout << "  [PASS] maximum 32-counter geometry\n";

    // ----------------------------------------------------------------------
    // 10. Missing-last on the maximum geometry, then W1C
    // ----------------------------------------------------------------------
    {
        drvB.write(CTRL, cfg::CTRL_RX_FLUSH);
        drvB.write(INTR_ENABLE, cfg::INTR_MISSING_LAST);
        settle();
        const std::vector<uint8_t> beats = smc::telemetry_encode_message(
            1, {}, cfg::NUM_COUNTER_REGS, /*set_last_packet=*/false);
        dutB.push_atb_beats(beats);
        settle();
        EXPECT_EQ(0u, dutB.fill_level());
        EXPECT_TRUE(irqB.read());
        uint32_t d = 0;
        drvB.read(INTR_STATUS, d);
        EXPECT_EQ(cfg::INTR_MISSING_LAST, d);
        drvB.write(INTR_STATUS, cfg::INTR_MISSING_LAST);
        settle();
        EXPECT_TRUE(!irqB.read());
    }
    std::cout << "  [PASS] missing-last on maximum geometry\n";

    dutB.dump_state(std::cout);

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
    auto broker = cci::cci_get_global_broker(platform_cfg);
    // Minimum legal queue depth, single counter per message.
    broker.set_preset_cci_value("tb.telmin.buffer_depth", cci::cci_value(2u));
    broker.set_preset_cci_value("tb.telmin.max_counters_per_message",
                                cci::cci_value(1u));
    // Maximum counters per message.
    broker.set_preset_cci_value("tb.telmax.buffer_depth", cci::cci_value(2u));
    broker.set_preset_cci_value("tb.telmax.max_counters_per_message",
                                cci::cci_value(smc::telemetry_receiver_cfg::NUM_COUNTER_REGS));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
