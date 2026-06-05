/**
 * @file test_edn_func_009.h
 * @brief EDN_FUNC_009 Test Suite - Interrupt Generation and Management Verification
 *
 * Comprehensive test suite for EDN_FUNC_009 (Interrupt Generation and Management) covering:
 * - Command completion interrupt generation (intr_edn_cmd_req_done)
 * - Fatal error interrupt generation (intr_edn_fatal_err)
 * - INTR_ENABLE masking behavior (enable/disable control)
 * - W1C clearing mechanism for INTR_STATE
 * - INTR_TEST forced interrupt assertion
 * - Interrupt signal logic validation (INTR_STATE AND INTR_ENABLE)
 * - Status bit setting regardless of enable mask
 * - Multiple interrupt sources simultaneously
 * - Reset behavior for interrupt registers
 *
 * Test Coverage:
 * - test_intr_cmd_req_done_generation: Validates command completion interrupt assertion
 * - test_intr_fatal_err_generation: Tests fatal error interrupt on FIFO overflow
 * - test_intr_enable_masking: Verifies enable/disable control for both interrupts
 * - test_intr_state_w1c_clearing: Validates Write-1-to-Clear mechanism
 * - test_intr_test_forced_assertion: Tests forced interrupt via INTR_TEST
 * - test_interrupt_signal_logic: Verifies interrupt output = INTR_STATE AND INTR_ENABLE
 * - test_status_bit_independent_of_enable: Tests status bits set regardless of enable mask
 * - test_multiple_interrupts_simultaneous: Validates both interrupts triggered together
 * - test_interrupt_reset_behavior: Tests reset behavior of interrupt registers
 *
 * Key Implementation Details:
 * - Two interrupt sources: intr_edn_cmd_req_done [bit 0], intr_edn_fatal_err [bit 1]
 * - Three control registers: INTR_STATE (RW1C), INTR_ENABLE (RW), INTR_TEST (WO)
 * - W1C clearing: Write 1 to clear status bit
 * - Interrupt output logic: signal = (INTR_STATE AND INTR_ENABLE)
 * - Status bits ALWAYS set when events occur, regardless of enable mask (bug fix validation)
 * - Fatal error triggers on FIFO overflow (>13 words)
 * - Command completion triggers when software command acknowledged by CSRNG
 *
 * @note This test suite implements all 9 test cases mapped to EDN_FUNC_009
 *       in the edn-functionality-testcases.md document.
 *
 * Registers Under Test:
 * - INTR_STATE (0x00): Interrupt status with W1C fields
 * - INTR_ENABLE (0x04): Interrupt enable/disable control
 * - INTR_TEST (0x08): Forced interrupt assertion for testing
 * - RESEED_CMD/GENERATE_CMD: Used to trigger FIFO overflow errors
 * - SW_CMD_REQ/SW_CMD_STS: Used to trigger command completion events
 *
 * Ports/Signals Monitored:
 * - intr_edn_cmd_req_done: Command completion interrupt output
 * - intr_edn_fatal_err: Fatal error interrupt output
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_009
 * @brief Test fixture for EDN_FUNC_009 interrupt verification
 *
 * Extends edn_test to provide comprehensive interrupt generation and management testing.
 * Implements all 9 test cases for EDN_FUNC_009 covering interrupt assertion, masking,
 * clearing, and status bit behavior.
 */
class test_edn_func_009 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for interrupt testing.
     */
    test_edn_func_009(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_009();

    /**
     * @brief Execute all EDN_FUNC_009 test cases
     * @return Number of failed tests
     *
     * Runs all 9 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Command Completion Interrupt Generation
    // =========================================================================
    /**
     * @brief Verify intr_edn_cmd_req_done interrupt generation on command completion
     *
     * Test Objective: Validate that command completion interrupt (intr_edn_cmd_req_done)
     * is generated when software CSRNG command completes and INTR_ENABLE is set.
     *
     * Test Plan Reference: Test case #79 (test_interrupt_edn_cmd_req_done_assertion)
     * Functionality: EDN_FUNC_009 (Interrupt Generation and Management)
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable EDN in software port mode (CTRL.EDN_ENABLE=0x6, modes=0x9)
     * 3. Enable command completion interrupt (INTR_ENABLE.edn_cmd_req_done=1)
     * 4. Issue instantiate command via SW_CMD_REQ
     * 5. Wait for command completion (SW_CMD_STS.CMD_ACK=1)
     * 6. Verify INTR_STATE.edn_cmd_req_done bit is set
     * 7. Verify intr_edn_cmd_req_done signal is asserted
     * 8. Clear interrupt status (write 1 to INTR_STATE.edn_cmd_req_done)
     * 9. Verify interrupt signal is de-asserted
     *
     * Pass Criteria:
     * - INTR_STATE.edn_cmd_req_done set when command completes
     * - intr_edn_cmd_req_done signal asserted when enabled
     * - Interrupt clears when status bit cleared
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_cmd_req_done_generation();

    // =========================================================================
    // Test Case 2: Fatal Error Interrupt Generation
    // =========================================================================
    /**
     * @brief Verify intr_edn_fatal_err interrupt generation on FIFO overflow
     *
     * Test Objective: Validate that fatal error interrupt (intr_edn_fatal_err)
     * is generated when FIFO overflow occurs and INTR_ENABLE is set.
     *
     * Test Plan Reference: Test case #81 (test_interrupt_edn_fatal_err_fifo_overflow)
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable fatal error interrupt (INTR_ENABLE.edn_fatal_err=1)
     * 3. Write 14 words to RESEED_CMD FIFO (exceeds 13-word depth)
     * 4. Verify INTR_STATE.edn_fatal_err bit is set
     * 5. Verify ERR_CODE.SFIFO_RESCMD_ERR bit is set
     * 6. Verify intr_edn_fatal_err signal is asserted
     * 7. Verify alert_fatal_alert signal is asserted
     * 8. Clear interrupt status (write 1 to INTR_STATE.edn_fatal_err)
     * 9. Verify interrupt signal is de-asserted (ERR_CODE remains sticky)
     *
     * Pass Criteria:
     * - INTR_STATE.edn_fatal_err set on FIFO overflow
     * - ERR_CODE sticky error bit set
     * - intr_edn_fatal_err signal asserted when enabled
     * - Fatal alert also triggered
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_fatal_err_generation();

    // =========================================================================
    // Test Case 3: Interrupt Enable Masking Behavior
    // =========================================================================
    /**
     * @brief Verify INTR_ENABLE masking controls interrupt signal assertion
     *
     * Test Objective: Validate that INTR_ENABLE register controls whether
     * interrupt signals are asserted when status bits are set.
     *
     * Test Plan Reference: Test case #3 (test_intr_enable_rw) extended for masking
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Force both interrupts via INTR_TEST (INTR_STATE bits set)
     * 3. Verify interrupt signals are NOT asserted (INTR_ENABLE=0x0)
     * 4. Enable edn_cmd_req_done interrupt (INTR_ENABLE=0x1)
     * 5. Verify intr_edn_cmd_req_done signal asserts
     * 6. Verify intr_edn_fatal_err signal remains de-asserted
     * 7. Enable edn_fatal_err interrupt (INTR_ENABLE=0x3)
     * 8. Verify both interrupt signals are asserted
     * 9. Disable both interrupts (INTR_ENABLE=0x0)
     * 10. Verify both interrupt signals de-assert (status bits remain set)
     *
     * Pass Criteria:
     * - Interrupt signals assert only when enabled
     * - Disabling interrupts de-asserts signals but preserves status bits
     * - Enable masking works independently for each interrupt
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_enable_masking();

    // =========================================================================
    // Test Case 4: W1C Clearing Mechanism
    // =========================================================================
    /**
     * @brief Verify INTR_STATE Write-1-to-Clear mechanism for interrupt status
     *
     * Test Objective: Validate that writing 1 to INTR_STATE bits clears
     * interrupt status, while writing 0 has no effect.
     *
     * Test Plan Reference: Test case #80 (test_interrupt_edn_cmd_req_done_clearing)
     *                       and #82 (test_interrupt_edn_fatal_err_clearing)
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Force both interrupts via INTR_TEST
     * 3. Enable both interrupts via INTR_ENABLE
     * 4. Verify both interrupt signals are asserted
     * 5. Write 0 to INTR_STATE.edn_cmd_req_done (no effect)
     * 6. Verify INTR_STATE.edn_cmd_req_done remains set
     * 7. Write 1 to INTR_STATE.edn_cmd_req_done (W1C)
     * 8. Verify INTR_STATE.edn_cmd_req_done is cleared
     * 9. Verify intr_edn_cmd_req_done signal de-asserts
     * 10. Verify edn_fatal_err interrupt remains asserted
     * 11. Write 1 to INTR_STATE.edn_fatal_err
     * 12. Verify INTR_STATE.edn_fatal_err cleared and signal de-asserts
     *
     * Pass Criteria:
     * - Writing 1 clears interrupt status bit
     * - Writing 0 has no effect
     * - Clearing status de-asserts interrupt signal when enabled
     * - Independent clearing for each interrupt source
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_state_w1c_clearing();

    // =========================================================================
    // Test Case 5: INTR_TEST Forced Interrupt Assertion
    // =========================================================================
    /**
     * @brief Verify INTR_TEST register forces interrupt status bits for testing
     *
     * Test Objective: Validate that writing 1 to INTR_TEST fields forces
     * corresponding INTR_STATE bits without actual event occurrence.
     *
     * Test Plan Reference: Test case #4 (test_intr_test_wo)
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Clear INTR_STATE register
     * 3. Verify INTR_STATE reads 0x0
     * 4. Write 0x1 to INTR_TEST (force edn_cmd_req_done)
     * 5. Verify INTR_STATE.edn_cmd_req_done is set
     * 6. Clear INTR_STATE
     * 7. Write 0x2 to INTR_TEST (force edn_fatal_err)
     * 8. Verify INTR_STATE.edn_fatal_err is set
     * 9. Clear INTR_STATE
     * 10. Write 0x3 to INTR_TEST (force both interrupts)
     * 11. Verify both INTR_STATE bits are set
     * 12. Attempt to read INTR_TEST (should return 0, write-only)
     *
     * Pass Criteria:
     * - INTR_TEST forces INTR_STATE bits without actual events
     * - INTR_TEST is write-only (reads return 0)
     * - Both interrupts can be forced simultaneously
     * - Reserved bits have no effect
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_test_forced_assertion();

    // =========================================================================
    // Test Case 6: Interrupt Signal Logic Verification
    // =========================================================================
    /**
     * @brief Verify interrupt signal logic: signal = INTR_STATE AND INTR_ENABLE
     *
     * Test Objective: Validate that interrupt output signals follow the logic
     * equation: interrupt_signal = (INTR_STATE AND INTR_ENABLE).
     *
     * Test Plan Reference: Test case from EDN_FUNC_009 requirements
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Test all four combinations for edn_cmd_req_done:
     *    a. INTR_STATE=0, INTR_ENABLE=0 -> signal=0
     *    b. INTR_STATE=0, INTR_ENABLE=1 -> signal=0
     *    c. INTR_STATE=1, INTR_ENABLE=0 -> signal=0
     *    d. INTR_STATE=1, INTR_ENABLE=1 -> signal=1
     * 3. Test all four combinations for edn_fatal_err
     * 4. Test mixed combinations (one enabled, one disabled)
     * 5. Verify signals follow AND gate logic exactly
     *
     * Pass Criteria:
     * - Signal asserts only when BOTH status=1 AND enable=1
     * - Signal de-asserts when either status=0 OR enable=0
     * - Logic applies independently to both interrupt sources
     * - No timing glitches or unexpected behavior
     *
     * @return true if test passes, false otherwise
     */
    bool test_interrupt_signal_logic();

    // =========================================================================
    // Test Case 7: Status Bit Setting Independent of Enable Mask
    // =========================================================================
    /**
     * @brief Verify status bits set regardless of enable mask (bug fix validation)
     *
     * Test Objective: Validate that INTR_STATE status bits are ALWAYS set when
     * interrupt events occur, regardless of INTR_ENABLE mask state. This validates
     * the bug fix mentioned in the implementation summary.
     *
     * Test Plan Reference: EDN_FUNC_009 implementation requirement
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Disable both interrupts (INTR_ENABLE=0x0)
     * 3. Enable EDN in software port mode
     * 4. Issue software command via SW_CMD_REQ
     * 5. Wait for command completion
     * 6. Verify INTR_STATE.edn_cmd_req_done is set (despite INTR_ENABLE=0)
     * 7. Verify intr_edn_cmd_req_done signal is NOT asserted (enable=0)
     * 8. Clear INTR_STATE
     * 9. Disable fatal error interrupt (INTR_ENABLE.edn_fatal_err=0)
     * 10. Trigger FIFO overflow (write 14 words to GENERATE_CMD)
     * 11. Verify INTR_STATE.edn_fatal_err is set (despite INTR_ENABLE=0)
     * 12. Verify intr_edn_fatal_err signal is NOT asserted
     *
     * Pass Criteria:
     * - Status bits set when events occur, regardless of enable mask
     * - Interrupt signals follow enable mask (signal = status AND enable)
     * - Bug fix validated: status bits are event-driven, not enable-dependent
     *
     * @return true if test passes, false otherwise
     */
    bool test_status_bit_independent_of_enable();

    // =========================================================================
    // Test Case 8: Multiple Interrupt Sources Simultaneously
    // =========================================================================
    /**
     * @brief Verify both interrupt sources can trigger simultaneously
     *
     * Test Objective: Validate that both interrupt sources (cmd_req_done and
     * fatal_err) can be active simultaneously with correct status and signal behavior.
     *
     * Test Plan Reference: Test case from EDN_FUNC_009 requirements
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable both interrupts (INTR_ENABLE=0x3)
     * 3. Force both interrupts via INTR_TEST
     * 4. Verify both INTR_STATE bits are set (0x3)
     * 5. Verify both interrupt signals are asserted
     * 6. Clear edn_cmd_req_done only (write 0x1 to INTR_STATE)
     * 7. Verify INTR_STATE=0x2 (only edn_fatal_err set)
     * 8. Verify intr_edn_cmd_req_done de-asserted
     * 9. Verify intr_edn_fatal_err remains asserted
     * 10. Clear edn_fatal_err (write 0x2 to INTR_STATE)
     * 11. Verify both interrupts cleared and signals de-asserted
     *
     * Pass Criteria:
     * - Both interrupts can be set simultaneously
     * - Both signals can assert simultaneously
     * - Interrupts can be cleared independently
     * - No interference between interrupt sources
     *
     * @return true if test passes, false otherwise
     */
    bool test_multiple_interrupts_simultaneous();

    // =========================================================================
    // Test Case 9: Interrupt Reset Behavior
    // =========================================================================
    /**
     * @brief Verify reset behavior of interrupt registers and signals
     *
     * Test Objective: Validate that system reset clears all interrupt registers
     * and de-asserts interrupt signals.
     *
     * Test Plan Reference: Test case #1 (test_register_reset_values) extended
     * Functionality: EDN_FUNC_009
     *
     * Procedure:
     * 1. Apply initial reset
     * 2. Enable both interrupts (INTR_ENABLE=0x3)
     * 3. Force both interrupts via INTR_TEST
     * 4. Verify INTR_STATE=0x3, INTR_ENABLE=0x3
     * 5. Verify both interrupt signals are asserted
     * 6. Apply system reset
     * 7. Verify INTR_STATE reset to 0x0
     * 8. Verify INTR_ENABLE reset to 0x0
     * 9. Verify INTR_TEST reset to 0x0
     * 10. Verify both interrupt signals are de-asserted
     * 11. Verify registers are writable after reset
     *
     * Pass Criteria:
     * - INTR_STATE resets to 0x0
     * - INTR_ENABLE resets to 0x0
     * - INTR_TEST resets to 0x0
     * - Interrupt signals de-assert on reset
     * - Registers functional after reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_interrupt_reset_behavior();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Issue instantiate command in software port mode
     * @return true if command issued successfully, false otherwise
     *
     * Helper to trigger command completion interrupt by issuing a simple
     * instantiate command via SW_CMD_REQ and waiting for acknowledgment.
     */
    bool issue_sw_instantiate_command();

    /**
     * @brief Trigger FIFO overflow error
     * @param fifo_type 0=RESEED_CMD, 1=GENERATE_CMD
     * @return true if overflow triggered, false otherwise
     *
     * Helper to trigger fatal error interrupt by writing 14 words to
     * either RESEED_CMD or GENERATE_CMD FIFO (exceeds 13-word depth).
     */
    bool trigger_fifo_overflow(uint32_t fifo_type);

    /**
     * @brief Verify interrupt signal state matches expected value
     * @param interrupt_id 0=edn_cmd_req_done, 1=edn_fatal_err
     * @param expected Expected signal value (true=asserted, false=de-asserted)
     * @return true if signal matches expected, false otherwise
     */
    bool verify_interrupt_signal(uint32_t interrupt_id, bool expected);

    /**
     * @brief Verify register value matches expected value
     * @param reg_name Register name for diagnostics
     * @param expected Expected value
     * @param actual Actual read value
     * @return true if values match, false otherwise
     */
    bool verify_register_value(const std::string& reg_name, uint32_t expected, uint32_t actual);

    /**
     * @brief Report test result with detailed information
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name, bool passed, const std::string& message = "");

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;       ///< Total tests executed
    unsigned int m_tests_passed;    ///< Tests that passed
    unsigned int m_tests_failed;    ///< Tests that failed
    std::vector<std::string> m_failed_tests; ///< List of failed test names
};
