// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smu/test/smu_interconnect_tb.cpp
//
// Self-checking unit test bench for the SMU on-die interconnect models:
//   * smu::smu_axi_xbar      (3x3 non-reflexive AXI crossbar)
//   * smu::axi_window_remap  (single-window address translator)
//
// Drives b_transport / transport_dbg transactions through every route and
// decode-miss path, checks address/data integrity, response-status
// propagation, caller-side address restoration, and runtime CCI aperture
// reprogramming.
// ===========================================================================

#include "smu_axi_xbar.h"
#include "axi_window_remap.h"

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// Recording stub target: terminates a socket, remembers the last access,
// serves a programmable read value and response status.
// ---------------------------------------------------------------------------
class stub_target : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<stub_target, 64> sock{"sock"};

    uint64_t              last_addr = 0;
    uint64_t              last_data = 0;
    tlm::tlm_command      last_cmd  = tlm::TLM_IGNORE_COMMAND;
    unsigned              calls     = 0;
    unsigned              dbg_calls = 0;
    uint64_t              dbg_addr  = 0;
    tlm::tlm_response_status rsp    = tlm::TLM_OK_RESPONSE;
    uint64_t              read_data = 0xdeadbeef'cafebabeULL;

    SC_HAS_PROCESS(stub_target);
    explicit stub_target(sc_core::sc_module_name name) : sc_core::sc_module(name)
    {
        sock.register_b_transport(this, &stub_target::b_transport);
        sock.register_transport_dbg(this, &stub_target::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++calls;
        last_addr = trans.get_address();
        last_cmd  = trans.get_command();
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&last_data, trans.get_data_ptr(), trans.get_data_length());
        } else {
            std::memcpy(trans.get_data_ptr(), &read_data, trans.get_data_length());
        }
        trans.set_response_status(rsp);
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++dbg_calls;
        dbg_addr = trans.get_address();
        if (trans.get_command() == tlm::TLM_READ_COMMAND)
            std::memcpy(trans.get_data_ptr(), &read_data, trans.get_data_length());
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};

// ---------------------------------------------------------------------------
// Transaction helpers (4- and 8-byte blocking accesses + debug transport).
// Templated on the socket type: simple_initiator_socket is parameterized on
// the owning module class.
// ---------------------------------------------------------------------------
template <typename Socket>
tlm::tlm_response_status do_write(Socket& sock, uint64_t addr, uint64_t data,
                                  unsigned len = 4)
{
    tlm::tlm_generic_payload gp;
    sc_core::sc_time delay{sc_core::SC_ZERO_TIME};
    gp.set_command(tlm::TLM_WRITE_COMMAND);
    gp.set_address(addr);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    gp.set_data_length(len);
    gp.set_streaming_width(len);
    sock->b_transport(gp, delay);
    return gp.get_response_status();
}

template <typename Socket>
tlm::tlm_response_status do_read(Socket& sock, uint64_t addr, uint64_t& data,
                                 unsigned len = 4)
{
    tlm::tlm_generic_payload gp;
    sc_core::sc_time delay{sc_core::SC_ZERO_TIME};
    data = 0;
    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(addr);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    gp.set_data_length(len);
    gp.set_streaming_width(len);
    sock->b_transport(gp, delay);
    return gp.get_response_status();
}

template <typename Socket>
unsigned do_dbg(Socket& sock, uint64_t addr, uint64_t& data,
                tlm::tlm_response_status& status)
{
    tlm::tlm_generic_payload gp;
    data = 0;
    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(addr);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    const unsigned n = sock->transport_dbg(gp);
    status = gp.get_response_status();
    return n;
}

// Address-restoration check needs direct access to the payload after the
// call, so it gets its own helper that returns the post-call address.
template <typename Socket>
uint64_t write_and_report_final_addr(Socket& sock, uint64_t addr, uint64_t data)
{
    tlm::tlm_generic_payload gp;
    sc_core::sc_time delay{sc_core::SC_ZERO_TIME};
    gp.set_command(tlm::TLM_WRITE_COMMAND);
    gp.set_address(addr);
    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    sock->b_transport(gp, delay);
    return gp.get_address();
}

} // namespace

// ---------------------------------------------------------------------------
// Test bench top
// ---------------------------------------------------------------------------
class smu_interconnect_tb : public sc_core::sc_module
{
public:
    // Drivers (one per master-side port of each DUT).
    tlm_utils::simple_initiator_socket<smu_interconnect_tb, 64> init_sep{"init_sep"};
    tlm_utils::simple_initiator_socket<smu_interconnect_tb, 64> init_smc{"init_smc"};
    tlm_utils::simple_initiator_socket<smu_interconnect_tb, 64> init_ext{"init_ext"};
    tlm_utils::simple_initiator_socket<smu_interconnect_tb, 64> init_r1{"init_r1"};
    tlm_utils::simple_initiator_socket<smu_interconnect_tb, 64> init_r2{"init_r2"};

    // DUTs.
    smu::smu_axi_xbar          xbar{"smu_xbar"};
    smu::axi_window_remap<64, 64> remap1{"remap1", 0x4000'0000ULL, 0x4000'0000ULL, 0x0ULL};
    smu::axi_window_remap<64, 64> remap2{"remap2", 0x1000ULL, 0x1000ULL, 0x8000ULL};

    // Subordinate-side recorders.
    stub_target t_sep_in{"t_sep_in"};
    stub_target t_smc_in{"t_smc_in"};
    stub_target t_ext_out{"t_ext_out"};
    stub_target t_r1{"t_r1"};
    stub_target t_r2{"t_r2"};

    int failures = 0;

    SC_HAS_PROCESS(smu_interconnect_tb);
    explicit smu_interconnect_tb(sc_core::sc_module_name name) : sc_core::sc_module(name)
    {
        init_sep.bind(xbar.sep_out);
        init_smc.bind(xbar.smc_out);
        init_ext.bind(xbar.ext_in);
        xbar.sep_in.bind(t_sep_in.sock);
        xbar.smc_in.bind(t_smc_in.sock);
        xbar.ext_out.bind(t_ext_out.sock);

        init_r1.bind(remap1.tgt);
        remap1.init.bind(t_r1.sock);
        init_r2.bind(remap2.tgt);
        remap2.init.bind(t_r2.sock);

        SC_THREAD(run);
    }

private:
    void check(bool cond, const std::string& what)
    {
        if (!cond) {
            ++failures;
            std::cout << "  FAIL: " << what << "\n";
        }
    }

    void run()
    {
        // Exercise the SIM_LOG_TRACE/DEBUG paths (and with them the
        // master_name() helper) instead of letting them short-circuit at
        // the default info level.
        simlog::set_level(simlog::level::trace);

        std::cout << "== smu_axi_xbar routing ==\n";
        test_xbar_routes();
        test_xbar_dbg();
        test_xbar_error_propagation();
        test_xbar_runtime_reprogram();
        std::cout << "== axi_window_remap ==\n";
        test_remap_window();
        test_remap_dbg();
        test_remap_error_propagation();

        std::cout << (failures == 0 ? "TESTBENCH PASS\n" : "TESTBENCH FAIL\n")
                  << std::flush;
    }

    // -----------------------------------------------------------------------
    void test_xbar_routes()
    {
        uint64_t data = 0;

        // sep_out -> smc_in (SMC aperture), address forwarded unchanged.
        check(do_write(init_sep, 0x4000'0004ULL, 0x11223344ULL) == tlm::TLM_OK_RESPONSE,
              "sep_out->smc_in status");
        check(t_smc_in.calls == 1 && t_smc_in.last_addr == 0x4000'0004ULL &&
              t_smc_in.last_data == 0x11223344ULL && t_smc_in.last_cmd == tlm::TLM_WRITE_COMMAND,
              "sep_out->smc_in addr/data");
        check(t_sep_in.calls == 0 && t_ext_out.calls == 0, "sep_out->smc_in exclusivity");

        // sep_out -> ext_out (catch-all outside both apertures).
        check(do_write(init_sep, 0x9000'0000ULL, 0x55ULL) == tlm::TLM_OK_RESPONSE,
              "sep_out->ext_out status");
        check(t_ext_out.calls == 1 && t_ext_out.last_addr == 0x9000'0000ULL,
              "sep_out->ext_out addr");

        // sep_out with an address in the SEP's own aperture is non-reflexive:
        // it falls to the ext_out catch-all (never sep_in).
        check(do_write(init_sep, 0x5000'0000ULL, 0x66ULL) == tlm::TLM_OK_RESPONSE,
              "sep_out own-aperture status");
        check(t_ext_out.calls == 2 && t_sep_in.calls == 0,
              "sep_out own-aperture non-reflexive");

        // smc_out -> sep_in (SEP aperture), read path returns stub data.
        check(do_read(init_smc, 0x5000'0100ULL, data) == tlm::TLM_OK_RESPONSE,
              "smc_out->sep_in status");
        check(t_sep_in.calls == 1 && t_sep_in.last_addr == 0x5000'0100ULL &&
              t_sep_in.last_cmd == tlm::TLM_READ_COMMAND &&
              data == (t_sep_in.read_data & 0xffffffffULL),
              "smc_out->sep_in addr/rdata");

        // smc_out -> ext_out catch-all (also for the SMC's own aperture).
        check(do_write(init_smc, 0x4000'0000ULL, 0x77ULL) == tlm::TLM_OK_RESPONSE,
              "smc_out own-aperture status");
        check(t_ext_out.calls == 3 && t_smc_in.calls == 1,
              "smc_out own-aperture non-reflexive");
        check(do_read(init_smc, 0x1234ULL, data) == tlm::TLM_OK_RESPONSE,
              "smc_out->ext_out status");
        check(t_ext_out.calls == 4, "smc_out->ext_out route");

        // ext_in -> sep_in / smc_in (both apertures reachable from outside).
        check(do_write(init_ext, 0x5000'0200ULL, 0x88ULL) == tlm::TLM_OK_RESPONSE,
              "ext_in->sep_in status");
        check(t_sep_in.calls == 2 && t_sep_in.last_addr == 0x5000'0200ULL,
              "ext_in->sep_in addr");
        check(do_write(init_ext, 0x4000'0200ULL, 0x99ULL) == tlm::TLM_OK_RESPONSE,
              "ext_in->smc_in status");
        check(t_smc_in.calls == 2 && t_smc_in.last_addr == 0x4000'0200ULL,
              "ext_in->smc_in addr");

        // ext_in outside both apertures: no legal route -> address error,
        // and no subordinate is touched.
        const unsigned sep_calls = t_sep_in.calls, smc_calls = t_smc_in.calls;
        check(do_write(init_ext, 0x9000'0000ULL, 0xaaULL) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "ext_in decode miss status");
        check(t_sep_in.calls == sep_calls && t_smc_in.calls == smc_calls,
              "ext_in decode miss not forwarded");

        // 8-byte access keeps full data width intact end-to-end.
        check(do_write(init_sep, 0x4000'1000ULL, 0x01020304'05060708ULL, 8) ==
              tlm::TLM_OK_RESPONSE, "8-byte write status");
        check(t_smc_in.last_data == 0x01020304'05060708ULL, "8-byte write data");
    }

    // -----------------------------------------------------------------------
    void test_xbar_dbg()
    {
        uint64_t data = 0;
        tlm::tlm_response_status st = tlm::TLM_OK_RESPONSE;

        // Debug transport follows the same routing (sep_out -> smc_in).
        const unsigned n = do_dbg(init_sep, 0x4000'0004ULL, data, st);
        check(n == 4 && st == tlm::TLM_OK_RESPONSE, "dbg sep_out->smc_in status");
        check(t_smc_in.dbg_calls == 1 && t_smc_in.dbg_addr == 0x4000'0004ULL &&
              data == (t_smc_in.read_data & 0xffffffffULL), "dbg sep_out->smc_in addr/rdata");

        // Debug transport on the ext_in decode miss: 0 bytes + address error.
        const unsigned n0 = do_dbg(init_ext, 0x9000'0000ULL, data, st);
        check(n0 == 0 && st == tlm::TLM_ADDRESS_ERROR_RESPONSE, "dbg ext_in miss");

        // Debug transport via smc_out -> sep_in and ext_in -> smc_in.
        check(do_dbg(init_smc, 0x5000'0000ULL, data, st) == 4 &&
              t_sep_in.dbg_calls == 1, "dbg smc_out->sep_in");
        check(do_dbg(init_ext, 0x4000'0000ULL, data, st) == 4 &&
              t_smc_in.dbg_calls == 2, "dbg ext_in->smc_in");
        check(do_dbg(init_ext, 0x5000'0000ULL, data, st) == 4 &&
              t_sep_in.dbg_calls == 2, "dbg ext_in->sep_in");
        check(do_dbg(init_smc, 0x9000'0000ULL, data, st) == 4 &&
              t_ext_out.dbg_calls == 1, "dbg smc_out->ext_out");
    }

    // -----------------------------------------------------------------------
    void test_xbar_error_propagation()
    {
        // A subordinate's error response propagates back to the master.
        t_ext_out.rsp = tlm::TLM_GENERIC_ERROR_RESPONSE;
        check(do_write(init_sep, 0x9000'0000ULL, 0x1ULL) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "xbar error propagation");
        t_ext_out.rsp = tlm::TLM_OK_RESPONSE;
    }

    // -----------------------------------------------------------------------
    void test_xbar_runtime_reprogram()
    {
        // Aperture CSRs are CCI-mutable and re-read per transaction.
        cci::cci_broker_handle broker = cci::cci_get_broker();
        broker.get_param_handle("tb.smu_xbar.sep_global_base")
              .set_cci_value(cci::cci_value(uint64_t(0x6000'0000ULL)));
        broker.get_param_handle("tb.smu_xbar.sep_region_size")
              .set_cci_value(cci::cci_value(uint64_t(0x1000ULL)));

        const unsigned sep_calls = t_sep_in.calls, ext_calls = t_ext_out.calls;

        // New aperture routes to sep_in ...
        check(do_write(init_smc, 0x6000'0000ULL, 0x2ULL) == tlm::TLM_OK_RESPONSE,
              "reprogrammed aperture status");
        check(t_sep_in.calls == sep_calls + 1, "reprogrammed aperture route");

        // ... the old aperture base no longer matches (falls to ext_out) ...
        check(do_write(init_smc, 0x5000'0000ULL, 0x3ULL) == tlm::TLM_OK_RESPONSE,
              "stale aperture status");
        check(t_ext_out.calls == ext_calls + 1, "stale aperture route");

        // ... and the shrunken region size is honoured.
        check(do_write(init_smc, 0x6000'1000ULL, 0x4ULL) == tlm::TLM_OK_RESPONSE,
              "region size status");
        check(t_ext_out.calls == ext_calls + 2, "region size honoured");
    }

    // -----------------------------------------------------------------------
    void test_remap_window()
    {
        uint64_t data = 0;

        // In-window base: 0x4000_0000 -> 0x0, caller's address restored.
        check(do_write(init_r1, 0x4000'0000ULL, 0xabcdULL) == tlm::TLM_OK_RESPONSE,
              "remap base status");
        check(t_r1.calls == 1 && t_r1.last_addr == 0x0ULL && t_r1.last_data == 0xabcdULL,
              "remap base addr/data");

        // In-window offset read: 0x4000_1000 -> 0x1000, data flows back.
        check(do_read(init_r1, 0x4000'1000ULL, data) == tlm::TLM_OK_RESPONSE,
              "remap read status");
        check(t_r1.last_addr == 0x1000ULL && data == (t_r1.read_data & 0xffffffffULL),
              "remap read addr/rdata");

        // Last in-window address.
        check(do_write(init_r1, 0x7fff'fffcULL, 0x1ULL) == tlm::TLM_OK_RESPONSE,
              "remap last in-window status");
        check(t_r1.last_addr == 0x3fff'fffcULL, "remap last in-window addr");

        // Just below / just above the window pass through unchanged.
        check(do_write(init_r1, 0x3fff'fffcULL, 0x2ULL) == tlm::TLM_OK_RESPONSE,
              "remap below-window status");
        check(t_r1.last_addr == 0x3fff'fffcULL, "remap below-window passthrough");
        check(do_write(init_r1, 0x8000'0000ULL, 0x3ULL) == tlm::TLM_OK_RESPONSE,
              "remap above-window status");
        check(t_r1.last_addr == 0x8000'0000ULL, "remap above-window passthrough");

        // The caller-visible address is restored after an in-window call.
        check(write_and_report_final_addr(init_r1, 0x4000'2000ULL, 0x4ULL) == 0x4000'2000ULL,
              "remap caller address restored");
        check(t_r1.last_addr == 0x2000ULL, "remap target saw rebased address");

        // Second instance: target_base above the alias base (0x1000 -> 0x8000).
        check(do_write(init_r2, 0x1000ULL, 0x5ULL) == tlm::TLM_OK_RESPONSE,
              "remap2 in-window status");
        check(t_r2.calls == 1 && t_r2.last_addr == 0x8000ULL, "remap2 upward remap");
        check(do_write(init_r2, 0x2000ULL, 0x6ULL) == tlm::TLM_OK_RESPONSE,
              "remap2 above-window status");
        check(t_r2.last_addr == 0x2000ULL, "remap2 above-window passthrough");
    }

    // -----------------------------------------------------------------------
    void test_remap_dbg()
    {
        uint64_t data = 0;
        tlm::tlm_response_status st = tlm::TLM_OK_RESPONSE;

        // Debug transport is remapped the same way (in-window).
        check(do_dbg(init_r1, 0x4000'0000ULL, data, st) == 4, "remap dbg in-window n");
        check(t_r1.dbg_calls == 1 && t_r1.dbg_addr == 0x0ULL && data == (t_r1.read_data & 0xffffffffULL),
              "remap dbg in-window addr/rdata");

        // Out-of-window debug transport passes through.
        check(do_dbg(init_r1, 0x9000'0000ULL, data, st) == 4, "remap dbg out-window n");
        check(t_r1.dbg_addr == 0x9000'0000ULL, "remap dbg out-window passthrough");
    }

    // -----------------------------------------------------------------------
    void test_remap_error_propagation()
    {
        t_r1.rsp = tlm::TLM_GENERIC_ERROR_RESPONSE;
        check(do_write(init_r1, 0x4000'0000ULL, 0x7ULL) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "remap error propagation");
        t_r1.rsp = tlm::TLM_OK_RESPONSE;
    }
};

// ===========================================================================
int sc_main(int, char**)
{
    // Global broker must exist before any cci_param is constructed.
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    smu_interconnect_tb tb{"tb"};
    sc_core::sc_start();
    return tb.failures == 0 ? 0 : 1;
}
