// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_007.cpp
 * @brief EDN_FUNC_007 Test Suite Implementation
 *
 * Comprehensive implementation of all 10 test cases for EDN_FUNC_007
 * (FIFO Overflow Detection and Handling) functionality verification.
 *
 * Test Coverage Matrix:
 * - T1:  RESEED_CMD FIFO 13-word boundary (no overflow)
 * - T2:  RESEED_CMD FIFO 14-word overflow (SFIFO_RESCMD_ERR)
 * - T3:  GENERATE_CMD FIFO 13-word boundary (no overflow)
 * - T4:  GENERATE_CMD FIFO 14-word overflow (SFIFO_GENCMD_ERR)
 * - T5:  FIFO_WRITE_ERR generic indicator (bit 28)
 * - T6:  Fatal alert assertion on overflow
 * - T7:  Fatal error interrupt generation
 * - T8:  State machine transition to Error state (0x47)
 * - T9:  ERR_CODE sticky behavior (read-only, reset clears)
 * - T10: FIFO clearing on reset
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#include "test_edn_func_007.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_007::test_edn_func_007(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    CSML_INFO(1, logger) << "EDN_FUNC_007 test suite initialized";
    CSML_INFO(1, logger) << "Test Coverage: FIFO Overflow Detection and Handling (10 test cases)";
}

test_edn_func_007::~test_edn_func_007()
{
    CSML_INFO(1, logger) << "EDN_FUNC_007 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_007::run_all_tests()
{
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_007 Test Suite Execution Start";
    CSML_INFO(1, logger) << "========================================";

    // Apply reset before starting tests
    CSML_INFO(1, logger) << "Applying system reset...";
    apply_reset(100.0); // 100ns reset pulse
    wait(10, SC_NS);    // Wait for reset propagation

    // Execute all 10 test cases
    bool result;

    // Test 1: RESEED_CMD FIFO 13-Word Boundary
    result = test_reseed_fifo_13word_boundary();
    report_test_result("T1: RESEED_CMD FIFO 13-Word Boundary (No Overflow)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 2: RESEED_CMD FIFO 14-Word Overflow
    result = test_reseed_fifo_14word_overflow();
    report_test_result("T2: RESEED_CMD FIFO 14-Word Overflow (SFIFO_RESCMD_ERR)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 3: GENERATE_CMD FIFO 13-Word Boundary
    result = test_generate_fifo_13word_boundary();
    report_test_result("T3: GENERATE_CMD FIFO 13-Word Boundary (No Overflow)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 4: GENERATE_CMD FIFO 14-Word Overflow
    result = test_generate_fifo_14word_overflow();
    report_test_result("T4: GENERATE_CMD FIFO 14-Word Overflow (SFIFO_GENCMD_ERR)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 5: FIFO_WRITE_ERR Generic Indicator
    result = test_fifo_write_err_generic();
    report_test_result("T5: FIFO_WRITE_ERR Generic Indicator (Bit 28)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 6: Fatal Alert Assertion
    result = test_fatal_alert_assertion();
    report_test_result("T6: Fatal Alert Assertion on Overflow", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 7: Fatal Interrupt Generation
    result = test_fatal_interrupt_generation();
    report_test_result("T7: Fatal Error Interrupt Generation", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 8: Error State Transition
    result = test_error_state_transition();
    report_test_result("T8: State Machine Transition to Error State (0x47)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 9: ERR_CODE Sticky Behavior
    result = test_err_code_sticky_behavior();
    report_test_result("T9: ERR_CODE Sticky Behavior (Read-Only, Reset Clears)", result);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Test 10: FIFO Reset Clearing
    result = test_fifo_reset_clearing();
    report_test_result("T10: FIFO Clearing on Reset", result);

    // Print final summary
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "EDN_FUNC_007 Test Suite Execution Complete";
    CSML_INFO(1, logger) << "========================================";
    CSML_INFO(1, logger) << "Total Tests Run:    " << m_tests_run;
    CSML_INFO(1, logger) << "Tests Passed:       " << m_tests_passed;
    CSML_INFO(1, logger) << "Tests Failed:       " << m_tests_failed;

    if (m_tests_failed > 0) {
        CSML_ERROR(1, logger) << "Failed Tests:";
        for (const auto& test_name : m_failed_tests) {
            CSML_ERROR(1, logger) << "  - " << test_name;
        }
    }

    return m_tests_failed;
}

// =============================================================================
// Test Case 1: RESEED_CMD FIFO 13-Word Boundary
// =============================================================================

bool test_edn_func_007::test_reseed_fifo_13word_boundary()
{
    CSML_INFO(1, logger) << "Starting T1: RESEED_CMD FIFO 13-Word Boundary Test";

    bool test_passed = true;
    uint32_t err_code_val, main_sm_state_val;

    // Step 1: Verify initial state after reset
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        test_passed = false;
    }

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_IDLE, main_sm_state_val)) {
        test_passed = false;
    }

    // Step 2: Write exactly 13 words to RESEED_CMD FIFO
    CSML_INFO(1, logger) << "Writing 13 words to RESEED_CMD FIFO...";
    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH, 0x00000001, "RESEED_CMD");

    // Step 3: Wait for error detection logic
    wait(10, SC_NS);

    // Step 4: Verify no error occurred
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        CSML_ERROR(1, logger) << "T1 FAIL: ERR_CODE should be 0x0 after 13 words, got 0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 5: Verify specific error bits remain clear
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 0)) {
        CSML_ERROR(1, logger) << "T1 FAIL: SFIFO_RESCMD_ERR bit [0] should be 0";
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 0)) {
        CSML_ERROR(1, logger) << "T1 FAIL: FIFO_WRITE_ERR bit [28] should be 0";
        test_passed = false;
    }

    // Step 6: Verify state machine not in Error state
    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (main_sm_state_val == STATE_ERROR) {
        CSML_ERROR(1, logger) << "T1 FAIL: MAIN_SM_STATE should not be in Error state (0x47)";
        test_passed = false;
    }

    // Step 7: Verify alert not asserted
    bool fatal_alert = alert_fatal_alert.read();
    if (fatal_alert) {
        CSML_ERROR(1, logger) << "T1 FAIL: alert_fatal_alert should not be asserted";
        test_passed = false;
    }

    // Step 8: Verify interrupt not asserted
    bool fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T1 FAIL: intr_edn_fatal_err should not be asserted";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T1 PASS: RESEED_CMD FIFO accepts 13 words without overflow";
    }

    return test_passed;
}

// =============================================================================
// Test Case 2: RESEED_CMD FIFO 14-Word Overflow
// =============================================================================

bool test_edn_func_007::test_reseed_fifo_14word_overflow()
{
    CSML_INFO(1, logger) << "Starting T2: RESEED_CMD FIFO 14-Word Overflow Test";

    bool test_passed = true;
    uint32_t err_code_val, intr_state_val, main_sm_state_val;

    // Step 1: Enable fatal error interrupt
    register_write_32(INTR_ENABLE_OFFSET, 0x00000002); // Bit [1] = edn_fatal_err enable
    wait(1, SC_NS);

    // Step 2: Write 13 words (should succeed)
    CSML_INFO(1, logger) << "Writing 13 words to RESEED_CMD FIFO...";
    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    // Verify no error yet
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (err_code_val != 0x00000000) {
        CSML_ERROR(1, logger) << "T2 FAIL: ERR_CODE should be 0x0 after 13 words, got 0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 3: Write 14th word (trigger overflow)
    CSML_INFO(1, logger) << "Writing 14th word to RESEED_CMD FIFO (trigger overflow)...";
    register_write_32(RESEED_CMD_OFFSET, 0x0000000E); // 14th word

    // Step 4: Wait for error detection and propagation
    wait(10, SC_NS);

    // Step 5: Verify ERR_CODE bits set correctly
    register_read_32(ERR_CODE_OFFSET, err_code_val);

    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T2 FAIL: SFIFO_RESCMD_ERR bit [0] should be 1, ERR_CODE=0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T2 FAIL: FIFO_WRITE_ERR bit [28] should be 1, ERR_CODE=0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 6: Verify INTR_STATE bit set
    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_bit_value("INTR_STATE", EDN_FATAL_ERR_BIT, 1, intr_state_val)) {
        test_passed = false;
    }

    // Step 7: Verify alert_fatal_alert asserted
    bool fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T2 FAIL: alert_fatal_alert should be asserted";
        test_passed = false;
    }

    // Step 8: Verify intr_edn_fatal_err asserted
    bool fatal_intr = intr_edn_fatal_err.read();
    if (!fatal_intr) {
        CSML_ERROR(1, logger) << "T2 FAIL: intr_edn_fatal_err should be asserted";
        test_passed = false;
    }

    // Step 9: Verify state machine in Error state
    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_ERROR, main_sm_state_val)) {
        test_passed = false;
    }

    // Step 10: Attempt to clear ERR_CODE (verify sticky behavior)
    CSML_INFO(1, logger) << "Testing ERR_CODE sticky behavior (write 0x0)...";
    register_write_32(ERR_CODE_OFFSET, 0x00000000);
    wait(1, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T2 FAIL: ERR_CODE should remain set after write attempt";
        test_passed = false;
    }

    // Step 11: Clear INTR_STATE (W1C) - interrupt clears but ERR_CODE remains
    CSML_INFO(1, logger) << "Clearing INTR_STATE[1] via W1C...";
    register_write_32(INTR_STATE_OFFSET, 0x00000002); // Write 1 to bit [1] to clear
    wait(1, SC_NS);

    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_bit_value("INTR_STATE", EDN_FATAL_ERR_BIT, 0, intr_state_val)) {
        CSML_ERROR(1, logger) << "T2 FAIL: INTR_STATE[1] should be cleared via W1C";
        test_passed = false;
    }

    // Step 12: Verify alert remains asserted (sticky)
    fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T2 FAIL: alert_fatal_alert should remain asserted despite interrupt clear";
        test_passed = false;
    }

    // Verify ERR_CODE still set
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T2 FAIL: ERR_CODE should remain set after INTR_STATE clear";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T2 PASS: RESEED_CMD FIFO 14th word triggers overflow with correct error propagation";
    }

    return test_passed;
}

// =============================================================================
// Test Case 3: GENERATE_CMD FIFO 13-Word Boundary
// =============================================================================

bool test_edn_func_007::test_generate_fifo_13word_boundary()
{
    CSML_INFO(1, logger) << "Starting T3: GENERATE_CMD FIFO 13-Word Boundary Test";

    bool test_passed = true;
    uint32_t err_code_val, main_sm_state_val;

    // Step 1: Verify initial state
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        test_passed = false;
    }

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_IDLE, main_sm_state_val)) {
        test_passed = false;
    }

    // Step 2: Write exactly 13 words to GENERATE_CMD FIFO
    CSML_INFO(1, logger) << "Writing 13 words to GENERATE_CMD FIFO...";
    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH, 0x10000001, "GENERATE_CMD");

    // Step 3: Wait for error detection
    wait(10, SC_NS);

    // Step 4: Verify no error occurred
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        CSML_ERROR(1, logger) << "T3 FAIL: ERR_CODE should be 0x0 after 13 words, got 0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 5: Verify specific error bits remain clear
    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 0)) {
        CSML_ERROR(1, logger) << "T3 FAIL: SFIFO_GENCMD_ERR bit [1] should be 0";
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 0)) {
        CSML_ERROR(1, logger) << "T3 FAIL: FIFO_WRITE_ERR bit [28] should be 0";
        test_passed = false;
    }

    // Step 6: Verify state machine not in Error state
    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (main_sm_state_val == STATE_ERROR) {
        CSML_ERROR(1, logger) << "T3 FAIL: MAIN_SM_STATE should not be in Error state (0x47)";
        test_passed = false;
    }

    // Step 7: Verify alert not asserted
    bool fatal_alert = alert_fatal_alert.read();
    if (fatal_alert) {
        CSML_ERROR(1, logger) << "T3 FAIL: alert_fatal_alert should not be asserted";
        test_passed = false;
    }

    // Step 8: Verify interrupt not asserted
    bool fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T3 FAIL: intr_edn_fatal_err should not be asserted";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T3 PASS: GENERATE_CMD FIFO accepts 13 words without overflow";
    }

    return test_passed;
}

// =============================================================================
// Test Case 4: GENERATE_CMD FIFO 14-Word Overflow
// =============================================================================

bool test_edn_func_007::test_generate_fifo_14word_overflow()
{
    CSML_INFO(1, logger) << "Starting T4: GENERATE_CMD FIFO 14-Word Overflow Test";

    bool test_passed = true;
    uint32_t err_code_val, intr_state_val, main_sm_state_val;

    // Step 1: Enable fatal error interrupt
    register_write_32(INTR_ENABLE_OFFSET, 0x00000002);
    wait(1, SC_NS);

    // Step 2: Write 13 words (should succeed)
    CSML_INFO(1, logger) << "Writing 13 words to GENERATE_CMD FIFO...";
    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    // Verify no error yet
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (err_code_val != 0x00000000) {
        CSML_ERROR(1, logger) << "T4 FAIL: ERR_CODE should be 0x0 after 13 words";
        test_passed = false;
    }

    // Step 3: Write 14th word (trigger overflow)
    CSML_INFO(1, logger) << "Writing 14th word to GENERATE_CMD FIFO (trigger overflow)...";
    register_write_32(GENERATE_CMD_OFFSET, 0x1000000E);

    // Step 4: Wait for error propagation
    wait(10, SC_NS);

    // Step 5: Verify ERR_CODE bits set correctly
    register_read_32(ERR_CODE_OFFSET, err_code_val);

    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T4 FAIL: SFIFO_GENCMD_ERR bit [1] should be 1, ERR_CODE=0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T4 FAIL: FIFO_WRITE_ERR bit [28] should be 1, ERR_CODE=0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 6: Verify INTR_STATE, alerts, and state machine
    if (!verify_error_state(0, 1, 1, true, true, STATE_ERROR)) {
        test_passed = false;
    }

    // Step 7: Test sticky behavior
    register_write_32(ERR_CODE_OFFSET, 0xFFFFFFFF);
    wait(1, SC_NS);

    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T4 FAIL: ERR_CODE should remain set (sticky)";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T4 PASS: GENERATE_CMD FIFO 14th word triggers overflow with correct error propagation";
    }

    return test_passed;
}

// =============================================================================
// Test Case 5: FIFO_WRITE_ERR Generic Indicator
// =============================================================================

bool test_edn_func_007::test_fifo_write_err_generic()
{
    CSML_INFO(1, logger) << "Starting T5: FIFO_WRITE_ERR Generic Indicator Test";

    bool test_passed = true;
    uint32_t err_code_val;

    // Part A: RESEED_CMD overflow sets FIFO_WRITE_ERR
    CSML_INFO(1, logger) << "Part A: Testing RESEED_CMD overflow sets FIFO_WRITE_ERR...";

    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);

    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T5 FAIL (Part A): SFIFO_RESCMD_ERR bit [0] should be 1";
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T5 FAIL (Part A): FIFO_WRITE_ERR bit [28] should be 1";
        test_passed = false;
    }

    // Reset for Part B
    apply_reset(100.0);
    wait(10, SC_NS);

    // Part B: GENERATE_CMD overflow sets FIFO_WRITE_ERR
    CSML_INFO(1, logger) << "Part B: Testing GENERATE_CMD overflow sets FIFO_WRITE_ERR...";

    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);

    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T5 FAIL (Part B): SFIFO_GENCMD_ERR bit [1] should be 1";
        test_passed = false;
    }

    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T5 FAIL (Part B): FIFO_WRITE_ERR bit [28] should be 1";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T5 PASS: FIFO_WRITE_ERR bit [28] set for both FIFO overflows";
    }

    return test_passed;
}

// =============================================================================
// Test Case 6: Fatal Alert Assertion on Overflow
// =============================================================================

bool test_edn_func_007::test_fatal_alert_assertion()
{
    CSML_INFO(1, logger) << "Starting T6: Fatal Alert Assertion Test";

    bool test_passed = true;
    bool fatal_alert;

    // Part A: RESEED_CMD overflow triggers alert
    CSML_INFO(1, logger) << "Part A: Testing alert assertion on RESEED_CMD overflow...";

    fatal_alert = alert_fatal_alert.read();
    if (fatal_alert) {
        CSML_ERROR(1, logger) << "T6 FAIL: alert_fatal_alert should be 0 initially";
        test_passed = false;
    }

    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T6 FAIL: alert_fatal_alert should be asserted after overflow";
        test_passed = false;
    }

    // Test sticky behavior - clear interrupt but alert remains
    register_write_32(INTR_STATE_OFFSET, 0x00000002); // W1C bit [1]
    wait(1, SC_NS);

    fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T6 FAIL: alert_fatal_alert should remain asserted (sticky)";
        test_passed = false;
    }

    // Reset and verify alert clears
    apply_reset(100.0);
    wait(10, SC_NS);

    fatal_alert = alert_fatal_alert.read();
    if (fatal_alert) {
        CSML_ERROR(1, logger) << "T6 FAIL: alert_fatal_alert should be deasserted after reset";
        test_passed = false;
    }

    // Part B: GENERATE_CMD overflow triggers alert
    CSML_INFO(1, logger) << "Part B: Testing alert assertion on GENERATE_CMD overflow...";

    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T6 FAIL: alert_fatal_alert should be asserted for GENERATE_CMD overflow";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T6 PASS: Fatal alert assertion and sticky behavior verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 7: Fatal Error Interrupt Generation
// =============================================================================

bool test_edn_func_007::test_fatal_interrupt_generation()
{
    CSML_INFO(1, logger) << "Starting T7: Fatal Error Interrupt Generation Test";

    bool test_passed = true;
    bool fatal_intr;
    uint32_t intr_state_val, err_code_val;

    // Part A: Interrupt generation when enabled
    CSML_INFO(1, logger) << "Part A: Testing interrupt generation with INTR_ENABLE[1]=1...";

    register_write_32(INTR_ENABLE_OFFSET, 0x00000002); // Enable edn_fatal_err interrupt
    wait(1, SC_NS);

    fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T7 FAIL: intr_edn_fatal_err should be 0 initially";
        test_passed = false;
    }

    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    fatal_intr = intr_edn_fatal_err.read();
    if (!fatal_intr) {
        CSML_ERROR(1, logger) << "T7 FAIL: intr_edn_fatal_err should be asserted";
        test_passed = false;
    }

    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_bit_value("INTR_STATE", EDN_FATAL_ERR_BIT, 1, intr_state_val)) {
        test_passed = false;
    }

    // Clear interrupt via W1C
    CSML_INFO(1, logger) << "Clearing interrupt via W1C...";
    register_write_32(INTR_STATE_OFFSET, 0x00000002);
    wait(1, SC_NS);

    fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T7 FAIL: intr_edn_fatal_err should be deasserted after W1C";
        test_passed = false;
    }

    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_bit_value("INTR_STATE", EDN_FATAL_ERR_BIT, 0, intr_state_val)) {
        CSML_ERROR(1, logger) << "T7 FAIL: INTR_STATE[1] should be cleared";
        test_passed = false;
    }

    // Verify ERR_CODE remains set (sticky)
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T7 FAIL: ERR_CODE should remain set after interrupt clear";
        test_passed = false;
    }

    // Verify alert remains asserted (sticky)
    bool fatal_alert = alert_fatal_alert.read();
    if (!fatal_alert) {
        CSML_ERROR(1, logger) << "T7 FAIL: alert_fatal_alert should remain asserted";
        test_passed = false;
    }

    // Part B: Interrupt masking when disabled
    apply_reset(100.0);
    wait(10, SC_NS);

    CSML_INFO(1, logger) << "Part B: Testing interrupt masking with INTR_ENABLE[1]=0...";

    register_write_32(INTR_ENABLE_OFFSET, 0x00000000); // Disable interrupts
    wait(1, SC_NS);

    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T7 FAIL: intr_edn_fatal_err should be masked when INTR_ENABLE[1]=0";
        test_passed = false;
    }

    // INTR_STATE should still be set (status bit)
    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_bit_value("INTR_STATE", EDN_FATAL_ERR_BIT, 1, intr_state_val)) {
        CSML_ERROR(1, logger) << "T7 FAIL: INTR_STATE[1] should be set even when masked";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T7 PASS: Fatal error interrupt generation and masking verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 8: State Machine Transition to Error State
// =============================================================================

bool test_edn_func_007::test_error_state_transition()
{
    CSML_INFO(1, logger) << "Starting T8: Error State Transition Test";

    bool test_passed = true;
    uint32_t main_sm_state_val;

    // Part A: RESEED_CMD overflow causes Error state
    CSML_INFO(1, logger) << "Part A: Testing state transition on RESEED_CMD overflow...";

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_IDLE, main_sm_state_val)) {
        test_passed = false;
    }

    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_ERROR, main_sm_state_val)) {
        CSML_ERROR(1, logger) << "T8 FAIL: MAIN_SM_STATE should transition to Error (0x47), got 0x"
                          << std::hex << main_sm_state_val;
        test_passed = false;
    }

    // Attempt to change state via CTRL register (should be ignored)
    CSML_INFO(1, logger) << "Attempting to change state via CTRL (should be ignored)...";
    register_write_32(CTRL_OFFSET, 0x00009996); // Try to enable EDN
    wait(1, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_ERROR, main_sm_state_val)) {
        CSML_ERROR(1, logger) << "T8 FAIL: MAIN_SM_STATE should remain in Error state";
        test_passed = false;
    }

    // Verify only reset exits Error state
    CSML_INFO(1, logger) << "Applying reset to exit Error state...";
    apply_reset(100.0);
    wait(10, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_IDLE, main_sm_state_val)) {
        CSML_ERROR(1, logger) << "T8 FAIL: MAIN_SM_STATE should return to Idle after reset";
        test_passed = false;
    }

    // Part B: GENERATE_CMD overflow causes Error state
    CSML_INFO(1, logger) << "Part B: Testing state transition on GENERATE_CMD overflow...";

    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_ERROR, main_sm_state_val)) {
        CSML_ERROR(1, logger) << "T8 FAIL: GENERATE_CMD overflow should cause Error state";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T8 PASS: State machine transitions to Error state on overflow";
    }

    return test_passed;
}

// =============================================================================
// Test Case 9: ERR_CODE Sticky Behavior
// =============================================================================

bool test_edn_func_007::test_err_code_sticky_behavior()
{
    CSML_INFO(1, logger) << "Starting T9: ERR_CODE Sticky Behavior Test";

    bool test_passed = true;
    uint32_t err_code_val;

    // Step 1: Verify initial state
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        test_passed = false;
    }

    // Step 2: Trigger RESEED_CMD overflow
    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: SFIFO_RESCMD_ERR should be set";
        test_passed = false;
    }
    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: FIFO_WRITE_ERR should be set";
        test_passed = false;
    }

    // Step 3: Attempt to clear by writing 0x0
    CSML_INFO(1, logger) << "Testing write 0x0 to ERR_CODE (should be ignored)...";
    register_write_32(ERR_CODE_OFFSET, 0x00000000);
    wait(1, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: ERR_CODE should remain set after write 0x0";
        test_passed = false;
    }

    // Step 4: Attempt to clear by writing 0xFFFFFFFF
    CSML_INFO(1, logger) << "Testing write 0xFFFFFFFF to ERR_CODE (should be ignored)...";
    register_write_32(ERR_CODE_OFFSET, 0xFFFFFFFF);
    wait(1, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: ERR_CODE should remain set after write 0xFFFFFFFF";
        test_passed = false;
    }

    // Step 5: Clear INTR_STATE and verify ERR_CODE unaffected
    CSML_INFO(1, logger) << "Clearing INTR_STATE, ERR_CODE should remain...";
    register_write_32(INTR_STATE_OFFSET, 0x00000002);
    wait(1, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: ERR_CODE unaffected by INTR_STATE clear";
        test_passed = false;
    }

    // Step 6: Apply reset and verify ERR_CODE cleared
    CSML_INFO(1, logger) << "Applying reset to clear ERR_CODE...";
    apply_reset(100.0);
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        CSML_ERROR(1, logger) << "T9 FAIL: ERR_CODE should be cleared by reset";
        test_passed = false;
    }

    // Step 7: Trigger GENERATE_CMD overflow
    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: SFIFO_GENCMD_ERR should be set";
        test_passed = false;
    }

    // Step 8: Repeat sticky tests for GENERATE_CMD error
    register_write_32(ERR_CODE_OFFSET, 0x00000000);
    wait(1, SC_NS);

    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T9 FAIL: SFIFO_GENCMD_ERR should remain sticky";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T9 PASS: ERR_CODE sticky behavior verified (read-only, reset clears)";
    }

    return test_passed;
}

// =============================================================================
// Test Case 10: FIFO Clearing on Reset
// =============================================================================

bool test_edn_func_007::test_fifo_reset_clearing()
{
    CSML_INFO(1, logger) << "Starting T10: FIFO Clearing on Reset Test";

    bool test_passed = true;
    uint32_t err_code_val, intr_state_val, main_sm_state_val;
    bool fatal_alert, fatal_intr;

    // Step 1: Trigger RESEED_CMD overflow
    CSML_INFO(1, logger) << "Triggering RESEED_CMD overflow...";
    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH + 1, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    // Verify error state
    if (!verify_error_state(1, 0, 1, true, false, STATE_ERROR)) {
        CSML_ERROR(1, logger) << "T10 FAIL: Initial RESEED_CMD overflow state incorrect";
        test_passed = false;
    }

    // Step 2: Trigger GENERATE_CMD overflow (compound error)
    CSML_INFO(1, logger) << "Triggering GENERATE_CMD overflow (compound error)...";
    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH + 1, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1) ||
        !verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, 1) ||
        !verify_err_code_bit(FIFO_WRITE_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T10 FAIL: Compound error state incorrect, ERR_CODE=0x"
                          << std::hex << err_code_val;
        test_passed = false;
    }

    // Step 3: Apply system reset
    CSML_INFO(1, logger) << "Applying system reset (100ns)...";
    apply_reset(100.0);
    wait(10, SC_NS);

    // Step 4: Verify all error state cleared
    CSML_INFO(1, logger) << "Verifying complete error state clearing...";

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_register_value("ERR_CODE", 0x00000000, err_code_val)) {
        CSML_ERROR(1, logger) << "T10 FAIL: ERR_CODE not cleared by reset";
        test_passed = false;
    }

    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if (!verify_register_value("INTR_STATE", 0x00000000, intr_state_val)) {
        CSML_ERROR(1, logger) << "T10 FAIL: INTR_STATE not cleared by reset";
        test_passed = false;
    }

    fatal_alert = alert_fatal_alert.read();
    if (fatal_alert) {
        CSML_ERROR(1, logger) << "T10 FAIL: alert_fatal_alert not deasserted by reset";
        test_passed = false;
    }

    fatal_intr = intr_edn_fatal_err.read();
    if (fatal_intr) {
        CSML_ERROR(1, logger) << "T10 FAIL: intr_edn_fatal_err not deasserted by reset";
        test_passed = false;
    }

    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (!verify_register_value("MAIN_SM_STATE", STATE_IDLE, main_sm_state_val)) {
        CSML_ERROR(1, logger) << "T10 FAIL: MAIN_SM_STATE not restored to Idle";
        test_passed = false;
    }

    // Step 5: Verify FIFO functionality restored
    CSML_INFO(1, logger) << "Verifying FIFO functionality restored (13 words should succeed)...";

    write_fifo_words(RESEED_CMD_OFFSET, FIFO_DEPTH, 0x00000001, "RESEED_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (err_code_val != 0x00000000) {
        CSML_ERROR(1, logger) << "T10 FAIL: RESEED_CMD FIFO not restored (error after 13 words)";
        test_passed = false;
    }

    write_fifo_words(GENERATE_CMD_OFFSET, FIFO_DEPTH, 0x10000001, "GENERATE_CMD");
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (err_code_val != 0x00000000) {
        CSML_ERROR(1, logger) << "T10 FAIL: GENERATE_CMD FIFO not restored (error after 13 words)";
        test_passed = false;
    }

    // Step 6: Verify overflow detection still works
    CSML_INFO(1, logger) << "Verifying overflow detection still functional (14th word should fail)...";

    register_write_32(RESEED_CMD_OFFSET, 0x0000000E); // 14th word
    wait(10, SC_NS);

    register_read_32(ERR_CODE_OFFSET, err_code_val);
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, 1)) {
        CSML_ERROR(1, logger) << "T10 FAIL: Overflow detection not functional after reset";
        test_passed = false;
    }

    if (test_passed) {
        CSML_INFO(1, logger) << "T10 PASS: Reset clears error state and restores FIFO functionality";
    }

    return test_passed;
}

// =============================================================================
// Helper Functions
// =============================================================================

void test_edn_func_007::write_fifo_words(uint32_t fifo_offset,
                                        unsigned int num_words,
                                        uint32_t base_value,
                                        const std::string& fifo_name)
{
    for (unsigned int i = 0; i < num_words; ++i) {
        uint32_t word_value = base_value + i;
        register_write_32(fifo_offset, word_value);
        wait(1, SC_NS); // Small delay between writes

        if ((i + 1) % 5 == 0) {
            CSML_DEBUG(1, logger) << fifo_name << ": Wrote word " << (i + 1)
                              << " of " << num_words << " (value=0x"
                              << std::hex << word_value << ")";
        }
    }
    CSML_INFO(1, logger) << fifo_name << ": Wrote " << num_words << " words total";
}

bool test_edn_func_007::verify_err_code_bit(unsigned int bit_position, unsigned int expected_value)
{
    uint32_t err_code_val;
    register_read_32(ERR_CODE_OFFSET, err_code_val);
    uint32_t bit_value = (err_code_val >> bit_position) & 0x1;

    return verify_bit_value("ERR_CODE", bit_position, expected_value, err_code_val);
}

bool test_edn_func_007::verify_error_state(unsigned int expected_rescmd_err,
                                          unsigned int expected_gencmd_err,
                                          unsigned int expected_fifo_write_err,
                                          bool expected_fatal_alert,
                                          bool expected_fatal_intr,
                                          uint32_t expected_state)
{
    bool all_match = true;
    uint32_t err_code_val, main_sm_state_val;
    bool fatal_alert, fatal_intr;

    // Check ERR_CODE bits
    if (!verify_err_code_bit(SFIFO_RESCMD_ERR_BIT, expected_rescmd_err)) {
        all_match = false;
    }
    if (!verify_err_code_bit(SFIFO_GENCMD_ERR_BIT, expected_gencmd_err)) {
        all_match = false;
    }
    if (!verify_err_code_bit(FIFO_WRITE_ERR_BIT, expected_fifo_write_err)) {
        all_match = false;
    }

    // Check alert signal
    fatal_alert = alert_fatal_alert.read();
    if (fatal_alert != expected_fatal_alert) {
        CSML_ERROR(1, logger) << "alert_fatal_alert mismatch: expected=" << expected_fatal_alert
                          << ", actual=" << fatal_alert;
        all_match = false;
    }

    // Check interrupt signal (if checking)
    if (expected_fatal_intr) {
        fatal_intr = intr_edn_fatal_err.read();
        if (fatal_intr != expected_fatal_intr) {
            CSML_ERROR(1, logger) << "intr_edn_fatal_err mismatch: expected=" << expected_fatal_intr
                              << ", actual=" << fatal_intr;
            all_match = false;
        }
    }

    // Check state machine
    register_read_32(MAIN_SM_STATE_OFFSET, main_sm_state_val);
    if (main_sm_state_val != expected_state) {
        CSML_ERROR(1, logger) << "MAIN_SM_STATE mismatch: expected=0x" << std::hex << expected_state
                          << ", actual=0x" << main_sm_state_val;
        all_match = false;
    }

    return all_match;
}

bool test_edn_func_007::verify_register_value(const std::string& reg_name,
                                              uint32_t expected,
                                              uint32_t actual)
{
    if (expected != actual) {
        CSML_ERROR(1, logger) << reg_name << " mismatch: expected=0x" << std::hex << expected
                          << ", actual=0x" << actual;
        return false;
    }
    return true;
}

bool test_edn_func_007::verify_bit_value(const std::string& reg_name,
                                        unsigned int bit_position,
                                        unsigned int expected_value,
                                        uint32_t actual_reg)
{
    uint32_t bit_value = (actual_reg >> bit_position) & 0x1;

    if (bit_value != expected_value) {
        CSML_ERROR(1, logger) << reg_name << "[" << bit_position << "] mismatch: expected="
                          << expected_value << ", actual=" << bit_value
                          << " (register=0x" << std::hex << actual_reg << ")";
        return false;
    }
    return true;
}

void test_edn_func_007::report_test_result(const std::string& test_name,
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
}
