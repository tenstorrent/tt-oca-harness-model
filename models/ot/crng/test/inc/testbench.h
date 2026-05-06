/******************************************************************************
 * Copyright (c) 2025, Vayavya Labs Pvt. Ltd.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * CRNG Testbench
 * Top-level testbench that instantiates CRNG DUT and test infrastructure
 *******************************************************************************/

#pragma once

#include <systemc.h>
#include <memory>
#include <vector>
#include <string>
#include "crng.h"
#include "crng_test.h"
#include "csml_logger.h"

class testbench : public sc_module
{
public:
    CsmlLogger logger;

    SC_HAS_PROCESS(testbench);

    testbench(sc_module_name name);

    // Test execution methods
    void run_tests();

    // Test helper methods
    void apply_reset();

    // Test result reporting
    void report_test_result(const char* test_name, bool passed);
    void report_test_start(const std::string& test_name);
    void report_test_pass(const std::string& test_name);
    void report_test_fail(const std::string& test_name, const std::string& reason);
    void report_test_summary();

    // Basic register access tests
    void test_register_reset_values();
    void test_register_read_write();
    void test_read_only_registers();

    // Port binding verification tests
    void test_reset_functionality();
    void test_interrupt_connections();
    void test_entropy_interface();
    void test_control_inputs();

    // ==========================================================================
    // CRNG_FUNC_008: Register Callbacks and Access Control (Priority 1)
    // ==========================================================================

    // Tests 001-016: Register Reset Values
    void test_001_intr_state_reset();
    void test_002_intr_enable_reset();
    void test_003_intr_test_reset();
    void test_004_alert_test_reset();
    void test_005_regwen_reset();
    void test_006_ctrl_reset();
    void test_007_cmd_req_reset();
    void test_008_reseed_interval_reset();
    void test_009_reseed_counter_0_reset();
    void test_010_sw_cmd_sts_reset();
    void test_011_genbits_vld_reset();
    void test_012_int_state_read_enable_reset();
    void test_013_hw_exc_sts_reset();
    void test_014_recov_alert_sts_reset();
    void test_015_err_code_reset();
    void test_016_main_sm_state_reset();

    // Test: Reserved Bits Read Zero
    void test_reserved_bits_read_zero();

    // Test: Reserved Bits Write Ignore
    void test_reserved_bits_write_ignore();

    // Test 034: Multi-bit Encoding Validation
    void test_034_ctrl_multibit_encoding();

    // Tests 069-077: REGWEN Lock Mechanism
    void test_069_regwen_lock_basic();
    void test_070_ctrl_regwen_protection();
    void test_071_batch_reset_verification();
    void test_072_fips_force_regwen_protection();
    void test_073_err_code_test_regwen_protection();
    void test_074_regwen_lock_persistence();
    void test_075_multiple_locked_register_writes();
    void test_076_regwen_write_one_when_locked();
    void test_077_regwen_unlock_only_by_reset();

    // Tests 085-093: Read/Write Access Masks
    void test_085_intr_enable_rw_mask();
    void test_086_sw_cmd_sts_readonly();
    void test_087_genbits_readonly();
    void test_088_alert_test_write_only();
    void test_089_reseed_counter_0_readonly();
    void test_090_hw_exc_sts_rw0c();
    void test_091_recov_alert_sts_rw0c();
    void test_092_err_code_readonly();
    void test_093_main_sm_state_readonly();

    // ==========================================================================
    // CRNG_FUNC_014: Alert Test (ALERT_TEST register functionality)
    // ==========================================================================
    void test_alert_test_recov_alert_trigger();
    void test_alert_test_fatal_alert_trigger();

    // Test 109: Comprehensive Access Mask Verification
    void test_109_comprehensive_access_masks();

    // Test 183: Register Lock Comprehensive
    void test_183_register_lock_comprehensive();

    // ==========================================================================
    // CRNG_FUNC_001: DRBG Instance Lifecycle Management (Priority 1)
    // ==========================================================================

    // Tests 020-029: INSTANTIATE Command Tests
    void test_020_instantiate_basic_no_additional_data();
    void test_021_instantiate_with_personalization_data();
    void test_instantiate_deterministic_mode();
    void test_023_instantiate_max_additional_data();
    void test_024_instantiate_reseed_counter_zero();
    void test_025_instantiate_entropy_request_interrupt();
    void test_instantiate_already_instantiated_error();
    void test_instantiate_invalid_flag0_encoding();
    void test_028_instantiate_cmd_rdy_polling();
    void test_029_instantiate_cmd_ack_polling();

    // Tests 030-040: GENERATE Command Tests
    void test_generate_single_block();
    void test_031_generate_multiple_blocks();
    void test_generate_maximum_blocks();
    void test_033_generate_reseed_counter_increment();
    void test_034_generate_genbits_vld_behavior();
    void test_generate_genbits_fips_flag_compliant();
    void test_generate_genbits_fips_flag_deterministic();
    void test_generate_with_additional_input();
    void test_generate_uninstantiated_instance_error();
    void test_generate_reseed_cnt_exceeded_error();
    void test_040_generate_non_blocking_interleaved();

    // Tests 041-047: RESEED Command Tests
    void test_reseed_basic_with_entropy();
    void test_reseed_deterministic_mode();
    void test_reseed_with_additional_input();
    void test_reseed_counter_reset_verification();
    void test_reseed_entropy_request_interrupt();
    void test_reseed_uninstantiated_instance_error();
    void test_reseed_extends_seed_life();

    // Tests 048-052: UPDATE Command Tests
    void test_update_basic_with_additional_data();
    void test__update_reseed_counter_unchanged();
    void test_update_max_additional_data();
    void test_update_uninstantiated_instance_error();
    void test_update_no_entropy_request();

    // Tests 053-057: UNINSTANTIATE Command Tests
    void test_uninstantiate_instantiated_instance();
    void test_uninstantiate_state_cleared();
    void test_uninstantiate_already_uninstantiated();
    void test_uninstantiate_reseed_counter_cleared();
    void test_uninstantiate_hw_exc_sts_cleared();

    // Tests 058-063: Command Sequence Validation Tests
    void test_command_sequence_instantiate_generate_reseed_uninstantiate();
    void test_059_command_sequence_double_instantiate_error();
    void test_060_command_sequence_generate_before_instantiate_error();
    void test_061_command_sequence_reseed_before_instantiate_error();
    void test_062_command_sequence_update_before_instantiate_error();
    void test_command_sequence_recovery_via_uninstantiate();

    // ==========================================================================
    // CRNG_FUNC_002: Pseudorandom Bit Generation (Priority 1)
    // ==========================================================================

    // Tests 068: Invalid Parameter Tests
    void test_invalid_glen_zero();

    // Tests 085-087: Sequential Read Pointer Tests
    void test_genbits_sequential_read_4_words();
    void test_genbits_read_pointer_wrap_after_4th_read();
    void test_genbits_multiple_blocks_pointer_management();

    // Tests 095-100: FIPS Compliance Tests
    void test_095_fips_compliance_entropy_mode();
    void test_096_fips_compliance_deterministic_mode_no_force();
    void test_fips_force_deterministic_with_fips_assertion();
    void test_fips_force_per_instance_instance0();
    void test_fips_force_per_instance_instance1();
    void test_fips_compliance_after_reseed_entropy();

    // Tests 149, 157, 193-194: Corner Cases and Boundary Values
    void test_corner_case_genbits_read_without_generate();
    void test_sw_flow_instantiate_generate_loop();
    void test_193_boundary_cmd_req_glen_min_1();
    void test_194_boundary_cmd_req_glen_max_4095();

    // ==========================================================================
    // CRNG_FUNC_003: Seed Life Management and Reseeding (Priority 2)
    // ==========================================================================

    // Note: Tests 41-52 (RESEED/UPDATE commands) are in crng_func001_drbg_lifecycle.cpp
    // Note: Test 39 (generate_reseed_cnt_exceeded_error) is in crng_func001_drbg_lifecycle.cpp
    // Note: Test 9 (reseed_interval_boundary_values) is in crng_func009_control_configuration.cpp
    // Note: Tests 58, 61-62 (command sequences) are in crng_func001_drbg_lifecycle.cpp
    // Note: Test 126 (edn_reseed_command) implemented in testbench.cpp

    // Tests 101-107: Reseed Interval Enforcement Tests
    void test_101_reseed_interval_enforcement_exact_threshold();
    void test_reseed_interval_enforcement_below_threshold();
    void test_reseed_interval_enforcement_disabled();
    void test_reseed_interval_enforcement_after_reseed_recovery();
    void test_reseed_interval_enforcement_after_instantiate_recovery();
    void test_reseed_interval_enforcement_per_instance_independent();
    void test_reseed_interval_alert_on_exceeded();

    // Tests 146-147: Corner Case Interval Boundary Tests
    void test_146_corner_case_reseed_interval_zero();
    void test_147_corner_case_reseed_interval_one();

    // Test 158: Software Flow Reseed Interval Recovery
    void test_158_sw_flow_reseed_interval_enforcement_recovery();

    // Tests 195-196: Boundary Value Interval Tests
    void test_boundary_reseed_interval_min_0();
    void test_boundary_reseed_interval_max_0xFFFFFFFF();

    // ==========================================================================
    // CRNG_FUNC_009: Control and Configuration (Priority 1)
    // ==========================================================================

    // Tests 002-012: Register Access and Configuration
    void ctrl_enable_field_write_read();
    void ctrl_sw_app_enable_field_write_read();
    void ctrl_read_int_state_field_write_read();
    void ctrl_fips_force_enable_field_write_read();
    void test_ctrl_invalid_encoding_alert();
    void test_regwen_lock_mechanism();
    void test_regwen_write_1_no_effect();
    void test_009_reseed_interval_boundary_values();
    void test_010_fips_force_per_instance_bits();
    void test_011_int_state_read_enable_per_instance();
    void test_int_state_read_enable_regwen_lock();
    void test_int_state_num_invalid_range();

    // Tests 017-019: Module Enable/Disable
    void test_017_module_enable_command_processing();
    void test_module_disable_command_rejection();
    void test_module_enable_after_disable();

    // Tests 069-077: Access Control
    void test_genbits_access_ctrl_sw_app_enable_disabled();
    void test_genbits_access_ctrl_otp_disabled();
    void test_genbits_access_ctrl_both_enabled();
    void test_int_state_val_access_ctrl_read_int_state_disabled();
    void test_074_int_state_val_access_ctrl_instance_disabled();
    void test_077_int_state_val_access_ctrl_all_conditions_met();

    // Tests 097-099: FIPS Compliance (declared in FUNC_002 section above)
    // Duplicate implementations in FUNC_009 renamed to avoid conflicts
    void test_REMOVED_097_fips_force_deterministic_with_fips_assertion();
    void test_REMOVED_098_fips_force_per_instance_instance0();
    void test_REMOVED_099_fips_force_per_instance_instance1();

    // Tests 152-162: Corner Cases and Software Flow Integration
    void test_152_corner_case_all_ctrl_fields_disabled();
    void test_153_corner_case_all_ctrl_fields_enabled();
    void test_corner_case_fips_force_all_instances();
    void test_sw_flow_initialization_sequence();
    void test_162_sw_flow_deterministic_kat_mode();

    // Tests 177-178, 199-200: Reset and Boundary Value Tests
    void test_177_reset_unlocks_regwen();
    void test_reset_disables_module();
    void test_boundary_int_state_num_min_0();
    void test_boundary_fips_force_all_bits_set();
    void test_boundary_fips_force_all_bits_clear();

    // ==========================================================================
    // CRNG_FUNC_005: Command Interface and FSM (Priority 1)
    // ==========================================================================

    // Tests 078-084: Command Interface Protocol Tests (SW_CMD_STS behavior)
    void test_078_cmd_rdy_initial_state_after_enable();
    void test_079_cmd_rdy_clears_during_processing();
    void test_080_cmd_ack_initial_state();
    void test_081_cmd_ack_sets_on_completion();
    void test_082_cmd_ack_clears_on_new_command();
    void test_083_cmd_sts_success_code();
    void test_084_cmd_sts_invalid_acmd_code();

    // Note: Tests 085-087 belong to FUNC_002 (GENBITS tests), see above
    // FUNC_005 implementations should be renumbered
    void test_088_multiple_consecutive_commands_cycling();
    void test_089_cmd_sts_encoding_completeness();
    void test_090_cmd_rdy_ack_across_disable_enable();
    void test_091_sw_cmd_sts_reserved_bits_zero();
    void test_092_sw_cmd_sts_readonly_verification();
    void test_093_cmd_ack_vs_interrupt_equivalence();
    void test_094_back_to_back_command_execution();

    // Note: Tests 095-100 belong to FUNC_002 (FIPS tests), see above
    // FUNC_005 tests renumbered to avoid conflicts

    // Tests 200-208: Renumbered FUNC_005 tests to avoid FUNC_002 conflicts
    void test_200_cmd_sts_invalid_cmd_seq_code();
    void test_201_sw_cmd_sts_field_persistence();
    void test_202_cmd_rdy_blocking_behavior();
    void test_203_cmd_sts_persistence_until_next_command();
    void test_204_command_interface_after_error();
    void test_205_cmd_req_write_only_verification();
    void test_206_main_sm_state_idle_after_reset();
    void test_207_main_sm_state_during_command();
    void test_208_main_sm_state_returns_idle();

    // Tests 209-215: FSM State Transition Tests (MAIN_SM_STATE monitoring)
    // Note: Renumbered to avoid conflict with FUNC_003 tests 101-107
    void test_209_main_sm_state_readonly();
    void test_210_main_sm_state_reserved_bits();
    void test_211_main_sm_state_during_error();
    void test_212_main_sm_state_multiple_sampling();
    void test_213_main_sm_state_stability_idle();
    void test_214_main_sm_state_across_disable_enable();
    void test_215_main_sm_state_all_commands();

    // Tests 216-221: Command Timing and Arbitration Tests
    // Note: Renumbered to avoid conflict with FUNC_003 tests 195-196
    void test_216_command_processing_latency();
    void test_217_cmd_rdy_ack_timing_relationship();
    void test_218_rapid_command_succession_timing();
    void test_219_fsm_state_transition_timing();
    void test_220_command_timeout_detection();
    void test_221_command_interface_synchronization();
    void test_write_only_registers();

    void test_int_state_num_valid_range();
    void test_combined_instantiate_polling_and_verification();
    void test_interrupt_cs_cmd_req_done_assertion();
    void test_interrupt_cs_cmd_req_done_deassertion();
    void test_interrupt_cs_entropy_req_assertion();
    void test_interrupt_cs_entropy_req_deassertion();
    void test_interrupt_cs_hw_inst_exc_assertion();
    void test_interrupt_cs_hw_inst_exc_deassertion();
    void test_interrupt_cs_fatal_err_assertion();
    void test_interrupt_cs_fatal_err_sticky();
    void test_interrupt_enable_gating();
    void test_interrupt_test_mode_cs_cmd_req_done();
    void test_interrupt_test_mode_cs_entropy_req();
    void test_interrupt_test_mode_cs_hw_inst_exc();
    void test_interrupt_test_mode_cs_fatal_err();
    void test_interrupt_multiple_simultaneous_sources();
    void test_interrupt_state_accumulation();
    void test_combined_invalid_acmd_values();
    void test_invalid_clen_greater_than_12();
    void test_reset_clears_all_instance_states();
    void test_reset_during_command_processing();
    void test_error_code_fifo_write_error_injection();
    void test_error_code_fifo_read_error_injection();
    void test_error_code_fsm_illegal_state_main_sm();
    void test_error_code_fsm_illegal_state_cmd_stage();
    void test_error_code_sticky_behavior();
    void test_error_code_multiple_errors();
    void test_recov_alert_sts_clear_mechanism();
    void test_recov_alert_sts_multiple_alerts();
    void test_cmd_req_flag0_invalid_encoding_comprehensive();
    void test_int_state_val_sequential_read_14_words();
    void test_int_state_val_pointer_wrap_after_14th_read();
    void test_genbits_no_repetition_normal_operation();
    void test_invalid_command_field_validation();
    void test_sw_flow_error_recovery_sequence();
    void test_sw_flow_interrupt_driven_operation();
    void test_sw_flow_polling_operation();
    void test_sw_flow_shutdown_sequence();
    void test_sw_flow_internal_state_inspection();
    void test_corner_case_all_ctrl_fields_disabled_and_enabled();
    void test_corner_case_rapid_instantiate_uninstantiate_cycle();
    void test_reset_clears_interrupt_states();
    void test_reset_clears_error_codes();
    void test_fatal_error_recovery_via_reset();
    void test_int_state_val_access_ctrl_otp_disabled();
    void test_int_state_val_read_multiple_instances();
    void test_int_state_val_pointer_reset_on_int_state_num_write();
    void test_int_state_val_reseed_status_fips();

private:
    // DUT and test module instances
    std::unique_ptr<crng_model> m_crng;
    std::unique_ptr<crng_test> m_test;

    // Internal signals for port binding
    sc_signal<bool> clk_signal;
    sc_signal<bool> rst_signal;
    sc_signal<uint8_t> otp_en_signal;
    sc_signal<bool> lc_debug_signal;
    sc_signal<bool> cs_cmd_req_done_signal;
    sc_signal<bool> cs_entropy_req_signal;
    sc_signal<bool> cs_hw_inst_exc_signal;
    sc_signal<bool> cs_fatal_err_signal;

    // Alert output signals (FUNC_ALERT_TEST)
    sc_signal<bool> recov_alert_signal;
    sc_signal<bool> fatal_alert_signal;

    // Test statistics
    uint32_t m_tests_run;
    uint32_t m_tests_passed;
    uint32_t m_tests_failed;
    std::vector<std::string> m_failed_tests;
};
