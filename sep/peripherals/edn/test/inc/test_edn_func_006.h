// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_006.h
 * @brief EDN_FUNC_006 Test Suite - Multi-bit Encoding Validation
 *
 * Comprehensive test suite for EDN_FUNC_006 (Multi-bit Encoding Validation) covering:
 * - Valid multi-bit encoded values (0x6=enable/true, 0x9=disable/false)
 * - Invalid multi-bit encoded values (all non-{0x6, 0x9} values)
 * - RECOV_ALERT_STS register bit mapping for multi-bit field alerts
 * - Recoverable alert signal (alert_recov_alert) assertion and deassertion
 * - W0C (Write-0-to-Clear) semantics for RECOV_ALERT_STS register
 * - Multiple simultaneous invalid field violations
 * - Alert clearing and deassertion verification
 * - Multi-bit encoding validation for all 4 CTRL register fields:
 *   - EDN_ENABLE [bits 3:0]
 *   - BOOT_REQ_MODE [bits 7:4]
 *   - AUTO_REQ_MODE [bits 11:8]
 *   - CMD_FIFO_RST [bits 15:12]
 *
 * Test Coverage (13 test cases):
 * - T1:  EDN_ENABLE valid values (0x6, 0x9) → no alerts
 * - T2:  EDN_ENABLE invalid values → EDN_ENABLE_FIELD_ALERT [bit 0]
 * - T3:  BOOT_REQ_MODE valid values (0x6, 0x9) → no alerts
 * - T4:  BOOT_REQ_MODE invalid values → BOOT_REQ_MODE_FIELD_ALERT [bit 1]
 * - T5:  AUTO_REQ_MODE valid values (0x6, 0x9) → no alerts
 * - T6:  AUTO_REQ_MODE invalid values → AUTO_REQ_MODE_FIELD_ALERT [bit 2]
 * - T7:  CMD_FIFO_RST valid values (0x6, 0x9) → no alerts
 * - T8:  CMD_FIFO_RST invalid values → CMD_FIFO_RST_FIELD_ALERT [bit 3]
 * - T9:  Multiple invalid fields simultaneously → multiple alert bits set
 * - T10: W0C clearing mechanism → write 0 clears, write 1 has no effect
 * - T11: Alert signal deassertion → alert clears when all bits cleared
 * - T12: Valid value after invalid → alert remains until firmware clears
 * - T13: Comprehensive invalid value coverage → all invalid encodings tested
 *
 * @note This test suite implements all 13 test cases mapped to EDN_FUNC_006
 *       in the edn-functionality-testcases.md document.
 *
 * Implementation Strategy:
 * - Each test validates specific multi-bit field encoding
 * - Tests verify RECOV_ALERT_STS bit mapping matches field violations
 * - W0C semantics rigorously tested (write 1 = no effect, write 0 = clear)
 * - Alert signal monitoring confirms proper assertion/deassertion
 * - Comprehensive coverage of all 14 invalid values per field (0x0-0xF except 0x6, 0x9)
 * - Multiple simultaneous violations tested to verify independent alert tracking
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-13
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_006
 * @brief Test fixture for EDN_FUNC_006 verification
 *
 * Extends edn_test to provide comprehensive multi-bit encoding validation testing.
 * Implements all 13 test cases for EDN_FUNC_006 covering valid/invalid encodings,
 * alert generation, RECOV_ALERT_STS W0C semantics, and alert signal behavior.
 */
class test_edn_func_006 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_006(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_006();

    /**
     * @brief Execute all EDN_FUNC_006 test cases
     * @return Number of failed tests
     *
     * Runs all 13 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: EDN_ENABLE Valid Values
    // =========================================================================
    /**
     * @brief Verify EDN_ENABLE field accepts valid values without alerts
     *
     * Test Objective: Validate that CTRL.EDN_ENABLE field [bits 3:0] accepts
     * valid multi-bit encoded values 0x6 (enable) and 0x9 (disable) without
     * triggering EDN_ENABLE_FIELD_ALERT in RECOV_ALERT_STS register.
     *
     * Test Plan Reference: Test case #8
     * Functionality: EDN_FUNC_006 (Multi-bit Encoding Validation)
     *
     * Procedure:
     * 1. Clear any existing recoverable alerts
     * 2. Write CTRL with EDN_ENABLE=0x6, other fields=0x9
     * 3. Wait for validation (1 clock cycle)
     * 4. Read RECOV_ALERT_STS and verify bit [0] = 0 (no alert)
     * 5. Read alert_recov_alert signal and verify not asserted
     * 6. Write CTRL with EDN_ENABLE=0x9, other fields=0x9
     * 7. Wait for validation
     * 8. Read RECOV_ALERT_STS and verify bit [0] = 0 (no alert)
     * 9. Verify alert_recov_alert signal remains deasserted
     *
     * Pass Criteria:
     * - EDN_ENABLE=0x6 and 0x9 accepted without alerts
     * - RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT [bit 0] remains 0
     * - alert_recov_alert signal not asserted
     *
     * @return true if test passes, false otherwise
     */
    bool test_edn_enable_valid_values();

    // =========================================================================
    // Test Case 2: EDN_ENABLE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify EDN_ENABLE invalid values trigger EDN_ENABLE_FIELD_ALERT
     *
     * Test Objective: Validate that CTRL.EDN_ENABLE field with invalid values
     * (anything other than 0x6 or 0x9) triggers EDN_ENABLE_FIELD_ALERT [bit 0]
     * in RECOV_ALERT_STS and asserts alert_recov_alert signal.
     *
     * Test Plan Reference: Test case #9
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * For each invalid value (0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA-0xF):
     * 1. Clear RECOV_ALERT_STS register
     * 2. Write CTRL with EDN_ENABLE=invalid_value, other fields=0x9
     * 3. Wait for validation
     * 4. Read RECOV_ALERT_STS and verify bit [0] = 1
     * 5. Monitor alert_recov_alert signal assertion
     * 6. Clear alert by writing 0 to bit [0]
     * 7. Verify bit [0] cleared and alert_recov_alert deasserted
     *
     * Pass Criteria:
     * - All 14 invalid values trigger EDN_ENABLE_FIELD_ALERT
     * - RECOV_ALERT_STS bit [0] set for each invalid value
     * - alert_recov_alert signal asserts when alert active
     * - W0C clearing works correctly
     *
     * @return true if test passes, false otherwise
     */
    bool test_edn_enable_invalid_values();

    // =========================================================================
    // Test Case 3: BOOT_REQ_MODE Valid Values
    // =========================================================================
    /**
     * @brief Verify BOOT_REQ_MODE field accepts valid values without alerts
     *
     * Test Objective: Validate that CTRL.BOOT_REQ_MODE field [bits 7:4] accepts
     * valid values 0x6 and 0x9 without triggering BOOT_REQ_MODE_FIELD_ALERT.
     *
     * Test Plan Reference: Test case #10
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear recoverable alerts
     * 2. Write CTRL with BOOT_REQ_MODE=0x6, other fields=0x9
     * 3. Verify RECOV_ALERT_STS bit [1] = 0 (no alert)
     * 4. Write CTRL with BOOT_REQ_MODE=0x9
     * 5. Verify RECOV_ALERT_STS bit [1] = 0
     * 6. Verify alert_recov_alert not asserted
     *
     * Pass Criteria:
     * - BOOT_REQ_MODE=0x6 and 0x9 accepted without alerts
     * - RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT [bit 1] = 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_req_mode_valid_values();

    // =========================================================================
    // Test Case 4: BOOT_REQ_MODE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify BOOT_REQ_MODE invalid values trigger alert
     *
     * Test Objective: Validate that invalid BOOT_REQ_MODE values trigger
     * BOOT_REQ_MODE_FIELD_ALERT [bit 1] in RECOV_ALERT_STS.
     *
     * Test Plan Reference: Test case #11
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * For each invalid value (0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA-0xF):
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with BOOT_REQ_MODE=invalid_value, other fields=0x9
     * 3. Verify RECOV_ALERT_STS bit [1] = 1
     * 4. Clear alert using W0C
     *
     * Pass Criteria:
     * - All invalid values trigger BOOT_REQ_MODE_FIELD_ALERT
     * - Correct RECOV_ALERT_STS bit [1] mapping
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_req_mode_invalid_values();

    // =========================================================================
    // Test Case 5: AUTO_REQ_MODE Valid Values
    // =========================================================================
    /**
     * @brief Verify AUTO_REQ_MODE field accepts valid values without alerts
     *
     * Test Objective: Validate that CTRL.AUTO_REQ_MODE field [bits 11:8] accepts
     * valid values 0x6 and 0x9 without triggering AUTO_REQ_MODE_FIELD_ALERT.
     *
     * Test Plan Reference: Test case #12
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear recoverable alerts
     * 2. Write CTRL with AUTO_REQ_MODE=0x6, other fields=0x9
     * 3. Verify RECOV_ALERT_STS bit [2] = 0
     * 4. Write CTRL with AUTO_REQ_MODE=0x9
     * 5. Verify RECOV_ALERT_STS bit [2] = 0
     *
     * Pass Criteria:
     * - AUTO_REQ_MODE=0x6 and 0x9 accepted without alerts
     * - RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT [bit 2] = 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_req_mode_valid_values();

    // =========================================================================
    // Test Case 6: AUTO_REQ_MODE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify AUTO_REQ_MODE invalid values trigger alert
     *
     * Test Objective: Validate that invalid AUTO_REQ_MODE values trigger
     * AUTO_REQ_MODE_FIELD_ALERT [bit 2] in RECOV_ALERT_STS.
     *
     * Test Plan Reference: Test case #13
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * For each invalid value:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with AUTO_REQ_MODE=invalid_value
     * 3. Verify RECOV_ALERT_STS bit [2] = 1
     * 4. Clear alert
     *
     * Pass Criteria:
     * - All invalid values trigger AUTO_REQ_MODE_FIELD_ALERT
     * - Correct bit [2] mapping in RECOV_ALERT_STS
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_req_mode_invalid_values();

    // =========================================================================
    // Test Case 7: CMD_FIFO_RST Valid Values
    // =========================================================================
    /**
     * @brief Verify CMD_FIFO_RST field accepts valid values without alerts
     *
     * Test Objective: Validate that CTRL.CMD_FIFO_RST field [bits 15:12] accepts
     * valid values 0x6 and 0x9 without triggering CMD_FIFO_RST_FIELD_ALERT.
     *
     * Test Plan Reference: Test case #14
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear recoverable alerts
     * 2. Write CTRL with CMD_FIFO_RST=0x6, other fields=0x9
     * 3. Verify RECOV_ALERT_STS bit [3] = 0
     * 4. Write CTRL with CMD_FIFO_RST=0x9
     * 5. Verify RECOV_ALERT_STS bit [3] = 0
     *
     * Pass Criteria:
     * - CMD_FIFO_RST=0x6 and 0x9 accepted without alerts
     * - RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT [bit 3] = 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_fifo_rst_valid_values();

    // =========================================================================
    // Test Case 8: CMD_FIFO_RST Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify CMD_FIFO_RST invalid values trigger alert
     *
     * Test Objective: Validate that invalid CMD_FIFO_RST values trigger
     * CMD_FIFO_RST_FIELD_ALERT [bit 3] in RECOV_ALERT_STS.
     *
     * Test Plan Reference: Test case #15
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * For each invalid value:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with CMD_FIFO_RST=invalid_value
     * 3. Verify RECOV_ALERT_STS bit [3] = 1
     * 4. Clear alert
     *
     * Pass Criteria:
     * - All invalid values trigger CMD_FIFO_RST_FIELD_ALERT
     * - Correct bit [3] mapping in RECOV_ALERT_STS
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_fifo_rst_invalid_values();

    // =========================================================================
    // Test Case 9: Multiple Invalid Fields Simultaneously
    // =========================================================================
    /**
     * @brief Verify multiple invalid fields trigger multiple alert bits
     *
     * Test Objective: Validate that writing multiple invalid field values
     * simultaneously in CTRL register triggers corresponding alert bits
     * independently in RECOV_ALERT_STS register.
     *
     * Test Plan Reference: Corner case coverage
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with EDN_ENABLE=0x0 (invalid) and BOOT_REQ_MODE=0x1 (invalid)
     * 3. Verify RECOV_ALERT_STS bits [0] and [1] both set
     * 4. Clear alerts
     * 5. Write CTRL with all 4 fields invalid
     * 6. Verify RECOV_ALERT_STS bits [0:3] all set
     * 7. Verify alert_recov_alert signal asserted
     *
     * Pass Criteria:
     * - Multiple invalid fields trigger independent alert bits
     * - All 4 alert bits can be set simultaneously
     * - Alert signal asserts when any alert bit is set
     *
     * @return true if test passes, false otherwise
     */
    bool test_multiple_invalid_fields();

    // =========================================================================
    // Test Case 10: W0C Clearing Mechanism
    // =========================================================================
    /**
     * @brief Verify W0C semantics for RECOV_ALERT_STS register
     *
     * Test Objective: Validate Write-0-to-Clear semantics where writing 0
     * to a set bit clears it, but writing 1 has no effect.
     *
     * Test Plan Reference: Test case #30
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Trigger EDN_ENABLE_FIELD_ALERT (bit 0)
     * 2. Verify RECOV_ALERT_STS bit [0] = 1
     * 3. Write 0xFFFFFFFF (all 1's) to RECOV_ALERT_STS
     * 4. Read RECOV_ALERT_STS - bit [0] should still be 1 (no effect)
     * 5. Write 0xFFFFFFFE (bit 0 = 0, all others = 1) to RECOV_ALERT_STS
     * 6. Read RECOV_ALERT_STS - bit [0] should be 0 (cleared)
     * 7. Verify alert_recov_alert deasserted
     * 8. Test same for bits [1], [2], [3]
     *
     * Pass Criteria:
     * - Writing 1 to alert bit has no effect (bit remains set)
     * - Writing 0 to alert bit clears it
     * - Alert signal deasserts when all bits cleared
     *
     * @return true if test passes, false otherwise
     */
    bool test_w0c_clearing_mechanism();

    // =========================================================================
    // Test Case 11: Alert Signal Deassertion
    // =========================================================================
    /**
     * @brief Verify alert_recov_alert signal deasserts when all bits cleared
     *
     * Test Objective: Validate that alert_recov_alert signal is asserted
     * when any RECOV_ALERT_STS bit is set, and deasserts only when all
     * alert bits are cleared.
     *
     * Test Plan Reference: Alert behavior specification
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Set multiple RECOV_ALERT_STS bits (bits 0, 1, 2)
     * 2. Verify alert_recov_alert signal asserted
     * 3. Clear bit [0] only (write 0xFFFFFFFE)
     * 4. Verify alert_recov_alert still asserted (bits 1, 2 still set)
     * 5. Clear bit [1] (write 0xFFFFFFFD)
     * 6. Verify alert_recov_alert still asserted (bit 2 still set)
     * 7. Clear bit [2] (write 0xFFFFFFFB)
     * 8. Verify alert_recov_alert deasserted (all bits cleared)
     *
     * Pass Criteria:
     * - Alert asserts when any bit is set
     * - Alert remains asserted while any bit is set
     * - Alert deasserts only when all bits cleared
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_signal_deassertion();

    // =========================================================================
    // Test Case 12: Valid Value After Invalid
    // =========================================================================
    /**
     * @brief Verify alert remains until firmware clears despite valid write
     *
     * Test Objective: Validate that once an invalid value triggers an alert,
     * writing a valid value to the same field does NOT automatically clear
     * the alert - firmware must explicitly clear RECOV_ALERT_STS.
     *
     * Test Plan Reference: Alert persistence specification
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * 1. Write CTRL with EDN_ENABLE=0x5 (invalid)
     * 2. Verify RECOV_ALERT_STS bit [0] = 1
     * 3. Verify alert_recov_alert asserted
     * 4. Write CTRL with EDN_ENABLE=0x6 (valid)
     * 5. Read RECOV_ALERT_STS - bit [0] should still be 1 (sticky)
     * 6. Verify alert_recov_alert still asserted
     * 7. Clear alert by writing 0 to bit [0]
     * 8. Verify alert_recov_alert deasserted
     *
     * Pass Criteria:
     * - Alert bit is sticky (not cleared by valid value write)
     * - Alert signal remains asserted until firmware clears
     * - Firmware must explicitly clear RECOV_ALERT_STS
     *
     * @return true if test passes, false otherwise
     */
    bool test_valid_value_after_invalid();

    // =========================================================================
    // Test Case 13: Comprehensive Invalid Value Coverage
    // =========================================================================
    /**
     * @brief Verify all 14 invalid values trigger alerts for all 4 fields
     *
     * Test Objective: Comprehensive coverage test ensuring every invalid
     * encoding (0x0-0xF except 0x6, 0x9) triggers alerts for all 4 fields.
     *
     * Test Plan Reference: Comprehensive coverage
     * Functionality: EDN_FUNC_006
     *
     * Procedure:
     * For each field (EDN_ENABLE, BOOT_REQ_MODE, AUTO_REQ_MODE, CMD_FIFO_RST):
     *   For each invalid value (0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA-0xF):
     *     1. Clear RECOV_ALERT_STS
     *     2. Write CTRL with field=invalid_value
     *     3. Verify corresponding RECOV_ALERT_STS bit set
     *     4. Verify alert_recov_alert asserted
     *     5. Clear alert
     *
     * Pass Criteria:
     * - All 14 invalid values tested for all 4 fields (56 total tests)
     * - 100% coverage of invalid encoding space
     * - Consistent alert generation behavior across all fields
     *
     * @return true if test passes, false otherwise
     */
    bool test_comprehensive_invalid_coverage();

    // =========================================================================
    // Helper Functions
    // =========================================================================

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
     * @brief Clear all recoverable alert status bits using W0C
     *
     * Writes 0x00000000 to RECOV_ALERT_STS to clear all alert bits.
     */
    void clear_recoverable_alerts();

    /**
     * @brief Verify alert_recov_alert signal state
     * @param expected_state Expected signal state (true = asserted)
     * @return true if signal matches expected state
     */
    bool verify_alert_signal(bool expected_state);

    /**
     * @brief Test single invalid value for specific field
     * @param field_name Field name for diagnostics
     * @param field_shift Bit shift for field in CTRL register
     * @param invalid_value Invalid value to test (0-15, except 6 and 9)
     * @param alert_bit RECOV_ALERT_STS bit position for this field
     * @return true if test passes
     */
    bool test_single_invalid_value(const std::string& field_name,
                                   unsigned int field_shift,
                                   uint32_t invalid_value,
                                   unsigned int alert_bit);

    /**
     * @brief Report test result with detailed information
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name, bool passed, const std::string& message = "");

    // =========================================================================
    // Register Address and Field Definitions
    // =========================================================================

    // CTRL register field shifts
    static constexpr unsigned int EDN_ENABLE_SHIFT = 0;
    static constexpr unsigned int BOOT_REQ_MODE_SHIFT = 4;
    static constexpr unsigned int AUTO_REQ_MODE_SHIFT = 8;
    static constexpr unsigned int CMD_FIFO_RST_SHIFT = 12;

    // RECOV_ALERT_STS bit positions
    static constexpr unsigned int EDN_ENABLE_FIELD_ALERT_BIT = 0;
    static constexpr unsigned int BOOT_REQ_MODE_FIELD_ALERT_BIT = 1;
    static constexpr unsigned int AUTO_REQ_MODE_FIELD_ALERT_BIT = 2;
    static constexpr unsigned int CMD_FIFO_RST_FIELD_ALERT_BIT = 3;

    // Multi-bit encoding valid values
    static constexpr uint32_t MULTIBIT_ENABLE = 0x6;   // True/Enable/Reset
    static constexpr uint32_t MULTIBIT_DISABLE = 0x9;  // False/Disable/Idle

    // Register reset values
    static constexpr uint32_t CTRL_RESET = 0x9999;
    static constexpr uint32_t RECOV_ALERT_STS_RESET = 0x0;

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;                    ///< Total tests executed
    unsigned int m_tests_passed;                 ///< Tests that passed
    unsigned int m_tests_failed;                 ///< Tests that failed
    std::vector<std::string> m_failed_tests;     ///< List of failed test names
};
