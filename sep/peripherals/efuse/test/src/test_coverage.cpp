#include "testbench.h"
#include "efuse_basetest.h"

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
    const uint32_t loaded = m_dut->lc_state.get_param_value();

    m_test->register_write_32(efuse_basetest::LC_STATE_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, val);
    if (val == (loaded | 0x1u))
        report_test_pass("LC_STATE: runtime WOSET ORs with fuse-loaded value");
    else
        report_test_fail("LC_STATE WOSET", "expected 0x" + std::to_string(loaded | 0x1u) +
            " got 0x" + std::to_string(val));

    if (m_dut->get_lc_state() == val)
        report_test_pass("LC_STATE: get_lc_state() matches register");
    else
        report_test_fail("LC_STATE get_lc_state()", "accessor mismatch");
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
    report_test_start("Test 15: EFUSE_WRITE_CTRL — write_go pulse");

    uint32_t val = 0;

    // Pulse bit 17 (write_go) with bit 24 (write_busy) set
    m_test->register_write_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, (1u << 17) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_WRITE_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25);  // write_done=1, write_go cleared, busy cleared
    if (val == expected)
        report_test_pass("EFUSE_WRITE_CTRL: write_go auto-completes (done=1, busy=0)");
    else
        report_test_fail("EFUSE_WRITE_CTRL write_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));
}

void testbench::test_efuse_read_ctrl_go()
{
    report_test_start("Test 16: EFUSE_READ_CTRL — read_go pulse");

    uint32_t val = 0;

    // Pulse bit 16 (read_go) with bit 24 (read_busy) set
    m_test->register_write_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, (1u << 16) | (1u << 24));
    wait(1, SC_NS);
    m_test->register_read_32(efuse_basetest::EFUSE_READ_CTRL_OFFSET, val);

    const uint32_t expected = (1u << 25);  // read_done=1, read_go cleared, busy cleared
    if (val == expected)
        report_test_pass("EFUSE_READ_CTRL: read_go auto-completes (done=1, busy=0)");
    else
        report_test_fail("EFUSE_READ_CTRL read_go", "expected 0x" + std::to_string(expected) +
            " got 0x" + std::to_string(val));
}

void testbench::test_otp_accessors()
{
    report_test_start("Test 17: OTP accessors — get_lc_state / get_chiplet_uid");

    const uint32_t lc = m_dut->get_lc_state();
    uint32_t reg = 0;
    m_test->register_read_32(efuse_basetest::LC_STATE_OFFSET, reg);
    if (lc == reg)
        report_test_pass("get_lc_state() matches LC_STATE register");
    else
        report_test_fail("get_lc_state()", "mismatch with register read");

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
}
