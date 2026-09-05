// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_dma_func_002.cpp
 * @brief FUNC-002: Interrupt Generation and Management test implementation
 *
 * This file implements comprehensive test cases for DMA Controller interrupt functionality:
 * - Basic interrupt functionality (enable, mask, test, clear)
 * - Transfer completion interrupts (done, chunk_done)
 * - Error interrupts (alignment, opcode, bus error)
 * - Edge cases (during transfer, level-sensitive behavior)
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// FUNC-002 Test Orchestration
// =============================================================================

/**
 * @brief Execute all FUNC-002 test cases
 *
 * Runs comprehensive interrupt generation and management tests covering:
 * - Basic interrupt operations (reset, read-only, masking, forcing, clearing)
 * - Transfer completion interrupts
 * - Error condition interrupts
 * - Edge cases and level-sensitive behavior
 */
void testbench::run_func002_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-002: Interrupt Generation and Management Tests\n"
                       << "========================================\n"
                       << std::endl;

  wait(20, SC_NS);

  // Basic Interrupt Functionality (6 tests)
  // test_func002_reset_interrupt_state();
  // test_func002_intr_state_read_only();
  test_intr_enable_masking();
  // test_func002_intr_test_forcing();
  // test_func002_status_rwc_clearing();
  // test_func002_interrupt_independence();

  // Transfer Completion Interrupts (4 tests)
  // test_func002_dma_done_interrupt();
  // test_func002_dma_chunk_done_interrupt();
  // test_func002_multiple_chunk_interrupts();
  // test_func002_done_and_chunk_simultaneous();

  // Error Interrupts (3 tests)
  // test_func002_error_interrupt_alignment();
  // test_func002_error_interrupt_invalid_opcode();
  // test_func002_error_interrupt_bus_error();

  // Edge Cases (3 tests)
  // test_func002_interrupt_during_transfer();
  // test_func002_multiple_enable_disable();
  // test_func002_level_sensitive_behavior();

  test_dma_done_interrupt_clear_rw1c();
  test_dma_done_auto_clear_on_new_transfer();
  test_dma_chunk_done_auto_clear();
  test_dma_error_interrupt_assert();
  test_dma_error_interrupt_clear();
  test_dma_chunk_done_interrupt_clear();

  REG_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-002 Test Suite Complete\n"
                       << "========================================\n"
                       << std::endl;
}

// =============================================================================
// FUNC-002 TC001: Reset Interrupt State
// =============================================================================

/**
 * @brief Verify all interrupts deasserted after reset
 *
 * Test Objective:
 * - Confirm INTR_STATE register reads 0x00000000 after reset
 * - Confirm all interrupt output signals are low
 *
 * Pass Criteria:
 * - INTR_STATE == 0x00000000
 * - All interrupt output ports read false
 */
void testbench::test_func002_reset_interrupt_state() {
  std::string test_name = "FUNC-002 TC001: Reset Interrupt State";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Read INTR_STATE - should be 0x00000000
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

  if (intr_state != 0x00000000) {
    passed = false;
    msg << "INTR_STATE after reset: 0x" << std::hex << intr_state
        << " (expected 0x00000000); ";
  }

  // Check interrupt output signals via testbench signal monitoring
  bool dma_done = dma_done_intr_signal.read();
  bool dma_chunk_done = dma_chunk_done_intr_signal.read();
  bool dma_error = dma_error_intr_signal.read();

  if (dma_done || dma_chunk_done || dma_error) {
    passed = false;
    msg << "Interrupt outputs not deasserted (done=" << dma_done
        << ", chunk_done=" << dma_chunk_done
        << ", error=" << dma_error << "); ";
  }

  if (passed) {
    msg << "All interrupts deasserted after reset (INTR_STATE=0x0, outputs=0)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC002: INTR_STATE Read-Only
// =============================================================================

/**
 * @brief Verify INTR_STATE is read-only (writes ignored)
 *
 * Test Objective:
 * - Confirm direct writes to INTR_STATE register are ignored
 * - Only hardware and INTR_TEST can modify INTR_STATE
 *
 * Pass Criteria:
 * - INTR_STATE value unchanged after write attempt
 */
void testbench::test_func002_intr_state_read_only() {
  std::string test_name = "FUNC-002 TC002: INTR_STATE Read-Only";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset to ensure clean state
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Read initial INTR_STATE (should be 0x0)
  uint32_t initial_value = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, initial_value);

  // Attempt direct write to INTR_STATE
  m_test->register_write_32(secure_dma_basetest::INTR_STATE_OFFSET, 0xFFFFFFFF);
  wait(20, SC_NS);

  // Read back INTR_STATE - should be unchanged
  uint32_t post_write_value = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, post_write_value);

  if (initial_value != post_write_value) {
    passed = false;
    msg << "INTR_STATE changed after write (initial=0x" << std::hex << initial_value
        << ", post-write=0x" << post_write_value << ") - should be read-only";
  } else {
    msg << "INTR_STATE correctly ignored direct write attempts (read-only behavior verified)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC003: INTR_ENABLE Masking
// =============================================================================

/**
 * @brief Verify INTR_ENABLE masks interrupt outputs
 *
 * Test Objective:
 * - Confirm interrupt outputs are gated by INTR_ENABLE bits
 * - Verify formula: interrupt_output = INTR_STATE[bit] AND INTR_ENABLE[bit]
 *
 * Pass Criteria:
 * - Interrupts masked when INTR_ENABLE bit = 0
 * - Interrupts unmasked when INTR_ENABLE bit = 1
 */
void testbench::test_intr_enable_masking() {
  std::string test_name = "INTR_ENABLE Masking";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  uint32_t intr_state = 0;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Force all INTR_STATE bits (done/chunk/error) first.
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000007);
  wait(5, SC_NS);

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x7) != 0x7) {
    passed = false;
    msg << "INTR_STATE not fully set by INTR_TEST (0x" << std::hex << intr_state << "); ";
  }

  // 1) Mask all outputs
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
  wait(5, SC_NS);

  if (dma_done_intr_signal.read() || dma_chunk_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Interrupt outputs not masked when INTR_ENABLE=0; ";
  }

  // 2) Unmask all outputs
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  wait(5, SC_NS);

  if (!dma_done_intr_signal.read() || !dma_chunk_done_intr_signal.read() || !dma_error_intr_signal.read()) {
    passed = false;
    msg << "Interrupt outputs not unmasked when INTR_ENABLE=0x7; ";
  }

  // 3) Per-bit masking checks
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001); // done only
  wait(5, SC_NS);
  if (!dma_done_intr_signal.read() || dma_chunk_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Per-bit mask failed for done-only enable; ";
  }

  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002); // chunk only
  wait(5, SC_NS);
  if (dma_done_intr_signal.read() || !dma_chunk_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Per-bit mask failed for chunk-only enable; ";
  }

  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004); // error only
  wait(5, SC_NS);
  if (dma_done_intr_signal.read() || dma_chunk_done_intr_signal.read() || !dma_error_intr_signal.read()) {
    passed = false;
    msg << "Per-bit mask failed for error-only enable; ";
  }

  if (passed) {
    msg << "INTR_ENABLE correctly masks/unmasks dma_done, dma_chunk_done, dma_error outputs while INTR_STATE stays set.";
  }

  report_test_result(test_name, passed, msg.str());
}


// =============================================================================
// FUNC-002 TC004: INTR_TEST Forcing
// =============================================================================

/**
 * @brief Verify INTR_TEST forces interrupt assertion
 *
 * Test Objective:
 * - Confirm INTR_TEST can force each interrupt bit in INTR_STATE
 * - Verify forced interrupts generate output when enabled
 *
 * Pass Criteria:
 * - Each INTR_TEST bit sets corresponding INTR_STATE bit
 * - Interrupt outputs assert when enabled
 */
void testbench::test_func002_intr_test_forcing() {
  std::string test_name = "FUNC-002 TC004: INTR_TEST Forcing";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable all interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Test forcing dma_done (bit 0)
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0) {
    passed = false;
    msg << "INTR_TEST bit[0] did not set INTR_STATE.dma_done; ";
  }

  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr not asserted after INTR_TEST; ";
  }

  // Clear via STATUS write
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Test forcing dma_chunk_done (bit 1)
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) == 0) {
    passed = false;
    msg << "INTR_TEST bit[1] did not set INTR_STATE.dma_chunk_done; ";
  }

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr not asserted after INTR_TEST; ";
  }

  // Clear and test forcing dma_error (bit 2)
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000020);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x4) == 0) {
    passed = false;
    msg << "INTR_TEST bit[2] did not set INTR_STATE.dma_error; ";
  }

  if (!dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr not asserted after INTR_TEST; ";
  }

  if (passed) {
    msg << "INTR_TEST correctly forces all three interrupt types (done, chunk_done, error)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC005: STATUS RW1C Clearing
// =============================================================================

/**
 * @brief Verify STATUS register RW1C clears interrupts
 *
 * Test Objective:
 * - Confirm writing 1 to STATUS bits clears corresponding INTR_STATE bits
 * - Verify interrupt outputs deassert when INTR_STATE cleared
 *
 * Pass Criteria:
 * - STATUS write-1-to-clear clears INTR_STATE
 * - Interrupt outputs deassert after clearing
 */
void testbench::test_func002_status_rwc_clearing() {
  std::string test_name = "FUNC-002 TC005: STATUS RW1C Clearing";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable all interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Force all interrupts via INTR_TEST
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000007);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify all interrupt states are set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x7) != 0x7) {
    passed = false;
    msg << "INTR_STATE not fully set (0x" << std::hex << intr_state << "); ";
  }

  // Verify all interrupt outputs are asserted
  if (!dma_done_intr_signal.read() || !dma_chunk_done_intr_signal.read() ||
      !dma_error_intr_signal.read()) {
    passed = false;
    msg << "Not all interrupt outputs asserted before clear; ";
  }

  // Clear dma_done via STATUS.done (bit 1) write-1-to-clear
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_done not cleared by STATUS.done RW1C; ";
  }

  if (dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr not deasserted after clear; ";
  }

  // Clear dma_chunk_done via STATUS.chunk_done (bit 5)
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000020);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_chunk_done not cleared by STATUS.chunk_done RW1C; ";
  }

  if (dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr not deasserted after clear; ";
  }

  // Clear dma_error via STATUS.error (bit 3)
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000008);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x4) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_error not cleared by STATUS.error RW1C; ";
  }

  if (dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr not deasserted after clear; ";
  }

  if (passed) {
    msg << "STATUS RW1C correctly clears INTR_STATE and deasserts interrupt outputs";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC006: Interrupt Independence
// =============================================================================

/**
 * @brief Verify each interrupt operates independently
 *
 * Test Objective:
 * - Confirm setting/clearing one interrupt does not affect others
 * - Verify independent enable/disable control
 *
 * Pass Criteria:
 * - Each interrupt can be set/cleared independently
 * - Enable mask operates per-bit independently
 */
void testbench::test_func002_interrupt_independence() {
  std::string test_name = "FUNC-002 TC006: Interrupt Independence";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable only dma_done (bit 0)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Force all three interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000007);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify only dma_done output is asserted (others masked)
  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr not asserted when enabled; ";
  }

  if (dma_chunk_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Other interrupts asserted when disabled; ";
  }

  // Verify all three INTR_STATE bits are set (independent of enable)
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x7) != 0x7) {
    passed = false;
    msg << "INTR_STATE not fully set (should be independent of enable); ";
  }

  // Clear only dma_done
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify only dma_done cleared
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_done not cleared; ";
  }

  if ((intr_state & 0x6) != 0x6) {
    passed = false;
    msg << "Other INTR_STATE bits incorrectly affected by dma_done clear; ";
  }

  // Now enable dma_chunk_done only
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify only chunk_done output asserted
  if (dma_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Wrong interrupts asserted with chunk_done-only enable; ";
  }

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr not asserted when enabled; ";
  }

  if (passed) {
    msg << "All three interrupts operate independently (set, clear, enable)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC007: DMA Done Interrupt
// =============================================================================

/**
 * @brief Verify dma_done asserts when transfer completes
 *
 * Test Objective:
 * - Confirm dma_done interrupt asserts when total_size bytes transferred
 * - Verify INTR_STATE.dma_done and output signal both set
 *
 * Pass Criteria:
 * - INTR_STATE.dma_done set when transfer complete
 * - dma_done_intr output asserted (if enabled)
 * - STATUS.done also set
 *
 * NOTE: Full transfer engine not yet implemented (FUNC-008+), so this test
 * uses INTR_TEST to simulate the hardware behavior. Full end-to-end testing
 * will be performed when transfer engine is complete.
 */
void testbench::test_func002_dma_done_interrupt() {
  std::string test_name = "FUNC-002 TC007: DMA Done Interrupt";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable dma_done interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate transfer completion by forcing dma_done via INTR_TEST
  // (Transfer engine will set this automatically when FUNC-008+ is implemented)
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify INTR_STATE.dma_done is set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0) {
    passed = false;
    msg << "INTR_STATE.dma_done not set on transfer completion; ";
  }

  // Verify interrupt output is asserted
  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr output not asserted; ";
  }

  // Verify other interrupts not affected
  if (dma_chunk_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Other interrupts incorrectly asserted; ";
  }

  if (passed) {
    msg << "dma_done interrupt correctly asserts on transfer completion (simulated via INTR_TEST)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC008: DMA Chunk Done Interrupt
// =============================================================================

/**
 * @brief Verify dma_chunk_done asserts after chunk transfer
 *
 * Test Objective:
 * - Confirm dma_chunk_done interrupt asserts when chunk_size bytes transferred
 * - Verify STATUS.chunk_done also set
 *
 * Pass Criteria:
 * - INTR_STATE.dma_chunk_done set after chunk completion
 * - dma_chunk_done_intr output asserted (if enabled)
 *
 * NOTE: Using INTR_TEST simulation until transfer engine implemented
 */
void testbench::test_func002_dma_chunk_done_interrupt() {
  std::string test_name = "FUNC-002 TC008: DMA Chunk Done Interrupt";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable dma_chunk_done interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate chunk completion by forcing dma_chunk_done via INTR_TEST
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify INTR_STATE.dma_chunk_done is set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) == 0) {
    passed = false;
    msg << "INTR_STATE.dma_chunk_done not set on chunk completion; ";
  }

  // Verify interrupt output is asserted
  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr output not asserted; ";
  }

  // Verify other interrupts not affected
  if (dma_done_intr_signal.read() || dma_error_intr_signal.read()) {
    passed = false;
    msg << "Other interrupts incorrectly asserted; ";
  }

  if (passed) {
    msg << "dma_chunk_done interrupt correctly asserts after chunk completion (simulated via INTR_TEST)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC009: Multiple Chunk Interrupts
// =============================================================================

/**
 * @brief Verify chunk interrupt for multi-chunk transfer
 *
 * Test Objective:
 * - Confirm dma_chunk_done can assert multiple times in one transfer
 * - Verify software can clear and re-trigger chunk interrupt
 *
 * Pass Criteria:
 * - dma_chunk_done can be cleared and reasserted multiple times
 * - Each chunk generates independent interrupt event
 */
void testbench::test_func002_multiple_chunk_interrupts() {
  std::string test_name = "FUNC-002 TC009: Multiple Chunk Interrupts";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable dma_chunk_done interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate first chunk completion
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "First chunk interrupt not asserted; ";
  }

  // Clear chunk_done interrupt
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000020);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Chunk interrupt not cleared after STATUS write; ";
  }

  // Simulate second chunk completion
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Second chunk interrupt not asserted; ";
  }

  // Clear again
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000020);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate third chunk completion
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Third chunk interrupt not asserted; ";
  }

  if (passed) {
    msg << "dma_chunk_done interrupt correctly asserts multiple times (3 chunks simulated)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC010: Done and Chunk Simultaneous
// =============================================================================

/**
 * @brief Verify simultaneous done+chunk on last chunk
 *
 * Test Objective:
 * - Confirm both dma_done and dma_chunk_done can assert simultaneously
 * - Verify this occurs on last chunk of multi-chunk transfer
 *
 * Pass Criteria:
 * - Both INTR_STATE bits can be set at same time
 * - Both interrupt outputs assert when both enabled
 */
void testbench::test_func002_done_and_chunk_simultaneous() {
  std::string test_name = "FUNC-002 TC010: Done and Chunk Simultaneous";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable both done and chunk_done interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000003);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate last chunk completion (both done and chunk_done)
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000003);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify both INTR_STATE bits are set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x3) != 0x3) {
    passed = false;
    msg << "Both INTR_STATE bits not set (got 0x" << std::hex << intr_state << "); ";
  }

  // Verify both interrupt outputs are asserted
  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr not asserted; ";
  }

  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr not asserted; ";
  }

  // Verify error interrupt not affected
  if (dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr incorrectly asserted; ";
  }

  if (passed) {
    msg << "Both dma_done and dma_chunk_done correctly assert simultaneously on last chunk";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC011: Error Interrupt - Alignment
// =============================================================================

/**
 * @brief Verify error interrupt on alignment error
 *
 * Test Objective:
 * - Confirm dma_error asserts on misaligned address/size
 * - Verify STATUS.error and ERROR_CODE also set
 *
 * Pass Criteria:
 * - INTR_STATE.dma_error set on alignment error
 * - dma_error_intr output asserted
 *
 * NOTE: Using INTR_TEST simulation until error detection implemented
 */
void testbench::test_func002_error_interrupt_alignment() {
  std::string test_name = "FUNC-002 TC011: Error Interrupt - Alignment";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable error interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate alignment error by forcing dma_error via INTR_TEST
  // (Transfer engine will set this when it detects alignment errors)
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify INTR_STATE.dma_error is set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x4) == 0) {
    passed = false;
    msg << "INTR_STATE.dma_error not set on alignment error; ";
  }

  // Verify interrupt output is asserted
  if (!dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr output not asserted; ";
  }

  // Verify other interrupts not affected
  if (dma_done_intr_signal.read() || dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Other interrupts incorrectly asserted; ";
  }

  if (passed) {
    msg << "dma_error interrupt correctly asserts on alignment error (simulated via INTR_TEST)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC012: Error Interrupt - Invalid Opcode
// =============================================================================

/**
 * @brief Verify error interrupt on invalid opcode
 *
 * Test Objective:
 * - Confirm dma_error asserts on invalid CONTROL.opcode value
 * - Verify error is immediate (before transfer starts)
 *
 * Pass Criteria:
 * - INTR_STATE.dma_error set on invalid opcode
 * - dma_error_intr output asserted
 *
 * NOTE: Using INTR_TEST simulation until opcode validation implemented
 */
void testbench::test_func002_error_interrupt_invalid_opcode() {
  std::string test_name = "FUNC-002 TC012: Error Interrupt - Invalid Opcode";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable error interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate invalid opcode error
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify error interrupt is set and asserted
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x4) == 0) {
    passed = false;
    msg << "INTR_STATE.dma_error not set on invalid opcode; ";
  }

  if (!dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr output not asserted; ";
  }

  if (passed) {
    msg << "dma_error interrupt correctly asserts on invalid opcode (simulated via INTR_TEST)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC013: Error Interrupt - Bus Error
// =============================================================================

/**
 * @brief Verify error interrupt on bus error response
 *
 * Test Objective:
 * - Confirm dma_error asserts when TLM transaction returns error response
 * - Verify transfer halts on bus error
 *
 * Pass Criteria:
 * - INTR_STATE.dma_error set on bus error
 * - dma_error_intr output asserted
 *
 * NOTE: Using INTR_TEST simulation until bus error handling implemented
 */
void testbench::test_func002_error_interrupt_bus_error() {
  std::string test_name = "FUNC-002 TC013: Error Interrupt - Bus Error";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable error interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate bus error
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000004);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify error interrupt is set and asserted
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x4) == 0) {
    passed = false;
    msg << "INTR_STATE.dma_error not set on bus error; ";
  }

  if (!dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr output not asserted; ";
  }

  if (passed) {
    msg << "dma_error interrupt correctly asserts on bus error (simulated via INTR_TEST)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC014: Interrupt During Transfer
// =============================================================================

/**
 * @brief Verify interrupt behavior during active transfer
 *
 * Test Objective:
 * - Confirm interrupts can be enabled/disabled during active DMA transfer
 * - Verify INTR_STATE persists even when interrupts disabled
 *
 * Pass Criteria:
 * - INTR_ENABLE can be modified during transfer
 * - Interrupt outputs respond immediately to enable changes
 * - INTR_STATE remains set regardless of enable state
 */
void testbench::test_func002_interrupt_during_transfer() {
  std::string test_name = "FUNC-002 TC014: Interrupt During Transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure a transfer (minimal setup)
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x00000100);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x00000100);
  wait(20, SC_NS);

  // Start transfer (DMA becomes busy)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000);
  wait(20, SC_NS);

  // Verify DMA is busy
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x0) {
    passed = false;
    msg << "DMA not busy after go command; ";
  }

  // Disable all interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Simulate chunk completion during transfer
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify INTR_STATE is set even with interrupts disabled
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) == 0) {
    passed = false;
    msg << "INTR_STATE not set during transfer (should be independent of enable); ";
  }

  // Verify interrupt output is masked
  if (dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt output asserted when disabled; ";
  }

  // Enable interrupt during transfer
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify interrupt output now asserted
  if (!dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt output not asserted after enable during transfer; ";
  }

  if (passed) {
    msg << "Interrupts correctly manageable during active transfer (enable/disable, state persistence)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC015: Multiple Enable/Disable
// =============================================================================

/**
 * @brief Verify rapid enable/disable cycles
 *
 * Test Objective:
 * - Confirm INTR_ENABLE can be modified rapidly without corruption
 * - Verify interrupt outputs respond correctly to rapid changes
 *
 * Pass Criteria:
 * - Multiple enable/disable cycles work correctly
 * - No race conditions or glitches in interrupt outputs
 */
void testbench::test_func002_multiple_enable_disable() {
  std::string test_name = "FUNC-002 TC015: Multiple Enable/Disable";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Set an interrupt state to test with
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify INTR_STATE is set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0) {
    passed = false;
    msg << "INTR_STATE not set for test; ";
  }

  // Rapid enable/disable cycles
  for (int i = 0; i < 5; i++) {
    // Enable
    m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
    wait(5, SC_NS);  // Wait for interrupt signal propagation

    if (!dma_done_intr_signal.read()) {
      passed = false;
      msg << "Interrupt not asserted in cycle " << i << "; ";
    }

    // Disable
    m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
    wait(5, SC_NS);  // Wait for interrupt signal propagation

    if (dma_done_intr_signal.read()) {
      passed = false;
      msg << "Interrupt not masked in cycle " << i << "; ";
    }
  }

  // Verify INTR_STATE still set after all cycles
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0) {
    passed = false;
    msg << "INTR_STATE corrupted after rapid enable/disable cycles; ";
  }

  if (passed) {
    msg << "Rapid enable/disable cycles work correctly (5 cycles tested, no corruption)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-002 TC016: Level-Sensitive Behavior
// =============================================================================

/**
 * @brief Verify level-sensitive (not edge) behavior
 *
 * Test Objective:
 * - Confirm interrupts are level-sensitive, not edge-triggered
 * - Verify interrupt remains asserted until explicitly cleared
 * - Verify re-enabling after disable reasserts interrupt if state still set
 *
 * Pass Criteria:
 * - Interrupt output remains high while INTR_STATE set and enabled
 * - Disabling then re-enabling reasserts output (not edge-triggered)
 * - Interrupt only deasserts when INTR_STATE cleared OR disabled
 */
void testbench::test_func002_level_sensitive_behavior() {
  std::string test_name = "FUNC-002 TC016: Level-Sensitive Behavior";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Set interrupt state
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Verify interrupt asserted
  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "Initial interrupt assertion failed; ";
  }

  // Wait extended time - interrupt should remain asserted (level-sensitive)
  wait(50, SC_NS);

  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt did not remain asserted (should be level-sensitive); ";
  }

  // Disable interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (dma_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt not masked when disabled; ";
  }

  // Re-enable interrupt - should immediately reassert (level, not edge)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (!dma_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt not reasserted on re-enable (should be level-sensitive, not edge); ";
  }

  // Clear INTR_STATE via STATUS write
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  // Interrupt should now deassert
  if (dma_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt not deasserted after clearing INTR_STATE; ";
  }

  // Re-enable should NOT reassert (state is cleared)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
  wait(5, SC_NS);  // Wait for interrupt signal propagation
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(5, SC_NS);  // Wait for interrupt signal propagation

  if (dma_done_intr_signal.read()) {
    passed = false;
    msg << "Interrupt incorrectly reasserted with cleared state; ";
  }

  if (passed) {
    msg << "Level-sensitive behavior verified (persistent assertion, re-enable reasserts, not edge-triggered)";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_dma_done_interrupt_clear_rw1c() {
  std::string test_name = "DMA Done Interrupt Clear RW1C";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Reuse setup style from test_dma_done_interrupt_assert to create done condition
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 16);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 16);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7 << 0) | (0x7 << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(10, SC_NS);

  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000100);
  wait(10, SC_NS);

  // Wait until done asserted
  uint32_t status = 0;
  bool done_seen = false;
  for (int i = 0; i < 200; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if ((status & 0x2) != 0) {
      done_seen = true;
      break;
    }
    wait(10, SC_NS);
  }
  if (!done_seen) {
    passed = false;
    msg << "Precondition failed: STATUS.done not asserted; ";
  }

  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0 || !dma_done_intr_signal.read()) {
    passed = false;
    msg << "Precondition failed: dma_done not asserted; ";
  }

  // Core requirement: clear via write-1 to STATUS.done (RW1C)
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002);
  wait(10, SC_NS);

  // Verify clear reflected in STATUS, INTR_STATE and signal
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

  if ((status & 0x2) != 0) {
    passed = false;
    msg << "STATUS.done not cleared by RW1C; ";
  }
  if ((intr_state & 0x1) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_done not cleared by STATUS.done RW1C; ";
  }
  if (dma_done_intr_signal.read()) {
    passed = false;
    msg << "dma_done_intr not deasserted after STATUS.done RW1C; ";
  }

  if (passed) {
    msg << "dma_done interrupt clears correctly on write-1 to STATUS.done (RW1C)";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_dma_done_auto_clear_on_new_transfer() {
  std::string test_name = "DMA Done Auto-Clear on New Transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Setup same as test_dma_done_interrupt_assert to generate done first.
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 16);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 16);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7 << 0) | (0x7 << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);
  wait(10, SC_NS);

  // First transfer: drive done/intr asserted.
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000100);

  uint32_t status = 0;
  bool done_seen = false;
  for (int i = 0; i < 200; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if ((status & 0x2) != 0) {
      done_seen = true;
      break;
    }
    wait(10, SC_NS);
  }
  if (!done_seen) {
    passed = false;
    msg << "Precondition failed: first transfer did not assert STATUS.done; ";
  }

  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0 || !dma_done_intr_signal.read()) {
    passed = false;
    msg << "Precondition failed: dma_done not asserted after first transfer; ";
  }

  // Second transfer start: STATUS.done should auto-clear when go is written.
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000100);

  bool auto_clear_seen = false;
  for (int i = 0; i < 50; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

    const bool done_cleared = (status & 0x2) == 0;
    const bool intr_cleared = (intr_state & 0x1) == 0;
    const bool sig_cleared = !dma_done_intr_signal.read();

    if (done_cleared && intr_cleared && sig_cleared) {
      auto_clear_seen = true;
      break;
    }
    wait(1, SC_NS);
  }

  if (!auto_clear_seen) {
    passed = false;
    msg << "STATUS.done/INTR_STATE.dma_done did not auto-clear when new go set; ";
  }

  if (passed) {
    msg << "dma_done auto-clear verified: new transfer go write cleared STATUS.done and deasserted interrupt";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_dma_chunk_done_auto_clear() {
  std::string test_name = "DMA Chunk Done Auto-Clear on Next Chunk";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Setup transfer that produces multiple chunk boundaries (3 chunks).
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t total_size = 192;
  const uint32_t chunk_size = 64;
  const uint32_t src_addr = 0x10000000;
  const uint32_t dst_addr = 0x20000000;

  for (uint32_t i = 0; i < total_size; ++i) {
    m_test->write_ot_memory_r_byte(src_addr + i, static_cast<uint8_t>(0x50 + i));
    m_test->write_ot_memory_w_byte(dst_addr + i, 0xA5);
  }

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7 << 0) | (0x7 << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000003); // done + chunk_done
  wait(10, SC_NS);

  // Start transfer
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000100);
  wait(10, SC_NS);

  uint32_t status = 0;
  uint32_t intr_state = 0;

  // Wait for first chunk_done assertion
  bool first_chunk_seen = false;
  for (int i = 0; i < 400; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
    if ((status & 0x20) && (intr_state & 0x2) && dma_chunk_done_intr_signal.read()) {
      first_chunk_seen = true;
      break;
    }
    wait(10, SC_NS);
  }

  if (!first_chunk_seen) {
    passed = false;
    msg << "First chunk_done event not observed; ";
  }

  // IMPORTANT: do NOT clear STATUS.chunk_done by RW1C here.
  // Verify hardware auto-clears it when next chunk starts.
  bool auto_clear_seen = false;
  bool second_chunk_seen = false;
  for (int i = 0; i < 800; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

    const bool chunk_set = (status & 0x20) != 0;
    const bool intr_set  = (intr_state & 0x2) != 0;
    const bool sig_set   = dma_chunk_done_intr_signal.read();

    // observe auto-clear window between first and second chunk_done events
    if (!chunk_set && !intr_set && !sig_set) {
      auto_clear_seen = true;
    }

    // observe later chunk_done assertion again (next chunk boundary)
    if (auto_clear_seen && chunk_set && intr_set && sig_set) {
      second_chunk_seen = true;
      break;
    }

    // stop if final done reached
    if ((status & 0x2) != 0) {
      break;
    }

    wait(10, SC_NS);
  }

  if (!auto_clear_seen) {
    passed = false;
    msg << "STATUS.chunk_done/INTR_STATE.dma_chunk_done did not auto-clear on next chunk start; ";
  }
  if (!second_chunk_seen) {
    passed = false;
    msg << "Second chunk_done event not observed after auto-clear; ";
  }

  if (passed) {
    msg << "dma_chunk_done auto-clear verified when next chunk starts";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_dma_error_interrupt_assert() {
  std::string test_name = "TC072: dma_error interrupt assert on error condition";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Program required registers (valid transfer config first)
  const uint32_t src_addr   = 0x10004000;
  const uint32_t dst_addr   = 0x20004000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;

  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002); // 4-byte
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);      // increment
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);      // increment
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004);     // dma_error enable
  wait(10, SC_NS);

  // Trigger real error condition: invalid opcode with go=1
  // CONTROL[3:0]=0x4 (invalid), CONTROL[8]=1 initial_transfer, CONTROL[31]=1 go
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000104);

  uint32_t status = 0, intr_state = 0, error_code = 0;
  bool err_seen = false;

  for (int i = 0; i < 200; ++i) {
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

    bool status_error   = (status & (1u << 3)) != 0;  // STATUS.error
    bool intr_error     = (intr_state & (1u << 2)) != 0; // INTR_STATE.dma_error
    bool signal_assert  = dma_error_intr_signal.read();

    if (status_error && intr_error && signal_assert) {
      err_seen = true;
      break;
    }
    wait(10, SC_NS);
  }

  if (!err_seen) {
    passed = false;
    msg << "dma_error assertion not observed on error condition; ";
  }

  // Check ERROR_CODE populated
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & (1u << 2)) == 0) { // opcode_error expected
    passed = false;
    msg << "ERROR_CODE.opcode_error not set (ERROR_CODE=0x" << std::hex
        << error_code << "); ";
  }

  // Optional sanity: done/chunk_done should not assert for validation error
  if (dma_done_intr_signal.read() || dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "done/chunk_done interrupts unexpectedly asserted; ";
  }

  // Cleanup
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000008); // clear STATUS.error (RW1C)
  wait(10, SC_NS);

  if (passed) {
    msg << "Verified dma_error interrupt assertion for real error condition "
           "using CONTROL/STATUS/ERROR_CODE/INTR_STATE via reg_target_socket.";
  }

  report_test_result(test_name, passed, msg.str());
}



void testbench::test_dma_error_interrupt_clear() {
  std::string test_name = "TC073: dma_error interrupt clear on STATUS.error RW1C";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Precondition: create a real error condition first (invalid opcode + go)
  // This ensures STATUS.error/INTR_STATE.dma_error/dma_error_intr are truly asserted.
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000004);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10004000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20004000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000104); // invalid opcode=0x4 + go
  wait(20, SC_NS);

  uint32_t status = 0, intr_state = 0, error_code = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);

  if ((status & (1u << 3)) == 0 || (intr_state & (1u << 2)) == 0 || !dma_error_intr_signal.read()) {
    passed = false;
    msg << "Precondition failed: dma_error not asserted before clear; ";
  }

  // TC073 target: clear via STATUS.error RW1C
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000008);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);

  if ((status & (1u << 3)) != 0) {
    passed = false;
    msg << "STATUS.error not cleared by RW1C; ";
  }
  if ((intr_state & (1u << 2)) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_error not cleared by STATUS.error RW1C; ";
  }
  if (dma_error_intr_signal.read()) {
    passed = false;
    msg << "dma_error_intr not deasserted after STATUS.error clear; ";
  }
  if (error_code != 0) {
    passed = false;
    msg << "ERROR_CODE not cleared when STATUS.error cleared (ERROR_CODE=0x"
        << std::hex << error_code << "); ";
  }

  if (passed) {
    msg << "Verified STATUS.error RW1C clears dma_error interrupt, INTR_STATE.dma_error, and ERROR_CODE.";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_dma_chunk_done_interrupt_clear() {
  std::string test_name = "TC070: dma_chunk_done interrupt clear via STATUS.chunk_done RW1C";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  uint32_t intr_state = 0;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Precondition: force chunk_done interrupt state (simple setup)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET,   0x00000002);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) == 0 || !dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "Precondition failed: chunk_done not asserted before clear; ";
  }

  // TC070 target: write-1 to STATUS.chunk_done
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000020);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x2) != 0) {
    passed = false;
    msg << "INTR_STATE.dma_chunk_done not cleared by STATUS.chunk_done RW1C; ";
  }
  if (dma_chunk_done_intr_signal.read()) {
    passed = false;
    msg << "dma_chunk_done_intr not deasserted after STATUS.chunk_done clear; ";
  }

  if (passed) {
    msg << "Verified dma_chunk_done interrupt clears on STATUS.chunk_done write-1.";
  }

  report_test_result(test_name, passed, msg.str());
}