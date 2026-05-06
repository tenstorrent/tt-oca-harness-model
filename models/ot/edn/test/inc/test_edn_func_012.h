/**
 * @file test_edn_func_012.h
 * @brief EDN_FUNC_012 Test Suite - Boot-Time Request Mode Operation Verification
 *
 * Comprehensive test suite for EDN_FUNC_012 (Boot-Time Request Mode Operation) covering:
 * - Boot mode enable sequence via CTRL register configuration
 * - Automatic Instantiate command generation using BOOT_INS_CMD register
 * - Automatic Generate command generation using BOOT_GEN_CMD register
 * - Endpoint servicing during boot phase with entropy distribution
 * - Pre-FIPS entropy validation (edn_fips signals de-asserted)
 * - Clean exit sequence with automatic Uninstantiate command
 * - State machine transitions through boot mode states
 *
 * Test Coverage (7 test cases):
 *
 * Boot Mode Configuration Tests (1 test):
 * - TC_EDN_BOOT_001: Boot Mode Enable Sequence (test_boot_mode_enable_sequence)
 *
 * Automatic Command Generation Tests (2 tests):
 * - TC_EDN_BOOT_002: Boot Mode Instantiate Command (test_boot_mode_instantiate_command)
 * - TC_EDN_BOOT_003: Boot Mode Generate Command (test_boot_mode_generate_command)
 *
 * Entropy Distribution Tests (2 tests):
 * - TC_EDN_BOOT_004: Boot Mode Entropy Distribution (test_boot_mode_entropy_distribution)
 * - TC_EDN_BOOT_005: Boot Mode Pre-FIPS Indicator (test_boot_mode_pre_fips_indicator)
 *
 * Exit and State Transition Tests (2 tests):
 * - TC_EDN_BOOT_006: Boot Mode Exit Sequence (test_boot_mode_exit_sequence)
 * - TC_EDN_BOOT_007: Boot Mode State Transitions (test_boot_mode_state_transitions)
 *
 * @note This test suite implements 7 test cases mapped to EDN_FUNC_012
 *       in the edn-test-plan.md document (tests #35-41).
 *
 * Implementation Strategy:
 * - Each test case is self-contained with setup, execution, and validation phases
 * - Tests use edn_test infrastructure for register access and signal monitoring
 * - Helper functions simplify boot mode configuration and state verification
 * - Assertions validate expected vs. observed behavior with detailed diagnostics
 * - Tests verify integration with endpoint distribution (EDN_FUNC_005)
 * - Failed tests report clear error messages for debugging
 *
 * Architecture References:
 * - edn-detailed-design.md Section 1.2.1: Boot-Time Request Mode
 * - edn-architecture-behaviour-map.json: boot_time_mode operations
 * - edn-memory-map-registers.md: BOOT_INS_CMD, BOOT_GEN_CMD registers
 * - edn.h: boot_mode_instantiate(), boot_mode_generate(), boot_mode_uninstantiate()
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_012
 * @brief Test fixture for EDN_FUNC_012 verification
 *
 * Extends edn_test to provide comprehensive boot-time request mode testing.
 * Implements all 7 test cases for EDN_FUNC_012 covering boot mode enable,
 * automatic command generation, entropy distribution, and state transitions.
 */
class test_edn_func_012 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_012(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_012();

    /**
     * @brief Execute all EDN_FUNC_012 test cases
     * @return Number of failed tests
     *
     * Runs all 7 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Boot Mode Enable Sequence
    // =========================================================================
    /**
     * @brief Verify boot-time mode activation via CTRL register
     *
     * Test Objective: Validate boot-time request mode activation by setting
     * EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6 in CTRL register, triggering
     * automatic state machine transition to BootInsAckWait.
     *
     * Test Plan Reference: test_boot_mode_enable_sequence (Test #35)
     * Functionality: EDN_FUNC_012 (Boot-Time Request Mode Operation)
     *
     * Procedure:
     * 1. Apply system reset to ensure clean start
     * 2. Configure BOOT_INS_CMD register (default 0x901 is acceptable)
     * 3. Configure BOOT_GEN_CMD register (default 0xFFF003 is acceptable)
     * 4. Read CTRL register, verify EDN_ENABLE=0x9 and BOOT_REQ_MODE=0x9 (disabled)
     * 5. Write CTRL with EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6
     * 6. Wait for state transition (allow async thread execution)
     * 7. Read MAIN_SM_STATE, verify state transitioned from Idle (0xC1) to BootInsAckWait (0x36)
     * 8. Read HW_CMD_STS, verify BOOT_MODE bit [0] = 1
     * 9. Verify AUTO_MODE bit [1] = 0 (not in auto mode)
     * 10. Verify CMD_TYPE field shows Instantiate command (0x1)
     *
     * Pass Criteria:
     * - CTRL write successful with correct multi-bit encoded values
     * - State machine transitions to BootInsAckWait (0x36)
     * - HW_CMD_STS.BOOT_MODE = 1 (boot mode active indicator)
     * - Boot mode takes precedence if both BOOT_REQ_MODE and AUTO_REQ_MODE set
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_enable_sequence();

    // =========================================================================
    // Test Case 2: Boot Mode Instantiate Command
    // =========================================================================
    /**
     * @brief Verify automatic Instantiate command generation using BOOT_INS_CMD
     *
     * Test Objective: Validate that hardware automatically issues Instantiate
     * command to CSRNG using BOOT_INS_CMD register configuration when entering
     * boot-time mode.
     *
     * Test Plan Reference: test_boot_mode_instantiate_command (Test #36)
     * Functionality: EDN_FUNC_012
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Read BOOT_INS_CMD register, verify default value 0x901 (cmd_type=1, clen=0)
     * 3. Optionally write custom BOOT_INS_CMD value (maintain clen=0 constraint)
     * 4. Enable boot mode (CTRL.EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6)
     * 5. Wait for Instantiate command execution (monitor csrng_cmd_port activity)
     * 6. Read HW_CMD_STS register:
     *    - Verify CMD_TYPE = 0x1 (Instantiate command type)
     *    - Verify CMD_ACK = 1 (CSRNG acknowledged command)
     *    - Verify CMD_STS = 0x0 (success status code)
     * 7. Read MAIN_SM_STATE, verify transitioned to BootGenAckWait (0x9C)
     * 8. Verify no CSRNG error alerts triggered
     *
     * Pass Criteria:
     * - Instantiate command automatically sent to CSRNG
     * - BOOT_INS_CMD register value used as command header
     * - HW_CMD_STS.CMD_TYPE shows Instantiate (0x1)
     * - CSRNG acknowledgment received (CMD_ACK=1, CMD_STS=0)
     * - State machine advances to BootGenAckWait (0x9C)
     * - Hardware enforces clen=0 constraint
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_instantiate_command();

    // =========================================================================
    // Test Case 3: Boot Mode Generate Command
    // =========================================================================
    /**
     * @brief Verify automatic Generate command generation using BOOT_GEN_CMD
     *
     * Test Objective: Validate that hardware automatically issues Generate
     * command to CSRNG using BOOT_GEN_CMD register configuration after
     * successful Instantiate acknowledgment.
     *
     * Test Plan Reference: test_boot_mode_generate_command (Test #37)
     * Functionality: EDN_FUNC_012
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Read BOOT_GEN_CMD register, verify default 0xFFF003 (cmd_type=3, glen=0xFFF)
     * 3. Enable boot mode to trigger Instantiate command
     * 4. Wait for Instantiate completion (MAIN_SM_STATE = BootGenAckWait)
     * 5. Monitor for automatic Generate command issuance
     * 6. Read HW_CMD_STS register:
     *    - Verify CMD_TYPE = 0x3 (Generate command type)
     *    - Verify CMD_ACK = 1 (CSRNG acknowledged)
     *    - Verify CMD_STS = 0x0 (success)
     * 7. Verify state remains in BootGenAckWait (0x9C)
     * 8. Inject entropy from CSRNG (simulate genbits delivery)
     * 9. Verify entropy buffered internally for endpoint distribution
     *
     * Pass Criteria:
     * - Generate command automatically sent after Instantiate succeeds
     * - BOOT_GEN_CMD register value used as command header
     * - Default glen=0xFFF (4096 blocks, maximum boot entropy)
     * - HW_CMD_STS.CMD_TYPE shows Generate (0x3)
     * - CSRNG acknowledgment received successfully
     * - Entropy delivered to internal buffer for endpoints
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_generate_command();

    // =========================================================================
    // Test Case 4: Boot Mode Entropy Distribution
    // =========================================================================
    /**
     * @brief Verify endpoint servicing during boot-time mode
     *
     * Test Objective: Validate that boot-time mode distributes entropy to
     * endpoint interfaces when peripherals assert edn_req signals, using
     * entropy generated by automatic boot Generate command.
     *
     * Test Plan Reference: test_boot_mode_entropy_distribution (Test #38)
     * Functionality: EDN_FUNC_012
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable boot mode (triggers Instantiate → Generate sequence)
     * 3. Inject 128-bit entropy block (0xAA_BB_CC_DD, 0x11_22_33_44, 0x55_66_77_88, 0x99_AA_BB_CC)
     * 4. Assert edn_req[0] = 1 (endpoint 0 requests entropy)
     * 5. Wait for edn_ack[0] assertion
     * 6. Verify edn_bus[0] = 0xAABBCCDD (first 32-bit chunk)
     * 7. Deassert edn_req[0]
     * 8. Assert edn_req[3] = 1 (endpoint 3 requests)
     * 9. Wait for edn_ack[3] assertion
     * 10. Verify edn_bus[3] = 0x11223344 (second chunk)
     * 11. Test multiple endpoints concurrently (edn_req[2], edn_req[5], edn_req[7])
     * 12. Verify all endpoints receive unique entropy chunks
     * 13. Verify round-robin arbitration functional during boot mode
     *
     * Pass Criteria:
     * - Endpoints serviced during boot-time mode
     * - Entropy distributed via existing EDN_FUNC_005 mechanism
     * - Each endpoint receives unique 32-bit chunks
     * - Handshake protocol functional (req → ack → deassert)
     * - Round-robin arbitration works for concurrent requests
     * - Data persistence on bus after acknowledge deasserts
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_entropy_distribution();

    // =========================================================================
    // Test Case 5: Boot Mode Pre-FIPS Indicator
    // =========================================================================
    /**
     * @brief Verify edn_fips signals de-asserted during boot-time mode
     *
     * Test Objective: Validate that boot-time request mode delivers pre-FIPS
     * entropy with edn_fips signals de-asserted (FIPS=0) since boot mode uses
     * fast seed without full NIST SP 800-90A health checks.
     *
     * Test Plan Reference: test_boot_mode_pre_fips_indicator (Test #39)
     * Functionality: EDN_FUNC_012 (also covers EDN_FUNC_011)
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable boot mode
     * 3. Inject entropy with FIPS=0 (pre-FIPS boot seed)
     * 4. Request entropy from multiple endpoints (0, 3, 7)
     * 5. Verify edn_fips[0] = 0 when edn_ack[0] = 1
     * 6. Verify edn_fips[3] = 0 when edn_ack[3] = 1
     * 7. Verify edn_fips[7] = 0 when edn_ack[7] = 1
     * 8. Verify edn_bus contains valid entropy data despite FIPS=0
     * 9. Verify FIPS=0 persists across all chunks from boot block
     * 10. Verify no spurious FIPS=1 assertions during boot mode
     *
     * Pass Criteria:
     * - All endpoints receive edn_fips = 0 (pre-FIPS indicator)
     * - Pre-FIPS entropy delivered correctly (32-bit chunks valid)
     * - FIPS indicator consistent across multiple endpoint requests
     * - Boot mode inherently provides pre-FIPS entropy (fast boot)
     * - FIPS=0 distinguishes boot entropy from FIPS-approved entropy
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_pre_fips_indicator();

    // =========================================================================
    // Test Case 6: Boot Mode Exit Sequence
    // =========================================================================
    /**
     * @brief Verify clean exit from boot mode with automatic Uninstantiate
     *
     * Test Objective: Validate that clearing BOOT_REQ_MODE field in CTRL
     * register transitions state machine to SWPortMode and issues automatic
     * Uninstantiate command to prevent CSRNG/EDN desynchronization.
     *
     * Test Plan Reference: test_boot_mode_exit_sequence (Test #40)
     * Functionality: EDN_FUNC_012
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Enable boot mode (CTRL.EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6)
     * 3. Wait for boot mode stable operation (BootGenAckWait state)
     * 4. Verify HW_CMD_STS.BOOT_MODE = 1 before exit
     * 5. Clear BOOT_REQ_MODE while keeping EDN_ENABLE:
     *    Write CTRL = 0x00009966 (EDN_ENABLE=0x6, BOOT_REQ_MODE=0x9)
     * 6. Wait for state transition and Uninstantiate command execution
     * 7. Read MAIN_SM_STATE, verify transitioned to SWPortMode (0x96)
     * 8. Read HW_CMD_STS:
     *    - Verify BOOT_MODE = 0 (boot mode exited)
     *    - Verify CMD_TYPE = 0x5 (Uninstantiate command type)
     *    - Verify CMD_ACK = 1, CMD_STS = 0 (successful uninstantiate)
     * 9. Verify endpoints still functional (EDN not disabled)
     * 10. Verify software can now use SW_CMD_REQ for new instantiate
     *
     * Pass Criteria:
     * - Clearing BOOT_REQ_MODE triggers state transition to SWPortMode
     * - Automatic Uninstantiate command issued to CSRNG
     * - HW_CMD_STS.BOOT_MODE cleared to 0
     * - MAIN_SM_STATE = SWPortMode (0x96)
     * - Clean exit prevents CSRNG/EDN desynchronization
     * - Firmware can issue new SW commands via SW_CMD_REQ
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_exit_sequence();

    // =========================================================================
    // Test Case 7: Boot Mode State Transitions
    // =========================================================================
    /**
     * @brief Verify state machine transitions through boot-time mode states
     *
     * Test Objective: Validate complete state machine flow during boot mode
     * lifecycle including all transitions: Idle → BootInsAckWait →
     * BootGenAckWait → SWPortMode.
     *
     * Test Plan Reference: test_boot_mode_state_transitions (Test #41)
     * Functionality: EDN_FUNC_012
     *
     * Procedure:
     * 1. Apply system reset
     * 2. Read MAIN_SM_STATE, verify Idle state (0xC1)
     * 3. Enable boot mode (CTRL.EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6)
     * 4. Monitor MAIN_SM_STATE during transition:
     *    a. Verify immediate transition from Idle (0xC1) to BootInsAckWait (0x36)
     *    b. Wait for Instantiate command completion
     *    c. Verify transition to BootGenAckWait (0x9C)
     *    d. Verify state stable in BootGenAckWait during entropy distribution
     * 5. Clear BOOT_REQ_MODE to trigger exit
     * 6. Verify transition from BootGenAckWait (0x9C) to SWPortMode (0x96)
     * 7. Verify state remains SWPortMode (no spurious transitions)
     * 8. Re-enable boot mode to test state reset capability
     * 9. Verify state transitions correctly on second boot cycle
     *
     * Pass Criteria:
     * - State sequence: Idle → BootInsAckWait → BootGenAckWait → SWPortMode
     * - Idle (0xC1) is initial reset state
     * - BootInsAckWait (0x36) entered on boot mode enable
     * - BootGenAckWait (0x9C) entered after Instantiate success
     * - SWPortMode (0x96) entered on boot mode exit
     * - No illegal state transitions observed
     * - State machine reusable for multiple boot cycles
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_state_transitions();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Enable boot-time request mode via CTRL register
     *
     * Writes CTRL register with EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6 to
     * activate boot-time mode. Clears AUTO_REQ_MODE to ensure boot mode
     * takes precedence.
     */
    void enable_boot_mode();

    /**
     * @brief Exit boot-time request mode via CTRL register
     *
     * Clears BOOT_REQ_MODE to 0x9 while maintaining EDN_ENABLE=0x6 to
     * trigger clean exit sequence with automatic Uninstantiate command.
     */
    void exit_boot_mode();

    /**
     * @brief Verify MAIN_SM_STATE register matches expected state
     * @param expected_state Expected sparse-encoded state value
     * @param context Test context string for error reporting
     * @return true if state matches, false otherwise
     *
     * Reads MAIN_SM_STATE register and compares to expected value.
     */
    bool verify_state(uint32_t expected_state, const std::string& context);

    /**
     * @brief Verify HW_CMD_STS register fields
     * @param boot_mode_expected Expected BOOT_MODE bit value
     * @param cmd_type_expected Expected CMD_TYPE field value
     * @param context Test context string for error reporting
     * @return true if all fields match, false otherwise
     *
     * Reads HW_CMD_STS register and validates BOOT_MODE and CMD_TYPE fields.
     */
    bool verify_hw_cmd_status(bool boot_mode_expected, uint32_t cmd_type_expected,
                              const std::string& context);

    /**
     * @brief Wait for state machine transition with timeout
     * @param target_state Expected state value after transition
     * @param timeout_ns Timeout in nanoseconds (default 1000ns)
     * @return true if state reached, false on timeout
     *
     * Polls MAIN_SM_STATE register until target state or timeout.
     */
    bool wait_for_state(uint32_t target_state, double timeout_ns = 1000.0);

    /**
     * @brief Inject 128-bit entropy block with FIPS status
     * @param chunk0 First 32-bit word (bits [127:96])
     * @param chunk1 Second 32-bit word (bits [95:64])
     * @param chunk2 Third 32-bit word (bits [63:32])
     * @param chunk3 Fourth 32-bit word (bits [31:0])
     * @param fips FIPS compliance indicator (false for boot mode)
     *
     * Simulates CSRNG genbits delivery to EDN entropy buffer.
     */
    void inject_entropy_to_buffer(uint32_t chunk0, uint32_t chunk1,
                                   uint32_t chunk2, uint32_t chunk3, bool fips);

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
     * @brief Assert endpoint request signal
     * @param endpoint_id Endpoint index (0-7)
     *
     * Drives edn_req[endpoint_id] = 1 and waits for propagation.
     */
    void assert_endpoint_request(unsigned int endpoint_id);

    /**
     * @brief Deassert endpoint request signal
     * @param endpoint_id Endpoint index (0-7)
     *
     * Drives edn_req[endpoint_id] = 0 and waits for propagation.
     */
    void deassert_endpoint_request(unsigned int endpoint_id);

    /**
     * @brief Read endpoint data bus value
     * @param endpoint_id Endpoint index (0-7)
     * @return 32-bit data value from edn_bus[endpoint_id]
     *
     * Samples current edn_bus value for verification.
     */
    uint32_t read_endpoint_data(unsigned int endpoint_id);

    /**
     * @brief Read endpoint FIPS indicator
     * @param endpoint_id Endpoint index (0-7)
     * @return FIPS indicator value from edn_fips[endpoint_id]
     *
     * Samples current edn_fips value for verification.
     */
    bool read_endpoint_fips(unsigned int endpoint_id);

    /**
     * @brief Verify register value matches expected value
     * @param context Description for diagnostics
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
    static constexpr unsigned int CTRL_OFFSET = 0x14;           ///< CTRL register offset
    static constexpr unsigned int BOOT_INS_CMD_OFFSET = 0x18;   ///< BOOT_INS_CMD register offset
    static constexpr unsigned int BOOT_GEN_CMD_OFFSET = 0x1C;   ///< BOOT_GEN_CMD register offset
    static constexpr unsigned int HW_CMD_STS_OFFSET = 0x28;     ///< HW_CMD_STS register offset
    static constexpr unsigned int MAIN_SM_STATE_OFFSET = 0x44;  ///< MAIN_SM_STATE register offset

    // =========================================================================
    // State Machine Constants (from edn.h)
    // =========================================================================
    static constexpr uint32_t STATE_IDLE = 0xC1;               ///< Idle state
    static constexpr uint32_t STATE_BOOT_INS_ACK_WAIT = 0x36;  ///< BootInsAckWait state
    static constexpr uint32_t STATE_BOOT_GEN_ACK_WAIT = 0x9C;  ///< BootGenAckWait state
    static constexpr uint32_t STATE_SW_PORT_MODE = 0x96;       ///< SWPortMode state

    // =========================================================================
    // CSRNG Command Types (from NIST SP 800-90A)
    // =========================================================================
    static constexpr uint32_t CSRNG_CMD_INSTANTIATE = 0x1;     ///< Instantiate command type
    static constexpr uint32_t CSRNG_CMD_GENERATE = 0x3;        ///< Generate command type
    static constexpr uint32_t CSRNG_CMD_UNINSTANTIATE = 0x5;   ///< Uninstantiate command type
};
