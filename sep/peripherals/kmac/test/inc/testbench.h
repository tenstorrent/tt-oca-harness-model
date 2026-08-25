// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file testbench.h
 * @brief KMAC SystemC testbench header
 *
 * This file defines the top-level testbench that instantiates the KMAC model
 * and test harness, performs port binding, and executes test cases.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include <systemc.h>
#include "../../include/kmac.h"
#include "kmac_test.h"
#include "csml_logger.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>

/******************************************************************************
 * @class testbench
 * @brief Top-level KMAC testbench
 *
 * Instantiates KMAC model and test harness, binds all port interfaces
 * (TLM sockets, custom interfaces, signals), and manages test execution.
 ******************************************************************************/
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    /**
     * @brief Testbench constructor
     * @param name Module name
     */
    testbench(sc_module_name name);

    /// @brief Destructor
    ~testbench();

    /**
     * @brief Main test execution entry point
     *
     * Executes all test cases. Registered as SC_THREAD in constructor.
     */
    void run_tests();

    /**
     * @brief Run test case by name
     * @param test_name Name of test to execute
     * @return Test pass/fail status
     */
    bool run_test(const std::string& test_name);

    // =========================================================================
    // Test Result Reporting Helpers
    // =========================================================================

    /**
     * @brief Report test start
     * @param test_name Name of the test
     */
    void report_test_start(const std::string& test_name);

    /**
     * @brief Report test pass and update statistics
     * @param test_name Name of the test
     */
    void report_test_pass(const std::string& test_name);

    /**
     * @brief Report test fail and update statistics
     * @param test_name Name of the test
     * @param reason Failure reason
     */
    void report_test_fail(const std::string& test_name, const std::string& reason = "");

    /**
     * @brief Report test result and update statistics
     * @param test_name Name of the test
     * @param passed True if test passed, false otherwise
     */
    void report_test_result(const char* test_name, bool passed);

    /**
     * @brief Print comprehensive test summary with statistics
     */
    void report_test_summary();

private:
    // =========================================================================
    // Component Instances
    // =========================================================================

    /// @brief KMAC model instance
    kmac_ip* dut;

    /// @brief KMAC test harness instance
    kmac_test* test;

    // =========================================================================
    // Interconnect Signals
    // =========================================================================

    /// @brief Idle status signal
    sc_signal<bool> idle_sig;

    /// @brief Interrupt output signal (asserted when enabled interrupt pending)
    sc_signal<bool> intr_sig;

    /// @brief Life cycle escalation signal
    sc_signal<bool> lc_escalate_en_sig;

    /// @brief Active-low reset signal
    sc_signal<bool> rst_ni_sig;

    /// @brief Primary clock signal
    sc_signal<bool> clk_sig;

    // =========================================================================
    // Test Statistics
    // =========================================================================

    /// @brief Number of tests executed
    int m_tests_run;

    /// @brief Number of tests passed
    int m_tests_passed;

    /// @brief Number of tests failed
public:
    int m_tests_failed;

    /// @brief List of failed test names
    std::vector<std::string> m_failed_tests;

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-001
    // =========================================================================
    void test_sha3_224_algorithm_selection();
    void test_sha3_256_algorithm_selection();
    void test_sha3_384_algorithm_selection();
    void test_sha3_512_algorithm_selection();
    void test_sha3_invalid_strength_l128();
    void test_sha3_configuration_validation();
    void test_sha3_reserved_kstrength_error();
    void test_sha3_back_to_back_algorithm_switching();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-002
    // =========================================================================
    void test_shake128_fixed_output();
    void test_shake256_fixed_output();
    void test_shake128_extended_output();
    void test_shake256_extended_output();
    void test_shake_padding_mechanism();
    void test_shake_run_command_squeeze_state();
    void test_shake_invalid_strength_l224();
    void test_shake_invalid_strength_l384();
    void test_shake_invalid_strength_l512();
    void test_corner_extended_output_many_runs();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-003
    // =========================================================================
    void test_cshake128_with_empty_customization();
    void test_cshake256_with_empty_customization();
    void test_cshake128_with_function_name();
    void test_cshake256_with_customization_string();
    void test_cshake_prefix_expansion();
    void test_cshake_padding_mechanism();
    void test_cshake_extended_output();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-004
    // =========================================================================
    void test_kmac_128bit_key_256bit_output();
    void test_kmac_256bit_key_256bit_output();
    void test_kmac_key_length_128bit();
    void test_kmac_key_length_192bit();
    void test_kmac_key_length_256bit();
    void test_kmac_key_length_384bit();
    void test_kmac_key_length_512bit();
    void test_kmac_prefix_validation();
    void test_kmac_incorrect_function_name_error();
    void test_kmac_output_length_encoding();
    void test_kmac_extended_output();
    void test_corner_empty_message_kmac();
    void test_corner_maximum_key_length_512bit();
    void test_corner_minimum_key_length_128bit();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-005
    // =========================================================================
    void test_key_single_share_128bit();
    void test_key_single_share_256bit();
    void test_key_zeroization_on_reset();
    void test_key_zeroization_on_done();
    void test_key_cfg_regwen_protection();
    void test_key_all_lengths_unmasked();
    void test_key_zeroization_on_error();
    void test_security_key_zeroization_on_escalation();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-006
    // =========================================================================
    void test_key_sideload_enable_128bit();
    void test_key_sideload_256bit_automatic_unmasking();
    void test_key_sideload_keylength_override();
    void test_key_sideload_toggle_switch();
    void test_key_sideload_maximum_length_256bit();
    void test_key_sideload_empty_message();
    void test_key_sideload_back_to_back_operations();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-011
    // =========================================================================
    void test_fsm_reset_to_idle();
    void test_fsm_idle_to_absorb_on_start();
    void test_fsm_absorb_to_squeeze_on_process();
    void test_fsm_squeeze_to_idle_on_done();
    void test_fsm_squeeze_persistent_on_run();
    void test_fsm_status_bits_mutually_exclusive();
    void test_fsm_transition_sequence_complete_operation();
    void test_fsm_multiple_operations_back_to_back();
    void test_fsm_idle_state_operations_blocked();
    void test_fsm_absorb_state_operations_permitted();
    void test_fsm_absorb_state_operations_blocked();
    void test_fsm_squeeze_state_operations_permitted();
    void test_fsm_squeeze_state_operations_blocked();
    void test_cmd_sparse_encoding_start_valid();
    void test_cmd_sparse_encoding_process_valid();
    void test_cmd_sparse_encoding_run_valid();
    void test_cmd_sparse_encoding_done_valid();
    void test_cmd_sparse_encoding_invalid();
    void test_cmd_entropy_req_bit();
    void test_cmd_hash_cnt_clr_bit();
    void test_cmd_err_processed_bit();
    void test_cmd_self_clearing_behavior();
    void test_err_code_swcmdsequence_0x08();
    void test_callback_cmd_write_start_side_effects();
    void test_callback_cmd_write_process_side_effects();
    void test_callback_cmd_write_run_side_effects();
    void test_callback_cmd_write_done_side_effects();
    void test_callback_status_read_dynamic();
    void test_corner_back_to_back_operations();
    void test_cfg_regwen_auto_lock_unlock();
    void test_idle_o_signal_updates();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-007
    // =========================================================================
    void test_app_keymgr_kmac_operation();
    void test_app_fixed_priority_arbitration();
    void test_app_keymgr_data_interface();
    void test_app_digest_two_share_output();
    void test_app_sw_lockout_during_app_active();
    void test_app_cmd_rejected_during_app_active();
    void test_app_state_read_blocked_during_app_active();
    void test_app_keymgr_automatic_output_length();
    void test_app_empty_message_not_supported();
    void test_err_code_swissuedcmdinappactive_0x03();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-008
    // =========================================================================
    void test_func_kmac_008_lc_ctrl_operations();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-009 (Application Interface - ROM_CTRL)
    // =========================================================================
    void test_app_rom_ctrl_cshake256_operation();
    void test_app_fixed_priority_arbitration_rom_ctrl();
    void test_app_data_interface_rom_ctrl();
    void test_app_digest_two_share_output_rom_ctrl();
    void test_app_sw_lockout_during_app_active_rom_ctrl();
    void test_app_state_read_blocked_during_app_active_rom_ctrl();
    void test_app_rom_ctrl_empty_message();
    void test_app_rom_ctrl_back_to_back_operations();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-010 (Message FIFO and Packer)
    // =========================================================================
    void test_fifo_depth_tracking();
    void test_fifo_empty_status_on_reset();
    void test_fifo_empty_to_nonempty_transition();
    void test_fifo_nonempty_to_empty_transition();
    void test_fifo_full_condition();
    void test_fifo_full_backpressure_blocking();
    void test_fifo_pass_through_mode();
    void test_fifo_address_window_abstraction();
    void test_fifo_byte_write_support();
    void test_fifo_halfword_write_support();
    void test_fifo_word_write_support();
    void test_fifo_packer_partial_entry_on_process();
    void test_fifo_write_before_start_error();
    void test_fifo_write_after_process_error();
    void test_fifo_write_during_app_active_error();
    void test_callback_msg_fifo_write_packing();
    void test_callback_msg_fifo_write_backpressure();
    void test_fifo_alternating_read_write();
    void test_fifo_maximum_throughput();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-012 (Endianness Configuration)
    // =========================================================================
    void test_sha3_msg_endianness_little();
    void test_sha3_msg_endianness_big();
    void test_sha3_state_endianness_little();
    void test_sha3_state_endianness_big();
    void test_state_endianness_word_granularity();
    void test_state_msg_endianness_independent();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-013 (EDN Mode Entropy Management)
    // =========================================================================
    void test_entropy_mode_edn_request();
    void test_entropy_ready_assertion();
    void test_entropy_mode_lock_after_ready();
    void test_entropy_timeout_edn_mode();
    void test_entropy_timeout_recovery();
    void test_entropy_period_prescaler();
    void test_entropy_period_wait_timer();
    void test_entropy_refresh_hash_cnt();
    void test_entropy_refresh_threshold_trigger();
    void test_entropy_refresh_threshold_zero_disable();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-014 (Software Mode Entropy Management)
    // =========================================================================
    void test_entropy_mode_sw_seed();
    void test_entropy_sw_mode_six_write_sequence();
    void test_entropy_sw_mode_activation_after_sixth_write();
    void test_entropy_sw_mode_post_activation_write_rejection();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-015 (Idle Mode Entropy Management)
    // =========================================================================
    void test_entropy_mode_idle();
    void test_entropy_incorrect_mode_error();
    void test_entropy_hashing_without_ready_error();
    void test_entropy_fast_process_blocking();
    void test_entropy_fast_process_nonblocking();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-017 (Dynamic Register Write Protection)
    // =========================================================================
    void test_cfg_regwen_protection_enable();
    void test_cfg_regwen_protection_disable();
    void test_cfg_regwen_auto_clear_on_start();
    void test_cfg_regwen_auto_set_on_done();
    void test_key_protection_cfg_regwen();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-018 (STATE Window Access Control)
    // =========================================================================
    void test_state_read_in_squeeze_state();
    void test_state_read_in_idle_returns_zero();
    void test_state_read_in_absorb_returns_zero();
    void test_state_two_share_masked();
    void test_state_single_share_unmasked();
    void test_state_share_xor_for_unmasked_digest();
    void test_state_byte_halfword_word_reads();
    void test_callback_state_read_conditional_access();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-020 (Reset and Initialization)
    // =========================================================================
    void test_reset_all_registers_default_values();
    void test_reset_key_shares_cleared();
    void test_reset_fsm_idle_state();
    void test_reset_fifo_empty_status();
    void test_reset_during_absorb_state();
    void test_reset_during_squeeze_state();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-021 (Life Cycle Escalation Response)
    // =========================================================================
    void test_escalation_immediate_key_zeroization();
    void test_escalation_fsm_invalid_state();
    void test_escalation_key_share_zeroed();
    void test_escalation_abort_in_progress_operation();
    void test_escalation_reset_only_recovery();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-022 (Idle Status Signaling)
    // =========================================================================
    void test_idle_o_high_after_reset();
    void test_idle_o_low_during_absorb();
    void test_idle_o_returns_high_on_done();

    // =========================================================================
    // Test Case Methods - FUNC-KMAC-025 (Coverage Gap Tests)
    // =========================================================================
    void test_intr_test_forcing();
    void test_intr_enable_write();
    void test_key_share_write_non_idle();
    void test_key_len_write_non_idle();
    void test_regwen_protected_writes_non_idle();
    void test_entropy_seed_wrong_mode();
    void test_incorrect_entropy_mode_error();
    void test_cshake_invalid_kstrength();
    void test_unknown_mode_error();
    void test_run_not_in_squeeze();
    void test_run_sha3_mode_rejected();
    void test_cfg_shadowed_mismatch_path();
    void test_msg_fifo_write_escalation();
    void test_key_share_write_escalation();
    void test_kmac_large_customization();
    void test_state_partial_read();
    void test_run_kmac_exhaust_output();
    void test_sideload_key_len_clamp();
    void test_defensive_error_paths();

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Bind all ports between model and test harness
     */
    void bind_ports();

    /**
     * @brief Initialize testbench environment
     */
    void initialize();

    /**
     * @brief Apply reset to DUT
     *
     * Asserts active-low reset (rst_ni=0), waits, then deasserts (rst_ni=1).
     * Follows OTBN/HMAC reset pattern for proper initialization.
     */
    void apply_reset();




    /**
     * @brief Ensure FSM is in IDLE state before running tests
     *
     * Checks FSM state and issues DONE command if in SQUEEZE state, or
     * applies reset if in an unrecoverable state. This is essential for
     * test isolation when tests run sequentially.
     *
     * @return true if FSM is now in IDLE, false if recovery failed
     */
    bool ensure_fsm_idle();

    /**
     * @brief Configure CFG_SHADOWED with entropy preservation
     *
     * Helper function that properly configures CFG_SHADOWED register while
     * preserving entropy configuration required when EnMasking=true.
     *
     * Per KMAC spec: When EnMasking=1, all CFG_SHADOWED writes must include
     * entropy_ready=1 and entropy_mode=0x1 to prevent error 0x09
     * (SwHashingWithoutEntropyReady).
     *
     * @param mode Mode value (0x0=SHA3, 0x2=SHAKE, 0x3=cSHAKE/KMAC)
     * @param kstrength Keccak strength (0x0=L128, 0x1=L224, 0x2=L256, 0x3=L384, 0x4=L512)
     * @param kmac_en KMAC enable (0=hash only, 1=MAC mode)
     * @param sideload Sideload key from KeyMgr (0=software key, 1=sideloaded key)
     * @param msg_endianness Message endianness (0=little-endian, 1=big-endian)
     * @param state_endianness State endianness (0=little-endian, 1=big-endian)
     * @param msg_mask Enable message masking (0=disabled, 1=enabled)
     */
    void configure_cfg_shadowed_with_entropy(
        uint32_t mode,
        uint32_t kstrength,
        uint32_t kmac_en = 0,
        uint32_t sideload = 0,
        uint32_t msg_endianness = 0,
        uint32_t state_endianness = 0,
        uint32_t msg_mask = 0);

    /// @brief Logger instance for structured logging
    mutable CsmlLogger logger;
};