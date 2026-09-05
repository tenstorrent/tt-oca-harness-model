// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_005.cpp
 * @brief EDN_FUNC_005 Test Suite Implementation
 *
 * Comprehensive implementation of all 20 test cases for EDN_FUNC_005
 * (Endpoint Entropy Distribution) functionality verification.
 *
 * Test Coverage Matrix:
 * - T1:  Single Endpoint Request - Endpoint 0
 * - T2:  Single Endpoint Request - Endpoint 7
 * - T3:  Request/Acknowledge Handshake Timing
 * - T4:  Data Bus Validity During Acknowledge
 * - T5:  FIPS Indicator Propagation (FIPS=1)
 * - T6:  FIPS Indicator Propagation (FIPS=0)
 * - T7:  Concurrent Requests - 2 Endpoints
 * - T8:  Concurrent Requests - All 8 Endpoints
 * - T9:  Round-Robin Arbitration Order
 * - T10: Arbitration Fairness (No Starvation)
 * - T11: 128-bit to 32-bit Conversion Correctness
 * - T12: Sequential Chunk Distribution
 * - T13: Data Persistence on Bus After Acknowledge
 * - T14: Buffer Empty Condition
 * - T15: Request During EDN Disabled
 * - T16: Request with Empty Entropy Buffer
 * - T17: Simultaneous Request and Acknowledge
 * - T18: Repeated Requests from Same Endpoint
 * - T19: Endpoint Interface After Reset
 * - T20: Buffer Refill During Active Distribution
 *
 * @date 2026-01-13
 */

#include "test_edn_func_005.h"
#include <iomanip>

// =============================================================================
// Constructor and Destructor
// =============================================================================

test_edn_func_005::test_edn_func_005(sc_module_name name)
    : edn_test(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    REG_INFO(1, logger) << "EDN_FUNC_005 test suite initialized";
    REG_INFO(1, logger) << "Test Coverage: Endpoint Entropy Distribution (20 test cases)";
}

test_edn_func_005::~test_edn_func_005()
{
    REG_INFO(1, logger) << "EDN_FUNC_005 test suite terminated";
}

// =============================================================================
// Main Test Execution
// =============================================================================

unsigned int test_edn_func_005::run_all_tests()
{
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_005 Test Suite Execution Start";
    REG_INFO(1, logger) << "========================================";

    // Apply reset before starting tests
    REG_INFO(1, logger) << "Applying system reset...";
    apply_reset(100.0); // 100ns reset pulse
    wait(10, SC_NS);    // Wait for reset propagation

    // Execute all 20 test cases
    bool result;

    // Test 1: Single Endpoint Request - Endpoint 0
    result = test_endpoint_single_request_ep0();
    report_test_result("T1: Single Endpoint Request - Endpoint 0", result);

    // Test 2: Single Endpoint Request - Endpoint 7
    result = test_endpoint_single_request_ep7();
    report_test_result("T2: Single Endpoint Request - Endpoint 7", result);

    // Test 3: Request/Acknowledge Handshake Timing
    result = test_endpoint_handshake_timing();
    report_test_result("T3: Request/Acknowledge Handshake Timing", result);

    // Test 4: Data Bus Validity During Acknowledge
    result = test_endpoint_data_bus_validity();
    report_test_result("T4: Data Bus Validity During Acknowledge", result);

    // Test 5: FIPS Indicator Propagation (FIPS=1)
    result = test_endpoint_fips_propagation_true();
    report_test_result("T5: FIPS Indicator Propagation (FIPS=1)", result);

    // Test 6: FIPS Indicator Propagation (FIPS=0)
    result = test_endpoint_fips_propagation_false();
    report_test_result("T6: FIPS Indicator Propagation (FIPS=0)", result);

    // Test 7: Concurrent Requests - 2 Endpoints
    result = test_endpoint_concurrent_two_requests();
    report_test_result("T7: Concurrent Requests - 2 Endpoints", result);

    // Test 8: Concurrent Requests - All 8 Endpoints
    result = test_endpoint_concurrent_all_eight();
    report_test_result("T8: Concurrent Requests - All 8 Endpoints", result);

    // Test 9: Round-Robin Arbitration Order
    result = test_endpoint_round_robin_order();
    report_test_result("T9: Round-Robin Arbitration Order", result);

    // Test 10: Arbitration Fairness (No Starvation)
    result = test_endpoint_arbitration_fairness();
    report_test_result("T10: Arbitration Fairness (No Starvation)", result);

    // Test 11: 128-bit to 32-bit Conversion Correctness
    result = test_endpoint_width_conversion();
    report_test_result("T11: 128-bit to 32-bit Conversion Correctness", result);

    // Test 12: Sequential Chunk Distribution
    result = test_endpoint_sequential_chunks();
    report_test_result("T12: Sequential Chunk Distribution", result);

    // Test 13: Data Persistence on Bus After Acknowledge
    result = test_endpoint_data_persistence();
    report_test_result("T13: Data Persistence on Bus After Acknowledge", result);

    // Test 14: Buffer Empty Condition
    result = test_endpoint_buffer_empty();
    report_test_result("T14: Buffer Empty Condition", result);

    // Test 15: Request During EDN Disabled
    result = test_endpoint_request_while_disabled();
    report_test_result("T15: Request During EDN Disabled", result);

    // Test 16: Request with Empty Entropy Buffer
    result = test_endpoint_empty_buffer_request();
    report_test_result("T16: Request with Empty Entropy Buffer", result);

    // Test 17: Simultaneous Request and Acknowledge
    result = test_endpoint_simultaneous_req_ack();
    report_test_result("T17: Simultaneous Request and Acknowledge", result);

    // Test 18: Repeated Requests from Same Endpoint
    result = test_endpoint_repeated_requests();
    report_test_result("T18: Repeated Requests from Same Endpoint", result);

    // Test 19: Endpoint Interface After Reset
    result = test_endpoint_interface_after_reset();
    report_test_result("T19: Endpoint Interface After Reset", result);

    // Test 20: Buffer Refill During Active Distribution
    result = test_endpoint_buffer_refill_during_distribution();
    report_test_result("T20: Buffer Refill During Active Distribution", result);

    // Print summary
    REG_INFO(1, logger) << "========================================";
    REG_INFO(1, logger) << "EDN_FUNC_005 Test Suite Summary";
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
// Test Case 1: Single Endpoint Request - Endpoint 0
// =============================================================================

bool test_edn_func_005::test_endpoint_single_request_ep0()
{
    REG_INFO(1, logger) << "Starting test_endpoint_single_request_ep0...";

    bool all_passed = true;

    // Enable EDN for endpoint operation
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject 128-bit entropy block with FIPS=1
    // Pattern: 0xDEADBEEF_CAFEBABE_12345678_9ABCDEF0
    inject_entropy_to_buffer(0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x9ABCDEF0, true);
    wait(5, SC_NS);

    // Assert request from endpoint 0
    assert_endpoint_request(0);
    wait(5, SC_NS);

    // Wait for acknowledge with timeout
    if (!wait_for_acknowledge(0, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_single_request_ep0: Timeout waiting for edn_ack[0]";
        all_passed = false;
    } else {
        // Verify data on edn_bus[0] (should be first chunk: 0xDEADBEEF)
        uint32_t data = read_endpoint_data(0);
        if (!verify_value("edn_bus[0]", 0xDEADBEEF, data)) {
            all_passed = false;
        }

        // Verify FIPS indicator (should be true)
        bool fips = read_endpoint_fips(0);
        if (fips != true) {
            REG_ERROR(1, logger) << "test_endpoint_single_request_ep0: edn_fips[0] expected 1, got " << fips;
            all_passed = false;
        }

        // Verify acknowledge is asserted
        if (!edn_ack[0].read()) {
            REG_ERROR(1, logger) << "test_endpoint_single_request_ep0: edn_ack[0] not asserted";
            all_passed = false;
        }
    }

    // Deassert request
    deassert_endpoint_request(0);
    wait(10, SC_NS);

    // Verify acknowledge deasserts
    if (edn_ack[0].read()) {
        REG_ERROR(1, logger) << "test_endpoint_single_request_ep0: edn_ack[0] did not deassert";
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_single_request_ep0: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_single_request_ep0: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 2: Single Endpoint Request - Endpoint 7
// =============================================================================

bool test_edn_func_005::test_endpoint_single_request_ep7()
{
    REG_INFO(1, logger) << "Starting test_endpoint_single_request_ep7...";

    bool all_passed = true;

    // Reset and re-enable EDN
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject entropy with FIPS=0 (pre-FIPS)
    inject_entropy_to_buffer(0x11111111, 0x22222222, 0x33333333, 0x44444444, false);
    wait(5, SC_NS);

    // Assert request from endpoint 7
    assert_endpoint_request(7);
    wait(5, SC_NS);

    // Wait for acknowledge
    if (!wait_for_acknowledge(7, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_single_request_ep7: Timeout waiting for edn_ack[7]";
        all_passed = false;
    } else {
        // Verify data (first chunk: 0x11111111)
        uint32_t data = read_endpoint_data(7);
        if (!verify_value("edn_bus[7]", 0x11111111, data)) {
            all_passed = false;
        }

        // Verify FIPS=0
        bool fips = read_endpoint_fips(7);
        if (fips != false) {
            REG_ERROR(1, logger) << "test_endpoint_single_request_ep7: edn_fips[7] expected 0, got " << fips;
            all_passed = false;
        }
    }

    // Deassert request
    deassert_endpoint_request(7);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_single_request_ep7: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_single_request_ep7: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 3: Request/Acknowledge Handshake Timing
// =============================================================================

bool test_edn_func_005::test_endpoint_handshake_timing()
{
    REG_INFO(1, logger) << "Starting test_endpoint_handshake_timing...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject entropy
    inject_entropy_to_buffer(0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD, true);
    wait(5, SC_NS);

    // Verify ack is low before request
    if (edn_ack[2].read()) {
        REG_ERROR(1, logger) << "test_endpoint_handshake_timing: edn_ack[2] asserted before request";
        all_passed = false;
    }

    // Assert request
    assert_endpoint_request(2);
    wait(2, SC_NS);

    // Monitor for ack assertion
    bool ack_asserted = false;
    for (int i = 0; i < 100; i++) {
        if (edn_ack[2].read()) {
            ack_asserted = true;
            break;
        }
        wait(1, SC_NS);
    }

    if (!ack_asserted) {
        REG_ERROR(1, logger) << "test_endpoint_handshake_timing: edn_ack[2] never asserted";
        all_passed = false;
    }

    // Verify data and FIPS are valid when ack asserted
    if (ack_asserted) {
        uint32_t data = read_endpoint_data(2);
        if (data != 0xAAAAAAAA) {
            REG_ERROR(1, logger) << "test_endpoint_handshake_timing: Invalid data when ack asserted";
            all_passed = false;
        }
    }

    // Deassert request
    deassert_endpoint_request(2);
    wait(5, SC_NS);

    // Verify ack deasserts
    if (edn_ack[2].read()) {
        REG_ERROR(1, logger) << "test_endpoint_handshake_timing: edn_ack[2] did not deassert after req low";
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_handshake_timing: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_handshake_timing: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 4: Data Bus Validity During Acknowledge
// =============================================================================

bool test_edn_func_005::test_endpoint_data_bus_validity()
{
    REG_INFO(1, logger) << "Starting test_endpoint_data_bus_validity...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject known pattern
    inject_entropy_to_buffer(0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00, true);
    wait(5, SC_NS);

    // Request from endpoint 3
    assert_endpoint_request(3);
    wait(5, SC_NS);

    // Wait for ack
    if (!wait_for_acknowledge(3, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_data_bus_validity: Timeout waiting for ack";
        all_passed = false;
    } else {
        // Sample bus multiple times while ack is high
        for (int i = 0; i < 5; i++) {
            uint32_t data = read_endpoint_data(3);
            if (data != 0x11223344) {
                REG_ERROR(1, logger) << "test_endpoint_data_bus_validity: Bus unstable during ack (sample " << i << ")";
                all_passed = false;
            }
            wait(2, SC_NS);
        }
    }

    // Deassert request
    deassert_endpoint_request(3);
    wait(10, SC_NS);

    // Verify data persists on bus after ack deasserts
    uint32_t data_after = read_endpoint_data(3);
    if (data_after != 0x11223344) {
        REG_ERROR(1, logger) << "test_endpoint_data_bus_validity: Data did not persist after ack deassert";
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_data_bus_validity: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_data_bus_validity: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 5: FIPS Indicator Propagation (FIPS=1)
// =============================================================================

bool test_edn_func_005::test_endpoint_fips_propagation_true()
{
    REG_INFO(1, logger) << "Starting test_endpoint_fips_propagation_true...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject FIPS-approved entropy
    inject_entropy_to_buffer(0xF1F1F1F1, 0xF2F2F2F2, 0xF3F3F3F3, 0xF4F4F4F4, true);
    wait(5, SC_NS);

    // Test multiple endpoints (0, 2, 5)
    unsigned int test_endpoints[] = {0, 2, 5};
    for (unsigned int ep : test_endpoints) {
        assert_endpoint_request(ep);
        wait(5, SC_NS);

        if (wait_for_acknowledge(ep, 500.0)) {
            bool fips = read_endpoint_fips(ep);
            if (!fips) {
                REG_ERROR(1, logger) << "test_endpoint_fips_propagation_true: endpoint " << ep << " FIPS=0, expected 1";
                all_passed = false;
            }
        } else {
            REG_ERROR(1, logger) << "test_endpoint_fips_propagation_true: Timeout on endpoint " << ep;
            all_passed = false;
        }

        deassert_endpoint_request(ep);
        wait(10, SC_NS);
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_fips_propagation_true: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_fips_propagation_true: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 6: FIPS Indicator Propagation (FIPS=0)
// =============================================================================

bool test_edn_func_005::test_endpoint_fips_propagation_false()
{
    REG_INFO(1, logger) << "Starting test_endpoint_fips_propagation_false...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject pre-FIPS entropy (FIPS=0)
    inject_entropy_to_buffer(0xE0E0E0E0, 0xE1E1E1E1, 0xE2E2E2E2, 0xE3E3E3E3, false);
    wait(5, SC_NS);

    // Test multiple endpoints (1, 4, 6)
    unsigned int test_endpoints[] = {1, 4, 6};
    for (unsigned int ep : test_endpoints) {
        assert_endpoint_request(ep);
        wait(5, SC_NS);

        if (wait_for_acknowledge(ep, 500.0)) {
            bool fips = read_endpoint_fips(ep);
            if (fips) {
                REG_ERROR(1, logger) << "test_endpoint_fips_propagation_false: endpoint " << ep << " FIPS=1, expected 0";
                all_passed = false;
            }
        } else {
            REG_ERROR(1, logger) << "test_endpoint_fips_propagation_false: Timeout on endpoint " << ep;
            all_passed = false;
        }

        deassert_endpoint_request(ep);
        wait(10, SC_NS);
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_fips_propagation_false: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_fips_propagation_false: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 7: Concurrent Requests - 2 Endpoints
// =============================================================================

bool test_edn_func_005::test_endpoint_concurrent_two_requests()
{
    REG_INFO(1, logger) << "Starting test_endpoint_concurrent_two_requests...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject sufficient entropy (2 chunks minimum)
    inject_entropy_to_buffer(0xAA000000, 0xBB000000, 0xCC000000, 0xDD000000, true);
    wait(5, SC_NS);

    // Simultaneously assert requests from endpoints 0 and 1
    assert_endpoint_request(0);
    assert_endpoint_request(1);
    wait(5, SC_NS);

    // Wait for both acknowledgments (order doesn't matter due to arbitration)
    bool ack0_received = false;
    bool ack1_received = false;
    uint32_t data0 = 0;
    uint32_t data1 = 0;

    for (int i = 0; i < 200; i++) {
        if (edn_ack[0].read() && !ack0_received) {
            ack0_received = true;
            data0 = read_endpoint_data(0);
            REG_INFO(1, logger) << "test_endpoint_concurrent_two_requests: endpoint 0 serviced with data 0x"
                                 << std::hex << data0;
        }
        if (edn_ack[1].read() && !ack1_received) {
            ack1_received = true;
            data1 = read_endpoint_data(1);
            REG_INFO(1, logger) << "test_endpoint_concurrent_two_requests: endpoint 1 serviced with data 0x"
                                 << std::hex << data1;
        }
        if (ack0_received && ack1_received) {
            break;
        }
        wait(1, SC_NS);
    }

    if (!ack0_received) {
        REG_ERROR(1, logger) << "test_endpoint_concurrent_two_requests: endpoint 0 not serviced";
        all_passed = false;
    }

    if (!ack1_received) {
        REG_ERROR(1, logger) << "test_endpoint_concurrent_two_requests: endpoint 1 not serviced";
        all_passed = false;
    }

    // Verify both received unique data
    if (ack0_received && ack1_received && (data0 == data1)) {
        REG_ERROR(1, logger) << "test_endpoint_concurrent_two_requests: Both endpoints received same data (duplication)";
        all_passed = false;
    }

    // Deassert requests
    deassert_endpoint_request(0);
    deassert_endpoint_request(1);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_concurrent_two_requests: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_concurrent_two_requests: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 8: Concurrent Requests - All 8 Endpoints
// =============================================================================

bool test_edn_func_005::test_endpoint_concurrent_all_eight()
{
    REG_INFO(1, logger) << "Starting test_endpoint_concurrent_all_eight...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject sufficient entropy (2x 128-bit blocks = 8 chunks)
    inject_entropy_to_buffer(0xA0000000, 0xA1000000, 0xA2000000, 0xA3000000, true);
    inject_entropy_to_buffer(0xB0000000, 0xB1000000, 0xB2000000, 0xB3000000, true);
    wait(5, SC_NS);

    // Assert all 8 endpoint requests simultaneously
    for (int i = 0; i < 8; i++) {
        assert_endpoint_request(i);
    }
    wait(5, SC_NS);

    // Monitor all endpoints for acknowledgment
    bool ack_received[8] = {false};
    uint32_t data_received[8] = {0};
    int acks_count = 0;

    for (int iteration = 0; iteration < 500; iteration++) {
        for (int ep = 0; ep < 8; ep++) {
            if (edn_ack[ep].read() && !ack_received[ep]) {
                ack_received[ep] = true;
                data_received[ep] = read_endpoint_data(ep);
                acks_count++;
                REG_INFO(1, logger) << "test_endpoint_concurrent_all_eight: endpoint " << ep
                                     << " serviced with data 0x" << std::hex << data_received[ep];
            }
        }
        if (acks_count == 8) {
            break;
        }
        wait(1, SC_NS);
    }

    // Verify all endpoints were serviced
    for (int i = 0; i < 8; i++) {
        if (!ack_received[i]) {
            REG_ERROR(1, logger) << "test_endpoint_concurrent_all_eight: endpoint " << i << " not serviced (starvation)";
            all_passed = false;
        }
    }

    // Verify all endpoints received unique data
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            if (ack_received[i] && ack_received[j] && (data_received[i] == data_received[j])) {
                REG_ERROR(1, logger) << "test_endpoint_concurrent_all_eight: endpoints " << i << " and " << j
                                      << " received duplicate data";
                all_passed = false;
            }
        }
    }

    // Deassert all requests
    for (int i = 0; i < 8; i++) {
        deassert_endpoint_request(i);
    }
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_concurrent_all_eight: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_concurrent_all_eight: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 9: Round-Robin Arbitration Order
// =============================================================================

bool test_edn_func_005::test_endpoint_round_robin_order()
{
    REG_INFO(1, logger) << "Starting test_endpoint_round_robin_order...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject entropy
    inject_entropy_to_buffer(0x10000000, 0x20000000, 0x30000000, 0x40000000, true);
    inject_entropy_to_buffer(0x50000000, 0x60000000, 0x70000000, 0x80000000, true);
    wait(5, SC_NS);

    // First, service endpoint 3 to set arbitration index to 4
    assert_endpoint_request(3);
    wait(5, SC_NS);
    if (!wait_for_acknowledge(3, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_round_robin_order: Failed to service endpoint 3 for setup";
        all_passed = false;
    }
    deassert_endpoint_request(3);
    wait(10, SC_NS);

    // Now assert concurrent requests from endpoints 0, 2, 5, 7
    assert_endpoint_request(0);
    assert_endpoint_request(2);
    assert_endpoint_request(5);
    assert_endpoint_request(7);

    // Monitor immediately so only the first newly-acked endpoint is visible
    // before the mock's next round-robin tick.
    // Expected order: 5 → 7 → 0 → 2 starting from arb index 4.
    // Deassert each endpoint as soon as it is acknowledged so the next RR
    // grant is visible — waiting first lets all four acks pile up and the
    // poll order {0,2,5,7} would be recorded instead of the grant order.
    int service_order[4] = {0, 0, 0, 0};
    int service_count = 0;

    for (int iteration = 0; iteration < 500; iteration++) {
        for (int ep : {0, 2, 5, 7}) {
            if (edn_ack[ep].read()) {
                bool already_serviced = false;
                for (int k = 0; k < service_count; k++) {
                    if (service_order[k] == ep) {
                        already_serviced = true;
                        break;
                    }
                }
                if (!already_serviced) {
                    service_order[service_count++] = ep;
                    REG_INFO(1, logger) << "test_endpoint_round_robin_order: Service order[" << (service_count-1)
                                         << "] = endpoint " << ep;
                    deassert_endpoint_request(ep);
                    wait(5, SC_NS);
                }
            }
        }
        if (service_count == 4) {
            break;
        }
        wait(1, SC_NS);
    }

    // Verify expected order: 5, 7, 0, 2
    int expected_order[] = {5, 7, 0, 2};
    for (int i = 0; i < 4; i++) {
        if (service_order[i] != expected_order[i]) {
            REG_ERROR(1, logger) << "test_endpoint_round_robin_order: Expected service_order[" << i << "]="
                                  << expected_order[i] << ", got " << service_order[i];
            all_passed = false;
        }
    }

    // Deassert all requests
    for (int ep : {0, 2, 5, 7}) {
        deassert_endpoint_request(ep);
    }
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_round_robin_order: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_round_robin_order: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 10: Arbitration Fairness (No Starvation)
// =============================================================================

bool test_edn_func_005::test_endpoint_arbitration_fairness()
{
    REG_INFO(1, logger) << "Starting test_endpoint_arbitration_fairness...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject abundant entropy (4 blocks = 16 chunks)
    for (int block = 0; block < 4; block++) {
        inject_entropy_to_buffer(
            0x10000000 + (block << 24),
            0x20000000 + (block << 24),
            0x30000000 + (block << 24),
            0x40000000 + (block << 24),
            true
        );
    }
    wait(5, SC_NS);

    // Count service occurrences per endpoint over 16 transactions
    int service_count[8] = {0};
    int total_services = 0;
    bool endpoint_acked[8] = {false};  // Track which endpoints have been acked this iteration

    // Assert initial requests from all 8 endpoints
    for (int i = 0; i < 8; i++) {
        assert_endpoint_request(i);
    }
    wait(5, SC_NS);

    // Monitor and service endpoints with proper handshake protocol
    for (int iteration = 0; iteration < 1000; iteration++) {
        // Check for new acknowledges
        for (int ep = 0; ep < 8; ep++) {
            if (edn_ack[ep].read() && !endpoint_acked[ep]) {
                // Endpoint was just acknowledged
                service_count[ep]++;
                total_services++;
                endpoint_acked[ep] = true;
                REG_INFO(1, logger) << "test_endpoint_arbitration_fairness: endpoint " << ep
                                     << " serviced (count=" << service_count[ep] << ")";

                // Deassert request (complete handshake)
                deassert_endpoint_request(ep);
                wait(2, SC_NS);

                // Re-assert request for next chunk (simulate continuous demand)
                assert_endpoint_request(ep);
                wait(1, SC_NS);

                // Reset acked flag for next service
                endpoint_acked[ep] = false;
            }
        }

        if (total_services >= 16) {
            break;
        }
        wait(1, SC_NS);
    }

    REG_INFO(1, logger) << "test_endpoint_arbitration_fairness: Total services = " << total_services;

    // Verify each endpoint serviced exactly twice (fair distribution)
    for (int i = 0; i < 8; i++) {
        REG_INFO(1, logger) << "test_endpoint_arbitration_fairness: endpoint " << i
                             << " service count = " << service_count[i];
        if (service_count[i] < 1) {
            REG_ERROR(1, logger) << "test_endpoint_arbitration_fairness: endpoint " << i
                                  << " starved (count=" << service_count[i] << ")";
            all_passed = false;
        }
        // Allow ±1 variance for fairness (ideal is 2, acceptable is 1-3)
        if (service_count[i] < 1 || service_count[i] > 3) {
            REG_ERROR(1, logger) << "test_endpoint_arbitration_fairness: endpoint " << i
                                  << " unfair distribution (count=" << service_count[i] << ", expected ~2)";
            all_passed = false;
        }
    }

    // Deassert all requests
    for (int i = 0; i < 8; i++) {
        deassert_endpoint_request(i);
    }
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_arbitration_fairness: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_arbitration_fairness: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 11: 128-bit to 32-bit Conversion Correctness
// =============================================================================

bool test_edn_func_005::test_endpoint_width_conversion()
{
    REG_INFO(1, logger) << "Starting test_endpoint_width_conversion...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject known 128-bit pattern with specific chunk ordering
    // genbits[0]=0xAABBCCDD (MSB, chunk 0)
    // genbits[1]=0x11223344 (chunk 1)
    // genbits[2]=0x55667788 (chunk 2)
    // genbits[3]=0x99AABBCC (LSB, chunk 3)
    inject_entropy_to_buffer(0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC, true);
    wait(5, SC_NS);

    // Request 4 times from endpoint 2 to consume all chunks
    uint32_t expected_chunks[4] = {0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC};
    uint32_t received_chunks[4];

    for (int chunk = 0; chunk < 4; chunk++) {
        assert_endpoint_request(2);
        wait(5, SC_NS);

        if (!wait_for_acknowledge(2, 500.0)) {
            REG_ERROR(1, logger) << "test_endpoint_width_conversion: Timeout on chunk " << chunk;
            all_passed = false;
            break;
        }

        received_chunks[chunk] = read_endpoint_data(2);
        REG_INFO(1, logger) << "test_endpoint_width_conversion: chunk[" << chunk << "] = 0x"
                             << std::hex << received_chunks[chunk];

        deassert_endpoint_request(2);
        wait(10, SC_NS);
    }

    // Verify chunk order and values
    for (int i = 0; i < 4; i++) {
        if (!verify_value("chunk", expected_chunks[i], received_chunks[i])) {
            all_passed = false;
        }
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_width_conversion: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_width_conversion: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 12: Sequential Chunk Distribution
// =============================================================================

bool test_edn_func_005::test_endpoint_sequential_chunks()
{
    REG_INFO(1, logger) << "Starting test_endpoint_sequential_chunks...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject ONE 128-bit block (4 chunks)
    inject_entropy_to_buffer(0xC0000000, 0xC1000000, 0xC2000000, 0xC3000000, true);
    wait(5, SC_NS);

    // Request 4 times from endpoint 4
    for (int i = 0; i < 4; i++) {
        assert_endpoint_request(4);
        wait(5, SC_NS);

        if (!wait_for_acknowledge(4, 500.0)) {
            REG_ERROR(1, logger) << "test_endpoint_sequential_chunks: Timeout on request " << i;
            all_passed = false;
        }

        deassert_endpoint_request(4);
        wait(10, SC_NS);
    }

    // 5th request should NOT be acknowledged (buffer empty)
    assert_endpoint_request(4);
    wait(5, SC_NS);

    bool ack_on_5th = false;
    for (int i = 0; i < 100; i++) {
        if (edn_ack[4].read()) {
            ack_on_5th = true;
            break;
        }
        wait(1, SC_NS);
    }

    if (ack_on_5th) {
        REG_ERROR(1, logger) << "test_endpoint_sequential_chunks: 5th request acknowledged (buffer should be empty)";
        all_passed = false;
    }

    deassert_endpoint_request(4);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_sequential_chunks: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_sequential_chunks: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 13: Data Persistence on Bus After Acknowledge
// =============================================================================

bool test_edn_func_005::test_endpoint_data_persistence()
{
    REG_INFO(1, logger) << "Starting test_endpoint_data_persistence...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject entropy
    inject_entropy_to_buffer(0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF, 0x00000000, true);
    wait(5, SC_NS);

    // Request from endpoint 1
    assert_endpoint_request(1);
    wait(5, SC_NS);

    if (!wait_for_acknowledge(1, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_data_persistence: Timeout waiting for ack";
        all_passed = false;
        return false;
    }

    // Capture data when ack is high
    uint32_t data_during_ack = read_endpoint_data(1);
    bool fips_during_ack = read_endpoint_fips(1);

    // Deassert request
    deassert_endpoint_request(1);
    wait(50, SC_NS); // Significant delay to test persistence

    // Verify data persists
    uint32_t data_after_ack = read_endpoint_data(1);
    bool fips_after_ack = read_endpoint_fips(1);

    if (data_during_ack != data_after_ack) {
        REG_ERROR(1, logger) << "test_endpoint_data_persistence: Data did not persist (during=0x" << std::hex
                              << data_during_ack << ", after=0x" << data_after_ack << ")";
        all_passed = false;
    }

    if (fips_during_ack != fips_after_ack) {
        REG_ERROR(1, logger) << "test_endpoint_data_persistence: FIPS did not persist";
        all_passed = false;
    }

    // Issue request from different endpoint (endpoint 5)
    assert_endpoint_request(5);
    wait(5, SC_NS);
    wait_for_acknowledge(5, 500.0);
    deassert_endpoint_request(5);
    wait(10, SC_NS);

    // Verify endpoint 1 data still persists (not affected by endpoint 5 transaction)
    uint32_t data_after_other_ep = read_endpoint_data(1);
    if (data_during_ack != data_after_other_ep) {
        REG_ERROR(1, logger) << "test_endpoint_data_persistence: Data corrupted by other endpoint transaction";
        all_passed = false;
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_data_persistence: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_data_persistence: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 14: Buffer Empty Condition
// =============================================================================

bool test_edn_func_005::test_endpoint_buffer_empty()
{
    REG_INFO(1, logger) << "Starting test_endpoint_buffer_empty...";

    bool all_passed = true;

    // Reset and enable with empty buffer
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);
    // DO NOT inject entropy - buffer is empty

    // Assert request from endpoint 5
    assert_endpoint_request(5);
    wait(5, SC_NS);

    // Wait reasonable timeout - should NOT get acknowledge
    bool ack_received = false;
    for (int i = 0; i < 200; i++) {
        if (edn_ack[5].read()) {
            ack_received = true;
            break;
        }
        wait(1, SC_NS);
    }

    if (ack_received) {
        REG_ERROR(1, logger) << "test_endpoint_buffer_empty: Acknowledge received with empty buffer (incorrect)";
        all_passed = false;
    }

    // Now inject entropy
    inject_entropy_to_buffer(0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB, false);
    wait(5, SC_NS);

    // Verify acknowledge now asserts
    if (!wait_for_acknowledge(5, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_buffer_empty: No acknowledge after entropy injection";
        all_passed = false;
    }

    deassert_endpoint_request(5);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_buffer_empty: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_buffer_empty: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 15: Request During EDN Disabled
// =============================================================================

bool test_edn_func_005::test_endpoint_request_while_disabled()
{
    REG_INFO(1, logger) << "Starting test_endpoint_request_while_disabled...";

    bool all_passed = true;

    // Reset (EDN disabled by default)
    apply_reset(100.0);
    wait(10, SC_NS);

    // Inject entropy (should not matter while disabled)
    inject_entropy_to_buffer(0x77777777, 0x66666666, 0x55555555, 0x44444444, true);
    wait(5, SC_NS);

    // Assert request from endpoint 6 while EDN is disabled
    assert_endpoint_request(6);
    wait(5, SC_NS);

    // Verify NO acknowledge (EDN disabled)
    bool ack_while_disabled = false;
    for (int i = 0; i < 200; i++) {
        if (edn_ack[6].read()) {
            ack_while_disabled = true;
            break;
        }
        wait(1, SC_NS);
    }

    if (ack_while_disabled) {
        REG_ERROR(1, logger) << "test_endpoint_request_while_disabled: Acknowledge while EDN disabled (incorrect)";
        all_passed = false;
    }

    // Now enable EDN
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Verify request is now serviced
    if (!wait_for_acknowledge(6, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_request_while_disabled: No acknowledge after EDN enable";
        all_passed = false;
    }

    deassert_endpoint_request(6);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_request_while_disabled: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_request_while_disabled: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 16: Request with Empty Entropy Buffer
// =============================================================================

bool test_edn_func_005::test_endpoint_empty_buffer_request()
{
    REG_INFO(1, logger) << "Starting test_endpoint_empty_buffer_request...";

    bool all_passed = true;

    // Reset and enable with empty buffer
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Assert request
    assert_endpoint_request(0);
    wait(5, SC_NS);

    // Verify no acknowledge initially
    if (edn_ack[0].read()) {
        REG_ERROR(1, logger) << "test_endpoint_empty_buffer_request: Spurious ack with empty buffer";
        all_passed = false;
    }

    wait(50, SC_NS); // Request waits patiently

    // Inject one chunk
    inject_entropy_to_buffer(0x12341234, 0x56785678, 0x9ABC9ABC, 0xDEF0DEF0, true);
    wait(5, SC_NS);

    // Verify immediate acknowledge
    if (!wait_for_acknowledge(0, 100.0)) {
        REG_ERROR(1, logger) << "test_endpoint_empty_buffer_request: No immediate ack after entropy injection";
        all_passed = false;
    }

    uint32_t data = read_endpoint_data(0);
    if (data != 0x12341234) {
        REG_ERROR(1, logger) << "test_endpoint_empty_buffer_request: Incorrect data delivered";
        all_passed = false;
    }

    deassert_endpoint_request(0);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_empty_buffer_request: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_empty_buffer_request: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 17: Simultaneous Request and Acknowledge
// =============================================================================

bool test_edn_func_005::test_endpoint_simultaneous_req_ack()
{
    REG_INFO(1, logger) << "Starting test_endpoint_simultaneous_req_ack...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject entropy (2 blocks = 8 chunks, need 5)
    inject_entropy_to_buffer(0xFAFAFAFA, 0xFBFBFBFB, 0xFCFCFCFC, 0xFDFDFDFD, true);
    inject_entropy_to_buffer(0x11111111, 0x22222222, 0x33333333, 0x44444444, true);
    wait(5, SC_NS);

    // Test rapid request/deassert cycles (5 iterations)
    for (int cycle = 0; cycle < 5; cycle++) {
        assert_endpoint_request(3);
        wait(2, SC_NS);

        // Wait for ack
        bool ack_received = false;
        for (int i = 0; i < 100; i++) {
            if (edn_ack[3].read()) {
                ack_received = true;
                // Immediately deassert request (minimal hold time)
                deassert_endpoint_request(3);
                break;
            }
            wait(1, SC_NS);
        }

        if (!ack_received) {
            REG_ERROR(1, logger) << "test_endpoint_simultaneous_req_ack: No ack on cycle " << cycle;
            all_passed = false;
            break;
        }

        wait(5, SC_NS);
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_simultaneous_req_ack: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_simultaneous_req_ack: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 18: Repeated Requests from Same Endpoint
// =============================================================================

bool test_edn_func_005::test_endpoint_repeated_requests()
{
    REG_INFO(1, logger) << "Starting test_endpoint_repeated_requests...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject abundant entropy (3 blocks = 12 chunks, need 10)
    for (int block = 0; block < 3; block++) {
        inject_entropy_to_buffer(
            0x00000000 + (block << 0),
            0x11111111 + (block << 0),
            0x22222222 + (block << 0),
            0x33333333 + (block << 0),
            true
        );
    }
    wait(5, SC_NS);

    // Loop 10 times requesting from endpoint 7
    uint32_t received_data[10];
    for (int req = 0; req < 10; req++) {
        assert_endpoint_request(7);
        wait(5, SC_NS);

        if (!wait_for_acknowledge(7, 500.0)) {
            REG_ERROR(1, logger) << "test_endpoint_repeated_requests: Timeout on request " << req;
            all_passed = false;
            break;
        }

        received_data[req] = read_endpoint_data(7);
        REG_INFO(1, logger) << "test_endpoint_repeated_requests: request " << req
                             << " data = 0x" << std::hex << received_data[req];

        deassert_endpoint_request(7);
        wait(10, SC_NS);
    }

    // Verify all data values are unique (no duplication)
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            if (received_data[i] == received_data[j]) {
                REG_ERROR(1, logger) << "test_endpoint_repeated_requests: Duplicate data at requests "
                                      << i << " and " << j;
                all_passed = false;
            }
        }
    }

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_repeated_requests: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_repeated_requests: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 19: Endpoint Interface After Reset
// =============================================================================

bool test_edn_func_005::test_endpoint_interface_after_reset()
{
    REG_INFO(1, logger) << "Starting test_endpoint_interface_after_reset...";

    bool all_passed = true;

    // Enable EDN and service some requests
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    inject_entropy_to_buffer(0x11111111, 0x22222222, 0x33333333, 0x44444444, true);
    wait(5, SC_NS);

    assert_endpoint_request(2);
    wait(5, SC_NS);
    wait_for_acknowledge(2, 500.0);
    wait(10, SC_NS);

    // Apply system reset
    apply_reset(100.0);
    wait(10, SC_NS);

    // Verify all endpoint outputs cleared
    for (int i = 0; i < 8; i++) {
        if (edn_ack[i].read()) {
            REG_ERROR(1, logger) << "test_endpoint_interface_after_reset: edn_ack[" << i << "] not cleared by reset";
            all_passed = false;
        }
        if (edn_bus[i].read() != 0) {
            REG_ERROR(1, logger) << "test_endpoint_interface_after_reset: edn_bus[" << i << "] not cleared by reset";
            all_passed = false;
        }
        if (edn_fips[i].read()) {
            REG_ERROR(1, logger) << "test_endpoint_interface_after_reset: edn_fips[" << i << "] not cleared by reset";
            all_passed = false;
        }
    }

    // Verify endpoints operational after reset
    enable_edn_for_endpoints();
    wait(10, SC_NS);
    inject_entropy_to_buffer(0xABCDABCD, 0xEF01EF01, 0x23452345, 0x67896789, false);
    wait(5, SC_NS);

    assert_endpoint_request(4);
    wait(5, SC_NS);

    if (!wait_for_acknowledge(4, 500.0)) {
        REG_ERROR(1, logger) << "test_endpoint_interface_after_reset: Endpoints not functional after reset";
        all_passed = false;
    }

    deassert_endpoint_request(4);
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_interface_after_reset: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_interface_after_reset: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Test Case 20: Buffer Refill During Active Distribution
// =============================================================================

bool test_edn_func_005::test_endpoint_buffer_refill_during_distribution()
{
    REG_INFO(1, logger) << "Starting test_endpoint_buffer_refill_during_distribution...";

    bool all_passed = true;

    // Reset and enable
    apply_reset(100.0);
    wait(10, SC_NS);
    enable_edn_for_endpoints();
    wait(10, SC_NS);

    // Inject one 128-bit block (4 chunks) with FIPS=1
    inject_entropy_to_buffer(0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD, true);
    wait(5, SC_NS);

    // Assert concurrent requests from 6 endpoints
    for (int i = 0; i < 6; i++) {
        assert_endpoint_request(i);
    }
    wait(5, SC_NS);

    // Wait for 2 endpoints to be serviced
    int serviced_count = 0;
    for (int iteration = 0; iteration < 500; iteration++) {
        for (int ep = 0; ep < 6; ep++) {
            if (edn_ack[ep].read()) {
                serviced_count++;
                REG_INFO(1, logger) << "test_endpoint_buffer_refill_during_distribution: endpoint "
                                     << ep << " serviced (count=" << serviced_count << ")";
                wait(5, SC_NS);
            }
        }
        if (serviced_count >= 2) {
            break;
        }
        wait(1, SC_NS);
    }

    // Inject additional 128-bit block with FIPS=0 (different FIPS status)
    inject_entropy_to_buffer(0x10101010, 0x20202020, 0x30303030, 0x40404040, false);
    wait(5, SC_NS);

    // Wait for all 6 endpoints to be serviced
    serviced_count = 0;
    bool all_serviced[6] = {false};
    for (int iteration = 0; iteration < 1000; iteration++) {
        for (int ep = 0; ep < 6; ep++) {
            if (edn_ack[ep].read() && !all_serviced[ep]) {
                all_serviced[ep] = true;
                serviced_count++;
                REG_INFO(1, logger) << "test_endpoint_buffer_refill_during_distribution: endpoint "
                                     << ep << " final service (total=" << serviced_count << ")";
            }
        }
        if (serviced_count >= 6) {
            break;
        }
        wait(1, SC_NS);
    }

    // Verify all 6 endpoints were serviced
    for (int i = 0; i < 6; i++) {
        if (!all_serviced[i]) {
            REG_ERROR(1, logger) << "test_endpoint_buffer_refill_during_distribution: endpoint "
                                  << i << " not serviced";
            all_passed = false;
        }
    }

    // Deassert all requests
    for (int i = 0; i < 6; i++) {
        deassert_endpoint_request(i);
    }
    wait(10, SC_NS);

    if (all_passed) {
        REG_INFO(1, logger) << "test_endpoint_buffer_refill_during_distribution: PASSED";
    } else {
        REG_ERROR(1, logger) << "test_endpoint_buffer_refill_during_distribution: FAILED";
    }

    return all_passed;
}

// =============================================================================
// Helper Functions
// =============================================================================

void test_edn_func_005::inject_entropy_to_buffer(uint32_t chunk0, uint32_t chunk1, uint32_t chunk2, uint32_t chunk3, bool fips)
{
    uint32_t genbits[4] = {chunk0, chunk1, chunk2, chunk3};
    provide_csrng_entropy(genbits, fips);
}

void test_edn_func_005::assert_endpoint_request(unsigned int endpoint_id)
{
    edn_req[endpoint_id].write(true);
}

void test_edn_func_005::deassert_endpoint_request(unsigned int endpoint_id)
{
    edn_req[endpoint_id].write(false);
}

bool test_edn_func_005::wait_for_acknowledge(unsigned int endpoint_id, double timeout_ns)
{
    sc_time start_time = sc_time_stamp();
    sc_time timeout = sc_time(timeout_ns, SC_NS);

    while ((sc_time_stamp() - start_time) < timeout) {
        if (edn_ack[endpoint_id].read()) {
            return true;
        }
        wait(1, SC_NS);
    }

    return false;
}

uint32_t test_edn_func_005::read_endpoint_data(unsigned int endpoint_id)
{
    return edn_bus[endpoint_id].read().to_uint();
}

bool test_edn_func_005::read_endpoint_fips(unsigned int endpoint_id)
{
    return edn_fips[endpoint_id].read();
}

void test_edn_func_005::enable_edn_for_endpoints()
{
    // Configure CTRL.EDN_ENABLE = 0x6 (enable)
    // Use software port mode (BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9)
    uint32_t ctrl_value = 0x9996; // EDN_ENABLE=0x6, all other modes disabled
    register_write_32(CTRL_OFFSET, ctrl_value);
    notify_mock();
    wait(10, SC_NS);
}

void test_edn_func_005::disable_edn()
{
    // Configure CTRL.EDN_ENABLE = 0x9 (disable)
    uint32_t ctrl_value = 0x9999;
    register_write_32(CTRL_OFFSET, ctrl_value);
    wait(10, SC_NS);
}

bool test_edn_func_005::verify_value(const std::string& context, uint32_t expected, uint32_t actual)
{
    if (expected != actual) {
        REG_ERROR(1, logger) << context << ": Expected 0x" << std::hex << expected
                              << ", got 0x" << actual;
        return false;
    }
    return true;
}

void test_edn_func_005::report_test_result(const std::string& test_name, bool passed, const std::string& message)
{
    m_tests_run++;
    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "[PASS] " << test_name;
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(1, logger) << "[FAIL] " << test_name;
        if (!message.empty()) {
            REG_ERROR(1, logger) << "       " << message;
        }
    }
}
