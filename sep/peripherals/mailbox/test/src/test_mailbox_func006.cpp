// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/**
 * @file test_mailbox_func006.cpp
 * @brief FUNC-006: Programmable Threshold-Based Interrupt System — Test Suite
 *
 * Implements test cases mapped to FUNC_006 from mailbox-functionality-testcases.md.
 * This file provides the following implementations:
 *
 * - TC033: test_interrupt_wtirq_port0         (WTIRQ Port 0 end-to-end)
 * - TC034: test_interrupt_rtirq_port0         (RTIRQ Port 0 end-to-end)
 * - TC035: test_interrupt_wtirq_port1         (WTIRQ Port 1 end-to-end)
 * - TC036: test_interrupt_rtirq_port1         (RTIRQ Port 1 end-to-end)
 * - TC037: test_threshold_saturation_wirqt    (WIRQT saturation + retroactive trigger)
 * - TC038: test_threshold_saturation_rirqt    (RIRQT saturation + retroactive trigger)
 * - TC039: test_threshold_zero_wirqt          (Zero WIRQT threshold)
 * - TC040: test_threshold_zero_rirqt          (Zero RIRQT threshold)
 * - TC041: test_threshold_retroactive_trigger (Retroactive triggering for WIRQT and RIRQT)
 * - TC042: test_boundary_threshold_max_value  (Threshold = MailboxDepth-1)
 * - TC043: test_boundary_threshold_equal_usage(Strictly-greater-than comparison)
 * - TC044: test_config_interrupt_level_triggered (Level-triggered irq_o behaviour)
 * - TC045: test_config_interrupt_polarity        (Active-high polarity verification)
 * - run_func006_tests()                           (FUNC-006 suite orchestration)
 *
 * Tests NOT defined here (implemented in other files, called by run_func006_tests):
 *   - Test IDs 6-10, 42-45 : test_mailbox_func002.cpp (register access)
 *   - Test IDs 26, 29      : test_mailbox_func005.cpp (EIRQ)
 *
 * Architecture Context (Composition + sc_fifo):
 *   - mailbox_ip contains mailbox_base b0/b1 (register containers, no sockets)
 *   - socket0/socket1 are mailbox_ip TLM targets that delegate to b0.memory / b1.memory
 *   - fifo_0_to_1: Port 0 writes → Port 1 reads
 *   - fifo_1_to_0: Port 1 writes → Port 0 reads
 *   - irq_o[port] is driven exclusively by irq_driver() SC_METHOD
 *   - After any interrupt state change, wait(SC_ZERO_TIME) is required before
 *     reading irq_port0_sig / irq_port1_sig from SC_THREAD context
 *
 * Default DUT configuration (from testbench constructor):
 *   mailbox_ip("dut", 0x50, 8, true)
 *   → memory_size=0x50, mailbox_depth=8, irq_act_high=true
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>
#include <vector>

// =============================================================================
// Internal constants used across this file
// =============================================================================
namespace {
  /// Default mailbox FIFO depth matching testbench DUT construction parameter
  static const int    MAILBOX_DEPTH   = 8;
  /// Maximum valid (non-saturated) threshold value = MailboxDepth - 1
  static const uint64_t MAX_THRESHOLD = MAILBOX_DEPTH - 1;
}

// =============================================================================
// TC033: test_interrupt_wtirq_port0
// =============================================================================

/**
 * @brief TC024 — Write Threshold Interrupt (WTIRQ) for Port 0.
 *
 * Verification Objective:
 * Program WIRQT on Port 0, enable IRQEN[0], write data to exceed the threshold.
 * Assert that IRQS[0], IRQP[0] and irq_o[0] all activate. Clear IRQS[0] with
 * write-1-to-clear and confirm IRQP[0] and irq_o[0] deassert.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write WIRQT=3 on Port 0  → m_wirqt_threshold[0]=3
 *  3. Read  WIRQT on Port 0    → verify readback == 3 (handle_read_WIRQT returns shadow)
 *  4. Write IRQEN=0x1 on Port 0 → IRQEN[0]=1 (WTIRQ enabled)
 *  5. Write 4 entries to Port 0 WRITE_DATA  (usage=4 > threshold=3)
 *  6. Read  STATUS Port 0      → STATUS[2] must be set
 *  7. Read  IRQS Port 0        → IRQS[0] must be set
 *  8. Read  IRQP Port 0        → IRQP[0] must be set
 *  9. wait(SC_ZERO_TIME)       → allow irq_driver() to fire
 * 10. Read  irq_port0_sig      → must be true (active-high, level-triggered)
 * 11. Write IRQS=0x1 on Port 0 (W1C — clear WTIRQ)
 * 12. Read  IRQS Port 0        → IRQS[0] must be 0
 * 13. Read  IRQP Port 0        → IRQP[0] must be 0
 * 14. wait(SC_ZERO_TIME)
 * 15. Read  irq_port0_sig      → must be false (deasserted)
 *
 * Pass Criteria: All assertions above hold.
 * Threshold Comparison: usage > threshold (strictly greater-than).
 *
 * Related Registers: WIRQT (0x20), IRQEN (0x38), IRQS (0x30), IRQP (0x40),
 *                    STATUS (0x10), WRITE_DATA (0x00)
 * Related Signals: irq_o[0]
 * Test Type: Positive
 */
void testbench::test_interrupt_wtirq_port0()
{
  const std::string test_name = "TC033: test_interrupt_wtirq_port0";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: WTIRQ end-to-end for Port 0 "
                          "(threshold=3, write 4 entries, verify IRQS/IRQP/irq_o, W1C clear)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  // Step 1: Reset DUT — ensures all shadow state is cleared
  apply_reset();

  // -----------------------------------------------------------------------
  // Step 2-3: Program and verify WIRQT threshold on Port 0
  // -----------------------------------------------------------------------
  const uint64_t THRESHOLD = 3;
  REG_INFO(2, logger) << "Step 2: Writing WIRQT=" << THRESHOLD << " to Port 0";
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT write rejected (expected TLM_OK_RESPONSE)";
    test_passed = false;
  }

  // handle_read_WIRQT returns the saturated shadow-state value; verify readback
  REG_INFO(2, logger) << "Step 3: Reading back WIRQT (expect saturated value=" << THRESHOLD << ")";
  resp = mailbox_read(0, mailbox_basetest::WIRQT_OFFSET, reg_val);
  if (reg_val != THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: WIRQT readback mismatch (got 0x" << std::hex << reg_val
        << ", expected 0x" << THRESHOLD << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: WIRQT readback correct (" << THRESHOLD << ")";
  }

  // -----------------------------------------------------------------------
  // Step 4: Enable WTIRQ interrupt (IRQEN[0]=1)
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 4: Writing IRQEN=0x1 (WTIRQ enable) to Port 0";
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }
  resp = mailbox_read(0, mailbox_basetest::IRQEN_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    REG_ERROR(0, logger) << "FAIL: IRQEN[0] not set after write";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQEN[0]=1 confirmed";
  }

  // -----------------------------------------------------------------------
  // Step 5: Write 4 entries to Port 0 (usage=4 > WIRQT=3 → IRQS[0] set)
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 5: Writing 4 entries to Port 0 WRITE_DATA (usage=4 > threshold=3)";
  for (int i = 0; i < 4; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // -----------------------------------------------------------------------
  // Step 6: Verify STATUS[2]=1 (write_level_above_thresh)
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 6: Verifying STATUS[2]=1 (write_level_above_thresh)";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[2] not set (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[2]=1 (write_level_above_thresh)";
  }

  // -----------------------------------------------------------------------
  // Step 7: Verify IRQS[0]=1 (WTIRQ sticky status)
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 7: Verifying IRQS[0]=1 (WTIRQ sticky status)";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[0] not set (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=1 (WTIRQ status set)";
  }

  // -----------------------------------------------------------------------
  // Step 8: Verify IRQP[0]=1 (IRQS[0] & IRQEN[0] = 1)
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 8: Verifying IRQP[0]=1 (hardware-computed: IRQS[0] & IRQEN[0])";
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQP[0] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQP[0]=1 (WTIRQ interrupt pending)";
  }

  // -----------------------------------------------------------------------
  // Step 9-10: Check irq_o[0] assertion via irq_port0_sig
  // wait(SC_ZERO_TIME) yields to irq_driver() SC_METHOD before reading sc_signal
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 9: wait(SC_ZERO_TIME) to allow irq_driver() SC_METHOD to fire";
  wait(SC_ZERO_TIME);

  REG_INFO(2, logger) << "Step 10: Reading irq_port0_sig (expect true: active-high, level-triggered)";
  {
    bool irq_asserted = irq_port0_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] not asserted (expected true, active-high level-triggered)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=true (active-high level-triggered, IRQP[0]=1)";
    }
  }

  // -----------------------------------------------------------------------
  // Step 11: Clear IRQS[0] via write-1-to-clear
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 11: Clearing IRQS[0] with write-1-to-clear (write 0x1 to IRQS)";
  resp = mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQS W1C write rejected";
    test_passed = false;
  }

  // -----------------------------------------------------------------------
  // Step 12: Verify IRQS[0]=0 after clear
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 12: Verifying IRQS[0]=0 after W1C clear";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) != 0) {
    REG_ERROR(0, logger) << "FAIL: IRQS[0] still set after W1C clear";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=0 (cleared)";
  }

  // -----------------------------------------------------------------------
  // Step 13: Verify IRQP[0]=0
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 13: Verifying IRQP[0]=0 after IRQS[0] clear";
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) != 0) {
    REG_ERROR(0, logger) << "FAIL: IRQP[0] still set after IRQS[0] clear";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQP[0]=0 (no interrupt pending)";
  }

  // -----------------------------------------------------------------------
  // Step 14-15: Verify irq_o[0] deassertion
  // -----------------------------------------------------------------------
  REG_INFO(2, logger) << "Step 14: wait(SC_ZERO_TIME) to allow irq_driver() to deassert irq_o[0]";
  wait(SC_ZERO_TIME);

  REG_INFO(2, logger) << "Step 15: Reading irq_port0_sig (expect false: no pending interrupt)";
  {
    bool irq_still_asserted = irq_port0_sig.read();
    if (irq_still_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] still asserted after IRQS[0] cleared";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=false (deasserted after IRQS[0] clear)";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC034: test_interrupt_rtirq_port0
// =============================================================================

/**
 * @brief TC025 — Read Threshold Interrupt (RTIRQ) for Port 0.
 *
 * Verification Objective:
 * Program RIRQT on Port 0, enable IRQEN[1], have Port 1 write data that
 * exceeds Port 0's read-FIFO threshold. Verify IRQS[1], IRQP[1] and irq_o[0]
 * all activate. Clear IRQS[1] with write-1-to-clear and confirm deassert.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write RIRQT=2 on Port 0
 *  3. Read  RIRQT on Port 0 → verify readback == 2
 *  4. Write IRQEN=0x2 on Port 0 (IRQEN[1]=1, RTIRQ enabled)
 *  5. Port 1 writes 3 entries (fifo_1_to_0 fill=3 > Port 0 RIRQT=2)
 *  6. Read  STATUS Port 0     → STATUS[3] must be set
 *  7. Read  IRQS Port 0       → IRQS[1] must be set
 *  8. Read  IRQP Port 0       → IRQP[1] must be set
 *  9. wait(SC_ZERO_TIME)
 * 10. Read  irq_port0_sig     → must be true
 * 11. Write IRQS=0x2 on Port 0 (W1C — clear RTIRQ)
 * 12. Read  IRQS Port 0       → IRQS[1] must be 0
 * 13. Read  IRQP Port 0       → IRQP[1] must be 0
 * 14. wait(SC_ZERO_TIME)
 * 15. Read  irq_port0_sig     → must be false
 *
 * Pass Criteria: All assertions above hold.
 *
 * Related Registers: RIRQT (0x28), IRQEN (0x38), IRQS (0x30), IRQP (0x40),
 *                    STATUS (0x10), WRITE_DATA (0x00)
 * Related Signals: irq_o[0]
 * Test Type: Positive
 */
void testbench::test_interrupt_rtirq_port0()
{
  const std::string test_name = "TC034: test_interrupt_rtirq_port0";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: RTIRQ end-to-end for Port 0 "
                          "(Port 0 RIRQT=2, Port 1 writes 3 entries, verify IRQS/IRQP/irq_o, W1C clear)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2-3: Program and verify RIRQT on Port 0
  const uint64_t THRESHOLD = 2;
  REG_INFO(2, logger) << "Step 2: Writing Port 0 RIRQT=" << THRESHOLD;
  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: RIRQT write rejected";
    test_passed = false;
  }

  REG_INFO(2, logger) << "Step 3: Reading back Port 0 RIRQT (expect " << THRESHOLD << ")";
  resp = mailbox_read(0, mailbox_basetest::RIRQT_OFFSET, reg_val);
  if (reg_val != THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: RIRQT readback mismatch (got 0x" << std::hex << reg_val
        << ", expected 0x" << THRESHOLD << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: RIRQT readback correct (" << THRESHOLD << ")";
  }

  // Step 4: Enable RTIRQ on Port 0 (IRQEN[1]=1)
  REG_INFO(2, logger) << "Step 4: Writing Port 0 IRQEN=0x2 (RTIRQ enable)";
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }
  resp = mailbox_read(0, mailbox_basetest::IRQEN_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    REG_ERROR(0, logger) << "FAIL: IRQEN[1] not set";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQEN[1]=1 confirmed";
  }

  // Step 5: Port 1 writes 3 entries → fifo_1_to_0 fill=3 > Port 0 RIRQT=2
  REG_INFO(2, logger) << "Step 5: Port 1 writing 3 entries (Port 0 read-FIFO fill=3 > RIRQT=2)";
  for (int i = 0; i < 3; i++) {
    resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xBB00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Port 1 WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // STATUS[3] uses num_available() (not num_free()) for read-FIFO fill.
  // After nb_write() on the peer's socket, num_available() is updated by
  // sc_fifo's update() process which runs at the next delta cycle.
  // wait(SC_ZERO_TIME) yields from SC_THREAD to allow that update to occur.
  wait(SC_ZERO_TIME);

  // Step 6: STATUS[3] must reflect read_level_above_thresh
  REG_INFO(2, logger) << "Step 6: Verifying Port 0 STATUS[3]=1 (read_level_above_thresh)";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[3] not set (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[3]=1 (read_level_above_thresh)";
  }

  // Step 7: IRQS[1] sticky status
  REG_INFO(2, logger) << "Step 7: Verifying Port 0 IRQS[1]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[1] not set (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[1]=1 (RTIRQ status set)";
  }

  // Step 8: IRQP[1] = IRQS[1] & IRQEN[1]
  REG_INFO(2, logger) << "Step 8: Verifying Port 0 IRQP[1]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQP[1] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQP[1]=1 (RTIRQ pending)";
  }

  // Step 9-10: irq_o[0] assertion
  wait(SC_ZERO_TIME);
  {
    bool irq_asserted = irq_port0_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] not asserted after RTIRQ threshold exceeded";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=true (IRQP[1]=1, active-high level-triggered)";
    }
  }

  // Step 11: W1C clear IRQS[1]
  REG_INFO(2, logger) << "Step 11: Clearing Port 0 IRQS[1] (write 0x2 W1C)";
  resp = mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQS W1C write rejected";
    test_passed = false;
  }

  // Step 12: Verify IRQS[1]=0
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) != 0) {
    REG_ERROR(0, logger) << "FAIL: IRQS[1] still set after W1C";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[1]=0 (cleared)";
  }

  // Step 13: Verify IRQP[1]=0
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x2) != 0) {
    REG_ERROR(0, logger) << "FAIL: IRQP[1] still set after IRQS[1] clear";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQP[1]=0";
  }

  // Step 14-15: irq_o[0] deassertion
  wait(SC_ZERO_TIME);
  {
    bool irq_still = irq_port0_sig.read();
    if (irq_still) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] still asserted after IRQS[1] cleared";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=false (deasserted)";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC035: test_interrupt_wtirq_port1
// =============================================================================

/**
 * @brief TC027 — Write Threshold Interrupt (WTIRQ) for Port 1.
 *
 * Verification Objective:
 * Symmetric to TC024 but exercising Port 1. Program WIRQT on Port 1, enable
 * IRQEN[0] on Port 1, write data to exceed threshold. Verify IRQS[0], IRQP[0]
 * and irq_o[1] all activate. Clear IRQS[0] and confirm deassert.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write Port 1 WIRQT=4
 *  3. Read  Port 1 WIRQT → verify readback == 4
 *  4. Write Port 1 IRQEN=0x1 (IRQEN[0]=1)
 *  5. Port 1 writes 5 entries (usage=5 > WIRQT=4)
 *  6. Read  Port 1 STATUS   → STATUS[2] must be set
 *  7. Read  Port 1 IRQS     → IRQS[0] must be set
 *  8. Read  Port 1 IRQP     → IRQP[0] must be set
 *  9. wait(SC_ZERO_TIME)
 * 10. Read  irq_port1_sig   → must be true
 * 11. Write Port 1 IRQS=0x1 (W1C)
 * 12. Read  Port 1 IRQS     → IRQS[0] must be 0
 * 13. Read  Port 1 IRQP     → IRQP[0] must be 0
 * 14. wait(SC_ZERO_TIME)
 * 15. Read  irq_port1_sig   → must be false
 *
 * Pass Criteria: All assertions above hold (symmetric to Port 0 behaviour).
 *
 * Related Registers: WIRQT (0x20), IRQEN (0x38), IRQS (0x30), IRQP (0x40),
 *                    STATUS (0x10), WRITE_DATA (0x00)
 * Related Signals: irq_o[1]
 * Test Type: Positive
 */
void testbench::test_interrupt_wtirq_port1()
{
  const std::string test_name = "TC035: test_interrupt_wtirq_port1";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: WTIRQ end-to-end for Port 1 "
                          "(threshold=4, write 5 entries, verify IRQS/IRQP/irq_o[1], W1C clear)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2-3: Program and verify WIRQT on Port 1
  const uint64_t THRESHOLD = 4;
  REG_INFO(2, logger) << "Step 2: Writing Port 1 WIRQT=" << THRESHOLD;
  resp = mailbox_write(1, mailbox_basetest::WIRQT_OFFSET, THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 WIRQT write rejected";
    test_passed = false;
  }

  REG_INFO(2, logger) << "Step 3: Reading back Port 1 WIRQT (expect " << THRESHOLD << ")";
  resp = mailbox_read(1, mailbox_basetest::WIRQT_OFFSET, reg_val);
  if (reg_val != THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 WIRQT readback mismatch (got 0x" << std::hex << reg_val
        << ", expected 0x" << THRESHOLD << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 WIRQT readback correct (" << THRESHOLD << ")";
  }

  // Step 4: Enable WTIRQ on Port 1 (IRQEN[0]=1)
  REG_INFO(2, logger) << "Step 4: Writing Port 1 IRQEN=0x1 (WTIRQ enable)";
  resp = mailbox_write(1, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQEN write rejected";
    test_passed = false;
  }
  resp = mailbox_read(1, mailbox_basetest::IRQEN_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQEN[0] not set";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQEN[0]=1 confirmed";
  }

  // Step 5: Port 1 writes 5 entries (usage=5 > WIRQT=4)
  REG_INFO(2, logger) << "Step 5: Port 1 writing 5 entries (usage=5 > threshold=4)";
  for (int i = 0; i < 5; i++) {
    resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xCC00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Port 1 WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // Step 6: STATUS[2]=1 on Port 1
  REG_INFO(2, logger) << "Step 6: Verifying Port 1 STATUS[2]=1";
  resp = mailbox_read(1, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 STATUS[2] not set (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 STATUS[2]=1";
  }

  // Step 7: IRQS[0] on Port 1
  REG_INFO(2, logger) << "Step 7: Verifying Port 1 IRQS[0]=1";
  resp = mailbox_read(1, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 IRQS[0] not set (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQS[0]=1 (WTIRQ status set)";
  }

  // Step 8: IRQP[0] on Port 1
  REG_INFO(2, logger) << "Step 8: Verifying Port 1 IRQP[0]=1";
  resp = mailbox_read(1, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 IRQP[0] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQP[0]=1 (WTIRQ pending)";
  }

  // Step 9-10: irq_o[1] assertion
  wait(SC_ZERO_TIME);
  {
    bool irq_asserted = irq_port1_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[1] not asserted (expected true, active-high level-triggered)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[1]=true (IRQP[0]=1 on Port 1)";
    }
  }

  // Step 11: W1C clear Port 1 IRQS[0]
  REG_INFO(2, logger) << "Step 11: Clearing Port 1 IRQS[0] (write 0x1 W1C)";
  resp = mailbox_write(1, mailbox_basetest::IRQS_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQS W1C write rejected";
    test_passed = false;
  }

  // Step 12: Verify Port 1 IRQS[0]=0
  resp = mailbox_read(1, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) != 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQS[0] still set after W1C";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQS[0]=0 (cleared)";
  }

  // Step 13: Verify Port 1 IRQP[0]=0
  resp = mailbox_read(1, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) != 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQP[0] still set after IRQS[0] clear";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQP[0]=0";
  }

  // Step 14-15: irq_o[1] deassertion
  wait(SC_ZERO_TIME);
  {
    bool irq_still = irq_port1_sig.read();
    if (irq_still) {
      REG_ERROR(0, logger) << "FAIL: irq_o[1] still asserted after Port 1 IRQS[0] cleared";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[1]=false (deasserted); Port 1 WTIRQ consistent with Port 0";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC036: test_interrupt_rtirq_port1
// =============================================================================

/**
 * @brief TC028 — Read Threshold Interrupt (RTIRQ) for Port 1.
 *
 * Verification Objective:
 * Symmetric to TC025. Program RIRQT on Port 1, enable IRQEN[1] on Port 1,
 * have Port 0 write data that exceeds Port 1's read-FIFO threshold. Verify
 * IRQS[1], IRQP[1] and irq_o[1] all activate. Clear IRQS[1] and confirm deassert.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write Port 1 RIRQT=3
 *  3. Read  Port 1 RIRQT → verify readback == 3
 *  4. Write Port 1 IRQEN=0x2 (IRQEN[1]=1)
 *  5. Port 0 writes 4 entries (fifo_0_to_1 fill=4 > Port 1 RIRQT=3)
 *  6. Read  Port 1 STATUS   → STATUS[3] must be set
 *  7. Read  Port 1 IRQS     → IRQS[1] must be set
 *  8. Read  Port 1 IRQP     → IRQP[1] must be set
 *  9. wait(SC_ZERO_TIME)
 * 10. Read  irq_port1_sig   → must be true
 * 11. Write Port 1 IRQS=0x2 (W1C)
 * 12. Read  Port 1 IRQS     → IRQS[1] must be 0
 * 13. Read  Port 1 IRQP     → IRQP[1] must be 0
 * 14. wait(SC_ZERO_TIME)
 * 15. Read  irq_port1_sig   → must be false
 *
 * Pass Criteria: All assertions above hold.
 *
 * Related Registers: RIRQT (0x28), IRQEN (0x38), IRQS (0x30), IRQP (0x40),
 *                    STATUS (0x10), WRITE_DATA (0x00)
 * Related Signals: irq_o[1]
 * Test Type: Positive
 */
void testbench::test_interrupt_rtirq_port1()
{
  const std::string test_name = "TC036: test_interrupt_rtirq_port1";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: RTIRQ end-to-end for Port 1 "
                          "(Port 1 RIRQT=3, Port 0 writes 4 entries, verify IRQS/IRQP/irq_o[1], W1C clear)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2-3: Program and verify Port 1 RIRQT
  const uint64_t THRESHOLD = 3;
  REG_INFO(2, logger) << "Step 2: Writing Port 1 RIRQT=" << THRESHOLD;
  resp = mailbox_write(1, mailbox_basetest::RIRQT_OFFSET, THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 RIRQT write rejected";
    test_passed = false;
  }

  REG_INFO(2, logger) << "Step 3: Reading back Port 1 RIRQT (expect " << THRESHOLD << ")";
  resp = mailbox_read(1, mailbox_basetest::RIRQT_OFFSET, reg_val);
  if (reg_val != THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 RIRQT readback mismatch (got 0x" << std::hex << reg_val
        << ", expected 0x" << THRESHOLD << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 RIRQT readback correct (" << THRESHOLD << ")";
  }

  // Step 4: Enable RTIRQ on Port 1 (IRQEN[1]=1)
  REG_INFO(2, logger) << "Step 4: Writing Port 1 IRQEN=0x2 (RTIRQ enable)";
  resp = mailbox_write(1, mailbox_basetest::IRQEN_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQEN write rejected";
    test_passed = false;
  }
  resp = mailbox_read(1, mailbox_basetest::IRQEN_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQEN[1] not set";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQEN[1]=1 confirmed";
  }

  // Step 5: Port 0 writes 4 entries → fifo_0_to_1 fill=4 > Port 1 RIRQT=3
  REG_INFO(2, logger) << "Step 5: Port 0 writing 4 entries (Port 1 read-FIFO fill=4 > RIRQT=3)";
  for (int i = 0; i < 4; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xDD00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Port 0 WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // STATUS[3] uses num_available() which updates after a delta cycle; yield first.
  wait(SC_ZERO_TIME);

  // Step 6: Port 1 STATUS[3]
  REG_INFO(2, logger) << "Step 6: Verifying Port 1 STATUS[3]=1";
  resp = mailbox_read(1, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 STATUS[3] not set (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 STATUS[3]=1 (read_level_above_thresh)";
  }

  // Step 7: Port 1 IRQS[1]
  REG_INFO(2, logger) << "Step 7: Verifying Port 1 IRQS[1]=1";
  resp = mailbox_read(1, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 IRQS[1] not set (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQS[1]=1 (RTIRQ status set)";
  }

  // Step 8: Port 1 IRQP[1]
  REG_INFO(2, logger) << "Step 8: Verifying Port 1 IRQP[1]=1";
  resp = mailbox_read(1, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 1 IRQP[1] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQP[1]=1 (RTIRQ pending)";
  }

  // Step 9-10: irq_o[1] assertion
  wait(SC_ZERO_TIME);
  {
    bool irq_asserted = irq_port1_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[1] not asserted after Port 1 RTIRQ threshold exceeded";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[1]=true (IRQP[1]=1 on Port 1)";
    }
  }

  // Step 11: W1C clear Port 1 IRQS[1]
  REG_INFO(2, logger) << "Step 11: Clearing Port 1 IRQS[1] (write 0x2 W1C)";
  resp = mailbox_write(1, mailbox_basetest::IRQS_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQS W1C write rejected";
    test_passed = false;
  }

  // Step 12: Verify Port 1 IRQS[1]=0
  resp = mailbox_read(1, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) != 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQS[1] still set after W1C";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQS[1]=0 (cleared)";
  }

  // Step 13: Verify Port 1 IRQP[1]=0
  resp = mailbox_read(1, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x2) != 0) {
    REG_ERROR(0, logger) << "FAIL: Port 1 IRQP[1] still set after IRQS[1] clear";
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 1 IRQP[1]=0";
  }

  // Step 14-15: irq_o[1] deassertion
  wait(SC_ZERO_TIME);
  {
    bool irq_still = irq_port1_sig.read();
    if (irq_still) {
      REG_ERROR(0, logger) << "FAIL: irq_o[1] still asserted after Port 1 IRQS[1] cleared";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[1]=false (deasserted); Port 1 RTIRQ consistent with Port 0";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC037: test_threshold_saturation_wirqt
// =============================================================================

/**
 * @brief TC031 — WIRQT saturation logic and retroactive triggering.
 *
 * Verification Objective:
 * Writing a value >= MailboxDepth to WIRQT must saturate to (MailboxDepth-1).
 * The handle_read_WIRQT callback returns the saturated shadow-state value;
 * readback is therefore verifiable. Additionally, lowering WIRQT below the
 * current FIFO usage must immediately set IRQS[0] and STATUS[2] (retroactive
 * triggering inside the write callback).
 *
 * Transaction Sequence:
 *  Part A — Saturation:
 *    1. apply_reset()
 *    2. Write WIRQT=0xFF (255) to Port 0
 *    3. Read  WIRQT Port 0 → expect readback == MailboxDepth-1 (7)
 *  Part B — Retroactive trigger:
 *    4. apply_reset()
 *    5. Fill Port 0 write FIFO to usage=5 (5 writes to WRITE_DATA)
 *    6. Write WIRQT=3 on Port 0 (usage=5 > new_threshold=3)
 *    7. Read  IRQS Port 0   → IRQS[0] must be set immediately
 *    8. Read  STATUS Port 0 → STATUS[2] must be set immediately
 *
 * Pass Criteria:
 * - WIRQT=0xFF saturates to 7 (MailboxDepth-1); readback == 7
 * - Lowering WIRQT below current usage immediately sets IRQS[0] and STATUS[2]
 *
 * Related Registers: WIRQT (0x20), IRQS (0x30), STATUS (0x10), WRITE_DATA (0x00)
 * Test Type: Positive
 */
void testbench::test_threshold_saturation_wirqt()
{
  const std::string test_name = "TC037: test_threshold_saturation_wirqt";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: WIRQT saturation (0xFF→7) and retroactive trigger "
                          "(fill=5, lower threshold=3)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  // ------------------------------------------------------------------
  // Part A: Saturation verification
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part A: Saturation — write WIRQT=0xFF, expect readback=7";
  apply_reset();

  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0xFF);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=0xFF write rejected";
    test_passed = false;
  }

  // handle_read_WIRQT returns m_wirqt_threshold[0] which was saturated to MAX_THRESHOLD
  resp = mailbox_read(0, mailbox_basetest::WIRQT_OFFSET, reg_val);
  if (reg_val != MAX_THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: WIRQT saturation incorrect (got 0x" << std::hex << reg_val
        << ", expected 0x" << MAX_THRESHOLD
        << " = MailboxDepth-1=" << MAILBOX_DEPTH-1 << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: WIRQT=0xFF saturated to " << MAX_THRESHOLD
                         << " (MailboxDepth-1)";
  }

  // ------------------------------------------------------------------
  // Part B: Retroactive trigger
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part B: Retroactive trigger — fill=5, lower WIRQT to 3";
  apply_reset();

  // Fill Port 0 write FIFO to usage=5
  for (int i = 0; i < 5; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WRITE_DATA fill entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 0 write FIFO filled to usage=5";

  // Lower WIRQT to 3 (usage=5 > 3 → retroactive IRQS[0] set inside callback)
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 3);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=3 write rejected";
    test_passed = false;
  }
  REG_INFO(2, logger) << "INFO: WIRQT lowered to 3 (below current usage=5)";

  // IRQS[0] must be set immediately (retroactive trigger in handle_write_WIRQT)
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[0] not set after retroactive WIRQT trigger (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=1 (retroactive WIRQT trigger works)";
  }

  // STATUS[2] must reflect usage > new threshold
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[2] not set after WIRQT lowered (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[2]=1 (write_level_above_thresh after retroactive trigger)";
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC038: test_threshold_saturation_rirqt
// =============================================================================

/**
 * @brief TC032 — RIRQT saturation logic and retroactive triggering.
 *
 * Verification Objective:
 * Writing a value >= MailboxDepth to RIRQT must saturate to (MailboxDepth-1).
 * handle_read_RIRQT returns the saturated shadow-state value; readback verifiable.
 * Additionally, lowering RIRQT below current read-FIFO fill level must immediately
 * set IRQS[1] and STATUS[3].
 *
 * Transaction Sequence:
 *  Part A — Saturation:
 *    1. apply_reset()
 *    2. Write Port 0 RIRQT=200
 *    3. Read  Port 0 RIRQT → expect readback == MailboxDepth-1 (7)
 *  Part B — Retroactive trigger:
 *    4. apply_reset()
 *    5. Port 1 writes 4 entries (fifo_1_to_0 fill=4 = Port 0 read-FIFO)
 *    6. Write Port 0 RIRQT=2 (fill=4 > new_threshold=2)
 *    7. Read  Port 0 IRQS   → IRQS[1] must be set immediately
 *    8. Read  Port 0 STATUS → STATUS[3] must be set immediately
 *
 * Pass Criteria:
 * - RIRQT=200 saturates to 7; readback == 7
 * - Lowering RIRQT below current fill level immediately sets IRQS[1] and STATUS[3]
 *
 * Related Registers: RIRQT (0x28), IRQS (0x30), STATUS (0x10), WRITE_DATA (0x00)
 * Test Type: Positive
 */
void testbench::test_threshold_saturation_rirqt()
{
  const std::string test_name = "TC038: test_threshold_saturation_rirqt";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: RIRQT saturation (200→7) and retroactive trigger "
                          "(Port 1 fill=4, lower Port 0 RIRQT=2)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  // ------------------------------------------------------------------
  // Part A: Saturation verification
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part A: Saturation — write Port 0 RIRQT=200, expect readback=7";
  apply_reset();

  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, 200);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: RIRQT=200 write rejected";
    test_passed = false;
  }

  resp = mailbox_read(0, mailbox_basetest::RIRQT_OFFSET, reg_val);
  if (reg_val != MAX_THRESHOLD) {
    std::ostringstream msg;
    msg << "FAIL: RIRQT saturation incorrect (got 0x" << std::hex << reg_val
        << ", expected 0x" << MAX_THRESHOLD << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: RIRQT=200 saturated to " << MAX_THRESHOLD
                         << " (MailboxDepth-1)";
  }

  // ------------------------------------------------------------------
  // Part B: Retroactive trigger
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part B: Retroactive trigger — Port 1 fill=4, lower Port 0 RIRQT=2";
  apply_reset();

  // Port 1 writes 4 entries → fifo_1_to_0 fill=4 = Port 0 read-FIFO fill
  for (int i = 0; i < 4; i++) {
    resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xBB00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Port 1 WRITE_DATA fill entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 1 filled fifo_1_to_0 to fill=4 (Port 0 read-FIFO)";

  // Lower Port 0 RIRQT to 2 (fill=4 > 2 → retroactive IRQS[1] set in callback)
  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, 2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 0 RIRQT=2 write rejected";
    test_passed = false;
  }
  REG_INFO(2, logger) << "INFO: Port 0 RIRQT lowered to 2 (below current fill=4)";

  // IRQS[1] must be set immediately
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 0 IRQS[1] not set after retroactive RIRQT trigger (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 0 IRQS[1]=1 (retroactive RIRQT trigger works)";
  }

  // STATUS[3] uses num_available() which updates after a delta cycle.
  // Port 1 writes happened in the same delta; yield before reading STATUS.
  wait(SC_ZERO_TIME);

  // STATUS[3] must reflect fill > new threshold
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 0 STATUS[3] not set after RIRQT lowered (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 0 STATUS[3]=1 (read_level_above_thresh)";
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC039: test_threshold_zero_wirqt
// =============================================================================

/**
 * @brief TC033 — Zero WIRQT threshold: any write triggers WTIRQ.
 *
 * Verification Objective:
 * With WIRQT=0, writing a single entry produces usage=1 which is strictly
 * greater than threshold=0. Verify IRQS[0], IRQP[0] and irq_o[0] all assert
 * immediately after the first WRITE_DATA transaction.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write WIRQT=0 on Port 0 (zero threshold; reset already zeroed it,
 *     explicit write confirms the path through handle_write_WIRQT)
 *  3. Write IRQEN=0x1 on Port 0 (WTIRQ enable)
 *  4. Write 1 entry to Port 0 WRITE_DATA (usage=1 > 0)
 *  5. Read  STATUS Port 0    → STATUS[2] must be set
 *  6. Read  IRQS Port 0      → IRQS[0] must be set
 *  7. Read  IRQP Port 0      → IRQP[0] must be set
 *  8. wait(SC_ZERO_TIME)
 *  9. Read  irq_port0_sig    → must be true
 *
 * Pass Criteria: All assertions hold. Zero threshold allows interrupt on first write.
 *
 * Related Registers: WIRQT (0x20), STATUS (0x10), IRQS (0x30), IRQEN (0x38),
 *                    IRQP (0x40), WRITE_DATA (0x00)
 * Related Signals: irq_o[0]
 * Test Type: Positive
 */
void testbench::test_threshold_zero_wirqt()
{
  const std::string test_name = "TC039: test_threshold_zero_wirqt";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Zero WIRQT threshold — any write triggers WTIRQ "
                          "(usage=1 > threshold=0)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2: Write WIRQT=0 (explicit write exercises the callback path)
  REG_INFO(2, logger) << "Step 2: Writing WIRQT=0 to Port 0";
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=0 write rejected";
    test_passed = false;
  }

  // Step 3: Enable WTIRQ
  REG_INFO(2, logger) << "Step 3: Writing IRQEN=0x1 (WTIRQ enable)";
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }

  // Step 4: Single write — usage=1 > threshold=0
  REG_INFO(2, logger) << "Step 4: Writing single entry to WRITE_DATA (usage=1 > WIRQT=0)";
  resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000001ULL);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WRITE_DATA single write rejected";
    test_passed = false;
  }

  // Step 5: STATUS[2]
  REG_INFO(2, logger) << "Step 5: Verifying STATUS[2]=1 (usage=1 > WIRQT=0)";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[2] not set with zero threshold (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[2]=1 (write_level_above_thresh, WIRQT=0)";
  }

  // Step 6: IRQS[0]
  REG_INFO(2, logger) << "Step 6: Verifying IRQS[0]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[0] not set with zero threshold (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=1 (zero threshold allows immediate interrupt)";
  }

  // Step 7: IRQP[0]
  REG_INFO(2, logger) << "Step 7: Verifying IRQP[0]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQP[0] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQP[0]=1 (interrupt pending)";
  }

  // Step 8-9: irq_o[0] assertion
  wait(SC_ZERO_TIME);
  {
    bool irq_asserted = irq_port0_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] not asserted with zero WIRQT threshold";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=true (zero threshold interrupt delivered)";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC040: test_threshold_zero_rirqt
// =============================================================================

/**
 * @brief TC034 — Zero RIRQT threshold: any peer write triggers RTIRQ.
 *
 * Verification Objective:
 * With Port 0 RIRQT=0, Port 1 writing a single entry fills the fifo_1_to_0
 * to level=1 which is strictly greater than threshold=0. Verify IRQS[1],
 * IRQP[1] and irq_o[0] all assert immediately.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write Port 0 RIRQT=0
 *  3. Write Port 0 IRQEN=0x2 (RTIRQ enable)
 *  4. Port 1 writes 1 entry (Port 0 read-FIFO fill=1 > RIRQT=0)
 *  5. Read  Port 0 STATUS  → STATUS[3] must be set
 *  6. Read  Port 0 IRQS    → IRQS[1] must be set
 *  7. Read  Port 0 IRQP    → IRQP[1] must be set
 *  8. wait(SC_ZERO_TIME)
 *  9. Read  irq_port0_sig  → must be true
 *
 * Pass Criteria: All assertions hold. Zero threshold allows interrupt on
 * first data arrival from peer port.
 *
 * Related Registers: RIRQT (0x28), STATUS (0x10), IRQS (0x30), IRQEN (0x38),
 *                    IRQP (0x40), WRITE_DATA (0x00)
 * Related Signals: irq_o[0]
 * Test Type: Positive
 */
void testbench::test_threshold_zero_rirqt()
{
  const std::string test_name = "TC040: test_threshold_zero_rirqt";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Zero RIRQT threshold — any peer write triggers RTIRQ "
                          "(Port 1 fill=1 > Port 0 RIRQT=0)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2: Write Port 0 RIRQT=0
  REG_INFO(2, logger) << "Step 2: Writing Port 0 RIRQT=0";
  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, 0);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: RIRQT=0 write rejected";
    test_passed = false;
  }

  // Step 3: Enable RTIRQ on Port 0
  REG_INFO(2, logger) << "Step 3: Writing Port 0 IRQEN=0x2 (RTIRQ enable)";
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }

  // Step 4: Port 1 writes 1 entry → fifo_1_to_0 fill=1 > Port 0 RIRQT=0
  REG_INFO(2, logger) << "Step 4: Port 1 writing single entry (Port 0 fill=1 > RIRQT=0)";
  resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xBB00000000000001ULL);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 1 WRITE_DATA single write rejected";
    test_passed = false;
  }

  // STATUS[3] uses num_available() which updates after a delta cycle;
  // yield from SC_THREAD to allow sc_fifo's update() to run.
  wait(SC_ZERO_TIME);

  // Step 5: STATUS[3]
  REG_INFO(2, logger) << "Step 5: Verifying Port 0 STATUS[3]=1 (fill=1 > RIRQT=0)";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 0 STATUS[3] not set (STATUS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 0 STATUS[3]=1 (read_level_above_thresh, RIRQT=0)";
  }

  // Step 6: IRQS[1]
  REG_INFO(2, logger) << "Step 6: Verifying Port 0 IRQS[1]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 0 IRQS[1] not set (IRQS=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 0 IRQS[1]=1 (zero threshold allows immediate RTIRQ)";
  }

  // Step 7: IRQP[1]
  REG_INFO(2, logger) << "Step 7: Verifying Port 0 IRQP[1]=1";
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Port 0 IRQP[1] not set (IRQP=0x" << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Port 0 IRQP[1]=1 (interrupt pending)";
  }

  // Step 8-9: irq_o[0] assertion
  wait(SC_ZERO_TIME);
  {
    bool irq_asserted = irq_port0_sig.read();
    if (!irq_asserted) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] not asserted with zero RIRQT threshold";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=true (zero RIRQT threshold interrupt delivered)";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC041: test_threshold_retroactive_trigger
// =============================================================================

/**
 * @brief TC035 — Retroactive threshold triggering for both WIRQT and RIRQT.
 *
 * Verification Objective:
 * Filling the FIFO first and then lowering the threshold below the current
 * level must immediately set the corresponding IRQS bit and STATUS flag
 * within the threshold-write callback — without any additional FIFO access.
 * Tested for both WIRQT (Part 1) and RIRQT (Part 2).
 *
 * Transaction Sequence:
 *  Part 1 — WIRQT retroactive:
 *    1. apply_reset()
 *    2. Port 0 writes 6 entries (usage=6)
 *    3. Write Port 0 IRQEN=0x1
 *    4. Write Port 0 WIRQT=4 (usage=6 > 4 → retroactive trigger)
 *    5. Read  Port 0 IRQS → IRQS[0] must be set
 *    6. Read  Port 0 STATUS → STATUS[2] must be set
 *  Part 2 — RIRQT retroactive:
 *    7. apply_reset()
 *    8. Port 1 writes 5 entries (fifo_1_to_0 fill=5 = Port 0 read-FIFO)
 *    9. Write Port 0 IRQEN=0x2
 *   10. Write Port 0 RIRQT=3 (fill=5 > 3 → retroactive trigger)
 *   11. Read  Port 0 IRQS → IRQS[1] must be set
 *   12. Read  Port 0 STATUS → STATUS[3] must be set
 *
 * Pass Criteria: Both parts pass with no additional FIFO operation after
 *                the threshold write.
 *
 * Related Registers: WIRQT (0x20), RIRQT (0x28), STATUS (0x10), IRQS (0x30),
 *                    IRQEN (0x38), WRITE_DATA (0x00)
 * Test Type: Positive
 */
void testbench::test_threshold_retroactive_trigger()
{
  const std::string test_name = "TC041: test_threshold_retroactive_trigger";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Retroactive threshold triggering for WIRQT (fill=6, lower=4) "
                          "and RIRQT (fill=5, lower=3)";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  // ------------------------------------------------------------------
  // Part 1: WIRQT retroactive trigger
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part 1: WIRQT retroactive trigger (Port 0 usage=6, lower WIRQT to 4)";
  apply_reset();

  for (int i = 0; i < 6; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WIRQT-part WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 0 write FIFO usage=6";

  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected (Part 1)";
    test_passed = false;
  }

  // Lower WIRQT to 4 — retroactive trigger fires inside handle_write_WIRQT
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 4);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=4 write rejected";
    test_passed = false;
  }

  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part 1 — IRQS[0] not set after retroactive WIRQT trigger (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part 1 — IRQS[0]=1 (WIRQT retroactive trigger)";
  }

  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part 1 — STATUS[2] not set after WIRQT lowered (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part 1 — STATUS[2]=1 (write_level_above_thresh)";
  }

  // ------------------------------------------------------------------
  // Part 2: RIRQT retroactive trigger
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part 2: RIRQT retroactive trigger (Port 0 read-FIFO fill=5, lower RIRQT to 3)";
  apply_reset();

  for (int i = 0; i < 5; i++) {
    resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xBB00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: RIRQT-part Port 1 WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 0 read-FIFO (fifo_1_to_0) fill=5";

  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected (Part 2)";
    test_passed = false;
  }

  // Lower Port 0 RIRQT to 3 — retroactive trigger fires inside handle_write_RIRQT
  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, 3);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 0 RIRQT=3 write rejected";
    test_passed = false;
  }

  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part 2 — Port 0 IRQS[1] not set after retroactive RIRQT trigger (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part 2 — Port 0 IRQS[1]=1 (RIRQT retroactive trigger)";
  }

  // STATUS[3] uses num_available() — yield for delta-cycle update before reading.
  wait(SC_ZERO_TIME);

  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part 2 — Port 0 STATUS[3] not set after RIRQT lowered (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part 2 — Port 0 STATUS[3]=1 (read_level_above_thresh)";
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC042: test_boundary_threshold_max_value
// =============================================================================

/**
 * @brief TC053 — Threshold = MailboxDepth-1: interrupt triggers only when FIFO is full.
 *
 * Verification Objective:
 * The maximum valid threshold is (MailboxDepth-1). Setting WIRQT=7 (with depth=8)
 * means only usage=8 (full FIFO) exceeds the threshold. Similarly for RIRQT.
 *
 * Transaction Sequence:
 *  Part A — WIRQT max:
 *    1. apply_reset()
 *    2. Write Port 0 WIRQT=7 (MailboxDepth-1)
 *    3. Enable Port 0 IRQEN[0]=1
 *    4. Write MailboxDepth (8) entries to Port 0 (usage=8 > threshold=7)
 *    5. Read  Port 0 IRQS   → IRQS[0] must be set
 *    6. Read  Port 0 STATUS → STATUS[2] must be set
 *  Part B — RIRQT max:
 *    7. apply_reset()
 *    8. Write Port 0 RIRQT=7
 *    9. Enable Port 0 IRQEN[1]=1
 *   10. Port 1 writes MailboxDepth (8) entries (Port 0 read-FIFO fill=8 > 7)
 *   11. Read  Port 0 IRQS   → IRQS[1] must be set
 *   12. Read  Port 0 STATUS → STATUS[3] must be set
 *
 * Pass Criteria: Maximum threshold allows interrupt only when FIFO reaches capacity.
 *
 * Related Registers: WIRQT (0x20), RIRQT (0x28), STATUS (0x10), IRQS (0x30),
 *                    IRQEN (0x38), WRITE_DATA (0x00)
 * Test Type: Positive (boundary)
 */
void testbench::test_boundary_threshold_max_value()
{
  const std::string test_name = "TC042: test_boundary_threshold_max_value";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Threshold=MailboxDepth-1 (7); interrupt triggers at full FIFO";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  // ------------------------------------------------------------------
  // Part A: WIRQT = MailboxDepth-1
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part A: WIRQT=7 (MailboxDepth-1), write 8 entries (full FIFO)";
  apply_reset();

  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, MAX_THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=7 write rejected";
    test_passed = false;
  }

  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected (Part A)";
    test_passed = false;
  }

  for (int i = 0; i < MAILBOX_DEPTH; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Part A WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 0 write FIFO filled to capacity (usage=" << MAILBOX_DEPTH << ")";

  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part A — IRQS[0] not set when FIFO full with max threshold (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part A — IRQS[0]=1 (max threshold, full FIFO triggers interrupt)";
  }

  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part A — STATUS[2] not set with max threshold (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part A — STATUS[2]=1";
  }

  // ------------------------------------------------------------------
  // Part B: RIRQT = MailboxDepth-1
  // ------------------------------------------------------------------
  REG_INFO(2, logger) << "Part B: Port 0 RIRQT=7, Port 1 writes 8 entries";
  apply_reset();

  resp = mailbox_write(0, mailbox_basetest::RIRQT_OFFSET, MAX_THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: Port 0 RIRQT=7 write rejected";
    test_passed = false;
  }

  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected (Part B)";
    test_passed = false;
  }

  for (int i = 0; i < MAILBOX_DEPTH; i++) {
    resp = mailbox_write(1, mailbox_basetest::WRITE_DATA_OFFSET, 0xBB00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Part B Port 1 WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }
  REG_INFO(2, logger) << "INFO: Port 0 read-FIFO filled to capacity (fill=" << MAILBOX_DEPTH << ")";

  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x2) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part B — Port 0 IRQS[1] not set with max threshold (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part B — Port 0 IRQS[1]=1 (max RIRQT, full read-FIFO)";
  }

  // STATUS[3] uses num_available() which updates after a delta cycle.
  wait(SC_ZERO_TIME);

  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x8) == 0) {
    std::ostringstream msg;
    msg << "FAIL: Part B — Port 0 STATUS[3] not set with max threshold (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: Part B — Port 0 STATUS[3]=1";
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC043: test_boundary_threshold_equal_usage
// =============================================================================

/**
 * @brief TC054 — Strictly-greater-than comparison: usage equal to threshold must NOT trigger.
 *
 * Verification Objective:
 * The hardware uses strictly-greater-than (usage > threshold), not
 * greater-or-equal. With WIRQT=5, writing exactly 5 entries (usage=5) must
 * leave IRQS[0]=0 and STATUS[2]=0. Writing a 6th entry (usage=6 > 5) must
 * then set IRQS[0]=1 and STATUS[2]=1.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write WIRQT=5 on Port 0
 *  3. Enable IRQEN[0]=1 on Port 0
 *  4. Write exactly 5 entries (usage=5 == threshold)
 *  5. Read  IRQS Port 0   → IRQS[0] must be 0 (equal does NOT trigger)
 *  6. Read  STATUS Port 0 → STATUS[2] must be 0
 *  7. Write 6th entry (usage=6 > threshold=5)
 *  8. Read  IRQS Port 0   → IRQS[0] must be 1 (exceeds threshold)
 *  9. Read  STATUS Port 0 → STATUS[2] must be 1
 *
 * Pass Criteria: All assertions hold. Strictly-greater-than confirmed.
 *
 * Related Registers: WIRQT (0x20), STATUS (0x10), IRQS (0x30), IRQEN (0x38),
 *                    WRITE_DATA (0x00)
 * Test Type: Positive (boundary / negative for equal case)
 */
void testbench::test_boundary_threshold_equal_usage()
{
  const std::string test_name = "TC043: test_boundary_threshold_equal_usage";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Strictly-greater-than comparison: usage=5 == WIRQT=5 "
                          "must NOT trigger; usage=6 must trigger";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  const uint64_t THRESHOLD = 5;
  const int      EQUAL_CNT = 5;   ///< Writes equal to threshold (no interrupt expected)

  // Step 2: Write WIRQT=5
  REG_INFO(2, logger) << "Step 2: Writing WIRQT=5";
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, THRESHOLD);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=5 write rejected";
    test_passed = false;
  }

  // Step 3: Enable WTIRQ
  REG_INFO(2, logger) << "Step 3: Writing IRQEN=0x1";
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }

  // Step 4: Write exactly 5 entries (usage == threshold, no interrupt)
  REG_INFO(2, logger) << "Step 4: Writing exactly 5 entries (usage=5, equal to threshold=5)";
  for (int i = 0; i < EQUAL_CNT; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WRITE_DATA equal-count entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // Step 5: IRQS[0] must be 0 (usage NOT > threshold)
  REG_INFO(2, logger) << "Step 5: Verifying IRQS[0]=0 (usage=5 NOT > threshold=5)";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) != 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[0] set when usage equals threshold — should be 0 (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=0 (equal usage does NOT trigger interrupt)";
  }

  // Step 6: STATUS[2] must be 0
  REG_INFO(2, logger) << "Step 6: Verifying STATUS[2]=0";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) != 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[2] set when usage equals threshold (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[2]=0 (threshold not exceeded)";
  }

  // Step 7: Write 6th entry (usage=6 > threshold=5 → IRQS[0] must set now)
  REG_INFO(2, logger) << "Step 7: Writing 6th entry (usage=6 > threshold=5)";
  resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000006ULL);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: 6th WRITE_DATA entry rejected";
    test_passed = false;
  }

  // Step 8: IRQS[0] must now be set
  REG_INFO(2, logger) << "Step 8: Verifying IRQS[0]=1 (usage=6 > threshold=5)";
  resp = mailbox_read(0, mailbox_basetest::IRQS_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    std::ostringstream msg;
    msg << "FAIL: IRQS[0] not set when usage exceeds threshold (IRQS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: IRQS[0]=1 (usage > threshold triggers interrupt)";
  }

  // Step 9: STATUS[2] must now be set
  REG_INFO(2, logger) << "Step 9: Verifying STATUS[2]=1";
  resp = mailbox_read(0, mailbox_basetest::STATUS_OFFSET, reg_val);
  if ((reg_val & 0x4) == 0) {
    std::ostringstream msg;
    msg << "FAIL: STATUS[2] not set when usage exceeds threshold (STATUS=0x"
        << std::hex << reg_val << ")";
    REG_ERROR(0, logger) << msg.str();
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "PASS: STATUS[2]=1 (write_level_above_thresh)";
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC044: test_config_interrupt_level_triggered
// =============================================================================

/**
 * @brief TC058 — Level-triggered interrupt mode verification.
 *
 * Verification Objective:
 * The default DUT is configured with irq_act_high=true (level-triggered).
 * In level-triggered mode irq_o[port] is held at the active level for the
 * entire duration that any IRQP bit is set.  This test exercises the full
 * lifecycle:
 *   - Trigger interrupt → irq_o asserts and stays asserted
 *   - IRQS sticky: adding more FIFO writes does not change irq_o level
 *   - Clearing IRQS → irq_o deasserts immediately
 * Observation uses irq_port0_sig.read() after wait(SC_ZERO_TIME).
 *
 *   mailbox_ip("dut", 0x50, 8, irq_act_high=true)
 * Creating a new SystemC module after start_of_simulation() is not permitted.
 * This test therefore validates level-triggered behaviour using the existing DUT.
 * Edge-triggered DUT validation would require a separate top-level test binary.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write WIRQT=2, IRQEN=0x1 on Port 0
 *  3. Write 3 entries → IRQS[0]=1, IRQP[0]=1
 *  4. wait(SC_ZERO_TIME) ; read irq_port0_sig → must be true (asserted)
 *  5. Write 2 more entries (FIFO now at 5, still > threshold=2)
 *  6. wait(SC_ZERO_TIME) ; read irq_port0_sig → must still be true (level held)
 *  7. Write IRQS=0x1 (W1C clear WTIRQ)
 *  8. wait(SC_ZERO_TIME) ; read irq_port0_sig → must be false (level dropped)
 *
 * Pass Criteria:
 * - irq_o asserts immediately when IRQP becomes non-zero
 * - irq_o holds active level for the full duration IRQP != 0
 * - irq_o deasserts as soon as IRQP returns to 0
 *
 * Related Registers: WIRQT (0x20), IRQEN (0x38), IRQS (0x30), IRQP (0x40),
 *                    WRITE_DATA (0x00)
 * Related Signals: irq_o[0]
 * Test Type: Positive (configuration / functional)
 */
void testbench::test_config_interrupt_level_triggered()
{
  const std::string test_name = "TC044: test_config_interrupt_level_triggered";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Level-triggered irq_o behaviour (default DUT: "
                          "irq_act_high=true)";
  REG_INFO(2, logger) << "NOTE: Level-triggered DUT is the testbench default; edge-triggered "
                          "validation requires a separately elaborated DUT instance.";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t reg_val = 0;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2: Configure Port 0 threshold and enable
  REG_INFO(2, logger) << "Step 2: WIRQT=2, IRQEN[0]=1 on Port 0";
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 2);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=2 write rejected";
    test_passed = false;
  }
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }

  // Step 3: Write 3 entries → usage=3 > WIRQT=2 → IRQS[0]=1, IRQP[0]=1
  REG_INFO(2, logger) << "Step 3: Writing 3 entries (usage=3 > WIRQT=2)";
  for (int i = 0; i < 3; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // Step 4: irq_o[0] must assert
  REG_INFO(2, logger) << "Step 4: wait(SC_ZERO_TIME) then read irq_port0_sig (expect true)";
  wait(SC_ZERO_TIME);
  {
    bool irq_val = irq_port0_sig.read();
    if (!irq_val) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] not asserted after threshold exceeded "
                               "(level-triggered mode)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=true (asserted, level-triggered mode)";
    }
  }

  // Step 5: Write 2 more entries — IRQS[0] sticky, IRQP[0] still set
  REG_INFO(2, logger) << "Step 5: Writing 2 more entries (FIFO usage=5, irq_o should stay asserted)";
  for (int i = 3; i < 5; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: Additional WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // Step 6: irq_o[0] must still be asserted (level held while IRQP[0]=1)
  REG_INFO(2, logger) << "Step 6: wait(SC_ZERO_TIME) then read irq_port0_sig "
                          "(expect still true — level held while IRQP[0] set)";
  wait(SC_ZERO_TIME);
  {
    bool irq_val = irq_port0_sig.read();
    if (!irq_val) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] deasserted unexpectedly while IRQP[0]=1 "
                               "(should hold active level in level-triggered mode)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0] holds active level while IRQP[0]=1 (level-triggered)";
    }
  }

  // Verify IRQP[0] is indeed still set before clearing
  resp = mailbox_read(0, mailbox_basetest::IRQP_OFFSET, reg_val);
  if ((reg_val & 0x1) == 0) {
    REG_ERROR(0, logger) << "FAIL: IRQP[0] unexpectedly 0 before W1C clear";
    test_passed = false;
  }

  // Step 7: W1C clear IRQS[0] → IRQP[0] becomes 0 → irq_o[0] must deassert
  REG_INFO(2, logger) << "Step 7: Clearing IRQS[0] (write 0x1 W1C)";
  resp = mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQS W1C write rejected";
    test_passed = false;
  }

  // Step 8: irq_o[0] must deassert immediately (no IRQP bits set)
  REG_INFO(2, logger) << "Step 8: wait(SC_ZERO_TIME) then read irq_port0_sig "
                          "(expect false — level drops when IRQP=0)";
  wait(SC_ZERO_TIME);
  {
    bool irq_val = irq_port0_sig.read();
    if (irq_val) {
      REG_ERROR(0, logger) << "FAIL: irq_o[0] still asserted after IRQS[0] cleared "
                               "(level-triggered should deassert when IRQP=0)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_o[0]=false (deasserted when IRQP[0] cleared, level-triggered)";
    }
  }

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC045: test_config_interrupt_polarity
// =============================================================================

/**
 * @brief TC060 — Active-high interrupt polarity verification.
 *
 * Verification Objective:
 * The default DUT is constructed with irq_act_high=true (active-high polarity).
 * In active-high mode:
 *   - Inactive state: irq_o = 0 (false)
 *   - Active state:   irq_o = 1 (true)
 * This test confirms both states by triggering an interrupt and then clearing it.
 *
 * NOTE: Active-low polarity verification (irq_act_high=false) requires a DUT
 * constructed with irq_act_high=false at elaboration time.  SystemC modules
 * cannot be instantiated after start_of_simulation(); therefore active-low
 * verification is documented here as a requirement for a separate test binary.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Verify inactive baseline: wait(SC_ZERO_TIME), read irq_port0_sig → must be false
 *  3. Write WIRQT=1, IRQEN=0x1 on Port 0
 *  4. Write 2 entries (usage=2 > 1)
 *  5. wait(SC_ZERO_TIME) ; read irq_port0_sig → must be true (active-high active state)
 *  6. Write IRQS=0x1 (W1C)
 *  7. wait(SC_ZERO_TIME) ; read irq_port0_sig → must be false (active-high inactive state)
 *
 * Pass Criteria:
 * - Inactive irq_o is false (0) in active-high mode
 * - Active irq_o is true (1) in active-high mode
 * - Active-low DUT requires separate elaboration binary
 *
 * Related Registers: WIRQT (0x20), IRQEN (0x38), IRQS (0x30), WRITE_DATA (0x00)
 * Related Signals: irq_o[0], irq_o[1]
 * Test Type: Positive (configuration)
 */
void testbench::test_config_interrupt_polarity()
{
  const std::string test_name = "TC045: test_config_interrupt_polarity";
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "Running: " << test_name;
  REG_INFO(2, logger) << "Description: Active-high polarity verification "
                          "(default DUT: irq_act_high=true)";
  REG_INFO(2, logger) << "NOTE: Active-low polarity (irq_act_high=false) requires a separately "
                          "elaborated DUT; cannot be verified in this testbench binary.";
  REG_INFO(2, logger) << "========================================";

  bool test_passed = true;
  tlm::tlm_response_status resp;

  apply_reset();

  // Step 2: Verify inactive baseline — after reset irq_o must be inactive (false)
  REG_INFO(2, logger) << "Step 2: Verify inactive baseline after reset (irq_port0_sig == false)";
  wait(SC_ZERO_TIME);
  {
    bool baseline = irq_port0_sig.read();
    if (baseline != false) {
      REG_ERROR(0, logger) << "FAIL: irq_port0_sig is not false at inactive baseline "
                               "(active-high: inactive=0)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_port0_sig=false at inactive baseline (active-high inactive=0)";
    }
  }

  // Also verify Port 1 irq is inactive
  {
    bool baseline1 = irq_port1_sig.read();
    if (baseline1 != false) {
      REG_ERROR(0, logger) << "FAIL: irq_port1_sig is not false at inactive baseline";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_port1_sig=false at inactive baseline";
    }
  }

  // Step 3: Configure Port 0 for an easy interrupt trigger
  REG_INFO(2, logger) << "Step 3: Write WIRQT=1, IRQEN[0]=1 on Port 0";
  resp = mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: WIRQT=1 write rejected";
    test_passed = false;
  }
  resp = mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQEN write rejected";
    test_passed = false;
  }

  // Step 4: Write 2 entries (usage=2 > WIRQT=1)
  REG_INFO(2, logger) << "Step 4: Writing 2 entries (usage=2 > WIRQT=1)";
  for (int i = 0; i < 2; i++) {
    resp = mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xAA00000000000000ULL | i);
    if (resp != tlm::TLM_OK_RESPONSE) {
      std::ostringstream msg;
      msg << "FAIL: WRITE_DATA entry " << i << " rejected";
      REG_ERROR(0, logger) << msg.str();
      test_passed = false;
    }
  }

  // Step 5: Active state — irq_o must be true (active-high active=1)
  REG_INFO(2, logger) << "Step 5: wait(SC_ZERO_TIME), read irq_port0_sig "
                          "(expect true — active-high active state=1)";
  wait(SC_ZERO_TIME);
  {
    bool active_val = irq_port0_sig.read();
    if (active_val != true) {
      REG_ERROR(0, logger) << "FAIL: irq_port0_sig not true in active state "
                               "(active-high active=1 expected)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_port0_sig=true (active-high active state=1 confirmed)";
    }
  }

  // Step 6: W1C clear
  REG_INFO(2, logger) << "Step 6: Clearing IRQS[0] (write 0x1 W1C)";
  resp = mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x1);
  if (resp != tlm::TLM_OK_RESPONSE) {
    REG_ERROR(0, logger) << "FAIL: IRQS W1C write rejected";
    test_passed = false;
  }

  // Step 7: Inactive state — irq_o must return to false (active-high inactive=0)
  REG_INFO(2, logger) << "Step 7: wait(SC_ZERO_TIME), read irq_port0_sig "
                          "(expect false — active-high inactive state=0)";
  wait(SC_ZERO_TIME);
  {
    bool inactive_val = irq_port0_sig.read();
    if (inactive_val != false) {
      REG_ERROR(0, logger) << "FAIL: irq_port0_sig not false after clear "
                               "(active-high inactive=0 expected)";
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "PASS: irq_port0_sig=false (active-high inactive state=0 confirmed)";
    }
  }

  REG_INFO(2, logger) << "INFO: Active-high polarity fully verified. "
                          "Inactive=0, Active=1 both confirmed via irq_port0_sig.";
  REG_INFO(2, logger) << "INFO: Active-low polarity requires a separately elaborated DUT "
                          "(irq_act_high=false); not verified in this testbench binary.";

  REG_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// FUNC-006 Test Suite Entry Point
// =============================================================================

/**
 * @brief Run all FUNC-006 test cases.
 *
 * Orchestrates the complete FUNC-006 verification suite:
 * Programmable Threshold-Based Interrupt System.
 *
 */
void testbench::run_func006_tests()
{
  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "FUNC-006: Programmable Threshold-Based Interrupt System";
  REG_INFO(2, logger) << "========================================";


  // Test IDs 33-36: Interrupt type end-to-end tests
  test_interrupt_wtirq_port0();               // TC033 [this file]
  test_interrupt_rtirq_port0();               // TC034 [this file]
  test_interrupt_wtirq_port1();               // TC035 [this file]
  test_interrupt_rtirq_port1();               // TC036 [this file]

  // Test IDs 37-41: Threshold configuration tests
  test_threshold_saturation_wirqt();          // TC037 [this file]
  test_threshold_saturation_rirqt();          // TC038 [this file]
  test_threshold_zero_wirqt();                // TC039 [this file]
  test_threshold_zero_rirqt();                // TC040 [this file]
  test_threshold_retroactive_trigger();       // TC041 [this file]

  // Test IDs 42-43: Boundary case tests
  test_boundary_threshold_max_value();        // TC042 [this file]
  test_boundary_threshold_equal_usage();      // TC043 [this file]

  // Test IDs 44-45: Configuration tests
  test_config_interrupt_level_triggered();    // TC044 [this file]
  test_config_interrupt_polarity();           // TC045 [this file]

  REG_INFO(2, logger) << "========================================";
  REG_INFO(2, logger) << "FUNC-006 Test Suite Complete";
  REG_INFO(2, logger) << "========================================";
}
