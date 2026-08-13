/**
 * @file sep_output_remap_ctrl_testbench.cpp
 * @brief Smoke-test testbench for sep_output_remap_ctrl_ip (generic model).
 *
 * Exercises the model with both the AP and STEE hardware configurations to
 * verify dual-instantiation — mirroring how och_sep_ss.hpp uses the class:
 *
 *   sep_output_remap_ctrl_ip ap_output_remap  ("ap_output_remap",   0x11000000UL, 16, 19);
 *   sep_output_remap_ctrl_ip stee_output_remap("stee_output_remap", 0x11800000UL, 16, 19);
 *
 * Test suite A (T1–T9): AP instance — full 9-case test covering CSR R/W,
 *   56-bit mask, data-path remap, address restore, and transport_dbg.
 *
 * Test suite B (B1–B3): STEE instance — sanity checks using STEE_BASE to
 *   confirm the parameterised constructor produces independent state.
 *
 * Wiring per instance:
 *   test_harness.initiator_socket ──► dut.target_socket   (CSR path)
 *   data_initiator                ──► dut.data_socket      (data path in)
 *   dut.remapped_socket           ──► stub.socket          (data path out)
 */

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "sep_output_remap_ctrl.h"
#include "sep_output_remap_ctrl_test.h"

#include <cassert>
#include <iostream>
#include <iomanip>
#include <cstring>
#include "csml_parameter.h"

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// =============================================================================
// StubTarget — records the last remapped address seen on the fabric side.
// =============================================================================
struct StubTarget : sc_core::sc_module
{
    tlm_utils::simple_target_socket<StubTarget> socket;
    uint64_t last_addr = 0;

    SC_CTOR(StubTarget) : socket("socket")
    {
        socket.register_b_transport(this,   &StubTarget::b_transport);
        socket.register_transport_dbg(this, &StubTarget::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        last_addr = trans.get_address();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay = sc_core::SC_ZERO_TIME;
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        last_addr = trans.get_address();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};

// =============================================================================
// Testbench
// =============================================================================
SC_MODULE(sep_output_remap_ctrl_testbench)
{
    SC_HAS_PROCESS(sep_output_remap_ctrl_testbench);

    // -------------------------------------------------------------------------
    // Hardware constants — AP instance (matches Args.hpp / och_sep_ss.hpp)
    // -------------------------------------------------------------------------
    static constexpr uint64_t AP_REGION_BASE   = 0x11000000ULL;
    static constexpr uint32_t AP_NUM_REGIONS   = 16;
    static constexpr uint32_t AP_IDX_START     = 19;
    static constexpr uint32_t CSR_STRIDE       = 8;  // bytes per REGION_ATTRS entry

    // -------------------------------------------------------------------------
    // Hardware constants — STEE instance
    // -------------------------------------------------------------------------
    static constexpr uint64_t STEE_REGION_BASE = 0x11800000ULL;
    static constexpr uint32_t STEE_NUM_REGIONS = 16;
    static constexpr uint32_t STEE_IDX_START   = 19;

    // Drive rst_ni high (deasserted) throughout the testbench
    sc_core::sc_signal<bool> rst_n_sig;

    // -------------------------------------------------------------------------
    // AP instance and its harness
    // -------------------------------------------------------------------------
    sep_output_remap_ctrl_ip   dut_ap;
    sep_output_remap_ctrl_test harness_ap;
    StubTarget                 stub_ap;
    tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_testbench> data_init_ap;

    // -------------------------------------------------------------------------
    // STEE instance and its harness
    // -------------------------------------------------------------------------
    sep_output_remap_ctrl_ip   dut_stee;
    sep_output_remap_ctrl_test harness_stee;
    StubTarget                 stub_stee;
    tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_testbench> data_init_stee;

    // -------------------------------------------------------------------------
    // Test statistics
    // -------------------------------------------------------------------------
    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;

    // =========================================================================
    // Constructor
    // =========================================================================
    sep_output_remap_ctrl_testbench(sc_module_name name)
        : sc_module(name)
        , dut_ap   ("dut_ap",   sep_output_remap_ctrl_ip::InstanceType::AP)
        , harness_ap("harness_ap")
        , stub_ap  ("stub_ap")
        , data_init_ap("data_init_ap")
        , dut_stee ("dut_stee", sep_output_remap_ctrl_ip::InstanceType::STEE)
        , harness_stee("harness_stee")
        , stub_stee("stub_stee")
        , data_init_stee("data_init_stee")
    {
        rst_n_sig.write(true);  // deassert reset for entire testbench run
        dut_ap.rst_ni(rst_n_sig);
        dut_stee.rst_ni(rst_n_sig);

        harness_ap.initiator_socket.bind(dut_ap.target_socket);
        data_init_ap.bind(dut_ap.data_socket);
        dut_ap.remapped_socket.bind(stub_ap.socket);

        harness_stee.initiator_socket.bind(dut_stee.target_socket);
        data_init_stee.bind(dut_stee.data_socket);
        dut_stee.remapped_socket.bind(stub_stee.socket);

        SC_THREAD(run_tests);
    }

    // =========================================================================
    // TLM helpers (data-path)
    // =========================================================================
    void data_read(tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_testbench>& sock,
                   uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        sock->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    void data_write(tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_testbench>& sock,
                    uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        sock->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    void data_dbg(tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_testbench>& sock,
                  uint64_t addr, uint64_t& buf)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        sock->transport_dbg(trans);
    }

    void report(const char* name, bool pass)
    {
        m_tests_run++;
        if (pass) { m_tests_passed++; std::cout << "[PASS] " << name << "\n"; }
        else      { m_tests_failed++; std::cout << "[FAIL] " << name << "\n"; }
    }

    // =========================================================================
    // run_tests — SC_THREAD entry point
    // =========================================================================
    void run_tests()
    {
        // ======================================================================
        // Suite A: AP instance — full 9 cases
        // ======================================================================
        std::cout << "\n=== Suite A: sep_output_remap_ctrl (AP params) ===\n"
                  << std::hex
                  << "  RegionBase  = 0x" << AP_REGION_BASE << "\n"
                  << std::dec
                  << "  NumRegions  = " << AP_NUM_REGIONS << "\n"
                  << "  IdxStart    = " << AP_IDX_START   << "\n\n";

        // ------------------------------------------------------------------
        // T1: Reset state — all 16 REGION_ATTRS read back as 0
        // ------------------------------------------------------------------
        {
            bool pass = true;
            for (uint32_t i = 0; i < AP_NUM_REGIONS; ++i) {
                uint64_t v = 0xDEAD;
                harness_ap.register_read_64(i * CSR_STRIDE, v);
                if (v != 0) { pass = false; break; }
            }
            report("T1: All region offsets reset to 0", pass);
        }

        // ------------------------------------------------------------------
        // T2: Write offset to region 0, read back correctly
        // ------------------------------------------------------------------
        {
            const uint64_t offset0 = 0x0000002000000000ULL;
            harness_ap.register_write_64(0 * CSR_STRIDE, offset0);
            uint64_t v = 0;
            harness_ap.register_read_64(0 * CSR_STRIDE, v);
            report("T2: Region 0 offset write/readback",
                   v == (offset0 & 0x00FFFFFFFFFFFFFFULL));
        }

        // ------------------------------------------------------------------
        // T3: Write all 16 regions, verify each reads back
        // ------------------------------------------------------------------
        {
            bool pass = true;
            for (uint32_t i = 0; i < AP_NUM_REGIONS; ++i) {
                uint64_t off = static_cast<uint64_t>(i + 1) << AP_IDX_START;
                harness_ap.register_write_64(i * CSR_STRIDE, off);
            }
            for (uint32_t i = 0; i < AP_NUM_REGIONS; ++i) {
                uint64_t expected = (static_cast<uint64_t>(i + 1) << AP_IDX_START)
                                    & 0x00FFFFFFFFFFFFFFULL;
                uint64_t got = 0;
                harness_ap.register_read_64(i * CSR_STRIDE, got);
                if (got != expected) { pass = false; break; }
            }
            report("T3: All 16 region offsets written and verified", pass);
        }

        // ------------------------------------------------------------------
        // T4: 56-bit mask — bits [63:56] discarded on write
        // ------------------------------------------------------------------
        {
            harness_ap.register_write_64(0 * CSR_STRIDE, 0xFFFFFFFFFFFFFFFFULL);
            uint64_t v = 0;
            harness_ap.register_read_64(0 * CSR_STRIDE, v);
            report("T4: 56-bit mask enforced on REGION_ATTRS.offset",
                   v == 0x00FFFFFFFFFFFFFFULL);
        }

        // ------------------------------------------------------------------
        // T5: Data-path remap — region 0 READ
        //
        //   REGION_ATTRS[0].offset = 0x0040_0000_0000
        //   incoming = AP_BASE + 0x1234  (region 0: adjusted[22:19]=0)
        //   lower_mask = (1<<19)-1 = 0x7FFFF
        //   expected = (0x4000000000 & ~0x7FFFF) | (0x1234 & 0x7FFFF)
        //            = 0x4000000000 | 0x01234 = 0x4000001234
        // ------------------------------------------------------------------
        {
            const uint64_t offset_t5  = 0x0000004000000000ULL;
            const uint64_t incoming   = AP_REGION_BASE + 0x00001234ULL;
            const uint64_t lower_mask = (1ULL << AP_IDX_START) - 1ULL;
            const uint64_t expected   = (offset_t5 & ~lower_mask)
                                      | (0x00001234ULL & lower_mask);

            harness_ap.register_write_64(0 * CSR_STRIDE, offset_t5);
            uint64_t dummy = 0;
            data_read(data_init_ap, incoming, dummy);
            report("T5: Data-path remap region 0 (READ)", stub_ap.last_addr == expected);
        }

        // ------------------------------------------------------------------
        // T6: Data-path remap — region 3 WRITE
        //
        //   REGION_ATTRS[3].offset = 0xF000_0000
        //   incoming = AP_BASE + (3<<19) + 0xABC = 0x11600ABC
        //   idx = bits[22:19] of adjusted = 3
        //   lower = 0x000ABC
        //   expected = (0xF0000000 & ~0x7FFFF) | 0x000ABC = 0xF0000ABC
        // ------------------------------------------------------------------
        {
            const uint64_t offset_t6  = 0x00000000F0000000ULL;
            const uint64_t incoming   = AP_REGION_BASE + (3ULL << AP_IDX_START) + 0xABCULL;
            const uint64_t lower_mask = (1ULL << AP_IDX_START) - 1ULL;
            const uint64_t expected   = (offset_t6 & ~lower_mask)
                                      | ((incoming - AP_REGION_BASE) & lower_mask);

            harness_ap.register_write_64(3 * CSR_STRIDE, offset_t6);
            uint64_t dummy = 0;
            data_write(data_init_ap, incoming, dummy);
            report("T6: Data-path remap region 3 (WRITE)", stub_ap.last_addr == expected);
        }

        // ------------------------------------------------------------------
        // T7: Original address restored on payload after remap
        // ------------------------------------------------------------------
        {
            const uint64_t incoming = AP_REGION_BASE + 0x00000100ULL;
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay  = sc_core::SC_ZERO_TIME;
            uint64_t dummy = 0;
            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_address(incoming);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&dummy));
            trans.set_data_length(8);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_streaming_width(8);
            data_init_ap->b_transport(trans, delay);
            report("T7: Original address restored on payload after remap",
                   trans.get_address() == incoming);
        }

        // ------------------------------------------------------------------
        // T8: csml_memory 56-bit mask via byte-enable write
        //   (csml_memory ignores byte enables — full write + 56-bit mask)
        // ------------------------------------------------------------------
        {
            uint64_t zero = 0;
            harness_ap.register_write_64(5 * CSR_STRIDE, zero);

            uint64_t wdata = 0xFFFFFFFFFFFFFFFFULL;
            uint8_t  be[8] = {
                TLM_BYTE_ENABLED, TLM_BYTE_ENABLED,
                TLM_BYTE_ENABLED, TLM_BYTE_ENABLED,
                TLM_BYTE_DISABLED, TLM_BYTE_DISABLED,
                TLM_BYTE_DISABLED, TLM_BYTE_DISABLED
            };
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(5 * CSR_STRIDE);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&wdata));
            trans.set_data_length(8);
            trans.set_byte_enable_ptr(be);
            trans.set_byte_enable_length(8);
            trans.set_streaming_width(8);
            harness_ap.initiator_socket->b_transport(trans, delay);

            uint64_t v = 0;
            harness_ap.register_read_64(5 * CSR_STRIDE, v);
            report("T8: 56-bit field mask enforced (byte enables not honoured by csml)",
                   v == 0x00FFFFFFFFFFFFFFULL);
        }

        // ------------------------------------------------------------------
        // T9: transport_dbg applies the same remap
        // ------------------------------------------------------------------
        {
            const uint64_t offset_t5  = 0x0000004000000000ULL;
            const uint64_t incoming   = AP_REGION_BASE + 0x00001234ULL;
            const uint64_t lower_mask = (1ULL << AP_IDX_START) - 1ULL;
            const uint64_t expected   = (offset_t5 & ~lower_mask)
                                      | (0x00001234ULL & lower_mask);

            harness_ap.register_write_64(0 * CSR_STRIDE, offset_t5);
            uint64_t dummy = 0;
            data_dbg(data_init_ap, incoming, dummy);
            report("T9: transport_dbg applies same remap", stub_ap.last_addr == expected);
        }

        // ------------------------------------------------------------------
        // T10: rst_ni actually clears programmed region state
        //
        // T1 only checked that freshly-constructed registers start at 0 —
        // that passes even if reset_handler()/reset() were never wired up,
        // since rst_n_sig is otherwise held high (deasserted) for the whole
        // run. This drives a real reset pulse and confirms it clears state
        // that T2/T3/T5/T6/T9 deliberately left non-zero.
        // ------------------------------------------------------------------
        {
            uint64_t pre = 0;
            harness_ap.register_read_64(0 * CSR_STRIDE, pre);
            bool preconditions_ok = (pre != 0);

            rst_n_sig.write(false);   // assert reset
            wait(10, sc_core::SC_NS);
            rst_n_sig.write(true);    // deassert reset
            wait(10, sc_core::SC_NS);

            bool pass = preconditions_ok;
            for (uint32_t i = 0; i < AP_NUM_REGIONS; ++i) {
                uint64_t v = 0xDEAD;
                harness_ap.register_read_64(i * CSR_STRIDE, v);
                if (v != 0) { pass = false; break; }
            }
            report("T10: rst_ni pulse clears previously-programmed regions", pass);
        }

        // ======================================================================
        // Suite B: STEE instance — sanity checks for dual-instantiation
        // ======================================================================
        std::cout << "\n=== Suite B: sep_output_remap_ctrl (STEE params) ===\n"
                  << std::hex
                  << "  RegionBase  = 0x" << STEE_REGION_BASE << "\n"
                  << std::dec
                  << "  NumRegions  = " << STEE_NUM_REGIONS << "\n"
                  << "  IdxStart    = " << STEE_IDX_START   << "\n\n";

        // ------------------------------------------------------------------
        // B1: Independent reset state (no cross-contamination from Suite A)
        // ------------------------------------------------------------------
        {
            bool pass = true;
            for (uint32_t i = 0; i < STEE_NUM_REGIONS; ++i) {
                uint64_t v = 0xDEAD;
                harness_stee.register_read_64(i * CSR_STRIDE, v);
                if (v != 0) { pass = false; break; }
            }
            report("B1: STEE instance reset state independent of AP instance", pass);
        }

        // ------------------------------------------------------------------
        // B2: Data-path remap with STEE base address
        //
        //   REGION_ATTRS[0].offset = 0x0040_0000_0000
        //   incoming = STEE_BASE + 0x1234
        //   expected = same algorithm, different base
        // ------------------------------------------------------------------
        {
            const uint64_t offset_b2  = 0x0000004000000000ULL;
            const uint64_t incoming   = STEE_REGION_BASE + 0x00001234ULL;
            const uint64_t lower_mask = (1ULL << STEE_IDX_START) - 1ULL;
            const uint64_t expected   = (offset_b2 & ~lower_mask)
                                      | (0x00001234ULL & lower_mask);

            harness_stee.register_write_64(0 * CSR_STRIDE, offset_b2);
            uint64_t dummy = 0;
            data_read(data_init_stee, incoming, dummy);
            report("B2: STEE data-path remap region 0 (READ)", stub_stee.last_addr == expected);
        }

        // ------------------------------------------------------------------
        // B3: AP CSR writes do not affect STEE registers
        // ------------------------------------------------------------------
        {
            harness_ap.register_write_64(2 * CSR_STRIDE, 0xDEADBEEFDEADBEEFULL);
            uint64_t v = 0xDEAD;
            harness_stee.register_read_64(2 * CSR_STRIDE, v);
            report("B3: AP CSR write does not affect STEE register", v == 0);
        }

        // ======================================================================
        // Summary
        // ======================================================================
        std::cout << "\n=== Test Summary: "
                  << m_tests_passed << "/" << m_tests_run << " passed";
        if (m_tests_failed > 0)
            std::cout << " [" << m_tests_failed << " FAILED]";
        std::cout << " ===\n";

        sc_core::sc_stop();
    }
};

// =============================================================================
// sc_main
// =============================================================================
int sc_main(int argc, char* argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);
    sep_output_remap_ctrl_testbench tb("tb");
    sc_core::sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);
    return 0;
}
