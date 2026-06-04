/**
 * @file test_edn_func_003.h
 * @brief EDN_FUNC_003 Test Suite - State Machine Management and Observability Verification
 *
 * Comprehensive test suite for EDN_FUNC_003 (State Machine Management and Observability) covering:
 * - MAIN_SM_STATE register read callback and observability
 * - State machine reset behavior (Idle state = 0xC1)
 * - State transitions in boot-time request mode (Idle → BootInsAckWait → BootGenAckWait)
 * - State transitions in auto request mode (Idle → AutoLoadIns → AutoDispatch → AutoGenAckWait → AutoReseedAckWait)
 * - State transitions in software port mode (Idle → SWPortMode, stable operation)
 * - Mode entry and exit transitions (Boot → SWPort, Auto → SWPort)
 * - Mode priority enforcement (Boot > Auto > Software)
 * - Illegal state detection and Error state entry
 * - Fatal alert generation on state machine errors (EDN_MAIN_SM_ERR, EDN_ACK_SM_ERR)
 * - Corner cases: exit during active commands, multiple mode disable
 *
 * Test Coverage (13 test cases mapped to EDN_FUNC_003):
 * - test_main_sm_state_visibility: Validates MAIN_SM_STATE register read access
 * - test_state_machine_reset_behavior: Verifies Idle state (0xC1) after reset
 * - test_boot_mode_state_transitions: Tests Idle → BootInsAckWait → BootGenAckWait
 * - test_auto_mode_state_transitions: Validates all auto mode state transitions
 * - test_state_idle_to_boot_transition: Tests Idle → BootInsAckWait on boot enable
 * - test_state_boot_to_swport_transition: Verifies boot exit to SWPortMode
 * - test_state_idle_to_auto_transition: Tests Idle → AutoLoadIns on auto enable
 * - test_state_auto_to_swport_transition: Verifies auto exit to SWPortMode
 * - test_state_swport_stable: Tests SWPortMode state persistence
 * - test_state_error_on_illegal_state: Validates Error state entry on illegal state
 * - test_alert_fatal_main_sm_illegal_state: Tests fatal alert on EDN_MAIN_SM error
 * - test_alert_fatal_ack_sm_illegal_state: Tests fatal alert on EDN_ACK_SM error
 * - test_corner_auto_mode_exit_during_command: Corner case for exit during active command
 *
 * State Machine Sparse Encoding Values:
 * - Idle = 0xC1
 * - BootInsAckWait = 0x36
 * - BootGenAckWait = 0x9C
 * - AutoLoadIns = 0x63
 * - AutoFirstAckWait = 0x5A
 * - AutoDispatch = 0x3C
 * - AutoGenAckWait = 0x59
 * - AutoReseedAckWait = 0xA5
 * - SWPortMode = 0x96
 * - Error = 0x47
 *
 * Implementation Strategy:
 * - Each test validates specific state transitions via MAIN_SM_STATE reads
 * - Tests verify mode enable/disable sequences drive correct state changes
 * - Illegal state detection tests validate fatal error handling
 * - Alert assertions verified for state machine error conditions
 * - Corner cases ensure robust state transition behavior
 *
 * @note This test suite implements all 13 test cases mapped to EDN_FUNC_003
 *       in the edn-functionality-testcases.md document.
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-12
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_003
 * @brief Test fixture for EDN_FUNC_003 verification
 *
 * Extends edn_test to provide comprehensive state machine management and observability testing.
 * Implements all 13 test cases for EDN_FUNC_003 covering state transitions, mode operations,
 * illegal state detection, and error handling.
 */
class test_edn_func_003 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for state machine test execution.
     */
    test_edn_func_003(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_003();

    /**
     * @brief Execute all EDN_FUNC_003 test cases
     * @return Number of failed tests
     *
     * Runs all 13 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: MAIN_SM_STATE Register Visibility and Read Callback
    // =========================================================================
    /**
     * @brief Verify MAIN_SM_STATE register provides state machine observability
     *
     * Test Objective: Validate that MAIN_SM_STATE register is readable and
     * accurately reflects the current state of the EDN_MAIN_SM state machine
     * with correct sparse-encoded values.
     *
     * Test Plan Reference: test_main_sm_state_visibility
     * Functionality: EDN_FUNC_003 (State Machine Management and Observability)
     *
     * Procedure:
     * 1. Read MAIN_SM_STATE reset value (should be 0xC1 = Idle)
     * 2. Verify sparse encoding matches documented value
     * 3. Enable EDN in different modes and read state changes
     * 4. Verify state values update correctly for each mode
     * 5. Test read-only behavior (writes should have no effect)
     * 6. Confirm state visibility for firmware debugging
     *
     * Pass Criteria:
     * - MAIN_SM_STATE reads as 0xC1 after reset
     * - State updates reflect actual state machine transitions
     * - Read-only access enforced (writes ignored)
     * - Sparse-encoded values match specification
     *
     * @return true if test passes, false otherwise
     */
    bool test_main_sm_state_visibility();

    // =========================================================================
    // Test Case 2: State Machine Reset Behavior
    // =========================================================================
    /**
     * @brief Verify state machine initializes to Idle state after reset
     *
     * Test Objective: Validate that MAIN_SM_STATE register reads as 0xC1 (Idle)
     * immediately after system reset, confirming proper state machine initialization.
     *
     * Test Plan Reference: test_state_machine_reset_behavior
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Apply system reset via rst_ni
     * 2. Wait for reset propagation
     * 3. Read MAIN_SM_STATE register
     * 4. Verify value is 0xC1 (Idle state)
     * 5. Verify EDN is disabled (CTRL.EDN_ENABLE = 0x9)
     * 6. Apply multiple resets and verify consistent behavior
     *
     * Pass Criteria:
     * - MAIN_SM_STATE = 0xC1 after every reset
     * - State machine stable in Idle until enabled
     * - No spurious state transitions
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_machine_reset_behavior();

    // =========================================================================
    // Test Case 3: Boot-Time Mode State Transitions
    // =========================================================================
    /**
     * @brief Verify state machine transitions through boot-time mode states
     *
     * Test Objective: Validate complete state transition sequence through
     * boot-time request mode: Idle → BootInsAckWait → BootGenAckWait.
     *
     * Test Plan Reference: test_boot_mode_state_transitions
     * Functionality: EDN_FUNC_003, EDN_FUNC_012 (Boot-Time Mode)
     *
     * Procedure:
     * 1. Verify initial state is Idle (0xC1)
     * 2. Enable boot-time mode (EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6)
     * 3. Read MAIN_SM_STATE - should transition to BootInsAckWait (0x36)
     * 4. Wait for CSRNG instantiate acknowledgment (simulated)
     * 5. Read MAIN_SM_STATE - should transition to BootGenAckWait (0x9C)
     * 6. Wait for CSRNG generate acknowledgment
     * 7. Clear BOOT_REQ_MODE, wait for exit
     * 8. Verify transition to SWPortMode (0x96) with automatic uninstantiate
     *
     * Pass Criteria:
     * - State transitions follow sequence: Idle → BootInsAckWait → BootGenAckWait
     * - Sparse-encoded state values match specification
     * - Exit from boot mode transitions to SWPortMode
     * - Automatic uninstantiate command issued on exit
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_state_transitions();

    // =========================================================================
    // Test Case 4: Auto Request Mode State Transitions
    // =========================================================================
    /**
     * @brief Verify state machine transitions through auto request mode states
     *
     * Test Objective: Validate complete state transition sequence through
     * auto request mode including all operational states.
     *
     * Test Plan Reference: test_auto_mode_state_transitions
     * Functionality: EDN_FUNC_003, EDN_FUNC_013 (Auto Request Mode)
     *
     * Procedure:
     * 1. Configure auto mode prerequisites (GENERATE_CMD, RESEED_CMD, MAX_NUM_REQS)
     * 2. Enable auto mode (EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6)
     * 3. Verify transition to AutoLoadIns (0x63)
     * 4. Issue manual instantiate command
     * 5. Verify transition to AutoFirstAckWait (0x5A)
     * 6. Simulate instantiate acknowledgment
     * 7. Verify transition to AutoDispatch (0x3C)
     * 8. Simulate endpoint request
     * 9. Verify transition to AutoGenAckWait (0x59)
     * 10. Simulate generate acknowledgment, return to AutoDispatch
     * 11. Simulate reseed interval threshold
     * 12. Verify transition to AutoReseedAckWait (0xA5)
     * 13. Simulate reseed acknowledgment, return to AutoDispatch
     * 14. Clear AUTO_REQ_MODE, verify transition to SWPortMode (0x96)
     *
     * Pass Criteria:
     * - State transitions follow documented auto mode sequence
     * - All auto mode states correctly observed via MAIN_SM_STATE
     * - Sparse-encoded values match specification
     * - Exit from auto mode transitions to SWPortMode
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_mode_state_transitions();

    // =========================================================================
    // Test Case 5: Idle to Boot Mode Transition
    // =========================================================================
    /**
     * @brief Verify state transition from Idle to BootInsAckWait when boot mode enabled
     *
     * Test Objective: Validate that enabling boot-time request mode causes
     * immediate transition from Idle to BootInsAckWait state.
     *
     * Test Plan Reference: test_state_idle_to_boot_transition
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Confirm MAIN_SM_STATE = 0xC1 (Idle)
     * 2. Write CTRL with EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6
     * 3. Wait brief time for state update
     * 4. Read MAIN_SM_STATE
     * 5. Verify state = 0x36 (BootInsAckWait)
     * 6. Verify HW_CMD_STS.BOOT_MODE bit set
     * 7. Verify instantiate command issued to CSRNG
     *
     * Pass Criteria:
     * - Transition occurs immediately upon boot mode enable
     * - MAIN_SM_STATE = 0x36 (BootInsAckWait)
     * - HW_CMD_STS reflects boot mode active
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_idle_to_boot_transition();

    // =========================================================================
    // Test Case 6: Boot Mode to Software Port Mode Transition
    // =========================================================================
    /**
     * @brief Verify state transition from boot mode to SWPortMode on exit
     *
     * Test Objective: Validate that clearing BOOT_REQ_MODE while in boot mode
     * causes transition to SWPortMode with automatic uninstantiate.
     *
     * Test Plan Reference: test_state_boot_to_swport_transition
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Enable boot-time mode and enter BootInsAckWait or later state
     * 2. Clear BOOT_REQ_MODE (set to 0x9) while EDN_ENABLE=0x6
     * 3. Poll MAIN_SM_STATE until transition completes
     * 4. Verify final state = 0x96 (SWPortMode)
     * 5. Verify HW_CMD_STS.BOOT_MODE bit cleared
     * 6. Verify automatic uninstantiate command issued
     * 7. Verify SW_CMD_STS.CMD_RDY indicates readiness
     *
     * Pass Criteria:
     * - Transition to SWPortMode completes after current command
     * - MAIN_SM_STATE = 0x96
     * - Automatic uninstantiate occurs
     * - Software port ready for commands
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_boot_to_swport_transition();

    // =========================================================================
    // Test Case 7: Idle to Auto Mode Transition
    // =========================================================================
    /**
     * @brief Verify state transition from Idle to AutoLoadIns when auto mode enabled
     *
     * Test Objective: Validate that enabling auto request mode causes transition
     * from Idle to AutoLoadIns state.
     *
     * Test Plan Reference: test_state_idle_to_auto_transition
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Configure auto mode prerequisites
     * 2. Confirm MAIN_SM_STATE = 0xC1 (Idle)
     * 3. Write CTRL with EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6
     * 4. Wait for state update
     * 5. Read MAIN_SM_STATE
     * 6. Verify state = 0x63 (AutoLoadIns)
     * 7. Verify HW_CMD_STS.AUTO_MODE bit set
     *
     * Pass Criteria:
     * - Transition occurs upon auto mode enable
     * - MAIN_SM_STATE = 0x63 (AutoLoadIns)
     * - HW_CMD_STS reflects auto mode active
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_idle_to_auto_transition();

    // =========================================================================
    // Test Case 8: Auto Mode to Software Port Mode Transition
    // =========================================================================
    /**
     * @brief Verify state transition from auto mode to SWPortMode on exit
     *
     * Test Objective: Validate that clearing AUTO_REQ_MODE causes transition
     * to SWPortMode after current command completes.
     *
     * Test Plan Reference: test_state_auto_to_swport_transition
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Enable auto mode and reach AutoDispatch state
     * 2. Clear AUTO_REQ_MODE (set to 0x9) while EDN_ENABLE=0x6
     * 3. Poll MAIN_SM_STATE with timeout
     * 4. Verify transition to 0x96 (SWPortMode)
     * 5. Verify HW_CMD_STS.AUTO_MODE bit cleared
     * 6. Verify GENERATE_CMD and RESEED_CMD FIFOs cleared
     * 7. Verify CSRNG instance remains instantiated (no auto-uninstantiate)
     *
     * Pass Criteria:
     * - Transition waits for current command completion
     * - Final state = 0x96 (SWPortMode)
     * - Command FIFOs cleared on exit
     * - No automatic uninstantiate (unlike boot mode exit)
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_auto_to_swport_transition();

    // =========================================================================
    // Test Case 9: Software Port Mode State Stability
    // =========================================================================
    /**
     * @brief Verify state machine remains stable in SWPortMode during operations
     *
     * Test Objective: Validate that MAIN_SM_STATE remains 0x96 (SWPortMode)
     * during software command sequences without spurious transitions.
     *
     * Test Plan Reference: test_state_swport_stable
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Enable EDN in software port mode (no BOOT/AUTO modes)
     * 2. Verify MAIN_SM_STATE = 0x96 (SWPortMode)
     * 3. Issue instantiate command via SW_CMD_REQ
     * 4. Read MAIN_SM_STATE - should remain 0x96
     * 5. Issue generate command via SW_CMD_REQ
     * 6. Read MAIN_SM_STATE - should remain 0x96
     * 7. Issue reseed command
     * 8. Read MAIN_SM_STATE - should remain 0x96
     * 9. Issue uninstantiate command
     * 10. Verify state remains 0x96 until EDN disabled
     *
     * Pass Criteria:
     * - MAIN_SM_STATE = 0x96 throughout software operations
     * - No spurious state changes during command sequences
     * - State stable until mode change or disable
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_swport_stable();

    // =========================================================================
    // Test Case 10: Error State Entry on Illegal State Detection
    // =========================================================================
    /**
     * @brief Verify state machine transitions to Error state on illegal state detection
     *
     * Test Objective: Validate that when an illegal state is detected in
     * EDN_MAIN_SM or EDN_ACK_SM, the state machine transitions to Error state (0x47).
     *
     * Test Plan Reference: test_state_error_on_illegal_state
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Note: Illegal state detection is typically internal to model
     * 2. Use ERR_CODE_TEST to inject EDN_MAIN_SM_ERR error
     * 3. Write ERR_CODE_TEST with bit position for EDN_MAIN_SM_ERR (bit 21)
     * 4. Read ERR_CODE register
     * 5. Verify EDN_MAIN_SM_ERR bit [21] set
     * 6. Read MAIN_SM_STATE
     * 7. Verify state = 0x47 (Error) OR model enters safe state
     * 8. Verify alert_fatal_alert asserted
     * 9. Verify INTR_STATE.edn_fatal_err set
     * 10. Verify state machine unrecoverable without reset
     *
     * Pass Criteria:
     * - ERR_CODE.EDN_MAIN_SM_ERR set on illegal state
     * - MAIN_SM_STATE transitions to Error state or safe state
     * - Fatal alert asserted
     * - Fatal error interrupt generated
     * - Recovery requires system reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_error_on_illegal_state();

    // =========================================================================
    // Test Case 11: Fatal Alert on EDN_MAIN_SM Illegal State
    // =========================================================================
    /**
     * @brief Verify fatal alert assertion when EDN_MAIN_SM enters illegal state
     *
     * Test Objective: Validate that illegal state detection in main state machine
     * sets ERR_CODE.EDN_MAIN_SM_ERR and triggers fatal alert.
     *
     * Test Plan Reference: test_alert_fatal_main_sm_illegal_state
     * Functionality: EDN_FUNC_003, EDN_FUNC_010 (Alert Generation)
     *
     * Procedure:
     * 1. Clear ERR_CODE and alerts via reset
     * 2. Use ERR_CODE_TEST to inject EDN_MAIN_SM_ERR
     * 3. Write 21 to ERR_CODE_TEST (bit position for EDN_MAIN_SM_ERR)
     * 4. Wait for error propagation
     * 5. Read ERR_CODE register
     * 6. Verify ERR_CODE.EDN_MAIN_SM_ERR bit [21] set
     * 7. Verify alert_fatal_alert signal asserted (if observable)
     * 8. Verify INTR_STATE.edn_fatal_err bit set
     * 9. Attempt to clear ERR_CODE (should fail - sticky until reset)
     * 10. Verify fatal alert remains asserted until reset
     *
     * Pass Criteria:
     * - ERR_CODE.EDN_MAIN_SM_ERR set on illegal state
     * - Fatal alert asserted
     * - Fatal error interrupt generated
     * - ERR_CODE sticky (cannot be cleared by software)
     * - Alert persistent until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_main_sm_illegal_state();

    // =========================================================================
    // Test Case 12: Fatal Alert on EDN_ACK_SM Illegal State
    // =========================================================================
    /**
     * @brief Verify fatal alert assertion when EDN_ACK_SM enters illegal state
     *
     * Test Objective: Validate that illegal state detection in ACK state machine
     * sets ERR_CODE.EDN_ACK_SM_ERR and triggers fatal alert.
     *
     * Test Plan Reference: test_alert_fatal_ack_sm_illegal_state
     * Functionality: EDN_FUNC_003, EDN_FUNC_010
     *
     * Procedure:
     * 1. Clear ERR_CODE via reset
     * 2. Use ERR_CODE_TEST to inject EDN_ACK_SM_ERR
     * 3. Write 20 to ERR_CODE_TEST (bit position for EDN_ACK_SM_ERR)
     * 4. Wait for error propagation
     * 5. Read ERR_CODE register
     * 6. Verify ERR_CODE.EDN_ACK_SM_ERR bit [20] set
     * 7. Verify alert_fatal_alert signal asserted
     * 8. Verify INTR_STATE.edn_fatal_err bit set
     * 9. Verify ERR_CODE sticky behavior
     * 10. Verify alert persistent until reset
     *
     * Pass Criteria:
     * - ERR_CODE.EDN_ACK_SM_ERR set on illegal state
     * - Fatal alert asserted
     * - Fatal error interrupt generated
     * - ERR_CODE sticky until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_alert_fatal_ack_sm_illegal_state();

    // =========================================================================
    // Test Case 13: Corner Case - Auto Mode Exit During Active Command
    // =========================================================================
    /**
     * @brief Verify auto mode exit waits for active command completion
     *
     * Test Objective: Validate that clearing AUTO_REQ_MODE during an active
     * generate or reseed command waits for command completion before transitioning
     * to SWPortMode.
     *
     * Test Plan Reference: test_corner_auto_mode_exit_during_command
     * Functionality: EDN_FUNC_003
     *
     * Procedure:
     * 1. Enable auto request mode and reach AutoDispatch
     * 2. Trigger generate command (endpoint request or buffer depletion)
     * 3. Verify transition to AutoGenAckWait
     * 4. Immediately clear AUTO_REQ_MODE (while in AutoGenAckWait)
     * 5. Poll MAIN_SM_STATE
     * 6. Verify state remains in AutoGenAckWait until command completes
     * 7. Simulate CSRNG generate acknowledgment
     * 8. Verify state transitions to SWPortMode (not back to AutoDispatch)
     * 9. Verify no further automatic commands issued
     * 10. Verify HW_CMD_STS.AUTO_MODE cleared
     *
     * Pass Criteria:
     * - State machine waits for command completion before exit
     * - No premature transition that could corrupt command
     * - Clean exit to SWPortMode after acknowledgment
     * - No further automatic operations after exit requested
     *
     * @return true if test passes, false otherwise
     */
    bool test_corner_auto_mode_exit_during_command();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Wait for state machine transition to target state
     * @param target_state Expected sparse-encoded state value
     * @param timeout_ns Timeout in nanoseconds
     * @param poll_interval_ns Polling interval in nanoseconds
     * @return true if target state reached, false if timeout
     *
     * Polls MAIN_SM_STATE register until target state is reached or timeout occurs.
     */
    bool wait_for_state_transition(uint32_t target_state, uint64_t timeout_ns = 100000, uint64_t poll_interval_ns = 1000);

    /**
     * @brief Verify current state matches expected state
     * @param expected_state Expected sparse-encoded state value
     * @param state_name Human-readable state name for diagnostics
     * @return true if state matches, false otherwise
     */
    bool verify_current_state(uint32_t expected_state, const std::string& state_name);

    /**
     * @brief Configure auto request mode prerequisites
     *
     * Writes valid commands to GENERATE_CMD and RESEED_CMD FIFOs and sets
     * MAX_NUM_REQS_BETWEEN_RESEEDS to non-zero value.
     */
    void configure_auto_mode_prerequisites();

    /**
     * @brief Configure minimal boot command for state transition tests
     *
     * Overrides BOOT_GEN_CMD to use glen=0 for fast state transitions.
     * State transition tests verify behavior, not entropy throughput.
     * This reduces entropy injection time from ~41µs to ~0ns, allowing
     * tests to complete within timeout periods.
     */
    void configure_minimal_boot_mode();

    /**
     * @brief Inject state machine error via ERR_CODE_TEST
     * @param err_bit_position Bit position in ERR_CODE to force (0-30)
     *
     * Uses ERR_CODE_TEST register to inject specific error condition.
     */
    void inject_state_machine_error(uint32_t err_bit_position);

    /**
     * @brief Verify ERR_CODE bit is set
     * @param bit_position Bit position to check
     * @param bit_name Human-readable bit name for diagnostics
     * @return true if bit is set, false otherwise
     */
    bool verify_err_code_bit(uint32_t bit_position, const std::string& bit_name);

    /**
     * @brief Report test result with detailed information
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name, bool passed, const std::string& message = "");

    // =========================================================================
    // Test Constants - State Machine Sparse Encoding
    // =========================================================================

    /// State machine sparse-encoded values (from MAIN_SM_STATE register)
    static constexpr uint32_t STATE_IDLE = 0xC1;                ///< Idle state (reset default)
    static constexpr uint32_t STATE_BOOT_INS_ACK_WAIT = 0x36;   ///< Boot instantiate ack wait
    static constexpr uint32_t STATE_BOOT_GEN_ACK_WAIT = 0x9C;   ///< Boot generate ack wait
    static constexpr uint32_t STATE_AUTO_LOAD_INS = 0x63;       ///< Auto mode load instantiate
    static constexpr uint32_t STATE_AUTO_FIRST_ACK_WAIT = 0x5A; ///< Auto first ack wait
    static constexpr uint32_t STATE_AUTO_DISPATCH = 0x3C;       ///< Auto dispatch (operational)
    static constexpr uint32_t STATE_AUTO_GEN_ACK_WAIT = 0x59;   ///< Auto generate ack wait
    static constexpr uint32_t STATE_AUTO_RESEED_ACK_WAIT = 0xA5;///< Auto reseed ack wait
    static constexpr uint32_t STATE_SW_PORT_MODE = 0x96;        ///< Software port mode
    static constexpr uint32_t STATE_ERROR = 0x47;               ///< Error state (fatal)

    /// Multi-bit encoding values
    static constexpr uint32_t MULTIBIT_ENABLE = 0x6;            ///< Enable value (0b0110)
    static constexpr uint32_t MULTIBIT_DISABLE = 0x9;           ///< Disable value (0b1001)

    /// CTRL register field shifts
    static constexpr uint32_t EDN_ENABLE_SHIFT = 0;
    static constexpr uint32_t BOOT_REQ_MODE_SHIFT = 4;
    static constexpr uint32_t AUTO_REQ_MODE_SHIFT = 8;
    static constexpr uint32_t CMD_FIFO_RST_SHIFT = 12;

    /// ERR_CODE bit positions
    static constexpr uint32_t EDN_ACK_SM_ERR_BIT = 20;          ///< ACK state machine error
    static constexpr uint32_t EDN_MAIN_SM_ERR_BIT = 21;         ///< Main state machine error

    /// HW_CMD_STS bit positions
    static constexpr uint32_t HW_CMD_STS_BOOT_MODE_BIT = 0;     ///< Boot mode active
    static constexpr uint32_t HW_CMD_STS_AUTO_MODE_BIT = 1;     ///< Auto mode active

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;                   ///< Total tests executed
    unsigned int m_tests_passed;                ///< Tests that passed
    unsigned int m_tests_failed;                ///< Tests that failed
    std::vector<std::string> m_failed_tests;    ///< List of failed test names
};
