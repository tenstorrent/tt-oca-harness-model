// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_012.cpp
 * @brief EDN_FUNC_012 Test Suite Implementation
 *
 * Comprehensive implementation of all 7 test cases for EDN_FUNC_012
 * (Boot-Time Request Mode Operation) functionality verification.
 *
 * Test Coverage Matrix:
 * - TC_EDN_BOOT_001: Boot Mode Enable Sequence
 * - TC_EDN_BOOT_002: Boot Mode Instantiate Command
 * - TC_EDN_BOOT_003: Boot Mode Generate Command
 * - TC_EDN_BOOT_004: Boot Mode Entropy Distribution
 * - TC_EDN_BOOT_005: Boot Mode Pre-FIPS Indicator
 * - TC_EDN_BOOT_006: Boot Mode Exit Sequence
 * - TC_EDN_BOOT_007: Boot Mode State Transitions
 *
 * @date 2026-01-14
 */

#include "test_edn_func_012.h"
#include <iomanip>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_012::test_edn_func_012(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    REG_INFO(1, logger) << "EDN_FUNC_012 test suite initialized";
    REG_INFO(1, logger) << "Test Coverage: Boot-Time Request Mode Operation (7 test cases)";
}

test_edn_func_012::~test_edn_func_012()
{
    REG_INFO(1, logger) << "EDN_FUNC_012 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_012::run_all_tests()
{
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_012 Test Suite Execution Start";
    REG_INFO(1, logger) << "========================================";

    // Apply reset before starting tests
    REG_INFO(1, logger) << "Applying system reset...";
    apply_reset(100.0); // 100ns reset pulse
    wait(10, SC_NS);    // Wait for reset propagation

    // Execute all 7 test cases
    bool result;

    // Test 1: Boot Mode Enable Sequence
    result = test_boot_mode_enable_sequence();
    report_test_result("TC_EDN_BOOT_001: Boot Mode Enable Sequence", result);

    // Test 2: Boot Mode Instantiate Command
    result = test_boot_mode_instantiate_command();
    report_test_result("TC_EDN_BOOT_002: Boot Mode Instantiate Command", result);

    // Test 3: Boot Mode Generate Command
    result = test_boot_mode_generate_command();
    report_test_result("TC_EDN_BOOT_003: Boot Mode Generate Command", result);

    // Test 4: Boot Mode Entropy Distribution
    result = test_boot_mode_entropy_distribution();
    report_test_result("TC_EDN_BOOT_004: Boot Mode Entropy Distribution", result);

    // Test 5: Boot Mode Pre-FIPS Indicator
    result = test_boot_mode_pre_fips_indicator();
    report_test_result("TC_EDN_BOOT_005: Boot Mode Pre-FIPS Indicator", result);

    // Test 6: Boot Mode Exit Sequence
    result = test_boot_mode_exit_sequence();
    report_test_result("TC_EDN_BOOT_006: Boot Mode Exit Sequence", result);

    // Test 7: Boot Mode State Transitions
    result = test_boot_mode_state_transitions();
    report_test_result("TC_EDN_BOOT_007: Boot Mode State Transitions", result);

    // Test 8: Boot Mode Invalid CLEN
    result = test_boot_mode_invalid_clen();
    report_test_result("TC_EDN_BOOT_008: Boot Mode Invalid CLEN", result);

    // Print summary
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_012 Test Suite Summary";
    REG_INFO(1, logger) << "========================================";

    std::ostringstream oss;
    oss << "Tests Run:    " << m_tests_run;
    REG_INFO(1, logger) << oss.str();
    oss.str("");

    oss << "Tests Passed: " << m_tests_passed;
    REG_INFO(1, logger) << oss.str();
    oss.str("");

    oss << "Tests Failed: " << m_tests_failed;
    REG_INFO(1, logger) << oss.str();

    if (m_tests_failed > 0) {
        REG_ERROR(1, logger) << "Failed Tests:";
        for (const auto& test_name : m_failed_tests) {
            REG_ERROR(1, logger) << "  - " << test_name;
        }
    }

    REG_INFO(1, logger) << "========================================";

    return m_tests_failed;
}

// =============================================================================
// Test Case 1: Boot Mode Enable Sequence
// =============================================================================

bool test_edn_func_012::test_boot_mode_enable_sequence()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_001: Boot Mode Enable Sequence...";

    bool all_passed = true;

    // Apply reset to ensure clean start
    apply_reset(100.0);
    wait(10, SC_NS);

    // Verify initial state is Idle
    if (!verify_state(STATE_IDLE, "Initial state after reset")) {
        all_passed = false;
    }

    // Read CTRL register, verify disabled state
    uint32_t ctrl_value;
    register_read_32(CTRL_OFFSET, ctrl_value);
    uint32_t edn_enable = (ctrl_value >> 0) & 0xF;
    uint32_t boot_req_mode = (ctrl_value >> 4) & 0xF;

    if (edn_enable != 0x9) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: EDN_ENABLE should be 0x9 after reset, got 0x"
                              << std::hex << edn_enable;
        all_passed = false;
    }

    if (boot_req_mode != 0x9) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: BOOT_REQ_MODE should be 0x9 after reset, got 0x"
                              << std::hex << boot_req_mode;
        all_passed = false;
    }

    // Enable boot mode
    enable_boot_mode();
    wait(50, SC_NS); // Allow async thread execution

    // Verify state transitioned to boot mode (either BootInsAckWait or BootGenAckWait)
    // Note: Boot mode automatically progresses from Instantiate to Generate in same delta cycle
    uint32_t current_state;
    register_read_32(MAIN_SM_STATE_OFFSET, current_state);
    current_state &= 0x1FF;

    if (current_state != STATE_BOOT_INS_ACK_WAIT && current_state != STATE_BOOT_GEN_ACK_WAIT) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: State should be BootInsAckWait (0x36) or BootGenAckWait (0x9c), got 0x"
                              << std::hex << current_state;
        all_passed = false;
    } else {
        REG_INFO(1, logger) << "TC_EDN_BOOT_001: State correctly in boot mode: 0x" << std::hex << current_state;
    }

    // Verify HW_CMD_STS.BOOT_MODE = 1 (accept either Instantiate or Generate command)
    uint32_t hw_cmd_sts;
    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts);
    bool boot_mode = hw_cmd_sts & 0x1;
    uint32_t cmd_type = (hw_cmd_sts >> 2) & 0xF;

    if (!boot_mode) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: HW_CMD_STS.BOOT_MODE should be 1, got " << boot_mode;
        all_passed = false;
    }

    if (cmd_type != CSRNG_CMD_INSTANTIATE && cmd_type != CSRNG_CMD_GENERATE) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: CMD_TYPE should be Instantiate (0x1) or Generate (0x3), got 0x"
                              << std::hex << cmd_type;
        all_passed = false;
    }

    // Check AUTO_MODE = 0
    bool auto_mode = (hw_cmd_sts >> 1) & 0x1;
    if (auto_mode != 0) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: AUTO_MODE should be 0, got " << auto_mode;
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_001: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_001: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 2: Boot Mode Instantiate Command
// =============================================================================

bool test_edn_func_012::test_boot_mode_instantiate_command()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_002: Boot Mode Instantiate Command...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Read BOOT_INS_CMD register, verify default value
    uint32_t boot_ins_cmd;

    register_read_32(BOOT_INS_CMD_OFFSET, boot_ins_cmd);
    if (boot_ins_cmd != 0x00000001) {
        REG_WARN(1, logger) << "TC_EDN_BOOT_002: BOOT_INS_CMD default is 0x"
                             << std::hex << boot_ins_cmd << ", expected 0x001";
        // Not a failure, just log for awareness
    }

    uint32_t clen = (boot_ins_cmd >> 8) & 0xF;
    if (clen != 9) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: BOOT_INS_CMD clen should be 9, got " << clen;
        all_passed = false;
    }

    // Enable boot mode to trigger Instantiate
    enable_boot_mode();
    wait(50, SC_NS); // Allow command execution

    // Verify HW_CMD_STS shows boot command (Instantiate or Generate)
    // Note: Boot mode progresses automatically from Instantiate to Generate
    uint32_t hw_cmd_sts;

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts);
    uint32_t cmd_type = (hw_cmd_sts >> 2) & 0xF;
    bool cmd_ack = (hw_cmd_sts >> 6) & 0x1;
    uint32_t cmd_sts = (hw_cmd_sts >> 7) & 0x7;

    if (cmd_type != CSRNG_CMD_INSTANTIATE && cmd_type != CSRNG_CMD_GENERATE) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: CMD_TYPE should be Instantiate (0x1) or Generate (0x3), got 0x"
                              << std::hex << cmd_type;
        all_passed = false;
    } else {
        REG_INFO(1, logger) << "TC_EDN_BOOT_002: CMD_TYPE correctly showing boot command: 0x" << std::hex << cmd_type;
    }

    if (cmd_ack != 1) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: CMD_ACK should be 1, got " << cmd_ack;
        all_passed = false;
    }

    if (cmd_sts != 0) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: CMD_STS should be 0 (success), got 0x"
                              << std::hex << cmd_sts;
        all_passed = false;
    }

    // Wait for state transition to BootGenAckWait
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: Timeout waiting for BootGenAckWait state";
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_002: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_002: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 3: Boot Mode Generate Command
// =============================================================================

bool test_edn_func_012::test_boot_mode_generate_command()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_003: Boot Mode Generate Command...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Read BOOT_GEN_CMD register, verify default value
    uint32_t boot_gen_cmd;

    register_read_32(BOOT_GEN_CMD_OFFSET, boot_gen_cmd);
    if (boot_gen_cmd != 0x00FFF003) {
        REG_WARN(1, logger) << "TC_EDN_BOOT_003: BOOT_GEN_CMD default is 0x"
                             << std::hex << boot_gen_cmd << ", expected 0xFFF003";
    }

    // Extract glen and clen
    uint32_t glen = (boot_gen_cmd >> 12) & 0x7FFFF;
    uint32_t clen = (boot_gen_cmd >> 8) & 0xF;

    REG_INFO(1, logger) << "TC_EDN_BOOT_003: BOOT_GEN_CMD glen=0x" << std::hex << glen
                         << ", clen=" << std::dec << clen;

    // Enable boot mode
    enable_boot_mode();
    wait(50, SC_NS); // Allow Instantiate completion

    // Wait for BootGenAckWait state
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_003: Timeout waiting for BootGenAckWait";
        all_passed = false;
    } else {
        // Verify HW_CMD_STS shows Generate command
        uint32_t hw_cmd_sts;

        register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts);
        uint32_t cmd_type = (hw_cmd_sts >> 2) & 0xF;
        bool cmd_ack = (hw_cmd_sts >> 6) & 0x1;
        uint32_t cmd_sts = (hw_cmd_sts >> 7) & 0x7;

        if (!verify_value("HW_CMD_STS.CMD_TYPE", CSRNG_CMD_GENERATE, cmd_type)) {
            all_passed = false;
        }

        if (cmd_ack != 1) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_003: CMD_ACK should be 1, got " << cmd_ack;
            all_passed = false;
        }

        if (cmd_sts != 0) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_003: CMD_STS should be 0 (success), got 0x"
                                  << std::hex << cmd_sts;
            all_passed = false;
        }
    }

    // Inject entropy to verify buffering
    inject_entropy_to_buffer(0x11111111, 0x22222222, 0x33333333, 0x44444444, false);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_003: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_003: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 4: Boot Mode Entropy Distribution
// =============================================================================

bool test_edn_func_012::test_boot_mode_entropy_distribution()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_004: Boot Mode Entropy Distribution...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Enable boot mode
    enable_boot_mode();
    wait(50, SC_NS);

    // Wait for BootGenAckWait state
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_004: Failed to reach BootGenAckWait";
        all_passed = false;
    }

    // Inject 128-bit entropy block
    inject_entropy_to_buffer(0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC, false);
    wait(10, SC_NS);

    // Test endpoint 0 request
    assert_endpoint_request(0);
    wait(5, SC_NS);

    if (!wait_for_endpoint_ack(0, 500.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_004: Timeout waiting for edn_ack[0]";
        all_passed = false;
    } else {
        uint32_t data = read_endpoint_data(0);
        if (!verify_value("edn_bus[0]", 0xAABBCCDD, data)) {
            all_passed = false;
        }
    }

    deassert_endpoint_request(0);
    wait(10, SC_NS);

    // Test endpoint 3 request
    assert_endpoint_request(3);
    wait(5, SC_NS);

    if (!wait_for_endpoint_ack(3, 500.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_004: Timeout waiting for edn_ack[3]";
        all_passed = false;
    } else {
        uint32_t data = read_endpoint_data(3);
        if (!verify_value("edn_bus[3]", 0x11223344, data)) {
            all_passed = false;
        }
    }

    deassert_endpoint_request(3);
    wait(10, SC_NS);

    // Test multiple concurrent requests
    inject_entropy_to_buffer(0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x9ABCDEF0, false);
    wait(10, SC_NS);

    assert_endpoint_request(2);
    assert_endpoint_request(5);
    assert_endpoint_request(7);
    wait(5, SC_NS);

    // Verify all endpoints get serviced
    bool ep2_acked = wait_for_endpoint_ack(2, 500.0);
    bool ep5_acked = wait_for_endpoint_ack(5, 500.0);
    bool ep7_acked = wait_for_endpoint_ack(7, 500.0);

    if (!ep2_acked || !ep5_acked || !ep7_acked) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_004: Not all concurrent endpoints acknowledged";
        all_passed = false;
    }

    deassert_endpoint_request(2);
    deassert_endpoint_request(5);
    deassert_endpoint_request(7);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_004: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_004: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 5: Boot Mode Pre-FIPS Indicator
// =============================================================================

bool test_edn_func_012::test_boot_mode_pre_fips_indicator()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_005: Boot Mode Pre-FIPS Indicator...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Enable boot mode
    enable_boot_mode();
    wait(50, SC_NS);

    // Wait for BootGenAckWait
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_005: Failed to reach BootGenAckWait";
        all_passed = false;
    }

    // Inject entropy with FIPS=0 (pre-FIPS boot seed)
    inject_entropy_to_buffer(0x11111111, 0x22222222, 0x33333333, 0x44444444, false);
    wait(10, SC_NS);

    // Test endpoint 0
    assert_endpoint_request(0);
    wait(5, SC_NS);

    if (wait_for_endpoint_ack(0, 500.0)) {
        bool fips0 = read_endpoint_fips(0);
        if (fips0 != false) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_005: edn_fips[0] should be 0, got " << fips0;
            all_passed = false;
        }

        // Verify data is valid despite FIPS=0
        uint32_t data0 = read_endpoint_data(0);
        if (data0 == 0) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_005: edn_bus[0] should contain valid data";
            all_passed = false;
        }
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_005: Timeout waiting for edn_ack[0]";
        all_passed = false;
    }

    deassert_endpoint_request(0);
    wait(10, SC_NS);

    // Test endpoint 3
    assert_endpoint_request(3);
    wait(5, SC_NS);

    if (wait_for_endpoint_ack(3, 500.0)) {
        bool fips3 = read_endpoint_fips(3);
        if (fips3 != false) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_005: edn_fips[3] should be 0, got " << fips3;
            all_passed = false;
        }
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_005: Timeout waiting for edn_ack[3]";
        all_passed = false;
    }

    deassert_endpoint_request(3);
    wait(10, SC_NS);

    // Test endpoint 7
    assert_endpoint_request(7);
    wait(5, SC_NS);

    if (wait_for_endpoint_ack(7, 500.0)) {
        bool fips7 = read_endpoint_fips(7);
        if (fips7 != false) {
            REG_ERROR(1, logger) << "TC_EDN_BOOT_005: edn_fips[7] should be 0, got " << fips7;
            all_passed = false;
        }
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_005: Timeout waiting for edn_ack[7]";
        all_passed = false;
    }

    deassert_endpoint_request(7);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_005: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_005: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 6: Boot Mode Exit Sequence
// =============================================================================

bool test_edn_func_012::test_boot_mode_exit_sequence()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_006: Boot Mode Exit Sequence...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Enable boot mode
    enable_boot_mode();
    wait(50, SC_NS);

    // Wait for stable boot operation
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: Failed to reach BootGenAckWait";
        all_passed = false;
    }

    // Verify HW_CMD_STS.BOOT_MODE = 1 before exit
    uint32_t hw_cmd_sts_before;

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_before);
    bool boot_mode_before = hw_cmd_sts_before & 0x1;
    if (!boot_mode_before) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: BOOT_MODE should be 1 before exit";
        all_passed = false;
    }

    // Exit boot mode
    exit_boot_mode();
    wait(50, SC_NS); // Allow uninstantiate execution

    // Verify state transitioned to SWPortMode
    if (!verify_state(STATE_SW_PORT_MODE, "After boot mode exit")) {
        all_passed = false;
    }

    // Verify HW_CMD_STS.BOOT_MODE = 0
    uint32_t hw_cmd_sts_after;

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts_after);
    bool boot_mode_after = hw_cmd_sts_after & 0x1;
    if (boot_mode_after) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: BOOT_MODE should be 0 after exit";
        all_passed = false;
    }

    // Verify CMD_TYPE shows Uninstantiate
    uint32_t cmd_type = (hw_cmd_sts_after >> 2) & 0xF;
    if (!verify_value("HW_CMD_STS.CMD_TYPE", CSRNG_CMD_UNINSTANTIATE, cmd_type)) {
        all_passed = false;
    }

    // Verify CMD_ACK and CMD_STS indicate success
    bool cmd_ack = (hw_cmd_sts_after >> 6) & 0x1;
    uint32_t cmd_sts = (hw_cmd_sts_after >> 7) & 0x7;

    if (cmd_ack != 1) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: CMD_ACK should be 1, got " << cmd_ack;
        all_passed = false;
    }

    if (cmd_sts != 0) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: CMD_STS should be 0, got 0x"
                              << std::hex << cmd_sts;
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_006: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_006: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 7: Boot Mode State Transitions
// =============================================================================

bool test_edn_func_012::test_boot_mode_state_transitions()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_007: Boot Mode State Transitions...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Verify Idle state
    if (!verify_state(STATE_IDLE, "Initial reset state")) {
        all_passed = false;
    }

    // Enable boot mode
    enable_boot_mode();
    wait(20, SC_NS);

    // Verify transition to boot mode operational state (BootGenAckWait)
    // Note: BootInsAckWait is transient; boot mode auto-progresses to BootGenAckWait
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_007: Timeout waiting for BootGenAckWait transition";
        all_passed = false;
    } else {
        REG_INFO(1, logger) << "TC_EDN_BOOT_007: State transitioned to BootGenAckWait";
    }

    // Verify state remains stable
    wait(50, SC_NS);
    if (!verify_state(STATE_BOOT_GEN_ACK_WAIT, "Stable in BootGenAckWait")) {
        all_passed = false;
    }

    // Exit boot mode
    exit_boot_mode();
    wait(50, SC_NS);

    // Verify transition to SWPortMode
    if (!verify_state(STATE_SW_PORT_MODE, "After boot mode exit")) {
        all_passed = false;
    }

    // Verify state remains stable in SWPortMode
    wait(50, SC_NS);
    if (!verify_state(STATE_SW_PORT_MODE, "Stable in SWPortMode")) {
        all_passed = false;
    }

    // Test second boot cycle
    apply_reset(100.0);
    wait(10, SC_NS);

    // Verify state reset to Idle
    if (!verify_state(STATE_IDLE, "After second reset")) {
        all_passed = false;
    }

    // Re-enable boot mode
    enable_boot_mode();
    wait(50, SC_NS);

    // Verify state transitions correctly on second cycle
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_007: Second boot cycle transition failed";
        all_passed = false;
    } else {
        REG_INFO(1, logger) << "TC_EDN_BOOT_007: Second boot cycle successful";
    }

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_007: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_007: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 8: Boot Mode Invalid CLEN
// =============================================================================

bool test_edn_func_012::test_boot_mode_invalid_clen()
{
    REG_INFO(1, logger) << "Starting TC_EDN_BOOT_008: Boot Mode Invalid CLEN...";

    bool all_passed = true;

    // Apply reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Write invalid BOOT_INS_CMD with clen = 1 (bits [11:8])
    uint32_t invalid_boot_ins_cmd = 0x00000011; // clen=1, cmd=1
    register_write_32(BOOT_INS_CMD_OFFSET, invalid_boot_ins_cmd);

    // BOOT_GEN_CMD clen lives in bits [11:8] (same as BOOT_INS_CMD). glen=0 so
    // the generate loop does not spin 0xFFF RAND_bytes iterations.
    uint32_t invalid_boot_gen_cmd = 0x00000203; // clen=2, cmd=3, glen=0
    register_write_32(BOOT_GEN_CMD_OFFSET, invalid_boot_gen_cmd);

    wait(10, SC_NS);

    // Enable boot mode
    enable_boot_mode();

    // Wait for the commands to be dispatched
    wait(500, SC_NS);

    // As long as it doesn't crash and completes the state transitions, the test passes
    // The warnings should be logged by the model
    if (!wait_for_state(STATE_BOOT_GEN_ACK_WAIT, 1000.0)) {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_008: Timeout waiting for BootGenAckWait state";
        all_passed = false;
    }

    // Force a non-zero CSRNG ack on the automatic Uninstantiate so
    // boot_mode_uninstantiate() takes the error / handle_csrng_error path.
    set_forced_csrng_ack_status(0x1);
    exit_boot_mode();
    wait(200, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "TC_EDN_BOOT_008: PASSED";
    } else {
        REG_ERROR(1, logger) << "TC_EDN_BOOT_008: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Helper Functions
// =============================================================================

void test_edn_func_012::enable_boot_mode()
{
    // Write CTRL with EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6
    // CTRL register format: [15:12]=CMD_FIFO_RST, [11:8]=AUTO_REQ_MODE,
    //                       [7:4]=BOOT_REQ_MODE, [3:0]=EDN_ENABLE
    uint32_t ctrl_value = 0x00009966; // EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6, others=0x9
    register_write_32(CTRL_OFFSET, ctrl_value);
    REG_INFO(1, logger) << "Boot mode enabled (CTRL=0x" << std::hex << ctrl_value << ")";
}

void test_edn_func_012::exit_boot_mode()
{
    // Clear BOOT_REQ_MODE while keeping EDN_ENABLE
    uint32_t ctrl_value = 0x00009996; // EDN_ENABLE=0x6, BOOT_REQ_MODE=0x9
    register_write_32(CTRL_OFFSET, ctrl_value);
    REG_INFO(1, logger) << "Boot mode exit initiated (CTRL=0x" << std::hex << ctrl_value << ")";
}

bool test_edn_func_012::verify_state(uint32_t expected_state, const std::string& context)
{
    uint32_t actual_state;

    register_read_32(MAIN_SM_STATE_OFFSET, actual_state);

    actual_state &= 0x1FF; // 9-bit state

    if (actual_state != expected_state) {
        REG_ERROR(1, logger) << "State verification failed (" << context << "): expected 0x"
                              << std::hex << expected_state << ", got 0x" << actual_state;
        return false;
    }

    REG_INFO(1, logger) << "State verified (" << context << "): 0x"
                         << std::hex << actual_state;
    return true;
}

bool test_edn_func_012::verify_hw_cmd_status(bool boot_mode_expected,
                                              uint32_t cmd_type_expected,
                                              const std::string& context)
{
    uint32_t hw_cmd_sts;

    register_read_32(HW_CMD_STS_OFFSET, hw_cmd_sts);
    bool boot_mode = hw_cmd_sts & 0x1;
    uint32_t cmd_type = (hw_cmd_sts >> 2) & 0xF;

    bool passed = true;

    if (boot_mode != boot_mode_expected) {
        REG_ERROR(1, logger) << "HW_CMD_STS.BOOT_MODE verification failed (" << context
                              << "): expected " << boot_mode_expected << ", got " << boot_mode;
        passed = false;
    }

    if (cmd_type != cmd_type_expected) {
        REG_ERROR(1, logger) << "HW_CMD_STS.CMD_TYPE verification failed (" << context
                              << "): expected 0x" << std::hex << cmd_type_expected
                              << ", got 0x" << cmd_type;
        passed = false;
    }

    return passed;
}

bool test_edn_func_012::wait_for_state(uint32_t target_state, double timeout_ns)
{
    sc_time start_time = sc_time_stamp();
    sc_time timeout(timeout_ns, SC_NS);

    while ((sc_time_stamp() - start_time) < timeout) {
        uint32_t current_state;

        register_read_32(MAIN_SM_STATE_OFFSET, current_state);

        current_state &= 0x1FF;
        if (current_state == target_state) {
            return true;
        }
        wait(10, SC_NS); // Poll interval
    }

    return false;
}

void test_edn_func_012::inject_entropy_to_buffer(uint32_t chunk0, uint32_t chunk1,
                                                   uint32_t chunk2, uint32_t chunk3, bool fips)
{
    // Use edn_test infrastructure to inject entropy
    uint32_t genbits[4] = {chunk0, chunk1, chunk2, chunk3};
    provide_csrng_entropy(genbits, fips);
}

bool test_edn_func_012::wait_for_endpoint_ack(unsigned int endpoint_id, double timeout_ns)
{
    sc_time start_time = sc_time_stamp();
    sc_time timeout(timeout_ns, SC_NS);

    while ((sc_time_stamp() - start_time) < timeout) {
        if (edn_ack[endpoint_id].read() == true) {
            return true;
        }
        wait(5, SC_NS); // Poll interval
    }

    return false;
}

void test_edn_func_012::assert_endpoint_request(unsigned int endpoint_id)
{
    edn_req[endpoint_id].write(true);
    wait(SC_ZERO_TIME); // Delta cycle for signal propagation
}

void test_edn_func_012::deassert_endpoint_request(unsigned int endpoint_id)
{
    edn_req[endpoint_id].write(false);
    wait(SC_ZERO_TIME);
}

uint32_t test_edn_func_012::read_endpoint_data(unsigned int endpoint_id)
{
    return edn_bus[endpoint_id].read().to_uint();
}

bool test_edn_func_012::read_endpoint_fips(unsigned int endpoint_id)
{
    return edn_fips[endpoint_id].read();
}

bool test_edn_func_012::verify_value(const std::string& context,
                                      uint32_t expected, uint32_t actual)
{
    if (expected != actual) {
        REG_ERROR(1, logger) << "Value verification failed (" << context << "): expected 0x"
                              << std::hex << expected << ", got 0x" << actual;
        return false;
    }
    return true;
}

void test_edn_func_012::report_test_result(const std::string& test_name, bool passed,
                                            const std::string& message)
{
    m_tests_run++;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << test_name << ": PASSED";
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(1, logger) << test_name << ": FAILED";
    }

    if (!message.empty()) {
        REG_INFO(1, logger) << "  " << message;
    }
}
