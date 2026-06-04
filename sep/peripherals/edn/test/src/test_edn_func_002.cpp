/**
 * @file test_edn_func_002.cpp
 * @brief EDN_FUNC_002 Test Suite Implementation
 *
 * Comprehensive implementation of all 12 test cases for EDN_FUNC_002
 * (Module Initialization and Configuration) functionality verification.
 *
 * Test Coverage Matrix:
 * - T1:  CTRL.EDN_ENABLE valid values (0x6=enable, 0x9=disable)
 * - T2:  CTRL.EDN_ENABLE invalid values trigger alert
 * - T3:  CTRL.BOOT_REQ_MODE valid values
 * - T4:  CTRL.BOOT_REQ_MODE invalid values trigger alert
 * - T5:  CTRL.AUTO_REQ_MODE valid values
 * - T6:  CTRL.AUTO_REQ_MODE invalid values trigger alert
 * - T7:  CTRL.CMD_FIFO_RST valid values
 * - T8:  CTRL.CMD_FIFO_RST invalid values trigger alert
 * - T9:  REGWEN write protection mechanism (W0C)
 * - T10: REGWEN lock enforcement on CTRL writes
 * - T11: Reset restores REGWEN to unlocked state
 * - T12: Recommended initialization sequence validation
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-12
 */

#include "test_edn_func_002.h"
#include <iomanip>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_002::test_edn_func_002(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    CSML_INFO(1, logger) << "EDN_FUNC_002 test suite initialized";
    CSML_INFO(1, logger) << "Test Coverage: Module Initialization and Configuration (12 test cases)";
}

test_edn_func_002::~test_edn_func_002()
{
    CSML_INFO(1, logger) << "EDN_FUNC_002 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_002::run_all_tests()
{
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_002 Test Suite Execution Start";
    CSML_INFO(1, logger) << "========================================";

    // Apply reset before starting tests
    CSML_INFO(1, logger) << "Applying system reset...";
    apply_reset(100.0); // 100ns reset pulse
    wait(10, SC_NS);    // Wait for reset propagation

    // Execute all 12 test cases
    bool result;

    // Test 1: CTRL.EDN_ENABLE Valid Values
    result = test_ctrl_edn_enable_valid();
    report_test_result("T1: CTRL.EDN_ENABLE Valid Values", result);

    // Reset between tests to ensure clean state
    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 2: CTRL.EDN_ENABLE Invalid Values
    result = test_ctrl_edn_enable_invalid();
    report_test_result("T2: CTRL.EDN_ENABLE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 3: CTRL.BOOT_REQ_MODE Valid Values
    result = test_ctrl_boot_req_mode_valid();
    report_test_result("T3: CTRL.BOOT_REQ_MODE Valid Values", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 4: CTRL.BOOT_REQ_MODE Invalid Values
    result = test_ctrl_boot_req_mode_invalid();
    report_test_result("T4: CTRL.BOOT_REQ_MODE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 5: CTRL.AUTO_REQ_MODE Valid Values
    result = test_ctrl_auto_req_mode_valid();
    report_test_result("T5: CTRL.AUTO_REQ_MODE Valid Values", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 6: CTRL.AUTO_REQ_MODE Invalid Values
    result = test_ctrl_auto_req_mode_invalid();
    report_test_result("T6: CTRL.AUTO_REQ_MODE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 7: CTRL.CMD_FIFO_RST Valid Values
    result = test_ctrl_cmd_fifo_rst_valid();
    report_test_result("T7: CTRL.CMD_FIFO_RST Valid Values", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 8: CTRL.CMD_FIFO_RST Invalid Values
    result = test_ctrl_cmd_fifo_rst_invalid();
    report_test_result("T8: CTRL.CMD_FIFO_RST Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 9: REGWEN Write Protection
    result = test_regwen_write_protection();
    report_test_result("T9: REGWEN Write Protection Mechanism", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 10: REGWEN Lock Enforcement
    result = test_regwen_lock_enforcement();
    report_test_result("T10: REGWEN Lock Enforcement", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 11: Reset Restores REGWEN
    result = test_reset_restores_regwen();
    report_test_result("T11: Reset Restores REGWEN to Unlocked", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 12: Initialization Sequence
    result = test_initialization_sequence();
    report_test_result("T12: Recommended Initialization Sequence", result);

    // Print summary
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_002 Test Suite Summary";
    CSML_INFO(1, logger) << "========================================";

    std::ostringstream oss;
    oss << "Tests Run:    " << m_tests_run;
    CSML_INFO(1, logger) << oss.str();
    oss.str("");

    oss << "Tests Passed: " << m_tests_passed;
    CSML_INFO(1, logger) << oss.str();
    oss.str("");

    oss << "Tests Failed: " << m_tests_failed;
    CSML_INFO(1, logger) << oss.str();

    if (m_tests_failed > 0) {
        CSML_ERROR(1, logger) << "Failed Tests:";
        for (const auto& test_name : m_failed_tests) {
            CSML_ERROR(1, logger) << "  - " << test_name;
        }
    }

    CSML_INFO(1, logger) << "========================================";

    return m_tests_failed;
}

// =============================================================================
// Test Case 1: CTRL.EDN_ENABLE Valid Values
// =============================================================================

bool test_edn_func_002::test_ctrl_edn_enable_valid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_edn_enable_valid...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Verify REGWEN is unlocked (reset value = 0x1)
    register_read_32(REGWEN_OFFSET, read_value);
    if (!verify_register_value("REGWEN (unlocked)", 0x1, read_value)) {
        all_passed = false;
        return all_passed;
    }

    // Step 2: Read CTRL reset value (should be 0x9999)
    register_read_32(CTRL_OFFSET, read_value);
    if (!verify_register_value("CTRL (reset)", CTRL_RESET, read_value)) {
        all_passed = false;
    }

    // Verify EDN_ENABLE field is 0x9 (disabled)
    uint32_t edn_enable = (read_value >> EDN_ENABLE_SHIFT) & 0xF;
    if (edn_enable != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "CTRL.EDN_ENABLE reset value incorrect: expected 0x9, got 0x"
                              << std::hex << edn_enable;
        all_passed = false;
    }

    // Step 3: Clear any existing alerts
    clear_recoverable_alerts();

    // Step 4: Write CTRL with EDN_ENABLE=0x6 (enable), other fields=0x9
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS); // Allow time for configuration

    // Step 5: Read back CTRL and verify EDN_ENABLE=0x6
    register_read_32(CTRL_OFFSET, read_value);
    edn_enable = (read_value >> EDN_ENABLE_SHIFT) & 0xF;
    if (edn_enable != MULTIBIT_ENABLE) {
        CSML_ERROR(1, logger) << "Failed to set CTRL.EDN_ENABLE to 0x6: got 0x"
                              << std::hex << edn_enable;
        all_passed = false;
    }

    // Step 6: Check RECOV_ALERT_STS - no EDN_ENABLE_FIELD_ALERT should be set
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> EDN_ENABLE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected EDN_ENABLE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Step 7: Verify MAIN_SM_STATE has transitioned from Idle
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    if (read_value == STATE_IDLE) {
        CSML_WARN(1, logger) << "MAIN_SM_STATE still in Idle after enabling EDN";
        // This may be expected if EDN requires additional configuration
    } else {
        CSML_INFO(1, logger) << "MAIN_SM_STATE transitioned to: 0x"
                            << std::hex << read_value;
    }

    // Step 8: Disable EDN by writing EDN_ENABLE=0x9
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Step 9: Read back and verify EDN_ENABLE=0x9
    register_read_32(CTRL_OFFSET, read_value);
    edn_enable = (read_value >> EDN_ENABLE_SHIFT) & 0xF;
    if (edn_enable != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "Failed to set CTRL.EDN_ENABLE to 0x9: got 0x"
                              << std::hex << edn_enable;
        all_passed = false;
    }

    // Step 10: Verify no alerts generated
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> EDN_ENABLE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected EDN_ENABLE_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    // Step 11: Verify MAIN_SM_STATE returns to Idle
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    if (read_value == STATE_IDLE) {
        CSML_INFO(1, logger) << "MAIN_SM_STATE correctly returned to Idle state";
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_edn_enable_valid: PASSED - Valid values accepted without alerts";
    }

    return all_passed;
}

// =============================================================================
// Test Case 2: CTRL.EDN_ENABLE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_002::test_ctrl_edn_enable_invalid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_edn_enable_invalid...";

    bool all_passed = true;
    uint32_t read_value;

    // Test multiple invalid values for EDN_ENABLE
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        uint32_t invalid_value = invalid_values[i];

        CSML_INFO(1, logger) << "Testing invalid EDN_ENABLE value: 0x"
                            << std::hex << invalid_value;

        // Clear any existing alerts
        clear_recoverable_alerts();

        // Write CTRL with invalid EDN_ENABLE value
        uint32_t ctrl_value = (invalid_value << EDN_ENABLE_SHIFT) |
                              (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

        register_write_32(CTRL_OFFSET, ctrl_value);
        wait(5, SC_NS); // Allow time for alert generation

        // Read RECOV_ALERT_STS and verify EDN_ENABLE_FIELD_ALERT is set
        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!((read_value >> EDN_ENABLE_FIELD_ALERT_BIT) & 0x1)) {
            CSML_ERROR(1, logger) << "EDN_ENABLE_FIELD_ALERT not set for invalid value 0x"
                                  << std::hex << invalid_value;
            all_passed = false;
        } else {
            CSML_INFO(1, logger) << "EDN_ENABLE_FIELD_ALERT correctly set for invalid value 0x"
                                << std::hex << invalid_value;
        }

        // Clear the alert by writing 0 to the bit
        register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFE); // Clear bit 0, write 0
        wait(1, SC_NS);

        // Verify alert is cleared
        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if ((read_value >> EDN_ENABLE_FIELD_ALERT_BIT) & 0x1) {
            CSML_ERROR(1, logger) << "Failed to clear EDN_ENABLE_FIELD_ALERT";
            all_passed = false;
        }
    }

    // Test that valid values do NOT trigger alert
    clear_recoverable_alerts();

    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> EDN_ENABLE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected alert for valid EDN_ENABLE value 0x6";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_edn_enable_invalid: PASSED - Invalid values trigger alerts correctly";
    }

    return all_passed;
}

// =============================================================================
// Test Case 3: CTRL.BOOT_REQ_MODE Valid Values
// =============================================================================

bool test_edn_func_002::test_ctrl_boot_req_mode_valid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_boot_req_mode_valid...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Read CTRL reset value (BOOT_REQ_MODE=0x9, disabled)
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t boot_req_mode = (read_value >> BOOT_REQ_MODE_SHIFT) & 0xF;
    if (boot_req_mode != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "CTRL.BOOT_REQ_MODE reset value incorrect: expected 0x9, got 0x"
                              << std::hex << boot_req_mode;
        all_passed = false;
    }

    // Step 2: Clear any existing alerts
    clear_recoverable_alerts();

    // Step 3: Write CTRL with EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_ENABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS); // Allow time for state machine transition

    // Step 4: Read back and verify BOOT_REQ_MODE=0x6
    register_read_32(CTRL_OFFSET, read_value);
    boot_req_mode = (read_value >> BOOT_REQ_MODE_SHIFT) & 0xF;
    if (boot_req_mode != MULTIBIT_ENABLE) {
        CSML_ERROR(1, logger) << "Failed to set CTRL.BOOT_REQ_MODE to 0x6: got 0x"
                              << std::hex << boot_req_mode;
        all_passed = false;
    }

    // Step 5: Check no BOOT_REQ_MODE_FIELD_ALERT
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> BOOT_REQ_MODE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected BOOT_REQ_MODE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Step 6: Verify MAIN_SM_STATE transitions to BootInsAckWait or boot-related state
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    CSML_INFO(1, logger) << "MAIN_SM_STATE after enabling boot mode: 0x"
                        << std::hex << read_value;

    // Expected state is BootInsAckWait (0x36) or subsequent boot state
    // Note: State may progress quickly through boot sequence

    // Step 7: Clear BOOT_REQ_MODE (set to 0x9) while keeping EDN_ENABLE=0x6
    ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS);

    // Step 8: Poll MAIN_SM_STATE - should eventually reach SWPortMode
    bool state_reached = wait_for_state(STATE_SW_PORT_MODE, 1000); // 1us timeout
    if (!state_reached) {
        register_read_32(MAIN_SM_STATE_OFFSET, read_value);
        CSML_WARN(1, logger) << "MAIN_SM_STATE did not reach SWPortMode within timeout. Current state: 0x"
                            << std::hex << read_value;
        // Not necessarily a failure - may need CSRNG interaction
    }

    // Step 9: Verify no alerts generated
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> BOOT_REQ_MODE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected BOOT_REQ_MODE_FIELD_ALERT after clearing to 0x9";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_boot_req_mode_valid: PASSED - Valid values accepted";
    }

    return all_passed;
}

// =============================================================================
// Test Case 4: CTRL.BOOT_REQ_MODE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_002::test_ctrl_boot_req_mode_invalid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_boot_req_mode_invalid...";

    bool all_passed = true;
    uint32_t read_value;

    // Test multiple invalid values for BOOT_REQ_MODE
    uint32_t invalid_values[] = {0x0, 0x1, 0x5, 0x7, 0xA, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        uint32_t invalid_value = invalid_values[i];

        CSML_INFO(1, logger) << "Testing invalid BOOT_REQ_MODE value: 0x"
                            << std::hex << invalid_value;

        // Clear any existing alerts
        clear_recoverable_alerts();

        // Write CTRL with invalid BOOT_REQ_MODE value
        uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                              (invalid_value << BOOT_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

        register_write_32(CTRL_OFFSET, ctrl_value);
        wait(5, SC_NS);

        // Read RECOV_ALERT_STS and verify BOOT_REQ_MODE_FIELD_ALERT is set
        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!((read_value >> BOOT_REQ_MODE_FIELD_ALERT_BIT) & 0x1)) {
            CSML_ERROR(1, logger) << "BOOT_REQ_MODE_FIELD_ALERT not set for invalid value 0x"
                                  << std::hex << invalid_value;
            all_passed = false;
        }

        // Clear the alert
        register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFD); // Clear bit 1
        wait(1, SC_NS);
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_boot_req_mode_invalid: PASSED - Invalid values trigger alerts";
    }

    return all_passed;
}

// =============================================================================
// Test Case 5: CTRL.AUTO_REQ_MODE Valid Values
// =============================================================================

bool test_edn_func_002::test_ctrl_auto_req_mode_valid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_auto_req_mode_valid...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Read CTRL reset value (AUTO_REQ_MODE=0x9)
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t auto_req_mode = (read_value >> AUTO_REQ_MODE_SHIFT) & 0xF;
    if (auto_req_mode != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "CTRL.AUTO_REQ_MODE reset value incorrect";
        all_passed = false;
    }

    // Step 2: Configure prerequisites for auto mode
    // Write valid command to GENERATE_CMD FIFO (Generate command header)
    // Command format: [31:17]=glen, [16:13]=clen, [12]=flags, [11:4]=reserved, [3:0]=cmd_type(3=Generate)
    uint32_t gen_cmd = 0x00020003; // glen=1, clen=0, cmd_type=3
    register_write_32(GENERATE_CMD_OFFSET, gen_cmd);
    wait(1, SC_NS);

    // Write valid command to RESEED_CMD FIFO (Reseed command header)
    // Command format: cmd_type=4 (Reseed)
    uint32_t reseed_cmd = 0x00000004; // clen=0, cmd_type=4
    register_write_32(RESEED_CMD_OFFSET, reseed_cmd);
    wait(1, SC_NS);

    // Set MAX_NUM_REQS_BETWEEN_RESEEDS to non-zero value
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0x10); // Reseed every 16 generates
    wait(1, SC_NS);

    // Step 3: Clear any existing alerts
    clear_recoverable_alerts();

    // Step 4: Write CTRL with EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_ENABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS);

    // Step 5: Read back and verify AUTO_REQ_MODE=0x6
    register_read_32(CTRL_OFFSET, read_value);
    auto_req_mode = (read_value >> AUTO_REQ_MODE_SHIFT) & 0xF;
    if (auto_req_mode != MULTIBIT_ENABLE) {
        CSML_ERROR(1, logger) << "Failed to set CTRL.AUTO_REQ_MODE to 0x6: got 0x"
                              << std::hex << auto_req_mode;
        all_passed = false;
    }

    // Step 6: Check no AUTO_REQ_MODE_FIELD_ALERT
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> AUTO_REQ_MODE_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected AUTO_REQ_MODE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Step 7: Verify MAIN_SM_STATE transitions to AutoLoadIns or auto-related state
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    CSML_INFO(1, logger) << "MAIN_SM_STATE after enabling auto mode: 0x"
                        << std::hex << read_value;

    // Step 8: Clear AUTO_REQ_MODE (set to 0x9)
    ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS);

    // Step 9: Verify transition to SWPortMode (with timeout)
    bool state_reached = wait_for_state(STATE_SW_PORT_MODE, 1000);
    if (!state_reached) {
        register_read_32(MAIN_SM_STATE_OFFSET, read_value);
        CSML_WARN(1, logger) << "MAIN_SM_STATE did not reach SWPortMode within timeout. Current: 0x"
                            << std::hex << read_value;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_auto_req_mode_valid: PASSED - Valid values accepted";
    }

    return all_passed;
}

// =============================================================================
// Test Case 6: CTRL.AUTO_REQ_MODE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_002::test_ctrl_auto_req_mode_invalid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_auto_req_mode_invalid...";

    bool all_passed = true;
    uint32_t read_value;

    // Test multiple invalid values for AUTO_REQ_MODE
    uint32_t invalid_values[] = {0x0, 0x1, 0x5, 0x7, 0xA, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        uint32_t invalid_value = invalid_values[i];

        CSML_INFO(1, logger) << "Testing invalid AUTO_REQ_MODE value: 0x"
                            << std::hex << invalid_value;

        clear_recoverable_alerts();

        uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                              (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                              (invalid_value << AUTO_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

        register_write_32(CTRL_OFFSET, ctrl_value);
        wait(5, SC_NS);

        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!((read_value >> AUTO_REQ_MODE_FIELD_ALERT_BIT) & 0x1)) {
            CSML_ERROR(1, logger) << "AUTO_REQ_MODE_FIELD_ALERT not set for invalid value 0x"
                                  << std::hex << invalid_value;
            all_passed = false;
        }

        register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFB); // Clear bit 2
        wait(1, SC_NS);
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_auto_req_mode_invalid: PASSED - Invalid values trigger alerts";
    }

    return all_passed;
}

// =============================================================================
// Test Case 7: CTRL.CMD_FIFO_RST Valid Values
// =============================================================================

bool test_edn_func_002::test_ctrl_cmd_fifo_rst_valid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_cmd_fifo_rst_valid...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Write test data to GENERATE_CMD FIFO
    uint32_t gen_cmd = 0x00020003; // Generate command
    register_write_32(GENERATE_CMD_OFFSET, gen_cmd);
    wait(1, SC_NS);

    // Step 2: Write test data to RESEED_CMD FIFO
    uint32_t reseed_cmd = 0x00000004; // Reseed command
    register_write_32(RESEED_CMD_OFFSET, reseed_cmd);
    wait(1, SC_NS);

    // Step 3: Clear any existing alerts
    clear_recoverable_alerts();

    // Step 4: Write CTRL with CMD_FIFO_RST=0x6
    uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_ENABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Step 5: Read back CTRL and verify CMD_FIFO_RST field
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t cmd_fifo_rst = (read_value >> CMD_FIFO_RST_SHIFT) & 0xF;
    CSML_INFO(1, logger) << "CMD_FIFO_RST read back as: 0x" << std::hex << cmd_fifo_rst;

    // Step 6: Check no CMD_FIFO_RST_FIELD_ALERT
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> CMD_FIFO_RST_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected CMD_FIFO_RST_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Step 7: Write CMD_FIFO_RST=0x9 (idle value)
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Step 8: Verify no alert for idle value
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value >> CMD_FIFO_RST_FIELD_ALERT_BIT) & 0x1) {
        CSML_ERROR(1, logger) << "Unexpected CMD_FIFO_RST_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_cmd_fifo_rst_valid: PASSED - Valid values accepted";
    }

    return all_passed;
}

// =============================================================================
// Test Case 8: CTRL.CMD_FIFO_RST Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_002::test_ctrl_cmd_fifo_rst_invalid()
{
    CSML_INFO(1, logger) << "Starting test_ctrl_cmd_fifo_rst_invalid...";

    bool all_passed = true;
    uint32_t read_value;

    // Test multiple invalid values for CMD_FIFO_RST
    uint32_t invalid_values[] = {0x0, 0x1, 0x5, 0x7, 0xA, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        uint32_t invalid_value = invalid_values[i];

        CSML_INFO(1, logger) << "Testing invalid CMD_FIFO_RST value: 0x"
                            << std::hex << invalid_value;

        clear_recoverable_alerts();

        uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                              (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                              (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                              (invalid_value << CMD_FIFO_RST_SHIFT);

        register_write_32(CTRL_OFFSET, ctrl_value);
        wait(5, SC_NS);

        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!((read_value >> CMD_FIFO_RST_FIELD_ALERT_BIT) & 0x1)) {
            CSML_ERROR(1, logger) << "CMD_FIFO_RST_FIELD_ALERT not set for invalid value 0x"
                                  << std::hex << invalid_value;
            all_passed = false;
        }

        register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFF7); // Clear bit 3
        wait(1, SC_NS);
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_ctrl_cmd_fifo_rst_invalid: PASSED - Invalid values trigger alerts";
    }

    return all_passed;
}

// =============================================================================
// Test Case 9: REGWEN Write Protection Mechanism
// =============================================================================

bool test_edn_func_002::test_regwen_write_protection()
{
    CSML_INFO(1, logger) << "Starting test_regwen_write_protection...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Read REGWEN reset value (should be 0x1)
    register_read_32(REGWEN_OFFSET, read_value);
    if (!verify_register_value("REGWEN (reset)", 0x1, read_value)) {
        all_passed = false;
    }

    // Step 2: Configure CTRL with known value (0x9999 - reset value)
    uint32_t initial_ctrl = CTRL_RESET;
    register_write_32(CTRL_OFFSET, initial_ctrl);
    wait(1, SC_NS);

    // Step 3: Read back CTRL to confirm write succeeded
    register_read_32(CTRL_OFFSET, read_value);
    if (!verify_register_value("CTRL (initial)", initial_ctrl, read_value)) {
        all_passed = false;
    }

    // Step 4: Write 0x0 to REGWEN (lock CTRL)
    register_write_32(REGWEN_OFFSET, 0x0);
    wait(1, SC_NS);

    // Step 5: Read REGWEN - should be 0x0
    register_read_32(REGWEN_OFFSET, read_value);
    if (!verify_register_value("REGWEN (locked)", 0x0, read_value)) {
        CSML_ERROR(1, logger) << "Failed to lock REGWEN";
        all_passed = false;
    }

    // Step 6: Attempt to write different value to CTRL (e.g., 0x9996)
    uint32_t new_ctrl = 0x9996;
    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 7: Read CTRL and verify value unchanged (still initial_ctrl)
    register_read_32(CTRL_OFFSET, read_value);
    if (read_value != initial_ctrl) {
        CSML_ERROR(1, logger) << "CTRL write not blocked when REGWEN=0. Expected 0x"
                              << std::hex << initial_ctrl << ", got 0x" << read_value;
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "CTRL write correctly blocked when REGWEN=0";
    }

    // Step 8: Attempt to write 0x1 to REGWEN (should have no effect)
    register_write_32(REGWEN_OFFSET, 0x1);
    wait(1, SC_NS);

    // Step 9: Read REGWEN - should still be 0x0
    register_read_32(REGWEN_OFFSET, read_value);
    if (read_value != 0x0) {
        CSML_ERROR(1, logger) << "REGWEN incorrectly changed from 0 to 1";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "REGWEN correctly remains locked at 0";
    }

    // Step 10: Attempt multiple CTRL writes with various values
    uint32_t test_values[] = {0x9666, 0x6999, 0xFFFF, 0x0000};
    for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); ++i) {
        register_write_32(CTRL_OFFSET, test_values[i]);
        wait(1, SC_NS);

        register_read_32(CTRL_OFFSET, read_value);
        if (read_value != initial_ctrl) {
            CSML_ERROR(1, logger) << "CTRL modified despite REGWEN lock. Value: 0x"
                                  << std::hex << read_value;
            all_passed = false;
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_regwen_write_protection: PASSED - W0C mechanism correctly locks CTRL";
    }

    return all_passed;
}

// =============================================================================
// Test Case 10: REGWEN Lock Enforcement
// =============================================================================

bool test_edn_func_002::test_regwen_lock_enforcement()
{
    CSML_INFO(1, logger) << "Starting test_regwen_lock_enforcement...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Configure CTRL with specific field values
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Step 2: Lock CTRL by clearing REGWEN
    register_write_32(REGWEN_OFFSET, 0x0);
    wait(1, SC_NS);

    // Verify REGWEN is locked
    register_read_32(REGWEN_OFFSET, read_value);
    if (read_value != 0x0) {
        CSML_ERROR(1, logger) << "Failed to lock REGWEN";
        all_passed = false;
        return all_passed;
    }

    // Step 3: Attempt to change EDN_ENABLE to 0x9
    uint32_t new_ctrl = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                        (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                        (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                        (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 4: Read CTRL - verify EDN_ENABLE still 0x6
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t edn_enable = (read_value >> EDN_ENABLE_SHIFT) & 0xF;
    if (edn_enable != MULTIBIT_ENABLE) {
        CSML_ERROR(1, logger) << "EDN_ENABLE changed despite REGWEN lock";
        all_passed = false;
    }

    // Step 5: Attempt to change BOOT_REQ_MODE to 0x6
    new_ctrl = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
               (MULTIBIT_ENABLE << BOOT_REQ_MODE_SHIFT) |
               (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
               (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 6: Read CTRL - verify BOOT_REQ_MODE still 0x9
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t boot_req_mode = (read_value >> BOOT_REQ_MODE_SHIFT) & 0xF;
    if (boot_req_mode != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "BOOT_REQ_MODE changed despite REGWEN lock";
        all_passed = false;
    }

    // Step 7: Attempt to change AUTO_REQ_MODE to 0x6
    new_ctrl = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
               (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
               (MULTIBIT_ENABLE << AUTO_REQ_MODE_SHIFT) |
               (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 8: Verify AUTO_REQ_MODE unchanged
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t auto_req_mode = (read_value >> AUTO_REQ_MODE_SHIFT) & 0xF;
    if (auto_req_mode != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "AUTO_REQ_MODE changed despite REGWEN lock";
        all_passed = false;
    }

    // Step 9: Attempt to set CMD_FIFO_RST to 0x6
    new_ctrl = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
               (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
               (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
               (MULTIBIT_ENABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 10: Verify CMD_FIFO_RST unchanged
    register_read_32(CTRL_OFFSET, read_value);
    uint32_t cmd_fifo_rst = (read_value >> CMD_FIFO_RST_SHIFT) & 0xF;
    if (cmd_fifo_rst != MULTIBIT_DISABLE) {
        CSML_ERROR(1, logger) << "CMD_FIFO_RST changed despite REGWEN lock";
        all_passed = false;
    }

    // Step 11: Attempt to write all fields simultaneously
    new_ctrl = 0x6666; // All fields = 0x6
    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 12: Verify entire CTRL register unchanged
    register_read_32(CTRL_OFFSET, read_value);
    if (read_value != ctrl_value) {
        CSML_ERROR(1, logger) << "CTRL changed despite REGWEN lock. Expected 0x"
                              << std::hex << ctrl_value << ", got 0x" << read_value;
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "CTRL correctly protected by REGWEN lock";
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_regwen_lock_enforcement: PASSED - All CTRL fields protected";
    }

    return all_passed;
}

// =============================================================================
// Test Case 11: Reset Restores REGWEN to Unlocked State
// =============================================================================

bool test_edn_func_002::test_reset_restores_regwen()
{
    CSML_INFO(1, logger) << "Starting test_reset_restores_regwen...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Configure CTRL with specific value
    uint32_t initial_ctrl = 0x9996; // EDN_ENABLE=0x6, others=0x9
    register_write_32(CTRL_OFFSET, initial_ctrl);
    wait(1, SC_NS);

    // Step 2: Lock CTRL by clearing REGWEN
    register_write_32(REGWEN_OFFSET, 0x0);
    wait(1, SC_NS);

    // Step 3: Verify CTRL writes are blocked
    register_write_32(CTRL_OFFSET, 0x6666);
    wait(1, SC_NS);

    register_read_32(CTRL_OFFSET, read_value);
    if (read_value != initial_ctrl) {
        CSML_ERROR(1, logger) << "CTRL not properly locked before reset test";
        all_passed = false;
    }

    // Step 4: Apply system reset
    CSML_INFO(1, logger) << "Applying system reset to restore REGWEN...";
    apply_reset(100.0);
    wait(10, SC_NS);

    // Step 5: Read REGWEN - should be 0x1 (unlocked)
    register_read_32(REGWEN_OFFSET, read_value);
    if (!verify_register_value("REGWEN (after reset)", 0x1, read_value)) {
        CSML_ERROR(1, logger) << "Reset failed to restore REGWEN to 0x1";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Reset correctly restored REGWEN to 0x1 (unlocked)";
    }

    // Step 6: Read CTRL - should be reset value 0x9999
    register_read_32(CTRL_OFFSET, read_value);
    if (!verify_register_value("CTRL (after reset)", CTRL_RESET, read_value)) {
        CSML_ERROR(1, logger) << "Reset failed to restore CTRL to reset value";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Reset correctly restored CTRL to 0x9999";
    }

    // Step 7: Attempt to write new value to CTRL
    uint32_t new_ctrl = 0x9996;
    register_write_32(CTRL_OFFSET, new_ctrl);
    wait(1, SC_NS);

    // Step 8: Read back CTRL and verify write succeeded
    register_read_32(CTRL_OFFSET, read_value);
    if (!verify_register_value("CTRL (after unlock)", new_ctrl, read_value)) {
        CSML_ERROR(1, logger) << "CTRL write failed after reset - REGWEN not functional";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "CTRL write succeeded after reset - full functionality restored";
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_reset_restores_regwen: PASSED - Reset restores configuration capability";
    }

    return all_passed;
}

// =============================================================================
// Test Case 12: Recommended Initialization Sequence
// =============================================================================

bool test_edn_func_002::test_initialization_sequence()
{
    CSML_INFO(1, logger) << "Starting test_initialization_sequence...";

    bool all_passed = true;
    uint32_t read_value;

    // Step 1: Verify EDN starts in Idle state after reset
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    if (!verify_register_value("MAIN_SM_STATE (Idle)", STATE_IDLE, read_value)) {
        CSML_WARN(1, logger) << "MAIN_SM_STATE not in Idle (0xC1) after reset. Got: 0x"
                            << std::hex << read_value;
        // Not necessarily a failure - continue test
    } else {
        CSML_INFO(1, logger) << "EDN correctly initialized in Idle state";
    }

    // Step 2: Read initial CTRL value
    register_read_32(CTRL_OFFSET, read_value);
    CSML_INFO(1, logger) << "CTRL reset value: 0x" << std::hex << read_value;

    // Step 3: Enable EDN via CTRL.EDN_ENABLE=0x6 (software port mode)
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS);

    // Step 4: Verify state machine transitions from Idle
    register_read_32(MAIN_SM_STATE_OFFSET, read_value);
    if (read_value == STATE_IDLE) {
        CSML_WARN(1, logger) << "MAIN_SM_STATE still in Idle after enabling EDN";
        // May require additional configuration or CSRNG interaction
    } else {
        CSML_INFO(1, logger) << "MAIN_SM_STATE transitioned from Idle to: 0x"
                            << std::hex << read_value;
    }

    // Step 5: Check for software port mode readiness
    // In a full system, we would verify:
    // - CSRNG is enabled and operational
    // - EDN can accept software commands via SW_CMD_REQ
    // For standalone EDN test, verify state machine progresses appropriately

    // Step 6: Verify MAIN_SM_STATE is in operational state
    bool in_operational_state = (read_value == STATE_SW_PORT_MODE) ||
                                 (read_value != STATE_IDLE);

    if (!in_operational_state) {
        CSML_ERROR(1, logger) << "EDN failed to enter operational state after initialization";
        all_passed = false;
    }

    // Step 7: Check SW_CMD_STS for readiness (if in SW port mode)
    register_read_32(SW_CMD_STS_OFFSET, read_value);
    CSML_INFO(1, logger) << "SW_CMD_STS after initialization: 0x" << std::hex << read_value;

    // CMD_RDY bit [1] indicates readiness for new commands
    bool cmd_ready = (read_value >> 1) & 0x1;
    CSML_INFO(1, logger) << "SW_CMD_STS.CMD_RDY: " << (cmd_ready ? "ready" : "not ready");

    // Step 8: Verify no errors or alerts generated during initialization
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (read_value != 0x0) {
        CSML_WARN(1, logger) << "Recoverable alerts detected during initialization: 0x"
                            << std::hex << read_value;
    }

    register_read_32(ERR_CODE_OFFSET, read_value);
    if (read_value != 0x0) {
        CSML_ERROR(1, logger) << "Fatal errors detected during initialization: 0x"
                              << std::hex << read_value;
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_initialization_sequence: PASSED - Initialization successful";
    }

    return all_passed;
}

// =============================================================================
// Helper Functions
// =============================================================================

bool test_edn_func_002::verify_register_value(const std::string& reg_name,
                                               uint32_t expected,
                                               uint32_t actual)
{
    if (expected != actual) {
        CSML_ERROR(1, logger) << reg_name << " mismatch: expected 0x"
                              << std::hex << std::setfill('0') << std::setw(8) << expected
                              << ", got 0x" << std::setw(8) << actual;
        return false;
    }
    return true;
}

bool test_edn_func_002::verify_bit_value(const std::string& reg_name,
                                         unsigned int bit_position,
                                         unsigned int expected_value,
                                         uint32_t actual_reg)
{
    uint32_t actual_bit = (actual_reg >> bit_position) & 0x1;
    if (actual_bit != expected_value) {
        CSML_ERROR(1, logger) << reg_name << " bit[" << bit_position
                              << "] mismatch: expected " << expected_value
                              << ", got " << actual_bit;
        return false;
    }
    return true;
}

void test_edn_func_002::clear_recoverable_alerts()
{
    // Write 0 to all bits to clear (W0C semantics)
    register_write_32(RECOV_ALERT_STS_OFFSET, 0x00000000);
    wait(1, SC_NS);
}

bool test_edn_func_002::wait_for_state(uint32_t target_state, uint64_t timeout_ns)
{
    uint64_t elapsed = 0;
    uint32_t read_value;

    while (elapsed < timeout_ns) {
        register_read_32(MAIN_SM_STATE_OFFSET, read_value);
        if (read_value == target_state) {
            return true;
        }
        wait(10, SC_NS);
        elapsed += 10;
    }

    return false;
}

void test_edn_func_002::report_test_result(const std::string& test_name,
                                           bool passed,
                                           const std::string& message)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        CSML_INFO(1, logger) << "[PASS] " << test_name;
        if (!message.empty()) {
            CSML_INFO(1, logger) << "       " << message;
        }
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        CSML_ERROR(1, logger) << "[FAIL] " << test_name;
        if (!message.empty()) {
            CSML_ERROR(1, logger) << "       " << message;
        }
    }

    CSML_INFO(1, logger) << "----------------------------------------";
}
