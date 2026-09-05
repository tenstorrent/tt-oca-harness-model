// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "efuse_basetest.h"
#include "reg_param.h"

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// =============================================================================
// Constructor
// =============================================================================

testbench::testbench(sc_module_name name)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    m_dut  = std::make_unique<efuse_model>("efuse_dut");
    m_test = std::make_unique<efuse_test>("efuse_test");

    logger.setMaxVerbosity(m_dut->verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    m_test->initiator_socket.bind(m_dut->target_socket);
    m_test->shim_initiator_socket.bind(m_dut->shim_target_socket);
    m_dut->locked_field_access_irq_o(locked_field_irq);

    SC_METHOD(count_locked_field_pulse);
    sensitive << locked_field_irq.posedge_event();
    dont_initialize();

    SC_THREAD(run_tests);
}

// =============================================================================
// Test Reporting
// =============================================================================

void testbench::report_test_start(const std::string &name)
{
    REG_INFO(1, logger) << "\n========================================"
                         << "\n" << name
                         << "\n========================================" << std::endl;
}

void testbench::report_test_pass(const std::string &name)
{
    m_tests_run++;
    m_tests_passed++;
    REG_INFO(1, logger) << "[PASS] " << name << std::endl;
}

void testbench::report_test_fail(const std::string &name, const std::string &reason)
{
    m_tests_run++;
    m_tests_failed++;
    m_failed_tests.push_back(name);
    REG_ERROR(0, logger) << "[FAIL] " << name << " — " << reason << std::endl;
}

void testbench::report_test_summary()
{
    REG_INFO(1, logger) << "\n========================================"
                         << "\nsep_efuse Test Summary"
                         << "\n========================================"
                         << "\nTotal : " << m_tests_run
                         << "\nPassed: " << m_tests_passed
                         << "\nFailed: " << m_tests_failed << std::endl;

    for (const auto &t : m_failed_tests)
        REG_ERROR(0, logger) << "  FAILED: " << t << std::endl;
}

// =============================================================================
// Test 1: RO registers loaded correctly from config
// =============================================================================

void testbench::test_fuse_load_ro_registers()
{
    report_test_start("Test 1: Fuse Load — RO registers");

    uint32_t val = 0;

    // The parameter is the raw state; the register holds its differential encoding.
    m_test->register_read_32(sep_efuse::LC_STATE_OFFSET, val);
    if (val == efuse_model::lc_state_encode(m_dut->lc_state.get_param_value()))
        report_test_pass("LC_STATE fuse load");
    else
        report_test_fail("LC_STATE fuse load",
            "expected 0x" + std::to_string(m_dut->lc_state.get_param_value()) +
            " got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::SBOOT_DIS_OFFSET, val);
    if (val == m_dut->sboot_dis.get_param_value())
        report_test_pass("SBOOT_DIS fuse load");
    else
        report_test_fail("SBOOT_DIS fuse load",
            "expected 0x" + std::to_string(m_dut->sboot_dis.get_param_value()) +
            " got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::CHIPLET_PUBK_REVOKE_OFFSET, val);
    if (val == m_dut->chiplet_pubk_revoke.get_param_value())
        report_test_pass("CHIPLET_PUBK_REVOKE fuse load");
    else
        report_test_fail("CHIPLET_PUBK_REVOKE fuse load",
            "expected 0x" + std::to_string(m_dut->chiplet_pubk_revoke.get_param_value()) +
            " got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::STATUS_RPT_OFFSET, val);
    if (val == m_dut->status_rpt.get_param_value())
        report_test_pass("STATUS_RPT fuse load");
    else
        report_test_fail("STATUS_RPT fuse load",
            "expected 0x" + std::to_string(m_dut->status_rpt.get_param_value()) +
            " got 0x" + std::to_string(val));
}

// =============================================================================
// Test 2: Array registers loaded correctly
// =============================================================================

void testbench::test_fuse_load_array_registers()
{
    report_test_start("Test 2: Fuse Load — Array registers (SIP_PUBK_HASH0, CHIPLET_UID)");

    uint32_t val = 0;
    bool passed = true;

    auto sip_pubk_v  = m_dut->sip_pubk_hash0.get_param_value();
    auto uid_v       = m_dut->chiplet_uid.get_param_value();
    auto vi = [](const std::vector<uint32_t>& v, int i) -> uint32_t {
        return (i < (int)v.size()) ? v[i] : 0u;
    };

    for (int i = 0; i < 8; i++) {
        unsigned int offset = sep_efuse::SIP_PUBK_HASH0_OFFSET + (i * 4);
        m_test->register_read_32(offset, val);
        if (val != vi(sip_pubk_v, i)) {
            report_test_fail("SIP_PUBK_HASH0[" + std::to_string(i) + "] fuse load",
                "expected 0x" + std::to_string(vi(sip_pubk_v, i)) +
                " got 0x" + std::to_string(val));
            passed = false;
        }
    }
    if (passed) report_test_pass("SIP_PUBK_HASH0[0..7] fuse load");

    passed = true;
    for (int i = 0; i < 8; i++) {
        unsigned int offset = sep_efuse::CHIPLET_UID_OFFSET + (i * 4);
        m_test->register_read_32(offset, val);
        if (val != vi(uid_v, i)) {
            report_test_fail("CHIPLET_UID[" + std::to_string(i) + "] fuse load",
                "expected 0x" + std::to_string(vi(uid_v, i)) +
                " got 0x" + std::to_string(val));
            passed = false;
        }
    }
    if (passed) report_test_pass("CHIPLET_UID[0..7] fuse load");
}

// =============================================================================
// Test 3: RO write protection — write attempt must be silently ignored
// =============================================================================

void testbench::test_ro_write_protection()
{
    report_test_start("Test 3: RO Write Protection");

    uint32_t before = 0, after = 0;

    m_test->register_read_32(sep_efuse::SBOOT_DIS_OFFSET, before);
    m_test->register_write_32(sep_efuse::SBOOT_DIS_OFFSET, 0xDEADBEEF);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SBOOT_DIS_OFFSET, after);

    if (after == before)
        report_test_pass("SBOOT_DIS is RO (write ignored)");
    else
        report_test_fail("SBOOT_DIS RO protection", "value changed after write");

    m_test->register_read_32(sep_efuse::CHIPLET_UID_OFFSET, before);
    m_test->register_write_32(sep_efuse::CHIPLET_UID_OFFSET, 0xCAFEBABE);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::CHIPLET_UID_OFFSET, after);

    if (after == before)
        report_test_pass("CHIPLET_UID[0] is RO (write ignored)");
    else
        report_test_fail("CHIPLET_UID RO protection", "value changed after write");
}

// =============================================================================
// Test 4: LOCKS_LO/HI WOSET behaviour
// =============================================================================

void testbench::test_woset_locks()
{
    report_test_start("Test 4: WOSET — LOCKS_LO/HI");

    uint32_t val = 0;

    // Bits 26..31 (SIP_UID, SYS_PUBK_HASH, SYS_UID) are chosen deliberately: LOCKS is
    // sticky with no software clear, so whatever this test sets stays locked for
    // every test after it. These three fields are read-only to software and nothing
    // else in the suite touches them, so exercising WOSET here cannot deny an access
    // a later test depends on. Test 22 covers enforcement on its own fields.
    m_test->register_write_32(sep_efuse::LOCKS_LO_OFFSET, 0x0C000000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val == 0x0C000000)
        report_test_pass("LOCKS_LO: first write sets SIP_UID lock bits");
    else
        report_test_fail("LOCKS_LO first write", "expected 0xC000000 got 0x" + std::to_string(val));

    // Attempt to clear bit 26 — should be ignored (WOSET)
    m_test->register_write_32(sep_efuse::LOCKS_LO_OFFSET, 0x08000000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val == 0x0C000000)
        report_test_pass("LOCKS_LO: bit 26 stays set after write-0 attempt");
    else
        report_test_fail("LOCKS_LO WOSET", "bit was cleared, expected 0xC000000 got 0x" + std::to_string(val));

    // Set additional bits
    m_test->register_write_32(sep_efuse::LOCKS_LO_OFFSET, 0xF0000000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::LOCKS_LO_OFFSET, val);
    if (val == 0xFC000000)
        report_test_pass("LOCKS_LO: additional bits ORed correctly");
    else
        report_test_fail("LOCKS_LO OR accumulation", "expected 0xFC000000 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 5: SIP_DIS WOSET behaviour
// =============================================================================

void testbench::test_woset_sip_dis()
{
    report_test_start("Test 5: WOSET — SIP_DIS_LO");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::SIP_DIS_LO_OFFSET, 0x1);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SIP_DIS_LO_OFFSET, val);
    if (val == 0x1)
        report_test_pass("SIP_DIS_LO: bit 0 set");
    else
        report_test_fail("SIP_DIS_LO set", "expected 0x1 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::SIP_DIS_LO_OFFSET, 0x0);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SIP_DIS_LO_OFFSET, val);
    if (val == 0x1)
        report_test_pass("SIP_DIS_LO: bit 0 not cleared by write-0 (WOSET)");
    else
        report_test_fail("SIP_DIS_LO WOSET", "bit cleared, expected 0x1 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::SIP_DIS_LO_OFFSET, 0x6);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SIP_DIS_LO_OFFSET, val);
    if (val == 0x7)
        report_test_pass("SIP_DIS_LO: bits 0,1,2 accumulated correctly");
    else
        report_test_fail("SIP_DIS_LO accumulation", "expected 0x7 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 6: SYS_DIS WOSET behaviour
// =============================================================================

void testbench::test_woset_sys_dis()
{
    report_test_start("Test 6: WOSET — SYS_DIS_LO");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::SYS_DIS_LO_OFFSET, 0x3);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SYS_DIS_LO_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SYS_DIS_LO: bits 0,1 set");
    else
        report_test_fail("SYS_DIS_LO set", "expected 0x3 got 0x" + std::to_string(val));

    m_test->register_write_32(sep_efuse::SYS_DIS_LO_OFFSET, 0x0);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::SYS_DIS_LO_OFFSET, val);
    if (val == 0x3)
        report_test_pass("SYS_DIS_LO: WOSET holds after write-0");
    else
        report_test_fail("SYS_DIS_LO WOSET", "bits cleared, expected 0x3 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 7: efuse_sense_done always reads 1; writes are ignored
// =============================================================================

void testbench::test_efuse_sense_done()
{
    report_test_start("Test 7: EFUSE_INTERFACE_CTRL_STATUS — efuse_sense_done");

    uint32_t val = 0;

    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x00000001)
        report_test_pass("EFUSE_INTF_STATUS reset=0x1 (efuse_sense_done=1)");
    else
        report_test_fail("EFUSE_INTF_STATUS reset", "expected 0x1 got 0x" + std::to_string(val));

    // Write attempt must be silently ignored (write_mask=0x0)
    m_test->register_write_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x00000001)
        report_test_pass("EFUSE_INTF_STATUS: write-0 ignored, still reads 0x1");
    else
        report_test_fail("EFUSE_INTF_STATUS RO", "expected 0x1 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 8: EFUSE_READ_CTRL is R/W
// =============================================================================

void testbench::test_efuse_read_ctrl_rw()
{
    report_test_start("Test 8: EFUSE_READ_CTRL R/W");

    uint32_t val = 0;

    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, 0x00001234);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    if (val == 0x00001234)
        report_test_pass("EFUSE_READ_CTRL: write 0x1234 reads back correctly");
    else
        report_test_fail("EFUSE_READ_CTRL R/W", "expected 0x1234 got 0x" + std::to_string(val));

    // Overwrite with a pattern that sets every software-writable bit. 0xABCC5678
    // keeps bit 16 (efuse_read_go) clear so the program/read sequence does not run,
    // and its bits 24 and 25 are dropped: read_busy/read_done/read_status are
    // hardware-owned (hw=w, sw=r), so the readback is the pattern minus [26:24].
    const uint32_t sw_writable = 0xABCC5678u & ~0x07000000u;
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, 0xABCC5678);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    if (val == sw_writable)
        report_test_pass("EFUSE_READ_CTRL: overwrite keeps hardware-owned status bits");
    else
        report_test_fail("EFUSE_READ_CTRL overwrite", "expected 0x" + std::to_string(sw_writable) +
            " got 0x" + std::to_string(val));
}

// =============================================================================
// Secure test mode — the whole of the secure_tm run
//
// efuse_guard.sv empties every fuse command while the strap is asserted, and
// efuse_shadow_regs.sv zeroes the secrets going to the key manager. Neither touches
// the shadow bus, which is the distinction worth pinning down: software reads the
// same values it always did, and only the hardware ports and the OTP interface change.
// =============================================================================

void testbench::test_secure_tm_mode()
{
    report_test_start("Secure test mode — fuse commands blocked, secrets zeroed");

    uint32_t val = 0;

    if (m_dut->get_secure_tm())
        report_test_pass("secure_tm strap latched at elaboration");
    else
        report_test_fail("secure_tm strap", "the config did not reach the model");

    // The gate is on the hardware port, not the read path.
    m_test->register_read_32(sep_efuse::CHIPLET_UID_OFFSET, val);
    const uint32_t uid0 = val;
    if (uid0 != 0)
        report_test_pass("shadow bus still returns CHIPLET_UID");
    else
        report_test_fail("shadow read under secure_tm",
                         "CHIPLET_UID reads zero, so this proves nothing about the port");

    bool uid_zeroed = true;
    for (int i = 0; i < 8; i++)
        if (m_dut->get_chiplet_uid()[i] != 0)
            uid_zeroed = false;
    if (uid_zeroed)
        report_test_pass("get_chiplet_uid() is zeroed for the key manager");
    else
        report_test_fail("secret zeroing", "the key manager would still receive the UID");

    // A program command is dropped, not refused: RTL is explicit that this is not an
    // error capture, so no status bit and no req_error.
    const uint32_t enable_prog = (1u << 27) | (1u << 16);
    m_test->register_write_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET,
                              enable_prog | (1u << 17) | (1u << 18) | 700u);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("program command completes with done and no error");
    else
        report_test_fail("secure_tm program", "expected done without error, got 0x" +
                         std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_PROGRAM_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0)
        report_test_pass("program read-back returns zero");
    else
        report_test_fail("secure_tm read-back", "expected 0x0 got 0x" + std::to_string(val));

    // A read of a word the shadow path just returned non-zero for: the OTP interface
    // returns nothing regardless.
    const uint32_t uid_bit = (sep_efuse::CHIPLET_UID_OFFSET / 4) * 32;
    m_test->register_write_32(sep_efuse::EFUSE_READ_CTRL_OFFSET,
                              (1u << 28) | (1u << 16) | uid_bit);
    wait(1, SC_NS);
    m_test->register_read_32(sep_efuse::EFUSE_READ_CTRL_OFFSET, val);
    if ((val & (1u << 25)) && !(val & (1u << 26)))
        report_test_pass("read command completes with done and no error");
    else
        report_test_fail("secure_tm read", "expected done without error, got 0x" +
                         std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_READ_INTERFACE_RD_DATA_OFFSET, val);
    if (val == 0)
        report_test_pass("OTP read returns zero for a word the shadow path shows");
    else
        report_test_fail("secure_tm OTP read", "expected 0x0 got 0x" + std::to_string(val));

    m_test->register_read_32(sep_efuse::EFUSE_INTERFACE_CTRL_STATUS_OFFSET, val);
    if (val == 0x1)
        report_test_pass("STATUS unchanged: a blocked command is not an error");
    else
        report_test_fail("secure_tm status", "expected 0x1 got 0x" + std::to_string(val));
}

// =============================================================================
// Main test sequence
// =============================================================================

void testbench::run_tests()
{
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));
    wait(10, SC_NS);

    REG_INFO(1, logger) << "\n========================================"
                         << "\nSEP_EFUSE IP TESTBENCH"
                         << "\n========================================" << std::endl;

    // secure_tm is a strap: it cannot be flipped once the run has started, so the
    // secure-mode behaviour gets a run of its own rather than a test in this list.
    // run_tests.sh launches the binary a second time with the config that sets it.
    if (m_dut->get_secure_tm()) {
        test_secure_tm_mode();
        report_test_summary();
        wait(100, SC_NS);
        sc_stop();
        return;
    }

    test_fuse_load_ro_registers();
    test_fuse_load_array_registers();
    test_ro_write_protection();
    test_woset_locks();
    test_woset_sip_dis();
    test_woset_sys_dis();
    test_efuse_sense_done();
    test_efuse_read_ctrl_rw();
    run_coverage_tests();

    report_test_summary();

    wait(100, SC_NS);
    sc_stop();
}

// =============================================================================
// sc_main
// =============================================================================

int sc_main(int argc, char *argv[])
{
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");

    sc_start();

#ifdef __GNUC__
#ifdef __COVERAGE__
    __gcov_dump();
#endif
#endif

    int ret = tb.m_tests_failed;
    std::quick_exit(ret);
    return ret;
}
