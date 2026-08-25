// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_010.h
 * @brief EDN_FUNC_010 Test Suite - Alert Generation and Error Reporting Verification
 *
 * Comprehensive test suite for EDN_FUNC_010 (Alert Generation and Error Reporting) covering:
 * - ALERT_TEST write-only access and signal pulsing (recov_alert, fatal_alert)
 * - ERR_CODE_TEST error injection for all error types
 * - ERR_CODE sticky read-only behavior (persistent until reset)
 * - Fatal alert assertion on all 9 error conditions
 * - Recoverable alert assertion on all 6 alert conditions
 * - Alert clearing mechanisms (W0C for recoverable, reset for fatal)
 * - Integration with existing error sources (FIFO overflow, multi-bit encoding, etc.)
 * - RECOV_ALERT_STS register W0C (Write-0-to-Clear) semantics
 *
 * Test Coverage (22 test cases per EDN_FUNC_010):
 * 1. test_alert_test_wo: Validates ALERT_TEST write-only register triggers alert pulses
 * 2. test_err_code_test_injection: Tests ERR_CODE_TEST forces error bits for testing
 * 3. test_err_code_sticky_ro: Validates ERR_CODE read-only and sticky until reset
 * 4. test_alert_recov_edn_enable_field: Tests recoverable alert on invalid EDN_ENABLE
 * 5. test_alert_recov_boot_req_mode_field: Validates alert on invalid BOOT_REQ_MODE
 * 6. test_alert_recov_auto_req_mode_field: Tests alert on invalid AUTO_REQ_MODE
 * 7. test_alert_recov_cmd_fifo_rst_field: Validates alert on invalid CMD_FIFO_RST
 * 8. test_alert_recov_entropy_bus_cmp: Tests alert on consecutive entropy match
 * 9. test_alert_recov_csrng_cmd_sts: Validates alert on CSRNG error status
 * 10. test_alert_recov_clearing_w0c: Tests W0C clearing for all recoverable alerts
 * 11. test_alert_fatal_main_sm_illegal_state: Validates fatal alert on MAIN_SM error
 * 12. test_alert_fatal_ack_sm_illegal_state: Tests fatal alert on ACK_SM error
 * 13. test_alert_fatal_reseed_fifo_overflow: Validates RESEED_CMD FIFO overflow alert
 * 14. test_alert_fatal_generate_fifo_overflow: Tests GENERATE_CMD FIFO overflow alert
 * 15. test_alert_fatal_fifo_write_error: Validates internal FIFO write error alert
 * 16. test_alert_fatal_fifo_read_error: Tests internal FIFO read error alert
 * 17. test_alert_fatal_fifo_state_error: Validates internal FIFO state error alert
 * 18. test_alert_fatal_counter_error: Tests hardened counter error alert
 * 19. test_alert_fatal_sticky_until_reset: Validates fatal alert persists until reset
 * 20. test_alert_test_no_status_change: Tests ALERT_TEST doesn't modify status registers
 * 21. test_alert_fatal_triggers_interrupt: Validates fatal alerts trigger intr_edn_fatal_err
 * 22. test_multiple_errors_simultaneous: Tests multiple error conditions simultaneously
 *
 * Key Implementation Details:
 * - Two alert signals: alert_recov_alert (recoverable), alert_fatal_alert (fatal)
 * - ALERT_TEST (0x0C, WO): Forces alert pulses without setting status registers
 * - RECOV_ALERT_STS (0x38, RW0C): Status for 6 recoverable alert conditions
 * - ERR_CODE (0x3C, RO sticky): Status for 9 fatal error conditions
 * - ERR_CODE_TEST (0x40, WO): Forces ERR_CODE bits for testing error paths
 * - Recoverable alerts cleared by writing 0 to RECOV_ALERT_STS bit (W0C)
 * - Fatal alerts/errors only cleared by system reset (ERR_CODE is sticky)
 * - Fatal alerts trigger intr_edn_fatal_err interrupt when INTR_ENABLE set
 *
 * @note This test suite implements all 22 test cases mapped to EDN_FUNC_010
 *       in the edn-functionality-testcases.md document.
 *
 * Registers Under Test:
 * - ALERT_TEST (0x0C): Force alert signal assertion
 * - RECOV_ALERT_STS (0x38): Recoverable alert status (6 bits, W0C)
 * - ERR_CODE (0x3C): Fatal error code (9 bits, read-only sticky)
 * - ERR_CODE_TEST (0x40): Force ERR_CODE bits for testing
 * - CTRL (0x14): Multi-bit encoded fields trigger recoverable alerts
 * - RESEED_CMD/GENERATE_CMD (0x28/0x2C): FIFO overflow triggers fatal alert
 * - INTR_STATE (0x00): Fatal alerts trigger edn_fatal_err interrupt
 *
 * Ports/Signals Monitored:
 * - alert_recov_alert: Recoverable alert output signal
 * - alert_fatal_alert: Fatal alert output signal
 * - intr_edn_fatal_err: Fatal error interrupt (triggered by fatal alerts)
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
 * @class test_edn_func_010
 * @brief Test fixture for EDN_FUNC_010 alert and error reporting verification
 *
 * Extends edn_test to provide comprehensive alert generation and error reporting testing.
 * Implements all 22 test cases for EDN_FUNC_010 covering both recoverable and fatal
 * alert conditions, error injection, sticky error behavior, and clearing mechanisms.
 */
class test_edn_func_010 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for alert/error testing.
     */
    test_edn_func_010(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_010();

    /**
     * @brief Execute all EDN_FUNC_010 test cases
     * @return Number of failed tests
     *
     * Runs all 22 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: ALERT_TEST Write-Only Register
    // =========================================================================
    /**
     * @brief Verify ALERT_TEST register triggers alert signals without status change
     *
     * Test Objective: Validate that ALERT_TEST register (write-only) triggers
     * alert signals for testing without modifying RECOV_ALERT_STS or ERR_CODE.
     *
     * Test Plan Reference: Test case #5 (test_alert_test_wo) and #116
     * Functionality: EDN_FUNC_010 (Alert Generation and Error Reporting)
     *
     * Procedure:
     * 1. Read initial RECOV_ALERT_STS and ERR_CODE values
     * 2. Write 0x1 to ALERT_TEST.recov_alert (bit 0)
     * 3. Verify alert_recov_alert signal pulses (brief assertion)
     * 4. Verify RECOV_ALERT_STS unchanged
     * 5. Write 0x2 to ALERT_TEST.fatal_alert (bit 1)
     * 6. Verify alert_fatal_alert signal pulses
     * 7. Verify ERR_CODE unchanged
     * 8. Write 0x3 to ALERT_TEST (both alerts)
     * 9. Verify both alert signals pulse
     * 10. Read ALERT_TEST (should return 0x0, write-only)
     *
     * Pass Criteria:
     * - ALERT_TEST triggers alert signal pulses
     * - Status registers (RECOV_ALERT_STS, ERR_CODE) remain unchanged
     * - ALERT_TEST reads as 0x0 (write-only register)
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_test_wo();

    // =========================================================================
    // Test Case 2: ERR_CODE_TEST Error Injection
    // =========================================================================
    /**
     * @brief Verify ERR_CODE_TEST forces error bits for testing error handling
     *
     * Test Objective: Validate that ERR_CODE_TEST register forces ERR_CODE bits
     * without actual hardware faults, triggers fatal alert and interrupt.
     *
     * Test Plan Reference: Test case #32 (test_err_code_test_injection)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Enable fatal error interrupt (INTR_ENABLE.edn_fatal_err=1)
     * 2. Write error bit pattern to ERR_CODE_TEST (e.g., 0x01 for bit 0)
     * 3. Verify corresponding ERR_CODE bit is set
     * 4. Verify alert_fatal_alert signal asserts
     * 5. Verify intr_edn_fatal_err interrupt asserts
     * 6. Test each of the 9 error bits individually
     * 7. Test multiple error bits simultaneously
     * 8. Verify ERR_CODE_TEST reads as 0x0 (write-only)
     * 9. Verify ERR_CODE remains sticky (cannot be cleared by register write)
     *
     * Pass Criteria:
     * - ERR_CODE_TEST forces corresponding ERR_CODE bits
     * - Fatal alert and interrupt triggered
     * - ERR_CODE is sticky (persists until reset)
     * - ERR_CODE_TEST is write-only
     *
     * @return true if test passes, false otherwise
     */
    bool test_err_code_test_injection();

    // =========================================================================
    // Test Case 3: ERR_CODE Sticky Read-Only Behavior
    // =========================================================================
    /**
     * @brief Verify ERR_CODE is read-only and sticky until system reset
     *
     * Test Objective: Validate that ERR_CODE register is read-only and error
     * bits remain set (sticky) until system reset, cannot be cleared by firmware.
     *
     * Test Plan Reference: Test case #31 (test_err_code_sticky_ro)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Trigger FIFO overflow error (RESEED_CMD with 14 words)
     * 2. Verify ERR_CODE.SFIFO_RESCMD_ERR (bit 0) is set
     * 3. Attempt to write 0x0 to ERR_CODE (should have no effect)
     * 4. Verify ERR_CODE bit remains set
     * 5. Attempt to write 0xFFFFFFFF to ERR_CODE
     * 6. Verify only actual error bits are set (no spurious bits)
     * 7. Clear INTR_STATE.edn_fatal_err interrupt
     * 8. Verify ERR_CODE still set (sticky, independent of interrupt)
     * 9. Apply system reset
     * 10. Verify ERR_CODE cleared to 0x0 after reset
     *
     * Pass Criteria:
     * - ERR_CODE is read-only (writes have no effect)
     * - Error bits remain set until system reset
     * - Only actual error conditions set ERR_CODE bits
     * - Reset clears all ERR_CODE bits
     *
     * @return true if test passes, false otherwise
     */
    bool test_err_code_sticky_ro();

    // =========================================================================
    // Test Case 4: Recoverable Alert - EDN_ENABLE Field Invalid Value
    // =========================================================================
    /**
     * @brief Verify recoverable alert on invalid CTRL.EDN_ENABLE value
     *
     * Test Objective: Validate that writing invalid multi-bit encoded value
     * to CTRL.EDN_ENABLE triggers EDN_ENABLE_FIELD_ALERT in RECOV_ALERT_STS.
     *
     * Test Plan Reference: Test case #83 (test_alert_recov_edn_enable_field)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Write invalid value (not 0x6 or 0x9) to CTRL.EDN_ENABLE (e.g., 0x5)
     * 2. Verify RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT (bit 0) is set
     * 3. Verify alert_recov_alert signal is asserted
     * 4. Write correct value 0x6 to CTRL.EDN_ENABLE
     * 5. Verify alert condition cleared (status may remain until W0C)
     * 6. Write 0 to RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT (W0C)
     * 7. Verify status bit cleared
     * 8. Verify alert_recov_alert de-asserted (if no other alerts active)
     *
     * Pass Criteria:
     * - Invalid EDN_ENABLE value triggers EDN_ENABLE_FIELD_ALERT
     * - Recoverable alert signal asserts
     * - Alert cleared via W0C after correcting value
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_edn_enable_field();

    // =========================================================================
    // Test Case 5: Recoverable Alert - BOOT_REQ_MODE Field Invalid Value
    // =========================================================================
    /**
     * @brief Verify recoverable alert on invalid CTRL.BOOT_REQ_MODE value
     *
     * Test Plan Reference: Test case #84 (test_alert_recov_boot_req_mode_field)
     *
     * Procedure: Similar to TC4, but for BOOT_REQ_MODE field (bits [7:4])
     * and RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT (bit 1).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_boot_req_mode_field();

    // =========================================================================
    // Test Case 6: Recoverable Alert - AUTO_REQ_MODE Field Invalid Value
    // =========================================================================
    /**
     * @brief Verify recoverable alert on invalid CTRL.AUTO_REQ_MODE value
     *
     * Test Plan Reference: Test case #85 (test_alert_recov_auto_req_mode_field)
     *
     * Procedure: Similar to TC4, but for AUTO_REQ_MODE field (bits [11:8])
     * and RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT (bit 2).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_auto_req_mode_field();

    // =========================================================================
    // Test Case 7: Recoverable Alert - CMD_FIFO_RST Field Invalid Value
    // =========================================================================
    /**
     * @brief Verify recoverable alert on invalid CTRL.CMD_FIFO_RST value
     *
     * Test Plan Reference: Test case #86 (test_alert_recov_cmd_fifo_rst_field)
     *
     * Procedure: Similar to TC4, but for CMD_FIFO_RST field (bits [15:12])
     * and RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT (bit 3).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_cmd_fifo_rst_field();

    // =========================================================================
    // Test Case 8: Recoverable Alert - Entropy Bus Consistency Check
    // =========================================================================
    /**
     * @brief Verify recoverable alert on consecutive duplicate entropy values
     *
     * Test Objective: Validate that consecutive matching 64-bit genbits values
     * from CSRNG trigger ENTROPY_BUS_CMP_ALERT recoverable alert.
     *
     * Test Plan Reference: Test case #87 (test_alert_recov_entropy_bus_cmp)
     * Functionality: EDN_FUNC_010, EDN_FUNC_008
     *
     * Procedure:
     * 1. Enable EDN in software port mode
     * 2. Issue instantiate command via SW_CMD_REQ
     * 3. Configure CSRNG mock to return duplicate genbits values
     * 4. Issue generate command via SW_CMD_REQ
     * 5. Verify RECOV_ALERT_STS.ENTROPY_BUS_CMP_ALERT (bit 5) is set
     * 6. Verify alert_recov_alert signal is asserted
     * 7. Write 0 to RECOV_ALERT_STS.ENTROPY_BUS_CMP_ALERT (W0C)
     * 8. Verify alert cleared
     *
     * Pass Criteria:
     * - Consecutive duplicate entropy values trigger alert
     * - ENTROPY_BUS_CMP_ALERT status bit set
     * - Alert cleared via W0C mechanism
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_entropy_bus_cmp();

    // =========================================================================
    // Test Case 9: Recoverable Alert - CSRNG Command Status Error
    // =========================================================================
    /**
     * @brief Verify recoverable alert on CSRNG non-zero status response
     *
     * Test Objective: Validate that CSRNG acknowledgment with non-zero CMD_STS
     * triggers CSRNG_CMD_STS_ALERT and sets ERR_CODE.SFIFO_ESRNG_ERR.
     *
     * Test Plan Reference: Test case #88 (test_alert_recov_csrng_cmd_sts)
     * Functionality: EDN_FUNC_010, EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable EDN in software port mode
     * 2. Issue instantiate command via SW_CMD_REQ
     * 3. Configure CSRNG mock to return non-zero CMD_STS (e.g., 0x2)
     * 4. Verify RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT (bit 4) is set
     * 5. Verify ERR_CODE.SFIFO_ESRNG_ERR (bit 2) is set
     * 6. Verify alert_recov_alert signal is asserted
     * 7. Verify alert_fatal_alert also asserts (ERR_CODE set)
     * 8. Write 0 to RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT (W0C)
     * 9. Verify recoverable alert cleared but ERR_CODE remains sticky
     *
     * Pass Criteria:
     * - CSRNG error status triggers both recoverable alert and fatal error
     * - RECOV_ALERT_STS bit can be cleared
     * - ERR_CODE remains sticky until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_csrng_cmd_sts();

    // =========================================================================
    // Test Case 10: Recoverable Alert W0C Clearing
    // =========================================================================
    /**
     * @brief Verify all recoverable alert status bits cleared via W0C
     *
     * Test Objective: Validate Write-0-to-Clear semantics for all 6 recoverable
     * alert status bits in RECOV_ALERT_STS register.
     *
     * Test Plan Reference: Test case #89 (test_alert_recov_clearing_w0c)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Trigger each of the 6 recoverable alert conditions
     * 2. Verify corresponding RECOV_ALERT_STS bits are set
     * 3. Verify alert_recov_alert signal is asserted
     * 4. Attempt to write 1 to alert bit (should have no effect, W0C not W1C)
     * 5. Write 0 to one alert bit position
     * 6. Verify that specific bit is cleared
     * 7. Write 0 to all alert bits simultaneously (clear all)
     * 8. Verify RECOV_ALERT_STS reads 0x0
     * 9. Verify alert_recov_alert signal de-asserted
     *
     * Pass Criteria:
     * - Writing 0 clears corresponding alert bit (W0C semantics)
     * - Writing 1 has no effect (not W1C)
     * - All alerts can be cleared independently
     * - Alert signal de-asserts when all status bits cleared
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_recov_clearing_w0c();

    // =========================================================================
    // Test Case 11: Fatal Alert - Main State Machine Illegal State
    // =========================================================================
    /**
     * @brief Verify fatal alert on EDN_MAIN_SM illegal state detection
     *
     * Test Objective: Validate that illegal state in EDN_MAIN_SM triggers
     * ERR_CODE.EDN_MAIN_SM_ERR and fatal alert/interrupt.
     *
     * Test Plan Reference: Test case #90 (test_alert_fatal_main_sm_illegal_state)
     * Functionality: EDN_FUNC_010, EDN_FUNC_003
     *
     * Procedure:
     * 1. Enable fatal error interrupt (INTR_ENABLE.edn_fatal_err=1)
     * 2. Force EDN_MAIN_SM_ERR via ERR_CODE_TEST (bit 20)
     * 3. Verify ERR_CODE.EDN_MAIN_SM_ERR (bit 20) is set
     * 4. Verify alert_fatal_alert signal is asserted
     * 5. Verify intr_edn_fatal_err interrupt is asserted
     * 6. Verify INTR_STATE.edn_fatal_err (bit 1) is set
     * 7. Clear interrupt status (W1C)
     * 8. Verify ERR_CODE remains sticky
     * 9. Verify fatal alert persists until reset
     *
     * Pass Criteria:
     * - Illegal state triggers EDN_MAIN_SM_ERR
     * - Fatal alert and interrupt asserted
     * - Error is sticky until system reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_main_sm_illegal_state();

    // =========================================================================
    // Test Case 12: Fatal Alert - ACK State Machine Illegal State
    // =========================================================================
    /**
     * @brief Verify fatal alert on EDN_ACK_SM illegal state detection
     *
     * Test Plan Reference: Test case #91 (test_alert_fatal_ack_sm_illegal_state)
     *
     * Procedure: Similar to TC11, but for EDN_ACK_SM_ERR (bit 21).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_ack_sm_illegal_state();

    // =========================================================================
    // Test Case 13: Fatal Alert - RESEED_CMD FIFO Overflow
    // =========================================================================
    /**
     * @brief Verify fatal alert on RESEED_CMD FIFO overflow (>13 words)
     *
     * Test Objective: Validate that writing 14th word to RESEED_CMD FIFO
     * triggers SFIFO_RESCMD_ERR and fatal alert/interrupt.
     *
     * Test Plan Reference: Test case #92 (test_alert_fatal_reseed_fifo_overflow)
     * Functionality: EDN_FUNC_010, EDN_FUNC_007
     *
     * Procedure:
     * 1. Enable fatal error interrupt
     * 2. Write 13 words to RESEED_CMD (within depth limit)
     * 3. Verify no error triggered
     * 4. Write 14th word to RESEED_CMD (overflow)
     * 5. Verify ERR_CODE.SFIFO_RESCMD_ERR (bit 0) is set
     * 6. Verify alert_fatal_alert asserted
     * 7. Verify intr_edn_fatal_err asserted
     * 8. Clear interrupt, verify error remains sticky
     *
     * Pass Criteria:
     * - 14th word triggers overflow error
     * - Fatal alert and interrupt generated
     * - Error is sticky until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_reseed_fifo_overflow();

    // =========================================================================
    // Test Case 14: Fatal Alert - GENERATE_CMD FIFO Overflow
    // =========================================================================
    /**
     * @brief Verify fatal alert on GENERATE_CMD FIFO overflow (>13 words)
     *
     * Test Plan Reference: Test case #93 (test_alert_fatal_generate_fifo_overflow)
     *
     * Procedure: Similar to TC13, but for GENERATE_CMD FIFO and
     * ERR_CODE.SFIFO_GENCMD_ERR (bit 1).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_generate_fifo_overflow();

    // =========================================================================
    // Test Case 15: Fatal Alert - Internal FIFO Write Error
    // =========================================================================
    /**
     * @brief Verify fatal alert on internal FIFO write error
     *
     * Test Plan Reference: Test case #94 (test_alert_fatal_fifo_write_error)
     *
     * Procedure: Use ERR_CODE_TEST to force FIFO_WRITE_ERR (bit 3).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_fifo_write_error();

    // =========================================================================
    // Test Case 16: Fatal Alert - Internal FIFO Read Error
    // =========================================================================
    /**
     * @brief Verify fatal alert on internal FIFO read error
     *
     * Test Plan Reference: Test case #95 (test_alert_fatal_fifo_read_error)
     *
     * Procedure: Use ERR_CODE_TEST to force FIFO_READ_ERR (bit 4).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_fifo_read_error();

    // =========================================================================
    // Test Case 17: Fatal Alert - Internal FIFO State Error
    // =========================================================================
    /**
     * @brief Verify fatal alert on internal FIFO state error
     *
     * Test Plan Reference: Test case #96 (test_alert_fatal_fifo_state_error)
     *
     * Procedure: Use ERR_CODE_TEST to force FIFO_STATE_ERR (bit 5).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_fifo_state_error();

    // =========================================================================
    // Test Case 18: Fatal Alert - Hardened Counter Error
    // =========================================================================
    /**
     * @brief Verify fatal alert on hardened counter error
     *
     * Test Plan Reference: Test case #97 (test_alert_fatal_counter_error)
     *
     * Procedure: Use ERR_CODE_TEST to force EDN_CNTR_ERR (bit 22).
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_counter_error();

    // =========================================================================
    // Test Case 19: Fatal Alert Persists Until Reset
    // =========================================================================
    /**
     * @brief Verify fatal alert remains asserted until system reset
     *
     * Test Objective: Validate that fatal alert signal remains asserted as long
     * as any ERR_CODE bit is set, and only clears on system reset.
     *
     * Test Plan Reference: Test case #98 (test_alert_fatal_sticky_until_reset)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Trigger multiple fatal errors via ERR_CODE_TEST
     * 2. Verify ERR_CODE has multiple bits set
     * 3. Verify alert_fatal_alert is asserted
     * 4. Clear interrupt status bits
     * 5. Verify fatal alert remains asserted
     * 6. Attempt to write to ERR_CODE (no effect)
     * 7. Verify fatal alert still asserted
     * 8. Apply system reset
     * 9. Verify ERR_CODE cleared to 0x0
     * 10. Verify alert_fatal_alert de-asserted
     *
     * Pass Criteria:
     * - Fatal alert persists as long as ERR_CODE has bits set
     * - Alert cannot be cleared by register writes
     * - Only reset clears ERR_CODE and de-asserts fatal alert
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_sticky_until_reset();

    // =========================================================================
    // Test Case 20: ALERT_TEST No Status Register Modification
    // =========================================================================
    /**
     * @brief Verify ALERT_TEST pulses alerts without modifying status registers
     *
     * Test Objective: Validate that ALERT_TEST register pulses alert signals
     * for testing without setting RECOV_ALERT_STS or ERR_CODE status bits.
     *
     * Test Plan Reference: Test case #116 (test_corner_alert_test_no_status_change)
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Read initial RECOV_ALERT_STS and ERR_CODE values
     * 2. Write to ALERT_TEST to pulse both alerts
     * 3. Observe alert signals pulse (brief assertion)
     * 4. Read RECOV_ALERT_STS and verify unchanged
     * 5. Read ERR_CODE and verify unchanged
     * 6. Verify no interrupt triggered (status bits not set)
     * 7. Test corner case: ALERT_TEST during active real alert
     * 8. Verify real alert status unaffected by ALERT_TEST
     *
     * Pass Criteria:
     * - ALERT_TEST pulses alert signals
     * - No modification to RECOV_ALERT_STS
     * - No modification to ERR_CODE
     * - No interrupt generation
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_test_no_status_change();

    // =========================================================================
    // Test Case 21: Fatal Alerts Trigger Interrupt
    // =========================================================================
    /**
     * @brief Verify fatal alerts trigger intr_edn_fatal_err interrupt
     *
     * Test Objective: Validate that all fatal alert conditions trigger the
     * edn_fatal_err interrupt when INTR_ENABLE is set.
     *
     * Test Plan Reference: Integration test for EDN_FUNC_009 and EDN_FUNC_010
     * Functionality: EDN_FUNC_010, EDN_FUNC_009
     *
     * Procedure:
     * 1. Enable fatal error interrupt (INTR_ENABLE.edn_fatal_err=1)
     * 2. Test each fatal error type triggers interrupt:
     *    a. FIFO overflow errors (RESCMD, GENCMD)
     *    b. State machine errors (MAIN_SM, ACK_SM)
     *    c. Internal FIFO errors (WRITE, READ, STATE)
     *    d. Counter error
     * 3. Verify INTR_STATE.edn_fatal_err set for each
     * 4. Verify intr_edn_fatal_err signal asserts
     * 5. Verify interrupt can be cleared (W1C) but ERR_CODE persists
     * 6. Verify interrupt disabled when INTR_ENABLE cleared
     *
     * Pass Criteria:
     * - All fatal errors trigger interrupt when enabled
     * - Interrupt can be cleared independently of ERR_CODE
     * - INTR_ENABLE controls interrupt signal assertion
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_triggers_interrupt();

    // =========================================================================
    // Test Case 22: Multiple Errors Simultaneously
    // =========================================================================
    /**
     * @brief Verify multiple error conditions can occur simultaneously
     *
     * Test Objective: Validate that multiple fatal errors and recoverable alerts
     * can be active simultaneously with correct status accumulation.
     *
     * Test Plan Reference: Integration test for EDN_FUNC_010
     * Functionality: EDN_FUNC_010
     *
     * Procedure:
     * 1. Trigger multiple recoverable alerts (invalid CTRL fields)
     * 2. Verify RECOV_ALERT_STS has multiple bits set
     * 3. Verify alert_recov_alert asserted
     * 4. Trigger multiple fatal errors (FIFO overflow + ERR_CODE_TEST)
     * 5. Verify ERR_CODE has multiple bits set
     * 6. Verify alert_fatal_alert asserted
     * 7. Verify both alert signals asserted simultaneously
     * 8. Clear recoverable alerts individually (W0C)
     * 9. Verify fatal alerts persist
     * 10. Apply reset and verify all errors cleared
     *
     * Pass Criteria:
     * - Multiple errors accumulate in status registers
     * - Both alert signals can be active simultaneously
     * - Recoverable alerts clear independently
     * - Fatal errors persist until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_multiple_errors_simultaneous();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Verify alert signal state matches expected value
     * @param alert_type 0=recoverable, 1=fatal
     * @param expected Expected signal value (true=asserted, false=de-asserted)
     * @return true if signal matches expected, false otherwise
     */
    bool verify_alert_signal(uint32_t alert_type, bool expected);

    /**
     * @brief Trigger FIFO overflow error for fatal alert testing
     * @param fifo_type 0=RESEED_CMD, 1=GENERATE_CMD
     * @return true if overflow triggered successfully
     */
    bool trigger_fifo_overflow(uint32_t fifo_type);

    /**
     * @brief Write invalid multi-bit encoded value to CTRL field
     * @param field_offset Bit offset in CTRL register (0, 4, 8, or 12)
     * @return true if write successful
     */
    bool write_invalid_ctrl_field(uint32_t field_offset);

    /**
     * @brief Force fatal error via ERR_CODE_TEST
     * @param err_bit ERR_CODE bit position to set
     * @return true if error forced successfully
     */
    bool force_fatal_error(uint32_t err_bit);

    /**
     * @brief Verify register bit is set
     * @param offset Register offset
     * @param bit_position Bit to check
     * @param expected Expected bit value
     * @return true if bit matches expected
     */
    bool verify_register_bit(uint32_t offset, uint32_t bit_position, bool expected);

    /**
     * @brief Clear recoverable alert status bit (W0C)
     * @param bit_position Bit position in RECOV_ALERT_STS
     * @return true if clear successful
     */
    bool clear_recoverable_alert(uint32_t bit_position);

    /**
     * @brief Verify register value matches expected
     * @param reg_name Register name for diagnostics
     * @param expected Expected value
     * @param actual Actual read value
     * @return true if values match
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
