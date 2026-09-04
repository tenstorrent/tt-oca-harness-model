// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_005.h
 * @brief EDN_FUNC_005 Test Suite - Endpoint Entropy Distribution Verification
 *
 * Comprehensive test suite for EDN_FUNC_005 (Endpoint Entropy Distribution) covering:
 * - Request/acknowledge handshake protocol for all 8 endpoints
 * - 32-bit data distribution from internal buffer to endpoints
 * - FIPS compliance indicator propagation (edn_fips signals)
 * - Round-robin arbitration for concurrent endpoint requests
 * - Data persistence on bus for asynchronous consumption
 * - 128-bit to 32-bit width conversion correctness
 * - Buffer empty conditions and handling
 * - Corner cases and error scenarios
 *
 * Test Coverage (20 test cases):
 *
 * Endpoint Interface Tests (6 tests):
 * - TC_EDN_ENDPOINT_001: Single Endpoint Request - Endpoint 0
 * - TC_EDN_ENDPOINT_002: Single Endpoint Request - Endpoint 7
 * - TC_EDN_ENDPOINT_003: Request/Acknowledge Handshake Timing
 * - TC_EDN_ENDPOINT_004: Data Bus Validity During Acknowledge
 * - TC_EDN_ENDPOINT_005: FIPS Indicator Propagation (FIPS=1)
 * - TC_EDN_ENDPOINT_006: FIPS Indicator Propagation (FIPS=0)
 *
 * Multiple Endpoint Tests (4 tests):
 * - TC_EDN_ENDPOINT_007: Concurrent Requests - 2 Endpoints
 * - TC_EDN_ENDPOINT_008: Concurrent Requests - All 8 Endpoints
 * - TC_EDN_ENDPOINT_009: Round-Robin Arbitration Order
 * - TC_EDN_ENDPOINT_010: Arbitration Fairness (No Starvation)
 *
 * Data Distribution Tests (4 tests):
 * - TC_EDN_ENDPOINT_011: 128-bit to 32-bit Conversion Correctness
 * - TC_EDN_ENDPOINT_012: Sequential Chunk Distribution (4 requests from 1 endpoint)
 * - TC_EDN_ENDPOINT_013: Data Persistence on Bus After Acknowledge
 * - TC_EDN_ENDPOINT_014: Buffer Empty Condition (No Entropy Available)
 *
 * Error/Corner Case Tests (6 tests):
 * - TC_EDN_ENDPOINT_015: Request During EDN Disabled
 * - TC_EDN_ENDPOINT_016: Request with Empty Entropy Buffer
 * - TC_EDN_ENDPOINT_017: Simultaneous Request and Acknowledge
 * - TC_EDN_ENDPOINT_018: Repeated Requests from Same Endpoint
 * - TC_EDN_ENDPOINT_019: Endpoint Interface After Reset
 * - TC_EDN_ENDPOINT_020: Buffer Refill During Active Distribution
 *
 * @note This test suite implements all 20 test cases mapped to EDN_FUNC_005
 *       in the edn-test-plan.md document.
 *
 * Implementation Strategy:
 * - Each test case is self-contained with setup, execution, and validation phases
 * - Tests use edn_test infrastructure for endpoint signaling and entropy injection
 * - Helper functions simplify entropy buffer population and endpoint interaction
 * - Assertions validate expected vs. observed behavior with detailed diagnostics
 * - Failed tests report clear error messages for debugging
 *
 * @date 2026-01-13
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_005
 * @brief Test fixture for EDN_FUNC_005 verification
 *
 * Extends edn_test to provide comprehensive endpoint entropy distribution testing.
 * Implements all 20 test cases for EDN_FUNC_005 covering handshake protocol,
 * arbitration, data correctness, and FIPS propagation.
 */
class test_edn_func_005 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_005(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_005();

    /**
     * @brief Execute all EDN_FUNC_005 test cases
     * @return Number of failed tests
     *
     * Runs all 20 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Single Endpoint Request - Endpoint 0
    // =========================================================================
    /**
     * @brief Verify single request from endpoint 0 with entropy delivery
     *
     * Test Objective: Validate basic request/acknowledge handshake for endpoint 0
     * with correct 32-bit data delivery and FIPS propagation.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_001
     * Functionality: EDN_FUNC_005 (Endpoint Entropy Distribution)
     *
     * Procedure:
     * 1. Enable EDN module (CTRL.EDN_ENABLE = 0x6)
     * 2. Inject 128-bit entropy block with FIPS=1 into buffer
     * 3. Assert edn_req[0] = 1 (endpoint 0 requests)
     * 4. Wait for edn_ack[0] assertion
     * 5. Verify edn_bus[0] contains first 32-bit chunk
     * 6. Verify edn_fips[0] = 1
     * 7. Deassert edn_req[0] = 0
     * 8. Verify edn_ack[0] deasserts
     *
     * Pass Criteria:
     * - edn_ack[0] asserts within timeout
     * - edn_bus[0] matches expected chunk (bits[127:96] of 128-bit block)
     * - edn_fips[0] = 1 (FIPS compliant)
     * - Handshake completes cleanly
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_single_request_ep0();

    // =========================================================================
    // Test Case 2: Single Endpoint Request - Endpoint 7
    // =========================================================================
    /**
     * @brief Verify single request from endpoint 7 with entropy delivery
     *
     * Test Objective: Validate endpoint 7 functionality to ensure all endpoints
     * work correctly (test boundary endpoint).
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_002
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN module
     * 2. Inject 128-bit entropy block with FIPS=0 into buffer
     * 3. Assert edn_req[7] = 1
     * 4. Wait for edn_ack[7] assertion
     * 5. Verify edn_bus[7] contains correct 32-bit chunk
     * 6. Verify edn_fips[7] = 0 (pre-FIPS entropy)
     * 7. Deassert edn_req[7]
     * 8. Verify handshake completes
     *
     * Pass Criteria:
     * - Endpoint 7 functions identically to endpoint 0
     * - FIPS=0 propagates correctly
     * - All handshake signals behave as expected
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_single_request_ep7();

    // =========================================================================
    // Test Case 3: Request/Acknowledge Handshake Timing
    // =========================================================================
    /**
     * @brief Verify correct timing sequence of request/acknowledge handshake
     *
     * Test Objective: Validate handshake protocol timing and sequencing per
     * TLM-2.0 abstractions.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_003
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject entropy
     * 2. Assert edn_req[2] = 1
     * 3. Monitor edn_ack[2] for rising edge
     * 4. Verify edn_ack[2] does not assert before request
     * 5. Verify edn_ack[2] asserts after request
     * 6. Verify data and FIPS signals valid when ack asserts
     * 7. Deassert edn_req[2]
     * 8. Verify edn_ack[2] deasserts after request deasserts
     *
     * Pass Criteria:
     * - Acknowledge never asserts before request
     * - Acknowledge asserts only when data is valid
     * - Acknowledge deasserts after request deasserts
     * - No spurious acknowledge pulses
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_handshake_timing();

    // =========================================================================
    // Test Case 4: Data Bus Validity During Acknowledge
    // =========================================================================
    /**
     * @brief Verify edn_bus data is stable and valid during acknowledge
     *
     * Test Objective: Validate that data bus contains correct entropy value
     * when acknowledge is asserted.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_004
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject known entropy pattern (0x11223344_55667788_99AABBCC_DDEEFF00)
     * 2. Request from endpoint 3
     * 3. Sample edn_bus[3] when edn_ack[3] = 1
     * 4. Verify data matches expected chunk (0x11223344)
     * 5. Hold request and verify bus remains stable
     * 6. Deassert request and verify bus persists
     *
     * Pass Criteria:
     * - Bus value is stable during acknowledge assertion
     * - Bus contains correct 32-bit chunk from 128-bit block
     * - No glitches or invalid data on bus
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_data_bus_validity();

    // =========================================================================
    // Test Case 5: FIPS Indicator Propagation (FIPS=1)
    // =========================================================================
    /**
     * @brief Verify FIPS compliance indicator propagates correctly for FIPS=1
     *
     * Test Objective: Validate that FIPS-approved entropy is correctly indicated
     * on edn_fips outputs.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_005
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN
     * 2. Inject entropy with FIPS=1 (FIPS-approved)
     * 3. Request from multiple endpoints (0, 2, 5)
     * 4. Verify each endpoint's edn_fips signal = 1
     * 5. Verify FIPS status persists for all chunks from same 128-bit block
     *
     * Pass Criteria:
     * - All endpoints receive FIPS=1 indicator
     * - FIPS status consistent across all chunks from same block
     * - No FIPS indicator corruption
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_fips_propagation_true();

    // =========================================================================
    // Test Case 6: FIPS Indicator Propagation (FIPS=0)
    // =========================================================================
    /**
     * @brief Verify FIPS indicator correctly shows pre-FIPS entropy (FIPS=0)
     *
     * Test Objective: Validate that pre-FIPS (boot-time) entropy is correctly
     * indicated on edn_fips outputs.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_006
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN
     * 2. Inject entropy with FIPS=0 (pre-FIPS)
     * 3. Request from multiple endpoints (1, 4, 6)
     * 4. Verify each endpoint's edn_fips signal = 0
     * 5. Verify FIPS=0 persists for all chunks
     *
     * Pass Criteria:
     * - All endpoints receive FIPS=0 indicator
     * - Pre-FIPS status consistent across chunks
     * - Clear distinction between FIPS and pre-FIPS entropy
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_fips_propagation_false();

    // =========================================================================
    // Test Case 7: Concurrent Requests - 2 Endpoints
    // =========================================================================
    /**
     * @brief Verify handling of concurrent requests from 2 endpoints
     *
     * Test Objective: Validate arbitration and fair service when 2 endpoints
     * request simultaneously.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_007
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject sufficient entropy (2+ chunks)
     * 2. Simultaneously assert edn_req[0] and edn_req[1]
     * 3. Monitor for first acknowledge (either endpoint)
     * 4. Verify first endpoint receives data
     * 5. Wait for second acknowledge
     * 6. Verify second endpoint receives data
     * 7. Verify both endpoints receive unique entropy chunks
     *
     * Pass Criteria:
     * - Both endpoints receive acknowledgments
     * - No endpoint is ignored (fairness)
     * - Each endpoint receives unique data
     * - No data corruption or duplication
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_concurrent_two_requests();

    // =========================================================================
    // Test Case 8: Concurrent Requests - All 8 Endpoints
    // =========================================================================
    /**
     * @brief Verify handling of concurrent requests from all 8 endpoints
     *
     * Test Objective: Validate arbitration and service for maximum concurrent
     * endpoint activity.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_008
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject sufficient entropy (2x 128-bit blocks = 8 chunks)
     * 2. Simultaneously assert all 8 edn_req signals
     * 3. Monitor all 8 edn_ack signals
     * 4. Verify all endpoints receive acknowledgments in sequence
     * 5. Verify each endpoint receives unique entropy chunk
     * 6. Track service order
     *
     * Pass Criteria:
     * - All 8 endpoints serviced
     * - No endpoint starvation
     * - Each endpoint receives unique 32-bit chunk
     * - Service order follows round-robin arbitration
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_concurrent_all_eight();

    // =========================================================================
    // Test Case 9: Round-Robin Arbitration Order
    // =========================================================================
    /**
     * @brief Verify round-robin arbitration order is maintained
     *
     * Test Objective: Validate that arbitration follows round-robin algorithm
     * starting from last serviced endpoint.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_009
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject entropy
     * 2. Service endpoint 3 (sets arb index to 4)
     * 3. Assert concurrent requests from endpoints 0, 2, 5, 7
     * 4. Verify service order: 5 → 7 → 0 → 2 (starting from arb index 4)
     * 5. Verify round-robin wraps correctly
     *
     * Pass Criteria:
     * - Service order matches round-robin algorithm
     * - Arbitration index updates correctly after each service
     * - Wrap-around from endpoint 7 to 0 works correctly
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_round_robin_order();

    // =========================================================================
    // Test Case 10: Arbitration Fairness (No Starvation)
    // =========================================================================
    /**
     * @brief Verify no endpoint starvation under sustained concurrent load
     *
     * Test Objective: Validate long-term fairness ensuring all endpoints
     * receive service over time.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_010
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject abundant entropy
     * 2. Maintain concurrent requests from all 8 endpoints
     * 3. Count service occurrences per endpoint over 16 transactions
     * 4. Verify each endpoint serviced exactly twice (fair distribution)
     * 5. Verify no endpoint serviced significantly more/less than others
     *
     * Pass Criteria:
     * - All endpoints receive service
     * - Service distribution is fair (±1 transaction variance)
     * - No endpoint starvation over extended operation
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_arbitration_fairness();

    // =========================================================================
    // Test Case 11: 128-bit to 32-bit Conversion Correctness
    // =========================================================================
    /**
     * @brief Verify correct width conversion from 128-bit CSRNG to 32-bit endpoints
     *
     * Test Objective: Validate that 128-bit entropy block is correctly split
     * into four 32-bit chunks in proper order.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_011
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN
     * 2. Inject known 128-bit pattern: genbits[0]=0xAABBCCDD, genbits[1]=0x11223344,
     *    genbits[2]=0x55667788, genbits[3]=0x99AABBCC
     * 3. Request 4 times from same endpoint
     * 4. Verify chunk order: [0]=0xAABBCCDD, [1]=0x11223344, [2]=0x55667788, [3]=0x99AABBCC
     * 5. Verify MSB-first chunk ordering
     *
     * Pass Criteria:
     * - All 4 chunks delivered in correct order
     * - Chunk values match source 128-bit block exactly
     * - No bit corruption during width conversion
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_width_conversion();

    // =========================================================================
    // Test Case 12: Sequential Chunk Distribution (4 requests from 1 endpoint)
    // =========================================================================
    /**
     * @brief Verify sequential chunk delivery from single 128-bit block
     *
     * Test Objective: Validate that single endpoint can consume all 4 chunks
     * from one 128-bit block sequentially.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_012
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject one 128-bit block
     * 2. Request from endpoint 4, receive chunk 0
     * 3. Request again from endpoint 4, receive chunk 1
     * 4. Request again from endpoint 4, receive chunk 2
     * 5. Request again from endpoint 4, receive chunk 3
     * 6. Verify 5th request finds buffer empty (no acknowledge)
     *
     * Pass Criteria:
     * - All 4 chunks delivered sequentially to same endpoint
     * - Chunks delivered in correct order
     * - Buffer empty after 4 chunks consumed
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_sequential_chunks();

    // =========================================================================
    // Test Case 13: Data Persistence on Bus After Acknowledge
    // =========================================================================
    /**
     * @brief Verify data persists on edn_bus for asynchronous consumption
     *
     * Test Objective: Validate that data and FIPS indicators remain stable
     * on bus after acknowledge deasserts to support async peripherals.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_013
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject entropy
     * 2. Request from endpoint 1
     * 3. Capture edn_bus[1] and edn_fips[1] values when ack asserts
     * 4. Deassert edn_req[1]
     * 5. Wait 100ns (async peripheral processing delay)
     * 6. Verify edn_bus[1] and edn_fips[1] still hold same values
     * 7. Issue new request from different endpoint
     * 8. Verify endpoint 1 bus persists until next request on endpoint 1
     *
     * Pass Criteria:
     * - Data persists on bus after acknowledge deasserts
     * - FIPS indicator persists with data
     * - Persistence allows asynchronous peripheral consumption
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_data_persistence();

    // =========================================================================
    // Test Case 14: Buffer Empty Condition (No Entropy Available)
    // =========================================================================
    /**
     * @brief Verify correct handling when entropy buffer is empty
     *
     * Test Objective: Validate that endpoint requests are NOT acknowledged
     * when no entropy is available in buffer.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_014
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN with empty buffer (no entropy injection)
     * 2. Assert edn_req[5] = 1
     * 3. Wait reasonable timeout (200ns)
     * 4. Verify edn_ack[5] does NOT assert
     * 5. Inject entropy into buffer
     * 6. Verify edn_ack[5] now asserts
     * 7. Verify data delivery completes
     *
     * Pass Criteria:
     * - No acknowledge when buffer empty
     * - Request remains pending (not lost)
     * - Acknowledge occurs immediately after entropy becomes available
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_buffer_empty();

    // =========================================================================
    // Test Case 15: Request During EDN Disabled
    // =========================================================================
    /**
     * @brief Verify endpoint requests ignored when EDN is disabled
     *
     * Test Objective: Validate that endpoint interface is inactive when
     * CTRL.EDN_ENABLE = 0x9 (disabled).
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_015
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Ensure EDN is disabled (CTRL.EDN_ENABLE = 0x9)
     * 2. Inject entropy into buffer (should not matter)
     * 3. Assert edn_req[6] = 1
     * 4. Wait timeout (200ns)
     * 5. Verify edn_ack[6] does NOT assert
     * 6. Enable EDN (CTRL.EDN_ENABLE = 0x6)
     * 7. Verify request is now serviced
     *
     * Pass Criteria:
     * - No endpoint service when EDN disabled
     * - Endpoints activate immediately upon EDN enable
     * - No spurious acknowledges during disabled state
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_request_while_disabled();

    // =========================================================================
    // Test Case 16: Request with Empty Entropy Buffer
    // =========================================================================
    /**
     * @brief Verify request/buffer empty interaction (duplicate of TC 14 with focus)
     *
     * Test Objective: Focused test on buffer empty condition with timing analysis.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_016
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN, verify buffer is empty
     * 2. Assert edn_req[0] = 1
     * 3. Verify edn_ack[0] = 0 (no acknowledge)
     * 4. After delay, inject one chunk (via provide_genbits)
     * 5. Verify edn_ack[0] = 1 immediately
     * 6. Verify data delivered correctly
     *
     * Pass Criteria:
     * - Request waits patiently for entropy
     * - No timeout or request cancellation
     * - Immediate service upon entropy availability
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_empty_buffer_request();

    // =========================================================================
    // Test Case 17: Simultaneous Request and Acknowledge
    // =========================================================================
    /**
     * @brief Verify behavior when request changes during acknowledge phase
     *
     * Test Objective: Corner case testing for concurrent handshake signals.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_017
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject entropy
     * 2. Assert edn_req[3] = 1
     * 3. Wait for edn_ack[3] = 1
     * 4. Immediately deassert edn_req[3] = 0 (minimal hold time)
     * 5. Verify handshake completes cleanly
     * 6. Verify no data corruption
     * 7. Test repeated rapid request/deassert cycles
     *
     * Pass Criteria:
     * - Handshake robust to rapid signal changes
     * - Data integrity maintained
     * - No spurious acknowledges or glitches
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_simultaneous_req_ack();

    // =========================================================================
    // Test Case 18: Repeated Requests from Same Endpoint
    // =========================================================================
    /**
     * @brief Verify single endpoint can make multiple sequential requests
     *
     * Test Objective: Validate repeated use of single endpoint over time.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_018
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject abundant entropy (4+ 128-bit blocks)
     * 2. Loop 10 times:
     *    a. Request from endpoint 7
     *    b. Wait for acknowledge
     *    c. Capture data
     *    d. Deassert request
     *    e. Verify unique data each iteration
     * 3. Verify all 10 requests served successfully
     *
     * Pass Criteria:
     * - All repeated requests serviced
     * - Each request receives unique entropy chunk
     * - No endpoint state corruption across requests
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_repeated_requests();

    // =========================================================================
    // Test Case 19: Endpoint Interface After Reset
    // =========================================================================
    /**
     * @brief Verify endpoint interface returns to safe state after reset
     *
     * Test Objective: Validate reset clears endpoint state and outputs.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_019
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN, service some endpoint requests
     * 2. Apply system reset (rst_ni = 0)
     * 3. Verify all edn_ack[i] = 0
     * 4. Verify all edn_bus[i] = 0
     * 5. Verify all edn_fips[i] = 0
     * 6. Deassert reset
     * 7. Verify endpoints operational after reset
     *
     * Pass Criteria:
     * - All endpoint outputs cleared by reset
     * - No residual state from pre-reset activity
     * - Endpoints functional immediately after reset release
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_interface_after_reset();

    // =========================================================================
    // Test Case 20: Buffer Refill During Active Distribution
    // =========================================================================
    /**
     * @brief Verify buffer refill while endpoints are actively requesting
     *
     * Test Objective: Validate seamless transition when buffer refilled
     * during ongoing endpoint service.
     *
     * Test Plan Reference: TC_EDN_ENDPOINT_020
     * Functionality: EDN_FUNC_005
     *
     * Procedure:
     * 1. Enable EDN and inject one 128-bit block (4 chunks)
     * 2. Assert concurrent requests from 6 endpoints
     * 3. After 2 endpoints serviced, inject additional 128-bit block
     * 4. Verify all 6 endpoints receive service
     * 5. Verify seamless transition between first and second blocks
     * 6. Verify FIPS status can change between blocks
     *
     * Pass Criteria:
     * - All endpoints serviced without interruption
     * - Buffer refill does not disrupt active distribution
     * - FIPS status correctly reflects source block for each chunk
     *
     * @return true if test passes, false otherwise
     */
    bool test_endpoint_buffer_refill_during_distribution();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Inject 128-bit entropy block into EDN buffer
     * @param chunk0 First 32-bit word (bits [127:96])
     * @param chunk1 Second 32-bit word (bits [95:64])
     * @param chunk2 Third 32-bit word (bits [63:32])
     * @param chunk3 Fourth 32-bit word (bits [31:0])
     * @param fips FIPS compliance indicator for this block
     *
     * Helper function to populate entropy buffer with known test pattern.
     */
    void inject_entropy_to_buffer(uint32_t chunk0, uint32_t chunk1, uint32_t chunk2, uint32_t chunk3, bool fips);

    /**
     * @brief Assert request signal for specific endpoint
     * @param endpoint_id Endpoint index (0-7)
     *
     * Drives edn_req[endpoint_id] = 1 and waits for signal propagation.
     */
    void assert_endpoint_request(unsigned int endpoint_id);

    /**
     * @brief Deassert request signal for specific endpoint
     * @param endpoint_id Endpoint index (0-7)
     *
     * Drives edn_req[endpoint_id] = 0 and waits for signal propagation.
     */
    void deassert_endpoint_request(unsigned int endpoint_id);

    /**
     * @brief Wait for acknowledge signal from endpoint with timeout
     * @param endpoint_id Endpoint index (0-7)
     * @param timeout_ns Timeout in nanoseconds (default 500ns)
     * @return true if acknowledge received within timeout, false otherwise
     *
     * Polls edn_ack[endpoint_id] with timeout protection.
     */
    bool wait_for_acknowledge(unsigned int endpoint_id, double timeout_ns = 500.0);

    /**
     * @brief Read current data value from endpoint bus
     * @param endpoint_id Endpoint index (0-7)
     * @return 32-bit data value from edn_bus[endpoint_id]
     *
     * Samples current edn_bus value for verification.
     */
    uint32_t read_endpoint_data(unsigned int endpoint_id);

    /**
     * @brief Read current FIPS status from endpoint
     * @param endpoint_id Endpoint index (0-7)
     * @return FIPS indicator value from edn_fips[endpoint_id]
     *
     * Samples current edn_fips value for verification.
     */
    bool read_endpoint_fips(unsigned int endpoint_id);

    /**
     * @brief Enable EDN module for endpoint testing
     *
     * Configures CTRL.EDN_ENABLE = 0x6 to activate EDN for endpoint operations.
     * Uses software port mode (no boot/auto mode).
     */
    void enable_edn_for_endpoints();

    /**
     * @brief Disable EDN module
     *
     * Configures CTRL.EDN_ENABLE = 0x9 to deactivate EDN.
     */
    void disable_edn();

    /**
     * @brief Verify register value matches expected value
     * @param reg_name Register name for diagnostics
     * @param expected Expected value
     * @param actual Actual read value
     * @return true if values match, false otherwise
     */
    bool verify_value(const std::string& context, uint32_t expected, uint32_t actual);

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
