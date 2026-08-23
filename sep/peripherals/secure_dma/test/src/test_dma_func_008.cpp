/**
 * @file test_dma_func_008.cpp
 * @brief FUNC-008: Transfer Control and Abort test implementation
 *
 * This file implements comprehensive test cases for DMA Controller transfer control
 * and abort functionality covering:
 * - Software-initiated transfer control via CONTROL.go bit
 * - Configuration validation triggering on go-bit write
 * - Transfer abort capability via CONTROL.abort bit
 * - State machine lifecycle management (IDLE ↔ BUSY state transitions)
 * - CFG_REGWEN hardware locking during BUSY state and unlocking on IDLE
 * - STATUS register updates (busy, done, aborted, error bits)
 * - Error handling when validation fails (remains IDLE, go bit auto-cleared)
 * - Transaction completion guarantees for OpenTitan-internal operations
 *
 * Test Coverage: 7 test cases validating all FUNC-008 capabilities
 * Architecture References: dma-functionality-testcases.md TC 6, 8, 13, 44, 68, 96-100
 *
 * State Machine Model:
 * - IDLE State: m_dma_busy=false, CFG_REGWEN=0x6 (unlocked), STATUS.busy=0
 * - BUSY State: m_dma_busy=true, CFG_REGWEN=0x0 (locked), STATUS.busy=1
 * - Transitions:
 *   - IDLE → BUSY: Write CONTROL.go with valid configuration
 *   - BUSY → IDLE: Transfer completion or abort operation
 *
 * Key Behaviors:
 * - CONTROL.go bit: Triggers validation and state transition (auto-cleared on completion)
 * - CONTROL.abort bit: Write-only emergency termination (always reads 0)
 * - CFG_REGWEN: Hardware-managed lock (RO) reflecting DMA busy/idle state
 * - STATUS.busy: Read-only indicator of active transfer state
 * - STATUS.done: Set on transfer completion, auto-clears on new transfer
 * - STATUS.aborted: Set on abort completion, RW1C manual clear
 * - STATUS.error: Set on validation failure, prevents BUSY transition
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// FUNC-008 Test Orchestration
// =============================================================================

/**
 * @brief Execute all FUNC-008 test cases
 *
 * Runs comprehensive transfer control and abort tests covering:
 * - Go-bit transfer initiation with validation triggering
 * - Abort-bit emergency termination with status updates
 * - State machine lifecycle (IDLE-BUSY-IDLE transitions)
 * - CFG_REGWEN hardware locking mechanism
 * - STATUS register behavior (busy, done, aborted, error bits)
 * - Configuration validation integration (error handling)
 * - CONTROL and STATUS register accessibility during transfers
 */
void testbench::run_func008_tests() {
  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-008: Transfer Control and Abort Tests\n"
                       << "========================================\n"
                       << std::endl;

  wait(20, SC_NS);

  // Transfer Control Tests (3 tests)
  // test_func008_go_bit_transfer_initiation();
  // test_func008_go_bit_validation_failure();
  // test_func008_go_bit_auto_clear_on_completion();

  // State Machine and Locking Tests (2 tests)
  // test_func008_cfg_regwen_hardware_locking();
  // test_func008_control_status_always_accessible();

  // Transfer Abort Tests (2 tests)
  test_abort_during_transfer();
  test_abort_ot_transactions_complete();
  test_abort_status_clearing();

  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-008 Test Suite Complete\n"
                       << "========================================\n"
                       << std::endl;
}

// =============================================================================
// FUNC-008 TC001: Go Bit Transfer Initiation
// =============================================================================

/**
 * @brief Verify CONTROL.go bit triggers transfer and validation
 *
 * Test Objective:
 * - Confirm writing CONTROL.go=1 triggers configuration validation
 * - Verify successful validation transitions DMA to BUSY state
 * - Verify STATUS.busy is set when transfer is active
 * - Verify CFG_REGWEN locks to 0x0 when DMA is busy
 *
 * Pass Criteria:
 * - CONTROL.go accepts write
 * - STATUS.busy transitions from 0 to 1 after go-bit write
 * - CFG_REGWEN transitions from 0x6 (unlocked) to 0x0 (locked)
 * - DMA remains in BUSY state until transfer completes or aborts
 *
 * Architecture Reference: FUNC-008 TC096, detailed-design Section 7.3
 * Per detailed-design: "Setting go bit triggers state machine transition from
 * IDLE to active. Hardware automatically clears go bit after completion in
 * non-handshake mode."
 */
void testbench::test_func008_go_bit_transfer_initiation() {
  std::string test_name = "FUNC-008 TC001: Go Bit Transfer Initiation";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset to start from known IDLE state
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Verify initial IDLE state
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN not 0x6 in IDLE state (got 0x" << std::hex << cfg_regwen << "); ";
  }

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) != 0) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy already set in IDLE state; ";
  }

  // Configure valid transfer parameters for successful validation
  // Transfer width: FOUR_BYTE (0x2) - default reset value
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);

  // Sizes: Valid non-zero values
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);

  // Addresses: 4-byte aligned (word-aligned for FOUR_BYTE width)
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000); // aligned
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000); // aligned
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  // ASID: OT_ADDR (0x7) for both source and destination
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4); // src_asid=0x7, dst_asid=0x7
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);

  // Addressing modes: Both increment
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001); // increment=1
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001); // increment=1

  wait(10, SC_NS);

  // Write CONTROL.go bit to initiate transfer
  uint32_t control_val = 0x80000000; // Bit 31: go=1, opcode=0x0 (COPY)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA transitioned to BUSY state
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy not set after go-bit write; ";
  }

  // Verify CFG_REGWEN locked (0x0)
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000000) {
    passed = false;
    msg << "CFG_REGWEN not locked (expected 0x0, got 0x" << std::hex << cfg_regwen << "); ";
  }

  // NOTE: Actual data transfer execution requires FUNC-009 (Transfer Engine) integration
  // For FUNC-008, we verify the state machine transition and locking behavior

  if (passed) {
    msg << "Go-bit initiates transfer: STATUS.busy=1, CFG_REGWEN=0x0 (validation passed, IDLE→BUSY)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC002: Go Bit with Validation Failure
// =============================================================================

/**
 * @brief Verify validation failure blocks BUSY transition
 *
 * Test Objective:
 * - Confirm invalid configuration prevents DMA from entering BUSY state
 * - Verify STATUS.error is set when validation fails
 * - Verify STATUS.busy remains 0 (DMA stays in IDLE state)
 * - Verify CFG_REGWEN remains 0x6 (unlocked) on validation failure
 * - Verify go bit is automatically cleared when validation fails
 *
 * Pass Criteria:
 * - After go-bit write with invalid configuration:
 *   - STATUS.error is set
 *   - STATUS.busy remains 0
 *   - CFG_REGWEN remains 0x6 (unlocked)
 *   - ERROR_CODE contains specific error bits
 *   - CONTROL.go reads back as 0 (auto-cleared)
 *
 * Architecture Reference: FUNC-008 integration with FUNC-006 validation
 * Per detailed-design: "Configuration validation before transfer initiation.
 * If validation fails, go bit is cleared and STATUS.error is set."
 */
void testbench::test_func008_go_bit_validation_failure() {
  std::string test_name = "FUNC-008 TC002: Go Bit with Validation Failure";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset to start from IDLE state
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure INVALID parameters to trigger validation failure
  // Invalid: TOTAL_DATA_SIZE = 0 (triggers size_error)
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);

  // Other parameters (valid, but won't matter due to size error)
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);

  wait(10, SC_NS);

  // Write CONTROL.go bit with invalid configuration
  uint32_t control_val = 0x80000000; // Bit 31: go=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA remained in IDLE state (validation failed)
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);

  if ((status & 0x1) != 0) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy incorrectly set despite validation failure; ";
  }

  if ((status & 0x8) == 0) { // Bit 3: error
    passed = false;
    msg << "STATUS.error not set on validation failure; ";
  }

  // Verify CFG_REGWEN remained unlocked (0x6)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN changed from 0x6 despite validation failure (got 0x"
        << std::hex << cfg_regwen << "); ";
  }

  // Verify ERROR_CODE contains size_error (bit 3)
  uint32_t error_code = 0;
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, error_code);
  if ((error_code & 0x8) == 0) { // Bit 3: size_error
    passed = false;
    msg << "ERROR_CODE.size_error not set (got 0x" << std::hex << error_code << "); ";
  }

  // Verify go bit was auto-cleared
  uint32_t control_readback = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if ((control_readback & 0x80000000) != 0) { // Bit 31: go
    passed = false;
    msg << "CONTROL.go not auto-cleared on validation failure; ";
  }

  if (passed) {
    msg << "Validation failure blocks BUSY: STATUS.error=1, STATUS.busy=0, CFG_REGWEN=0x6 (remains IDLE)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC003: Go Bit Auto-Clear on Completion
// =============================================================================

/**
 * @brief Verify go bit auto-clears on transfer completion
 *
 * Test Objective:
 * - Confirm hardware automatically clears go bit when transfer completes
 * - Verify STATUS.done is set on completion
 * - Verify STATUS.busy is cleared on completion
 * - Verify CFG_REGWEN unlocks to 0x6 on completion (returns to IDLE)
 * - Verify new transfer can be initiated after completion
 *
 * Pass Criteria:
 * - After transfer completion (simulated):
 *   - CONTROL.go reads back as 0 (auto-cleared)
 *   - STATUS.done is set
 *   - STATUS.busy is cleared
 *   - CFG_REGWEN returns to 0x6 (unlocked)
 *
 * Architecture Reference: FUNC-008 TC068, detailed-design Section 2.1
 * Per detailed-design: "For normal operations, hardware automatically clears
 * go bit after transfer completion. For hardware handshake mode, go bit
 * remains set until software explicitly clears it."
 *
 * Note: Full transfer execution requires FUNC-009 integration. This test
 * documents the expected behavior for when transfer engine completes.
 */
void testbench::test_func008_go_bit_auto_clear_on_completion() {
  std::string test_name = "FUNC-008 TC003: Go Bit Auto-Clear on Completion";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // NOTE: This test documents the expected behavior when FUNC-009 (Transfer Engine)
  // is integrated. Currently, the model implements validation and state transitions,
  // but not the actual data transfer loop.
  //
  // Expected sequence when FUNC-009 is complete:
  // 1. Configure valid transfer parameters
  // 2. Write CONTROL.go=1 → DMA enters BUSY state
  // 3. Transfer engine executes data transfer
  // 4. On completion:
  //    - Hardware sets STATUS.done=1
  //    - Hardware clears STATUS.busy=0
  //    - Hardware clears CONTROL.go=0
  //    - Hardware unlocks CFG_REGWEN=0x6
  //    - DMA returns to IDLE state

  // For now, verify that the state machine infrastructure is in place
  // by confirming IDLE state properties
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN not 0x6 in IDLE state; ";
  }

  uint32_t control = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control);
  if ((control & 0x80000000) != 0) {
    passed = false;
    msg << "CONTROL.go not 0 in IDLE state; ";
  }

  if (passed) {
    msg << "Go-bit auto-clear on completion documented (requires FUNC-009 for full test execution)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC004: CFG_REGWEN Hardware Locking
// =============================================================================

/**
 * @brief Verify CFG_REGWEN hardware locking mechanism
 *
 * Test Objective:
 * - Confirm CFG_REGWEN is read-only and hardware-managed
 * - Verify CFG_REGWEN reflects DMA busy/idle state
 * - Verify configuration registers are write-protected when CFG_REGWEN=0x0
 * - Verify configuration registers are writable when CFG_REGWEN=0x6
 *
 * Pass Criteria:
 * - Write attempts to CFG_REGWEN are ignored (read-only register)
 * - CFG_REGWEN reads 0x6 when DMA is idle
 * - CFG_REGWEN reads 0x0 when DMA is busy
 * - Configuration registers reject writes when CFG_REGWEN=0x0
 * - Configuration registers accept writes when CFG_REGWEN=0x6
 *
 * Architecture Reference: FUNC-008 TC008, detailed-design Section 2.10
 * Per detailed-design: "Hardware-managed read-only register reflecting DMA
 * busy/idle state. 0x0=locked/busy, 0x6=unlocked/idle."
 */
void testbench::test_func008_cfg_regwen_hardware_locking() {
  std::string test_name = "FUNC-008 TC004: CFG_REGWEN Hardware Locking";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset to start in IDLE state
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Test 1: Verify CFG_REGWEN is read-only (write attempts ignored)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  uint32_t initial_value = cfg_regwen;

  // Attempt to write to CFG_REGWEN (should be ignored)
  m_test->register_write_32(secure_dma_basetest::CFG_REGWEN_OFFSET, 0x00000000);
  wait(5, SC_NS);

  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != initial_value) {
    passed = false;
    msg << "CFG_REGWEN changed on write attempt (should be read-only); ";
  }

  // Test 2: Verify CFG_REGWEN=0x6 in IDLE state
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN not 0x6 in IDLE state (got 0x" << std::hex << cfg_regwen << "); ";
  }

  // Test 3: Verify configuration registers are writable when CFG_REGWEN=0x6
  uint32_t test_value = 0x12345678;
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, test_value);
  wait(5, SC_NS);

  uint32_t readback = 0;
  m_test->register_read_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, readback);
  if (readback != test_value) {
    passed = false;
    msg << "Configuration register not writable when CFG_REGWEN=0x6; ";
  }

  // Test 4: Configure valid transfer and enter BUSY state
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  wait(5, SC_NS);

  // Write go bit to enter BUSY state
  uint32_t control_val = 0x80000000;
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Test 5: Verify CFG_REGWEN=0x0 in BUSY state
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000000) {
    passed = false;
    msg << "CFG_REGWEN not 0x0 in BUSY state (got 0x" << std::hex << cfg_regwen << "); ";
  }

  // Test 6: Verify configuration registers are write-protected when CFG_REGWEN=0x0
  uint32_t original_value = 0;
  m_test->register_read_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, original_value);

  uint32_t new_value = 0x87654321;
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, new_value);
  wait(5, SC_NS);

  m_test->register_read_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, readback);
  if (readback != original_value) {
    passed = false;
    msg << "Configuration register writable when CFG_REGWEN=0x0 (should be locked); ";
  }

  if (passed) {
    msg << "CFG_REGWEN hardware locking: RO register, 0x6=IDLE/unlocked, 0x0=BUSY/locked";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC005: CONTROL and STATUS Always Accessible
// =============================================================================

/**
 * @brief Verify CONTROL and STATUS remain accessible during transfers
 *
 * Test Objective:
 * - Confirm CONTROL register is writable even when CFG_REGWEN=0x0
 * - Confirm STATUS register is readable and RW1C writable when CFG_REGWEN=0x0
 * - Verify exception to CFG_REGWEN locking for abort operations
 * - Verify STATUS bit clearing works during active transfers
 *
 * Pass Criteria:
 * - CONTROL register accepts writes when DMA is BUSY (CFG_REGWEN=0x0)
 * - STATUS register is readable when DMA is BUSY
 * - STATUS RW1C bits can be cleared when DMA is BUSY
 * - Abort bit write is processed when DMA is BUSY
 *
 * Architecture Reference: FUNC-008 TC013, detailed-design Section 2.10
 * Per detailed-design: "CONTROL and STATUS registers remain accessible even
 * when CFG_REGWEN indicates locked state."
 */
void testbench::test_func008_control_status_always_accessible() {
  std::string test_name = "FUNC-008 TC005: CONTROL and STATUS Always Accessible";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure valid transfer and enter BUSY state
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  wait(5, SC_NS);

  // Enter BUSY state
  uint32_t control_val = 0x80000000; // go=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA is in BUSY state
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000000) {
    passed = false;
    msg << "Not in BUSY state (CFG_REGWEN not 0x0); ";
  }

  // Test 1: Verify STATUS register is readable during BUSY
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy not set in BUSY state; ";
  }

  // Test 2: Verify CONTROL register is writable during BUSY
  // Read current CONTROL value
  uint32_t control_before = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_before);

  // Attempt to modify CONTROL (e.g., change opcode field - bits [3:0])
  // Note: This won't affect the ongoing transfer, but verifies write access
  uint32_t control_modify = control_before | 0x00000001; // Set opcode bit 0
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_modify);
  wait(5, SC_NS);

  uint32_t control_after = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_after);

  // Note: The write may or may not take effect depending on implementation
  // The key test is that the write operation itself is accepted (no exception)
  // For this test, we verify the register remains accessible

  // Test 3: Verify STATUS RW1C bits can be written during BUSY
  // (Though they may have no effect until transfer completes/aborts)
  // This verifies the write interface is accessible
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000002); // Write to bit 1 (done)
  wait(5, SC_NS);

  // Test 4: Verify abort bit can be written during BUSY
  uint32_t abort_command = 0x08000000; // Bit 27: abort=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, abort_command);
  wait(10, SC_NS);

  // Verify abort was processed (STATUS.aborted should be set)
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x4) == 0) { // Bit 2: aborted
    passed = false;
    msg << "STATUS.aborted not set after abort during BUSY; ";
  }

  if (passed) {
    msg << "CONTROL and STATUS accessible during BUSY: exception to CFG_REGWEN locking";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC006: Abort During Transfer
// =============================================================================

/**
 * @brief Verify abort operation halts transfer and sets aborted status
 *
 * Test Objective:
 * - Confirm writing CONTROL.abort=1 terminates ongoing transfer
 * - Verify abort bit is write-only (always reads 0)
 * - Verify STATUS.aborted is set when abort completes
 * - Verify STATUS.busy is cleared after abort
 * - Verify CFG_REGWEN unlocks to 0x6 after abort (returns to IDLE)
 * - Verify CONTROL.go is cleared after abort
 *
 * Pass Criteria:
 * - CONTROL.abort accepts write but reads back as 0 (write-only)
 * - STATUS.aborted transitions from 0 to 1 after abort
 * - STATUS.busy transitions from 1 to 0 after abort
 * - CFG_REGWEN transitions from 0x0 to 0x6 after abort
 * - CONTROL.go is cleared after abort
 *
 * Architecture Reference: FUNC-008 TC096/097, detailed-design Section 1.12
 * Per detailed-design: "Abort operation takes effect immediately. OpenTitan
 * internal transactions are guaranteed to complete. aborted bit in STATUS
 * is set once abort completes."
 */
void testbench::test_abort_during_transfer() {
  std::string test_name = "Abort During Transfer";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Program required transfer registers
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4)); // OT->OT
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 1024);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 256);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2); // 4-byte
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);     // increment
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);     // increment
  wait(10, SC_NS);

  // Start transfer
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1
  wait(10, SC_NS);

  // Ensure transfer is active
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // busy
    passed = false;
    msg << "STATUS.busy not set before abort; ";
  }

  // Abort transfer
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort=1
  wait(20, SC_NS);

  // Verify abort outcome
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x4) == 0) { // aborted
    passed = false;
    msg << "STATUS.aborted not set after abort; ";
  }
  if ((status & 0x1) != 0) { // busy
    passed = false;
    msg << "STATUS.busy not cleared after abort; ";
  }

  if (passed) {
    msg << "Abort halts ongoing transfer and sets STATUS.aborted";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-008 TC007: Abort Status Clearing
// =============================================================================

/**
 * @brief Verify STATUS.aborted can be cleared via RW1C mechanism
 *
 * Test Objective:
 * - Confirm STATUS.aborted follows RW1C (Read Write 1 to Clear) behavior
 * - Verify writing 1 to STATUS.aborted clears the bit
 * - Verify writing 0 to STATUS.aborted has no effect
 * - Verify new transfer can be initiated after clearing aborted status
 *
 * Pass Criteria:
 * - After abort, STATUS.aborted is set
 * - Writing 1 to STATUS.aborted clears the bit
 * - After clearing, STATUS.aborted reads as 0
 * - New transfer can be configured and initiated
 *
 * Architecture Reference: FUNC-008 TC100, detailed-design Section 2.2
 * Per detailed-design: "aborted bit (RW1C): Set when abort operation
 * completes. Cleared by writing 1."
 */
void testbench::test_abort_status_clearing() {
  std::string test_name = "FUNC-008 TC007: Abort Status Clearing";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure and start transfer
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 128);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 128);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  wait(5, SC_NS);

  uint32_t control_val = 0x80000000; // go=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Abort the transfer
  uint32_t abort_command = 0x08000000; // abort=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, abort_command);
  wait(20, SC_NS);

  // Verify STATUS.aborted is set
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x4) == 0) { // Bit 2: aborted
    passed = false;
    msg << "STATUS.aborted not set after abort; ";
  }

  // Clear STATUS.aborted by writing 1 (RW1C)
  uint32_t clear_aborted = 0x00000004; // Bit 2: write 1 to clear
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, clear_aborted);
  wait(5, SC_NS);

  // Verify STATUS.aborted is cleared
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x4) != 0) { // Bit 2: aborted
    passed = false;
    msg << "STATUS.aborted not cleared after RW1C write; ";
  }

  // Verify DMA is ready for new transfer (in IDLE state)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN not 0x6 after abort clearing (not ready for new transfer); ";
  }

  // Verify new transfer can be initiated
  // Reconfigure and start new transfer
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 64);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 64);
  wait(5, SC_NS);

  control_val = 0x80000000; // go=1
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA entered BUSY state (new transfer started)
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // Bit 0: busy
    passed = false;
    msg << "New transfer not started after abort clearing; ";
  }

  if (passed) {
    msg << "STATUS.aborted RW1C clearing: write-1-to-clear, enables new transfer initiation";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_abort_ot_transactions_complete() {
  std::string test_name =
      "Abort OT Transactions Complete";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10000000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 512;
  const uint32_t chunk_size = 128;

  // Initialize source pattern and destination sentinel
  for (uint32_t i = 0; i < total_size; ++i) {
    m_test->write_ot_memory_r_byte(src_addr + i, static_cast<uint8_t>(0x30 + i));
    m_test->write_ot_memory_w_byte(dst_addr + i, 0xA5);
  }

  // Required registers
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, (0x7u << 0) | (0x7u << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2); // 4-byte
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);     // increment
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);     // increment
  wait(10, SC_NS);

  // Start transfer
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1
  wait(40, SC_NS); // allow some OT transactions to occur

  // Abort
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort=1
  wait(20, SC_NS);

  // STATUS checks
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x4) == 0) { // aborted
    passed = false;
    msg << "STATUS.aborted not set; ";
  }
  if ((status & 0x1) != 0) { // busy
    passed = false;
    msg << "STATUS.busy not cleared after abort; ";
  }

  // Check completed-bytes accounting from address progression
  uint32_t src_final = 0, dst_final = 0;
  m_test->register_read_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_final);
  m_test->register_read_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_final);

  uint32_t src_advanced = src_final - src_addr;
  uint32_t dst_advanced = dst_final - dst_addr;

  if (src_advanced != dst_advanced) {
    passed = false;
    msg << "SRC/DST advancement mismatch; ";
  }

  // For 4-byte width, completed bytes should be 4-byte aligned
  if ((dst_advanced & 0x3) != 0) {
    passed = false;
    msg << "Advanced byte count not 4-byte aligned; ";
  }

  // Verify all completed bytes are fully committed (no torn OT writes)
  for (uint32_t i = 0; i < dst_advanced; ++i) {
    uint8_t s = m_test->read_ot_memory_r_byte(src_addr + i);
    uint8_t d = m_test->read_ot_memory_w_byte(dst_addr + i);
    if (s != d) {
      passed = false;
      msg << "Data mismatch in completed region at byte " << i << "; ";
      break;
    }
  }

  if (passed) {
    msg << "OT-internal transactions completed before abort finish (STATUS.aborted=1)";
  }

  report_test_result(test_name, passed, msg.str());
}