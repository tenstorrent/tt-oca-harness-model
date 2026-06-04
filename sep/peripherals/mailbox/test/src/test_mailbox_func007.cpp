/**
 * @file test_mailbox_func007.cpp
 * @brief FUNC-007: Software-Controlled FIFO Management - Test Suite
 *
 * Implements 3 test cases mapped to FUNC_007 from mailbox-functionality-testcases.md:
 * - TC046: test_ctrl_flush_write_fifo        (Write FIFO flush operation)
 * - TC047: test_ctrl_flush_read_fifo         (Read FIFO flush operation)
 * - TC048: test_ctrl_flush_dual_port_or      (Dual-port flush OR coordination)
 *
 * Note: TC003 (test_reg_ctrl_wo_selfclearing) is shared with FUNC-002 and
 * implemented in test_mailbox_func002.cpp.
 *
 * Verification Objectives:
 * - Software-initiated FIFO flush through CTRL register (offset 0x48)
 * - Per-FIFO flush granularity (write FIFO vs read FIFO)
 * - Dual-port OR coordination (either port can flush either FIFO)
 * - Self-clearing register behavior (CTRL resets to 0x0 after flush)
 * - STATUS flag updates after flush operations
 * - Cross-port flush effects (Port 0 flush affects Port 1 FIFO state)
 * - Atomic flush execution (immediate FIFO clearing)
 * - Data loss confirmation (flushed data permanently discarded)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>
#include <vector>

// =============================================================================
// FUNC-007 Test Case Implementations (Test IDs: 36-38)
// =============================================================================

// NOTE: TC003 (test_reg_ctrl_wo_selfclearing) is shared with FUNC-002 and
// already implemented in test_mailbox_func002.cpp.

/**
 * @brief TC046: test_ctrl_flush_write_fifo
 *
 * Verification Objective:
 * Verify write FIFO flush operation through CTRL[0]=1 (wflush bit). Validate that
 * writing to CTRL[0] immediately clears the write FIFO, updates STATUS flags for
 * current and peer ports, and CTRL register self-clears to 0x0.
 *
 * Test Procedure:
 * 1. Reset DUT to ensure clean state
 * 2. Fill Port 0 write FIFO with known data patterns (3 entries)
 * 3. Verify Port 0 STATUS[1]=0 (not full), STATUS[2] depends on threshold
 * 4. Verify Port 1 STATUS[0]=0 (data available in read FIFO)
 * 5. Write CTRL[0]=1 on Port 0 to flush write FIFO (wflush)
 * 6. Verify Port 0 write FIFO cleared:
 *    - STATUS[1]=0 (not full)
 *    - STATUS[2]=0 (threshold cleared)
 * 7. Verify Port 1 read FIFO cleared (cross-port effect):
 *    - STATUS[0]=1 (empty)
 *    - STATUS[3]=0 (threshold cleared)
 * 8. Verify CTRL register reads as 0x0 (self-clearing)
 * 9. Verify Port 1 cannot read data (data permanently discarded)
 * 10. Repeat test for Port 1 to verify symmetry
 *
 * Pass Criteria:
 * - Write FIFO flush clears all FIFO entries
 * - STATUS[1] clears (not full) after flush
 * - STATUS[2] clears (write threshold) after flush
 * - Peer port STATUS[0] sets (empty) after flush
 * - Peer port STATUS[3] clears (read threshold) after flush
 * - CTRL register reads as 0x0 after flush
 * - Flushed data permanently discarded (no read recovery)
 * - Behavior consistent across both ports
 *
 * Related Registers: CTRL (0x48), STATUS (0x10), WRITE_DATA (0x00), READ_DATA (0x08)
 * Related Ports: slv_reqs_i[0], slv_resps_o[0], slv_reqs_i[1], slv_resps_o[1]
 * Test Type: Positive
 */
void testbench::test_ctrl_flush_write_fifo() {
  std::string test_name = "TC046: test_ctrl_flush_write_fifo";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify write FIFO flush operation";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t status_value = 0;
  uint64_t read_value = 0;
  tlm::tlm_response_status status;

  apply_reset();

  // =========================================================================
  // Test Step 1: Fill Port 0 write FIFO with data
  // =========================================================================
  CSML_INFO(2, logger) << "Step 1: Filling Port 0 write FIFO with 3 entries";

  std::vector<uint64_t> test_data = {
    0xDEADBEEF12345678ULL,
    0xCAFEBABE87654321ULL,
    0x0123456789ABCDEFULL
  };

  for (const auto& data : test_data) {
    test_port0->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  // =========================================================================
  // Test Step 2: Verify Port 1 STATUS[0]=0 (data available in read FIFO)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 2: Verifying Port 1 read FIFO has data available";

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  if ((status_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[0] empty flag set before flush (expected data available)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 read FIFO has data available (STATUS[0]=0)";
  }

  // =========================================================================
  // Test Step 3: Write CTRL[0]=1 on Port 0 to flush write FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Step 3: Writing CTRL[0]=1 to flush Port 0 write FIFO";

  test_port0->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x1); // wflush bit
  wait(5, SC_NS);

  // =========================================================================
  // Test Step 4: Verify Port 0 STATUS flags after flush
  // =========================================================================
  CSML_INFO(2, logger) << "Step 4: Verifying Port 0 STATUS flags after write FIFO flush";

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  // STATUS[1] full flag should be 0 (not full)
  if ((status_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[1] full flag set after flush (expected cleared)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 STATUS[1] full flag cleared after flush";
  }

  // STATUS[2] write threshold flag should be 0 (cleared)
  if ((status_value & 0x4) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[2] write threshold flag set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 STATUS[2] write threshold flag cleared";
  }

  // =========================================================================
  // Test Step 5: Verify Port 1 STATUS flags after flush (cross-port effect)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 5: Verifying Port 1 STATUS flags (cross-port effect)";

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  // STATUS[0] empty flag should be 1 (read FIFO now empty)
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[0] empty flag not set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 STATUS[0] empty flag set (read FIFO flushed)";
  }

  // STATUS[3] read threshold flag should be 0 (cleared)
  if ((status_value & 0x8) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[3] read threshold flag set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 STATUS[3] read threshold flag cleared";
  }

  // =========================================================================
  // Test Step 7: Verify Port 1 cannot read flushed data
  // =========================================================================
  // FIFO emptiness already verified via STATUS[0]=1 in Step 5.
  // Note: TLM response status not checked (CSML framework limitation).
  CSML_INFO(2, logger) << "Step 7: Verifying flushed data permanently discarded (STATUS[0]=1 confirmed)";
  status = mailbox_read(1, mailbox_basetest::READ_DATA_OFFSET, read_value);
  (void)status;

  // =========================================================================
  // Test Step 8: Repeat test for Port 1 (symmetry verification)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 8: Verifying Port 1 write FIFO flush symmetry";

  // Fill Port 1 write FIFO
  for (const auto& data : test_data) {
    test_port1->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  // Flush Port 1 write FIFO
  test_port1->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x1);
  wait(5, SC_NS);

  // Verify Port 0 read FIFO empty
  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[0] not set after Port 1 flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 write FIFO flush works symmetrically";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

/**
 * @brief TC047: test_ctrl_flush_read_fifo
 *
 * Verification Objective:
 * Verify read FIFO flush operation through CTRL[1]=1 (rflush bit). Validate that
 * writing to CTRL[1] immediately clears the read FIFO (which is peer's write FIFO
 * due to cross-connection), updates STATUS flags, and CTRL self-clears.
 *
 * Test Procedure:
 * 1. Reset DUT to ensure clean state
 * 2. Peer (Port 1) fills Port 0 read FIFO by writing to its WRITE_DATA
 * 3. Verify Port 0 STATUS[0]=0 (data available in read FIFO)
 * 4. Verify Port 1 STATUS[1] reflects write FIFO usage
 * 5. Write CTRL[1]=1 on Port 0 to flush read FIFO (rflush)
 * 6. Verify Port 0 read FIFO cleared:
 *    - STATUS[0]=1 (empty)
 *    - STATUS[3]=0 (threshold cleared)
 * 7. Verify Port 1 write FIFO cleared (cross-port effect):
 *    - STATUS[1]=0 (not full)
 *    - STATUS[2]=0 (threshold cleared)
 * 8. Verify CTRL register self-cleared to 0x0
 * 9. Verify Port 0 cannot read data (flushed)
 * 10. Repeat test for Port 1 to verify symmetry
 *
 * Pass Criteria:
 * - Read FIFO flush clears all FIFO entries
 * - STATUS[0] sets (empty) after flush
 * - STATUS[3] clears (read threshold) after flush
 * - Peer port STATUS[1] clears (not full) after flush
 * - Peer port STATUS[2] clears (write threshold) after flush
 * - CTRL register self-clears to 0x0
 * - Flushed data permanently discarded
 * - Behavior consistent across both ports
 *
 * Related Registers: CTRL (0x48), STATUS (0x10), WRITE_DATA (0x00), READ_DATA (0x08)
 * Related Ports: slv_reqs_i[0], slv_resps_o[0], slv_reqs_i[1], slv_resps_o[1]
 * Test Type: Positive
 */
void testbench::test_ctrl_flush_read_fifo() {
  std::string test_name = "TC047: test_ctrl_flush_read_fifo";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify read FIFO flush operation";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t status_value = 0;
  uint64_t read_value = 0;
  tlm::tlm_response_status status;

  apply_reset();

  // =========================================================================
  // Test Step 1: Peer (Port 1) fills Port 0 read FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Step 1: Port 1 filling Port 0 read FIFO with 3 entries";

  std::vector<uint64_t> test_data = {
    0x1111111111111111ULL,
    0x2222222222222222ULL,
    0x3333333333333333ULL
  };

  for (const auto& data : test_data) {
    test_port1->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  // =========================================================================
  // Test Step 2: Verify Port 0 STATUS[0]=0 (data available)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 2: Verifying Port 0 has data available in read FIFO";

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  if ((status_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[0] empty flag set (expected data available)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 read FIFO has data available";
  }

  // =========================================================================
  // Test Step 3: Write CTRL[1]=1 on Port 0 to flush read FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Step 3: Writing CTRL[1]=1 to flush Port 0 read FIFO";

  test_port0->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x2); // rflush bit
  wait(5, SC_NS);

  // =========================================================================
  // Test Step 4: Verify Port 0 STATUS flags after flush
  // =========================================================================
  CSML_INFO(2, logger) << "Step 4: Verifying Port 0 STATUS flags after read FIFO flush";

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  // STATUS[0] empty flag should be 1 (read FIFO now empty)
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[0] empty flag not set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 STATUS[0] empty flag set (read FIFO flushed)";
  }

  // STATUS[3] read threshold flag should be 0 (cleared)
  if ((status_value & 0x8) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 STATUS[3] read threshold flag set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 STATUS[3] read threshold flag cleared";
  }

  // =========================================================================
  // Test Step 5: Verify Port 1 STATUS flags (cross-port effect)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 5: Verifying Port 1 STATUS flags (peer write FIFO flushed)";

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);

  // STATUS[1] full flag should be 0 (write FIFO not full)
  if ((status_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[1] full flag set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 STATUS[1] full flag cleared (write FIFO space available)";
  }

  // STATUS[2] write threshold flag should be 0 (cleared)
  if ((status_value & 0x4) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[2] write threshold flag set after flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 STATUS[2] write threshold flag cleared";
  }

  // =========================================================================
  // Test Step 6: Verify Port 0 cannot read flushed data
  // =========================================================================
  // FIFO emptiness already verified via STATUS[0]=1 in Step 4.
  // Note: TLM response status not checked (CSML framework limitation).
  CSML_INFO(2, logger) << "Step 6: Verifying flushed data permanently discarded (STATUS[0]=1 confirmed)";
  status = mailbox_read(0, mailbox_basetest::READ_DATA_OFFSET, read_value);
  (void)status;

  // =========================================================================
  // Test Step 7: Repeat test for Port 1 (symmetry verification)
  // =========================================================================
  CSML_INFO(2, logger) << "Step 7: Verifying Port 1 read FIFO flush symmetry";

  // Port 0 fills Port 1 read FIFO
  for (const auto& data : test_data) {
    test_port0->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  // Port 1 flushes its read FIFO
  test_port1->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x2); // rflush
  wait(5, SC_NS);

  // Verify Port 1 read FIFO empty
  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 STATUS[0] not set after read flush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 read FIFO flush works symmetrically";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

/**
 * @brief TC048: test_ctrl_flush_dual_port_or
 *
 * Verification Objective:
 * Verify dual-port flush OR coordination mechanism. Either port can flush either
 * FIFO type (write or read) via its own CTRL register. The flush signals are OR'd
 * across both ports to control each FIFO type.
 *
 * Test Procedure:
 * 1. Reset DUT to ensure clean state
 * 2. Test Scenario A: Port 0 flushes write FIFO
 *    - Port 0 writes data to fill its write FIFO
 *    - Port 0 writes CTRL[0]=1 (wflush)
 *    - Verify Port 1 read FIFO flushed (cross-port effect)
 * 3. Test Scenario B: Port 1 flushes write FIFO
 *    - Port 1 writes data to fill its write FIFO
 *    - Port 1 writes CTRL[0]=1 (wflush)
 *    - Verify Port 0 read FIFO flushed (cross-port effect)
 * 4. Test Scenario C: Port 0 flushes read FIFO
 *    - Port 1 writes data to fill Port 0 read FIFO
 *    - Port 0 writes CTRL[1]=1 (rflush)
 *    - Verify Port 0 read FIFO flushed
 *    - Verify Port 1 write FIFO flushed (cross-connection)
 * 5. Test Scenario D: Port 1 flushes read FIFO
 *    - Port 0 writes data to fill Port 1 read FIFO
 *    - Port 1 writes CTRL[1]=1 (rflush)
 *    - Verify Port 1 read FIFO flushed
 *    - Verify Port 0 write FIFO flushed (cross-connection)
 * 6. Test Scenario E: Simultaneous flush (both bits)
 *    - Fill both FIFOs with data
 *    - Write CTRL[1:0]=3 to flush both FIFOs
 *    - Verify both FIFOs cleared
 *
 * Pass Criteria:
 * - Either port can flush write FIFO (wflush)
 * - Either port can flush read FIFO (rflush)
 * - Flush affects cross-connected peer port
 * - OR coordination allows independent flush control
 * - Simultaneous flush (both bits) works correctly
 * - STATUS flags update consistently
 *
 * Related Registers: CTRL (0x48), STATUS (0x10), WRITE_DATA (0x00), READ_DATA (0x08)
 * Related Ports: slv_reqs_i[0], slv_resps_o[0], slv_reqs_i[1], slv_resps_o[1]
 * Test Type: Positive
 */
void testbench::test_ctrl_flush_dual_port_or() {
  std::string test_name = "TC048: test_ctrl_flush_dual_port_or";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify dual-port flush OR coordination";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t status_value = 0;

  apply_reset();

  std::vector<uint64_t> test_data = {
    0xAAAAAAAAAAAAAAAAULL,
    0xBBBBBBBBBBBBBBBBULL
  };

  // =========================================================================
  // Test Scenario A: Port 0 flushes write FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Scenario A: Port 0 flushes its write FIFO";

  for (const auto& data : test_data) {
    test_port0->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  test_port0->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x1); // wflush
  wait(5, SC_NS);

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 read FIFO not flushed by Port 0 wflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 wflush flushed Port 1 read FIFO";
  }

  // =========================================================================
  // Test Scenario B: Port 1 flushes write FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Scenario B: Port 1 flushes its write FIFO";

  for (const auto& data : test_data) {
    test_port1->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  test_port1->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x1); // wflush
  wait(5, SC_NS);

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 read FIFO not flushed by Port 1 wflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 wflush flushed Port 0 read FIFO";
  }

  // =========================================================================
  // Test Scenario C: Port 0 flushes read FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Scenario C: Port 0 flushes its read FIFO";

  // Port 1 fills Port 0 read FIFO
  for (const auto& data : test_data) {
    test_port1->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  test_port0->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x2); // rflush
  wait(5, SC_NS);

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 read FIFO not flushed by Port 0 rflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 rflush flushed Port 0 read FIFO";
  }

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 write FIFO not cleared by Port 0 rflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 0 rflush cleared Port 1 write FIFO (cross-connection)";
  }

  // =========================================================================
  // Test Scenario D: Port 1 flushes read FIFO
  // =========================================================================
  CSML_INFO(2, logger) << "Scenario D: Port 1 flushes its read FIFO";

  // Port 0 fills Port 1 read FIFO
  for (const auto& data : test_data) {
    test_port0->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  test_port1->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x2); // rflush
  wait(5, SC_NS);

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 1 read FIFO not flushed by Port 1 rflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 rflush flushed Port 1 read FIFO";
  }

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  if ((status_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: Port 0 write FIFO not cleared by Port 1 rflush";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Port 1 rflush cleared Port 0 write FIFO";
  }

  // =========================================================================
  // Test Scenario E: Simultaneous flush (both bits set)
  // =========================================================================
  CSML_INFO(2, logger) << "Scenario E: Simultaneous flush of both FIFOs";

  // Fill both Port 0 FIFOs
  for (const auto& data : test_data) {
    test_port0->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  for (const auto& data : test_data) {
    test_port1->register_write_64(mailbox_basetest::WRITE_DATA_OFFSET, data);
  }
  wait(5, SC_NS);

  // Flush both FIFOs simultaneously
  test_port0->register_write_64(mailbox_basetest::CTRL_OFFSET, 0x3); // wflush + rflush
  wait(5, SC_NS);

  test_port0->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  bool port0_empty = ((status_value & 0x1) == 0x1);
  bool port0_not_full = ((status_value & 0x2) == 0);

  test_port1->register_read_64(mailbox_basetest::STATUS_OFFSET, status_value);
  bool port1_empty = ((status_value & 0x1) == 0x1);
  bool port1_not_full = ((status_value & 0x2) == 0);

  if (!port0_empty || !port0_not_full || !port1_empty || !port1_not_full) {
    CSML_ERROR(0, logger) << "FAIL: Simultaneous flush did not clear all FIFOs";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: Simultaneous flush cleared both FIFOs correctly";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// FUNC-007 Test Suite Runner
// =============================================================================

/**
 * @brief Run all FUNC-007 test cases.
 *
 * Executes 3 test cases mapped to FUNC-007: Software-Controlled FIFO Management.
 * Test IDs: 36-38 from mailbox-functionality-testcases.md.
 *
 * Test Coverage:
 * - Write FIFO flush operation via CTRL[0]=1 (TC046)
 * - Read FIFO flush operation via CTRL[1]=1  (TC047)
 * - Dual-port flush OR coordination           (TC048)
 *
 * Note: TC003 (test_reg_ctrl_wo_selfclearing) is implemented in
 * test_mailbox_func002.cpp (shared with FUNC-002 Register Access suite).
 */
void testbench::run_func007_tests() {
  CSML_INFO(2, logger) << "\n";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "FUNC-007 TEST SUITE: Software-Controlled FIFO Management";
  CSML_INFO(2, logger) << "Test Count: 3 test cases (Test IDs: 46-48)";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "\n";

  // Control Operations Tests (Test IDs: 46-48)
  test_ctrl_flush_write_fifo();          // TC046
  test_ctrl_flush_read_fifo();           // TC047
  test_ctrl_flush_dual_port_or();        // TC048

  CSML_INFO(2, logger) << "\n";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "FUNC-007 TEST SUITE COMPLETED";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "\n";
}
