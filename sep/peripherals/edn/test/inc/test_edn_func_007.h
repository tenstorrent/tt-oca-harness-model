// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_007.h
 * @brief EDN_FUNC_007 Test Suite - FIFO Overflow Detection and Handling
 *
 * Comprehensive test suite for EDN_FUNC_007 (FIFO Overflow Detection and Handling) covering:
 * - RESEED_CMD FIFO 13-word depth boundary testing
 * - GENERATE_CMD FIFO 13-word depth boundary testing
 * - FIFO overflow detection and error code setting (SFIFO_RESCMD_ERR bit 0, SFIFO_GENCMD_ERR bit 1, FIFO_WRITE_ERR bit 28)
 * - Fatal alert assertion on overflow (alert_fatal_alert)
 * - Fatal error interrupt generation on overflow (intr_edn_fatal_err)
 * - State machine transition to Error state (0x47) on fatal conditions
 * - ERR_CODE register sticky behavior (read-only until reset)
 * - FIFO clearing on reset
 * - Edge cases (write at exactly 13 words, multiple overflows, consecutive writes)
 *
 * Test Coverage (10 test cases):
 * - T1:  RESEED_CMD FIFO accepts exactly 13 words without overflow
 * - T2:  RESEED_CMD FIFO 14th word triggers overflow error (SFIFO_RESCMD_ERR bit 0)
 * - T3:  GENERATE_CMD FIFO accepts exactly 13 words without overflow
 * - T4:  GENERATE_CMD FIFO 14th word triggers overflow error (SFIFO_GENCMD_ERR bit 1)
 * - T5:  FIFO_WRITE_ERR bit 28 set on overflow (generic write error indicator)
 * - T6:  Fatal alert (alert_fatal_alert) asserted on FIFO overflow
 * - T7:  Fatal error interrupt (intr_edn_fatal_err) generated on overflow
 * - T8:  State machine transitions to Error state (MAIN_SM_STATE = 0x47) on fatal error
 * - T9:  ERR_CODE register sticky behavior (cannot be cleared by software, only reset)
 * - T10: FIFO clearing on reset (overflow errors cleared, FIFOs emptied)
 *
 * @note This test suite implements all 10 test cases mapped to EDN_FUNC_007
 *       in the edn-functionality-testcases.md document (test cases #92, #93, #99-102, #81).
 *
 * FIFO Architecture Details:
 * - Both RESEED_CMD and GENERATE_CMD FIFOs have 13-word (32-bit) depth
 * - Writes to FIFO registers are write-only (no read-back capability)
 * - FIFO overflow occurs on 14th word write
 * - Overflow triggers fatal error condition (unrecoverable without reset)
 * - ERR_CODE bits are sticky (hardware sets, only reset clears)
 * - Fatal alert and interrupt remain asserted until reset
 *
 * Error Code Bit Mapping:
 * - ERR_CODE[0]  = SFIFO_RESCMD_ERR: Reseed command FIFO overflow
 * - ERR_CODE[1]  = SFIFO_GENCMD_ERR: Generate command FIFO overflow
 * - ERR_CODE[28] = FIFO_WRITE_ERR: Generic FIFO write error (bits [1:0] indicate source)
 *
 * Implementation Strategy:
 * - Boundary testing: Verify 13-word acceptance and 14-word rejection
 * - Error propagation: Validate error code, interrupt, alert, state machine transitions
 * - Sticky behavior: Confirm software cannot clear ERR_CODE (write attempts ignored)
 * - Reset verification: Validate complete error state clearing on reset
 * - Multiple FIFO testing: Ensure independent overflow detection for both FIFOs
 * - Edge cases: Test consecutive overflows, mixed FIFO operations
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>

/**
 * @class test_edn_func_007
 * @brief Test fixture for EDN_FUNC_007 verification
 *
 * Extends edn_test to provide comprehensive FIFO overflow detection and handling testing.
 * Implements all 10 test cases for EDN_FUNC_007 covering boundary conditions, error codes,
 * fatal alerts, interrupts, state machine transitions, and reset behavior.
 */
class test_edn_func_007 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_007(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_007();

    /**
     * @brief Execute all EDN_FUNC_007 test cases
     * @return Number of failed tests
     *
     * Runs all 10 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: RESEED_CMD FIFO 13-Word Boundary
    // =========================================================================
    /**
     * @brief Verify RESEED_CMD FIFO accepts exactly 13 words without overflow
     *
     * Test Objective: Validate that RESEED_CMD FIFO (write-only register at
     * offset 0x2C) accepts exactly 13 consecutive 32-bit word writes without
     * triggering overflow error condition.
     *
     * Test Plan Reference: Test case #99 (test_error_reseed_fifo_13word_boundary)
     * Functionality: EDN_FUNC_007 (FIFO Overflow Detection and Handling)
     *
     * Procedure:
     * 1. Apply reset to clear any previous state
     * 2. Read ERR_CODE register to confirm 0x0 (no errors)
     * 3. Read MAIN_SM_STATE register to confirm Idle state (0xC1)
     * 4. Write 13 consecutive words to RESEED_CMD register (offset 0x2C)
     *    - Use distinct values (0x00000001 through 0x0000000D) for traceability
     * 5. Wait 10ns for error detection logic
     * 6. Read ERR_CODE register and verify all bits = 0 (no overflow)
     * 7. Read MAIN_SM_STATE and verify still in Idle or SWPortMode (not Error 0x47)
     * 8. Verify alert_fatal_alert signal not asserted
     * 9. Verify intr_edn_fatal_err interrupt not asserted
     *
     * Pass Criteria:
     * - All 13 writes accepted without error
     * - ERR_CODE register remains 0x0
     * - ERR_CODE[0] (SFIFO_RESCMD_ERR) = 0
     * - ERR_CODE[28] (FIFO_WRITE_ERR) = 0
     * - alert_fatal_alert signal = 0 (not asserted)
     * - intr_edn_fatal_err interrupt = 0 (not asserted)
     * - MAIN_SM_STATE ≠ 0x47 (not in Error state)
     *
     * @return true if test passes, false otherwise
     */
    bool test_reseed_fifo_13word_boundary();

    // =========================================================================
    // Test Case 2: RESEED_CMD FIFO 14-Word Overflow
    // =========================================================================
    /**
     * @brief Verify RESEED_CMD FIFO 14th word triggers overflow error
     *
     * Test Objective: Validate that writing a 14th word to RESEED_CMD FIFO
     * triggers overflow detection, setting SFIFO_RESCMD_ERR (bit 0) and
     * FIFO_WRITE_ERR (bit 28) in ERR_CODE register, asserting fatal alert
     * and interrupt, and transitioning state machine to Error state.
     *
     * Test Plan Reference: Test case #100 (test_error_reseed_fifo_14word_overflow)
     * Functionality: EDN_FUNC_007
     *
     * Procedure:
     * 1. Apply reset
     * 2. Enable fatal error interrupt (write INTR_ENABLE[1] = 1)
     * 3. Write 13 words to RESEED_CMD FIFO (verify no error)
     * 4. Write 14th word to RESEED_CMD (trigger overflow)
     * 5. Wait 10ns for error detection and propagation
     * 6. Read ERR_CODE register:
     *    - Verify bit [0] (SFIFO_RESCMD_ERR) = 1
     *    - Verify bit [28] (FIFO_WRITE_ERR) = 1
     *    - Optional: Check bits [29:30] remain 0 (not read/state errors)
     * 7. Read INTR_STATE register:
     *    - Verify bit [1] (edn_fatal_err) = 1
     * 8. Monitor alert_fatal_alert signal:
     *    - Verify signal asserted (= 1)
     * 9. Monitor intr_edn_fatal_err signal:
     *    - Verify interrupt asserted (= 1)
     * 10. Read MAIN_SM_STATE register:
     *     - Verify state = 0x47 (Error state)
     * 11. Attempt to clear ERR_CODE by writing 0x0 (verify sticky behavior)
     * 12. Attempt to clear INTR_STATE by writing 0x2 (W1C) - interrupt clears but ERR_CODE sticky
     * 13. Verify alert_fatal_alert remains asserted (sticky until reset)
     *
     * Pass Criteria:
     * - 14th write triggers overflow detection
     * - ERR_CODE[0] = 1 (SFIFO_RESCMD_ERR)
     * - ERR_CODE[28] = 1 (FIFO_WRITE_ERR)
     * - INTR_STATE[1] = 1 (edn_fatal_err)
     * - alert_fatal_alert = 1 (asserted)
     * - intr_edn_fatal_err = 1 (asserted)
     * - MAIN_SM_STATE = 0x47 (Error state)
     * - ERR_CODE sticky (write attempts have no effect)
     * - alert_fatal_alert sticky (remains asserted until reset)
     *
     * @return true if test passes, false otherwise
     */
    bool test_reseed_fifo_14word_overflow();

    // =========================================================================
    // Test Case 3: GENERATE_CMD FIFO 13-Word Boundary
    // =========================================================================
    /**
     * @brief Verify GENERATE_CMD FIFO accepts exactly 13 words without overflow
     *
     * Test Objective: Validate that GENERATE_CMD FIFO (write-only register at
     * offset 0x30) accepts exactly 13 consecutive 32-bit word writes without
     * triggering overflow error condition.
     *
     * Test Plan Reference: Test case #101 (test_error_generate_fifo_13word_boundary)
     * Functionality: EDN_FUNC_007
     *
     * Procedure:
     * 1. Apply reset
     * 2. Verify initial state (ERR_CODE = 0x0, MAIN_SM_STATE = Idle 0xC1)
     * 3. Write 13 consecutive words to GENERATE_CMD register (offset 0x30)
     *    - Use distinct values (0x10000001 through 0x1000000D) for traceability
     * 4. Wait 10ns for error detection
     * 5. Read ERR_CODE and verify 0x0 (no overflow)
     * 6. Read MAIN_SM_STATE and verify not in Error state (≠ 0x47)
     * 7. Verify alert_fatal_alert = 0 (not asserted)
     * 8. Verify intr_edn_fatal_err = 0 (not asserted)
     *
     * Pass Criteria:
     * - All 13 writes accepted without error
     * - ERR_CODE register remains 0x0
     * - ERR_CODE[1] (SFIFO_GENCMD_ERR) = 0
     * - ERR_CODE[28] (FIFO_WRITE_ERR) = 0
     * - No fatal alert or interrupt
     * - State machine not in Error state
     *
     * @return true if test passes, false otherwise
     */
    bool test_generate_fifo_13word_boundary();

    // =========================================================================
    // Test Case 4: GENERATE_CMD FIFO 14-Word Overflow
    // =========================================================================
    /**
     * @brief Verify GENERATE_CMD FIFO 14th word triggers overflow error
     *
     * Test Objective: Validate that writing a 14th word to GENERATE_CMD FIFO
     * triggers overflow detection, setting SFIFO_GENCMD_ERR (bit 1) and
     * FIFO_WRITE_ERR (bit 28) in ERR_CODE register, with fatal alert,
     * interrupt, and state machine transition to Error state.
     *
     * Test Plan Reference: Test case #102 (test_error_generate_fifo_14word_overflow)
     * Functionality: EDN_FUNC_007
     *
     * Procedure:
     * 1. Apply reset
     * 2. Enable fatal error interrupt (INTR_ENABLE[1] = 1)
     * 3. Write 13 words to GENERATE_CMD FIFO (verify no error)
     * 4. Write 14th word to GENERATE_CMD (trigger overflow)
     * 5. Wait 10ns for error propagation
     * 6. Read ERR_CODE:
     *    - Verify bit [1] (SFIFO_GENCMD_ERR) = 1
     *    - Verify bit [28] (FIFO_WRITE_ERR) = 1
     * 7. Verify INTR_STATE[1] (edn_fatal_err) = 1
     * 8. Verify alert_fatal_alert asserted
     * 9. Verify intr_edn_fatal_err asserted
     * 10. Verify MAIN_SM_STATE = 0x47 (Error state)
     * 11. Verify ERR_CODE sticky behavior
     * 12. Verify alert sticky behavior
     *
     * Pass Criteria:
     * - 14th write triggers overflow
     * - ERR_CODE[1] = 1 (SFIFO_GENCMD_ERR)
     * - ERR_CODE[28] = 1 (FIFO_WRITE_ERR)
     * - Fatal alert and interrupt asserted
     * - State machine in Error state
     * - Sticky error behavior confirmed
     *
     * @return true if test passes, false otherwise
     */
    bool test_generate_fifo_14word_overflow();

    // =========================================================================
    // Test Case 5: FIFO_WRITE_ERR Generic Error Indicator
    // =========================================================================
    /**
     * @brief Verify FIFO_WRITE_ERR bit 28 set on any FIFO overflow
     *
     * Test Objective: Validate that ERR_CODE[28] (FIFO_WRITE_ERR) is set
     * as a generic indicator whenever either RESEED_CMD or GENERATE_CMD
     * FIFO overflows, regardless of which specific FIFO caused the error.
     *
     * Test Plan Reference: Architecture behavior map (ERR_CODE bit 28 documentation)
     * Functionality: EDN_FUNC_007
     *
     * Procedure:
     * Part A: RESEED_CMD overflow sets FIFO_WRITE_ERR
     * 1. Apply reset
     * 2. Trigger RESEED_CMD overflow (write 14 words)
     * 3. Read ERR_CODE and verify:
     *    - Bit [0] = 1 (SFIFO_RESCMD_ERR)
     *    - Bit [28] = 1 (FIFO_WRITE_ERR)
     * 4. Apply reset
     *
     * Part B: GENERATE_CMD overflow sets FIFO_WRITE_ERR
     * 5. Trigger GENERATE_CMD overflow (write 14 words)
     * 6. Read ERR_CODE and verify:
     *    - Bit [1] = 1 (SFIFO_GENCMD_ERR)
     *    - Bit [28] = 1 (FIFO_WRITE_ERR)
     *
     * Pass Criteria:
     * - RESEED_CMD overflow: ERR_CODE[0] = 1 AND ERR_CODE[28] = 1
     * - GENERATE_CMD overflow: ERR_CODE[1] = 1 AND ERR_CODE[28] = 1
     * - FIFO_WRITE_ERR serves as generic overflow indicator
     *
     * @return true if test passes, false otherwise
     */
    bool test_fifo_write_err_generic();

    // =========================================================================
    // Test Case 6: Fatal Alert Assertion on Overflow
    // =========================================================================
    /**
     * @brief Verify fatal alert signal asserted on FIFO overflow
     *
     * Test Objective: Validate that alert_fatal_alert signal is asserted
     * when FIFO overflow error occurs, and remains asserted (sticky)
     * until system reset, regardless of interrupt clearing or other actions.
     *
     * Test Plan Reference: Test cases #92, #93 (test_alert_fatal_reseed_fifo_overflow,
     *                      test_alert_fatal_generate_fifo_overflow)
     * Functionality: EDN_FUNC_007 + EDN_FUNC_010 (Alert Generation)
     *
     * Procedure:
     * 1. Apply reset
     * 2. Monitor alert_fatal_alert signal (should be 0)
     * 3. Trigger RESEED_CMD overflow
     * 4. Wait 10ns
     * 5. Verify alert_fatal_alert asserted (= 1)
     * 6. Attempt to deassert by clearing INTR_STATE[1] (W1C)
     * 7. Verify alert_fatal_alert remains asserted (sticky behavior)
     * 8. Apply reset
     * 9. Verify alert_fatal_alert deasserted (= 0) after reset
     * 10. Trigger GENERATE_CMD overflow
     * 11. Verify alert_fatal_alert asserted again
     * 12. Verify sticky behavior
     *
     * Pass Criteria:
     * - alert_fatal_alert asserts on overflow
     * - Signal remains asserted despite interrupt clearing
     * - Signal is sticky (only reset clears)
     * - Both RESEED_CMD and GENERATE_CMD overflows trigger alert
     *
     * @return true if test passes, false otherwise
     */
    bool test_fatal_alert_assertion();

    // =========================================================================
    // Test Case 7: Fatal Error Interrupt Generation
    // =========================================================================
    /**
     * @brief Verify fatal error interrupt generated on FIFO overflow
     *
     * Test Objective: Validate that intr_edn_fatal_err interrupt signal
     * is generated when FIFO overflow occurs and INTR_ENABLE[1] is set,
     * and that the interrupt can be cleared via W1C while ERR_CODE remains
     * sticky.
     *
     * Test Plan Reference: Test case #81 (test_interrupt_edn_fatal_err_fifo_overflow)
     * Functionality: EDN_FUNC_007 + EDN_FUNC_009 (Interrupt Generation)
     *
     * Procedure:
     * 1. Apply reset
     * 2. Write INTR_ENABLE register, set bit [1] = 1 (enable edn_fatal_err)
     * 3. Monitor intr_edn_fatal_err signal (should be 0)
     * 4. Trigger RESEED_CMD overflow
     * 5. Wait 10ns
     * 6. Verify intr_edn_fatal_err asserted (= 1)
     * 7. Verify INTR_STATE[1] = 1
     * 8. Clear interrupt by writing 0x2 to INTR_STATE (W1C bit 1)
     * 9. Verify intr_edn_fatal_err deasserted (= 0)
     * 10. Verify INTR_STATE[1] = 0 (cleared)
     * 11. Verify ERR_CODE[0] still = 1 (sticky, not cleared)
     * 12. Verify alert_fatal_alert still asserted (sticky)
     * 13. Test with interrupt disabled (INTR_ENABLE[1] = 0)
     * 14. Trigger overflow, verify interrupt NOT asserted (masked)
     *
     * Pass Criteria:
     * - Interrupt generated when enabled and overflow occurs
     * - INTR_STATE[1] can be cleared via W1C
     * - intr_edn_fatal_err signal follows INTR_STATE AND INTR_ENABLE
     * - ERR_CODE remains sticky despite interrupt clearing
     * - alert_fatal_alert remains asserted despite interrupt clearing
     * - Interrupt masked when INTR_ENABLE[1] = 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_fatal_interrupt_generation();

    // =========================================================================
    // Test Case 8: State Machine Transition to Error State
    // =========================================================================
    /**
     * @brief Verify state machine transitions to Error state on fatal error
     *
     * Test Objective: Validate that EDN_MAIN_SM transitions to Error state
     * (sparse-encoded value 0x47) when FIFO overflow fatal error occurs,
     * and that the Error state persists until system reset.
     *
     * Test Plan Reference: Test case #128 (test_state_error_on_illegal_state)
     * Functionality: EDN_FUNC_007 + EDN_FUNC_003 (State Machine Management)
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read MAIN_SM_STATE register (offset 0x44)
     *    - Verify initial state = 0xC1 (Idle state)
     * 3. Trigger RESEED_CMD overflow
     * 4. Wait 10ns for state transition
     * 5. Read MAIN_SM_STATE register
     *    - Verify state = 0x47 (Error state)
     * 6. Attempt to write CTRL register to change state (should be ignored)
     * 7. Read MAIN_SM_STATE again
     *    - Verify still in Error state 0x47 (stuck in error)
     * 8. Verify only reset can exit Error state
     * 9. Apply reset
     * 10. Read MAIN_SM_STATE
     *     - Verify returned to Idle state 0xC1
     * 11. Repeat for GENERATE_CMD overflow
     *
     * Pass Criteria:
     * - MAIN_SM_STATE transitions to 0x47 on overflow
     * - Error state persists despite configuration changes
     * - Error state only cleared by reset
     * - Both FIFO overflows cause Error state transition
     *
     * @return true if test passes, false otherwise
     */
    bool test_error_state_transition();

    // =========================================================================
    // Test Case 9: ERR_CODE Sticky Behavior
    // =========================================================================
    /**
     * @brief Verify ERR_CODE register sticky behavior (read-only, reset-only clear)
     *
     * Test Objective: Validate that ERR_CODE register is read-only with
     * sticky error bits that cannot be cleared by software writes, and
     * only system reset clears all error bits.
     *
     * Test Plan Reference: Test cases #31, #82 (test_err_code_sticky_ro,
     *                      test_interrupt_edn_fatal_err_clearing)
     * Functionality: EDN_FUNC_007 + EDN_FUNC_010 (Error Reporting)
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read ERR_CODE (offset 0x3C) - verify 0x0
     * 3. Trigger RESEED_CMD overflow
     * 4. Read ERR_CODE - verify bits [0] and [28] = 1
     * 5. Attempt to clear by writing 0x00000000 to ERR_CODE
     * 6. Read ERR_CODE - verify bits [0] and [28] still = 1 (write ignored)
     * 7. Attempt to clear by writing 0xFFFFFFFF to ERR_CODE
     * 8. Read ERR_CODE - verify no change (write ignored)
     * 9. Attempt to set other error bits by writing to ERR_CODE
     * 10. Read ERR_CODE - verify only overflow bits set (write ignored)
     * 11. Clear INTR_STATE[1] (W1C)
     * 12. Read ERR_CODE - verify still set (interrupt clear doesn't affect ERR_CODE)
     * 13. Apply reset
     * 14. Read ERR_CODE - verify all bits = 0 (cleared by reset)
     * 15. Trigger GENERATE_CMD overflow
     * 16. Read ERR_CODE - verify bit [1] = 1
     * 17. Repeat sticky behavior tests
     *
     * Pass Criteria:
     * - ERR_CODE register is read-only (all writes ignored)
     * - Error bits are sticky (hardware sets, software cannot clear)
     * - Clearing INTR_STATE does not clear ERR_CODE
     * - Only system reset clears ERR_CODE bits
     * - Multiple error bits can be set simultaneously
     *
     * @return true if test passes, false otherwise
     */
    bool test_err_code_sticky_behavior();

    // =========================================================================
    // Test Case 10: FIFO Clearing on Reset
    // =========================================================================
    /**
     * @brief Verify system reset clears FIFO overflow errors and empties FIFOs
     *
     * Test Objective: Validate that system reset (rst_no assertion) clears
     * all FIFO overflow error conditions, empties FIFO contents, deasserts
     * alerts, returns state machine to Idle, and restores normal operation.
     *
     * Test Plan Reference: Test case #120 (test_reset_clears_err_code)
     * Functionality: EDN_FUNC_007 + EDN_FUNC_002 (Module Initialization)
     *
     * Procedure:
     * 1. Trigger RESEED_CMD overflow (14 words)
     * 2. Verify error state:
     *    - ERR_CODE[0] = 1, ERR_CODE[28] = 1
     *    - alert_fatal_alert = 1
     *    - MAIN_SM_STATE = 0x47 (Error)
     * 3. Trigger GENERATE_CMD overflow (14 words)
     * 4. Verify compounded error state:
     *    - ERR_CODE[0] = 1, ERR_CODE[1] = 1, ERR_CODE[28] = 1
     *    - alert_fatal_alert = 1
     *    - MAIN_SM_STATE = 0x47 (Error)
     * 5. Apply system reset (100ns pulse)
     * 6. Wait 10ns for reset propagation
     * 7. Verify cleared state:
     *    - ERR_CODE = 0x00000000 (all bits cleared)
     *    - INTR_STATE = 0x00000000 (all interrupts cleared)
     *    - alert_fatal_alert = 0 (deasserted)
     *    - intr_edn_fatal_err = 0 (deasserted)
     *    - MAIN_SM_STATE = 0xC1 (Idle state restored)
     * 8. Verify FIFO functionality restored:
     *    - Write 13 words to RESEED_CMD (should succeed)
     *    - Verify no error
     *    - Write 13 words to GENERATE_CMD (should succeed)
     *    - Verify no error
     * 9. Verify overflow detection still works:
     *    - Write 14th word to RESEED_CMD
     *    - Verify overflow detected again
     *
     * Pass Criteria:
     * - Reset clears all ERR_CODE bits
     * - Reset deasserts alert_fatal_alert
     * - Reset clears INTR_STATE bits
     * - Reset deasserts intr_edn_fatal_err
     * - Reset returns MAIN_SM_STATE to Idle (0xC1)
     * - FIFOs emptied (can accept 13 words again)
     * - Overflow detection mechanism functional after reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_fifo_reset_clearing();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Write multiple words to FIFO register with logging
     * @param fifo_offset FIFO register offset (RESEED_CMD or GENERATE_CMD)
     * @param num_words Number of words to write
     * @param base_value Base value for word data (incremented for each word)
     * @param fifo_name FIFO name for logging
     *
     * Writes consecutive words to specified FIFO register for overflow testing.
     * Each word value = base_value + word_index for traceability.
     */
    void write_fifo_words(uint32_t fifo_offset,
                         unsigned int num_words,
                         uint32_t base_value,
                         const std::string& fifo_name);

    /**
     * @brief Verify ERR_CODE register bit value
     * @param bit_position Bit position to check (0-31)
     * @param expected_value Expected bit value (0 or 1)
     * @return true if bit matches expected value
     *
     * Reads ERR_CODE register and verifies specific bit matches expected state.
     */
    bool verify_err_code_bit(unsigned int bit_position, unsigned int expected_value);

    /**
     * @brief Verify complete error state (ERR_CODE, alerts, interrupts, state machine)
     * @param expected_rescmd_err Expected SFIFO_RESCMD_ERR bit [0] value
     * @param expected_gencmd_err Expected SFIFO_GENCMD_ERR bit [1] value
     * @param expected_fifo_write_err Expected FIFO_WRITE_ERR bit [28] value
     * @param expected_fatal_alert Expected alert_fatal_alert signal state
     * @param expected_fatal_intr Expected intr_edn_fatal_err signal state
     * @param expected_state Expected MAIN_SM_STATE value (0x47 for Error, 0xC1 for Idle)
     * @return true if all conditions match
     *
     * Comprehensive verification of all error-related state after overflow.
     */
    bool verify_error_state(unsigned int expected_rescmd_err,
                           unsigned int expected_gencmd_err,
                           unsigned int expected_fifo_write_err,
                           bool expected_fatal_alert,
                           bool expected_fatal_intr,
                           uint32_t expected_state);

    /**
     * @brief Verify register value matches expected value
     * @param reg_name Register name for diagnostics
     * @param expected Expected value
     * @param actual Actual read value
     * @return true if values match, false otherwise
     */
    bool verify_register_value(const std::string& reg_name, uint32_t expected, uint32_t actual);

    /**
     * @brief Verify specific bit in register matches expected value
     * @param reg_name Register name for diagnostics
     * @param bit_position Bit position to check
     * @param expected_value Expected bit value (0 or 1)
     * @param actual_reg Actual register value
     * @return true if bit matches, false otherwise
     */
    bool verify_bit_value(const std::string& reg_name,
                         unsigned int bit_position,
                         unsigned int expected_value,
                         uint32_t actual_reg);

    /**
     * @brief Report test result with detailed information
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name, bool passed, const std::string& message = "");

    // =========================================================================
    // Register Address and Bit Position Definitions
    // =========================================================================

    // FIFO register offsets (from edn_basetest.h)
    // RESEED_CMD_OFFSET = 0x2C
    // GENERATE_CMD_OFFSET = 0x30

    // ERR_CODE register bit positions
    static constexpr unsigned int SFIFO_RESCMD_ERR_BIT = 0;    ///< Reseed FIFO overflow error
    static constexpr unsigned int SFIFO_GENCMD_ERR_BIT = 1;    ///< Generate FIFO overflow error
    static constexpr unsigned int FIFO_WRITE_ERR_BIT = 28;     ///< Generic FIFO write error
    static constexpr unsigned int FIFO_READ_ERR_BIT = 29;      ///< FIFO read error (not tested here)
    static constexpr unsigned int FIFO_STATE_ERR_BIT = 30;     ///< FIFO state error (not tested here)

    // INTR_STATE bit positions
    static constexpr unsigned int EDN_CMD_REQ_DONE_BIT = 0;    ///< Command completion interrupt
    static constexpr unsigned int EDN_FATAL_ERR_BIT = 1;       ///< Fatal error interrupt

    // State machine states (sparse-encoded)
    static constexpr uint32_t STATE_IDLE = 0xC1;               ///< Idle state
    static constexpr uint32_t STATE_ERROR = 0x47;              ///< Error state

    // FIFO depth limit
    static constexpr unsigned int FIFO_DEPTH = 13;             ///< Maximum FIFO depth (words)

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;                    ///< Total tests executed
    unsigned int m_tests_passed;                 ///< Tests that passed
    unsigned int m_tests_failed;                 ///< Tests that failed
    std::vector<std::string> m_failed_tests;     ///< List of failed test names
};
