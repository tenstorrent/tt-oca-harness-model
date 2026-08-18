#include "testbench.h"
#include "efuse_basetest.h"
#include <fstream>

// ============================================================================
// Coverage tests — exercise WOSET handlers and interface-ctrl pulse paths
// in src/efuse.cpp that are not hit by Tests 1–8.
// ============================================================================

void testbench::test_woset_locks_hi()
{
    report_test_start("Test 9: WOSET — LOCKS_HI");

    uint32_t val = 0;

    m_test->register_write_32(efuse_basetest::LOCKS_HI_OFFSET, 0x00000005);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::LOCKS_HI_OFFSET, val);
    if (val == 0x00000005)
        report_test_pass("LOCKS_HI: first write sets bits 0x5");
    else
        report_test_fail("LOCKS_HI first write", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::LOCKS_HI_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::LOCKS_HI_OFFSET, val);
    if (val == 0x00000005)
        report_test_pass("LOCKS_HI: bit 0 stays set after write-0 attempt (WOSET)");
    else
        report_test_fail("LOCKS_HI WOSET", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::LOCKS_HI_OFFSET, 0x000000F0);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::LOCKS_HI_OFFSET, val);
    if (val == 0x000000F5)
        report_test_pass("LOCKS_HI: additional bits ORed correctly");
    else
        report_test_fail("LOCKS_HI OR accumulation", "expected 0xF5 got 0x" + std::to_string(val));
}

void testbench::test_woset_lc_state()
{
    report_test_start("Test 10: WOSET — LC_STATE");

    uint32_t val = 0;
    // The register holds the differential code, so the fuse-loaded value is the
    // encoding of the raw parameter, not the parameter itself.
    const uint32_t loaded = efuse_model::lc_state_encode(m_dut->lc_state.get_param_value());

    m_test->register_write_32(efuse_basetest::LC_STATE_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, val);
    if (val == (loaded | 0x1u))
        report_test_pass("LC_STATE: runtime WOSET ORs with fuse-loaded value");
    else
        report_test_fail("LC_STATE WOSET", "expected 0x" + std::to_string(loaded | 0x1u) +
            " got 0x" + std::to_string(val));

    // WOSET has just ORed a bit into a legal code, which makes it illegal: 0xE1 | 0x1
    // is 0xE1 for raw 1, but for other states the extra bit breaks the {~raw, raw}
    // relationship. The accessor must report INVALID rather than the low nibble, which
    // is the whole point of the differential encoding.
    const uint32_t expect_raw = efuse_model::lc_state_code_valid(val)
                                  ? (val & 0xFu) : efuse_model::LC_STATE_RAW_INVALID;
    if (m_dut->get_lc_state() == expect_raw)
        report_test_pass("LC_STATE: get_lc_state() decodes the differential code");
    else
        report_test_fail("LC_STATE get_lc_state()", "expected raw 0x" +
            std::to_string(expect_raw) + " got 0x" + std::to_string(m_dut->get_lc_state()));
}

void testbench::test_woset_sip_dis_hi()
{
    report_test_start("Test 11: WOSET — SIP_DIS_HI");

    uint32_t val = 0;

    m_test->register_write_32(efuse_basetest::SIP_DIS_HI_OFFSET, 0x3);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SIP_DIS_HI_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SIP_DIS_HI: bits 0,1 set");
    else
        report_test_fail("SIP_DIS_HI set", "expected 0x3 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::SIP_DIS_HI_OFFSET, 0x0);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SIP_DIS_HI_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SIP_DIS_HI: WOSET holds after write-0");
    else
        report_test_fail("SIP_DIS_HI WOSET", "bits cleared, expected 0x3 got 0x" + std::to_string(val));
}

void testbench::test_woset_sys_dis_hi()
{
    report_test_start("Test 12: WOSET — SYS_DIS_HI");

    uint32_t val = 0;

    m_test->register_write_32(efuse_basetest::SYS_DIS_HI_OFFSET, 0x6);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SYS_DIS_HI_OFFSET, val);
    if (val == 0x6)
        report_test_pass("SYS_DIS_HI: bits 1,2 set");
    else
        report_test_fail("SYS_DIS_HI set", "expected 0x6 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::SYS_DIS_HI_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SYS_DIS_HI_OFFSET, val);
    if (val == 0x7)
        report_test_pass("SYS_DIS_HI: additional bit ORed");
    else
        report_test_fail("SYS_DIS_HI OR", "expected 0x7 got 0x" + std::to_string(val));
}

void testbench::test_woset_chiplet_pubk_revoke()
{
    report_test_start("Test 13: WOSET — CHIPLET_PUBK_REVOKE");

    uint32_t val = 0;
    const uint32_t loaded = m_dut->chiplet_pubk_revoke.get_param_value();

    m_test->register_write_32(efuse_basetest::CHIPLET_PUBK_REVOKE_OFFSET, 0x5);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::CHIPLET_PUBK_REVOKE_OFFSET, val);
    if (val == (loaded | 0x5u))
        report_test_pass("CHIPLET_PUBK_REVOKE: runtime WOSET ORs with fuse value");
    else
        report_test_fail("CHIPLET_PUBK_REVOKE WOSET", "expected 0x" + std::to_string(loaded | 0x5u) +
            " got 0x" + std::to_string(val));
}

void testbench::test_woset_bl_version()
{
    report_test_start("Test 14: WOSET — BL1/BL2_VERSION arrays");

    uint32_t val = 0;

    m_test->register_write_32(efuse_basetest::BL1_VERSION_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET, val);
    if (val == 0x1)
        report_test_pass("BL1_VERSION[0]: WOSET sets bit 0");
    else
        report_test_fail("BL1_VERSION[0]", "expected 0x1 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::BL1_VERSION_OFFSET + 4, 0x00000002);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET + 4, val);
    if (val == 0x2)
        report_test_pass("BL1_VERSION[1]: per-word WOSET");
    else
        report_test_fail("BL1_VERSION[1]", "expected 0x2 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::BL2_VERSION_OFFSET, 0x00000004);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL2_VERSION_OFFSET, val);
    if (val == 0x4)
        report_test_pass("BL2_VERSION[0]: WOSET sets bit 2");
    else
        report_test_fail("BL2_VERSION[0]", "expected 0x4 got 0x" + std::to_string(val));
}

void testbench::test_efuse_write_ctrl_go()
{
    report_test_start("Test 15: EFUSE_WRITE_CTRL — program_go without program_enable");

    uint32_t val = 0;

    // Pulse bit 17 (program_go) with bit 24 (program_busy) set, but leave
    // program_enable (bit 27) clear. efuse_program_interface.sv answers
    // done-with-error rather than stalling or programming anything.
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, (1u << 17) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25) | (1u << 26);  // done=1, status=error, go/busy cleared
    if (val == expected)
        report_test_pass("EFUSE_WRITE_CTRL: gated program reports done + error");
    else
        report_test_fail("EFUSE_WRITE_CTRL write_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));

    // The refusal is sticky in STATUS.efuse_req_error until explicitly cleared.
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val & (1u << 4))
        report_test_pass("STATUS: efuse_req_error latched by the refused program");
    else
        report_test_fail("STATUS efuse_req_error", "expected bit 4 set, got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("STATUS: efuse_req_err_clear clears it, sense_done still set");
    else
        report_test_fail("STATUS req_err_clear", "expected 0x1 got 0x" + std::to_string(val));
}

void testbench::test_efuse_read_ctrl_go()
{
    report_test_start("Test 16: EFUSE_READ_CTRL — read_go without read_enable");

    uint32_t val = 0;

    // Same shape as Test 15, on the read side: read_enable is bit 28.
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, (1u << 16) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25) | (1u << 26);
    if (val == expected)
        report_test_pass("EFUSE_READ_CTRL: gated read reports done + error");
    else
        report_test_fail("EFUSE_READ_CTRL read_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));

    // A refused read must not leave data behind for a caller that skips the status.
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val == 0u)
        report_test_pass("EFUSE_READ_INTERFACE_READ_DATA: zero after a refused read");
    else
        report_test_fail("read data after refusal", "expected 0x0 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
}

void testbench::test_fuse_array_program_read()
{
    report_test_start("Test 19: Fuse array — program, read back, and program-once");

    uint32_t val = 0;

    const uint32_t enable_read = (1u << 28);

    // The array and the shadow map are the same 8192 bits, so a fuse-backed field
    // must read identically through both paths. CHIPLET_UID is read-only to
    // software, so no earlier test has disturbed it: byte offset 0xC8 is array word
    // 50, hence bit 1600.
    uint32_t shadow_uid = 0;
    m_test->register_read_32(efuse_basetest::CHIPLET_UID_OFFSET, shadow_uid);

    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | 1600u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("enabled read completes without error");
    else
        report_test_fail("enabled read", "expected done + no error, got 0x" + std::to_string(val));

    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val == shadow_uid)
        report_test_pass("OTP read agrees with the shadow register over the same bits");
    else
        report_test_fail("array/shadow agreement", "shadow 0x" + std::to_string(shadow_uid) +
            " vs OTP 0x" + std::to_string(val));

    // A shadow write must not reach the array. Silicon latches shadow writes in
    // flops and only EFUSE_PROGRAM_CTRL burns fuses, so LOCKS accumulating WOSET
    // bits at the register interface leaves the corresponding fuses erased -- the
    // two deliberately diverge after any shadow write.
    uint32_t shadow_locks = 0;
    m_test->register_read_32(efuse_basetest::LOCKS_LO_OFFSET, shadow_locks);
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | 0u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (shadow_locks != 0u && val != shadow_locks)
        report_test_pass("shadow writes do not burn fuses (LOCKS diverges from the array)");
    else
        report_test_fail("shadow write isolation", "shadow 0x" + std::to_string(shadow_locks) +
            " unexpectedly equals OTP 0x" + std::to_string(val));

    // Program a bit in a word nothing else uses, then read it back. RESERVED_2
    // starts at byte 0x254, so word 0x254/4 = 149, bit 149*32 = 4768.
    const uint32_t test_bit = 4768u + 3u;
    const uint32_t enable_prog = (1u << 27) | (1u << 16);  // program_enable, data=1
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (1u << 18) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("enabled program completes without error");
    else
        report_test_fail("enabled program", "expected done + no error, got 0x" + std::to_string(val));

    m_test->register_read_32(efuse_basetest::EFUSE_PROG_INTF_RD_DATA_OFFSET, val);
    if (val == (1u << 3))
        report_test_pass("program read-back returns the word with the new bit set");
    else
        report_test_fail("program read-back", "expected 0x8 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val & (1u << 3))
        report_test_pass("programmed bit reads back as 1");
    else
        report_test_fail("programmed bit read-back", "expected bit 3 set, got 0x" + std::to_string(val));

    // Programming data=0 is rejected outright rather than clearing the bit: the
    // array is one-time-programmable, so nothing can unburn bit 3.
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              (1u << 27) | (1u << 17) | test_bit);   // data=0
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("program with data=0 is refused");
    else
        report_test_fail("program data=0", "expected error status, got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | test_bit);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val & (1u << 3))
        report_test_pass("burned bit survives the attempt to clear it");
    else
        report_test_fail("program-once", "bit 3 was cleared, got 0x" + std::to_string(val));

    // Out-of-range addresses latch their own sticky error, distinct from req_error.
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | 8192u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val & (1u << 6))
        report_test_pass("STATUS: read_addr_error latched by an out-of-range read");
    else
        report_test_fail("read_addr_error", "expected bit 6 set, got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, (1u << 8) | (1u << 10));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("STATUS: addr-error clear returns the register to sense_done only");
    else
        report_test_fail("addr-error clear", "expected 0x1 got 0x" + std::to_string(val));
}

void testbench::test_otp_accessors()
{
    report_test_start("Test 17: OTP accessors — get_lc_state / get_chiplet_uid");

    // get_lc_state() returns the raw state, get_lc_state_code() the fuse contents;
    // the latter is what software reads from the register.
    const uint32_t lc = m_dut->get_lc_state();
    uint32_t reg = 0;
    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, reg);
    if (m_dut->get_lc_state_code() == reg)
        report_test_pass("get_lc_state_code() matches LC_STATE register");
    else
        report_test_fail("get_lc_state_code()", "mismatch with register read");

    const uint32_t expect_raw = efuse_model::lc_state_code_valid(reg)
                                  ? (reg & 0xFu) : efuse_model::LC_STATE_RAW_INVALID;
    if (lc == expect_raw)
        report_test_pass("get_lc_state() returns the decoded raw state");
    else
        report_test_fail("get_lc_state()", "expected raw 0x" + std::to_string(expect_raw) +
            " got 0x" + std::to_string(lc));

    const uint32_t *uid = m_dut->get_chiplet_uid();
    bool match = true;
    for (int i = 0; i < 8; ++i) {
        uint32_t word = 0;
        m_test->register_read_32(efuse_basetest::CHIPLET_UID_OFFSET + static_cast<unsigned>(i * 4), word);
        if (word != uid[i]) {
            match = false;
            break;
        }
    }
    if (match)
        report_test_pass("get_chiplet_uid() matches CHIPLET_UID array");
    else
        report_test_fail("get_chiplet_uid()", "mismatch with register reads");
}

void testbench::test_shim_ctrl_window()
{
    report_test_start("Test 18: EFUSE_SHIM_CTRL — separate window R/W");

    // The shim answers on its own socket, so an offset here must not reach the
    // register the same offset names in the sep_efuse window. LOCKS_LO sits at
    // 0x000 there and is WOSET, which would make a clean read-back impossible if
    // the two windows were ever collapsed back into one.
    uint32_t val = 0;

    m_test->shim_write_32(efuse_basetest::SHIM_STATUS_OFFSET, 0x0000ABCD);
    wait(1, SC_NS);
    m_test->shim_read_32(efuse_basetest::SHIM_STATUS_OFFSET, val);
    if (val == 0x0000ABCD)
        report_test_pass("SHIM_EFUSE_CTRL_STATUS: read-back at shim offset 0x000");
    else
        report_test_fail("SHIM_EFUSE_CTRL_STATUS", "expected 0xABCD got 0x" + std::to_string(val));

    // TIMING_CTRL_7 is the register efuse_sanity_csr_test touches.
    const unsigned int timing_7 = efuse_basetest::SHIM_TIMING_CTRL_OFFSET + (7 * 4);
    m_test->shim_write_32(timing_7, 0x00001234);
    wait(1, SC_NS);
    m_test->shim_read_32(timing_7, val);
    if (val == 0x00001234)
        report_test_pass("SHIM_EFUSE_TIMING_CTRL_7: read-back");
    else
        report_test_fail("SHIM_EFUSE_TIMING_CTRL_7", "expected 0x1234 got 0x" + std::to_string(val));

    // The windows are disjoint: writing the shim must leave LOCKS_LO alone.
    m_test->register_read_32(efuse_basetest::LOCKS_LO_OFFSET, val);
    if (val != 0x0000ABCD)
        report_test_pass("shim writes do not alias into the sep_efuse window");
    else
        report_test_fail("window isolation", "LOCKS_LO took the shim's value");
}

void testbench::test_fuse_preload_file()
{
    report_test_start("Test 20: Fuse array — .preload image backdoor");

    uint32_t val = 0;

    // Written here rather than shipped as a fixture so the test is independent of
    // where it runs from, and so the malformed cases below can be constructed too.
    // Format is the RTL's: one ASCII bit per line, LSB first.
    const std::string good = "/tmp/efuse_test_good.preload";
    {
        std::ofstream out(good);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            // LC_STATE occupies bits 64..95; write 0xF0 into its low byte, the
            // differential encoding of TEST_DEV that the RTL default image carries.
            const bool set = (bit >= 68 && bit <= 71);
            out << (set ? '1' : '0') << "\n";
        }
    }

    if (m_dut->preload_fuses_from_file(good))
        report_test_pass("preload image accepted");
    else
        report_test_fail("preload accept", "a well-formed image was rejected");

    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("LC_STATE sensed from the image as 0xF0 (TEST_DEV)");
    else
        report_test_fail("preload sense", "expected 0xF0 got 0x" + std::to_string(val));

    // The image replaces the array wholesale, so previously programmed bits are gone.
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | 64u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("OTP read of the LC_STATE word matches the image");
    else
        report_test_fail("preload array", "expected 0xF0 got 0x" + std::to_string(val));

    // A malformed image must be refused outright and leave the array as it was,
    // rather than committing a half-parsed load.
    const std::string bad = "/tmp/efuse_test_bad.preload";
    {
        std::ofstream out(bad);
        out << "0\n1\n0\nX\n0\n";
    }
    if (!m_dut->preload_fuses_from_file(bad))
        report_test_pass("malformed image refused");
    else
        report_test_fail("preload reject", "a bad character was accepted");

    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, val);
    if (val == 0x000000F0u)
        report_test_pass("refused image left the array untouched");
    else
        report_test_fail("preload atomicity", "array changed, LC_STATE now 0x" + std::to_string(val));

    if (!m_dut->preload_fuses_from_file("/tmp/efuse_no_such_file.preload"))
        report_test_pass("missing image refused");
    else
        report_test_fail("preload missing", "a nonexistent path was accepted");

    // Blank and whitespace-only lines are skipped rather than counted as bits.
    const std::string spaced = "/tmp/efuse_test_spaced.preload";
    {
        std::ofstream out(spaced);
        out << "1\n\n  \n1\n";   // two bits, at positions 0 and 1
    }
    if (m_dut->preload_fuses_from_file(spaced))
        report_test_pass("whitespace-tolerant image accepted");
    else
        report_test_fail("preload whitespace", "blank lines were treated as an error");

    m_test->register_read_32(efuse_basetest::LOCKS_LO_OFFSET, val);
    if (val == 0x3u)
        report_test_pass("blank lines skipped: bits landed at 0 and 1");
    else
        report_test_fail("preload whitespace bits", "expected 0x3 got 0x" + std::to_string(val));
}

void testbench::test_fuse_program_out_of_range()
{
    report_test_start("Test 21: Fuse array — out-of-range program");

    uint32_t val = 0;

    // 8192 is one past the last bit. The command completes with an error and
    // latches its own sticky flag, distinct from the read side's.
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              (1u << 27) | (1u << 16) | (1u << 17) | 8192u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("out-of-range program reports done + error");
    else
        report_test_fail("oob program", "expected done + error, got 0x" + std::to_string(val));

    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val & (1u << 5))
        report_test_pass("STATUS: program_addr_error latched");
    else
        report_test_fail("program_addr_error", "expected bit 5 set, got 0x" + std::to_string(val));

    // Clearing only the program flag must leave the others alone.
    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, (1u << 9));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (!(val & (1u << 5)) && (val & (1u << 4)))
        report_test_pass("program_addr_error_clear is selective (req_error survives)");
    else
        report_test_fail("selective clear", "expected bit 5 clear and bit 4 set, got 0x" +
            std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val == 0x1u)
        report_test_pass("all clears together return STATUS to sense_done only");
    else
        report_test_fail("clear all", "expected 0x1 got 0x" + std::to_string(val));

    // The refused program must not have burned anything at the wrapped address.
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | 0u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if ((val & 0x1u) == 0x1u)
        report_test_pass("word 0 still holds only what the image put there");
    else
        report_test_fail("oob program side effect", "word 0 is 0x" + std::to_string(val));
}

void testbench::test_lock_enforcement()
{
    report_test_start("Test 22: LOCKS — write and read enforcement");

    uint32_t val = 0;

    // Start from an erased image so the lock state is known: LOCKS is sticky with no
    // software clear, and earlier tests have set bits in it. Sensing an all-zero array
    // reloads the lock flops along with everything else, which is the only way back to
    // an unlocked model short of a reset.
    const std::string erased = "/tmp/efuse_test_erased.preload";
    {
        std::ofstream out(erased);
        for (unsigned int bit = 0; bit < 8192; bit++)
            out << "0\n";
    }
    if (m_dut->preload_fuses_from_file(erased))
        report_test_pass("erased image sensed: locks back to zero");
    else
        report_test_fail("lock test setup", "erased image was rejected");

    // BL1_VERSION is the field under test: eight words at 0x088 (array word 34, bit
    // 1088), WOSET from software, and governed by LOCKS_LO bits 18 (write) and 19
    // (read). BL2_VERSION next door has its own pair and stays unlocked throughout,
    // so it witnesses that the policy is per field rather than global.
    const uint32_t bl1_bit  = 34u * 32u;
    const uint32_t enable_read = (1u << 28);
    const uint32_t enable_prog = (1u << 27) | (1u << 16);   // program_enable, data=1

    m_test->register_write_32(efuse_basetest::BL1_VERSION_OFFSET, 0x5);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("unlocked: shadow write accepted");
    else
        report_test_fail("unlocked shadow write", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (bl1_bit + 2u));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("unlocked: OTP program accepted");
    else
        report_test_fail("unlocked program", "expected done + no error, got 0x" + std::to_string(val));

    // Write lock only. Reads must keep working, which is what makes the two bits
    // independent rather than one coarse "locked" state.
    m_test->register_write_32(efuse_basetest::LOCKS_LO_OFFSET, (1u << 18));
    wait(1, SC_NS);

    m_test->register_write_32(efuse_basetest::BL1_VERSION_OFFSET, 0x8);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("write-locked: shadow write dropped, old value intact");
    else
        report_test_fail("write lock", "expected 0x5 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (bl1_bit + 3u));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("write-locked: OTP program refused with done + error");
    else
        report_test_fail("write lock OTP", "expected done + error, got 0x" + std::to_string(val));

    m_test->register_read_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, val);
    if (val & (1u << 4))
        report_test_pass("write-locked: req_error latched");
    else
        report_test_fail("write lock req_error", "expected bit 4 set, got 0x" + std::to_string(val));

    // Nothing was burned: bit 2 from the unlocked program is there, bit 3 is not.
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | bl1_bit);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val == 0x4)
        report_test_pass("write-locked: refused program burned nothing");
    else
        report_test_fail("write lock no-burn", "expected 0x4 got 0x" + std::to_string(val));

    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET, val);
    if (val == 0x5)
        report_test_pass("write-locked: shadow still readable");
    else
        report_test_fail("write lock read path", "expected 0x5 got 0x" + std::to_string(val));

    // Read lock as well.
    m_test->register_write_32(efuse_basetest::LOCKS_LO_OFFSET, (1u << 19));
    wait(1, SC_NS);

    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET, val);
    if (val == 0xBADCAB1Eu)
        report_test_pass("read-locked: shadow read returns 0xBADCAB1E");
    else
        report_test_fail("read lock shadow", "expected 0xBADCAB1E got 0x" + std::to_string(val));

    // One lock pair covers all eight words of the field, not just the first.
    m_test->register_read_32(efuse_basetest::BL1_VERSION_OFFSET + 7 * 4, val);
    if (val == 0xBADCAB1Eu)
        report_test_pass("read-locked: last word of the field denied too");
    else
        report_test_fail("read lock span", "expected 0xBADCAB1E got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET, (1u << 8));
    wait(1, SC_NS);
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET,
                              enable_read | (1u << 16) | bl1_bit);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && (val & (1u << 26)))
        report_test_pass("read-locked: OTP read refused with done + error");
    else
        report_test_fail("read lock OTP", "expected done + error, got 0x" + std::to_string(val));

    // The OTP path reports the refusal in status and returns zero, rather than
    // returning the sentinel the shadow path uses.
    m_test->register_read_32(efuse_basetest::EFUSE_READ_INTF_RD_DATA_OFFSET, val);
    if (val == 0u)
        report_test_pass("read-locked: OTP read data is zero, not the sentinel");
    else
        report_test_fail("read lock OTP data", "expected 0x0 got 0x" + std::to_string(val));

    // BL2_VERSION shares no lock bits with BL1_VERSION and is still fully accessible.
    m_test->register_write_32(efuse_basetest::BL2_VERSION_OFFSET, 0x9);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::BL2_VERSION_OFFSET, val);
    if (val == 0x9)
        report_test_pass("neighbouring field unaffected by BL1_VERSION's locks");
    else
        report_test_fail("lock isolation", "expected 0x9 got 0x" + std::to_string(val));

    // LOCKS_LO governs other fields and is never itself lockable, so it has to stay
    // readable -- otherwise software could not tell what it had locked.
    m_test->register_read_32(efuse_basetest::LOCKS_LO_OFFSET, val);
    if (val == ((1u << 18) | (1u << 19)))
        report_test_pass("LOCKS_LO itself remains readable");
    else
        report_test_fail("LOCKS_LO readable", "expected 0xC0000 got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
}

void testbench::test_token_matching()
{
    report_test_start("Test 23: Token matching — SHA-256 compare against the digest fuse");

    uint32_t val = 0;

    // SHA-256 of a 32-byte all-zero token. This is the constant the standalone SEP
    // testbench uses (top_define.sv SEP_SEC_DIS_TEST_TOKEN_HASH), which makes it a
    // useful independent check: if the model's message byte order or digest word order
    // were wrong, the digest of the all-zero token would not be this value.
    //   66687aad f862bd77 6c8fc18b 8e9f8e20 08971485 6ee233b3 902a591d 0d5f2925
    // Word 0 is digest bits [31:0], so the words run opposite to that byte order.
    const uint32_t zero_token_digest[8] = {
        0x0d5f2925u, 0x902a591du, 0x6ee233b3u, 0x08971485u,
        0x8e9f8e20u, 0x6c8fc18bu, 0xf862bd77u, 0x66687aadu,
    };

    // The RMA SIP reference is a fuse field, so it is set the way hardware sets it:
    // through the array. RMA_SIP_TOKEN_DIGEST sits at shadow byte 0x24, i.e. array
    // word 9, i.e. bit 288.
    const std::string image = "/tmp/efuse_test_token.preload";
    {
        std::ofstream out(image);
        for (unsigned int bit = 0; bit < 8192; bit++) {
            const unsigned int word = bit / 32;
            bool set = false;
            if (word >= 9 && word <= 16)
                set = (zero_token_digest[word - 9] >> (bit % 32)) & 1u;
            out << (set ? '1' : '0') << "\n";
        }
    }
    if (m_dut->preload_fuses_from_file(image))
        report_test_pass("digest image sensed into RMA_SIP_TOKEN_DIGEST");
    else
        report_test_fail("token test setup", "digest image was rejected");

    // Token inputs reset to zero, so the staged token is already the all-zero one.
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x15u)
        report_test_pass("zero token matches the digest fuse (status 0x15)");
    else
        report_test_fail("token match", "expected 0x15 got 0x" + std::to_string(val));

    // Flip one bit of the token: the digest changes completely, so this is also a
    // check that the comparison looks at the whole 256 bits.
    m_test->register_write_32(efuse_basetest::RMA_SIP_TOKEN_I_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("altered token mismatches (status 0x2A)");
    else
        report_test_fail("token mismatch", "expected 0x2A got 0x" + std::to_string(val));

    // The word order matters: the same eight words in the opposite order hash to a
    // different digest, so a model that streamed them the other way would fail here
    // while still passing the all-zero case (which is order-independent).
    const uint32_t ordered_token[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    for (int i = 0; i < 8; i++)
        m_test->register_write_32(efuse_basetest::RMA_SIP_TOKEN_I_OFFSET + i * 4, ordered_token[i]);
    wait(1, SC_NS);
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::RMA_SIP_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("ordered token mismatches a digest it was not built from");
    else
        report_test_fail("token order", "expected 0x2A got 0x" + std::to_string(val));

    // Each token type has its own go bit, reference and result. Triggering SIP must
    // leave the other two results alone.
    m_test->register_read_32(efuse_basetest::RMA_CHIPLET_TOKEN_MATCH_OFFSET, val);
    if (val == 0x0u)
        report_test_pass("untriggered chiplet result still reads 0x00");
    else
        report_test_fail("token independence", "expected 0x0 got 0x" + std::to_string(val));

    // The chiplet reference is all zeros in this image, so no token can match it --
    // exactly the situation an unprogrammed digest fuse leaves in silicon.
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x100);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::RMA_CHIPLET_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("zero reference cannot be matched (chiplet mismatch)");
    else
        report_test_fail("zero reference", "expected 0x2A got 0x" + std::to_string(val));

    // Same for secure disable, whose reference is the parameter rather than a fuse and
    // defaults to zero as the RTL parameter does.
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x10000);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SEC_DISABLE_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("sec-disable mismatches with the default zero reference");
    else
        report_test_fail("sec disable default", "expected 0x2A got 0x" + std::to_string(val));

    // Results are hardware-written; software cannot forge a match.
    m_test->register_write_32(efuse_basetest::SEC_DISABLE_TOKEN_MATCH_OFFSET, 0x15);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::SEC_DISABLE_TOKEN_MATCH_OFFSET, val);
    if (val == 0x2Au)
        report_test_pass("match result is read-only to software");
    else
        report_test_fail("match RO", "software wrote the result, now 0x" + std::to_string(val));

    // TOKEN_EOP go bits are singlepulse and the register is write-only.
    m_test->register_read_32(efuse_basetest::TOKEN_EOP_OFFSET, val);
    if (val == 0x0u)
        report_test_pass("TOKEN_EOP reads back zero (write-only singlepulse)");
    else
        report_test_fail("TOKEN_EOP readback", "expected 0x0 got 0x" + std::to_string(val));

    // What the match authorises: LC_STATE bit 1 (array bit 65) can only be burned
    // while the SIP token matches. Restore the matching token first.
    const uint32_t enable_prog = (1u << 27) | (1u << 16);
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | 65u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("LC_STATE RMA bit refused while the token mismatches");
    else
        report_test_fail("LC gating", "expected error status, got 0x" + std::to_string(val));

    for (int i = 0; i < 8; i++)
        m_test->register_write_32(efuse_basetest::RMA_SIP_TOKEN_I_OFFSET + i * 4, 0);
    wait(1, SC_NS);
    m_test->register_write_32(efuse_basetest::TOKEN_EOP_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | 65u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("LC_STATE RMA bit accepted once the token matches");
    else
        report_test_fail("LC gating match", "expected done + no error, got 0x" + std::to_string(val));

    // The chiplet bit has its own gate, and its token has not matched.
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET,
                              enable_prog | (1u << 17) | 66u);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);
    if (val & (1u << 26))
        report_test_pass("LC_STATE chiplet bit still gated by its own token");
    else
        report_test_fail("LC gating chiplet", "expected error status, got 0x" + std::to_string(val));

    m_test->register_write_32(efuse_basetest::EFUSE_INTF_STATUS_OFFSET,
                              (1u << 8) | (1u << 9) | (1u << 10));
    wait(1, SC_NS);
}

void testbench::run_coverage_tests()
{
    test_woset_locks_hi();
    test_woset_lc_state();
    test_woset_sip_dis_hi();
    test_woset_sys_dis_hi();
    test_woset_chiplet_pubk_revoke();
    test_woset_bl_version();
    test_efuse_write_ctrl_go();
    test_efuse_read_ctrl_go();
    test_otp_accessors();
    test_shim_ctrl_window();
    test_fuse_array_program_read();
    test_fuse_preload_file();
    test_fuse_program_out_of_range();
    // Last: LOCKS is sticky, so anything this sets stays locked for whatever follows.
    test_lock_enforcement();
    test_token_matching();
}
