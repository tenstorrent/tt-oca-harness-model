// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_scratch_warm_testbench.cpp
 * @brief Testbench for the warm-domain scratch register bank.
 *
 * Implements the three cases in docs/03_sep_scratch_warm_Test_Plan.md. The model
 * is a plain store with one behaviour worth checking beyond readback — rst_ni
 * clearing programmed state — so the suite is deliberately small rather than
 * padded out to match larger peripherals.
 */

#include <systemc.h>
#include <tlm.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

#include "sep_scratch_warm.h"
#include "sep_scratch_warm_test.h"

namespace {

/// Byte stride between SCRATCH entries (one 64-bit word each).
constexpr unsigned SCRATCH_STRIDE = 8;
constexpr unsigned NUM_SCRATCH    = 8;

/// Only the lower 32 bits are implemented; Reserved0 above them reads 0.
constexpr uint64_t DATA_MASK = 0xFFFFFFFFULL;

unsigned scratch_offset(unsigned idx) { return idx * SCRATCH_STRIDE; }

}  // namespace

class testbench : public sc_module
{
  public:
    SC_HAS_PROCESS(testbench);

    explicit testbench(sc_module_name name)
        : sc_module(name)
        , m_test("m_test")
        , dut("dut")
        , rst_n_sig("rst_n_sig")
    {
        m_test.initiator_socket.bind(dut.target_socket);
        dut.rst_ni(rst_n_sig);
        rst_n_sig.write(true);

        SC_THREAD(run_tests);
    }

    int failures() const { return m_failures; }

  private:
    sep_scratch_warm_test           m_test;
    sep_scratch_warm_ip             dut;
    sc_core::sc_signal<bool>        rst_n_sig;
    int                             m_failures = 0;

    void check(bool cond, const std::string& what)
    {
        if (cond) {
            std::cout << "[PASS] " << what << std::endl;
        } else {
            std::cout << "[FAIL] " << what << std::endl;
            ++m_failures;
        }
    }

    uint64_t read64(unsigned offset)
    {
        tlm::tlm_generic_payload trans;
        uint64_t data = 0;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        m_test.initiator_socket->b_transport(trans, delay);
        return data;
    }

    void write64(unsigned offset, uint64_t value)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(8);
        trans.set_streaming_width(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        m_test.initiator_socket->b_transport(trans, delay);
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-001: reset values
    // -----------------------------------------------------------------------
    void test_reset_values()
    {
        bool all_zero = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            if (read64(scratch_offset(i)) != 0) all_zero = false;

        check(all_zero, "FUNC-SCRATCHWARM-001: all 8 SCRATCH entries read 0 at power-on");
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-002: read/write and reserved-bit masking
    // -----------------------------------------------------------------------
    void test_readwrite_and_masking()
    {
        // A distinct pattern per entry, so a stride or aliasing error cannot pass.
        bool readback_ok = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i) {
            const uint64_t pattern = 0xA5A50000ULL | (i * 0x11u);
            write64(scratch_offset(i), pattern);
            if (read64(scratch_offset(i)) != pattern) readback_ok = false;
        }
        check(readback_ok, "FUNC-SCRATCHWARM-002a: per-entry write/readback");

        // Reserved0 occupies bits [63:32] and must stay 0 however it is written.
        // The wide write also confirms the lower half is unaffected by the mask.
        write64(scratch_offset(3), 0xFFFFFFFFFFFFFFFFULL);
        const uint64_t wide = read64(scratch_offset(3));
        check(wide == DATA_MASK,
              "FUNC-SCRATCHWARM-002b: Reserved0 masked on a full 64-bit write");

        // Same check one byte at a time, which is the narrowest path into the
        // reserved half and the one a masking bug is most likely to survive.
        // Entry 4 still holds its 002a pattern, which is the point: the byte writes
        // must leave the reserved half at zero *and* the data half untouched.
        const uint64_t entry4_pattern = 0xA5A50000ULL | (4u * 0x11u);
        m_test.register_write_8(scratch_offset(4) + 4, 0xFFu);
        m_test.register_write_8(scratch_offset(4) + 7, 0xFFu);
        const uint64_t bytewise = read64(scratch_offset(4));
        {
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-002c: Reserved0 masked on byte writes (read 0x"
                << std::hex << bytewise << ")";
            check(bytewise == entry4_pattern, oss.str());
        }

        uint8_t low_byte = 0;
        m_test.register_write_8(scratch_offset(5), 0x5Au);
        m_test.register_read_8(scratch_offset(5), low_byte);
        check(low_byte == 0x5Au, "FUNC-SCRATCHWARM-002d: byte access to the data half");
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-003: rst_ni clears programmed state
    //
    // Distinct from case 001: that one only sees the constructor's defaults.
    // This proves reset_handler() is actually wired to the port, which a model
    // that merely initialised its storage would fail.
    // -----------------------------------------------------------------------
    void test_reset_clears_state()
    {
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            write64(scratch_offset(i), 0xDEADBEEFULL);

        bool programmed = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            if (read64(scratch_offset(i)) != 0xDEADBEEFULL) programmed = false;
        check(programmed, "FUNC-SCRATCHWARM-003a: entries hold programmed values");

        // reset_handler is an SC_METHOD on rst_ni, so the scheduler has to run
        // for the edge to be delivered — hence real time either side of the pulse.
        rst_n_sig.write(false);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);

        bool cleared = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            if (read64(scratch_offset(i)) != 0) cleared = false;
        check(cleared, "FUNC-SCRATCHWARM-003b: rst_ni pulse clears all entries");
    }

    void run_tests()
    {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "SEP SCRATCH WARM TESTBENCH" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        test_reset_values();
        test_readwrite_and_masking();
        test_reset_clears_state();

        std::cout << std::string(70, '=') << std::endl;
        if (m_failures == 0)
            std::cout << "ALL TESTS PASSED" << std::endl;
        else
            std::cout << m_failures << " TEST(S) FAILED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        sc_core::sc_stop();
    }
};

int sc_main(int, char*[])
{
    testbench tb("tb");
    sc_core::sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    // Bypass SystemC/CCI teardown. Returning from sc_main leaks and aborts
    // under ASan, which is why every other SEP peripheral testbench does this.
    std::quick_exit(tb.failures() == 0 ? 0 : 1);
    return 0;
}
