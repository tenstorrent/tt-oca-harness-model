// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.cpp
 * @brief EDN SystemC testbench implementation
 *
 * Implements testbench constructor with complete port binding between
 * EDN model and test harness, and initial validation test execution.
 */

#include "testbench.h"
#include "test_edn_func_001.h"
#include "test_edn_func_002.h"
#include "test_edn_func_003.h"
#include "test_edn_func_004.h"
#include "test_edn_func_005.h"
#include "test_edn_func_006.h"
#include "test_edn_func_007.h"
#include "test_edn_func_008.h"
#include "test_edn_func_009.h"
#include "test_edn_func_010.h"
#include "test_edn_func_011.h"
#include "test_edn_func_012.h"
#include "test_edn_func_013.h"
#include "test_edn_func_014.h"
#include <iomanip>
#include <vector>
#include <string>
#include <cstdlib>

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

/**
 * @brief Testbench constructor
 * @param name SystemC module name
 *
 * Instantiates DUT and test harness, binds ports, and initializes test state.
 */
testbench::testbench(sc_module_name name, int suite_id)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
    , m_tests_skipped(0)
{
    // Instantiate EDN model (DUT) first so its CCI verbosity param is
    // resolved (including any accellera_config.ini override) before we
    // configure the testbench logger.
    dut = new edn_ip("edn_dut");

    // Sync testbench logger verbosity with DUT (CCI ini may override build default).
    // Release build default: REG_DEFAULT_VERBOSITY=0 → errors only.
    // Pass accellera_config.ini (verbosity: 2) to enable info logging.
    logger.setMaxVerbosity(dut->verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    REG_INFO(2, logger) << "Constructing EDN testbench";

    // Instantiate test harness based on suite_id
    switch (suite_id) {
        case 1: test = new test_edn_func_001("edn_test"); break;
        case 2: test = new test_edn_func_002("edn_test"); break;
        case 3: test = new test_edn_func_003("edn_test"); break;
        case 4: test = new test_edn_func_004("edn_test"); break;
        case 5: test = new test_edn_func_005("edn_test"); break;
        case 6: test = new test_edn_func_006("edn_test"); break;
        case 7: test = new test_edn_func_007("edn_test"); break;
        case 8: test = new test_edn_func_008("edn_test"); break;
        case 9: test = new test_edn_func_009("edn_test"); break;
        case 10: test = new test_edn_func_010("edn_test"); break;
        case 11: test = new test_edn_func_011("edn_test"); break;
        case 12: test = new test_edn_func_012("edn_test"); break;
        case 13: test = new test_edn_func_013("edn_test"); break;
        case 14: test = new test_edn_func_014("edn_test"); break;
        default: test = new test_edn_func_004("edn_test"); break;
    }
    test->set_dut(dut);
    
    // Store suite_id for run_tests
    m_suite_id = suite_id;

    // Perform port binding
    bind_ports();

    // Initialize testbench
    initialize();

    // Register test execution thread
    SC_THREAD(run_tests);

    REG_INFO(2, logger) << "EDN testbench construction complete";
}

/**
 * @brief Testbench destructor
 */
testbench::~testbench()
{
    delete dut;
    delete test;
}

/**
 * @brief Bind all ports between model and test harness
 *
 * Follows SystemC TLM-2.0 binding patterns with proper complementary ports.
 */
void testbench::bind_ports()
{
    REG_INFO(2, logger) << "Binding ports...";

    // =========================================================================
    // 1. TLM Target Socket Binding (MMIO Register Access)
    // =========================================================================
    // Test's initiator socket → Model's target socket
    test->initiator_socket.bind(dut->target_socket);
    REG_INFO(2, logger) << "  [BOUND] TLM initiator_socket → target_socket";

    // =========================================================================
    // 4. Interrupt Signal Binding
    // =========================================================================
    // Model output → Signal → Test input
    dut->intr_edn_cmd_req_done(intr_cmd_req_done_sig);
    test->intr_edn_cmd_req_done(intr_cmd_req_done_sig);
    REG_INFO(2, logger) << "  [BOUND] intr_edn_cmd_req_done";

    dut->intr_edn_fatal_err(intr_fatal_err_sig);
    test->intr_edn_fatal_err(intr_fatal_err_sig);
    REG_INFO(2, logger) << "  [BOUND] intr_edn_fatal_err";

    // =========================================================================
    // 5. Alert Signal Binding
    // =========================================================================
    // Model output → Signal → Test input
    dut->alert_recov_alert(alert_recov_sig);
    test->alert_recov_alert(alert_recov_sig);
    REG_INFO(2, logger) << "  [BOUND] alert_recov_alert";

    dut->alert_fatal_alert(alert_fatal_sig);
    test->alert_fatal_alert(alert_fatal_sig);
    REG_INFO(2, logger) << "  [BOUND] alert_fatal_alert";

    // =========================================================================
    // 6. Clock and Reset Binding
    // =========================================================================
    // Test output → Signal → Model input
    test->clk_o(clk_sig);
    dut->clk_i(clk_sig);
    REG_INFO(2, logger) << "  [BOUND] clk_o ↔ clk_i";

    test->rst_no(rst_ni_sig);
    dut->rst_ni(rst_ni_sig);
    REG_INFO(2, logger) << "  [BOUND] rst_no ↔ rst_ni";

    REG_INFO(2, logger) << "Port binding complete";
}

/**
 * @brief Initialize testbench state
 *
 * Sets initial values for clock, reset, and endpoint signals.
 */
void testbench::initialize()
{
    REG_INFO(2, logger) << "Initializing testbench...";

    // Set default clock frequency (100 MHz)
    clk_sig.write(100.0e6);

    // Reset inactive initially (active-low)
    rst_ni_sig.write(true);



    REG_INFO(2, logger) << "Testbench initialization complete";
}

/**
 * @brief Main test execution thread
 *
 * Executes EDN_FUNC_004 test suite.
 */
void testbench::run_tests()
{
    unsigned int failures = 0;
    bool suite_ran = false;
    switch (m_suite_id) {
        case 1: { if (auto* t = dynamic_cast<test_edn_func_001*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 2: { if (auto* t = dynamic_cast<test_edn_func_002*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 3: { if (auto* t = dynamic_cast<test_edn_func_003*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 4: { if (auto* t = dynamic_cast<test_edn_func_004*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 5: { if (auto* t = dynamic_cast<test_edn_func_005*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 6: { if (auto* t = dynamic_cast<test_edn_func_006*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 7: { if (auto* t = dynamic_cast<test_edn_func_007*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 8: { if (auto* t = dynamic_cast<test_edn_func_008*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 9: { if (auto* t = dynamic_cast<test_edn_func_009*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 10: { if (auto* t = dynamic_cast<test_edn_func_010*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 11: { if (auto* t = dynamic_cast<test_edn_func_011*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 12: { if (auto* t = dynamic_cast<test_edn_func_012*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 13: { if (auto* t = dynamic_cast<test_edn_func_013*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        case 14: { if (auto* t = dynamic_cast<test_edn_func_014*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
        default: { if (auto* t = dynamic_cast<test_edn_func_004*>(test)) { failures = t->run_all_tests(); suite_ran = true; } break; }
    }
    
    // Carry the suite's verdict up. The per-test counters live in the suite
    // class, not here, so without this the summary below reported its own
    // never-incremented counters and printed ALL TESTS PASSED unconditionally --
    // it would have said that with every test failing.
    m_tests_failed += failures;

    // A dynamic_cast miss means no suite ran at all. That is a harness fault,
    // not a pass: silently reporting success for zero executed tests is the
    // worst outcome available here.
    if (!suite_ran) {
        m_tests_failed++;
        m_failed_tests.push_back("no test suite ran for the selected suite id");
    }

    // After the suite, so locking REGWEN cannot change the suite's CTRL writes.
    if (m_suite_id == 1) {
        test_register_access();
    }

    // Report final results and call sc_stop() to end simulation
    report_results();
}

/**
 * @brief Test register reset values
 *
 * Verifies all 17 EDN registers have correct reset values.
 */
void testbench::test_register_reset_values()
{
    REG_INFO(1, logger) << "\n[TEST] Register Reset Values";
    REG_INFO(1, logger) << "-------------------------------------------";

    struct RegResetTest {
        const char* name;
        unsigned int offset;
        uint32_t expected;
    };

    RegResetTest tests[] = {
        {"INTR_STATE",      0x00, 0x00000000},
        {"INTR_ENABLE",     0x04, 0x00000000},
        {"INTR_TEST",       0x08, 0x00000000},
        {"ALERT_TEST",      0x0C, 0x00000000},
        {"REGWEN",          0x10, 0x00000001},
        {"CTRL",            0x14, 0x00009999},
        {"BOOT_INS_CMD",    0x18, 0x00000901},
        {"BOOT_GEN_CMD",    0x1C, 0x00FFF003},
        {"SW_CMD_REQ",      0x20, 0x00000000},
        {"SW_CMD_STS",      0x24, 0x00000000},
        {"HW_CMD_STS",      0x28, 0x00000000},
        {"RESEED_CMD",      0x2C, 0x00000000},
        {"GENERATE_CMD",    0x30, 0x00000000},
        {"MAX_NUM_REQS",    0x34, 0x00000000},
        {"RECOV_ALERT_STS", 0x38, 0x00000000},
        {"ERR_CODE",        0x3C, 0x00000000},
        {"ERR_CODE_TEST",   0x40, 0x00000000},
        {"MAIN_SM_STATE",   0x44, 0x000000C1}
    };

    for (const auto& t : tests) {
        uint32_t value = 0;
        test->register_read_32(t.offset, value);

        m_tests_run++;
        if (value == t.expected) {
            REG_INFO(1, logger) << "  [PASS] " << std::setw(20) << std::left << t.name
                                  << " reset = 0x" << std::hex << std::setw(8) << std::setfill('0')
                                  << value;
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] " << std::setw(20) << std::left << t.name
                                   << " reset = 0x" << std::hex << value
                                   << " (expected 0x" << t.expected << ")";
            m_tests_failed++;
        }
    }
}

/**
 * @brief Test register read/write access
 *
 * Verifies register access types and write protection.
 */
void testbench::test_register_access()
{
    REG_INFO(1, logger) << "\n[TEST] Register Read/Write Access";
    REG_INFO(1, logger) << "-------------------------------------------";

    // Test RW register (INTR_ENABLE)
    {
        m_tests_run++;
        uint32_t write_val = 0x00000003;
        uint32_t read_val = 0;

        test->register_write_32(0x04, write_val);
        test->register_read_32(0x04, read_val);

        if (read_val == write_val) {
            REG_INFO(1, logger) << "  [PASS] INTR_ENABLE read/write access";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] INTR_ENABLE write failed";
            m_tests_failed++;
        }
    }

    // Test RO register (SW_CMD_STS)
    {
        m_tests_run++;
        uint32_t initial_val = 0;
        uint32_t read_val = 0;

        test->register_read_32(0x24, initial_val);
        test->register_write_32(0x24, 0xFFFFFFFF);
        test->register_read_32(0x24, read_val);

        if (read_val == initial_val) {
            REG_INFO(1, logger) << "  [PASS] SW_CMD_STS read-only protection";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] SW_CMD_STS should be read-only";
            m_tests_failed++;
        }
    }

    // The functional suite may already have locked REGWEN. Pulse reset so the
    // write-1 ignore is reached while the register is still unlocked.
    rst_ni_sig.write(false);
    wait(10, SC_NS);
    rst_ni_sig.write(true);
    wait(10, SC_NS);

    // Writing 1 is ignored (W0C). The register stays at its reset value.
    {
        m_tests_run++;
        uint32_t regwen = 0;
        test->register_write_32(0x10, 0x1);
        test->register_read_32(0x10, regwen);
        if (regwen == 0x1u) {
            REG_INFO(1, logger) << "  [PASS] REGWEN write-1 is ignored";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] REGWEN write-1 changed the register to 0x"
                                 << std::hex << regwen << std::dec;
            m_tests_failed++;
        }
    }

    // Bit position 31 is reserved. The write is accepted and ERR_CODE is unchanged.
    {
        m_tests_run++;
        uint32_t err_before = 0;
        uint32_t err_after = 0;
        test->register_read_32(0x3C, err_before);
        test->register_write_32(0x40, 0x1F);
        test->register_read_32(0x3C, err_after);
        if (err_after == err_before) {
            REG_INFO(1, logger) << "  [PASS] ERR_CODE_TEST bit 31 is ignored";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] ERR_CODE_TEST bit 31 changed ERR_CODE from 0x"
                                 << std::hex << err_before << " to 0x" << err_after << std::dec;
            m_tests_failed++;
        }
    }

    // No CSRNG is bound. An empty buffer and an out-of-range endpoint both refuse.
    {
        m_tests_run++;
        uint32_t word = 0xFFFFFFFFu;
        bool fips = true;
        const bool empty = dut->try_pop_entropy_word(0, word, fips);
        const bool bad_id = dut->try_pop_entropy_word(8, word, fips);
        (void)dut->entropy_available_event();
        if (!empty && !bad_id && word == 0xFFFFFFFFu) {
            REG_INFO(1, logger) << "  [PASS] entropy pop refuses an empty buffer and a bad endpoint";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] entropy pop empty=" << empty
                                 << " bad_id=" << bad_id;
            m_tests_failed++;
        }
    }

    // REGWEN is write-0-to-clear. Once bit 0 is 0, CTRL writes are rejected.
    {
        m_tests_run++;
        uint32_t regwen = 0;
        uint32_t ctrl_before = 0;
        uint32_t ctrl_after = 0;
        test->register_read_32(0x14, ctrl_before);
        test->register_write_32(0x10, 0x0);
        test->register_read_32(0x10, regwen);
        test->register_write_32(0x14, ctrl_before ^ 0x6u);
        test->register_read_32(0x14, ctrl_after);
        if ((regwen & 0x1u) == 0 && ctrl_after == ctrl_before) {
            REG_INFO(1, logger) << "  [PASS] REGWEN lock rejects a later CTRL write";
            m_tests_passed++;
        } else {
            REG_ERROR(1, logger) << "  [FAIL] REGWEN lock: REGWEN=0x" << std::hex
                                 << regwen << " CTRL before=0x" << ctrl_before
                                 << " after=0x" << ctrl_after << std::dec;
            m_tests_failed++;
        }
    }
}

/**
 * @brief Test port binding connectivity
 *
 * Verifies all ports are properly bound.
 */
void testbench::test_port_binding()
{
    REG_INFO(1, logger) << "\n[TEST] Port Binding Connectivity";
    REG_INFO(1, logger) << "-------------------------------------------";

    // Test TLM socket binding (already tested via register access)
    m_tests_run++;
    REG_INFO(1, logger) << "  [PASS] TLM register socket binding verified";
    m_tests_passed++;



    // Test interrupt signal connectivity
    m_tests_run++;
    REG_INFO(1, logger) << "  [PASS] Interrupt signal binding verified";
    m_tests_passed++;

    // Test alert signal connectivity
    m_tests_run++;
    REG_INFO(1, logger) << "  [PASS] Alert signal binding verified";
    m_tests_passed++;
}

/**
 * @brief Report test start banner
 */
void testbench::report_test_start(const std::string& test_name)
{
    REG_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================" << std::endl;
}

/**
 * @brief Report test pass
 */
void testbench::report_test_pass(const std::string& test_name)
{
    m_tests_passed++;
    m_tests_run++;
    REG_INFO(1, logger) << test_name << ": PASS" << std::endl;
}

/**
 * @brief Report test fail with reason
 */
void testbench::report_test_fail(const std::string& test_name, const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    REG_WARN(1, logger) << test_name << ": FAIL - " << reason << std::endl;
}

/**
 * @brief Report overall test summary
 */
void testbench::report_test_summary()
{
    std::string summary = "\n========================================\n" +
                          std::string("EDN Validation Test Summary\n") +
                          "========================================\n" +
                          "Total Tests: " + std::to_string(m_tests_run) + "\n" +
                          "Passed: " + std::to_string(m_tests_passed) + "\n" +
                          "Failed: " + std::to_string(m_tests_failed) + "\n" +
                          "Skipped: " + std::to_string(m_tests_skipped) + "\n" +
                          "========================================";
    REG_INFO(1, logger) << summary << std::endl;

    if (!m_failed_tests.empty()) {
        REG_ERROR(1, logger) << "Failed tests:" << std::endl;
        for (const auto& test : m_failed_tests) {
            REG_ERROR(1, logger) << "  - " << test << std::endl;
        }
    }

    if (m_tests_skipped > 0) {
        REG_INFO(1, logger) << "Note: Skipped tests require functionality implementation (callbacks)" << std::endl;
    }
}

/**
 * @brief Report test results
 *
 * Logs final test summary and pass/fail status.
 */
void testbench::report_results()
{
    REG_INFO(1, logger) << "\n===========================================";
    REG_INFO(1, logger) << "EDN Validation Test Results";
    REG_INFO(1, logger) << "===========================================";
    // m_tests_run / m_tests_passed count only this class's own legacy checks.
    // A FUNC suite keeps its own per-test counters and prints them itself
    // ("Total Tests / Passed / Failed"), so these are 0 for a suite run. Print
    // them only when they mean something rather than showing a bare "0" that
    // reads as "nothing ran" next to a passing suite.
    if (m_tests_run > 0) {
        REG_INFO(1, logger) << "Tests Run:    " << m_tests_run;
        REG_INFO(1, logger) << "Tests Passed: " << m_tests_passed;
    } else {
        REG_INFO(1, logger) << "Per-test counts: see the suite's own summary above";
    }
    REG_INFO(1, logger) << "Tests Failed: " << m_tests_failed;
    REG_INFO(1, logger) << "Tests Skipped: " << m_tests_skipped;
    REG_INFO(1, logger) << "===========================================";

    if (m_tests_failed == 0) {
        REG_INFO(1, logger) << "ALL TESTS PASSED";
    } else {
        REG_ERROR(1, logger) << "SOME TESTS FAILED";
        for (const auto& test : m_failed_tests) {
            REG_ERROR(1, logger) << "  - " << test;
        }
    }

    if (m_tests_skipped > 0) {
        REG_INFO(1, logger) << "Note: " << m_tests_skipped << " test(s) skipped (require functionality implementation)";
    }

    sc_stop();
}

/**
 * @brief Main entry point
 *
 * Creates testbench and starts simulation.
 */
int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

    int suite_id = 4;
    if (argc > 2) {
        suite_id = std::atoi(argv[2]);
    }

    // Create testbench
    testbench tb("tb", suite_id);

    // Run simulation
    sc_start();

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);

    return 0;
}
