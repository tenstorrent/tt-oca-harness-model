// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_011.h
 * @brief EDN_FUNC_011 Test Suite - FIPS Compliance Status Propagation Verification
 *
 * Comprehensive test suite for EDN_FUNC_011 (FIPS Compliance Status Propagation) covering:
 * - FIPS indicator propagation from CSRNG to peripheral endpoints
 * - Pre-FIPS (boot-time) vs. FIPS-approved entropy differentiation
 * - FIPS status consistency across operating modes (boot, auto, software port)
 * - FIPS indicator transitions when seed becomes FIPS-approved
 *
 * Test Coverage (4 new test cases + 2 existing in test_edn_func_005.cpp):
 *
 * **Existing Tests (in test_edn_func_005.cpp):**
 * - TC_EDN_FIPS_001: test_endpoint_fips_propagation_true() - FIPS=1 propagation
 * - TC_EDN_FIPS_002: test_endpoint_fips_propagation_false() - FIPS=0 propagation
 *
 * **New Tests (implemented in this file):**
 *
 * Boot Mode FIPS Tests (1 test):
 * - TC_EDN_FIPS_003: Boot Mode Pre-FIPS Indicator (test_boot_mode_pre_fips_indicator)
 *
 * Operating Mode FIPS Tests (2 tests):
 * - TC_EDN_FIPS_004: Auto Mode Entropy Distribution (test_auto_mode_entropy_distribution)
 * - TC_EDN_FIPS_005: SW Port Mode Entropy Distribution (test_sw_port_entropy_distribution)
 *
 * FIPS Transition Tests (1 test):
 * - TC_EDN_FIPS_006: FIPS Transition Pre-FIPS to Approved (test_fips_transition_pre_to_approved)
 *
 * @note This test suite implements 4 of the 6 test cases mapped to EDN_FUNC_011
 *       in the edn-functionality-testcases.md document. The other 2 tests already
 *       exist in test_edn_func_005.cpp.
 *
 * Implementation Strategy:
 * - Tests focus on FIPS indicator behavior even if full operating modes not implemented
 * - Each test validates edn_fips signals match CSRNG genbits FIPS status
 * - Tests verify FIPS status consistency across multiple endpoint requests
 * - Helper functions simplify entropy injection with specific FIPS values
 * - Assertions validate expected vs. observed FIPS indicators with diagnostics
 *
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_011
 * @brief Test fixture for EDN_FUNC_011 verification
 *
 * Extends edn_test to provide comprehensive FIPS compliance status propagation testing.
 * Implements 4 new test cases for EDN_FUNC_011 covering boot mode, operating modes,
 * and FIPS indicator transitions.
 */
class test_edn_func_011 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_011(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_011();

    /**
     * @brief Execute all EDN_FUNC_011 test cases
     * @return Number of failed tests
     *
     * Runs all 4 new test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 3: Boot Mode Pre-FIPS Indicator
    // =========================================================================
    /**
     * @brief Verify edn_fips signals de-asserted during boot-time request mode
     *
     * Test Objective: Validate that boot-time mode delivers pre-FIPS entropy
     * with edn_fips signals de-asserted (FIPS=0) since boot mode uses fast
     * seed without full NIST SP 800-90A health checks.
     *
     * Test Plan Reference: test_boot_mode_pre_fips_indicator (Test #39)
     * Functionality: EDN_FUNC_011 (FIPS Compliance Status Propagation)
     *
     * Procedure:
     * 1. Configure BOOT_INS_CMD and BOOT_GEN_CMD registers (if mode available)
     * 2. Enable boot-time mode (CTRL.BOOT_REQ_MODE = 0x6) or simulate pre-FIPS seed
     * 3. Inject entropy with FIPS=0 (pre-FIPS boot seed)
     * 4. Request entropy from multiple endpoints (0, 3, 7)
     * 5. Verify edn_fips[i] = 0 for all endpoints
     * 6. Verify edn_bus[i] contains valid entropy data
     * 7. Verify consistent FIPS=0 across all chunks from boot block
     *
     * Pass Criteria:
     * - All endpoints receive edn_fips = 0 (pre-FIPS indicator)
     * - Entropy data delivered correctly (32-bit chunks)
     * - FIPS indicator consistent across multiple endpoint requests
     * - No spurious FIPS=1 assertions during boot mode
     *
     * @note If boot mode not fully implemented, test focuses on FIPS=0 propagation
     *       using software port mode with pre-FIPS entropy injection.
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_pre_fips_indicator();

    // =========================================================================
    // Test Case 4: Auto Mode Entropy Distribution
    // =========================================================================
    /**
     * @brief Verify auto request mode distributes entropy with FIPS propagation
     *
     * Test Objective: Validate that auto request mode correctly propagates
     * FIPS-approved entropy indicator to endpoints during hardware-managed
     * automatic distribution.
     *
     * Test Plan Reference: test_auto_mode_entropy_distribution (Test #47)
     * Functionality: EDN_FUNC_011 (FIPS Compliance Status Propagation)
     *
     * Procedure:
     * 1. Configure GENERATE_CMD and RESEED_CMD FIFOs (if mode available)
     * 2. Set MAX_NUM_REQS_BETWEEN_RESEEDS counter
     * 3. Enable auto request mode (CTRL.AUTO_REQ_MODE = 0x6) or simulate auto behavior
     * 4. Inject FIPS-approved entropy (FIPS=1)
     * 5. Assert concurrent requests from multiple endpoints (0, 2, 5, 7)
     * 6. Verify all endpoints receive edn_fips = 1
     * 7. Verify data integrity and FIPS consistency across distribution
     *
     * Pass Criteria:
     * - All endpoints receive FIPS=1 indicator with FIPS-approved entropy
     * - Automatic distribution maintains FIPS status correctly
     * - No FIPS indicator corruption during concurrent endpoint service
     * - FIPS status matches source entropy block
     *
     * @note If auto mode not fully implemented, test focuses on FIPS=1 propagation
     *       with simulated automatic distribution using software port mode.
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_mode_entropy_distribution();

    // =========================================================================
    // Test Case 5: Software Port Mode Entropy Distribution
    // =========================================================================
    /**
     * @brief Verify software port mode distributes entropy with FIPS indicator
     *
     * Test Objective: Validate that firmware-controlled software port mode
     * correctly propagates FIPS indicator after generate command completion.
     *
     * Test Plan Reference: test_sw_port_entropy_distribution (Test #57)
     * Functionality: EDN_FUNC_011 (FIPS Compliance Status Propagation)
     *
     * Procedure:
     * 1. Enable software port mode (CTRL.EDN_ENABLE=0x6, no mode bits)
     * 2. Issue SW instantiate command via SW_CMD_REQ
     * 3. Poll SW_CMD_STS.CMD_ACK for completion
     * 4. Issue SW generate command with glen parameter
     * 5. Inject FIPS-approved entropy (FIPS=1) from CSRNG
     * 6. Request entropy from endpoints (1, 4, 6)
     * 7. Verify edn_fips[i] = 1 for all requesting endpoints
     * 8. Verify data correctness and FIPS consistency
     *
     * Pass Criteria:
     * - FIPS indicator propagates correctly in software port mode
     * - FIPS status available on endpoints after SW generate command
     * - Multiple endpoints receive consistent FIPS indicator
     * - edn_cmd_req_done interrupt generated on command completion
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_port_entropy_distribution();

    // =========================================================================
    // Test Case 6: FIPS Transition Pre-FIPS to Approved
    // =========================================================================
    /**
     * @brief Verify edn_fips signals transition from de-asserted to asserted
     *
     * Test Objective: Validate FIPS indicator transition when entropy source
     * transitions from pre-FIPS (fast boot seed) to FIPS-approved (full health checks).
     *
     * Test Plan Reference: test_fips_transition_pre_to_approved (Test #145)
     * Functionality: EDN_FUNC_011 (FIPS Compliance Status Propagation)
     *
     * Procedure:
     * 1. Enable EDN in software port mode
     * 2. Issue instantiate with pre-FIPS seed (simulated boot-time instantiate)
     * 3. Issue generate command and inject entropy with FIPS=0
     * 4. Request from endpoint 0, verify edn_fips[0] = 0
     * 5. Issue reseed command with FIPS-approved seed
     * 6. Inject entropy with FIPS=1 (FIPS-approved after reseed)
     * 7. Request from endpoint 0 again, verify edn_fips[0] = 1
     * 8. Request from additional endpoints (3, 5), verify all FIPS=1
     * 9. Verify FIPS transition is clean and persistent
     *
     * Pass Criteria:
     * - Initial entropy delivered with FIPS=0 (pre-FIPS)
     * - After reseed, entropy delivered with FIPS=1 (FIPS-approved)
     * - FIPS transition occurs cleanly without glitches
     * - All endpoints receive updated FIPS status after transition
     * - FIPS=1 persists for all subsequent entropy from FIPS-approved seed
     *
     * @return true if test passes, false otherwise
     */
    bool test_fips_transition_pre_to_approved();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Enable EDN in software port mode for FIPS testing
     *
     * Configures CTRL.EDN_ENABLE = 0x6 with no operating mode bits set,
     * placing EDN in software port mode for firmware-controlled testing.
     */
    void enable_software_port_mode();

    /**
     * @brief Inject 128-bit entropy block with specific FIPS status
     * @param chunk0 First 32-bit word (bits [127:96])
     * @param chunk1 Second 32-bit word (bits [95:64])
     * @param chunk2 Third 32-bit word (bits [63:32])
     * @param chunk3 Fourth 32-bit word (bits [31:0])
     * @param fips FIPS compliance indicator for this block
     *
     * Wrapper around provide_csrng_entropy for consistent entropy injection.
     */
    void inject_entropy_with_fips(uint32_t chunk0, uint32_t chunk1,
                                   uint32_t chunk2, uint32_t chunk3, bool fips);

    /**
     * @brief Verify FIPS indicator matches expected value for endpoint
     * @param endpoint_id Endpoint index (0-7)
     * @param expected_fips Expected FIPS indicator value (true=1, false=0)
     * @param context Test context string for error reporting
     * @return true if FIPS matches, false otherwise
     *
     * Reads edn_fips[endpoint_id] and compares to expected value.
     */
    bool verify_fips_indicator(unsigned int endpoint_id, bool expected_fips,
                                const std::string& context);

    /**
     * @brief Wait for endpoint acknowledge with timeout
     * @param endpoint_id Endpoint index (0-7)
     * @param timeout_ns Timeout in nanoseconds
     * @return true if ack received, false on timeout
     *
     * Polls edn_ack[endpoint_id] until asserted or timeout.
     */
    bool wait_for_endpoint_ack(unsigned int endpoint_id, double timeout_ns = 500.0);

    /**
     * @brief Issue SW instantiate command and wait for completion
     * @param fips_expected Expected FIPS status after instantiate
     * @return true if command succeeds, false otherwise
     *
     * Issues CSRNG instantiate command via SW_CMD_REQ and polls SW_CMD_STS.
     */
    bool issue_sw_instantiate_command(bool fips_expected = false);

    /**
     * @brief Issue SW generate command and wait for completion
     * @param glen Generate length parameter (default 0x1 for 1 block)
     * @return true if command succeeds, false otherwise
     *
     * Issues CSRNG generate command via SW_CMD_REQ and polls SW_CMD_STS.
     */
    bool issue_sw_generate_command(uint32_t glen = 0x1);

    /**
     * @brief Issue SW reseed command and wait for completion
     * @return true if command succeeds, false otherwise
     *
     * Issues CSRNG reseed command via SW_CMD_REQ to transition to FIPS-approved.
     */
    bool issue_sw_reseed_command();

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
    void report_test_result(const std::string& test_name, bool passed,
                            const std::string& message = "");

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;       ///< Total tests executed
    unsigned int m_tests_passed;    ///< Tests that passed
    unsigned int m_tests_failed;    ///< Tests that failed
    std::vector<std::string> m_failed_tests; ///< List of failed test names

    // =========================================================================
    // Register Offsets (from edn-memory-map-registers.md)
    // =========================================================================
    static constexpr unsigned int CTRL_OFFSET = 0x14;        ///< CTRL register offset
    static constexpr unsigned int SW_CMD_REQ_OFFSET = 0x20;  ///< SW_CMD_REQ register offset
    static constexpr unsigned int SW_CMD_STS_OFFSET = 0x24;  ///< SW_CMD_STS register offset
    static constexpr unsigned int BOOT_INS_CMD_OFFSET = 0x18; ///< BOOT_INS_CMD offset
    static constexpr unsigned int BOOT_GEN_CMD_OFFSET = 0x1C; ///< BOOT_GEN_CMD offset
    static constexpr unsigned int GENERATE_CMD_OFFSET = 0x30; ///< GENERATE_CMD offset
    static constexpr unsigned int RESEED_CMD_OFFSET = 0x2C;   ///< RESEED_CMD offset
    static constexpr unsigned int MAX_NUM_REQS_OFFSET = 0x34; ///< MAX_NUM_REQS_BETWEEN_RESEEDS offset

    // =========================================================================
    // CSRNG Command Encodings (from NIST SP 800-90A / CSRNG spec)
    // =========================================================================
    static constexpr uint32_t CSRNG_CMD_INSTANTIATE = 1;  ///< Instantiate command type
    static constexpr uint32_t CSRNG_CMD_RESEED = 3;       ///< Reseed command type
    static constexpr uint32_t CSRNG_CMD_GENERATE = 4;     ///< Generate command type
    static constexpr uint32_t CSRNG_CMD_UNINSTANTIATE = 5; ///< Uninstantiate command type
};
