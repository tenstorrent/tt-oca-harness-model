// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"


// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

testbench::testbench(sc_module_name name)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    // Initialize regmodel logger
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Instantiate DUT and testmodel
    m_aes = std::make_unique<aes_model>("aes_dut");

    // Sync testbench logger verbosity with DUT (CCI ini may override build default)
    logger.setMaxVerbosity(m_aes->verbosity.get_param_value());
    m_test = std::make_unique<aes_test>("aes_test");

    // Bind TLM sockets between testmodel and DUT
    m_test->initiator_socket.bind(m_aes->target_socket);

    // Bind Clock Interface
    m_test->clk_o.bind(clk_signal);
    m_aes->clk_i.bind(clk_signal);

    // Bind Reset Interface
    m_test->rst_no.bind(rst_signal);
    m_aes->rst_ni.bind(rst_signal);

    // Bind Key Manager Sideload Interface (test pushes keys via TLM)
    m_test->keymgr_socket.bind(m_aes->keymgr_tl_socket);

    // Bind Alert Interfaces (AES outputs, Test monitors)
    m_aes->alert_recov_ctrl_update_err.bind(alert_recov_signal);
    m_test->alert_recov_ctrl_update_err_i.bind(alert_recov_signal);

    m_aes->alert_fatal_fault.bind(alert_fatal_signal);
    m_test->alert_fatal_fault_i.bind(alert_fatal_signal);

    // Bind Idle Status Interface (AES outputs, Test monitors)
    m_aes->idle_o.bind(idle_signal);
    m_test->idle_i.bind(idle_signal);

    // Bind Life Cycle Escalation Interface (Test drives, AES monitors)
    m_test->lc_escalate_en_o.bind(lc_escalate_signal);
    m_aes->lc_escalate_en.bind(lc_escalate_signal);

    // Register test process
    SC_THREAD(run_tests);
}

// =============================================================================
// Test Result Reporting Helpers
// =============================================================================
void testbench::report_test_result(const char* test_name, bool passed)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "\n========================================\n"
                             << "[*** TEST PASSED ***] " << test_name << "\n"
                             << "========================================\n" << std::endl;
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(0, logger) << "\n========================================\n"
                              << "[XXX TEST FAILED XXX] " << test_name << "\n"
                              << "========================================\n" << std::endl;
    }
}

void testbench::report_test_start(const std::string& test_name)
{
    REG_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================" << std::endl;
}

void testbench::report_test_pass(const std::string& test_name)
{
    m_tests_passed++;
    m_tests_run++;
    REG_INFO(1, logger) << test_name << ": PASS" << std::endl;
}

void testbench::report_test_fail(const std::string& test_name,
                                 const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    REG_WARN(1, logger) << test_name << ": FAIL - " << reason << std::endl;
}

void testbench::report_test_summary()
{
    std::string summary = "\n========================================\n" +
                          std::string("FUNC-AES-001 Test Summary\n") +
                          "========================================\n" +
                          "Total Tests: " + std::to_string(m_tests_passed + m_tests_failed) + "\n" +
                          "Passed: " + std::to_string(m_tests_passed) + "\n" +
                          "Failed: " + std::to_string(m_tests_failed) + "\n" +
                          "========================================";
    REG_INFO(1, logger) << summary << std::endl;
}

void testbench::run_tests()
{
    //Set global quantum for temporal decoupling
    // This determines when quantum keepers need to synchronize
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));

    clk_signal.write(false);  // Clock signal (initial state)
    rst_signal.write(true);
    lc_escalate_signal.write(false);

    wait(10, SC_NS);

    REG_INFO(1, logger) << "\n========================================" 
                         << "AES IP TESTBENCH" 
                         << "========================================" << std::endl;

    // Test 1: Reset and Port Binding Verification
    report_test_start("test_reset_and_port_binding_verification");

    // Trigger reset
    m_test->trigger_reset();
    wait(20, SC_NS);

    {
        bool idle_ok   = m_test->is_idle();
        bool alerts_ok = !alert_recov_signal.read() && !alert_fatal_signal.read();

        if (idle_ok && alerts_ok) {
            report_test_pass("test_reset_and_port_binding_verification");
        } else {
            std::string reason;
            if (!idle_ok)   reason += "AES not idle after reset. ";
            if (!alerts_ok) reason += "Unexpected alerts after reset.";
            report_test_fail("test_reset_and_port_binding_verification", reason);
        }
    }

    // Test 2: Idle Status Interface Test
    report_test_start("test_idle_status_interface");

    if (idle_signal.read()) {
        report_test_pass("test_idle_status_interface");
    } else {
        report_test_fail("test_idle_status_interface", "idle_o signal is LOW after reset");
    }

    // Test 3: Clock Interface Test
    report_test_start("test_clock_interface");

    {
        (void)clk_signal.read();  // Verify signal is bound and readable
        report_test_pass("test_clock_interface");
    }

    // Test 4: Key Manager Sideload Interface Test
    report_test_start("test_keymgr_sideload_interface");

    {
        aes_if::keymgr_sideload_key_t test_key;
        test_key.valid = true;
        test_key.key_share0[0] = 0x01234567;
        test_key.key_share1[0] = 0x89ABCDEF;

        m_test->set_keymgr_key(test_key);
        wait(10, SC_NS);

        aes_if::keymgr_sideload_key_t retrieved_key;
        bool key_valid = m_test->get_keymgr_key(retrieved_key);

        if (key_valid && retrieved_key.key_share0[0] == test_key.key_share0[0]) {
            report_test_pass("test_keymgr_sideload_interface");
        } else {
            report_test_fail("test_keymgr_sideload_interface",
                             "get_key failed or key_share0[0] mismatch");
        }
    }

    // Test 6: Life Cycle Escalation Interface Test
    report_test_start("test_life_cycle_escalation_interface");

    {
        m_test->trigger_escalation();
        wait(50, SC_NS);

        bool fatal_asserted = alert_fatal_signal.read();
        bool not_idle       = !idle_signal.read();

        if (fatal_asserted && not_idle) {
            report_test_pass("test_life_cycle_escalation_interface");
        } else {
            std::string reason;
            if (!fatal_asserted) reason += "alert_fatal_fault not asserted after escalation. ";
            if (!not_idle)       reason += "AES still idle after escalation (expected ERROR state).";
            report_test_fail("test_life_cycle_escalation_interface", reason);
        }
    }

    // Test 7: Reset Recovery from Error State
    report_test_start("test_reset_recovery_from_error_state");

    {
        // De-assert escalation first
        lc_escalate_signal.write(false);
        wait(10, SC_NS);

        // Trigger reset to recover
        m_test->trigger_reset();
        wait(20, SC_NS);

        bool alerts_cleared = !alert_fatal_signal.read() && !alert_recov_signal.read();
        bool idle_restored  = idle_signal.read();

        if (alerts_cleared && idle_restored) {
            report_test_pass("test_reset_recovery_from_error_state");
        } else {
            std::string reason;
            if (!alerts_cleared) reason += "Alerts still active after reset. ";
            if (!idle_restored)  reason += "AES not idle after reset recovery.";
            report_test_fail("test_reset_recovery_from_error_state", reason);
        }
    }

    REG_INFO(1, logger) << "========================================"
                         << "All Port Binding Tests Completed"
                         << "========================================" << std::endl;

    // Run FUNC-AES-001 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-001: Block Cipher Operations" 
                         << "========================================" << std::endl;

    // ECB Mode - All Key Sizes
    test_openssl_aes128_ecb_equivalence();
    test_openssl_aes192_ecb_equivalence();
    test_openssl_aes256_ecb_equivalence();

    // CBC Mode - All Key Sizes
    test_openssl_aes128_cbc_equivalence();
    test_openssl_aes192_cbc_equivalence();
    test_openssl_aes256_cbc_equivalence();

    // CFB Mode - All Key Sizes
    test_openssl_aes128_cfb_equivalence();
    test_openssl_aes192_cfb_equivalence();
    test_openssl_aes256_cfb_equivalence();

    // OFB Mode - All Key Sizes
    test_openssl_aes128_ofb_equivalence();
    test_openssl_aes192_ofb_equivalence();
    test_openssl_aes256_ofb_equivalence();

    // CTR Mode - All Key Sizes
    test_openssl_aes128_ctr_equivalence();
    test_openssl_aes192_ctr_equivalence();
    test_openssl_aes256_ctr_equivalence();

    // Decryption Equivalence Tests
    // ECB Mode - All Key Sizes
    test_openssl_aes128_ecb_decryption_equivalence();
    test_openssl_aes192_ecb_decryption_equivalence();
    test_openssl_aes256_ecb_decryption_equivalence();

    // CBC Mode - All Key Sizes
    test_openssl_aes128_cbc_decryption_equivalence();
    test_openssl_aes192_cbc_decryption_equivalence();
    test_openssl_aes256_cbc_decryption_equivalence();

    // CFB Mode - All Key Sizes
    test_openssl_aes128_cfb_decryption_equivalence();
    test_openssl_aes192_cfb_decryption_equivalence();
    test_openssl_aes256_cfb_decryption_equivalence();

    // OFB Mode - All Key Sizes
    test_openssl_aes128_ofb_decryption_equivalence();
    test_openssl_aes192_ofb_decryption_equivalence();
    test_openssl_aes256_ofb_decryption_equivalence();

    // CTR Mode - All Key Sizes
    test_openssl_aes128_ctr_decryption_equivalence();
    test_openssl_aes192_ctr_decryption_equivalence();
    test_openssl_aes256_ctr_decryption_equivalence();

    // Run FUNC-AES-002 tests
    REG_INFO(1, logger) << "\n\n========================================"
                         << "FUNC-AES-002: Sideload Key Management, Write Protection(non-idle), Invalid Key Length"
                         << "========================================" << std::endl;

    // Sideload Key Equivalence Tests
    test_openssl_sideload_aes128_ecb_equivalence();
    test_openssl_sideload_aes256_cbc_equivalence();
    test_openssl_sideload_ignores_key_share_writes();

    // Write Protection Validation Tests
    test_ctrl_shadowed_write_protection_when_non_idle();
    test_iv_write_protection_when_non_idle();
    
    // Invalid Key Length Handling Test
    test_openssl_invalid_key_length_defaults_to_aes256();


    // Run FUNC-AES-003 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-003: Initialization Vector Management" 
                         << "========================================" << std::endl;

    test_iv_register_read_write();
    test_cbc_encryption_iv_auto_update();
    test_cbc_decryption_iv_auto_update();
    test_cfb_mode_iv_auto_update();
    test_ofb_mode_iv_auto_update();
    test_ctr_mode_counter_increment();
    test_ctr_counter_overflow();
    test_ecb_mode_ignores_iv();
    test_iv_required_for_non_ecb_auto_start();
    test_multi_block_cbc_chaining();

    // Run FUNC-AES-004 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-004: Automatic Operation Mode" 
                         << "========================================" << std::endl;

    test_auto_start_on_data_in_complete();
    test_input_ready_status_behavior();
    test_output_valid_status_behavior();
    test_output_protection_prevents_overwrite();
    test_multi_block_pipelining();
    test_back_to_back_processing();
    test_auto_start_requires_all_conditions();
    test_automatic_vs_manual_mode();
    test_status_transitions_during_operation();

    // Run FUNC-AES-005 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-005: Manual Operation Mode" 
                         << "========================================" << std::endl;

    test_manual_mode_explicit_start();
    test_manual_mode_no_back_pressure();
    test_output_lost_flag_on_overwrite();
    test_output_lost_cleared_by_ctrl_write();
    test_manual_mode_precondition_data_in();
    test_manual_mode_precondition_key();
    test_manual_mode_precondition_iv();
    test_manual_mode_multi_block_output_overwrite();

    // Run FUNC-AES-006 tests (except REGWEN locking tests - those run last)
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-006: Shadowed Register Fault Detection" 
                         << "========================================" << std::endl;

    test_ctrl_shadowed_two_write_matching();
    test_ctrl_shadowed_two_write_mismatch();
    test_ctrl_aux_shadowed_two_write_matching();
    test_ctrl_aux_shadowed_two_write_mismatch();
    test_shadowed_read_resets_sequence();
    test_alert_cleared_by_successful_write();
    test_multiple_consecutive_mismatches();
    test_shadowed_write_rejected_when_busy();
    test_ctrl_shadowed_per_field_update_error();
    test_ctrl_aux_shadowed_per_field_update_error();
    test_ctrl_shadowed_sanitised_readback();

    // Galois/Counter Mode
    REG_INFO(1, logger) << "\n========================================"
                         << "Galois/Counter Mode"
                         << "========================================" << std::endl;

    test_gcm_ctrl_reset_and_sanitisation();
    test_gcm_phase_transition_gating();
    test_gcm_encrypt_nist_case4();
    test_gcm_decrypt_nist_case4();
    test_gcm_save_restore();

    // Run FUNC-AES-007 tests (Register Interface Tests for Security Features)
    REG_INFO(1, logger) << "\n========================================"
                         << "FUNC-AES-007: Register Interface Tests for Security Features"
                         << "========================================" << std::endl;

    test_ctrl_aux_key_touch_forces_reseed_register();
    test_trigger_key_iv_data_in_clear();
    test_trigger_data_out_clear();

    // Run FUNC-AES-008 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-008: Alert Generation and Error Reporting" 
                         << "========================================" << std::endl;

    test_alert_test_register_recoverable_trigger();
    test_alert_test_register_fatal_trigger();
    test_fatal_alert_terminal_error_state();
    test_fatal_alert_recovery_requires_reset();
    test_life_cycle_escalation_fatal_alert();
    test_life_cycle_escalation_register_writes_ignored();
    test_register_clearing_on_fatal_alert();
    test_status_register_readable_in_error_state();
    test_multiple_fatal_alert_triggers();
    test_persistent_error_state();
    test_recoverable_alert_no_error_state();
    test_alert_outputs_match_status_register();


    // Run FUNC-AES-009 tests
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-009: Functional Timing and Temporal Decoupling" 
                         << "========================================" << std::endl;


    // Reset DUT to clear any state from previous tests
    m_test->trigger_reset();
    wait(20, SC_NS);

    test_func009_aes128_block_latency();
    test_func009_aes192_block_latency();
    test_func009_aes256_block_latency();
    test_func009_timing_across_modes();

    REG_INFO(1, logger) << "\n========================================"
                         << "Coverage: uncovered model paths"
                         << "========================================" << std::endl;

    m_test->trigger_reset();
    wait(20, SC_NS);

    test_coverage_prng_reseed_trigger_and_rates();
    test_coverage_keymgr_read_rejected();
    test_coverage_escalation_aborts_cipher();
    test_coverage_error_state_writes_rejected();
    test_coverage_gcm_shadow_and_busy();
    test_coverage_sideload_and_gcm_init_guards();
    test_coverage_gcm_aes192_aes256_init();
    test_coverage_auto_start_gcm_and_output_valid();
    
    // Run FUNC-AES-006 REGWEN locking tests LAST (these lock CTRL_AUX_REGWEN permanently)
    REG_INFO(1, logger) << "\n========================================" 
                         << "FUNC-AES-006: REGWEN Locking Tests (run last)" 
                         << "========================================" << std::endl;

    // Reset DUT to clear any ERROR state from previous tests
    m_test->trigger_reset();
    wait(20, SC_NS);

    test_ctrl_aux_write_protection_when_locked();
    test_ctrl_aux_regwen_cannot_unlock();
    test_ctrl_aux_shadowed_read_resets_sequence();

    // Print final test summary
    REG_INFO(1, logger) << "\n========================================" 
                         << "       TEST SUITE SUMMARY" 
                         << "========================================" << std::endl;

    std::stringstream ss;
    ss << "Total Tests:  " << m_tests_run;
    REG_INFO(1, logger) << ss.str() << std::endl;

    ss.str("");
    ss << "Passed:       " << m_tests_passed << " (PASS)";
    REG_INFO(1, logger) << ss.str() << std::endl;

    ss.str("");
    ss << "Failed:       " << m_tests_failed << " (FAIL)";
    REG_INFO(1, logger) << ss.str() << std::endl;

    if (m_tests_run > 0) {
        double success_rate = (100.0 * m_tests_passed) / m_tests_run;
        ss.str("");
        ss << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%";
        REG_INFO(1, logger) << ss.str() << std::endl;
    }

    REG_INFO(1, logger) << "========================================" << std::endl;

    // Show list of failed tests if any
    if (m_tests_failed > 0) {
        REG_ERROR(0, logger) << "\nFailed Tests:" << std::endl;
        for (const auto& test : m_failed_tests) {
            ss.str("");
            ss << "  - " << test;
            REG_ERROR(0, logger) << ss.str() << std::endl;
        }
        ss.str("");
        ss << "\n[OVERALL RESULT: FAILED - " << m_tests_failed << " test(s) failed]";
        REG_ERROR(0, logger) << ss.str() << std::endl;
    } else if (m_tests_passed > 0) {
        REG_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]" << std::endl;
    } else {
        REG_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]" << std::endl;
    }

    REG_INFO(1, logger) << "========================================\n" << std::endl;

    wait(100, SC_NS);
    sc_stop();
}

// =============================================================================
// Main Entry Point
// =============================================================================

int sc_main(int argc, char* argv[])
{
    // Initialize CCI broker and optionally load INI config file.
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

    testbench tb("testbench");

    REG_INFO(1, tb.logger) << "Starting AES Port Binding Testbench" << std::endl;
    sc_start();
    REG_INFO(1, tb.logger) << "Simulation completed" << std::endl;

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);

    return 0;
}
