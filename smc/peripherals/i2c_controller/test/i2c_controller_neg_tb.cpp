// SPDX-License-Identifier: Apache-2.0
//
// i2c_controller_neg_tb.cpp -- negative-path / edge-case coverage for the OCA
// I2C Controller model.
//
//   - constructor SC_REPORT_FATAL branch: zero FIFO depth
//   - TLM error responses: non-word width, misaligned, out-of-window,
//     in-window decode miss, bad command
//   - read-only registers ignore writes; write-only registers read as zero
//   - FIFO overflow error interrupts (FMT / TX / ACQ)
//   - back door on a disabled target is NACKed
//   - transport_dbg rejects malformed accesses, accepts a storage round-trip
//   - CCI parameter immutability
//
// Like the sister IPs' negative benches, this test exercises the model
// entirely during elaboration (b_transport / transport_dbg are plain function
// calls through the bound socket) and never calls sc_start, so the unbound
// irq_o output is harmless and the constructor-throw probes do not perturb a
// running kernel.
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "i2c_controller.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                     \
    } while (0)

#define EXPECT_EQ(expected, actual)                                           \
    do {                                                                       \
        const auto _e = (expected);                                           \
        const auto _a = (actual);                                             \
        if (!(_e == _a)) {                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__               \
                      << "  expected=" << +_e << " actual=" << +_a            \
                      << "  (" #expected " == " #actual ")\n";                \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

using cfg_t = smc::i2c_controller_cfg;

// Run `body`; return true iff it raised any exception (SC_REPORT_FATAL throws).
template <typename F>
bool expect_fatal(F&& body)
{
    try { body(); }
    catch (const sc_core::sc_report&) { return true; }
    catch (const std::exception&)     { return true; }
    catch (...)                       { return true; }
    return false;
}

// Initiator that issues raw transactions against the controller socket.
struct probe : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<probe> sock;
    explicit probe(sc_module_name n) : sc_module(n), sock("sock") {}

    tlm::tlm_response_status raw(tlm::tlm_command cmd, uint64_t addr,
                                 uint32_t len, void* data) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    uint32_t read_ok(uint64_t addr) {
        uint32_t v = 0;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, raw(tlm::TLM_READ_COMMAND, addr, 4, &v));
        return v;
    }
    void write_ok(uint64_t addr, uint32_t v) {
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, raw(tlm::TLM_WRITE_COMMAND, addr, 4, &v));
    }

    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, uint32_t len, void* data) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        return sock->transport_dbg(gp);
    }
};

} // namespace

int sc_main(int, char**)
{
    // Make SC_REPORT_FATAL throw so we can probe the constructor guard rail.
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    std::cout << "==== OCA I2C Controller negative-path TB ====\n";

    // ---- Constructor guard rail: any zero FIFO depth is fatal ----
    {
        cfg_t c0; c0.fmt_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i2c_controller r("bad_fmt", c0); }));
        cfg_t c1; c1.acq_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i2c_controller r("bad_acq", c1); }));
        std::cout << "  [PASS] constructor guard rail (zero FIFO depth)\n";
    }

    // ---- Valid DUT with tiny FIFOs to reach overflow paths quickly ----
    cfg_t cfg;
    cfg.fmt_fifo_depth = 2;
    cfg.tx_fifo_depth  = 2;
    cfg.acq_fifo_depth = 2;
    smc::i2c_controller dut("i2c_controller", cfg);
    probe               pr("probe");
    pr.sock.bind(dut.reg_socket);

    uint32_t scratch = 0;
    uint64_t scratch64 = 0;

    // ---- TLM error responses ----
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::CTRL, 2, &scratch));       // width 2
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::CTRL, 8, &scratch64));     // width 8
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::CTRL + 2, 4, &scratch));   // misaligned
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, 4, &scratch));// out of window
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, 0x84, 4, &scratch));             // in-window miss (RD)
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_WRITE_COMMAND, 0x84, 4, &scratch));            // in-window miss (WR)
    EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
              pr.raw(tlm::TLM_IGNORE_COMMAND, cfg_t::CTRL, 4, &scratch));    // bad command
    std::cout << "  [PASS] TLM error responses (width/align/window/miss/cmd)\n";

    // ---- Read-only registers ignore writes; write-only read as 0 ----
    pr.write_ok(cfg_t::STATUS, 0xFFFFFFFF);            // RO -> ignored
    EXPECT_TRUE((pr.read_ok(cfg_t::STATUS) & (1u << 2)) != 0); // still FMTEMPTY
    pr.write_ok(cfg_t::HOST_FIFO_STATUS, 0xFFFFFFFF);  // RO -> ignored
    pr.write_ok(cfg_t::ACQ_FIFO_NEXT_DATA, 0xFFFF);    // RO -> ignored
    EXPECT_EQ(0u, pr.read_ok(cfg_t::FDATA));           // WO -> RAZ
    EXPECT_EQ(0u, pr.read_ok(cfg_t::FIFO_CTRL));       // WO -> RAZ
    EXPECT_EQ(0u, pr.read_ok(cfg_t::TXDATA));          // WO -> RAZ
    std::cout << "  [PASS] RO write-ignore / WO read-as-zero\n";

    // ---- FMT FIFO overflow -> CONTROLLER_TX_FIFO_ERROR (host disabled) ----
    pr.write_ok(cfg_t::FDATA, 0x01);
    pr.write_ok(cfg_t::FDATA, 0x02);
    pr.write_ok(cfg_t::FDATA, 0x03);                   // 3rd into depth-2 FIFO
    EXPECT_TRUE((pr.read_ok(cfg_t::INTR_STATE) & (1u << 16)) != 0);
    std::cout << "  [PASS] FMT FIFO overflow error\n";

    // ---- TX FIFO overflow -> TARGET_TX_FIFO_ERROR ----
    pr.write_ok(cfg_t::TXDATA, 0x11);
    pr.write_ok(cfg_t::TXDATA, 0x22);
    pr.write_ok(cfg_t::TXDATA, 0x33);                  // 3rd into depth-2 FIFO
    EXPECT_TRUE((pr.read_ok(cfg_t::INTR_STATE) & (1u << 18)) != 0);
    std::cout << "  [PASS] TX FIFO overflow error\n";

    // ---- ACQ FIFO overflow -> TARGET_RX_FIFO_ERROR (via target back door) ----
    pr.write_ok(cfg_t::CTRL, 1u << 1);                 // ENABLETARGET
    pr.write_ok(cfg_t::TARGET_ID, 0x50u | (0x7Fu << 7));
    // Start + 1 data + Stop = 3 entries into a depth-2 ACQ FIFO -> overflow.
    EXPECT_TRUE(dut.target_write(0x50, {0xAA}));
    EXPECT_TRUE((pr.read_ok(cfg_t::INTR_STATE) & (1u << 19)) != 0);
    std::cout << "  [PASS] ACQ FIFO overflow error\n";

    // ---- Back door on a disabled target is NACKed ----
    pr.write_ok(cfg_t::CTRL, 0);                       // ENABLETARGET = 0
    std::vector<uint8_t> out;
    EXPECT_TRUE(!dut.target_write(0x50, {0x01}));
    EXPECT_TRUE(!dut.target_read(0x50, 1, out));
    std::cout << "  [PASS] disabled-target back door NACK\n";

    // ---- Direct TARGET_NACK_COUNT write path ----
    pr.write_ok(cfg_t::TARGET_NACK_COUNT, 0x7);
    EXPECT_EQ(0x7u, pr.read_ok(cfg_t::TARGET_NACK_COUNT));

    // ---- TARGET_EVENTS W1C write path (set via back door, then clear) ----
    EXPECT_TRUE((pr.read_ok(cfg_t::TARGET_EVENTS) & (1u << 3)) != 0); // START_DETECT (from ACQ test)
    pr.write_ok(cfg_t::TARGET_EVENTS, 0x1F);                          // W1C all
    EXPECT_EQ(0u, pr.read_ok(cfg_t::TARGET_EVENTS));

    // ---- transport_dbg malformed reject + storage round-trip ----
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::CTRL + 2, 4, &scratch));      // misaligned
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::CTRL, 2, &scratch));          // width
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, 4, &scratch));   // OOB
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_IGNORE_COMMAND, cfg_t::CTRL, 4, &scratch));        // bad cmd
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_WRITE_COMMAND, 0x84, 4, &scratch));                // write miss
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, 0x84, 4, &scratch));                 // hole reads 0
    EXPECT_EQ(0u, scratch);
    scratch = 0x0FEEDFACEu & cfg_t::TIMEOUT_CTRL_MASK;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_WRITE_COMMAND, cfg_t::TIMEOUT_CTRL, 4, &scratch));
    scratch = 0;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::TIMEOUT_CTRL, 4, &scratch));
    EXPECT_EQ(uint32_t(0x0FEEDFACEu & cfg_t::TIMEOUT_CTRL_MASK), scratch);
    // dbg peek of every side-effect-free branch (coverage of dbg_reg).
    for (uint64_t off : {cfg_t::INTR_STATE, cfg_t::INTR_TEST, cfg_t::STATUS,
                         cfg_t::RDATA, cfg_t::FDATA, cfg_t::FIFO_CTRL,
                         cfg_t::HOST_FIFO_STATUS, cfg_t::TARGET_FIFO_STATUS,
                         cfg_t::ACQDATA, cfg_t::TXDATA, cfg_t::TARGET_NACK_COUNT,
                         cfg_t::TARGET_ACK_CTRL, cfg_t::ACQ_FIFO_NEXT_DATA,
                         cfg_t::CONTROLLER_EVENTS, cfg_t::TARGET_EVENTS,
                         cfg_t::INTR_ENABLE /* regmap fallback */}) {
        EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, off, 4, &scratch));
    }
    // dbg peek must NOT clear TARGET_NACK_COUNT (unlike a real read).
    pr.write_ok(cfg_t::TARGET_NACK_COUNT, 0x9);
    EXPECT_EQ(0x9u, dut.dbg_reg(cfg_t::TARGET_NACK_COUNT));
    EXPECT_EQ(0x9u, dut.dbg_reg(cfg_t::TARGET_NACK_COUNT)); // still 9
    std::cout << "  [PASS] transport_dbg malformed reject + round-trip\n";

    // ---- CCI parameter immutability ----
    {
        cci::cci_originator orig("neg_tb");
        auto broker = cci::cci_get_global_broker(orig);
        auto h = broker.get_param_handle("i2c_controller.fmt_fifo_depth");
        EXPECT_TRUE(h.is_valid());
        const bool threw = expect_fatal([&]{ h.set_cci_value(cci::cci_value(99u)); });
        // Immutable params reject post-construction writes (throw or no-op).
        EXPECT_TRUE(threw || h.get_cci_value().to_json() == std::string("2"));
        std::cout << "  [PASS] CCI immutability\n";
    }

    dut.dump_state(std::cout);

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    return g_failures == 0 ? 0 : 1;
}
