/**
 * @file test_mailbox_func002.cpp
 * @brief FUNC-002: Register Callback Verification — Test Suite
 *
 * Implements four callback-level test cases that verify register write
 * callback behaviour not covered by the basic register-access tests in
 * testbench.cpp:
 *
 *   TC007 – test_callback_irqs_write1clear
 *     W1C semantics: write-0 is a no-op; write-1 atomically clears the
 *     sticky bit and irq_o tracks the change.
 *
 *   TC008 – test_callback_irqen_masking
 *     Retroactive IRQP recomputation when IRQEN toggles while IRQS bits
 *     remain sticky.
 *
 *   TC009 – test_callback_wirqt_saturation_immediate
 *     WIRQT saturation enforcement and immediate (retroactive) threshold
 *     re-evaluation on write.
 *
 *   TC010 – test_callback_rirqt_saturation_immediate
 *     Same as TC009 for RIRQT / Port 1 read FIFO.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// TC007 – IRQS write-1-to-clear callback semantics
// =============================================================================

/**
 * @brief TC007 — IRQS Register Write-1-to-Clear Callback Verification.
 *
 * Verification Objective:
 * Verify that the IRQS write callback correctly implements write-1-to-clear
 * (W1C) semantics: writing 0 has no effect on set bits, while writing 1
 * atomically clears the corresponding sticky status bit. Also confirms that
 * IRQP and irq_o[0] update immediately after IRQS is cleared.
 *
 * Transaction Sequence:
 *  1. apply_reset()
 *  2. Write IRQEN=0x1 on Port 0   (enable WTIRQ only)
 *  3. Write WIRQT=2 on Port 0     (threshold = 2)
 *  4. Write 3 entries to Port 0 WRITE_DATA  (usage=3 > 2 → IRQS[0] should set)
 *  5. wait(1, SC_NS) + re-write WIRQT=2    (retroactive check triggers IRQS[0])
 *  6. wait(SC_ZERO_TIME)          → verify irq_o[0] asserted
 *  7. Write 0x0 to IRQS           (write-0 has no effect)
 *  8. Read IRQS                   → verify IRQS[0] still 1
 *  9. Write 0x1 to IRQS           (write-1-to-clear IRQS[0])
 * 10. Read IRQS                   → verify IRQS[0] == 0
 * 11. Read IRQP                   → verify IRQP[0] == 0
 * 12. wait(SC_ZERO_TIME)          → verify irq_o[0] deasserted
 * 13. Write 0x2 to IRQS           (attempt to clear IRQS[1] which is not set)
 * 14. Read IRQS                   → verify IRQS still 0 (no regression)
 *
 * Pass Criteria:
 * - Writing 0 to IRQS does not clear any bits
 * - Writing 1 to IRQS[0] clears IRQS[0]
 * - IRQP reflects updated IRQS after clear
 * - irq_o[0] deasserts once all IRQP bits are zero
 */
void testbench::test_callback_irqs_write1clear()
{
  std::string test_name = "TC007: test_callback_irqs_write1clear";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify IRQS write-1-to-clear callback";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t irqs_value = 0;
  uint64_t irqp_value = 0;

  apply_reset();

  // Step 1: Enable WTIRQ, set WIRQT=2, write 3 entries (usage=3 > 2 → IRQS[0])
  CSML_INFO(2, logger) << "Step 1: Configure IRQEN=0x1, WIRQT=2, write 3 entries";
  mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0x2);
  for (int i = 0; i < 3; i++)
    mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xA000 + i);

  // sc_fifo delta settle + retroactive threshold re-check via re-write
  wait(1, SC_NS);
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0x2);

  // Step 2: Verify IRQS[0]=1 and irq_o[0] asserted
  CSML_INFO(2, logger) << "Step 2: Verify IRQS[0]=1 and irq_o[0] asserted";
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] not set after write threshold exceeded";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0] set by hardware condition";
  }

  wait(SC_ZERO_TIME);
  if (!irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] not asserted when IRQP[0]=1";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] asserted";
  }

  // Step 3: Write 0 to IRQS — must have NO effect (W1C: writing 0 is no-op)
  CSML_INFO(2, logger) << "Step 3: Write 0 to IRQS - verify no effect";
  mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x0);
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] incorrectly cleared by writing 0 (W1C violation)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0] unaffected by writing 0";
  }

  // Step 4: Write 1 to IRQS[0] — must atomically clear the sticky bit
  CSML_INFO(2, logger) << "Step 4: Write 0x1 to IRQS - clear IRQS[0]";
  mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x1);
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] not cleared after writing 1 (W1C failed)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0] cleared by writing 1";
  }

  // Step 5: Verify IRQP[0]=0 after IRQS[0] cleared
  CSML_INFO(2, logger) << "Step 5: Verify IRQP[0] cleared";
  mailbox_read(0, mailbox_basetest::IRQP_OFFSET, irqp_value);
  if ((irqp_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQP[0] not zero after IRQS[0] cleared";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQP[0]=0 after IRQS[0] cleared";
  }

  // Step 6: Verify irq_o[0] deasserts
  CSML_INFO(2, logger) << "Step 6: Verify irq_o[0] deasserted";
  wait(SC_ZERO_TIME);
  if (irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] still asserted after IRQS[0] cleared";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] deasserted";
  }

  // Step 7: W1C on already-cleared bit — must remain 0
  CSML_INFO(2, logger) << "Step 7: Write 0x2 to IRQS (IRQS[1] not set) - no regression";
  mailbox_write(0, mailbox_basetest::IRQS_OFFSET, 0x2);
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x7) != 0x0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS unexpectedly non-zero after W1C on unset bit";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS remains 0 after W1C on already-cleared bit";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC008 – IRQEN dynamic masking & retroactive IRQP recomputation
// =============================================================================

/**
 * @brief TC008 — IRQEN Write Callback: Dynamic Interrupt Masking and
 *                Retroactive IRQP Recomputation.
 *
 * Verification Objective:
 * Verify that writing IRQEN triggers immediate recomputation of IRQP
 * (IRQP = IRQS & IRQEN). When IRQEN[0] is enabled while IRQS[0] is already
 * set, IRQP[0] must assert immediately (retroactive enable). When IRQEN[0] is
 * cleared, IRQP[0] must deassert even though IRQS[0] remains sticky.
 * irq_o[0] must track IRQP state after each delta cycle.
 */
void testbench::test_callback_irqen_masking()
{
  std::string test_name = "TC008: test_callback_irqen_masking";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify IRQEN masking + retroactive IRQP update";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t irqs_value = 0;
  uint64_t irqp_value = 0;

  apply_reset();

  // Step 1: Set WIRQT=1, write 2 entries, trigger IRQS[0] via retroactive check
  CSML_INFO(2, logger) << "Step 1: Set WIRQT=1, write 2 entries, trigger IRQS[0]";
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0x1);
  for (int i = 0; i < 2; i++)
    mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xB000 + i);
  wait(1, SC_NS);
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0x1);

  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] not set (prerequisite failed)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0]=1 confirmed as prerequisite";
  }

  // Step 2: IRQEN=0 (default) → IRQP[0]=0, irq_o[0] not asserted
  CSML_INFO(2, logger) << "Step 2: Verify IRQP[0]=0 and irq_o[0] not asserted with IRQEN=0";
  mailbox_read(0, mailbox_basetest::IRQP_OFFSET, irqp_value);
  if ((irqp_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQP[0] set when IRQEN[0]=0";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQP[0]=0 while IRQEN[0]=0 (correct masking)";
  }
  wait(SC_ZERO_TIME);
  if (irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] asserted while IRQEN=0";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] not asserted with IRQEN=0";
  }

  // Step 3: Enable IRQEN[0]=1 — retroactive enable while IRQS[0]=1
  CSML_INFO(2, logger) << "Step 3: Write IRQEN=0x1 (retroactive enable while IRQS[0]=1)";
  mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  mailbox_read(0, mailbox_basetest::IRQP_OFFSET, irqp_value);
  if ((irqp_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQP[0] not set after IRQEN[0] enabled with IRQS[0]=1";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQP[0]=1 after retroactive IRQEN[0] enable";
  }
  wait(SC_ZERO_TIME);
  if (!irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] not asserted after IRQEN[0] enabled";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] asserted after IRQEN[0] enabled";
  }

  // Step 4: Disable IRQEN[0]=0 — IRQP must clear, IRQS[0] must remain sticky
  CSML_INFO(2, logger) << "Step 4: Write IRQEN=0x0 (mask while IRQS[0]=1)";
  mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x0);
  mailbox_read(0, mailbox_basetest::IRQP_OFFSET, irqp_value);
  if ((irqp_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQP[0] not cleared after IRQEN[0] disabled";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQP[0]=0 after IRQEN[0] disabled";
  }
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] cleared by IRQEN change (sticky violation)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0] remains sticky despite IRQEN=0";
  }
  wait(SC_ZERO_TIME);
  if (irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] still asserted after IRQEN[0] disabled";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] deasserted after IRQEN[0] disabled";
  }

  // Step 5: Re-enable IRQEN[0]=1 — IRQP and irq_o must reassert
  CSML_INFO(2, logger) << "Step 5: Re-enable IRQEN[0]=1 - verify IRQP[0] and irq_o reassert";
  mailbox_write(0, mailbox_basetest::IRQEN_OFFSET, 0x1);
  mailbox_read(0, mailbox_basetest::IRQP_OFFSET, irqp_value);
  if ((irqp_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQP[0] not set after re-enabling IRQEN[0]";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQP[0]=1 after re-enabling IRQEN[0]";
  }
  wait(SC_ZERO_TIME);
  if (!irq_port0_sig.read()) {
    CSML_ERROR(0, logger) << "FAIL: irq_o[0] not reasserted after IRQEN[0] re-enabled";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: irq_o[0] reasserted correctly";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC009 – WIRQT saturation + immediate retroactive threshold trigger
// =============================================================================

/**
 * @brief TC009 — WIRQT Write Callback: Saturation Enforcement and
 *                Immediate Threshold Re-evaluation.
 *
 * Verification Objective:
 * (1) Saturation: values >= MailboxDepth (8) are stored as MailboxDepth-1 (7).
 * (2) Immediate comparison: writing a new threshold triggers an immediate
 *     check against current FIFO write-side fill level; if fill > new_threshold
 *     then IRQS[0] is set retroactively.
 */
void testbench::test_callback_wirqt_saturation_immediate()
{
  std::string test_name = "TC009: test_callback_wirqt_saturation_immediate";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify WIRQT saturation + immediate threshold comparison";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t read_value  = 0;
  uint64_t irqs_value  = 0;
  uint64_t status_val  = 0;

  const uint64_t SATURATED_MAX = 7;  // MailboxDepth(8) - 1

  apply_reset();

  // Step 1: Write WIRQT=0xFF → expect saturation to 7
  CSML_INFO(2, logger) << "Step 1: Write WIRQT=0xFF - verify saturation to 7";
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0xFF);
  mailbox_read(0, mailbox_basetest::WIRQT_OFFSET, read_value);
  if ((read_value & 0xFF) != SATURATED_MAX) {
    CSML_ERROR(0, logger) << "FAIL: WIRQT saturation incorrect (got " << (read_value & 0xFF) << ", expected 7)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: WIRQT saturated to 7";
  }

  // Step 2: IRQS[0] must be 0 (empty FIFO, 0 > 7 is false)
  CSML_INFO(2, logger) << "Step 2: Verify IRQS[0]=0 (empty FIFO, 0 not > 7)";
  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] set unexpectedly with empty FIFO";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0]=0 with empty FIFO";
  }

  // Step 3: Write 5 entries (fill=5, threshold=7) — 5>7 is false → no IRQ
  CSML_INFO(2, logger) << "Step 3: Write 5 entries (fill=5, threshold=7) - verify no interrupt";
  for (int i = 0; i < 5; i++)
    mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xC000 + i);
  wait(1, SC_NS);

  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] set when fill=5, threshold=7 (5 not > 7)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0]=0 (fill=5 not > threshold=7)";
  }

  // Step 4: Lower WIRQT=4 (fill=5 > 4 is true) → retroactive IRQS[0] must fire
  CSML_INFO(2, logger) << "Step 4: Write WIRQT=4 (fill=5 > 4) - verify retroactive IRQS[0]";
  mailbox_write(0, mailbox_basetest::WIRQT_OFFSET, 0x4);

  mailbox_read(0, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x1) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[0] not set after WIRQT lowered to 4 (fill=5 > 4)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[0]=1 - retroactive trigger fired correctly";
  }

  mailbox_read(0, mailbox_basetest::STATUS_OFFSET, status_val);
  if (!((status_val >> 2) & 0x1)) {
    CSML_ERROR(0, logger) << "FAIL: STATUS[2] not set after WIRQT lowered to 4";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: STATUS[2]=1 (write_level_above_thresh)";
  }

  // Step 5: Verify WIRQT reads back as 4
  CSML_INFO(2, logger) << "Step 5: Verify WIRQT reads back as 4";
  mailbox_read(0, mailbox_basetest::WIRQT_OFFSET, read_value);
  if ((read_value & 0xFF) != 0x4) {
    CSML_ERROR(0, logger) << "FAIL: WIRQT readback incorrect (got " << (read_value & 0xFF) << ")";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: WIRQT reads back as 4";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC010 – RIRQT saturation + immediate retroactive threshold trigger
// =============================================================================

/**
 * @brief TC010 — RIRQT Write Callback: Saturation Enforcement and
 *                Immediate Threshold Re-evaluation on Read FIFO.
 *
 * Verification Objective:
 * Same as TC009 but for RIRQT on Port 1's read FIFO (fifo_0_to_1, filled by
 * Port 0 writes).
 */
void testbench::test_callback_rirqt_saturation_immediate()
{
  std::string test_name = "TC010: test_callback_rirqt_saturation_immediate";
  CSML_INFO(2, logger) << "========================================";
  CSML_INFO(2, logger) << "Running: " << test_name;
  CSML_INFO(2, logger) << "Description: Verify RIRQT saturation + immediate threshold comparison";
  CSML_INFO(2, logger) << "========================================";

  bool test_passed = true;
  uint64_t read_value  = 0;
  uint64_t irqs_value  = 0;
  uint64_t status_val  = 0;

  const uint64_t SATURATED_MAX = 7;  // MailboxDepth(8) - 1

  apply_reset();

  // Step 1: Write RIRQT=0xFF on Port 1 → expect saturation to 7
  CSML_INFO(2, logger) << "Step 1: Write RIRQT=0xFF on Port 1 - verify saturation to 7";
  mailbox_write(1, mailbox_basetest::RIRQT_OFFSET, 0xFF);
  mailbox_read(1, mailbox_basetest::RIRQT_OFFSET, read_value);
  if ((read_value & 0xFF) != SATURATED_MAX) {
    CSML_ERROR(0, logger) << "FAIL: RIRQT saturation incorrect on Port 1 (got " << (read_value & 0xFF) << ")";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: RIRQT on Port 1 saturated to 7";
  }

  // Step 2: IRQS[1] on Port 1 must be 0 (empty FIFO)
  CSML_INFO(2, logger) << "Step 2: Verify IRQS[1]=0 on Port 1 (empty read FIFO)";
  mailbox_read(1, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[1] set unexpectedly with empty Port 1 read FIFO";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[1]=0 on Port 1 with empty read FIFO";
  }

  // Step 3: Port 0 writes 5 entries (fifo_0_to_1 fill=5, threshold=7) → no IRQ
  CSML_INFO(2, logger) << "Step 3: Port 0 writes 5 entries (fill=5, threshold=7) - no interrupt on Port 1";
  for (int i = 0; i < 5; i++)
    mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xD000 + i);
  wait(1, SC_NS);

  mailbox_read(1, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x2) != 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[1] set when Port 1 read fill=5, threshold=7";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[1]=0 on Port 1 (fill=5 not > threshold=7)";
  }

  // Step 4: Lower RIRQT=3 on Port 1 (fill=5 > 3 is true) → retroactive IRQS[1]
  CSML_INFO(2, logger) << "Step 4: Write RIRQT=3 on Port 1 (fill=5 > 3) - verify retroactive IRQS[1]";
  mailbox_write(1, mailbox_basetest::RIRQT_OFFSET, 0x3);

  mailbox_read(1, mailbox_basetest::IRQS_OFFSET, irqs_value);
  if ((irqs_value & 0x2) == 0) {
    CSML_ERROR(0, logger) << "FAIL: IRQS[1] not set after RIRQT lowered to 3 on Port 1 (fill=5 > 3)";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: IRQS[1]=1 on Port 1 - retroactive trigger fired correctly";
  }

  mailbox_read(1, mailbox_basetest::STATUS_OFFSET, status_val);
  if (!((status_val >> 3) & 0x1)) {
    CSML_ERROR(0, logger) << "FAIL: STATUS[3] not set on Port 1 after RIRQT lowered to 3";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: STATUS[3]=1 on Port 1 (read_level_above_thresh)";
  }

  // Step 5: Verify RIRQT reads back as 3 on Port 1
  CSML_INFO(2, logger) << "Step 5: Verify RIRQT reads back as 3 on Port 1";
  mailbox_read(1, mailbox_basetest::RIRQT_OFFSET, read_value);
  if ((read_value & 0xFF) != 0x3) {
    CSML_ERROR(0, logger) << "FAIL: RIRQT readback incorrect on Port 1 (got " << (read_value & 0xFF) << ")";
    test_passed = false;
  } else {
    CSML_INFO(2, logger) << "PASS: RIRQT reads back as 3 on Port 1";
  }

  CSML_INFO(2, logger) << "========================================";
  report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// FUNC-002 suite runner (unique callback-level tests only)
// =============================================================================

void testbench::run_func002_comprehensive_tests() {
  CSML_INFO(2, logger) << "\n========================================";
  CSML_INFO(2, logger) << "FUNC-002 Callback-Level Tests (TC007-TC010)";
  CSML_INFO(2, logger) << "========================================\n";

  apply_reset();
  wait(20, SC_NS);

  test_callback_irqs_write1clear();
  test_callback_irqen_masking();
  test_callback_wirqt_saturation_immediate();
  test_callback_rirqt_saturation_immediate();

  CSML_INFO(2, logger) << "\n========================================";
  CSML_INFO(2, logger) << "FUNC-002 Callback Tests COMPLETED (TC007-TC010)";
  CSML_INFO(2, logger) << "========================================\n";
}
