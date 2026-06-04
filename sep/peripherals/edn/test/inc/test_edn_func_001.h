/**
 * @file test_edn_func_001.h
 * @brief EDN_FUNC_001 Test Suite - Register Access and TLM Interface Verification
 *
 * Comprehensive test suite for EDN_FUNC_001 (Register Access and TLM Interface) covering:
 * - Register reset value verification for all 17 registers
 * - Access type enforcement (RW, RO, WO, W1C, W0C)
 * - Reserved bit handling (read as 0, writes ignored)
 * - Register write protection via REGWEN mechanism
 * - Register side effects validation
 * - TLM-2.0 protocol compliance
 *
 * Test Coverage:
 * - test_register_reset_values: Validates all 17 register reset values
 * - test_intr_state_rw1c: Verifies Write-1-to-Clear for INTR_STATE
 * - test_intr_enable_rw: Tests read/write access for INTR_ENABLE
 * - test_intr_test_wo: Confirms write-only access for INTR_TEST
 * - test_alert_test_wo: Validates write-only access for ALERT_TEST
 * - test_regwen_write_protection: Tests W0C mechanism for REGWEN
 * - test_regwen_lock_enforcement: Validates CTRL write blocking when REGWEN=0
 * - test_boot_ins_cmd_rw: Verifies read/write for BOOT_INS_CMD
 * - test_boot_gen_cmd_rw: Confirms read/write for BOOT_GEN_CMD
 * - test_sw_cmd_req_wo: Validates write-only FIFO for SW_CMD_REQ
 * - test_sw_cmd_sts_cmd_reg_rdy: Tests CMD_REG_RDY read-only field
 * - test_sw_cmd_sts_cmd_rdy: Validates CMD_RDY read-only field
 * - test_reseed_cmd_fifo_wo: Confirms write-only FIFO for RESEED_CMD
 * - test_generate_cmd_fifo_wo: Validates write-only FIFO for GENERATE_CMD
 * - test_max_num_reqs_between_reseeds_rw: Tests read/write for reseed counter
 * - test_recov_alert_sts_rw0c: Verifies Write-0-to-Clear for RECOV_ALERT_STS
 * - test_err_code_sticky_ro: Validates read-only sticky behavior of ERR_CODE
 * - test_err_code_test_injection: Tests error injection via ERR_CODE_TEST
 * - test_main_sm_state_visibility: Confirms read-only access to MAIN_SM_STATE
 * - test_reserved_bits_read_zero: Validates reserved bit handling
 * - test_corner_ctrl_write_when_regwen_locked: Corner case for protected writes
 *
 * @note This test suite implements all 21 test cases mapped to EDN_FUNC_001
 *       in the edn-functionality-testcases.md document.
 *
 * Implementation Strategy:
 * - Each test case is self-contained with setup, execution, and validation phases
 * - Tests use edn_basetest infrastructure for register access
 * - Assertions validate expected vs. observed behavior
 * - Failed tests report detailed diagnostic information
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
 * @class test_edn_func_001
 * @brief Test fixture for EDN_FUNC_001 verification
 *
 * Extends edn_test to provide comprehensive register access testing.
 * Implements all 21 test cases for EDN_FUNC_001 covering register access patterns,
 * TLM interface compliance, and access type enforcement.
 */
class test_edn_func_001 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_001(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_001();

    /**
     * @brief Execute all EDN_FUNC_001 test cases
     * @return Number of failed tests
     *
     * Runs all 21 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Register Reset Values Verification
    // =========================================================================
    /**
     * @brief Verify all registers return correct reset values after system reset
     *
     * Test Objective: Validate that all 17 EDN registers have correct reset values
     * after system reset, ensuring proper TLM register initialization.
     *
     * Test Plan Reference: Test case #1
     * Functionality: EDN_FUNC_001 (Register Access and TLM Interface)
     *
     * Procedure:
     * 1. Apply system reset via rst_ni signal
     * 2. Read all 17 register addresses
     * 3. Compare read values against documented reset values
     * 4. Verify read-only fields return defined values
     * 5. Verify write-only fields return 0 or undefined behavior
     *
     * Pass Criteria:
     * - All readable registers return documented reset values
     * - REGWEN=0x1, CTRL=0x9999, MAIN_SM_STATE=0xC1
     * - Status registers=0x0
     *
     * @return true if test passes, false otherwise
     */
    bool test_register_reset_values();

    // =========================================================================
    // Test Case 2: INTR_STATE Write-1-to-Clear Verification
    // =========================================================================
    /**
     * @brief Verify INTR_STATE register Write-1-to-Clear mechanism
     *
     * Test Objective: Validate W1C semantics for edn_cmd_req_done and
     * edn_fatal_err bits in INTR_STATE register.
     *
     * Test Plan Reference: Test case #2
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Force interrupt bits via INTR_TEST register
     * 2. Read INTR_STATE to confirm bits are set
     * 3. Write 1 to edn_cmd_req_done bit
     * 4. Verify bit is cleared
     * 5. Write 0 to edn_fatal_err bit
     * 6. Verify bit remains set (no effect)
     * 7. Write 1 to edn_fatal_err bit
     * 8. Verify bit is cleared
     *
     * Pass Criteria:
     * - Writing 1 clears the interrupt bit
     * - Writing 0 has no effect
     * - Reserved bits unaffected by writes
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_state_rw1c();

    // =========================================================================
    // Test Case 3: INTR_ENABLE Read/Write Verification
    // =========================================================================
    /**
     * @brief Verify INTR_ENABLE register read/write functionality
     *
     * Test Objective: Validate read/write access for enabling edn_cmd_req_done
     * and edn_fatal_err interrupts.
     *
     * Test Plan Reference: Test case #3
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read INTR_ENABLE reset value (should be 0x0)
     * 2. Write 0x3 to enable both interrupt sources
     * 3. Read back and verify value is 0x3
     * 4. Write 0x1 to enable only edn_cmd_req_done
     * 5. Read back and verify value is 0x1
     * 6. Write 0x0 to disable all interrupts
     * 7. Read back and verify value is 0x0
     *
     * Pass Criteria:
     * - Register supports full read/write access
     * - Written values can be read back
     * - Reserved bits read as 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_enable_rw();

    // =========================================================================
    // Test Case 4: INTR_TEST Write-Only Verification
    // =========================================================================
    /**
     * @brief Verify INTR_TEST register forces interrupt status bits
     *
     * Test Objective: Validate write-only access for INTR_TEST and verify
     * writing 1 forces corresponding INTR_STATE bits.
     *
     * Test Plan Reference: Test case #4
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Clear INTR_STATE register
     * 2. Write 0x1 to INTR_TEST.edn_cmd_req_done
     * 3. Read INTR_STATE and verify edn_cmd_req_done bit is set
     * 4. Clear INTR_STATE
     * 5. Write 0x2 to INTR_TEST.edn_fatal_err
     * 6. Read INTR_STATE and verify edn_fatal_err bit is set
     * 7. Attempt to read INTR_TEST (should return 0 or undefined)
     *
     * Pass Criteria:
     * - Writing 1 to INTR_TEST forces INTR_STATE bits
     * - INTR_TEST is write-only (reads return 0)
     * - Reserved bits have no effect
     *
     * @return true if test passes, false otherwise
     */
    bool test_intr_test_wo();

    // =========================================================================
    // Test Case 5: ALERT_TEST Write-Only Verification
    // =========================================================================
    /**
     * @brief Verify ALERT_TEST register triggers alert signals
     *
     * Test Objective: Validate write-only access for ALERT_TEST and verify
     * writing 1 triggers recov_alert and fatal_alert signals.
     *
     * Test Plan Reference: Test case #5
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Monitor alert_recov_alert signal
     * 2. Write 0x1 to ALERT_TEST.recov_alert
     * 3. Verify alert_recov_alert signal assertion
     * 4. Monitor alert_fatal_alert signal
     * 5. Write 0x2 to ALERT_TEST.fatal_alert
     * 6. Verify alert_fatal_alert signal assertion
     * 7. Attempt to read ALERT_TEST (should return 0)
     *
     * Pass Criteria:
     * - Writing 1 triggers corresponding alert signal
     * - ALERT_TEST is write-only
     * - Alerts are pulsed without modifying status registers
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_test_wo();

    // =========================================================================
    // Test Case 6: REGWEN Write-0-to-Clear Verification
    // =========================================================================
    /**
     * @brief Verify REGWEN register Write-0-to-Clear mechanism
     *
     * Test Objective: Validate W0C mechanism permanently locks CTRL register
     * until reset.
     *
     * Test Plan Reference: Test case #6
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read REGWEN reset value (should be 0x1)
     * 2. Write test value to CTRL register
     * 3. Read back CTRL to confirm write succeeded
     * 4. Write 0x0 to REGWEN
     * 5. Read REGWEN (should be 0x0)
     * 6. Attempt to write different value to CTRL
     * 7. Read CTRL and verify value is unchanged
     * 8. Write 0x1 to REGWEN (should have no effect)
     * 9. Verify REGWEN remains 0x0
     *
     * Pass Criteria:
     * - REGWEN defaults to 0x1 after reset
     * - Writing 0 permanently locks CTRL writes
     * - REGWEN cannot be set back to 1 after clearing
     * - CTRL writes blocked when REGWEN=0
     *
     * @return true if test passes, false otherwise
     */
    bool test_regwen_write_protection();

    // =========================================================================
    // Test Case 7: REGWEN Lock Enforcement Verification
    // =========================================================================
    /**
     * @brief Verify writes to CTRL are blocked when REGWEN=0
     *
     * Test Objective: Validate that CTRL register write protection is enforced
     * when REGWEN bit is cleared.
     *
     * Test Plan Reference: Test case #7
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Configure CTRL with known value
     * 2. Lock CTRL by writing 0 to REGWEN
     * 3. Attempt multiple writes to CTRL with different values
     * 4. Read CTRL after each write attempt
     * 5. Verify CTRL value remains unchanged
     * 6. Apply reset
     * 7. Verify CTRL returns to reset value and REGWEN=0x1
     *
     * Pass Criteria:
     * - All write attempts to CTRL fail when REGWEN=0
     * - CTRL value remains constant
     * - Reset restores REGWEN to unlocked state
     *
     * @return true if test passes, false otherwise
     */
    bool test_regwen_lock_enforcement();

    // =========================================================================
    // Test Case 8: BOOT_INS_CMD Read/Write Verification
    // =========================================================================
    /**
     * @brief Verify BOOT_INS_CMD register read/write functionality
     *
     * Test Objective: Validate full 32-bit read/write access for boot-time
     * instantiate command word storage.
     *
     * Test Plan Reference: Test case #16
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read BOOT_INS_CMD reset value (should be 0x901)
     * 2. Write test command word (e.g., 0x12345678)
     * 3. Read back and verify value
     * 4. Write another value (e.g., 0xABCDEF00)
     * 5. Read back and verify
     * 6. Restore default value 0x901
     *
     * Pass Criteria:
     * - Reset value is 0x901
     * - Full 32-bit read/write access functional
     * - Written values are preserved until changed
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_ins_cmd_rw();

    // =========================================================================
    // Test Case 9: BOOT_GEN_CMD Read/Write Verification
    // =========================================================================
    /**
     * @brief Verify BOOT_GEN_CMD register read/write functionality
     *
     * Test Objective: Validate full 32-bit read/write access for boot-time
     * generate command word storage.
     *
     * Test Plan Reference: Test case #17
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read BOOT_GEN_CMD reset value (should be 0xFFF003)
     * 2. Write test command word (e.g., 0x87654321)
     * 3. Read back and verify value
     * 4. Write another value with different glen field
     * 5. Read back and verify
     * 6. Restore default value 0xFFF003
     *
     * Pass Criteria:
     * - Reset value is 0xFFF003
     * - Full 32-bit read/write access functional
     * - Default glen=0xFFF for 4K blocks
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_gen_cmd_rw();

    // =========================================================================
    // Test Case 10: SW_CMD_REQ Write-Only FIFO Verification
    // =========================================================================
    /**
     * @brief Verify SW_CMD_REQ register write-only FIFO interface
     *
     * Test Objective: Validate write-only access for SW_CMD_REQ and verify
     * command words can be written (reads should return 0).
     *
     * Test Plan Reference: Test case #18
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Attempt to read SW_CMD_REQ (should return 0 or undefined)
     * 2. Write command header word to SW_CMD_REQ
     * 3. Verify write does not generate error
     * 4. Write additional data words
     * 5. Attempt to read SW_CMD_REQ again
     * 6. Verify reads still return 0
     *
     * Pass Criteria:
     * - SW_CMD_REQ is write-only
     * - Reads return 0 or undefined value
     * - Writes accepted without error (up to FIFO depth)
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_req_wo();

    // =========================================================================
    // Test Case 11: SW_CMD_STS.CMD_REG_RDY Read-Only Verification
    // =========================================================================
    /**
     * @brief Verify SW_CMD_STS.CMD_REG_RDY bit indicates readiness
     *
     * Test Objective: Validate CMD_REG_RDY read-only field indicates readiness
     * to accept next command word before SW_CMD_REQ writes.
     *
     * Test Plan Reference: Test case #19
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read SW_CMD_STS register
     * 2. Extract CMD_REG_RDY bit [0]
     * 3. Verify bit indicates FIFO readiness state
     * 4. Attempt to write to SW_CMD_STS
     * 5. Read back SW_CMD_STS
     * 6. Verify written value was ignored (read-only)
     *
     * Pass Criteria:
     * - CMD_REG_RDY bit reflects FIFO ready state
     * - SW_CMD_STS is read-only
     * - Write attempts have no effect
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_sts_cmd_reg_rdy();

    // =========================================================================
    // Test Case 12: SW_CMD_STS.CMD_RDY Read-Only Verification
    // =========================================================================
    /**
     * @brief Verify SW_CMD_STS.CMD_RDY bit indicates EDN readiness
     *
     * Test Objective: Validate CMD_RDY read-only field indicates EDN readiness
     * for new multi-word command sequence.
     *
     * Test Plan Reference: Test case #20
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read SW_CMD_STS register
     * 2. Extract CMD_RDY bit [1]
     * 3. Verify bit indicates command readiness state
     * 4. Attempt to write to SW_CMD_STS
     * 5. Verify write was ignored
     *
     * Pass Criteria:
     * - CMD_RDY bit reflects EDN command ready state
     * - SW_CMD_STS remains read-only
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_sts_cmd_rdy();

    // =========================================================================
    // Test Case 13: RESEED_CMD Write-Only FIFO Verification
    // =========================================================================
    /**
     * @brief Verify RESEED_CMD register write-only FIFO interface
     *
     * Test Objective: Validate write-only access for RESEED_CMD FIFO
     * (up to 13 words depth).
     *
     * Test Plan Reference: Test case #27
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Attempt to read RESEED_CMD (should return 0)
     * 2. Write test command words to RESEED_CMD (up to 13 words)
     * 3. Verify writes accepted without error
     * 4. Attempt to read RESEED_CMD again
     * 5. Verify reads return 0
     *
     * Pass Criteria:
     * - RESEED_CMD is write-only
     * - Accepts up to 13 words without overflow error
     * - Reads return 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_reseed_cmd_fifo_wo();

    // =========================================================================
    // Test Case 14: GENERATE_CMD Write-Only FIFO Verification
    // =========================================================================
    /**
     * @brief Verify GENERATE_CMD register write-only FIFO interface
     *
     * Test Objective: Validate write-only access for GENERATE_CMD FIFO
     * (up to 13 words depth).
     *
     * Test Plan Reference: Test case #28
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Attempt to read GENERATE_CMD (should return 0)
     * 2. Write test command words to GENERATE_CMD (up to 13 words)
     * 3. Verify writes accepted without error
     * 4. Attempt to read GENERATE_CMD again
     * 5. Verify reads return 0
     *
     * Pass Criteria:
     * - GENERATE_CMD is write-only
     * - Accepts up to 13 words without overflow error
     * - Reads return 0
     *
     * @return true if test passes, false otherwise
     */
    bool test_generate_cmd_fifo_wo();

    // =========================================================================
    // Test Case 15: MAX_NUM_REQS_BETWEEN_RESEEDS Read/Write Verification
    // =========================================================================
    /**
     * @brief Verify MAX_NUM_REQS_BETWEEN_RESEEDS register read/write access
     *
     * Test Objective: Validate full 32-bit read/write access for auto mode
     * reseed interval counter configuration.
     *
     * Test Plan Reference: Test case #29
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read reset value (should be 0x0)
     * 2. Write test value (e.g., 0x100)
     * 3. Read back and verify
     * 4. Write maximum value (0xFFFFFFFF)
     * 5. Read back and verify
     * 6. Write 0x0
     * 7. Read back and verify
     *
     * Pass Criteria:
     * - Reset value is 0x0
     * - Full 32-bit read/write access functional
     * - Written values preserved
     *
     * @return true if test passes, false otherwise
     */
    bool test_max_num_reqs_between_reseeds_rw();

    // =========================================================================
    // Test Case 16: RECOV_ALERT_STS Write-0-to-Clear Verification
    // =========================================================================
    /**
     * @brief Verify RECOV_ALERT_STS register Write-0-to-Clear mechanism
     *
     * Test Objective: Validate W0C (or RW0C) semantics for all recoverable
     * alert status bits.
     *
     * Test Plan Reference: Test case #30
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Force alert status bits via multi-bit encoding errors
     * 2. Read RECOV_ALERT_STS to confirm bits are set
     * 3. Write 0 to specific alert bit
     * 4. Verify bit is cleared
     * 5. Write 1 to another alert bit
     * 6. Verify bit remains set (no effect)
     * 7. Write 0 to all alert bits
     * 8. Verify all bits cleared
     *
     * Pass Criteria:
     * - Writing 0 clears alert status bits
     * - Writing 1 has no effect
     * - Reserved bits unaffected
     *
     * @return true if test passes, false otherwise
     */
    bool test_recov_alert_sts_rw0c();

    // =========================================================================
    // Test Case 17: ERR_CODE Sticky Read-Only Verification
    // =========================================================================
    /**
     * @brief Verify ERR_CODE register sticky read-only behavior
     *
     * Test Objective: Validate ERR_CODE is read-only and sticky until system
     * reset for all fatal error bits.
     *
     * Test Plan Reference: Test case #31
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read ERR_CODE reset value (should be 0x0)
     * 2. Force error via ERR_CODE_TEST register
     * 3. Read ERR_CODE and verify error bit is set
     * 4. Attempt to write 0 to ERR_CODE
     * 5. Read ERR_CODE and verify bit remains set (sticky)
     * 6. Attempt to write 1 to ERR_CODE
     * 7. Verify no change (read-only)
     * 8. Apply system reset
     * 9. Read ERR_CODE and verify all bits cleared
     *
     * Pass Criteria:
     * - ERR_CODE is read-only
     * - Error bits are sticky (cannot be cleared by writes)
     * - Only system reset clears error bits
     *
     * @return true if test passes, false otherwise
     */
    bool test_err_code_sticky_ro();

    // =========================================================================
    // Test Case 18: ERR_CODE_TEST Error Injection Verification
    // =========================================================================
    /**
     * @brief Verify ERR_CODE_TEST register forces ERR_CODE bits
     *
     * Test Objective: Validate write-only ERR_CODE_TEST forces specific
     * ERR_CODE bits for testing error handling paths.
     *
     * Test Plan Reference: Test case #32
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Clear any existing errors via reset
     * 2. Write bit position 0 to ERR_CODE_TEST
     * 3. Read ERR_CODE and verify bit 0 is set
     * 4. Write bit position 1 to ERR_CODE_TEST
     * 5. Read ERR_CODE and verify bit 1 is set
     * 6. Verify INTR_STATE.edn_fatal_err is set
     * 7. Verify alert_fatal_alert signal is asserted
     *
     * Pass Criteria:
     * - ERR_CODE_TEST forces corresponding ERR_CODE bits
     * - Fatal error interrupt generated
     * - Fatal alert asserted
     *
     * @return true if test passes, false otherwise
     */
    bool test_err_code_test_injection();

    // =========================================================================
    // Test Case 19: MAIN_SM_STATE Visibility Verification
    // =========================================================================
    /**
     * @brief Verify MAIN_SM_STATE register exposes state machine state
     *
     * Test Objective: Validate read-only access to MAIN_SM_STATE register
     * for debug visibility.
     *
     * Test Plan Reference: Test case #33
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Read MAIN_SM_STATE reset value (should be 0xC1 = Idle)
     * 2. Attempt to write different value to MAIN_SM_STATE
     * 3. Read back and verify value unchanged (read-only)
     * 4. Enable EDN in different modes and observe state changes
     * 5. Verify MAIN_SM_STATE reflects actual state machine state
     *
     * Pass Criteria:
     * - MAIN_SM_STATE reset value is 0xC1 (Idle)
     * - Register is read-only
     * - State reflects actual EDN_MAIN_SM state
     *
     * @return true if test passes, false otherwise
     */
    bool test_main_sm_state_visibility();

    // =========================================================================
    // Test Case 20: Reserved Bits Read Zero Verification
    // =========================================================================
    /**
     * @brief Verify reserved bits read as 0 and writes are ignored
     *
     * Test Objective: Validate reserved bit handling across all registers
     * (read as 0, writes ignored).
     *
     * Test Plan Reference: Test case #34
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. For each register with reserved bits:
     *    a. Read register value
     *    b. Mask out valid bit fields
     *    c. Verify reserved bits read as 0
     *    d. Write all 1's to register
     *    e. Read back and verify reserved bits still 0
     * 2. Test applies to all 17 registers
     *
     * Pass Criteria:
     * - All reserved bits read as 0
     * - Writes to reserved bits have no effect
     * - Valid bit fields operate normally
     *
     * @return true if test passes, false otherwise
     */
    bool test_reserved_bits_read_zero();

    // =========================================================================
    // Test Case 21: Corner Case - CTRL Write When REGWEN Locked
    // =========================================================================
    /**
     * @brief Corner case: Verify CTRL write attempt when REGWEN locked
     *
     * Test Objective: Validate corner case where firmware attempts CTRL write
     * when REGWEN=0, ensuring write protection is robust.
     *
     * Test Plan Reference: Test case from corner cases section
     * Functionality: EDN_FUNC_001
     *
     * Procedure:
     * 1. Configure CTRL with specific value
     * 2. Lock CTRL by clearing REGWEN
     * 3. Attempt writes to all CTRL fields:
     *    - EDN_ENABLE
     *    - BOOT_REQ_MODE
     *    - AUTO_REQ_MODE
     *    - CMD_FIFO_RST
     * 4. Read CTRL after each attempt
     * 5. Verify configuration remains unchanged
     *
     * Pass Criteria:
     * - No CTRL field can be modified when REGWEN=0
     * - No alerts or errors generated
     * - Configuration preserved exactly
     *
     * @return true if test passes, false otherwise
     */
    bool test_corner_ctrl_write_when_regwen_locked();

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
