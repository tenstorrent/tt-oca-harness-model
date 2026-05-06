/**
 * @file test_edn_func_006.cpp
 * @brief EDN_FUNC_006 Test Suite Implementation
 *
 * Comprehensive implementation of all 13 test cases for EDN_FUNC_006
 * (Multi-bit Encoding Validation) functionality verification.
 *
 * Test Coverage Matrix:
 * - T1:  EDN_ENABLE valid values (0x6, 0x9) → no alerts
 * - T2:  EDN_ENABLE invalid values → EDN_ENABLE_FIELD_ALERT
 * - T3:  BOOT_REQ_MODE valid values → no alerts
 * - T4:  BOOT_REQ_MODE invalid values → BOOT_REQ_MODE_FIELD_ALERT
 * - T5:  AUTO_REQ_MODE valid values → no alerts
 * - T6:  AUTO_REQ_MODE invalid values → AUTO_REQ_MODE_FIELD_ALERT
 * - T7:  CMD_FIFO_RST valid values → no alerts
 * - T8:  CMD_FIFO_RST invalid values → CMD_FIFO_RST_FIELD_ALERT
 * - T9:  Multiple invalid fields simultaneously
 * - T10: W0C clearing mechanism
 * - T11: Alert signal deassertion
 * - T12: Valid value after invalid (alert persistence)
 * - T13: Comprehensive invalid value coverage (all 4 fields × 14 invalid values)
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-13
 */

#include "test_edn_func_006.h"
#include <iomanip>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_006::test_edn_func_006(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    CSML_INFO(1, logger) << "EDN_FUNC_006 test suite initialized";
    CSML_INFO(1, logger) << "Test Coverage: Multi-bit Encoding Validation (13 test cases)";
}

test_edn_func_006::~test_edn_func_006()
{
    CSML_INFO(1, logger) << "EDN_FUNC_006 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_006::run_all_tests()
{
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_006 Test Suite Execution Start";
    CSML_INFO(1, logger) << "========================================";

    // Apply reset before starting tests
    CSML_INFO(1, logger) << "Applying system reset...";
    apply_reset(100.0); // 100ns reset pulse
    wait(10, SC_NS);    // Wait for reset propagation

    // Execute all 13 test cases
    bool result;

    // Test 1: EDN_ENABLE Valid Values
    result = test_edn_enable_valid_values();
    report_test_result("T1: EDN_ENABLE Valid Values (0x6, 0x9)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 2: EDN_ENABLE Invalid Values
    result = test_edn_enable_invalid_values();
    report_test_result("T2: EDN_ENABLE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 3: BOOT_REQ_MODE Valid Values
    result = test_boot_req_mode_valid_values();
    report_test_result("T3: BOOT_REQ_MODE Valid Values (0x6, 0x9)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 4: BOOT_REQ_MODE Invalid Values
    result = test_boot_req_mode_invalid_values();
    report_test_result("T4: BOOT_REQ_MODE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 5: AUTO_REQ_MODE Valid Values
    result = test_auto_req_mode_valid_values();
    report_test_result("T5: AUTO_REQ_MODE Valid Values (0x6, 0x9)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 6: AUTO_REQ_MODE Invalid Values
    result = test_auto_req_mode_invalid_values();
    report_test_result("T6: AUTO_REQ_MODE Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 7: CMD_FIFO_RST Valid Values
    result = test_cmd_fifo_rst_valid_values();
    report_test_result("T7: CMD_FIFO_RST Valid Values (0x6, 0x9)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 8: CMD_FIFO_RST Invalid Values
    result = test_cmd_fifo_rst_invalid_values();
    report_test_result("T8: CMD_FIFO_RST Invalid Values Trigger Alert", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 9: Multiple Invalid Fields
    result = test_multiple_invalid_fields();
    report_test_result("T9: Multiple Invalid Fields Simultaneously", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 10: W0C Clearing Mechanism
    result = test_w0c_clearing_mechanism();
    report_test_result("T10: W0C Clearing Mechanism", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 11: Alert Signal Deassertion
    result = test_alert_signal_deassertion();
    report_test_result("T11: Alert Signal Deassertion", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 12: Valid Value After Invalid
    result = test_valid_value_after_invalid();
    report_test_result("T12: Valid Value After Invalid (Alert Persistence)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 13: Comprehensive Invalid Coverage
    result = test_comprehensive_invalid_coverage();
    report_test_result("T13: Comprehensive Invalid Value Coverage", result);

    // Print summary
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_006 Test Suite Summary";
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
// Test Case 1: EDN_ENABLE Valid Values
// =============================================================================

bool test_edn_func_006::test_edn_enable_valid_values()
{
    CSML_INFO(1, logger) << "Starting test_edn_enable_valid_values...";

    bool all_passed = true;
    uint32_t read_value;

    // Clear any existing alerts
    clear_recoverable_alerts();

    // Test 1: EDN_ENABLE = 0x6 (enable)
    uint32_t ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS); // Allow time for validation

    // Check RECOV_ALERT_STS bit [0] should be 0
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT", EDN_ENABLE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected EDN_ENABLE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Verify alert_recov_alert signal not asserted
    if (!verify_alert_signal(false)) {
        CSML_ERROR(1, logger) << "alert_recov_alert unexpectedly asserted for EDN_ENABLE=0x6";
        all_passed = false;
    }

    // Test 2: EDN_ENABLE = 0x9 (disable)
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Check RECOV_ALERT_STS bit [0] should still be 0
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT", EDN_ENABLE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected EDN_ENABLE_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    // Verify alert signal remains deasserted
    if (!verify_alert_signal(false)) {
        CSML_ERROR(1, logger) << "alert_recov_alert unexpectedly asserted for EDN_ENABLE=0x9";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_edn_enable_valid_values: PASSED - Valid values accepted without alerts";
    }

    return all_passed;
}

// =============================================================================
// Test Case 2: EDN_ENABLE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_006::test_edn_enable_invalid_values()
{
    CSML_INFO(1, logger) << "Starting test_edn_enable_invalid_values...";

    bool all_passed = true;

    // Test all 14 invalid values (0x0-0xF except 0x6 and 0x9)
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        uint32_t invalid_value = invalid_values[i];

        if (!test_single_invalid_value("EDN_ENABLE", EDN_ENABLE_SHIFT, invalid_value, EDN_ENABLE_FIELD_ALERT_BIT)) {
            all_passed = false;
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_edn_enable_invalid_values: PASSED - All 14 invalid values trigger alerts correctly";
    }

    return all_passed;
}

// =============================================================================
// Test Case 3: BOOT_REQ_MODE Valid Values
// =============================================================================

bool test_edn_func_006::test_boot_req_mode_valid_values()
{
    CSML_INFO(1, logger) << "Starting test_boot_req_mode_valid_values...";

    bool all_passed = true;
    uint32_t read_value;

    clear_recoverable_alerts();

    // Test 1: BOOT_REQ_MODE = 0x6
    uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_ENABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT", BOOT_REQ_MODE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected BOOT_REQ_MODE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Test 2: BOOT_REQ_MODE = 0x9
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT", BOOT_REQ_MODE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected BOOT_REQ_MODE_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    if (!verify_alert_signal(false)) {
        CSML_ERROR(1, logger) << "alert_recov_alert unexpectedly asserted for valid BOOT_REQ_MODE values";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_boot_req_mode_valid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 4: BOOT_REQ_MODE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_006::test_boot_req_mode_invalid_values()
{
    CSML_INFO(1, logger) << "Starting test_boot_req_mode_invalid_values...";

    bool all_passed = true;
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        if (!test_single_invalid_value("BOOT_REQ_MODE", BOOT_REQ_MODE_SHIFT, invalid_values[i], BOOT_REQ_MODE_FIELD_ALERT_BIT)) {
            all_passed = false;
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_boot_req_mode_invalid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 5: AUTO_REQ_MODE Valid Values
// =============================================================================

bool test_edn_func_006::test_auto_req_mode_valid_values()
{
    CSML_INFO(1, logger) << "Starting test_auto_req_mode_valid_values...";

    bool all_passed = true;
    uint32_t read_value;

    clear_recoverable_alerts();

    // Test 1: AUTO_REQ_MODE = 0x6
    uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_ENABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT", AUTO_REQ_MODE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected AUTO_REQ_MODE_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Test 2: AUTO_REQ_MODE = 0x9
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT", AUTO_REQ_MODE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected AUTO_REQ_MODE_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_auto_req_mode_valid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 6: AUTO_REQ_MODE Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_006::test_auto_req_mode_invalid_values()
{
    CSML_INFO(1, logger) << "Starting test_auto_req_mode_invalid_values...";

    bool all_passed = true;
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        if (!test_single_invalid_value("AUTO_REQ_MODE", AUTO_REQ_MODE_SHIFT, invalid_values[i], AUTO_REQ_MODE_FIELD_ALERT_BIT)) {
            all_passed = false;
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_auto_req_mode_invalid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 7: CMD_FIFO_RST Valid Values
// =============================================================================

bool test_edn_func_006::test_cmd_fifo_rst_valid_values()
{
    CSML_INFO(1, logger) << "Starting test_cmd_fifo_rst_valid_values...";

    bool all_passed = true;
    uint32_t read_value;

    clear_recoverable_alerts();

    // Test 1: CMD_FIFO_RST = 0x6
    uint32_t ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_ENABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT", CMD_FIFO_RST_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected CMD_FIFO_RST_FIELD_ALERT for valid value 0x6";
        all_passed = false;
    }

    // Test 2: CMD_FIFO_RST = 0x9
    ctrl_value = (MULTIBIT_DISABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT", CMD_FIFO_RST_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Unexpected CMD_FIFO_RST_FIELD_ALERT for valid value 0x9";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_cmd_fifo_rst_valid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 8: CMD_FIFO_RST Invalid Values Trigger Alert
// =============================================================================

bool test_edn_func_006::test_cmd_fifo_rst_invalid_values()
{
    CSML_INFO(1, logger) << "Starting test_cmd_fifo_rst_invalid_values...";

    bool all_passed = true;
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    for (size_t i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); ++i) {
        if (!test_single_invalid_value("CMD_FIFO_RST", CMD_FIFO_RST_SHIFT, invalid_values[i], CMD_FIFO_RST_FIELD_ALERT_BIT)) {
            all_passed = false;
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_cmd_fifo_rst_invalid_values: PASSED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 9: Multiple Invalid Fields Simultaneously
// =============================================================================

bool test_edn_func_006::test_multiple_invalid_fields()
{
    CSML_INFO(1, logger) << "Starting test_multiple_invalid_fields...";

    bool all_passed = true;
    uint32_t read_value;

    // Test 1: Two invalid fields (EDN_ENABLE=0x0, BOOT_REQ_MODE=0x1)
    clear_recoverable_alerts();

    uint32_t ctrl_value = (0x0 << EDN_ENABLE_SHIFT) |
                          (0x1 << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);

    // Verify bits [0] and [1] are both set
    if (!verify_bit_value("RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT", EDN_ENABLE_FIELD_ALERT_BIT, 1, read_value)) {
        CSML_ERROR(1, logger) << "EDN_ENABLE_FIELD_ALERT not set for multiple violations";
        all_passed = false;
    }

    if (!verify_bit_value("RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT", BOOT_REQ_MODE_FIELD_ALERT_BIT, 1, read_value)) {
        CSML_ERROR(1, logger) << "BOOT_REQ_MODE_FIELD_ALERT not set for multiple violations";
        all_passed = false;
    }

    // Verify alert signal asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not asserted for multiple violations";
        all_passed = false;
    }

    // Test 2: All four fields invalid
    clear_recoverable_alerts();

    ctrl_value = (0x2 << EDN_ENABLE_SHIFT) |
                 (0x3 << BOOT_REQ_MODE_SHIFT) |
                 (0x4 << AUTO_REQ_MODE_SHIFT) |
                 (0x5 << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);

    // Verify all 4 alert bits are set (bits [0:3])
    if ((read_value & 0xF) != 0xF) {
        CSML_ERROR(1, logger) << "Not all 4 alert bits set for 4 invalid fields. RECOV_ALERT_STS = 0x"
                              << std::hex << read_value;
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "All 4 alert bits correctly set for 4 invalid fields";
    }

    // Verify alert signal asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not asserted for 4 invalid fields";
        all_passed = false;
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_multiple_invalid_fields: PASSED - Multiple alerts trigger independently";
    }

    return all_passed;
}

// =============================================================================
// Test Case 10: W0C Clearing Mechanism
// =============================================================================

bool test_edn_func_006::test_w0c_clearing_mechanism()
{
    CSML_INFO(1, logger) << "Starting test_w0c_clearing_mechanism...";

    bool all_passed = true;
    uint32_t read_value;

    // Test W0C semantics for each alert bit
    const unsigned int alert_bits[] = {
        EDN_ENABLE_FIELD_ALERT_BIT,
        BOOT_REQ_MODE_FIELD_ALERT_BIT,
        AUTO_REQ_MODE_FIELD_ALERT_BIT,
        CMD_FIFO_RST_FIELD_ALERT_BIT
    };

    const unsigned int field_shifts[] = {
        EDN_ENABLE_SHIFT,
        BOOT_REQ_MODE_SHIFT,
        AUTO_REQ_MODE_SHIFT,
        CMD_FIFO_RST_SHIFT
    };

    const char* field_names[] = {
        "EDN_ENABLE",
        "BOOT_REQ_MODE",
        "AUTO_REQ_MODE",
        "CMD_FIFO_RST"
    };

    for (size_t i = 0; i < 4; ++i) {
        CSML_INFO(1, logger) << "Testing W0C for " << field_names[i] << " alert bit [" << alert_bits[i] << "]";

        clear_recoverable_alerts();

        // Trigger the alert by writing invalid value to the field
        uint32_t ctrl_value = CTRL_RESET;
        ctrl_value &= ~(0xF << field_shifts[i]); // Clear field
        ctrl_value |= (0x5 << field_shifts[i]);   // Set invalid value

        register_write_32(CTRL_OFFSET, ctrl_value);
        wait(5, SC_NS);

        // Verify alert bit is set
        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!verify_bit_value("RECOV_ALERT_STS", alert_bits[i], 1, read_value)) {
            CSML_ERROR(1, logger) << "Alert bit " << alert_bits[i] << " not set initially";
            all_passed = false;
            continue;
        }

        // Test 1: Write all 1's (should have NO effect - bit remains set)
        register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFF);
        wait(1, SC_NS);

        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!verify_bit_value("RECOV_ALERT_STS after write 1's", alert_bits[i], 1, read_value)) {
            CSML_ERROR(1, logger) << "Alert bit " << alert_bits[i] << " incorrectly cleared by writing 1";
            all_passed = false;
        } else {
            CSML_INFO(1, logger) << "W0C verified: Writing 1 has no effect on bit " << alert_bits[i];
        }

        // Test 2: Write 0 to specific bit (should clear it)
        uint32_t clear_mask = 0xFFFFFFFF & ~(1 << alert_bits[i]); // All 1's except target bit
        register_write_32(RECOV_ALERT_STS_OFFSET, clear_mask);
        wait(1, SC_NS);

        register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
        if (!verify_bit_value("RECOV_ALERT_STS after write 0", alert_bits[i], 0, read_value)) {
            CSML_ERROR(1, logger) << "Alert bit " << alert_bits[i] << " not cleared by writing 0";
            all_passed = false;
        } else {
            CSML_INFO(1, logger) << "W0C verified: Writing 0 clears bit " << alert_bits[i];
        }
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_w0c_clearing_mechanism: PASSED - W0C semantics correct for all alert bits";
    }

    return all_passed;
}

// =============================================================================
// Test Case 11: Alert Signal Deassertion
// =============================================================================

bool test_edn_func_006::test_alert_signal_deassertion()
{
    CSML_INFO(1, logger) << "Starting test_alert_signal_deassertion...";

    bool all_passed = true;
    uint32_t read_value;

    clear_recoverable_alerts();

    // Set multiple alert bits (bits 0, 1, 2)
    uint32_t ctrl_value = (0x0 << EDN_ENABLE_SHIFT) |      // Invalid
                          (0x1 << BOOT_REQ_MODE_SHIFT) |   // Invalid
                          (0x2 << AUTO_REQ_MODE_SHIFT) |   // Invalid
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Verify all 3 bits set
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value & 0x7) != 0x7) {
        CSML_ERROR(1, logger) << "Expected bits [0:2] set, got 0x" << std::hex << read_value;
        all_passed = false;
    }

    // Verify alert signal asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not asserted with multiple bits set";
        all_passed = false;
    }

    // Clear bit [0] only - alert should remain asserted
    register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFE); // Clear bit 0
    wait(1, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS after clearing bit 0", EDN_ENABLE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Bit 0 not cleared";
        all_passed = false;
    }

    // Alert should still be asserted (bits 1, 2 still set)
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert incorrectly deasserted with bits 1,2 still set";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert correctly remains asserted after clearing one bit";
    }

    // Clear bit [1] - alert should still be asserted
    register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFD); // Clear bit 1
    wait(1, SC_NS);

    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert incorrectly deasserted with bit 2 still set";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert correctly remains asserted after clearing two bits";
    }

    // Clear bit [2] - now alert should deassert
    register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFB); // Clear bit 2
    wait(1, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if ((read_value & 0xF) != 0x0) {
        CSML_ERROR(1, logger) << "Not all alert bits cleared. RECOV_ALERT_STS = 0x" << std::hex << read_value;
        all_passed = false;
    }

    // Alert should now be deasserted
    if (!verify_alert_signal(false)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not deasserted after clearing all bits";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert correctly deasserted after clearing all bits";
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_alert_signal_deassertion: PASSED - Alert deasserts only when all bits cleared";
    }

    return all_passed;
}

// =============================================================================
// Test Case 12: Valid Value After Invalid (Alert Persistence)
// =============================================================================

bool test_edn_func_006::test_valid_value_after_invalid()
{
    CSML_INFO(1, logger) << "Starting test_valid_value_after_invalid...";

    bool all_passed = true;
    uint32_t read_value;

    clear_recoverable_alerts();

    // Write invalid EDN_ENABLE value
    uint32_t ctrl_value = (0x5 << EDN_ENABLE_SHIFT) |
                          (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                          (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Verify alert bit set
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS after invalid", EDN_ENABLE_FIELD_ALERT_BIT, 1, read_value)) {
        CSML_ERROR(1, logger) << "Alert bit not set for invalid value";
        all_passed = false;
    }

    // Verify alert signal asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not asserted for invalid value";
        all_passed = false;
    }

    // Write valid EDN_ENABLE value (should NOT clear alert)
    ctrl_value = (MULTIBIT_ENABLE << EDN_ENABLE_SHIFT) |
                 (MULTIBIT_DISABLE << BOOT_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << AUTO_REQ_MODE_SHIFT) |
                 (MULTIBIT_DISABLE << CMD_FIFO_RST_SHIFT);

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Verify alert bit STILL set (sticky)
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS after valid write", EDN_ENABLE_FIELD_ALERT_BIT, 1, read_value)) {
        CSML_ERROR(1, logger) << "Alert bit incorrectly cleared by valid value write";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert bit correctly sticky after valid value write";
    }

    // Verify alert signal STILL asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << "alert_recov_alert incorrectly deasserted after valid value write";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert signal correctly remains asserted after valid value write";
    }

    // Firmware must explicitly clear alert
    register_write_32(RECOV_ALERT_STS_OFFSET, 0xFFFFFFFE); // Clear bit 0
    wait(1, SC_NS);

    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS after clear", EDN_ENABLE_FIELD_ALERT_BIT, 0, read_value)) {
        CSML_ERROR(1, logger) << "Alert bit not cleared by firmware write";
        all_passed = false;
    }

    // Alert should now be deasserted
    if (!verify_alert_signal(false)) {
        CSML_ERROR(1, logger) << "alert_recov_alert not deasserted after firmware clear";
        all_passed = false;
    } else {
        CSML_INFO(1, logger) << "Alert correctly deasserted after firmware clears bit";
    }

    if (all_passed) {
        CSML_INFO(1, logger) << "test_valid_value_after_invalid: PASSED - Alert sticky until firmware clears";
    }

    return all_passed;
}

// =============================================================================
// Test Case 13: Comprehensive Invalid Value Coverage
// =============================================================================

bool test_edn_func_006::test_comprehensive_invalid_coverage()
{
    CSML_INFO(1, logger) << "Starting test_comprehensive_invalid_coverage...";
    CSML_INFO(1, logger) << "Testing all 4 fields x 14 invalid values = 56 tests";

    bool all_passed = true;
    unsigned int total_tests = 0;
    unsigned int passed_tests = 0;

    // All 14 invalid values
    uint32_t invalid_values[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};

    // Field configurations
    struct FieldConfig {
        const char* name;
        unsigned int shift;
        unsigned int alert_bit;
    };

    FieldConfig fields[] = {
        {"EDN_ENABLE", EDN_ENABLE_SHIFT, EDN_ENABLE_FIELD_ALERT_BIT},
        {"BOOT_REQ_MODE", BOOT_REQ_MODE_SHIFT, BOOT_REQ_MODE_FIELD_ALERT_BIT},
        {"AUTO_REQ_MODE", AUTO_REQ_MODE_SHIFT, AUTO_REQ_MODE_FIELD_ALERT_BIT},
        {"CMD_FIFO_RST", CMD_FIFO_RST_SHIFT, CMD_FIFO_RST_FIELD_ALERT_BIT}
    };

    // Test each field with each invalid value
    for (size_t field_idx = 0; field_idx < 4; ++field_idx) {
        CSML_INFO(1, logger) << "Testing field: " << fields[field_idx].name;

        for (size_t val_idx = 0; val_idx < 14; ++val_idx) {
            total_tests++;

            if (test_single_invalid_value(fields[field_idx].name,
                                         fields[field_idx].shift,
                                         invalid_values[val_idx],
                                         fields[field_idx].alert_bit)) {
                passed_tests++;
            } else {
                all_passed = false;
            }
        }
    }

    CSML_INFO(1, logger) << "Comprehensive coverage results: " << passed_tests << "/" << total_tests << " tests passed";

    if (all_passed) {
        CSML_INFO(1, logger) << "test_comprehensive_invalid_coverage: PASSED - 100% invalid value coverage";
    } else {
        CSML_ERROR(1, logger) << "test_comprehensive_invalid_coverage: FAILED - Some invalid values not detected";
    }

    return all_passed;
}

// =============================================================================
// Helper Functions
// =============================================================================

bool test_edn_func_006::verify_register_value(const std::string& reg_name,
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

bool test_edn_func_006::verify_bit_value(const std::string& reg_name,
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

void test_edn_func_006::clear_recoverable_alerts()
{
    // Write 0 to all bits to clear (W0C semantics)
    register_write_32(RECOV_ALERT_STS_OFFSET, 0x00000000);
    wait(1, SC_NS);
}

bool test_edn_func_006::verify_alert_signal(bool expected_state)
{
    bool actual_state = alert_recov_alert.read();
    if (actual_state != expected_state) {
        CSML_ERROR(1, logger) << "alert_recov_alert mismatch: expected "
                              << (expected_state ? "asserted" : "deasserted")
                              << ", got " << (actual_state ? "asserted" : "deasserted");
        return false;
    }
    return true;
}

bool test_edn_func_006::test_single_invalid_value(const std::string& field_name,
                                                  unsigned int field_shift,
                                                  uint32_t invalid_value,
                                                  unsigned int alert_bit)
{
    uint32_t read_value;

    // Clear any existing alerts
    clear_recoverable_alerts();

    // Build CTRL value with invalid field, other fields = 0x9
    uint32_t ctrl_value = CTRL_RESET; // All fields = 0x9
    ctrl_value &= ~(0xF << field_shift); // Clear target field
    ctrl_value |= (invalid_value << field_shift); // Set invalid value

    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(5, SC_NS);

    // Verify alert bit is set
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS", alert_bit, 1, read_value)) {
        CSML_ERROR(1, logger) << field_name << ": Alert bit not set for invalid value 0x"
                              << std::hex << invalid_value;
        return false;
    }

    // Verify alert signal asserted
    if (!verify_alert_signal(true)) {
        CSML_ERROR(1, logger) << field_name << ": Alert signal not asserted for invalid value 0x"
                              << std::hex << invalid_value;
        return false;
    }

    // Clear the alert
    uint32_t clear_mask = 0xFFFFFFFF & ~(1 << alert_bit);
    register_write_32(RECOV_ALERT_STS_OFFSET, clear_mask);
    wait(1, SC_NS);

    // Verify alert bit cleared
    register_read_32(RECOV_ALERT_STS_OFFSET, read_value);
    if (!verify_bit_value("RECOV_ALERT_STS after clear", alert_bit, 0, read_value)) {
        CSML_ERROR(1, logger) << field_name << ": Alert bit not cleared for invalid value 0x"
                              << std::hex << invalid_value;
        return false;
    }

    return true;
}

void test_edn_func_006::report_test_result(const std::string& test_name,
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
