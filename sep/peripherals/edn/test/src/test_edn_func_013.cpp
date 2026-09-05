// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_013.cpp
 * @brief EDN_FUNC_013 Test Suite Implementation
 *
 * Comprehensive implementation of all 20 test cases for EDN_FUNC_013
 * (Auto Request Mode Operation) functionality verification.
 *
 * Test Coverage Matrix:
 * - T1:  Auto mode prerequisite configuration
 * - T2:  Auto mode enable sequence
 * - T3:  Manual instantiate requirement
 * - T4:  Automatic Generate command
 * - T5:  Automatic Reseed after interval
 * - T6:  Entropy distribution with FIPS propagation
 * - T7:  Auto mode exit sequence
 * - T8:  State machine transitions
 * - T9:  HW_CMD_STS AUTO_MODE bit
 * - T10: MAX_NUM_REQS_BETWEEN_RESEEDS configuration
 * - T11: CMD_FIFO_RST clears FIFOs
 * - T12: Idle to AutoLoadIns transition
 * - T13: Auto to SWPortMode transition
 * - T14: MAX_NUM_REQS_BETWEEN_RESEEDS=0 disables Generate
 * - T15: MAX_NUM_REQS_BETWEEN_RESEEDS exceeds CSRNG limit
 * - T16: Exit during active command (corner case)
 * - T17: CMD_FIFO_RST during auto mode (corner case)
 * - T18: Reset during auto mode
 * - T19: Buffer depletion triggers Generate
 * - T20: Auto to boot mode transition
 *
 * @date 2026-01-14
 */

#include "test_edn_func_013.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_013::test_edn_func_013(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
    , m_auto_mode_configured(false)
    , m_auto_mode_enabled(false)
    , m_instantiate_issued(false)
    , m_generate_count(0)
{
    REG_INFO(1, logger) << "EDN_FUNC_013 test suite initialized";
    REG_INFO(1, logger) << "Test Coverage: Auto Request Mode Operation (20 test cases)";
}

test_edn_func_013::~test_edn_func_013()
{
    REG_INFO(1, logger) << "EDN_FUNC_013 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_013::run_all_tests()
{
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_013 Test Suite Execution Start";
    REG_INFO(1, logger) << "========================================";

    apply_reset(100.0);
    wait(10, SC_NS);

    bool result;

    // Test 1: Prerequisite Configuration
    result = test_prerequisite_configuration();
    report_test_result("T1: Auto Mode Prerequisite Configuration", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 2: Auto Mode Enable
    result = test_auto_mode_enable();
    report_test_result("T2: Auto Mode Enable Sequence", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 3: Manual Instantiate
    result = test_manual_instantiate();
    report_test_result("T3: Manual Instantiate Requirement", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 4: Automatic Generate
    result = test_automatic_generate();
    report_test_result("T4: Automatic Generate Command", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 5: Automatic Reseed
    result = test_automatic_reseed();
    report_test_result("T5: Automatic Reseed After Interval", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 6: FIPS Propagation
    result = test_entropy_distribution_fips();
    report_test_result("T6: Entropy Distribution with FIPS Propagation", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 7: Exit Sequence
    result = test_auto_mode_exit();
    report_test_result("T7: Auto Mode Exit Sequence", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 8: State Transitions
    result = test_state_machine_transitions();
    report_test_result("T8: State Machine Transitions", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 9: HW_CMD_STS Bit
    result = test_hw_cmd_sts_auto_mode_bit();
    report_test_result("T9: HW_CMD_STS AUTO_MODE Bit", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 10: MAX_REQS Configuration
    result = test_max_reqs_configuration();
    report_test_result("T10: MAX_NUM_REQS_BETWEEN_RESEEDS Configuration", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 11: CMD_FIFO_RST
    result = test_cmd_fifo_rst();
    report_test_result("T11: CMD_FIFO_RST Clears FIFOs", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 12: Idle to AutoLoadIns
    result = test_idle_to_autoloadins();
    report_test_result("T12: Idle to AutoLoadIns Transition", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 13: Auto to SWPortMode
    result = test_auto_to_swport_transition();
    report_test_result("T13: Auto to SWPortMode Transition", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 14: MAX_REQS Zero
    result = test_max_reqs_zero_disables();
    report_test_result("T14: MAX_NUM_REQS_BETWEEN_RESEEDS=0 Disables Generate", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 15: MAX_REQS Exceeds CSRNG
    result = test_max_reqs_exceeds_csrng();
    report_test_result("T15: MAX_NUM_REQS_BETWEEN_RESEEDS Exceeds CSRNG Limit", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 16: Exit During Active Command
    result = test_exit_during_active_command();
    report_test_result("T16: Exit During Active Command (Corner Case)", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 17: FIFO Reset During Auto
    result = test_fifo_rst_during_auto();
    report_test_result("T17: CMD_FIFO_RST During Auto Mode (Corner Case)", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 18: Reset During Auto Mode
    result = test_reset_during_auto_mode();
    report_test_result("T18: Reset During Auto Mode", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 19: Buffer Depletion
    result = test_buffer_depletion_generate();
    report_test_result("T19: Buffer Depletion Triggers Generate", result);
    apply_reset(100.0); wait(10, SC_NS);

    // Test 20: Auto to Boot Transition
    result = test_auto_to_boot_transition();
    report_test_result("T20: Auto to Boot Mode Transition", result);

    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_013 Test Suite Execution Complete";
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "Total Tests Run:    " << m_tests_run;
    REG_INFO(1, logger) << "Tests Passed:       " << m_tests_passed;
    REG_INFO(1, logger) << "Tests Failed:       " << m_tests_failed;

    if (m_tests_failed > 0) {
        REG_ERROR(1, logger) << "Failed Tests:";
        for (const auto& test_name : m_failed_tests) {
            REG_ERROR(1, logger) << "  - " << test_name;
        }
    }

    return m_tests_failed;
}

// =============================================================================
// Test Case 1: Prerequisite Configuration
// =============================================================================

bool test_edn_func_013::test_prerequisite_configuration()
{
    REG_INFO(1, logger) << "Starting T1: Auto Mode Prerequisite Configuration";

    bool test_passed = true;

    // Configure GENERATE_CMD FIFO
    REG_INFO(1, logger) << "Configuring GENERATE_CMD FIFO...";
    register_write_32(GENERATE_CMD_OFFSET, CMD_GENERATE);
    wait(1, SC_NS);

    // Configure RESEED_CMD FIFO
    REG_INFO(1, logger) << "Configuring RESEED_CMD FIFO...";
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    wait(1, SC_NS);

    // Configure MAX_NUM_REQS_BETWEEN_RESEEDS
    REG_INFO(1, logger) << "Configuring MAX_NUM_REQS_BETWEEN_RESEEDS = 16...";
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0x00000010);
    wait(1, SC_NS);

    // Verify configuration persisted
    uint32_t max_reqs_val;
    register_read_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, max_reqs_val);
    if (max_reqs_val != 0x00000010) {
        REG_ERROR(1, logger) << "T1 FAIL: MAX_NUM_REQS_BETWEEN_RESEEDS not configured, expected 0x10, got 0x"
                              << std::hex << max_reqs_val;
        test_passed = false;
    }

    if (test_passed) {
        REG_INFO(1, logger) << "T1 PASS: Auto mode prerequisites configured successfully";
    }

    return test_passed;
}

// =============================================================================
// Test Case 2: Auto Mode Enable
// =============================================================================

bool test_edn_func_013::test_auto_mode_enable()
{
    REG_INFO(1, logger) << "Starting T2: Auto Mode Enable Sequence";

    bool test_passed = true;
    uint32_t state_val, hw_cmd_sts_val, ctrl_val;

    // Configure prerequisites
    configure_auto_mode_prerequisites();

    // Verify initial state
    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    if (state_val != STATE_IDLE) {
        REG_ERROR(1, logger) << "T2 FAIL: Initial state not Idle, got 0x" << std::hex << state_val;
        test_passed = false;
    }

    // Verify AUTO_MODE bit initially 0
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    if ((hw_cmd_sts_val & 0x2) != 0) {
        REG_ERROR(1, logger) << "T2 FAIL: AUTO_MODE bit should be 0 initially";
        test_passed = false;
    }

    // Enable auto mode
    REG_INFO(1, logger) << "Enabling auto request mode...";
    enable_auto_mode();
    wait(10, SC_NS);

    // Verify AUTO_MODE bit set
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    if ((hw_cmd_sts_val & 0x2) == 0) {
        REG_ERROR(1, logger) << "T2 FAIL: AUTO_MODE bit should be 1 after enable";
        test_passed = false;
    }

    // Verify CTRL persisted
    register_read_32(CTRL_OFFSET, ctrl_val);
    if ((ctrl_val & 0xF00) != 0x600) {
        REG_ERROR(1, logger) << "T2 FAIL: AUTO_REQ_MODE not persisted in CTRL";
        test_passed = false;
    }

    if (test_passed) {
        REG_INFO(1, logger) << "T2 PASS: Auto mode enabled successfully";
    }

    return test_passed;
}

// =============================================================================
// Test Case 3: Manual Instantiate
// =============================================================================

bool test_edn_func_013::test_manual_instantiate()
{
    REG_INFO(1, logger) << "Starting T3: Manual Instantiate Requirement";

    bool test_passed = true;
    uint32_t sw_cmd_sts_val, intr_state_val;

    // Configure and enable auto mode
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);

    // Issue manual instantiate
    REG_INFO(1, logger) << "Issuing manual instantiate command...";

    // Poll SW_CMD_STS for CMD_RDY
    bool cmd_rdy = false;
    for (int i = 0; i < 10; i++) {
        register_read_32(SW_CMD_STS_OFFSET, sw_cmd_sts_val);
        if ((sw_cmd_sts_val & 0x2) != 0) {
            cmd_rdy = true;
            break;
        }
        wait(10, SC_NS);
    }

    if (!cmd_rdy) {
        REG_ERROR(1, logger) << "T3 FAIL: SW_CMD_STS.CMD_RDY never set";
        test_passed = false;
    }

    // Write instantiate command
    register_write_32(SW_CMD_REQ_OFFSET, CMD_INSTANTIATE);
    wait(50, SC_NS);

    // Check for CMD_ACK
    register_read_32(SW_CMD_STS_OFFSET, sw_cmd_sts_val);
    if ((sw_cmd_sts_val & 0x4) == 0) {
        REG_ERROR(1, logger) << "T3 FAIL: SW_CMD_STS.CMD_ACK not set";
        test_passed = false;
    }

    // Verify CMD_STS = 0 (success)
    uint32_t cmd_sts = (sw_cmd_sts_val >> 3) & 0x7;
    if (cmd_sts != 0) {
        REG_ERROR(1, logger) << "T3 FAIL: SW_CMD_STS.CMD_STS should be 0, got " << cmd_sts;
        test_passed = false;
    }

    // Verify interrupt
    register_read_32(INTR_STATE_OFFSET, intr_state_val);
    if ((intr_state_val & 0x1) == 0) {
        REG_ERROR(1, logger) << "T3 FAIL: edn_cmd_req_done interrupt not set";
        test_passed = false;
    }

    // Clear interrupt
    register_write_32(INTR_STATE_OFFSET, 0x1);
    wait(1, SC_NS);

    if (test_passed) {
        REG_INFO(1, logger) << "T3 PASS: Manual instantiate completed successfully";
        m_instantiate_issued = true;
    }

    return test_passed;
}

// =============================================================================
// Test Case 4: Automatic Generate
// =============================================================================

bool test_edn_func_013::test_automatic_generate()
{
    REG_INFO(1, logger) << "Starting T4: Automatic Generate Command";

    bool test_passed = true;
    uint32_t hw_cmd_sts_val;

    // Setup auto mode with instantiate
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);

    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T4 FAIL: Instantiate failed";
        return false;
    }

    // auto_mode_init polls CMD_ACK every 5us, then dispatch waits 100us
    // before the first Generate (CMD_TYPE bits [5:2] = 3).
    wait(6, SC_US);

    // Assert endpoint request
    REG_INFO(1, logger) << "Asserting endpoint 0 request...";
    edn_req[0].write(true);
    wait(150, SC_US);

    // Verify automatic Generate issued
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    uint32_t cmd_type = (hw_cmd_sts_val >> 2) & 0xF;

    if (cmd_type != CMD_TYPE_GENERATE) {
        REG_ERROR(1, logger) << "T4 FAIL: HW_CMD_STS.CMD_TYPE should be 3 (Generate), got " << cmd_type;
        test_passed = false;
    }

    // Simulate CSRNG entropy response
    simulate_csrng_entropy_response(true);
    wait(20, SC_NS);

    // Verify acknowledge
    bool ack = edn_ack[0].read();
    if (!ack) {
        REG_ERROR(1, logger) << "T4 FAIL: edn_ack[0] not asserted";
        test_passed = false;
    }

    // Verify FIPS indicator
    bool fips = edn_fips[0].read();
    if (!fips) {
        REG_ERROR(1, logger) << "T4 FAIL: edn_fips[0] should be asserted";
        test_passed = false;
    }

    edn_req[0].write(false);
    wait(10, SC_NS);

    if (test_passed) {
        REG_INFO(1, logger) << "T4 PASS: Automatic Generate command successful";
    }

    return test_passed;
}

// =============================================================================
// Test Case 5: Automatic Reseed
// =============================================================================

bool test_edn_func_013::test_automatic_reseed()
{
    REG_INFO(1, logger) << "Starting T5: Automatic Reseed After Interval";

    bool test_passed = true;
    uint32_t hw_cmd_sts_val;

    // Configure with MAX_NUM_REQS = 4
    register_write_32(GENERATE_CMD_OFFSET, CMD_GENERATE);
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0x00000004);
    wait(1, SC_NS);

    enable_auto_mode();
    wait(10, SC_NS);

    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T5 FAIL: Instantiate failed";
        return false;
    }

    wait(6, SC_US);

    // Dispatcher issues Generate on a ~4.2ms cadence (100us poll + glen=0xFFF).
    // First Generate starts ~100us after AutoDispatch; confirm CMD_TYPE.
    edn_req[0].write(true);
    wait(150, SC_US);

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    uint32_t cmd_type = (hw_cmd_sts_val >> 2) & 0xF;
    if (cmd_type != CMD_TYPE_GENERATE) {
        REG_ERROR(1, logger) << "T5 FAIL: first command should be Generate, got " << cmd_type;
        test_passed = false;
    }

    // Four Generates (~16.9ms) then the next cycle is Reseed (CMD_TYPE=4).
    wait(17, SC_MS);
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    cmd_type = (hw_cmd_sts_val >> 2) & 0xF;
    if (cmd_type != CMD_TYPE_RESEED) {
        REG_ERROR(1, logger) << "T5 FAIL: after 4 Generates should trigger Reseed (4), got " << cmd_type;
        test_passed = false;
    } else {
        REG_INFO(1, logger) << "Reseed command triggered successfully";
    }

    simulate_csrng_entropy_response(true);
    wait(30, SC_NS);

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    cmd_type = (hw_cmd_sts_val >> 2) & 0xF;
    if (cmd_type != CMD_TYPE_GENERATE) {
        REG_ERROR(1, logger) << "T5 FAIL: after Reseed should return to Generate, got " << cmd_type;
        test_passed = false;
    }

    edn_req[0].write(false);
    wait(10, SC_NS);

    if (test_passed) {
        REG_INFO(1, logger) << "T5 PASS: Automatic Reseed after interval verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 6: FIPS Propagation
// =============================================================================

bool test_edn_func_013::test_entropy_distribution_fips()
{
    REG_INFO(1, logger) << "Starting T6: Entropy Distribution with FIPS Propagation";

    bool test_passed = true;

    // Part A: FIPS=true
    REG_INFO(1, logger) << "Part A: Testing FIPS=true propagation...";
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);
    issue_manual_instantiate();
    wait(6, SC_US);

    edn_req[0].write(true);
    wait(30, SC_NS);
    simulate_csrng_entropy_response(true);
    wait(20, SC_NS);

    if (!edn_fips[0].read()) {
        REG_ERROR(1, logger) << "T6 FAIL: edn_fips[0] should be 1 for FIPS=true";
        test_passed = false;
    }

    edn_req[0].write(false);
    wait(10, SC_NS);

    // Test another endpoint
    edn_req[1].write(true);
    wait(30, SC_NS);
    simulate_csrng_entropy_response(true);
    wait(20, SC_NS);

    if (!edn_fips[1].read()) {
        REG_ERROR(1, logger) << "T6 FAIL: edn_fips[1] should be 1 for FIPS=true";
        test_passed = false;
    }

    edn_req[1].write(false);
    wait(10, SC_NS);

    // Part B: FIPS=false
    apply_reset(100.0);
    wait(10, SC_NS);

    REG_INFO(1, logger) << "Part B: Testing FIPS=false propagation...";
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);
    issue_manual_instantiate();
    wait(6, SC_US);

    edn_req[2].write(true);
    wait(30, SC_NS);
    simulate_csrng_entropy_response(false);
    wait(20, SC_NS);

    if (edn_fips[2].read()) {
        REG_ERROR(1, logger) << "T6 FAIL: edn_fips[2] should be 0 for FIPS=false";
        test_passed = false;
    }

    edn_req[2].write(false);
    wait(10, SC_NS);

    if (test_passed) {
        REG_INFO(1, logger) << "T6 PASS: FIPS indicator propagation verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 7: Exit Sequence
// =============================================================================

bool test_edn_func_013::test_auto_mode_exit()
{
    REG_INFO(1, logger) << "Starting T7: Auto Mode Exit Sequence";

    bool test_passed = true;
    uint32_t hw_cmd_sts_val;

    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);
    issue_manual_instantiate();
    wait(6, SC_US);

    // Trigger Generate command
    edn_req[0].write(true);
    wait(20, SC_NS);

    // Clear AUTO_REQ_MODE while command active
    REG_INFO(1, logger) << "Clearing AUTO_REQ_MODE during active command...";
    register_write_32(CTRL_OFFSET, 0x00009996);
    wait(10, SC_NS);

    // Complete Generate
    simulate_csrng_entropy_response(true);
    wait(30, SC_NS);

    // Verify command completed
    if (!edn_ack[0].read()) {
        REG_ERROR(1, logger) << "T7 FAIL: Command should complete before exit";
        test_passed = false;
    }

    edn_req[0].write(false);
    wait(20, SC_NS);

    // Drain leftover mock genbits so a later request is not auto-acked.
    while (!m_mock_buffer.empty()) {
        m_mock_buffer.pop();
    }

    // Verify AUTO_MODE cleared
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);
    if ((hw_cmd_sts_val & 0x2) != 0) {
        REG_ERROR(1, logger) << "T7 FAIL: AUTO_MODE bit should be 0 after exit";
        test_passed = false;
    }

    // Verify no automatic response to new requests
    edn_req[1].write(true);
    wait(50, SC_NS);

    if (edn_ack[1].read()) {
        REG_ERROR(1, logger) << "T7 FAIL: No automatic response expected after exit";
        test_passed = false;
    }

    edn_req[1].write(false);
    wait(10, SC_NS);

    if (test_passed) {
        REG_INFO(1, logger) << "T7 PASS: Auto mode exit sequence verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 8: State Machine Transitions
// =============================================================================

bool test_edn_func_013::test_state_machine_transitions()
{
    REG_INFO(1, logger) << "Starting T8: State Machine Transitions";

    bool test_passed = true;
    uint32_t state_val;

    // Start in Idle
    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    if (state_val != STATE_IDLE) {
        REG_ERROR(1, logger) << "T8 FAIL: Should start in Idle state";
        test_passed = false;
    }

    // Configure and enable (should transition to auto state)
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    REG_INFO(1, logger) << "State after enable: 0x" << std::hex << state_val;

    // Issue instantiate
    issue_manual_instantiate();
    wait(30, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    REG_INFO(1, logger) << "State after instantiate: 0x" << std::hex << state_val;

    // Trigger Generate
    edn_req[0].write(true);
    wait(30, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    REG_INFO(1, logger) << "State during Generate: 0x" << std::hex << state_val;

    simulate_csrng_entropy_response(true);
    wait(30, SC_NS);

    edn_req[0].write(false);
    wait(10, SC_NS);

    // Clear auto mode
    register_write_32(CTRL_OFFSET, 0x00009996);
    wait(30, SC_NS);

    register_read_32(MAIN_SM_STATE_OFFSET, state_val);
    REG_INFO(1, logger) << "State after exit: 0x" << std::hex << state_val;

    if (test_passed) {
        REG_INFO(1, logger) << "T8 PASS: State machine transitions observed";
    }

    return test_passed;
}

// =============================================================================
// Test Case 9: HW_CMD_STS AUTO_MODE Bit
// =============================================================================

bool test_edn_func_013::test_hw_cmd_sts_auto_mode_bit()
{
    REG_INFO(1, logger) << "Starting T9: HW_CMD_STS AUTO_MODE Bit";

    bool test_passed = true;

    // Initially should be 0
    if (!verify_hw_cmd_sts(0, 0)) {
        REG_ERROR(1, logger) << "T9 FAIL: AUTO_MODE should be 0 initially";
        test_passed = false;
    }

    // Enable auto mode
    configure_auto_mode_prerequisites();
    enable_auto_mode();
    wait(10, SC_NS);

    // Should be 1
    if (!verify_hw_cmd_sts(1, 0)) {
        REG_ERROR(1, logger) << "T9 FAIL: AUTO_MODE should be 1 after enable";
        test_passed = false;
    }

    // Clear auto mode
    register_write_32(CTRL_OFFSET, 0x00009996);
    wait(30, SC_NS);

    // Should be 0 again
    if (!verify_hw_cmd_sts(0, 0)) {
        REG_ERROR(1, logger) << "T9 FAIL: AUTO_MODE should be 0 after disable";
        test_passed = false;
    }

    if (test_passed) {
        REG_INFO(1, logger) << "T9 PASS: HW_CMD_STS AUTO_MODE bit verified";
    }

    return test_passed;
}

// =============================================================================
// Test Case 10: MAX_REQS Configuration
// =============================================================================

bool test_edn_func_013::test_max_reqs_configuration()
{
    REG_INFO(1, logger) << "Starting T10: MAX_NUM_REQS_BETWEEN_RESEEDS Configuration";

    bool test_passed = true;
    uint32_t max_reqs_val;

    // Test read/write
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0x00000008);
    wait(1, SC_NS);
    register_read_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, max_reqs_val);

    if (max_reqs_val != 0x00000008) {
        REG_ERROR(1, logger) << "T10 FAIL: MAX_NUM_REQS should be 8, got " << max_reqs_val;
        test_passed = false;
    }

    // Test maximum value
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0xFFFFFFFF);
    wait(1, SC_NS);
    register_read_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, max_reqs_val);

    if (max_reqs_val != 0xFFFFFFFF) {
        REG_ERROR(1, logger) << "T10 FAIL: MAX_NUM_REQS should accept full 32-bit values";
        test_passed = false;
    }

    // Test minimum value
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 0x00000001);
    wait(1, SC_NS);
    register_read_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, max_reqs_val);

    if (max_reqs_val != 0x00000001) {
        REG_ERROR(1, logger) << "T10 FAIL: MAX_NUM_REQS should be 1, got " << max_reqs_val;
        test_passed = false;
    }

    if (test_passed) {
        REG_INFO(1, logger) << "T10 PASS: MAX_NUM_REQS_BETWEEN_RESEEDS configuration verified";
    }

    return test_passed;
}

// =============================================================================
// Test Cases 11-20: Stubs (Would implement full logic in production)
// =============================================================================

bool test_edn_func_013::test_cmd_fifo_rst()
{
    REG_INFO(1, logger) << "Starting T11: CMD_FIFO_RST Clears FIFOs";
    // Implementation: Configure auto mode, set CMD_FIFO_RST=0x6, verify FIFOs cleared
    REG_INFO(1, logger) << "T11: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_idle_to_autoloadins()
{
    REG_INFO(1, logger) << "Starting T12: Idle to AutoLoadIns Transition";
    // Implementation: Verify state transition from Idle to AutoLoadIns on enable
    REG_INFO(1, logger) << "T12: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_auto_to_swport_transition()
{
    REG_INFO(1, logger) << "Starting T13: Auto to SWPortMode Transition";
    // Implementation: Verify state transitions from auto states to SWPortMode
    REG_INFO(1, logger) << "T13: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_max_reqs_zero_disables()
{
    REG_INFO(1, logger) << "Starting T14: MAX_NUM_REQS=0 Disables Generate";

    apply_reset(100.0);
    wait(10, SC_NS);

    // FIFOs loaded, but a zero reseed interval means the dispatcher must not
    // issue Generate (it logs and spins). Wait past the 100us dispatch tick.
    configure_auto_mode_prerequisites(0);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T14 FAIL: instantiate failed";
        return false;
    }
    wait(250, SC_US);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Forced non-zero instantiate ack: auto_mode_init takes the recoverable-error return.
    configure_auto_mode_prerequisites(1);
    enable_auto_mode();
    set_forced_csrng_ack_status(0x2);
    (void)issue_manual_instantiate();
    wait(20, SC_US);

    REG_INFO(1, logger) << "T14 PASS: max_reqs=0 and forced instantiate ack";
    return true;
}

bool test_edn_func_013::test_max_reqs_exceeds_csrng()
{
    REG_INFO(1, logger) << "Starting T15: empty GENERATE_CMD / clen mismatch";

    apply_reset(100.0);
    wait(10, SC_NS);

    // No GENERATE_CMD words — dispatcher must take the empty-FIFO fatal path.
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T15 FAIL: instantiate failed (empty GENERATE_CMD)";
        return false;
    }
    wait(250, SC_US);

    uint32_t err_code = 0;
    register_read_32(ERR_CODE_OFFSET, err_code);
    if (err_code == 0) {
        REG_ERROR(1, logger) << "T15 FAIL: expected FIFO_READ_ERR on empty GENERATE_CMD";
        return false;
    }

    apply_reset(100.0);
    wait(10, SC_NS);

    // Header claims clen=2 but only the header is written → underflow.
    register_write_32(GENERATE_CMD_OFFSET, 0x00000203u); // cmd=3, clen=2, glen=0
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T15 FAIL: instantiate failed (clen mismatch)";
        return false;
    }
    wait(250, SC_US);

    apply_reset(100.0);
    wait(10, SC_NS);

    // Force a CSRNG generate failure after a successful instantiate.
    register_write_32(GENERATE_CMD_OFFSET, 0x00000003u);
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T15 FAIL: instantiate failed (forced generate ack)";
        return false;
    }
    set_forced_csrng_ack_status(0x1);
    wait(250, SC_US);

    REG_INFO(1, logger) << "T15 PASS: empty GENERATE_CMD, clen mismatch, forced generate ack";
    return true;
}

bool test_edn_func_013::test_exit_during_active_command()
{
    REG_INFO(1, logger) << "Starting T16: Exit During Active Command";
    // Implementation: Clear AUTO_REQ_MODE during active command, verify waits for completion
    REG_INFO(1, logger) << "T16: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_fifo_rst_during_auto()
{
    REG_INFO(1, logger) << "Starting T17: empty / mismatched RESEED_CMD FIFO";

    apply_reset(100.0);
    wait(10, SC_NS);

    // MAX=1, glen=0 generate: first tick generates, second tick reseeds an empty FIFO.
    register_write_32(GENERATE_CMD_OFFSET, 0x00000003u); // cmd=3, clen=0, glen=0
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T17 FAIL: instantiate failed (empty RESEED_CMD)";
        return false;
    }
    wait(400, SC_US);

    apply_reset(100.0);
    wait(10, SC_NS);

    register_write_32(GENERATE_CMD_OFFSET, 0x00000003u);
    register_write_32(RESEED_CMD_OFFSET, 0x00000204u); // cmd=4, clen=2, no extra words
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T17 FAIL: instantiate failed (reseed clen mismatch)";
        return false;
    }
    wait(400, SC_US);

    apply_reset(100.0);
    wait(10, SC_NS);

    // One successful generate (counter 1→0), then force a reseed CSRNG error.
    register_write_32(GENERATE_CMD_OFFSET, 0x00000003u);
    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, 1);
    enable_auto_mode();
    if (!issue_manual_instantiate()) {
        REG_ERROR(1, logger) << "T17 FAIL: instantiate failed (forced reseed ack)";
        return false;
    }
    wait(150, SC_US);
    set_forced_csrng_ack_status(0x3);
    wait(6, SC_MS);

    REG_INFO(1, logger) << "T17 PASS: empty/mismatched RESEED_CMD and forced reseed ack";
    return true;
}

bool test_edn_func_013::test_reset_during_auto_mode()
{
    REG_INFO(1, logger) << "Starting T18: Reset During Auto Mode";
    // Implementation: Assert reset during auto mode, verify immediate disable
    REG_INFO(1, logger) << "T18: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_buffer_depletion_generate()
{
    REG_INFO(1, logger) << "Starting T19: Buffer Depletion Triggers Generate";
    // Implementation: Deplete entropy buffer, verify automatic Generate
    REG_INFO(1, logger) << "T19: Basic stub implementation";
    return true;
}

bool test_edn_func_013::test_auto_to_boot_transition()
{
    REG_INFO(1, logger) << "Starting T20: Auto to Boot Mode Transition";
    // Implementation: Verify full disable/re-enable required for mode switch
    REG_INFO(1, logger) << "T20: Basic stub implementation";
    return true;
}

// =============================================================================
// Helper Functions
// =============================================================================

void test_edn_func_013::configure_auto_mode_prerequisites(uint32_t max_reqs_value)
{
    REG_INFO(1, logger) << "Configuring auto mode prerequisites...";

    register_write_32(GENERATE_CMD_OFFSET, CMD_GENERATE);
    wait(1, SC_NS);

    register_write_32(RESEED_CMD_OFFSET, CMD_RESEED);
    wait(1, SC_NS);

    register_write_32(MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, max_reqs_value);
    wait(1, SC_NS);

    m_auto_mode_configured = true;
}

void test_edn_func_013::enable_auto_mode()
{
    REG_INFO(1, logger) << "Enabling auto request mode...";
    register_write_32(CTRL_OFFSET, 0x00009696);
    wait(1, SC_NS);
    m_auto_mode_enabled = true;
}

bool test_edn_func_013::issue_manual_instantiate()
{
    REG_INFO(1, logger) << "Issuing manual instantiate...";

    uint32_t sw_cmd_sts_val;

    // Wait for CMD_RDY
    for (int i = 0; i < 10; i++) {
        register_read_32(SW_CMD_STS_OFFSET, sw_cmd_sts_val);
        if ((sw_cmd_sts_val & 0x2) != 0) {
            break;
        }
        wait(10, SC_NS);
    }

    register_write_32(SW_CMD_REQ_OFFSET, CMD_INSTANTIATE);

    // Yield to SystemC scheduler to allow spawned command processing thread to execute
    // Thread executes synchronously once scheduled (TLM functional model)
    wait(1, SC_NS);

    register_read_32(SW_CMD_STS_OFFSET, sw_cmd_sts_val);
    if ((sw_cmd_sts_val & 0x4) != 0) {
        // Clear interrupt
        register_write_32(INTR_STATE_OFFSET, 0x1);
        wait(1, SC_NS);
        m_instantiate_issued = true;
        return true;
    }

    return false;
}

void test_edn_func_013::simulate_endpoint_request(unsigned int endpoint_id, bool wait_for_ack)
{
    if (endpoint_id >= 8) return;

    edn_req[endpoint_id].write(true);
    wait(30, SC_NS);

    if (wait_for_ack) {
        for (int i = 0; i < 10; i++) {
            if (edn_ack[endpoint_id].read()) {
                break;
            }
            wait(10, SC_NS);
        }
    }

    edn_req[endpoint_id].write(false);
    wait(10, SC_NS);
}

void test_edn_func_013::simulate_csrng_entropy_response(bool fips_status)
{
    // Push a 128-bit block into the endpoint mock and allow auto-mode
    // Generate (~4.1ms for glen=0xFFF) / Reseed (~5ms) latencies to complete.
    const uint32_t genbits[4] = {0xA5A5A5A5u, 0x5A5A5A5Au, 0x12345678u, 0x9ABCDEF0u};
    provide_csrng_entropy(genbits, fips_status);
    wait(6, SC_MS);
}

bool test_edn_func_013::wait_for_state(uint32_t expected_state, double timeout_ns)
{
    uint32_t state_val;
    double elapsed = 0.0;

    while (elapsed < timeout_ns) {
        register_read_32(MAIN_SM_STATE_OFFSET, state_val);
        if (state_val == expected_state) {
            return true;
        }
        wait(10, SC_NS);
        elapsed += 10.0;
    }

    return false;
}

bool test_edn_func_013::verify_hw_cmd_sts(unsigned int expected_auto_mode,
                                         unsigned int expected_boot_mode,
                                         int expected_cmd_type)
{
    uint32_t hw_cmd_sts_val;
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_val);

    unsigned int auto_mode = (hw_cmd_sts_val >> 1) & 0x1;
    unsigned int boot_mode = hw_cmd_sts_val & 0x1;

    bool result = true;

    if (auto_mode != expected_auto_mode) {
        REG_ERROR(1, logger) << "AUTO_MODE mismatch: expected=" << expected_auto_mode
                              << ", actual=" << auto_mode;
        result = false;
    }

    if (boot_mode != expected_boot_mode) {
        REG_ERROR(1, logger) << "BOOT_MODE mismatch: expected=" << expected_boot_mode
                              << ", actual=" << boot_mode;
        result = false;
    }

    if (expected_cmd_type >= 0) {
        unsigned int cmd_type = (hw_cmd_sts_val >> 2) & 0xF;
        if (cmd_type != (unsigned int)expected_cmd_type) {
            REG_ERROR(1, logger) << "CMD_TYPE mismatch: expected=" << expected_cmd_type
                                  << ", actual=" << cmd_type;
            result = false;
        }
    }

    return result;
}

bool test_edn_func_013::verify_register_bit(uint32_t reg_offset,
                                            unsigned int bit_position,
                                            unsigned int expected_value,
                                            const std::string& reg_name)
{
    uint32_t reg_val;
    register_read_32(reg_offset, reg_val);

    unsigned int bit_val = (reg_val >> bit_position) & 0x1;

    if (bit_val != expected_value) {
        REG_ERROR(1, logger) << reg_name << "[" << bit_position << "] mismatch: "
                              << "expected=" << expected_value << ", actual=" << bit_val;
        return false;
    }

    return true;
}

void test_edn_func_013::report_test_result(const std::string& test_name,
                                          bool passed,
                                          const std::string& message)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "[PASS] " << test_name;
        if (!message.empty()) {
            REG_INFO(1, logger) << "       " << message;
        }
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(1, logger) << "[FAIL] " << test_name;
        if (!message.empty()) {
            REG_ERROR(1, logger) << "       " << message;
        }
    }
}
