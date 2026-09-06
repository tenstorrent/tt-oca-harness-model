// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.h
 * @brief DMA Controller SystemC/TLM Testbench
 *
 * This file defines the top-level testbench for DMA Controller verification.
 * It instantiates the DMA model and test harness, binds all ports and sockets,
 * and executes test sequences.
 */

#pragma once

#include "secure_dma.h"
#include "secure_dma_test.h"
#include "reg_logger.h"
#include <memory>
#include <vector>
#include <string>


bool compute_sha256_reference(const unsigned char *data, size_t length,
                              unsigned char *digest);
bool compare_digests(const unsigned char *digest1, const unsigned char *digest2,
                     size_t length);
std::string format_digest_hex(const unsigned char *digest, size_t length);

/**
 * @class testbench
 * @brief Top-level testbench for DMA Controller verification
 *
 * This class provides the complete test environment including:
 * - DMA model instantiation
 * - Test harness instantiation
 * - Port and socket binding (register bus, memory buses, interrupts, handshakes)
 * - Test execution framework
 * - Result reporting and statistics
 *
 * The testbench manages all signal connections between model and test harness,
 * executes test sequences, and reports pass/fail results.
 */
class testbench : public sc_module {
public:
  SC_HAS_PROCESS(testbench);

  /**
   * @brief Constructor - instantiates modules and binds all ports
   * @param name SystemC module name
   *
   * Creates DMA model and test harness instances, binds all TLM sockets,
   * signal ports, and registers the test execution thread.
   */
  testbench(sc_module_name name);

  /**
   * @brief Destructor - cleanup
   */
  ~testbench();

  /**
   * @brief Report individual test result
   * @param test_name Name of the test case
   * @param passed True if test passed, false if failed
   *
   * Updates test statistics and logs formatted pass/fail message.
   */
  void report_test_result(const char *test_name, bool passed);

  /**
   * @brief Report individual test result with custom message
   * @param test_name Name of the test case
   * @param passed True if test passed, false if failed
   * @param message Additional diagnostic message
   *
   * Updates test statistics and logs formatted pass/fail message with details.
   */
  void report_test_result(const std::string &test_name, bool passed,
                          const std::string &message);

  /**
   * @brief Print final test summary
   *
   * Displays total tests run, passed, failed, and lists all failed test names.
   */
  void print_test_summary();

private:
  /// RegLogger instance for testbench diagnostics
  RegLogger logger;

  /// DMA model instance (Device Under Test)
  std::unique_ptr<secure_dma_model> m_model;

  /// Test harness instance
  std::unique_ptr<secure_dma_test> m_test;

  /// Test statistics - total tests run
  unsigned int m_tests_run;

public:
  /// Test statistics - tests passed
  unsigned int m_tests_passed;

  /// Test statistics - tests failed
  unsigned int m_tests_failed;

  /// List of failed test names for reporting
  std::vector<std::string> m_failed_tests;

  // =========================================================================
  // Internal Signal Channels
  // =========================================================================

  /// Clock signal connecting test harness to DMA model
  sc_signal<sc_time> clk_signal;

  /// Reset signal connecting test harness to DMA model
  sc_signal<bool> rst_signal;

  /// DMA done interrupt signal
  sc_signal<bool> dma_done_intr_signal;

  /// DMA chunk done interrupt signal
  sc_signal<bool> dma_chunk_done_intr_signal;

  /// DMA error interrupt signal
  sc_signal<bool> dma_error_intr_signal;

  /// Fatal fault alert signal
  sc_signal<bool> alert_fatal_fault_signal;

  /// Hardware handshake trigger signals [10:0]
  sc_signal<bool> lsio_trigger_signal[11];

  /**
   * @brief Main test execution thread
   *
   * Executes all test sequences including:
   * - Reset verification
   * - Register access tests (R/W, RO, WO)
   * - Port binding verification
   * - Interrupt generation tests
   * - Hardware handshake tests
   */
  void run_tests();
  /**
   * @brief Test: Port connectivity
   *
   * Verifies all ports are properly bound and can pass signals/data.
   */
  void test_port_binding();

  /**
   * @brief Test: Interrupt generation
   *
   * Verifies interrupt outputs respond to INTR_STATE and INTR_ENABLE changes.
   */
  void test_interrupts();

  /**
   * @brief Test: Hardware handshake triggers
   *
   * Verifies lsio_trigger inputs are detected when enabled.
   */
  void test_hardware_handshake();

  // =========================================================================
  // FUNC-001: Register Access and Configuration Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-001 test cases
   *
   * Executes comprehensive test suite for Register Access and Configuration
   */
  void run_func001_tests();

  // =========================================================================
  // FUNC-001: Register Access and Configuration Test Cases
  // =========================================================================

  void test_reset_values();
  void test_func001_intr_state_read_only();
  void test_func001_intr_enable_read_write();
  void test_func001_intr_test_write_only();
  void test_func001_alert_test_write_only();
  void test_func001_control_abort_write_only();
  void test_func001_status_rw1c_clear();
  void test_func001_cfg_regwen_read_only();
  void test_range_regwen_write_lock();
  void test_reserved_bits_read_zero();
  void test_reserved_bits_write_ignored();
  void test_cfg_regwen_locked_registers();
  void test_control_status_always_accessible();
  void test_reset_during_idle();
  void test_reset_during_active_transfer();
  void test_reset_unlocks_range_regwen();
  void test_register_rw();
  void test_register_ro();
  void test_register_wo();
  void test_register_rw0c();
  void test_register_rw1c();
  void test_reset_deasserts_interrupts();

  // =========================================================================
  // FUNC-002: Interrupt Generation and Management Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-002 test cases
   *
   * Executes comprehensive test suite for Interrupt Generation and Management
   */
  void run_func002_tests();

  // =========================================================================
  // FUNC-002: Interrupt Generation and Management Test Cases
  // =========================================================================

  // Basic Interrupt Functionality (6 tests)
  void test_func002_reset_interrupt_state();
  void test_func002_intr_state_read_only();
  void test_intr_enable_masking();
  void test_func002_intr_test_forcing();
  void test_func002_status_rwc_clearing();
  void test_func002_interrupt_independence();

  // Transfer Completion Interrupts (4 tests)
  void test_func002_dma_done_interrupt();
  void test_func002_dma_chunk_done_interrupt();
  void test_func002_multiple_chunk_interrupts();
  void test_func002_done_and_chunk_simultaneous();

  // Error Interrupts (3 tests)
  void test_func002_error_interrupt_alignment();
  void test_func002_error_interrupt_invalid_opcode();
  void test_func002_error_interrupt_bus_error();

  // Edge Cases (3 tests)
  void test_func002_interrupt_during_transfer();
  void test_func002_multiple_enable_disable();
  void test_func002_level_sensitive_behavior();
  void test_dma_done_interrupt_clear_rw1c();
  void test_dma_done_auto_clear_on_new_transfer();
  void test_dma_chunk_done_auto_clear();
  void test_dma_error_interrupt_assert();
  void test_dma_error_interrupt_clear();
  void test_dma_chunk_done_interrupt_clear();

  // =========================================================================
  // FUNC-003: Transfer Granularity Control Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-003 test cases
   *
   * Executes comprehensive test suite for Transfer Granularity Control
   */
  void run_func003_tests();

  // =========================================================================
  // FUNC-003: Transfer Granularity Control Test Cases
  // =========================================================================

  // Transfer Width Decoding Tests (4 tests)
  void test_func003_transfer_width_1byte();
  void test_func003_transfer_width_2byte();
  void test_func003_transfer_width_4byte();
  void test_func003_transfer_width_invalid();

  // Alignment Validation Tests (5 tests)
  void test_func003_1byte_no_alignment();
  void test_func003_2byte_aligned();
  void test_error_2byte_misaligned();
  void test_func003_4byte_aligned();
  void test_error_4byte_misaligned();

  // Byte Enable Generation Tests (3 tests)
  void test_func003_byte_enable_1byte_lanes();
  void test_func003_byte_enable_2byte_halfwords();
  void test_func003_byte_enable_4byte_fullword();

  // Sub-word Read Extraction Tests (3 tests)
  void test_func003_extract_byte_lanes();
  void test_func003_extract_halfword_positions();
  void test_func003_extract_fullword();

  // Sub-word Write Replication Tests (3 tests)
  void test_func003_replicate_byte_to_lanes();
  void test_func003_replicate_halfword_positions();
  void test_func003_replicate_fullword();

  // SHA-2 Constraint Tests (3 tests)
  void test_func003_sha2_valid_4byte_width();
  void test_func003_sha2_invalid_1byte_width();
  void test_func003_sha2_invalid_2byte_width();

  // Error Code Validation Tests (3 tests)
  void test_func003_alignment_error_src_addr();
  void test_func003_alignment_error_dst_addr();
  void test_func003_invalid_width_error();

  // =========================================================================
  // FUNC-004: Addressing Mode Management Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-004 test cases
   *
   * Executes comprehensive test suite for Addressing Mode Management
   */
  void run_func004_tests();

  // =========================================================================
  // FUNC-004: Addressing Mode Management Test Cases
  // =========================================================================

  // Addressing Mode Decoding Tests (6 tests)
  void test_func004_src_fixed_mode_decode();
  void test_func004_src_incrementing_mode_decode();
  void test_func004_src_wrapping_mode_decode();
  void test_func004_dst_fixed_mode_decode();
  void test_func004_dst_incrementing_mode_decode();
  void test_func004_dst_wrapping_mode_decode();

  // Fixed Mode Address Tests (2 tests)
  void test_func004_src_fixed_address_unchanged();
  void test_func004_dst_fixed_address_unchanged();

  // Incrementing Mode Tests (6 tests)
  void test_func004_src_increment_1byte_width();
  void test_func004_src_increment_2byte_width();
  void test_func004_src_increment_4byte_width();
  void test_func004_dst_increment_1byte_width();
  void test_func004_dst_increment_2byte_width();
  void test_func004_dst_increment_4byte_width();

  // Wrapping Mode Tests (6 tests)
  void test_func004_src_wrap_single_iteration();
  void test_func004_src_wrap_boundary_crossing();
  void test_func004_src_wrap_multiple_iterations();
  void test_func004_dst_wrap_single_iteration();
  void test_func004_dst_wrap_boundary_crossing();
  void test_func004_dst_wrap_multiple_iterations();

  // Dynamic Register Update Tests (4 tests)
  void test_func004_src_addr_lo_dynamic_update();
  void test_func004_src_addr_hi_overflow_update();
  void test_func004_dst_addr_lo_dynamic_update();
  void test_func004_dst_addr_hi_overflow_update();

  // 64-bit Address Support Tests (4 tests)
  void test_func004_32bit_src_address_ot_bus();
  void test_func004_64bit_src_address_sys_bus();
  void test_func004_32bit_dst_address_ctn_bus();
  void test_func004_64bit_dst_address_sys_bus();

  // Wrap Boundary Validation Tests (4 tests)
  void test_func004_valid_wrap_boundaries();
  void test_func004_invalid_wrap_boundaries();
  void test_func004_chunk_size_alignment();
  void test_func004_boundary_overflow_detection();

  // Integration with FUNC-003 Tests (2 tests)
  void test_func004_transfer_width_affects_increment();
  void test_func004_alignment_maintained_during_advance();

  // =========================================================================
  // FUNC-005: Multi-Bus Interface Transaction Routing Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-005 test cases
   *
   * Executes comprehensive test suite for Multi-Bus Interface Transaction Routing
   */
  void run_func005_tests();

  // =========================================================================
  // FUNC-005: Multi-Bus Interface Transaction Routing Test Cases
  // =========================================================================

  // ASID Decoding Tests (4 tests)
  void test_func005_asid_decode_ot_internal();
  void test_func005_asid_decode_system_bus();
  void test_func005_asid_decode_ctn_bus();
  void test_func005_asid_decode_invalid();

  // ASID Validation Tests (4 tests)
  void test_func005_asid_valid_multibit();
  void test_func005_asid_invalid_zero();
  void test_func005_asid_invalid_reserved();
  void test_func005_asid_invalid_all_ones();

  // Bus Selection Tests (6 tests)
  void test_func005_bus_selection_src_ot();
  void test_func005_bus_selection_dst_ot();
  void test_func005_bus_selection_src_dst_different();
  void test_func005_bus_selection_ot_to_system();
  void test_func005_bus_selection_system_to_ctn();
  void test_func005_bus_selection_same_bus();

  // Address Width Validation Tests (8 tests)
  void test_func005_ot_bus_32bit_src_valid();
  void test_func005_ot_bus_64bit_src_invalid();
  void test_func005_ot_bus_32bit_dst_valid();
  void test_func005_ot_bus_64bit_dst_invalid();
  void test_func005_ctn_bus_32bit_valid();
  void test_func005_system_bus_64bit_src_valid();
  void test_func005_system_bus_64bit_dst_valid();
  void test_func005_address_error_code_validation();

  // TLM Transaction Creation Tests (4 tests)
  void test_func005_tlm_read_transaction();
  void test_func005_tlm_write_transaction();
  void test_func005_tlm_byte_enable_conversion();
  void test_func005_tlm_transaction_parameters();

  // Error Detection Tests (4 tests)
  void test_func005_src_addr_error_detection();
  void test_func005_dst_addr_error_detection();
  void test_func005_asid_error_src_detection();
  void test_func005_asid_error_dst_detection();

  // Integration Tests (4 tests)
  void test_func005_func003_transfer_width_integration();
  void test_func005_func003_byte_enable_integration();
  void test_func005_func004_64bit_src_integration();
  void test_func005_func004_64bit_dst_integration();

  // =========================================================================
  // FUNC-006: Error Detection and Reporting Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-006 test cases
   *
   * Executes comprehensive test suite for Error Detection and Reporting
   */
  void run_func006_tests();

  // =========================================================================
  // FUNC-006: Error Detection and Reporting Test Cases
  // =========================================================================

  // Opcode Validation Tests (5 tests)
  void test_func006_valid_opcode_0x0_copy();
  void test_func006_valid_opcode_0x1_sha256();
  void test_func006_valid_opcode_0x2_sha384();
  void test_func006_valid_opcode_0x3_sha512();
  void test_func006_invalid_opcode_0x4_to_0xF();

  // Transfer Size Validation Tests (4 tests)
  void test_func006_valid_transfer_sizes();
  void test_func006_zero_total_data_size();
  void test_func006_zero_chunk_data_size();
  void test_func006_chunk_greater_than_total();

  // Pre-Transfer Validation Tests (10 tests)
  void test_func006_validation_all_pass();
  void test_func006_validation_opcode_failure();
  void test_func006_validation_transfer_width_failure();
  void test_func006_validation_transfer_size_failure();
  void test_func006_validation_src_alignment_failure();
  void test_func006_validation_dst_alignment_failure();
  void test_func006_validation_src_asid_failure();
  void test_func006_validation_dst_asid_failure();
  void test_func006_validation_src_addr_width_failure();
  void test_func006_validation_dst_addr_width_failure();

  // Bus Error Handling Tests (5 tests)
  void test_func006_bus_error_tlm_ok_response();
  void test_func006_bus_error_address_error();
  void test_func006_bus_error_command_error();
  void test_func006_bus_error_generic_error();
  void test_func006_bus_error_incomplete_response();

  // ERROR_CODE Register Tests (8 tests)
  void test_error_code_bit0_src_addr_error();
  void test_error_code_bit1_dst_addr_error();
  void test_error_code_bit2_opcode_error();
  void test_error_code_bit3_size_error();
  void test_error_code_bit4_bus_error();
  void test_error_code_bit5_base_limit_error();
  void test_error_code_bit6_range_valid_error();
  void test_error_code_bit7_asid_error();

  // STATUS.error Integration Tests (3 tests)
  void test_func006_status_error_on_validation_failure();
  void test_func006_status_error_on_bus_error();
  void test_func006_dma_error_interrupt_triggered();

  // Error Recovery Tests (2 tests)
  void test_error_recovery_sequence();
  void test_func006_multiple_errors_accumulate();
  void test_error_src_addr_upper32_ot_asid();
  void test_error_dst_addr_upper32_ot_asid();
  void test_error_invalid_asid_src();
  void test_error_invalid_asid_dst();
  void test_error_zero_total_data_size();
  void test_error_zero_chunk_data_size();
  void test_error_invalid_transfer_width();

  void test_error_hash_width_mismatch();
  void test_error_base_greater_than_limit();

  // =========================================================================
  // FUNC-007: Security Isolation and Access Control Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-007 test cases
   *
   * Executes comprehensive test suite for Security Isolation and Access Control
   */
  void run_func007_tests();

  // =========================================================================
  // FUNC-007: Security Isolation and Access Control Test Cases
  // =========================================================================

  // Memory Range Configuration Tests (4 tests)
  void test_memory_range_base_limit_config();
  void test_range_valid_bit_requirement();
  void test_func007_base_greater_than_limit_error();
  void test_error_range_not_valid();

  // RANGE_REGWEN Locking Tests (2 tests)
  void test_func007_range_regwen_write_lock();
  void test_range_regwen_lock_prevents_modifications();

  // Three-Tier Memory Model Tests (6 tests)
  void test_ot_private_to_ot_private();
  void test_ot_private_to_ot_dma_enabled();
  void test_ot_dma_enabled_to_ot_private();
  void test_ot_dma_enabled_to_soc();
  void test_soc_to_ot_dma_enabled();
  void test_soc_to_soc();

  // Security Policy Violation Tests (2 tests)
  void test_ot_private_to_soc_blocked();
  void test_soc_to_ot_private_blocked();

  // Address Range Boundary Tests (3 tests)
  void test_memory_range_boundary_base();
  void test_memory_range_boundary_limit();
  void test_func007_address_outside_range_error();

  void test_memory_range_below_base();
  void test_memory_range_above_limit();
  void test_address_overflow_32bit();

  // =========================================================================
  // FUNC-008: Transfer Control and Abort Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-008 test cases
   *
   * Executes comprehensive test suite for Transfer Control and Abort
   */
  void run_func008_tests();

  // =========================================================================
  // FUNC-008: Transfer Control and Abort Test Cases
  // =========================================================================

  // Transfer Control Tests (3 tests)
  void test_func008_go_bit_transfer_initiation();
  void test_func008_go_bit_validation_failure();
  void test_func008_go_bit_auto_clear_on_completion();

  // State Machine and Locking Tests (2 tests)
  void test_func008_cfg_regwen_hardware_locking();
  void test_func008_control_status_always_accessible();

  // Transfer Abort Tests (2 tests)
  void test_abort_during_transfer();
  void test_abort_ot_transactions_complete();
  void test_abort_status_clearing();

  // =========================================================================
  // FUNC-009: DMA Transfer Engine Operation Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-009 test cases
   *
   * Executes comprehensive test suite for DMA Transfer Engine Operation
   */
  void run_func009_tests();

  // =========================================================================
  // FUNC-009: DMA Transfer Engine Operation Test Cases
  // =========================================================================

  // Basic Memory-to-Memory Transfers (7 tests)
  void test_mem_to_mem_single_chunk_4byte();
  void test_mem_to_mem_single_chunk_2byte();
  void test_mem_to_mem_single_chunk_1byte();
  void test_mem_to_mem_multi_chunk();
  void test_mem_to_mem_ctn_32bit();
  void test_func009_mem_to_mem_ctn_64bit();
  void test_func009_mem_to_mem_sys_64bit();

  // Transfer Size Variations (3 tests)
  void test_transfer_size_16bytes();
  void test_transfer_size_1024bytes();
  void test_transfer_size_4096bytes();

  // Addressing Mode Integration (4 tests)
  void test_src_increment_dst_increment();
  void test_src_fixed_dst_increment();
  void test_src_increment_dst_fixed();
  void test_src_fixed_dst_fixed();
  void test_src_wrap_mode();
  void test_dst_wrap_mode();
  void test_both_wrap_mode();

  // Completion Detection (2 tests)
  void test_dma_done_interrupt_assert();
  void test_dma_chunk_done_interrupt_assert();

  // Corner Cases (5 tests)
  void test_chunk_size_not_divisor_of_total();
  void test_minimum_transfer_size_1byte();
  void test_func009_minimum_transfer_size_4bytes();
  void test_maximum_transfer_size_4gb();
  void test_abort_during_multi_chunk();
  void test_address_alignment_byte_boundary();
  void test_address_alignment_halfword_boundary();
  void test_address_alignment_word_boundary();
  void test_wrap_mode_chunk_boundary();
  void test_64bit_address_full_range();
  void test_32bit_address_max_value();
  void test_sub_word_extract_1byte_lane0();
  void test_sub_word_extract_1byte_lane3();
  void test_sub_word_extract_2byte_lane0();
  void test_sub_word_extract_2byte_lane2();


  // =========================================================================
  // FUNC-010: Inline SHA-2 Hash Computation Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-010 test cases
   *
   * Executes comprehensive test suite for Inline SHA-2 Hash Computation
   */
  void run_func010_tests();

  // =========================================================================
  // FUNC-010: Inline SHA-2 Hash Computation Test Cases
  // =========================================================================

  // Algorithm Verification (3 tests)
  void test_sha256_single_chunk();
  void test_sha384_single_chunk();
  void test_sha512_single_chunk();

  // Multi-Chunk Accumulation (3 tests)
  void test_sha256_multi_chunk();
  void test_sha384_multi_chunk();
  void test_sha512_multi_chunk();

  // Digest Byte-Swap Control (3 tests)
  void test_digest_swap_endianness();
  void test_func010_digest_swap_sha384();
  void test_func010_digest_swap_sha512();

  // Hash State Management (3 tests)
  void test_initial_transfer_bit_hash_reset();
  void test_func010_hash_state_continuation();
  void test_func010_multiple_independent_hashes();

  // Integration with Transfer Engine (4 tests)
  void test_func010_hash_with_mem_to_mem();
  void test_func010_hash_with_addressing_modes();
  void test_func010_hash_with_different_bus_interfaces();
  void test_func010_hash_with_maximum_transfer_size();

  // Error and Abort Handling (3 tests)
  void test_func010_error_hash_width_mismatch();
  void test_abort_during_hashing();
  void test_func010_reset_clears_digest_valid();

  // Register Interface (3 tests)
  void test_sha2_digest_valid_bit();
  void test_func010_digest_register_reads();
  void test_func010_digest_persistence();

  // =========================================================================
  // FUNC-011: Hardware Handshaking Mechanism Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-011 test cases
   *
   * Executes comprehensive test suite for Hardware Handshaking Mechanism
   */
  void run_func011_tests();

  // =========================================================================
  // FUNC-011: Hardware Handshaking Mechanism Test Cases
  // =========================================================================

  // Individual Trigger Input Tests (11 tests)
  void test_hw_handshake_trigger0();
  void test_hw_handshake_trigger1();
  void test_hw_handshake_trigger2();
  void test_hw_handshake_trigger3();
  void test_hw_handshake_trigger4();
  void test_hw_handshake_trigger5();
  void test_hw_handshake_trigger6();
  void test_hw_handshake_trigger7();
  void test_hw_handshake_trigger8();
  void test_hw_handshake_trigger9();
  void test_hw_handshake_trigger10();

  // Automatic Interrupt Clearing Tests (2 tests)
  void test_hw_handshake_auto_clear_ot_bus();
  void test_hw_handshake_auto_clear_ctn_bus();

  // Operational Behavior Tests (3 tests)
  void test_hw_handshake_go_bit_remains_set();
  void test_hw_handshake_no_chunk_done_intr();
  void test_hw_handshake_total_size_reached();

  // RX-drain handshake behavior (peripheral-to-memory streaming)
  void test_hw_handshake_no_drain_before_trigger();
  void test_hw_handshake_multichunk_reference_74();

  // Helper function for generic trigger tests
  bool run_generic_hw_handshake_test(
      uint32_t trigger_index,
      uint32_t src_addr,
      uint32_t dst_addr,
      uint32_t total_size,
      uint32_t chunk_size);

  // =========================================================================
  // FUNC-012: Hardware Trigger Control Disabled Test Orchestration
  // =========================================================================

  /**
   * @brief Run all FUNC-012 test cases
   *
   * Executes test for hardware trigger when handshake mode is disabled
   */
  void run_func012_tests();

  // =========================================================================
  // FUNC-012: Hardware Trigger Control Disabled Test Cases
  // =========================================================================

  void test_hw_trigger_ctrl_off();
  void test_abort_terminates_transfer_loop();
  void test_hw_handshake_trigger_ignored_when_go_not_set();
  void test_hash_initial_transfer_zero_no_context();
  void test_hw_handshake_auto_clear_bus_error_halts();
  void test_sha2_requires_four_byte_width_size_error();
  void test_chunk_size_exceeds_total_size_warning();
  void test_hash_init_frees_previous_context_after_failed_transfer();

  // Coverage tests for helper/error paths not hit by FUNC-* suites
  void run_coverage_tests();
  void test_cov_helper_error_guards();
  void test_cov_hash_reset_and_inactive();
  void test_cov_handshake_trigger_already_high();
  void test_cov_invalid_asid_transaction();
  void test_cov_bus_name_and_hash_helpers();
  void test_cov_handshake_already_high_on_arm();
  void test_cov_hash_reset_frees_context();
};
