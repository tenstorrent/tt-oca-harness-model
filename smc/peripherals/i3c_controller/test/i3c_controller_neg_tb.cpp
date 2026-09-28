// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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

/// Changes the report actions for one severity and restores them on scope
/// exit, so an expected diagnostic cannot silence an unrelated one later.
struct scoped_report_actions {
    sc_core::sc_severity sev;
    sc_core::sc_actions  saved;
    scoped_report_actions(sc_core::sc_severity s, sc_core::sc_actions a)
        : sev(s), saved(sc_core::sc_report_handler::set_actions(s, a))
    {
    }
    ~scoped_report_actions()
    {
        sc_core::sc_report_handler::set_actions(sev, saved);
    }
};

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

    tlm::tlm_response_status raw_with_axi(tlm::tlm_command cmd, uint64_t addr,
                                          uint32_t& data,
                                          smc::smc_axi_extension& ext) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(sizeof(data));
        gp.set_streaming_width(sizeof(data));
        gp.set_extension(&ext);
        sock->b_transport(gp, t);
        gp.clear_extension<smc::smc_axi_extension>();
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

    bool dmi(uint64_t addr, tlm::tlm_dmi& dmi_data) {
        tlm::tlm_generic_payload gp;
        uint32_t scratch = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&scratch));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        return sock->get_direct_mem_ptr(gp, dmi_data);
    }

    /// True iff the target left `dmi_allowed` set after a transaction.
    bool dmi_allowed_after(tlm::tlm_command cmd, uint64_t addr, uint32_t len,
                           void* data) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(true);
        sock->b_transport(gp, t);
        return gp.is_dmi_allowed();
    }
};

} // namespace

int sc_main(int, char**)
{
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    std::cout << "==== OCA I3C Controller negative-path TB ====\n";

    // ---- Constructor guard rails ----
    // The report-action changes are scoped to this block: leaving SC_FATAL
    // throwing and SC_ERROR demoted for the whole run would let an unrelated
    // fatal or error later in the bench pass unnoticed.
    {
        const scoped_report_actions relax_fatal(
            sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);
        const scoped_report_actions relax_error(sc_core::SC_ERROR,
                                                sc_core::SC_DISPLAY);

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
    sc_core::sc_signal<bool> rst_n("rst_n");
    dut.rst_n_i(rst_n);
    rst_n.write(true);
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
    // Misaligned word: every byte offset that is not 4-byte aligned.
    for (uint64_t off : {UINT64_C(1), UINT64_C(2), UINT64_C(3)}) {
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL + off, 4,
                         &scratch));
    }
    // Streaming width: a register access is a single beat, so TLM-2.0 requires
    // streaming_width >= data_length.  Smaller is a burst error; equal and
    // larger are both accepted and ignored.
    for (unsigned sw = 1; sw <= 8; ++sw) {
        const tlm::tlm_response_status exp =
            (sw < 4) ? tlm::TLM_BURST_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE;
        EXPECT_EQ(exp, pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4,
                              &scratch, sw));
    }
    // Byte enables are refused whatever the byte-enable length, including the
    // zero-length and all-lanes-enabled forms.
    {
        uint8_t be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        for (unsigned be_len : {0u, 1u, 2u, 4u}) {
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      pr.raw(tlm::TLM_WRITE_COMMAND, cfg_t::HC_CONTROL, 4,
                             &scratch, 0, be, be_len));
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4,
                             &scratch, 0, be, be_len));
        }
    }
    // Zero data length.
    EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 0, &scratch));
    // Out-of-aperture.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, aperture, 4, &scratch));
    // Bad command.
    EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
              pr.raw(tlm::TLM_IGNORE_COMMAND, cfg_t::HC_CONTROL, 4, &scratch));
    EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
              pr.raw(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4, nullptr));
    smc::smc_axi_extension axi;
    axi.source_id = smc::SMC_ID;
    axi.axi_id = 0x5A;
    axi.set_priv(true);
    EXPECT_EQ(tlm::TLM_OK_RESPONSE,
              pr.raw_with_axi(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL,
                              scratch, axi));
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
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DAT_BASE, 4, nullptr));      // null
    EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::HC_CONTROL, 4, &scratch));    // CSR not backed
    // Valid dbg write/read round-trip through the DAT window.
    scratch = 0x0F0F0F0Fu;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_WRITE_COMMAND, cfg_t::DAT_BASE + 8, 4, &scratch));
    scratch = 0;
    EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DAT_BASE + 8, 4, &scratch));
    EXPECT_EQ(uint32_t(0x0F0F0F0Fu), scratch);
    std::cout << "  [PASS] transport_dbg malformed reject + valid table round-trip\n";

    // transport_dbg at the first and last word of each table window, and the
    // first word past each one.
    {
        uint32_t v = 0;
        for (uint64_t off : {cfg_t::DAT_BASE, cfg_t::DAT_END - 4,
                             cfg_t::DCT_BASE, cfg_t::DCT_END - 4}) {
            EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND, off, 4, &v));
        }
        // 0x500 is the first word past the DCT window: not table-backed.
        EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, cfg_t::DCT_END, 4, &v));
        // Last instance's last table word is reachable; the first word beyond
        // the whole aperture is not.
        const uint64_t last_inst = uint64_t(cfg.num_instances - 1) *
                                   cfg_t::INSTANCE_SPACING;
        EXPECT_EQ(4u, pr.dbg(tlm::TLM_READ_COMMAND,
                             last_inst + cfg_t::DCT_END - 4, 4, &v));
        EXPECT_EQ(0u, pr.dbg(tlm::TLM_READ_COMMAND, aperture, 4, &v));
        // An unsupported debug command is refused.
        EXPECT_EQ(0u, pr.dbg(tlm::TLM_IGNORE_COMMAND, cfg_t::DAT_BASE, 4, &v));
    }
    std::cout << "  [PASS] transport_dbg table boundaries and aperture edge\n";

    // DMI is denied everywhere: FIFO ports pop on read and a CSR write can
    // start a transfer, so a direct pointer would bypass real side effects.
    {
        for (uint64_t off : {uint64_t(cfg_t::HC_CONTROL),
                             uint64_t(cfg_t::DAT_BASE), aperture - 4}) {
            tlm::tlm_dmi d;
            d.allow_read_write();
            EXPECT_TRUE(!pr.dmi(off, d));
            EXPECT_TRUE(!d.is_read_allowed());
            EXPECT_TRUE(!d.is_write_allowed());
        }
        // dmi_allowed is cleared on both the success and the error path.
        EXPECT_TRUE(!pr.dmi_allowed_after(tlm::TLM_READ_COMMAND,
                                          cfg_t::HC_CONTROL, 4, &scratch));
        EXPECT_TRUE(!pr.dmi_allowed_after(tlm::TLM_READ_COMMAND, aperture, 4,
                                          &scratch));
    }
    std::cout << "  [PASS] DMI denied on hit and miss; no stale dmi_allowed\n";

#ifdef I3C_UB_CANARY
    // Built only by `run_tests.sh --ubsan-canary`.  Proves the UBSan build
    // really does report undefined behaviour, so a clean --asan run means
    // something.  It sits in this auxiliary bench so the canary run also
    // proves the runner propagates a non-primary bench's exit status.
    {
        volatile int shift = 33;
        volatile int value = 1;
        std::cout << "  [UB CANARY] " << (value << shift) << "\n";
    }
#endif

    if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
    else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
    return g_failures == 0 ? 0 : 1;
}
