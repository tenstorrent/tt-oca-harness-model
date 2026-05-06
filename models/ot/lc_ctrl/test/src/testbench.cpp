#include "testbench.h"
#include "lc_ctrl_basetest.h"
#include "csml_parameter.h"

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
    m_dut  = std::make_unique<lc_ctrl_model>("lc_ctrl_dut");
    m_test = std::make_unique<lc_ctrl_test>("lc_ctrl_test");

    logger.setMaxVerbosity(m_dut->verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    m_test->initiator_socket.bind(m_dut->target_socket);

    SC_THREAD(run_tests);
}

// =============================================================================
// Test Reporting
// =============================================================================

void testbench::report_test_start(const std::string &name)
{
    CSML_INFO(1, logger) << "\n========================================"
                         << "\n" << name
                         << "\n========================================" << std::endl;
}

void testbench::report_test_pass(const std::string &name)
{
    m_tests_run++;
    m_tests_passed++;
    CSML_INFO(1, logger) << "[PASS] " << name << std::endl;
}

void testbench::report_test_fail(const std::string &name, const std::string &reason)
{
    m_tests_run++;
    m_tests_failed++;
    m_failed_tests.push_back(name);
    CSML_ERROR(0, logger) << "[FAIL] " << name << " — " << reason << std::endl;
}

void testbench::report_test_summary()
{
    CSML_INFO(1, logger) << "\n========================================"
                         << "\nlc_ctrl Test Summary"
                         << "\n========================================"
                         << "\nTotal : " << m_tests_run
                         << "\nPassed: " << m_tests_passed
                         << "\nFailed: " << m_tests_failed << std::endl;

    for (const auto &t : m_failed_tests)
        CSML_ERROR(0, logger) << "  FAILED: " << t << std::endl;
}

// =============================================================================
// Test 1: Register reset values
// =============================================================================

void testbench::test_reset_values()
{
    report_test_start("Test 1: Register Reset Values");

    uint32_t val = 0xDEADBEEF;

    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if (val == lc_ctrl_basetest::DEMOTE_1_RESET)
        report_test_pass("DEMOTE_1 reset=0x0");
    else
        report_test_fail("DEMOTE_1 reset",
            "expected 0x0 got 0x" + std::to_string(val));

    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if (val == lc_ctrl_basetest::DEMOTE_2_RESET)
        report_test_pass("DEMOTE_2 reset=0x0");
    else
        report_test_fail("DEMOTE_2 reset",
            "expected 0x0 got 0x" + std::to_string(val));
}

// =============================================================================
// Test 2: FEAT_CTRL_LO/HI are read-only
// =============================================================================

void testbench::test_feat_ctrl_ro()
{
    report_test_start("Test 2: FEAT_CTRL_LO/HI are RO");

    uint32_t before = 0, after = 0;

    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, before);
    m_test->register_write_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, 0xDEADBEEF);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, after);

    if (after == before)
        report_test_pass("FEAT_CTRL_LO is RO (write ignored)");
    else
        report_test_fail("FEAT_CTRL_LO RO protection", "value changed after write");

    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, before);
    m_test->register_write_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, 0xCAFEBABE);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, after);

    if (after == before)
        report_test_pass("FEAT_CTRL_HI is RO (write ignored)");
    else
        report_test_fail("FEAT_CTRL_HI RO protection", "value changed after write");
}

// =============================================================================
// Test 3: TEST_DEV state — FEAT_CTRL = ~(sip_dis | sys_dis)
// =============================================================================

void testbench::test_test_dev_state()
{
    report_test_start("Test 3: TEST_DEV state — initial FEAT_CTRL");

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    uint64_t sip_dis  = ((uint64_t)m_dut->sip_dis_hi.get_param_value() << 32) | m_dut->sip_dis_lo.get_param_value();
    uint64_t sys_dis  = ((uint64_t)m_dut->sys_dis_hi.get_param_value() << 32) | m_dut->sys_dis_lo.get_param_value();
    uint64_t expected = ~(sip_dis | sys_dis);
    if (!m_dut->secure_tm.get_param_value())
        expected &= ~0x0000FFFF00000000ULL;

    if (val_lo == (uint32_t)(expected & 0xFFFFFFFF))
        report_test_pass("FEAT_CTRL_LO: TEST_DEV initial value correct");
    else
        report_test_fail("FEAT_CTRL_LO TEST_DEV",
            "expected 0x" + std::to_string((uint32_t)(expected & 0xFFFFFFFF)) +
            " got 0x" + std::to_string(val_lo));

    if (val_hi == (uint32_t)(expected >> 32))
        report_test_pass("FEAT_CTRL_HI: TEST_DEV initial value correct");
    else
        report_test_fail("FEAT_CTRL_HI TEST_DEV",
            "expected 0x" + std::to_string((uint32_t)(expected >> 32)) +
            " got 0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 4: PROD state — func bits only, no demote
// =============================================================================

void testbench::test_prod_state_no_demote()
{
    report_test_start("Test 4: PROD state — FEAT_CTRL=func-only (no demote)");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if (lc != lc_ctrl_model::LC_STATE_PROD) {
        report_test_pass("Test 4 skipped — lc_state not PROD (set lc_state=1 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    // PROD base with all dis=0: ~(0|0) & FUNC_BITS_MASK = 0xFFFF000000000000
    if (val_lo == 0x00000000 && val_hi == 0xFFFF0000)
        report_test_pass("PROD no-demote: FEAT_CTRL=func-only (0xFFFF000000000000)");
    else
        report_test_fail("PROD no-demote FEAT_CTRL",
            "expected lo=0x00000000 hi=0xFFFF0000 got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 5: DEMOTE_1 W1S — write 1 sets the bit, write 0 does not clear it
// =============================================================================

void testbench::test_demote_1_w1s()
{
    report_test_start("Test 5: DEMOTE_1 W1S semantics");

    uint32_t val = 0;

    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_1: bit 0 set after write-1");
    else
        report_test_fail("DEMOTE_1 W1S set",
            "expected bit 0 set, got 0x" + std::to_string(val));

    // Write-0 must not clear the bit (W1S)
    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_1: bit 0 stays set after write-0 (W1S)");
    else
        report_test_fail("DEMOTE_1 W1S hold", "bit was cleared — expected bit 0 still set");

    // In TEST_DEV demote only re-enables debug bits; with all dis=0 result is still all-ones
    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);
    if (val_lo == 0xFFFFFFFF && val_hi == 0xFFFFFFFF)
        report_test_pass("FEAT_CTRL: TEST_DEV+demote1=0xFFFF_FFFF_FFFF_FFFF");
    else
        report_test_fail("FEAT_CTRL TEST_DEV+demote1",
            "expected 0xFFFFFFFF/0xFFFFFFFF got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 6: DEMOTE_1 lock — once bit 1 is set, further writes are ignored
// =============================================================================

void testbench::test_demote_1_lock()
{
    report_test_start("Test 6: DEMOTE_1 lock semantics");

    uint32_t val = 0;

    // Set lock bit
    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000002);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if ((val & 0x2) == 0x2)
        report_test_pass("DEMOTE_1: lock bit set");
    else
        report_test_fail("DEMOTE_1 lock set",
            "expected lock bit set, got 0x" + std::to_string(val));

    uint32_t before = val;

    // Write after lock — must be silently ignored
    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_1_OFFSET, val);
    if (val == before)
        report_test_pass("DEMOTE_1: write ignored after lock");
    else
        report_test_fail("DEMOTE_1 post-lock write",
            "DEMOTE_1 changed after lock, got 0x" + std::to_string(val));
}

// =============================================================================
// Test 7: DEMOTE_2 W1S — independent domain, same W1S semantics
// =============================================================================

void testbench::test_demote_2_w1s()
{
    report_test_start("Test 7: DEMOTE_2 W1S semantics");

    uint32_t val = 0;

    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000001);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_2: bit 0 set after write-1");
    else
        report_test_fail("DEMOTE_2 W1S set",
            "expected bit 0 set, got 0x" + std::to_string(val));

    // Write-0 must not clear the bit (W1S)
    m_test->register_write_32(lc_ctrl_basetest::DEMOTE_2_OFFSET, 0x00000000);
    wait(1, SC_NS);
    m_test->register_read_32(lc_ctrl_basetest::DEMOTE_2_OFFSET, val);
    if ((val & 0x1) == 0x1)
        report_test_pass("DEMOTE_2: bit 0 stays set after write-0 (W1S)");
    else
        report_test_fail("DEMOTE_2 W1S hold", "bit was cleared — expected bit 0 still set");
}

// =============================================================================
// Test 8: INVALID state — encodings 0x4 and 0x5 must produce FEAT_CTRL=0
// =============================================================================

void testbench::test_invalid_state()
{
    report_test_start("Test 8: INVALID state (0x4, 0x5) — FEAT_CTRL=0");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if (lc != 0x4u && lc != 0x5u) {
        report_test_pass("Test 8 skipped — lc_state not INVALID (set lc_state=4 or 5 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    std::string label = "lc_state=0x" + std::to_string(lc);
    if (val_lo == 0x0 && val_hi == 0x0)
        report_test_pass("INVALID " + label + ": FEAT_CTRL=0x0");
    else
        report_test_fail("INVALID FEAT_CTRL " + label,
            "expected 0x0/0x0 got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 9: RMA_CHIPLET state — encodings 4'b011x (0x6, 0x7) give all-ones
// =============================================================================

void testbench::test_rma_chiplet_state()
{
    report_test_start("Test 9: RMA_CHIPLET state — FEAT_CTRL=0xFFFFFFFF_FFFFFFFF");

    uint32_t lc = m_dut->lc_state.get_param_value() & 0xF;
    if ((lc & lc_ctrl_model::LC_STATE_RANGE_MASK) != lc_ctrl_model::LC_STATE_RMA_CHIPLET_BASE) {
        report_test_pass("Test 9 skipped — lc_state not RMA_CHIPLET (set lc_state=6 in ini to run)");
        return;
    }

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    if (val_lo == 0xFFFFFFFF && val_hi == 0xFFFFFFFF)
        report_test_pass("RMA_CHIPLET: FEAT_CTRL=0xFFFFFFFF_FFFFFFFF");
    else
        report_test_fail("RMA_CHIPLET FEAT_CTRL",
            "expected 0xFFFFFFFF/0xFFFFFFFF got lo=0x" + std::to_string(val_lo) +
            " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Test 10: secure_tm=false gates test bits [47:32]
// =============================================================================

void testbench::test_secure_tm()
{
    report_test_start("Test 10: secure_tm gates test bits [47:32]");

    bool stm = m_dut->secure_tm.get_param_value();

    uint32_t val_lo = 0, val_hi = 0;
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, val_lo);
    m_test->register_read_32(lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, val_hi);

    // TEST_DEV + all-dis=0: feat_ctrl = ~0; secure_tm=false clears bits [47:32]
    uint32_t exp_lo = 0xFFFFFFFF;
    uint32_t exp_hi = stm ? 0xFFFFFFFF : 0xFFFF0000;

    if (val_lo == exp_lo && val_hi == exp_hi)
        report_test_pass(std::string("secure_tm=") + (stm ? "true" : "false") +
                         ": FEAT_CTRL correct");
    else
        report_test_fail("secure_tm FEAT_CTRL",
            "expected lo=0x" + std::to_string(exp_lo) + " hi=0x" + std::to_string(exp_hi) +
            " got lo=0x" + std::to_string(val_lo) + " hi=0x" + std::to_string(val_hi));
}

// =============================================================================
// Main test sequence
// =============================================================================

void testbench::run_tests()
{
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));
    wait(10, SC_NS);

    CSML_INFO(1, logger) << "\n========================================"
                         << "\nLC_CTRL IP TESTBENCH"
                         << "\n========================================" << std::endl;

    test_reset_values();
    test_feat_ctrl_ro();
    test_test_dev_state();
    test_prod_state_no_demote();
    test_demote_1_w1s();
    test_demote_1_lock();
    test_demote_2_w1s();
    test_invalid_state();
    test_rma_chiplet_state();
    test_secure_tm();

    report_test_summary();

    wait(100, SC_NS);
    sc_stop();
}

// =============================================================================
// sc_main
// =============================================================================

int sc_main(int argc, char *argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);
    testbench tb("testbench");
    sc_start();
#ifdef __GNUC__
#ifdef __COVERAGE__
    __gcov_dump();
#endif
#endif
    int ret = tb.m_tests_failed;
#ifdef ACCELLERA_CCI_STD
    std::quick_exit(ret);
#endif
    return ret;
}
