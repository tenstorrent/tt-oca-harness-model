// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_002.h
 * @brief EDN_FUNC_002 Test Suite - Module Initialization and Configuration Verification
 *
 * Comprehensive test suite for EDN_FUNC_002 (Module Initialization and Configuration) covering:
 * - REGWEN register Write-0-to-Clear (W0C) mechanism
 * - CTRL register write protection enforcement via REGWEN
 * - Module enable/disable via CTRL.EDN_ENABLE field
 * - Multi-bit encoding validation for all 4 CTRL fields (0x6=enable, 0x9=disable)
 * - Boot-time request mode configuration and enablement
 * - Auto request mode configuration and enablement
 * - Software port mode enablement
 * - Command FIFO reset functionality
 * - Alert generation for invalid multi-bit field values (RECOV_ALERT_STS)
 * - Reset behavior restoration of REGWEN and CTRL
 *
 * Test Coverage:
 * - test_ctrl_edn_enable_valid: Validates EDN module enable/disable via EDN_ENABLE field
 * - test_ctrl_edn_enable_invalid: Tests invalid EDN_ENABLE values trigger recoverable alert
 * - test_ctrl_boot_req_mode_valid: Verifies boot-time mode selection valid values
 * - test_ctrl_boot_req_mode_invalid: Tests invalid BOOT_REQ_MODE triggers alert
 * - test_ctrl_auto_req_mode_valid: Validates auto request mode selection valid values
 * - test_ctrl_auto_req_mode_invalid: Tests invalid AUTO_REQ_MODE triggers alert
 * - test_ctrl_cmd_fifo_rst_valid: Verifies command FIFO reset with valid 0x6 value
 * - test_ctrl_cmd_fifo_rst_invalid: Tests invalid CMD_FIFO_RST triggers alert
 * - test_regwen_write_protection: Validates W0C mechanism locks CTRL permanently
 * - test_regwen_lock_enforcement: Tests CTRL writes blocked when REGWEN=0
 * - test_reset_restores_regwen: Confirms reset restores REGWEN to unlocked state
 * - test_initialization_sequence: Validates recommended initialization ordering
 *
 * @note This test suite implements all 12 test cases mapped to EDN_FUNC_002
 *       in the edn-functionality-testcases.md document.
 *
 * Implementation Strategy:
 * - Each test case validates specific CTRL register field behavior
 * - Tests verify multi-bit encoding validation (0x6=enable, 0x9=disable)
 * - Invalid values tested to ensure RECOV_ALERT_STS bits are set
 * - REGWEN protection mechanism thoroughly validated
 * - Alert signal assertions verified for configuration errors
 * - Reset behavior validates configuration restoration
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-12
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_002
 * @brief Test fixture for EDN_FUNC_002 verification
 *
 * Extends edn_test to provide comprehensive module initialization and configuration testing.
 * Implements all 12 test cases for EDN_FUNC_002 covering CTRL register fields, REGWEN
 * protection, multi-bit encoding validation, and alert generation.
 */
class test_edn_func_002 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_002(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_002();

    /**
     * @brief Execute all EDN_FUNC_002 test cases
     * @return Number of failed tests
     *
     * Runs all 12 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: CTRL.EDN_ENABLE Valid Values (0x6=Enable, 0x9=Disable)
    // =========================================================================
    /**
     * @brief Verify CTRL.EDN_ENABLE field accepts valid multi-bit encoded values
     *
     * Test Objective: Validate that EDN_ENABLE field [3:0] correctly accepts
     * valid values 0x6 (enable) and 0x9 (disable) without triggering alerts.
     *
     * Test Plan Reference: test_ctrl_edn_enable_valid
     * Functionality: EDN_FUNC_002 (Module Initialization and Configuration)
     *
     * Procedure:
     * 1. Ensure REGWEN=1 (unlocked)
     * 2. Read CTRL reset value (should be 0x9999 with EDN_ENABLE=0x9)
     * 3. Write CTRL with EDN_ENABLE=0x6 (enable), other fields=0x9
     * 4. Read back CTRL and verify EDN_ENABLE=0x6
     * 5. Check RECOV_ALERT_STS - no EDN_ENABLE_FIELD_ALERT should be set
     * 6. Verify MAIN_SM_STATE transitions from Idle (0xC1)
     * 7. Write CTRL with EDN_ENABLE=0x9 (disable)
     * 8. Read back and verify EDN_ENABLE=0x9
     * 9. Verify MAIN_SM_STATE returns to Idle
     * 10. Check no alerts generated
     *
     * Pass Criteria:
     * - EDN_ENABLE=0x6 enables module without alert
     * - EDN_ENABLE=0x9 disables module without alert
     * - State machine transitions correctly
     * - No RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT set
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_edn_enable_valid();

    // =========================================================================
    // Test Case 2: CTRL.EDN_ENABLE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify invalid EDN_ENABLE values trigger EDN_ENABLE_FIELD_ALERT
     *
     * Test Objective: Validate that writing invalid values to EDN_ENABLE field
     * (anything other than 0x6 or 0x9) sets RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT
     * and asserts the recoverable alert signal.
     *
     * Test Plan Reference: Derived from multi-bit encoding validation
     * Functionality: EDN_FUNC_002, EDN_FUNC_006 (Multi-bit Encoding)
     *
     * Procedure:
     * 1. Clear RECOV_ALERT_STS register
     * 2. Write CTRL with EDN_ENABLE=0x5 (invalid value)
     * 3. Wait for alert propagation
     * 4. Read RECOV_ALERT_STS and verify EDN_ENABLE_FIELD_ALERT bit [0] is set
     * 5. Verify alert_recov_alert signal asserted (if observable)
     * 6. Clear alert by writing 0 to RECOV_ALERT_STS bit [0]
     * 7. Test additional invalid values (0x0, 0x7, 0xA, 0xF)
     * 8. Verify alert triggered for each invalid value
     *
     * Pass Criteria:
     * - Invalid EDN_ENABLE values trigger EDN_ENABLE_FIELD_ALERT
     * - Recoverable alert signal asserted
     * - Alert can be cleared via W0C to RECOV_ALERT_STS
     * - Valid values (0x6, 0x9) do not trigger alert
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_edn_enable_invalid();

    // =========================================================================
    // Test Case 3: CTRL.BOOT_REQ_MODE Valid Values
    // =========================================================================
    /**
     * @brief Verify CTRL.BOOT_REQ_MODE field accepts valid values
     *
     * Test Objective: Validate BOOT_REQ_MODE field [7:4] accepts 0x6 (enable)
     * and 0x9 (disable) without alerts, enabling boot-time request mode.
     *
     * Test Plan Reference: test_ctrl_boot_req_mode_valid
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Read CTRL reset value (BOOT_REQ_MODE=0x9, disabled)
     * 2. Write CTRL with EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6, others=0x9
     * 3. Read back and verify BOOT_REQ_MODE=0x6
     * 4. Check no BOOT_REQ_MODE_FIELD_ALERT in RECOV_ALERT_STS
     * 5. Verify MAIN_SM_STATE transitions to BootInsAckWait state
     * 6. Clear BOOT_REQ_MODE (set to 0x9) while EDN_ENABLE=0x6
     * 7. Poll MAIN_SM_STATE until transition to SWPortMode
     * 8. Verify no alerts generated
     *
     * Pass Criteria:
     * - BOOT_REQ_MODE=0x6 enables boot-time mode
     * - State machine enters BootInsAckWait state
     * - Clearing BOOT_REQ_MODE causes transition to SWPortMode
     * - No alerts for valid values
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_boot_req_mode_valid();

    // =========================================================================
    // Test Case 4: CTRL.BOOT_REQ_MODE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify invalid BOOT_REQ_MODE values trigger BOOT_REQ_MODE_FIELD_ALERT
     *
     * Test Objective: Validate invalid BOOT_REQ_MODE values set
     * RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT and assert recoverable alert.
     *
     * Test Plan Reference: Multi-bit encoding validation
     * Functionality: EDN_FUNC_002, EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with BOOT_REQ_MODE=0x5 (invalid)
     * 3. Read RECOV_ALERT_STS and verify BOOT_REQ_MODE_FIELD_ALERT bit [1] set
     * 4. Clear alert
     * 5. Test additional invalid values (0x0, 0x7, 0xA, 0xF)
     * 6. Verify each triggers alert
     *
     * Pass Criteria:
     * - Invalid values trigger BOOT_REQ_MODE_FIELD_ALERT
     * - Recoverable alert asserted
     * - Alerts clearable via W0C
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_boot_req_mode_invalid();

    // =========================================================================
    // Test Case 5: CTRL.AUTO_REQ_MODE Valid Values
    // =========================================================================
    /**
     * @brief Verify CTRL.AUTO_REQ_MODE field accepts valid values
     *
     * Test Objective: Validate AUTO_REQ_MODE field [11:8] accepts 0x6 and 0x9
     * without alerts, enabling auto request mode operation.
     *
     * Test Plan Reference: test_ctrl_auto_req_mode_valid
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Read CTRL reset value (AUTO_REQ_MODE=0x9)
     * 2. Configure prerequisites for auto mode:
     *    - Write valid command to GENERATE_CMD FIFO
     *    - Write valid command to RESEED_CMD FIFO
     *    - Set MAX_NUM_REQS_BETWEEN_RESEEDS to non-zero value
     * 3. Write CTRL with EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6, others=0x9
     * 4. Read back and verify AUTO_REQ_MODE=0x6
     * 5. Check no AUTO_REQ_MODE_FIELD_ALERT
     * 6. Verify MAIN_SM_STATE transitions to AutoLoadIns state
     * 7. Clear AUTO_REQ_MODE (set to 0x9)
     * 8. Verify transition to SWPortMode
     *
     * Pass Criteria:
     * - AUTO_REQ_MODE=0x6 enables auto request mode
     * - State machine enters AutoLoadIns state
     * - Clearing AUTO_REQ_MODE causes SWPortMode transition
     * - No alerts for valid values
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_auto_req_mode_valid();

    // =========================================================================
    // Test Case 6: CTRL.AUTO_REQ_MODE Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify invalid AUTO_REQ_MODE values trigger AUTO_REQ_MODE_FIELD_ALERT
     *
     * Test Objective: Validate invalid AUTO_REQ_MODE values set
     * RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT.
     *
     * Test Plan Reference: Multi-bit encoding validation
     * Functionality: EDN_FUNC_002, EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with AUTO_REQ_MODE=0x5 (invalid)
     * 3. Read RECOV_ALERT_STS and verify AUTO_REQ_MODE_FIELD_ALERT bit [2] set
     * 4. Clear alert
     * 5. Test additional invalid values
     * 6. Verify alerts triggered
     *
     * Pass Criteria:
     * - Invalid values trigger AUTO_REQ_MODE_FIELD_ALERT
     * - Recoverable alert asserted
     * - Alerts clearable
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_auto_req_mode_invalid();

    // =========================================================================
    // Test Case 7: CTRL.CMD_FIFO_RST Valid Values
    // =========================================================================
    /**
     * @brief Verify CTRL.CMD_FIFO_RST field clears FIFOs with valid 0x6 value
     *
     * Test Objective: Validate CMD_FIFO_RST field [15:12] accepts 0x6 to clear
     * GENERATE_CMD and RESEED_CMD FIFOs without triggering alerts.
     *
     * Test Plan Reference: test_ctrl_cmd_fifo_rst_valid
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Write test data to GENERATE_CMD FIFO
     * 2. Write test data to RESEED_CMD FIFO
     * 3. Write CTRL with CMD_FIFO_RST=0x6
     * 4. Read back CTRL and verify CMD_FIFO_RST field
     * 5. Check no CMD_FIFO_RST_FIELD_ALERT in RECOV_ALERT_STS
     * 6. Attempt to use auto mode - should require FIFO reconfiguration
     * 7. Write CMD_FIFO_RST=0x9 (idle value)
     * 8. Verify no alert
     *
     * Pass Criteria:
     * - CMD_FIFO_RST=0x6 clears both FIFOs
     * - No alert generated for valid values
     * - FIFOs must be reconfigured after reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_cmd_fifo_rst_valid();

    // =========================================================================
    // Test Case 8: CTRL.CMD_FIFO_RST Invalid Values Trigger Alert
    // =========================================================================
    /**
     * @brief Verify invalid CMD_FIFO_RST values trigger CMD_FIFO_RST_FIELD_ALERT
     *
     * Test Objective: Validate invalid CMD_FIFO_RST values set
     * RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT.
     *
     * Test Plan Reference: Multi-bit encoding validation
     * Functionality: EDN_FUNC_002, EDN_FUNC_006
     *
     * Procedure:
     * 1. Clear RECOV_ALERT_STS
     * 2. Write CTRL with CMD_FIFO_RST=0x5 (invalid)
     * 3. Read RECOV_ALERT_STS and verify CMD_FIFO_RST_FIELD_ALERT bit [3] set
     * 4. Clear alert
     * 5. Test additional invalid values
     * 6. Verify alerts triggered
     *
     * Pass Criteria:
     * - Invalid values trigger CMD_FIFO_RST_FIELD_ALERT
     * - Recoverable alert asserted
     * - Alerts clearable
     *
     * @return true if test passes, false otherwise
     */
    bool test_ctrl_cmd_fifo_rst_invalid();

    // =========================================================================
    // Test Case 9: REGWEN Write Protection Mechanism
    // =========================================================================
    /**
     * @brief Verify REGWEN W0C mechanism permanently locks CTRL register
     *
     * Test Objective: Validate REGWEN Write-0-to-Clear mechanism prevents all
     * writes to CTRL register until system reset.
     *
     * Test Plan Reference: test_regwen_write_protection
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Read REGWEN reset value (should be 0x1, unlocked)
     * 2. Configure CTRL with known value (e.g., 0x9999)
     * 3. Read back CTRL to confirm write succeeded
     * 4. Write 0x0 to REGWEN (lock CTRL)
     * 5. Read REGWEN - should be 0x0
     * 6. Attempt to write different value to CTRL (e.g., 0x9996)
     * 7. Read CTRL and verify value unchanged (still 0x9999)
     * 8. Attempt to write 0x1 to REGWEN (should have no effect)
     * 9. Read REGWEN - should still be 0x0
     * 10. Attempt multiple CTRL writes with various values
     * 11. Verify all writes blocked
     *
     * Pass Criteria:
     * - REGWEN defaults to 0x1 after reset
     * - Writing 0 to REGWEN locks CTRL
     * - REGWEN cannot be set back to 1
     * - All CTRL writes blocked when REGWEN=0
     * - Only reset restores REGWEN=1
     *
     * @return true if test passes, false otherwise
     */
    bool test_regwen_write_protection();

    // =========================================================================
    // Test Case 10: REGWEN Lock Enforcement
    // =========================================================================
    /**
     * @brief Verify CTRL write protection enforced when REGWEN=0
     *
     * Test Objective: Validate comprehensive CTRL field write blocking when
     * REGWEN is locked, including all field combinations.
     *
     * Test Plan Reference: test_regwen_lock_enforcement
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Configure CTRL with specific field values:
     *    - EDN_ENABLE=0x6, BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9, CMD_FIFO_RST=0x9
     * 2. Lock CTRL by writing 0 to REGWEN
     * 3. Attempt to change EDN_ENABLE to 0x9
     * 4. Read CTRL - verify EDN_ENABLE still 0x6
     * 5. Attempt to change BOOT_REQ_MODE to 0x6
     * 6. Read CTRL - verify BOOT_REQ_MODE still 0x9
     * 7. Attempt to change AUTO_REQ_MODE to 0x6
     * 8. Verify no change
     * 9. Attempt to set CMD_FIFO_RST to 0x6
     * 10. Verify no change
     * 11. Attempt to write all fields simultaneously
     * 12. Verify entire CTRL register unchanged
     *
     * Pass Criteria:
     * - No CTRL field can be modified when REGWEN=0
     * - All field combinations remain locked
     * - No alerts or errors generated by blocked writes
     * - Module state unaffected by write attempts
     *
     * @return true if test passes, false otherwise
     */
    bool test_regwen_lock_enforcement();

    // =========================================================================
    // Test Case 11: Reset Restores REGWEN to Unlocked State
    // =========================================================================
    /**
     * @brief Verify system reset restores REGWEN to unlocked state
     *
     * Test Objective: Validate that system reset via rst_ni signal restores
     * REGWEN to 0x1 (unlocked) and CTRL to reset value, re-enabling configuration.
     *
     * Test Plan Reference: test_reset_restores_regwen
     * Functionality: EDN_FUNC_002
     *
     * Procedure:
     * 1. Configure CTRL with specific value
     * 2. Lock CTRL by clearing REGWEN
     * 3. Verify CTRL writes are blocked
     * 4. Apply system reset via rst_ni signal
     * 5. Wait for reset propagation
     * 6. Read REGWEN - should be 0x1 (unlocked)
     * 7. Read CTRL - should be reset value 0x9999
     * 8. Attempt to write new value to CTRL
     * 9. Read back CTRL and verify write succeeded
     * 10. Verify full configuration functionality restored
     *
     * Pass Criteria:
     * - Reset restores REGWEN to 0x1
     * - Reset restores CTRL to 0x9999
     * - CTRL writes functional after reset
     * - All configuration capabilities restored
     *
     * @return true if test passes, false otherwise
     */
    bool test_reset_restores_regwen();

    // =========================================================================
    // Test Case 12: Recommended Initialization Sequence
    // =========================================================================
    /**
     * @brief Verify recommended initialization ordering (ENTROPY_SRC → CSRNG → EDN)
     *
     * Test Objective: Validate the recommended initialization sequence ensures
     * proper dependency ordering and module readiness.
     *
     * Test Plan Reference: test_initialization_sequence
     * Functionality: EDN_FUNC_002
     *
     * Note: This test is conceptual for EDN-only testbench. In full system
     * testbench with ENTROPY_SRC and CSRNG models, this would validate actual
     * initialization ordering. For EDN standalone testing, we validate that
     * EDN can be enabled and configured correctly.
     *
     * Procedure:
     * 1. Verify EDN starts in Idle state after reset
     * 2. Read MAIN_SM_STATE - should be 0xC1 (Idle)
     * 3. Enable EDN via CTRL.EDN_ENABLE=0x6 (simulating post-CSRNG-enable)
     * 4. Verify state machine transitions from Idle
     * 5. Configure operating mode (software port mode for testing)
     * 6. Verify MAIN_SM_STATE transitions to SWPortMode
     * 7. Verify EDN ready for command acceptance
     * 8. Check SW_CMD_STS.CMD_RDY indicates readiness
     *
     * Pass Criteria:
     * - EDN initializes in Idle state after reset
     * - Module enablement causes state transition
     * - Configuration succeeds without errors
     * - EDN enters operational state correctly
     *
     * @return true if test passes, false otherwise
     */
    bool test_initialization_sequence();

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
    bool verify_bit_value(const std::string& reg_name, unsigned int bit_position,
                          unsigned int expected_value, uint32_t actual_reg);

    /**
     * @brief Clear all recoverable alert status bits
     *
     * Writes 0 to all bits in RECOV_ALERT_STS to clear any pending alerts.
     */
    void clear_recoverable_alerts();

    /**
     * @brief Wait for state machine transition with timeout
     * @param target_state Expected state value
     * @param timeout_ns Timeout in nanoseconds
     * @return true if state reached, false if timeout
     */
    bool wait_for_state(uint32_t target_state, uint64_t timeout_ns);

    /**
     * @brief Report test result with detailed information
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name, bool passed, const std::string& message = "");

    // =========================================================================
    // Test Constants
    // =========================================================================

    /// Valid multi-bit encoded enable value (0x6 = 0b0110)
    static constexpr uint32_t MULTIBIT_ENABLE = 0x6;

    /// Valid multi-bit encoded disable value (0x9 = 0b1001)
    static constexpr uint32_t MULTIBIT_DISABLE = 0x9;

    /// CTRL field bit positions
    static constexpr uint32_t EDN_ENABLE_SHIFT = 0;
    static constexpr uint32_t BOOT_REQ_MODE_SHIFT = 4;
    static constexpr uint32_t AUTO_REQ_MODE_SHIFT = 8;
    static constexpr uint32_t CMD_FIFO_RST_SHIFT = 12;

    /// CTRL field masks
    static constexpr uint32_t EDN_ENABLE_MASK = 0xF;
    static constexpr uint32_t BOOT_REQ_MODE_MASK = 0xF0;
    static constexpr uint32_t AUTO_REQ_MODE_MASK = 0xF00;
    static constexpr uint32_t CMD_FIFO_RST_MASK = 0xF000;

    /// RECOV_ALERT_STS bit positions
    static constexpr uint32_t EDN_ENABLE_FIELD_ALERT_BIT = 0;
    static constexpr uint32_t BOOT_REQ_MODE_FIELD_ALERT_BIT = 1;
    static constexpr uint32_t AUTO_REQ_MODE_FIELD_ALERT_BIT = 2;
    static constexpr uint32_t CMD_FIFO_RST_FIELD_ALERT_BIT = 3;

    /// State machine state values (from MAIN_SM_STATE register)
    static constexpr uint32_t STATE_IDLE = 0xC1;
    static constexpr uint32_t STATE_BOOT_INS_ACK_WAIT = 0x36;
    static constexpr uint32_t STATE_AUTO_LOAD_INS = 0x63;
    static constexpr uint32_t STATE_SW_PORT_MODE = 0x9C;

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;       ///< Total tests executed
    unsigned int m_tests_passed;    ///< Tests that passed
    unsigned int m_tests_failed;    ///< Tests that failed
    std::vector<std::string> m_failed_tests; ///< List of failed test names
};
