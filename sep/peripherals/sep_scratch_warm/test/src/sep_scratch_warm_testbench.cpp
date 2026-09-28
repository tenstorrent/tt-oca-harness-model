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
#include "tlm_probe.h"

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

    // Every access goes through simtlm, which hands back the response status
    // instead of discarding it. A transport that fails is now a test failure at
    // the point of access rather than a zero that quietly satisfies whatever
    // reset or masking expectation happened to be next.
    uint64_t read64(unsigned offset)
    {
        uint64_t data = 0;
        const auto r = simtlm::read_word<uint64_t>(m_test.initiator_socket, offset, data);
        expect_ok(r, "read64", offset);
        return data;
    }

    void write64(unsigned offset, uint64_t value)
    {
        const auto r = simtlm::write_word<uint64_t>(m_test.initiator_socket, offset, value);
        expect_ok(r, "write64", offset);
    }

    /// Fail the run if a transaction that should have been serviced was not.
    void expect_ok(const simtlm::access_result& r, const char* what, unsigned offset)
    {
        if (r.ok())
            return;
        std::ostringstream oss;
        oss << what << " at offset 0x" << std::hex << offset
            << " returned " << simtlm::response_name(r.status);
        check(false, oss.str());
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
        expect_ok(m_test.register_write_8(scratch_offset(4) + 4, 0xFFu),
                  "register_write_8", scratch_offset(4) + 4);
        expect_ok(m_test.register_write_8(scratch_offset(4) + 7, 0xFFu),
                  "register_write_8", scratch_offset(4) + 7);
        const uint64_t bytewise = read64(scratch_offset(4));
        {
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-002c: Reserved0 masked on byte writes (read 0x"
                << std::hex << bytewise << ")";
            check(bytewise == entry4_pattern, oss.str());
        }

        // Sentinel is deliberately not 0x5A: if the transport were to fail, the
        // helper leaves it alone and the check below fails loudly, rather than
        // the old behaviour where a forced 0 could look like a legitimate read.
        uint8_t low_byte = 0xFF;
        expect_ok(m_test.register_write_8(scratch_offset(5), 0x5Au),
                  "register_write_8", scratch_offset(5));
        expect_ok(m_test.register_read_8(scratch_offset(5), low_byte),
                  "register_read_8", scratch_offset(5));
        check(low_byte == 0x5Au, "FUNC-SCRATCHWARM-002d: byte access to the data half");
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-005: explicit byte enables
    //
    // The narrow writes in 002 pass a null BE pointer and let address+length
    // imply the lanes, which never reaches byte_enable_to_mask()/merge_be() in
    // the model. These send a real BE array so the merge is actually proven,
    // lane by lane, including the short-array case TLM says must repeat.
    // -----------------------------------------------------------------------
    void test_explicit_byte_enables()
    {
        const unsigned off = scratch_offset(6);
        const unsigned char EN  = simtlm::BYTE_ENABLED;
        const unsigned char DIS = simtlm::BYTE_DISABLED;

        // One-hot: only lane 1 may move; the other three data lanes must keep
        // their previous contents.
        write64(off, 0x11223344ULL);
        {
            const unsigned char be[8] = {DIS, EN, DIS, DIS, DIS, DIS, DIS, DIS};
            expect_ok(simtlm::write_word_be<uint64_t>(m_test.initiator_socket, off,
                                                      0xAABBCCDDULL, be, sizeof(be)),
                      "write_word_be one-hot", off);
            const uint64_t got = read64(off);
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-005a: one-hot BE merges lane 1 only (read 0x"
                << std::hex << got << ")";
            check(got == 0x1122CC44ULL, oss.str());
        }

        // All lanes disabled is legal TLM and must be a no-op, not a zero fill.
        {
            const unsigned char be[8] = {DIS, DIS, DIS, DIS, DIS, DIS, DIS, DIS};
            expect_ok(simtlm::write_word_be<uint64_t>(m_test.initiator_socket, off,
                                                      0xFFFFFFFFULL, be, sizeof(be)),
                      "write_word_be all-disabled", off);
            const uint64_t got = read64(off);
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-005b: all-disabled BE leaves the word unchanged (read 0x"
                << std::hex << got << ")";
            check(got == 0x1122CC44ULL, oss.str());
        }

        // Non-contiguous lanes, to catch a mask built with a range rather than
        // a per-bit test.
        write64(off, 0x00000000ULL);
        {
            const unsigned char be[8] = {EN, DIS, EN, DIS, DIS, DIS, DIS, DIS};
            simtlm::write_word_be<uint64_t>(m_test.initiator_socket, off,
                                            0xEEEEEEEEULL, be, sizeof(be));
            const uint64_t got = read64(off);
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-005c: non-contiguous BE merges lanes 0 and 2 (read 0x"
                << std::hex << got << ")";
            check(got == 0x00EE00EEULL, oss.str());
        }

        // A BE array shorter than the data length repeats across the transfer.
        // With a 2-byte pattern {EN, DIS} over 8 bytes, the even lanes move.
        write64(off, 0x00000000ULL);
        {
            const unsigned char be[2] = {EN, DIS};
            simtlm::write_word_be<uint64_t>(m_test.initiator_socket, off,
                                            0xFFFFFFFFULL, be, sizeof(be));
            const uint64_t got = read64(off);
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-005d: short BE array repeats across the word (read 0x"
                << std::hex << got << ")";
            check(got == 0x00FF00FFULL, oss.str());
        }

        // Reserved0 must stay masked even when the BE says those lanes are live.
        write64(off, 0x00000000ULL);
        {
            const unsigned char be[8] = {DIS, DIS, DIS, DIS, EN, EN, EN, EN};
            expect_ok(simtlm::write_word_be<uint64_t>(m_test.initiator_socket, off,
                                                      0xFFFFFFFFFFFFFFFFULL, be, sizeof(be)),
                      "write_word_be reserved lanes", off);
            const uint64_t got = read64(off);
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-005e: BE cannot defeat the Reserved0 mask (read 0x"
                << std::hex << got << ")";
            check(got == 0x0ULL, oss.str());
        }

        // A non-null byte-enable pointer with length 0 is illegal. The shared
        // register file must reject it instead of writing every lane.
        write64(off, 0xA5A5A5A5ULL);
        {
            const unsigned char be[1] = {EN};
            const auto r = simtlm::write_word_be<uint64_t>(
                m_test.initiator_socket, off, 0xFFFFFFFFULL, be, 0);
            const uint64_t got = read64(off);
            check(r.status == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE &&
                      got == 0xA5A5A5A5ULL,
                  "FUNC-SCRATCHWARM-005f: BE pointer with length 0 is rejected");
        }
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-006: debug transport and DMI policy
    //
    // Both were previously untested, so "the bank does not support DMI" was an
    // assumption rather than an assertion.
    // -----------------------------------------------------------------------
    void test_debug_and_dmi()
    {
        const unsigned off = scratch_offset(2);
        write64(off, 0x0BADF00DULL);

        uint64_t dbg = 0;
        const unsigned n = simtlm::debug_read(m_test.initiator_socket, off,
                                              reinterpret_cast<unsigned char*>(&dbg),
                                              sizeof(dbg));
        check(n == sizeof(dbg) && dbg == 0x0BADF00DULL,
              "FUNC-SCRATCHWARM-006a: transport_dbg reads the programmed value");

        // Debug writes bypass timing but must still respect the reserved mask,
        // otherwise a debug path can put the bank in a state MMIO cannot.
        uint64_t poke = 0xFFFFFFFFFFFFFFFFULL;
        const unsigned w = simtlm::debug_write(m_test.initiator_socket, off,
                                               reinterpret_cast<unsigned char*>(&poke),
                                               sizeof(poke));
        const uint64_t after = read64(off);
        {
            std::ostringstream oss;
            oss << "FUNC-SCRATCHWARM-006b: debug write transferred " << std::dec << w
                << " bytes, MMIO reads back 0x" << std::hex << after;
            check(w == sizeof(poke) && after == DATA_MASK, oss.str());
        }

        // Record the DMI answer explicitly. regmodel::Memory does not implement
        // get_direct_mem_ptr, so the default refusal is what should be seen; if
        // that ever changes, this fails rather than silently handing out a
        // pointer that bypasses the write mask and the reset handler.
        const auto dmi = simtlm::dmi_request(m_test.initiator_socket, off);
        check(!dmi.granted,
              "FUNC-SCRATCHWARM-006c: DMI is refused, so no path bypasses the register mask");
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-007: reset sensitivity
    //
    // 003 pulses reset once while idle. reset_handler is an SC_METHOD on any
    // edge of rst_ni that acts only while low, so the cases that can actually
    // break it are: already low at startup, writes arriving while held low,
    // and a second assertion with no intervening write.
    // -----------------------------------------------------------------------
    void test_reset_sensitivity()
    {
        // Held low: writes must not survive, because reset keeps re-clearing.
        rst_n_sig.write(false);
        wait(1, sc_core::SC_NS);

        write64(scratch_offset(0), 0xA5A5A5A5ULL);
        // The write lands in storage; the next edge-triggered reset pass is
        // what must remove it. Toggling low->low does not re-trigger, so drive
        // a real edge pair to represent "reset is still asserted downstream".
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(false);
        wait(1, sc_core::SC_NS);

        check(read64(scratch_offset(0)) == 0,
              "FUNC-SCRATCHWARM-007a: a write while reset is asserted does not survive");

        // Repeated assertion with no intervening traffic must stay clear and
        // must not fault.
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(false);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);
        check(read64(scratch_offset(0)) == 0,
              "FUNC-SCRATCHWARM-007b: repeated reset assertions leave the bank clear");

        // Leave the DUT out of reset for whatever runs next.
        bool all_clear = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            if (read64(scratch_offset(i)) != 0) all_clear = false;
        check(all_clear, "FUNC-SCRATCHWARM-007c: every entry is clear after the reset sequence");
    }

    // -----------------------------------------------------------------------
    // FUNC-SCRATCHWARM-004: malformed generic payloads
    //
    // The bank must give a defined answer to every defect in the shared matrix
    // and must not corrupt live state while doing so. What each defect *should*
    // return is the bank's own contract, so the expectations are stated here
    // rather than in the helper.
    // -----------------------------------------------------------------------
    void test_malformed_payloads()
    {
        using simtlm::defect;

        // Park a known pattern in every entry so any collateral damage shows up.
        for (unsigned i = 0; i < NUM_SCRATCH; ++i)
            write64(scratch_offset(i), 0xC0FFEE00ULL | i);

        simtlm::target_geometry geo;
        geo.valid_address  = scratch_offset(1);
        geo.word_bytes     = SCRATCH_STRIDE;
        geo.aperture_bytes = NUM_SCRATCH * SCRATCH_STRIDE;

        bool all_defined = true;
        for (defect d : simtlm::all_defects()) {
            const auto r = simtlm::probe_defect(m_test.initiator_socket, d, geo);

            // TLM_INCOMPLETE_RESPONSE means the target never even looked at the
            // transaction. Any other status is a decision, which is all this
            // case asks for.
            if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                all_defined = false;
                std::cout << "       " << simtlm::defect_name(d)
                          << " left the payload INCOMPLETE" << std::endl;
            }
        }
        check(all_defined,
              "FUNC-SCRATCHWARM-004a: every malformed payload gets a defined response");

        // Entries the matrix cannot legitimately reach must be untouched. The
        // widest defect is a 9-byte access at entry 1, and the unaligned one
        // starts at entry 1 + 1 byte, so both can spill into entry 2 by design —
        // entries 0 and 3..7 are the ones that must not move.
        bool others_intact = true;
        for (unsigned i = 0; i < NUM_SCRATCH; ++i) {
            if (i == 1 || i == 2) continue;
            if (read64(scratch_offset(i)) != (0xC0FFEE00ULL | i)) others_intact = false;
        }
        check(others_intact,
              "FUNC-SCRATCHWARM-004b: malformed payloads do not corrupt unrelated entries");

        // An address past the bank must not alias back into it. Note this does
        // *not* assert a failing response: regmodel::Memory currently services
        // reserved locations as read-zero/write-drop with TLM_OK_RESPONSE, which
        // is a deliberate shared-framework policy rather than a bug in this bank.
        uint64_t past_end = 0x1234567890ABCDEFULL;
        simtlm::read_word<uint64_t>(m_test.initiator_socket,
                                    NUM_SCRATCH * SCRATCH_STRIDE, past_end);
        uint64_t wrapped = 0x1234567890ABCDEFULL;
        simtlm::read_word<uint64_t>(m_test.initiator_socket,
                                    (1ULL << 32) + scratch_offset(0), wrapped);
        check(past_end == 0 && wrapped == 0,
              "FUNC-SCRATCHWARM-004c: reads past the bank return zero, never aliased register data");
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
        test_explicit_byte_enables();
        test_debug_and_dmi();
        test_reset_clears_state();
        test_malformed_payloads();
        // Last: it deliberately leaves the bank cleared and toggles reset a lot.
        test_reset_sensitivity();

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
