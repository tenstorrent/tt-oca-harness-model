// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.h
 * @brief OTBN testbench for comprehensive functional verification
 * 
 * SystemC testbench that instantiates OTBN DUT and executes comprehensive test suite:\n * - Register access tests (reset values, read/write, access control)
 * - State machine tests (IDLE, BUSY_EXECUTE, BUSY_SEC_WIPE, LOCKED)
 * - Memory access tests (IMEM/DMEM windows, protection, access control)
 * - Algorithm execution tests (RSA-2048, Summation)
 * - Error handling tests (recoverable/fatal errors, locked state)
 * - Interface tests (EDN, OTP key rotation, Life Cycle)
 * - Compliance fix validation tests
 * 
 * Contains 40+ individual test cases covering all critical OTBN functionality.
 */

#pragma once
#include <systemc.h>
#include "otbn_test.h"
#include "otbn.h"
#include "otbn_interfaces.h"
#include "reg_logger.h"

// ============================================================================
// OTBN Register Offset Definitions (Common for all tests)
// ============================================================================

/**
 * @brief OTBN register offset namespace
 * 
 * Centralizes all OTBN register and memory offsets for test access.
 * Includes interrupt registers, control/status registers, and memory windows.
 */
namespace otbn_regs {
    // Interrupt Registers
    constexpr uint32_t INTR_STATE_OFFSET        = 0x00;
    constexpr uint32_t INTR_ENABLE_OFFSET       = 0x04;
    constexpr uint32_t INTR_TEST_OFFSET         = 0x08;

    // Control and Status Registers
    constexpr uint32_t ALERT_TEST_OFFSET        = 0x0C;
    constexpr uint32_t CMD_OFFSET               = 0x10;
    constexpr uint32_t CTRL_OFFSET              = 0x14;
    constexpr uint32_t STATUS_OFFSET            = 0x18;
    constexpr uint32_t ERR_BITS_OFFSET          = 0x1C;
    constexpr uint32_t FATAL_ALERT_CAUSE_OFFSET = 0x20;
    constexpr uint32_t INSN_CNT_OFFSET          = 0x24;

    // Checksum Register
    constexpr uint32_t LOAD_CHECKSUM_OFFSET     = 0x28;

    // Memory Windows
    constexpr uint32_t IMEM_OFFSET              = 0x4000;
    constexpr uint32_t DMEM_OFFSET              = 0x8000;

    // Memory Access Spacing
    constexpr uint32_t IMEM_SPACING             = 4;  // 32-bit words
    constexpr uint32_t DMEM_SPACING             = 4;  // 32-bit words
}

/**
 * @brief Main OTBN testbench class
 * 
 * Comprehensive SystemC testbench that:
 * - Instantiates OTBN DUT and test infrastructure
 * - Executes 40+ test cases covering all functionality
 * - Provides helper functions for common operations
 * - Reports test results with pass/fail statistics
 * 
 * Test categories:
 * - Core functionality (execute, secure wipe, state transitions)
 * - Memory access and protection
 * - Error handling and recovery
 * - Algorithm integration (RSA-2048, Summation)
 * - Interface interactions (EDN, OTP, Life Cycle)
 * - Compliance fixes validation
 */
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    /**
     * @brief Constructor
     * @param name SystemC module name
     */
    testbench(sc_module_name name);
    
    /// @brief Destructor
    ~testbench();

    /**
     * @brief Main test execution entry point
     * 
     * Executes all test cases and reports results.
     */
    void run_tests();

private:
    otbn_test *test_model;  ///< Test model for TLM transactions
    otbn_ip *dut;           ///< Device under test (OTBN IP)

    /// RegLogger instance
    RegLogger logger;  ///< Logger for debug and tracing

    // =========================================================================
    // Test Statistics and Reporting
    // =========================================================================
    uint32_t m_tests_run;
    uint32_t m_tests_passed;
public:
    uint32_t m_tests_failed;
    std::vector<std::string> m_failed_test_names;

    void report_test_result(const char* test_name, bool passed);
    void print_test_summary();

    // =========================================================================
    // Common Helper Functions
    // =========================================================================
    void apply_reset();
    void wait_for_idle(const char* context = "operation");
    void wait_for_algorithm_completion(uint32_t timeout_us = 1000);
    void execute_command(uint8_t cmd_code);
    bool verify_status_ready();
    bool verify_status_idle();
    void clear_all_errors();
    void trigger_secure_wipe_dmem();
    void trigger_secure_wipe_imem();
    uint32_t read_status();
    uint32_t read_err_bits();
    uint32_t read_fatal_alert_cause();
    void load_imem_word(uint32_t index, uint32_t value);
    uint32_t read_imem_word(uint32_t index);
    void load_dmem_word(uint32_t index, uint32_t value);
    uint32_t read_dmem_word(uint32_t index);
    void write_dmem_byte(uint32_t byte_offset, uint8_t value);
    uint8_t read_dmem_byte(uint32_t byte_offset);

    // RSA Test Data Helper (follows DMEM layout: 0x000=base, 0x100=exp, 0x200=mod, 0x300=result)
    void load_rsa_test_data();
    void load_invalid_rsa_test_data();  // Loads invalid RSA data (zero modulus) to trigger algorithm error

    // Test case functions
    void test_register_reset_values();
    void test_read_write_register();
    void test_read_only_register();
    void test_imem_access();
    void test_dmem_access();

    // Port binding verification tests
    void test_port_binding();
    void test_interrupt_signals();

    // ==================================================================
    // CRITICAL CORE FUNCTIONAL TESTCASES (15 tests from test plan)
    // ==================================================================

    // Core Functionality Tests
    void test_execute_command_triggers_algorithm();      // Test 7
    void test_secwipe_dmem_command();                    // Test 8
    void test_secwipe_imem_command();                    // Test 9
    void test_state_transitions_on_success();            // Test 16
    void test_state_transitions_on_fatal_error();        // Test 18

    // Memory & Protection Tests
    void test_protected_dmem_region_enforcement();       // Test 21
    void test_memory_access_blocked_during_busy();       // Test 22
    void test_load_checksum_updates();                   // Test 6

    // Error Handling Tests
    void test_state_transitions_on_recoverable_error();  // Test 17
    void test_algorithm_returns_error_status();         // Verify algorithm ERROR status with CTRL.software_errs_fatal=0
    void test_locked_state_terminal();                   // Test 15

    // Algorithm Integration Tests
    void test_algorithm_invocation_via_execute();        // Test 27
    void test_mock_instruction_counter();                // Test 31

    // Interface Interaction Tests
    void test_urnd_prng_seeding_from_edn();
    void test_rnd_register_blocking_edn_requests();
             // Test 38
    void test_dmem_secure_wipe_key_rotation();          // Test 44
    void test_imem_secure_wipe_key_rotation();          // Test 47
    void test_done_interrupt_generation();               // Test 34

    // ==================================================================
    // NEW COMPLIANCE FIX VALIDATION TESTS (9 tests)
    // ==================================================================
    void test_fatal_alert_cause_bit_mapping();           // Validates Fix #1
    void test_fatal_alert_cause_read_callback();         // Validates FATAL_ALERT_CAUSE persistence
    void test_ctrl_register_access_control();            // Validates Fix #2
    void test_internal_secure_wipe_after_execution();    // Validates Fixes #3, #4, #5
    void test_cmd_write_silent_ignore();                 // Validates Fix #6
    void test_cmd_write_callback();                      // Validates CMD write callback state transitions
    void test_insn_cnt_read_callback();                  // Validates Fix #7 (read path)
    void test_insn_cnt_write_callback();                 // Validates Fix #7
    void test_err_bits_clear_in_locked();                // Validates Fix #8
    void test_otp_key_rotation_secure_wipe();            // Validates Fix #11
    void test_life_cycle_escalation();                   // Validates Fix #10
    void test_life_cycle_rma_request();                  // Validates Fix #10

    // ==================================================================
    // HIGH PRIORITY MISSING CORE FUNCTIONALITY TESTS (13 tests)
    // ==================================================================

    // State Machine Comprehensive Tests (3 tests)
    void test_idle_state_operations();                   // Test 12 from plan
    void test_busy_execute_state_restrictions();         // Test 13 from plan (SECURITY CRITICAL)
    void test_busy_secwipe_states();                     // Test 14 from plan

    // Memory Access Comprehensive Tests (4 tests)
    void test_imem_window_access_when_idle();
    void test_imem_window_comprehensive();               // Test 19 from plan
    void test_dmem_window_comprehensive();               // Test 20 from plan
    void test_dmem_window_host_accessible_region();         // Test 20 from plan
    void test_memory_access_returns_zero_locked();       // Test 23 from plan
    void test_memory_access_during_secwipe();            // Test 26 from plan

    // Algorithm Integration Comprehensive Tests (2 tests)
    void test_algorithm_dmem_api_comprehensive();        // Test 28 from plan
    void test_algorithm_success_path_complete();         // Test 29 from plan

    // Test Infrastructure Tests (2 tests)
    void test_write_only_registers();                    // Test 3 from plan
    void test_unrecognized_commands();                   // Test 11 from plan

    // Usage Analysis Recommended Tests (2 tests)
    void test_intr_test_register();                      // From usage analysis
    void test_intr_state_write_callback();               // W1C verification
    void test_err_bits_read_callback();                  // Live error accumulator verification
    void test_err_bits_write_callback();                 // Conditional clearing verification
    void test_status_read_callback();                    // Live state machine state verification
    void test_internal_state_secure_wipe();              // 2-pass URND wipe verification
    void test_alert_test_register();                     // From usage analysis
    void test_fatal_alert_continuous();                  // Continuous fatal alert assertion verification
    void test_alert_test_write_callback();               // ALERT_TEST alert forcing verification
    void test_reset_mechanisms();
    void test_reset_during_busy_state();              // Test reset during active operation
    void test_reserved_register_field();
    void test_command_ignored_when_not_idle();         
    void test_unrecognized_command_codes();
    void test_rapid_command_sequence();               // Rapid consecutive EXECUTE commands handling
    void test_imem_read_callback();                   // IMEM window read callback behavior validation
    void test_dmem_read_callback();                   // DMEM window read callback behavior validation 
    void test_dmem_write_callback();                  // DMEM window write callback behavior validation
    void test_imem_write_callback();                  // IMEM window write callback behavior validation
    void test_imem_dmem_boundary_addresses();         // IMEM/DMEM window boundary address handling (Test 64)
    void test_dmem_protected_region_enforcement();    // DMEM protected 1 KiB region enforcement (algorithm vs host)
    void test_recoverable_alert_pulse();              // Recoverable alert pulse behavior validation
    void test_rsa2048_algorithm_execution_key_enabled();
    void test_secwipe_with_intr_enabled();       // Cover secwipe interrupt-enable paths
    void test_keymgr_key_programming();           // Cover keymgr_b_transport paths
    void load_p256_test_data();                   // Load P256 ECDSA test vectors into DMEM
    void test_p256_ecdsa_execution();             // P256 ECDSA signature verify happy path
    // ==================================================================
    // SUMMATION ALGORITHM TEST
    // ==================================================================
    // Summation Test Data Helper (follows DMEM layout: byte 0=N, bytes 1-N=inputs, byte N+1=result_len, bytes N+2..=result)
    void load_summation_test_data();

    // Summation Algorithm Test (requires algorithm="summation" in constructor)
    void test_summation_algorithm_execution();           // Test 45

    // RSA-2048 Algorithm Execution Test
    void test_rsa2048_algorithm_execution();            // Comprehensive RSA-2048 end-to-end test

    // Coverage tests (exercise uncovered otbn.cpp / algorithm error paths)
    void run_coverage_tests();
    void test_cov_keymgr_tlm_read_error();
    void test_cov_keymgr_tlm_bad_address();
    void test_cov_keymgr_key_invalidate();
    void test_cov_keymgr_wdr_s1_h_write();
    void test_cov_dmem_protected_read();
    void test_cov_lc_escalation_intr_enable();
    void test_cov_lc_rma_intr_enable();
    void test_cov_rsa2048_key_invalid();
    void test_cov_summation_n_zero();
    void test_cov_p256_invalid_signature();
    void test_cov_p256_invalid_pubkey();
    void test_cov_rsa3072_happy_path();
    void test_cov_rsa3072_zero_modulus();
    void test_cov_csr_wdr_callback_execute();
    void test_cov_wdr_key_read_with_key();
    void test_cov_otp_key_rsp_channel();
    void test_cov_algorithm_error_guards();
    void test_cov_otp_scramble_key_channel();
    void test_cov_imem_oob_and_busy_block();
    void test_cov_keymgr_ignore_and_invalid_cmd();
    void test_cov_alert_fatal_and_err_bits_busy();
    void test_cov_software_errs_fatal();
    void test_cov_algorithm_standalone_error_paths();

    // Signals for port binding
    sc_signal<bool> intr_done_sig;
    sc_signal<bool> alert_fatal_sig;
    sc_signal<bool> alert_recov_sig;
    sc_signal<double> clk_core_sig;
    sc_signal<bool> rst_n_sig;
    sc_signal<bool> lc_escalate_rsp_sig;
    sc_signal<bool> lc_rma_rsp_sig;
};