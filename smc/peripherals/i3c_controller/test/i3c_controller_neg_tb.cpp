// SPDX-License-Identifier: Apache-2.0
//
// i3c_controller_neg_tb.cpp -- negative-path / edge-case coverage for the OCA
// I3C Controller model.
//
//   - constructor SC_REPORT_FATAL branches: num_instances 0 / >6,
//     negative delays, zero FIFO depth
//   - TLM error responses: non-word width, misaligned, streaming mismatch,
//     byte-enable, out-of-aperture, bad command
//   - holes inside the window are RAZ/WI
//   - transport_dbg rejects malformed/CSR accesses, accepts table accesses
//
// Like the sister IPs' negative benches, this test exercises the model
// entirely during elaboration (b_transport / transport_dbg are plain function
// calls through the bound socket) and never calls sc_start, so unbound output
// vectors are harmless and the constructor-throw probes do not perturb a
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

#include "i3c_controller.h"

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
        }                                                                      \
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

using cfg_t = smc::i3c_controller_cfg;

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
                                 uint32_t len, void* data, uint32_t sw = 0,
                                 uint8_t* be = nullptr, uint32_t be_len = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(sw == 0 ? len : sw);
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
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
    // Make SC_REPORT_FATAL throw so we can probe the constructor guard rails.
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    std::cout << "==== OCA I3C Controller negative-path TB ====\n";

    // ---- Constructor guard rails ----
    {
        cfg_t c0; c0.num_instances = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i3c_controller r("bad_ni0", c0); }));

        cfg_t c1; c1.num_instances = 7;
        EXPECT_TRUE(expect_fatal([&]{ smc::i3c_controller r("bad_ni7", c1); }));

        cfg_t c2; c2.access_delay_ns = -1.0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i3c_controller r("bad_delay", c2); }));

        cfg_t c3; c3.xfer_delay_ns = -3.0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i3c_controller r("bad_xfer", c3); }));

        cfg_t c4; c4.tx_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&]{ smc::i3c_controller r("bad_fifo", c4); }));

        std::cout << "  [PASS] constructor guard rails (instances/delays/fifo)\n";
    }

    // ---- TLM error / dbg paths against a valid DUT ----
    cfg_t cfg; cfg.num_instances = 2;
    smc::i3c_controller dut("i3c_controller", cfg);
    probe               pr("probe");
    pr.sock.bind(dut.reg_socket);

    const uint64_t aperture = uint64_t(cfg.num_instances) * cfg_t::INSTANCE_SPACING;

    uint32_t scratch = 0;
    uint64_t scratch64 = 0;

    // Non-word width.
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 2, &scratch));
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 8, &scratch64));
    // Misaligned word.
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL + 2, 4, &scratch));
    // Streaming-width mismatch.
    EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4, &scratch, /*sw=*/1));
    // Byte-enable not supported.
    {
        uint8_t be[4] = {0xFF, 0x00, 0xFF, 0x00};
        EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                  pr.raw(tlm::TLM_WRITE_COMMAND, cfg_t::HC_CONTROL, 4, &scratch, 0, be, 4));
    }
    // Out-of-aperture.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, aperture, 4, &scratch));
    // Bad command.
    EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
              pr.raw(tlm::TLM_IGNORE_COMMAND, cfg_t::HC_CONTROL, 4, &scratch));
    std::cout << "  [PASS] TLM error responses (width/align/sw/be/aperture/cmd)\n";

    // Hole inside the window: RAZ/WI (0x18 is unmapped).
    EXPECT_EQ(tlm::TLM_OK_RESPONSE, pr.raw(tlm::TLM_READ_COMMAND, 0x18, 4, &scratch));
    EXPECT_EQ(uint32_t(0), scratch);
    EXPECT_EQ(tlm::TLM_OK_RESPONSE, pr.raw(tlm::TLM_WRITE_COMMAND, 0x18, 4, &scratch));
    std::cout << "  [PASS] hole inside window is RAZ/WI\n";

    // transport_dbg malformed / unsupported accesses return 0.
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DAT_BASE + 2, 4, &scratch)); // misaligned
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DAT_BASE, 2, &scratch));      // bad width
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, aperture, 4, &scratch));            // OOB
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4, &scratch));    // CSR not backed
    // Valid dbg write/read round-trip through the DAT window.
    scratch = 0x0F0F0F0Fu;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_WRITE_COMMAND, cfg_t::DAT_BASE + 8, 4, &scratch));
    scratch = 0;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DAT_BASE + 8, 4, &scratch));
    EXPECT_EQ(uint32_t(0x0F0F0F0Fu), scratch);
    std::cout << "  [PASS] transport_dbg malformed reject + valid table round-trip\n";

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    return g_failures == 0 ? 0 : 1;
}
