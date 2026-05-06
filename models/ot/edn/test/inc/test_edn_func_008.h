/**
 * @file test_edn_func_008.h
 * @brief EDN_FUNC_008 Test Suite Header - Entropy Bus Consistency Checking
 *
 * Comprehensive test suite for verifying entropy bus consistency checking
 * functionality (EDN_FUNC_008) per the EDN detailed design specification.
 *
 * Functionality Under Test:
 * - Consecutive 128-bit genbits comparison (all 4 words must match)
 * - Detection of identical consecutive entropy values from CSRNG
 * - RECOV_ALERT_STS.EDN_BUS_CMP_ALERT (bit 12) assertion on match
 * - alert_recov_alert signal assertion (recoverable alert)
 * - W0C clearing mechanism for the alert bit
 * - State tracking with m_prev_genbits[4] and m_prev_genbits_valid flag
 * - Reset clearing of state
 * - Entropy distribution continues despite alert (non-blocking)
 *
 * Test Coverage Matrix:
 * - TC1: Normal Operation - Consecutive Different Values (No Alert)
 * - TC2: First Genbits After Reset/Enable (No Comparison, No Alert)
 * - TC3: Recoverable Alert on Consecutive Identical Genbits
 * - TC4: W0C Clearing Mechanism for EDN_BUS_CMP_ALERT
 * - TC5: Alert Signal Assertion and Persistence
 * - TC6: Multiple Consecutive Matches (Alert Per Match)
 * - TC7: Partial Match (Different 4th Word) - No Alert
 * - TC8: Reset Clearing of Consistency Check State
 * - TC9: Entropy Distribution Continues During Alert
 * - TC10: Alert Clearing Does Not Clear m_prev_genbits State
 * - TC11: Zero Values Consecutive Match Detection
 * - TC12: All-Ones Values Consecutive Match Detection
 *
 * Architecture Alignment:
 * - RECOV_ALERT_STS register (offset 0x38, bit 12)
 * - alert_recov_alert port signal
 * - receive_csrng_entropy() method
 * - check_entropy_bus_consistency() private method
 *
 * Reference Documents:
 * - edn-detailed-design.md: Section 1.10 (Entropy Bus Consistency Checking)
 * - edn-test-plan.md: Tests 87-89
 * - edn-functionality-testcases.md: EDN_FUNC_008 mapping
 * - edn-architecture-behaviour-map.json: RECOV_ALERT_STS.EDN_BUS_CMP_ALERT
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"

/**
 * @class test_edn_func_008
 * @brief Test harness for EDN_FUNC_008 (Entropy Bus Consistency Checking)
 *
 * Provides comprehensive verification of the entropy bus consistency checking
 * feature which detects consecutive duplicate 128-bit entropy values from CSRNG,
 * indicating potential bus faults, data corruption, or entropy source degradation.
 *
 * Test Execution Pattern:
 * 1. Reset DUT to clear consistency check state
 * 2. Configure EDN for software port mode (enable entropy flow)
 * 3. Send consecutive genbits values via CSRNG interface
 * 4. Verify RECOV_ALERT_STS.EDN_BUS_CMP_ALERT behavior
 * 5. Verify alert_recov_alert signal behavior
 * 6. Test W0C clearing mechanism
 * 7. Verify entropy distribution continues despite alert
 *
 * Key Verification Points:
 * - All 4 words (128 bits) must match for alert
 * - First genbits after reset has no previous value (no comparison)
 * - Alert is recoverable (does not block entropy distribution)
 * - W0C mechanism clears alert status bit
 * - Alert signal follows register bit state
 * - State persists across alert clearing (m_prev_genbits retained)
 */
class test_edn_func_008 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test suite for entropy bus consistency checking verification.
     */
    test_edn_func_008(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_008();

    /**
     * @brief Execute all EDN_FUNC_008 test cases
     * @return Number of failed tests (0 = all passed)
     *
     * Runs all 12 test cases covering entropy bus consistency checking.
     * Each test is isolated with reset between executions.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case Implementations
    // =========================================================================

    /**
     * @brief TC1: Normal operation with consecutive different values
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that sending consecutive genbits with different values does NOT
     * trigger the consistency check alert.
     *
     * Test Steps:
     * 1. Enable EDN in software port mode
     * 2. Send first genbits value (e.g., 0x11111111_22222222_33333333_44444444)
     * 3. Send second genbits value (different: 0xAAAAAAAA_BBBBBBBB_CCCCCCCC_DDDDDDDD)
     * 4. Verify RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 0
     * 5. Verify alert_recov_alert signal = 0
     *
     * Pass Criteria:
     * - No alert triggered when consecutive values differ
     * - RECOV_ALERT_STS.EDN_BUS_CMP_ALERT remains 0
     * - alert_recov_alert remains deasserted
     */
    bool test_consecutive_different_values();

    /**
     * @brief TC2: First genbits after reset (no previous value)
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that the first genbits value received after reset does NOT
     * trigger alert (no previous value to compare against).
     *
     * Test Steps:
     * 1. Apply reset (clears m_prev_genbits_valid = false)
     * 2. Enable EDN in software port mode
     * 3. Send first genbits value
     * 4. Verify RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 0
     * 5. Verify alert_recov_alert signal = 0
     * 6. Send second genbits with SAME value as first
     * 7. Verify alert IS triggered (now have previous value)
     *
     * Pass Criteria:
     * - First genbits after reset never triggers alert
     * - m_prev_genbits_valid flag properly set after first genbits
     * - Second genbits with same value DOES trigger alert
     */
    bool test_first_genbits_after_reset();

    /**
     * @brief TC3: Recoverable alert on consecutive identical genbits
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that consecutive identical 128-bit genbits values trigger
     * the EDN_BUS_CMP_ALERT recoverable alert.
     *
     * Test Steps:
     * 1. Enable EDN in software port mode
     * 2. Send first genbits: 0x12345678_9ABCDEF0_FEDCBA09_87654321
     * 3. Wait for processing
     * 4. Send second genbits: SAME as first (all 4 words match)
     * 5. Wait 1 delta cycle for alert propagation
     * 6. Read RECOV_ALERT_STS register
     * 7. Verify bit 12 (EDN_BUS_CMP_ALERT) = 1
     * 8. Verify alert_recov_alert signal = 1
     *
     * Pass Criteria:
     * - RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 1 after match
     * - alert_recov_alert signal asserted
     * - Alert occurs within 1 delta cycle of second genbits
     */
    bool test_alert_on_consecutive_match();

    /**
     * @brief TC4: W0C clearing mechanism for EDN_BUS_CMP_ALERT
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify Write-0-to-Clear (W0C) mechanism correctly clears the
     * EDN_BUS_CMP_ALERT bit in RECOV_ALERT_STS register.
     *
     * Test Steps:
     * 1. Trigger alert (send consecutive matching genbits)
     * 2. Verify RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 1
     * 3. Write 0 to bit 12 of RECOV_ALERT_STS (W0C)
     * 4. Wait 1 delta cycle for propagation
     * 5. Read RECOV_ALERT_STS register
     * 6. Verify EDN_BUS_CMP_ALERT = 0 (cleared)
     * 7. Verify alert_recov_alert signal = 0 (deasserted)
     * 8. Verify writing 1 to bit 12 does NOT set the bit (W0C semantics)
     *
     * Pass Criteria:
     * - Writing 0 to bit 12 clears the alert status
     * - Alert signal follows register bit state
     * - Writing 1 has no effect (W0C, not RW)
     * - Other RECOV_ALERT_STS bits unaffected
     */
    bool test_w0c_clearing_mechanism();

    /**
     * @brief TC5: Alert signal assertion and persistence
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify alert_recov_alert signal correctly asserts when alert triggered
     * and persists until firmware clears the status bit.
     *
     * Test Steps:
     * 1. Enable EDN, send different genbits → verify alert = 0
     * 2. Send matching genbits → verify alert = 1
     * 3. Wait 100ns → verify alert still = 1 (persistent)
     * 4. Clear RECOV_ALERT_STS.EDN_BUS_CMP_ALERT via W0C
     * 5. Wait 1 delta cycle → verify alert = 0 (deasserted)
     * 6. Verify alert remains 0 until next consistency error
     *
     * Pass Criteria:
     * - Alert asserts within 1 delta cycle of match detection
     * - Alert persists until firmware clears status bit
     * - Alert deasserts immediately after W0C clear
     * - Level-sensitive behavior (not pulsed)
     */
    bool test_alert_signal_persistence();

    /**
     * @brief TC6: Multiple consecutive matches
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify multiple consecutive matching genbits trigger alert each time
     * (if alert cleared between matches).
     *
     * Test Steps:
     * 1. Enable EDN, send genbits A
     * 2. Send matching genbits A → verify alert = 1
     * 3. Clear alert via W0C
     * 4. Send genbits A again (third consecutive match) → verify alert = 1
     * 5. Clear alert via W0C
     * 6. Send different genbits B → verify alert = 0
     * 7. Send matching genbits B → verify alert = 1
     *
     * Pass Criteria:
     * - Each consecutive match triggers alert
     * - Alert can be cleared and retriggered multiple times
     * - m_prev_genbits correctly updated after each genbits
     */
    bool test_multiple_consecutive_matches();

    /**
     * @brief TC7: Partial match (only 3 of 4 words match)
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that partial matches (not all 4 words identical) do NOT
     * trigger the consistency check alert.
     *
     * Test Steps:
     * 1. Send genbits: 0xAAAAAAAA_BBBBBBBB_CCCCCCCC_DDDDDDDD
     * 2. Send genbits: 0xAAAAAAAA_BBBBBBBB_CCCCCCCC_EEEEEEEE (4th word differs)
     * 3. Verify RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 0
     * 4. Test all 4 positions (word 0, 1, 2, 3 differs)
     * 5. Test multiple words differ
     *
     * Pass Criteria:
     * - Alert only triggers when ALL 4 words match
     * - Single word difference prevents alert
     * - Any word position difference prevents alert
     */
    bool test_partial_match_no_alert();

    /**
     * @brief TC8: Reset clearing of consistency check state
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that reset clears m_prev_genbits_valid flag and
     * m_prev_genbits state, preventing false alerts after reset.
     *
     * Test Steps:
     * 1. Enable EDN, send genbits A
     * 2. Send matching genbits A → trigger alert
     * 3. Apply reset (without clearing alert via W0C)
     * 4. Verify RECOV_ALERT_STS = 0 (reset clears all status)
     * 5. Verify alert_recov_alert = 0
     * 6. Send genbits A again (same as pre-reset value)
     * 7. Verify alert = 0 (no previous value after reset)
     * 8. Send genbits A again → verify alert = 1 (now triggers)
     *
     * Pass Criteria:
     * - Reset clears RECOV_ALERT_STS.EDN_BUS_CMP_ALERT
     * - Reset clears m_prev_genbits_valid flag
     * - First genbits after reset never triggers alert
     */
    bool test_reset_clears_state();

    /**
     * @brief TC9: Entropy distribution continues during alert
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that entropy distribution to endpoints continues normally
     * even when EDN_BUS_CMP_ALERT is active (recoverable, non-blocking).
     *
     * Test Steps:
     * 1. Enable EDN, configure endpoint 0 request
     * 2. Send genbits A → verify endpoint receives data
     * 3. Send matching genbits A → trigger alert
     * 4. Verify alert = 1
     * 5. Configure endpoint 1 request
     * 6. Verify endpoint 1 receives entropy from buffer
     * 7. Send genbits B (triggers new generate command)
     * 8. Verify endpoints continue to receive entropy
     *
     * Pass Criteria:
     * - Entropy buffer populated despite alert
     * - Endpoints receive entropy during alert
     * - No functional blocking of entropy distribution
     * - Alert is truly "recoverable" (informational)
     */
    bool test_entropy_distribution_continues();

    /**
     * @brief TC10: Alert clearing does not clear m_prev_genbits
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify that clearing RECOV_ALERT_STS.EDN_BUS_CMP_ALERT does NOT
     * clear the m_prev_genbits state (next match should still trigger).
     *
     * Test Steps:
     * 1. Send genbits A
     * 2. Send matching genbits A → trigger alert
     * 3. Clear alert via W0C
     * 4. Send genbits A again (still matches m_prev_genbits)
     * 5. Verify alert triggers again
     *
     * Pass Criteria:
     * - W0C only clears alert status bit
     * - m_prev_genbits state retained after alert clear
     * - Consecutive match after clear still triggers alert
     */
    bool test_alert_clear_preserves_prev_state();

    /**
     * @brief TC11: Zero values consecutive match detection
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify consistency check correctly detects consecutive zero-valued
     * genbits (edge case: all zeros).
     *
     * Test Steps:
     * 1. Send genbits: 0x00000000_00000000_00000000_00000000
     * 2. Send matching genbits: 0x00000000_00000000_00000000_00000000
     * 3. Verify alert triggers
     *
     * Pass Criteria:
     * - Zero values correctly compared (not treated as uninitialized)
     * - Alert triggers for consecutive zero matches
     */
    bool test_zero_values_match();

    /**
     * @brief TC12: All-ones values consecutive match detection
     * @return true if test passes, false otherwise
     *
     * Test Objective:
     * Verify consistency check correctly detects consecutive all-ones
     * genbits (edge case: maximum values).
     *
     * Test Steps:
     * 1. Send genbits: 0xFFFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFF
     * 2. Send matching genbits: 0xFFFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFF
     * 3. Verify alert triggers
     *
     * Pass Criteria:
     * - All-ones values correctly compared
     * - Alert triggers for consecutive maximum value matches
     */
    bool test_all_ones_match();

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Send genbits to EDN via CSRNG interface
     * @param genbits Array of 4x 32-bit words (128-bit entropy block)
     * @param fips_compliance FIPS compliance indicator (default true)
     *
     * Simulates CSRNG sending entropy data to EDN via csrng_genbits_target_socket.
     * Triggers receive_csrng_entropy() method which calls check_entropy_bus_consistency().
     */
    void send_genbits_to_edn(const uint32_t genbits[4], bool fips_compliance = true);

    /**
     * @brief Read RECOV_ALERT_STS register
     * @return Current value of RECOV_ALERT_STS register
     */
    uint32_t read_recov_alert_sts();

    /**
     * @brief Clear EDN_BUS_CMP_ALERT via W0C
     *
     * Writes 0 to bit 12 of RECOV_ALERT_STS to clear the alert status.
     */
    void clear_edn_bus_cmp_alert();

    /**
     * @brief Clear all recoverable alerts via W0C
     *
     * Writes 0 to all bits in RECOV_ALERT_STS register.
     */
    void clear_all_recoverable_alerts();

    /**
     * @brief Verify EDN_BUS_CMP_ALERT bit state
     * @param expected_state Expected state of bit 12 (0 or 1)
     * @return true if actual state matches expected, false otherwise
     */
    bool verify_edn_bus_cmp_alert(uint32_t expected_state);

    /**
     * @brief Verify alert_recov_alert signal state
     * @param expected_state Expected signal state (true = asserted)
     * @return true if actual state matches expected, false otherwise
     */
    bool verify_alert_recov_signal(bool expected_state);

    /**
     * @brief Enable EDN in software port mode for testing
     *
     * Configures EDN for software port mode to allow manual entropy injection:
     * - Write CTRL: EDN_ENABLE=0x6, BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9
     */
    void enable_edn_software_mode();

    /**
     * @brief Report test result
     * @param test_name Name of the test
     * @param passed true if test passed, false if failed
     */
    void report_test_result(const std::string& test_name, bool passed);

    // =========================================================================
    // Test Tracking
    // =========================================================================

    unsigned int m_tests_run;       ///< Number of tests executed
    unsigned int m_tests_passed;    ///< Number of tests passed
    unsigned int m_tests_failed;    ///< Number of tests failed
    std::vector<std::string> m_failed_tests; ///< List of failed test names
};
