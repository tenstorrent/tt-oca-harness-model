/**
 * @file test_edn_func_013.h
 * @brief EDN_FUNC_013 Test Suite - Auto Request Mode Operation
 *
 * Comprehensive test suite for EDN_FUNC_013 (Auto Request Mode Operation) covering:
 * - Hardware-managed continuous entropy distribution
 * - Automatic Generate and Reseed command scheduling from pre-configured FIFOs
 * - Configurable reseed intervals via MAX_NUM_REQS_BETWEEN_RESEEDS
 * - Firmware-initiated instantiate followed by autonomous operation
 * - State machine transitions (AutoLoadIns → AutoFirstAckWait → AutoDispatch → AutoGenAckWait → AutoReseedAckWait)
 * - Command FIFO prerequisite configuration (GENERATE_CMD, RESEED_CMD)
 * - HW_CMD_STS monitoring for auto mode status
 * - FIPS compliance indicator propagation during auto mode
 * - Mode exit sequences and error handling
 * - Buffer depletion triggering automatic Generate commands
 *
 * Test Coverage (20 test cases mapped to EDN_FUNC_013):
 * - T1:  Auto mode prerequisite configuration (GENERATE_CMD, RESEED_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS)
 * - T2:  Auto mode enable sequence (EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6)
 * - T3:  Manual instantiate requirement (firmware must issue instantiate via SW_CMD_REQ)
 * - T4:  Automatic Generate command from GENERATE_CMD FIFO on endpoint request
 * - T5:  Automatic Reseed after MAX_NUM_REQS_BETWEEN_RESEEDS Generate commands
 * - T6:  Entropy distribution with FIPS indicator propagation in auto mode
 * - T7:  Auto mode exit sequence (clearing AUTO_REQ_MODE waits for completion)
 * - T8:  State machine transitions through auto-specific states
 * - T9:  HW_CMD_STS.AUTO_MODE bit reflects active auto request mode
 * - T10: MAX_NUM_REQS_BETWEEN_RESEEDS register configuration
 * - T11: CMD_FIFO_RST clears command FIFOs during auto mode
 * - T12: State transition from Idle to AutoLoadIns
 * - T13: State transition from auto states to SWPortMode
 * - T14: MAX_NUM_REQS_BETWEEN_RESEEDS=0 disables automatic Generate
 * - T15: MAX_NUM_REQS_BETWEEN_RESEEDS exceeding CSRNG limit causes error
 * - T16: Exit during active command waits for completion (corner case)
 * - T17: CMD_FIFO_RST during auto mode requires reconfiguration (corner case)
 * - T18: Reset during auto mode immediately disables EDN
 * - T19: Buffer depletion triggers automatic Generate in auto mode
 * - T20: Transition from auto to boot-time mode requires full disable/re-enable
 *
 * @note This test suite implements all 20 test cases mapped to EDN_FUNC_013
 *       in the edn-functionality-testcases.md document (test cases #42-49, #54-55, #104-105, #108, #114, #118, #133, #142).
 *
 * Auto Request Mode Architecture:
 * - Prerequisites: GENERATE_CMD, RESEED_CMD FIFOs and MAX_NUM_REQS_BETWEEN_RESEEDS must be configured
 * - Activation: Set EDN_ENABLE=0x6 and AUTO_REQ_MODE=0x6 in CTRL register
 * - Initial Instantiate: Firmware manually issues instantiate command via SW_CMD_REQ
 * - Autonomous Operation: Hardware automatically issues Generate when endpoints request entropy
 * - Automatic Reseed: Hardware issues Reseed after MAX_NUM_REQS_BETWEEN_RESEEDS Generate commands
 * - Generate Counter: Internal counter tracks Generate commands, resets on Reseed
 * - FIPS Indicator: Propagates FIPS status from CSRNG to endpoints
 * - Exit: Clearing AUTO_REQ_MODE waits for current command completion before transitioning to SWPortMode
 *
 * State Machine Flow:
 * 1. Idle → AutoLoadIns: When AUTO_REQ_MODE enabled
 * 2. AutoLoadIns → AutoFirstAckWait: After firmware issues instantiate
 * 3. AutoFirstAckWait → AutoDispatch: CSRNG acknowledges instantiate
 * 4. AutoDispatch → AutoGenAckWait: Endpoint requests entropy, Generate issued
 * 5. AutoGenAckWait → AutoDispatch: CSRNG returns entropy
 * 6. AutoDispatch → AutoReseedAckWait: Generate counter reaches MAX_NUM_REQS_BETWEEN_RESEEDS
 * 7. AutoReseedAckWait → AutoDispatch: CSRNG acknowledges reseed
 * 8. AutoDispatch → SWPortMode: AUTO_REQ_MODE cleared after command completes
 *
 * Command FIFO Format:
 * - GENERATE_CMD FIFO: Header word + 0-12 data words (total max 13 words)
 * - RESEED_CMD FIFO: Header word + 0-12 data words (total max 13 words)
 * - Header format: [31:4]=command_type, [3:0]=clen
 * - Generate command type: 0x403 (cmd_type=4, clen=0)
 * - Reseed command type: 0x303 (cmd_type=3, clen=0)
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-14
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>

/**
 * @class test_edn_func_013
 * @brief Test fixture for EDN_FUNC_013 verification
 *
 * Extends edn_test to provide comprehensive Auto Request Mode operation testing.
 * Implements all 20 test cases for EDN_FUNC_013 covering prerequisite configuration,
 * mode activation, autonomous Generate/Reseed scheduling, state transitions, FIPS
 * propagation, exit sequences, error conditions, and corner cases.
 */
class test_edn_func_013 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for auto mode test execution.
     */
    test_edn_func_013(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_013();

    /**
     * @brief Execute all EDN_FUNC_013 test cases
     * @return Number of failed tests
     *
     * Runs all 20 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Auto Mode Prerequisite Configuration
    // =========================================================================
    /**
     * @brief Verify auto request mode requires prerequisite FIFO and counter configuration
     *
     * Test Objective: Validate that auto request mode requires GENERATE_CMD,
     * RESEED_CMD FIFOs and MAX_NUM_REQS_BETWEEN_RESEEDS to be configured before
     * enabling AUTO_REQ_MODE. Tests that missing configuration prevents proper
     * autonomous operation.
     *
     * Test Plan Reference: Test case #42 (test_auto_mode_prerequisite_config)
     * Functionality: EDN_FUNC_013 (Auto Request Mode Operation)
     *
     * Procedure:
     * 1. Apply reset
     * 2. Configure GENERATE_CMD FIFO:
     *    - Write generate command header (0x00FFF003: cmd_type=4, glen=0xFFF, clen=0)
     * 3. Configure RESEED_CMD FIFO:
     *    - Write reseed command header (0x00000301: cmd_type=3, clen=0)
     * 4. Configure MAX_NUM_REQS_BETWEEN_RESEEDS:
     *    - Write value 0x00000010 (16 generate commands between reseeds)
     * 5. Verify all configurations written successfully
     * 6. Test negative case: Try to enable AUTO_REQ_MODE without configuration
     *    - Apply reset
     *    - Set AUTO_REQ_MODE=0x6 without configuring FIFOs
     *    - Issue instantiate command
     *    - Request endpoint entropy
     *    - Verify no automatic Generate occurs (state machine stalls)
     *
     * Pass Criteria:
     * - GENERATE_CMD FIFO accepts command words
     * - RESEED_CMD FIFO accepts command words
     * - MAX_NUM_REQS_BETWEEN_RESEEDS accepts counter value
     * - Auto mode without configuration fails to operate autonomously
     *
     * @return true if test passes, false otherwise
     */
    bool test_prerequisite_configuration();

    // =========================================================================
    // Test Case 2: Auto Mode Enable Sequence
    // =========================================================================
    /**
     * @brief Verify auto request mode activation sequence
     *
     * Test Objective: Validate activation of auto request mode by setting
     * EDN_ENABLE=0x6 and AUTO_REQ_MODE=0x6 in CTRL register after prerequisite
     * configuration. Confirm HW_CMD_STS.AUTO_MODE bit set and state machine
     * transitions to AutoLoadIns state.
     *
     * Test Plan Reference: Test case #43 (test_auto_mode_enable_sequence)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset
     * 2. Configure prerequisites (GENERATE_CMD, RESEED_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS)
     * 3. Read MAIN_SM_STATE (verify Idle state 0xC1)
     * 4. Read HW_CMD_STS (verify AUTO_MODE bit [1] = 0)
     * 5. Write CTRL register:
     *    - EDN_ENABLE=0x6 (bits [3:0])
     *    - AUTO_REQ_MODE=0x6 (bits [11:8])
     *    - BOOT_REQ_MODE=0x9 (bits [7:4], disabled)
     *    - Value: 0x00009696
     * 6. Wait 10ns for state transition
     * 7. Read MAIN_SM_STATE (verify AutoLoadIns state expected value)
     * 8. Read HW_CMD_STS (verify AUTO_MODE bit [1] = 1)
     * 9. Read CTRL register (verify configuration persisted)
     *
     * Pass Criteria:
     * - CTRL register accepts AUTO_REQ_MODE=0x6
     * - HW_CMD_STS.AUTO_MODE bit set to 1
     * - MAIN_SM_STATE transitions from Idle to auto-specific state
     * - EDN enters auto request mode ready for firmware instantiate
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_mode_enable();

    // =========================================================================
    // Test Case 3: Manual Instantiate Requirement
    // =========================================================================
    /**
     * @brief Verify firmware must manually issue instantiate command after enabling auto mode
     *
     * Test Objective: Validate that auto request mode requires firmware to
     * manually issue instantiate command via SW_CMD_REQ after enabling
     * AUTO_REQ_MODE. Hardware does not automatically instantiate (unlike
     * boot-time mode).
     *
     * Test Plan Reference: Test case #44 (test_auto_mode_manual_instantiate)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset and configure auto mode prerequisites
     * 2. Enable auto mode (CTRL: EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6)
     * 3. Verify state = AutoLoadIns (waiting for firmware instantiate)
     * 4. Poll SW_CMD_STS.CMD_RDY until ready (bit [1] = 1)
     * 5. Issue instantiate command via SW_CMD_REQ:
     *    - Write 0x00000901 (cmd_type=1, clen=0, no additional data)
     * 6. Wait for instantiate completion
     * 7. Poll SW_CMD_STS.CMD_ACK (bit [2] = 1)
     * 8. Read SW_CMD_STS.CMD_STS (bits [5:3], verify 0x0 = success)
     * 9. Verify INTR_STATE.edn_cmd_req_done set (bit [0] = 1)
     * 10. Clear interrupt (write 0x1 to INTR_STATE)
     * 11. Read MAIN_SM_STATE (verify transition to AutoDispatch or AutoFirstAckWait)
     * 12. Read HW_CMD_STS (verify AUTO_MODE still active)
     *
     * Pass Criteria:
     * - SW_CMD_REQ accepts instantiate command
     * - Instantiate completes with CMD_ACK and CMD_STS=0
     * - edn_cmd_req_done interrupt fires
     * - State machine advances from AutoLoadIns to dispatching state
     * - AUTO_MODE remains active after manual instantiate
     *
     * @return true if test passes, false otherwise
     */
    bool test_manual_instantiate();

    // =========================================================================
    // Test Case 4: Automatic Generate Command
    // =========================================================================
    /**
     * @brief Verify hardware automatically issues Generate command when endpoints request entropy
     *
     * Test Objective: Validate that after manual instantiate in auto mode,
     * hardware automatically issues Generate command from GENERATE_CMD FIFO
     * when endpoint asserts edn_req signal, without firmware intervention.
     *
     * Test Plan Reference: Test case #45 (test_auto_mode_generate_command)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset and complete auto mode setup:
     *    - Configure FIFOs and counter
     *    - Enable AUTO_REQ_MODE
     *    - Issue manual instantiate
     * 2. Verify state = AutoDispatch (ready for endpoint requests)
     * 3. Read HW_CMD_STS.CMD_TYPE (should be 0x1 from instantiate)
     * 4. Assert edn_req[0] signal (endpoint 0 requests entropy)
     * 5. Wait 20ns for hardware Generate command issuance
     * 6. Verify state transition to AutoGenAckWait
     * 7. Read HW_CMD_STS.CMD_TYPE (should be 0x4 = Generate)
     * 8. Simulate CSRNG providing entropy (128-bit genbits, FIPS=true)
     * 9. Wait for CSRNG acknowledgment
     * 10. Read HW_CMD_STS.CMD_ACK (bit [6] = 1)
     * 11. Verify edn_ack[0] asserted
     * 12. Read edn_bus[0] (verify 32-bit entropy data present)
     * 13. Read edn_fips[0] (verify FIPS indicator set)
     * 14. Deassert edn_req[0]
     * 15. Verify state returns to AutoDispatch (ready for next request)
     *
     * Pass Criteria:
     * - Hardware automatically issues Generate (no firmware writes to SW_CMD_REQ)
     * - HW_CMD_STS.CMD_TYPE updates to 0x4 (Generate)
     * - State machine AutoDispatch → AutoGenAckWait → AutoDispatch
     * - Entropy delivered to requesting endpoint
     * - FIPS indicator propagated correctly
     * - No interrupts generated for hardware commands
     *
     * @return true if test passes, false otherwise
     */
    bool test_automatic_generate();

    // =========================================================================
    // Test Case 5: Automatic Reseed After Interval
    // =========================================================================
    /**
     * @brief Verify automatic Reseed after MAX_NUM_REQS_BETWEEN_RESEEDS Generate commands
     *
     * Test Objective: Validate that hardware automatically issues Reseed
     * command from RESEED_CMD FIFO after MAX_NUM_REQS_BETWEEN_RESEEDS Generate
     * commands have been issued, ensuring compliance with NIST SP 800-90A
     * reseed interval requirements.
     *
     * Test Plan Reference: Test case #46 (test_auto_mode_reseed_interval)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset and configure auto mode:
     *    - GENERATE_CMD FIFO: 0x00FFF003 (Generate command)
     *    - RESEED_CMD FIFO: 0x00000301 (Reseed command)
     *    - MAX_NUM_REQS_BETWEEN_RESEEDS = 0x00000004 (reseed after 4 generates)
     * 2. Enable AUTO_REQ_MODE and issue manual instantiate
     * 3. Issue 4 consecutive endpoint requests:
     *    - For i = 0 to 3:
     *      a. Assert edn_req[0]
     *      b. Wait for AutoGenAckWait state
     *      c. Verify HW_CMD_STS.CMD_TYPE = 0x4 (Generate)
     *      d. Provide CSRNG entropy
     *      e. Wait for edn_ack[0]
     *      f. Deassert edn_req[0]
     *      g. Verify return to AutoDispatch
     * 4. After 4th Generate, verify internal counter reached threshold
     * 5. Assert edn_req[0] for 5th request
     * 6. Wait 20ns
     * 7. Verify state transitions to AutoReseedAckWait (not AutoGenAckWait)
     * 8. Read HW_CMD_STS.CMD_TYPE (should be 0x3 = Reseed)
     * 9. Wait for CSRNG Reseed acknowledgment
     * 10. Verify HW_CMD_STS.CMD_ACK = 1
     * 11. Verify HW_CMD_STS.CMD_STS = 0x0 (success)
     * 12. Verify state returns to AutoDispatch
     * 13. Verify counter reset (next Generate should be immediate, not Reseed)
     * 14. Assert edn_req[0] again
     * 15. Verify HW_CMD_STS.CMD_TYPE = 0x4 (Generate, not Reseed)
     *
     * Pass Criteria:
     * - 4 Generate commands issued for first 4 requests
     * - 5th request triggers Reseed command (counter reached threshold)
     * - Reseed completes successfully
     * - Counter resets after Reseed
     * - 6th request triggers Generate (not Reseed)
     * - State machine AutoDispatch → AutoReseedAckWait → AutoDispatch
     *
     * @return true if test passes, false otherwise
     */
    bool test_automatic_reseed();

    // =========================================================================
    // Test Case 6: Entropy Distribution with FIPS Propagation
    // =========================================================================
    /**
     * @brief Verify auto mode distributes entropy with FIPS indicator propagation
     *
     * Test Objective: Validate that auto request mode correctly distributes
     * entropy to endpoint interfaces with proper FIPS compliance indicator
     * propagation from CSRNG to endpoints, distinguishing FIPS-approved from
     * pre-FIPS seeds.
     *
     * Test Plan Reference: Test case #47 (test_auto_mode_entropy_distribution)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_005 + EDN_FUNC_011
     *
     * Procedure:
     * Part A: FIPS-approved entropy (FIPS=true)
     * 1. Configure and enable auto mode
     * 2. Issue manual instantiate
     * 3. Request entropy from endpoint 0 (edn_req[0])
     * 4. Provide CSRNG entropy with FIPS=true
     * 5. Verify edn_ack[0] asserted
     * 6. Read edn_bus[0] (verify 32-bit entropy data)
     * 7. Read edn_fips[0] (verify = 1, FIPS-approved)
     * 8. Request from endpoint 1
     * 9. Verify edn_fips[1] = 1
     * 10. Request from multiple endpoints (2, 3, 4)
     * 11. Verify all edn_fips signals = 1
     *
     * Part B: Pre-FIPS entropy (FIPS=false)
     * 12. Apply reset and reconfigure auto mode
     * 13. Issue manual instantiate
     * 14. Request from endpoint 0
     * 15. Provide CSRNG entropy with FIPS=false (pre-FIPS seed)
     * 16. Verify edn_ack[0] asserted
     * 17. Read edn_fips[0] (verify = 0, pre-FIPS)
     * 18. Request from endpoints 5, 6, 7
     * 19. Verify all edn_fips signals = 0
     *
     * Part C: 128-bit to 32-bit conversion
     * 20. Provide 128-bit entropy (4x 32-bit words)
     * 21. Verify endpoints receive 32-bit chunks sequentially
     * 22. Verify data persistence on edn_bus until next request
     *
     * Pass Criteria:
     * - FIPS=true: edn_fips signals asserted
     * - FIPS=false: edn_fips signals de-asserted
     * - All 8 endpoints receive entropy correctly
     * - 128-bit to 32-bit conversion accurate
     * - Data persists on bus for asynchronous consumption
     *
     * @return true if test passes, false otherwise
     */
    bool test_entropy_distribution_fips();

    // =========================================================================
    // Test Case 7: Auto Mode Exit Sequence
    // =========================================================================
    /**
     * @brief Verify clearing AUTO_REQ_MODE waits for command completion before transitioning
     *
     * Test Objective: Validate that clearing AUTO_REQ_MODE field in CTRL
     * register during auto mode operation waits for current command completion
     * before transitioning state machine to SWPortMode, ensuring clean exit
     * without command truncation.
     *
     * Test Plan Reference: Test case #48 (test_auto_mode_exit_sequence)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Configure and enable auto mode
     * 2. Issue manual instantiate
     * 3. Trigger endpoint request (edn_req[0])
     * 4. Wait for state = AutoGenAckWait (Generate command in flight)
     * 5. While Generate command active, write CTRL:
     *    - Clear AUTO_REQ_MODE (set to 0x9)
     *    - Keep EDN_ENABLE=0x6
     *    - Value: 0x00009996
     * 6. Verify state remains AutoGenAckWait (command not aborted)
     * 7. Provide CSRNG entropy to complete Generate
     * 8. Wait for CSRNG acknowledgment
     * 9. Verify edn_ack[0] asserted (command completed successfully)
     * 10. Wait 20ns for state transition
     * 11. Read MAIN_SM_STATE (verify transitioned to SWPortMode)
     * 12. Read HW_CMD_STS.AUTO_MODE (verify bit [1] = 0, auto mode exited)
     * 13. Verify HW_CMD_STS.BOOT_MODE (verify bit [0] = 0, not in boot mode)
     * 14. Request entropy from endpoint (edn_req[1])
     * 15. Wait 50ns
     * 16. Verify no automatic Generate (hardware does not respond)
     * 17. Verify state remains SWPortMode (auto mode fully exited)
     *
     * Pass Criteria:
     * - Clearing AUTO_REQ_MODE does not abort active command
     * - Generate command completes successfully
     * - State machine waits for completion before transitioning
     * - Final state = SWPortMode
     * - HW_CMD_STS.AUTO_MODE cleared
     * - No automatic responses to endpoint requests after exit
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_mode_exit();

    // =========================================================================
    // Test Case 8: State Machine Transitions
    // =========================================================================
    /**
     * @brief Verify state machine transitions through auto-specific states
     *
     * Test Objective: Validate complete state machine transition sequence
     * through all auto request mode states: AutoLoadIns → AutoFirstAckWait →
     * AutoDispatch → AutoGenAckWait → AutoReseedAckWait → AutoDispatch.
     *
     * Test Plan Reference: Test case #49 (test_auto_mode_state_transitions)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_003
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read MAIN_SM_STATE (verify Idle state 0xC1)
     * 3. Configure prerequisites and enable AUTO_REQ_MODE
     * 4. Read MAIN_SM_STATE (verify AutoLoadIns state)
     * 5. Issue manual instantiate command
     * 6. Read MAIN_SM_STATE (verify AutoFirstAckWait state)
     * 7. Wait for CSRNG acknowledgment
     * 8. Read MAIN_SM_STATE (verify AutoDispatch state)
     * 9. Assert edn_req[0]
     * 10. Wait for hardware Generate issuance
     * 11. Read MAIN_SM_STATE (verify AutoGenAckWait state)
     * 12. Provide CSRNG entropy
     * 13. Wait for acknowledgment
     * 14. Read MAIN_SM_STATE (verify return to AutoDispatch)
     * 15. Trigger reseed by exceeding MAX_NUM_REQS_BETWEEN_RESEEDS
     * 16. Read MAIN_SM_STATE (verify AutoReseedAckWait state)
     * 17. Wait for CSRNG acknowledgment
     * 18. Read MAIN_SM_STATE (verify return to AutoDispatch)
     * 19. Clear AUTO_REQ_MODE
     * 20. Wait for current command completion
     * 21. Read MAIN_SM_STATE (verify transition to SWPortMode)
     *
     * Pass Criteria:
     * - All state transitions occur in correct sequence
     * - States persist until transition conditions met
     * - No illegal state transitions or skipped states
     * - State values match expected sparse-encoded values
     * - Complete cycle from Idle through auto states back to SWPortMode
     *
     * @return true if test passes, false otherwise
     */
    bool test_state_machine_transitions();

    // =========================================================================
    // Test Case 9: HW_CMD_STS AUTO_MODE Bit
    // =========================================================================
    /**
     * @brief Verify HW_CMD_STS.AUTO_MODE bit reflects auto request mode status
     *
     * Test Objective: Validate HW_CMD_STS.AUTO_MODE bit (bit [1]) accurately
     * reflects whether EDN is currently in auto request mode, providing
     * firmware with visibility into hardware-controlled mode status.
     *
     * Test Plan Reference: Test case #24 (test_hw_cmd_sts_auto_mode)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read HW_CMD_STS (offset 0x28)
     * 3. Verify AUTO_MODE bit [1] = 0 (not in auto mode)
     * 4. Configure auto mode prerequisites
     * 5. Write CTRL with AUTO_REQ_MODE=0x6
     * 6. Wait 10ns
     * 7. Read HW_CMD_STS
     * 8. Verify AUTO_MODE bit [1] = 1 (auto mode active)
     * 9. Verify BOOT_MODE bit [0] = 0 (not in boot mode)
     * 10. Issue manual instantiate
     * 11. Read HW_CMD_STS (verify AUTO_MODE still = 1)
     * 12. Trigger automatic Generate
     * 13. Read HW_CMD_STS (verify AUTO_MODE still = 1 during operation)
     * 14. Clear AUTO_REQ_MODE
     * 15. Wait for transition to SWPortMode
     * 16. Read HW_CMD_STS
     * 17. Verify AUTO_MODE bit [1] = 0 (auto mode exited)
     * 18. Re-enable AUTO_REQ_MODE
     * 19. Verify AUTO_MODE bit [1] = 1 again
     *
     * Pass Criteria:
     * - AUTO_MODE bit = 0 when not in auto mode
     * - AUTO_MODE bit = 1 throughout auto mode operation
     * - AUTO_MODE bit clears on mode exit
     * - AUTO_MODE bit independent of BOOT_MODE bit
     * - Bit updates reflect state machine mode changes
     *
     * @return true if test passes, false otherwise
     */
    bool test_hw_cmd_sts_auto_mode_bit();

    // =========================================================================
    // Test Case 10: MAX_NUM_REQS_BETWEEN_RESEEDS Configuration
    // =========================================================================
    /**
     * @brief Verify MAX_NUM_REQS_BETWEEN_RESEEDS register configuration and reseed interval control
     *
     * Test Objective: Validate MAX_NUM_REQS_BETWEEN_RESEEDS register (offset
     * 0x34) read/write functionality and verify it correctly controls the
     * automatic reseed interval in auto mode.
     *
     * Test Plan Reference: Test case #29 (test_max_num_reqs_between_reseeds_rw)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read MAX_NUM_REQS_BETWEEN_RESEEDS (verify reset value 0x0)
     * 3. Write value 0x00000008 (8 generates between reseeds)
     * 4. Read back and verify value persisted
     * 5. Write value 0xFFFFFFFF (maximum value)
     * 6. Read back and verify
     * 7. Write value 0x00000001 (minimum non-zero)
     * 8. Read back and verify
     * 9. Configure auto mode with MAX_NUM_REQS_BETWEEN_RESEEDS = 3
     * 10. Enable AUTO_REQ_MODE and issue instantiate
     * 11. Issue 3 endpoint requests (trigger 3 Generates)
     * 12. Verify HW_CMD_STS.CMD_TYPE = 0x4 for all 3
     * 13. Issue 4th request
     * 14. Verify HW_CMD_STS.CMD_TYPE = 0x3 (Reseed triggered at threshold)
     * 15. Reconfigure to MAX_NUM_REQS_BETWEEN_RESEEDS = 1
     * 16. Verify Reseed after every Generate
     *
     * Pass Criteria:
     * - Register read/write functionality correct
     * - Reset value = 0x0
     * - Full 32-bit values accepted
     * - Reseed interval matches configured value
     * - Counter resets after Reseed
     * - Different thresholds work correctly
     *
     * @return true if test passes, false otherwise
     */
    bool test_max_reqs_configuration();

    // =========================================================================
    // Test Case 11: CMD_FIFO_RST Clears FIFOs
    // =========================================================================
    /**
     * @brief Verify CTRL.CMD_FIFO_RST clears command FIFOs during auto mode
     *
     * Test Objective: Validate that setting CTRL.CMD_FIFO_RST field to 0x6
     * clears both GENERATE_CMD and RESEED_CMD FIFOs during auto mode,
     * requiring reconfiguration before resuming autonomous operation.
     *
     * Test Plan Reference: Test case #14 (test_ctrl_cmd_fifo_rst_valid),
     *                      Test case #114 (test_corner_cmd_fifo_rst_during_auto_mode)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Configure and enable auto mode with FIFOs loaded
     * 2. Issue manual instantiate
     * 3. Trigger automatic Generate (verify FIFOs operational)
     * 4. Write CTRL.CMD_FIFO_RST = 0x6 (bits [15:12])
     *    - Value: 0x00006696 (keeping AUTO_REQ_MODE enabled)
     * 5. Wait 10ns
     * 6. Write CTRL.CMD_FIFO_RST = 0x9 (clear reset, normal operation)
     *    - Value: 0x00009696
     * 7. Assert edn_req[0]
     * 8. Wait 50ns
     * 9. Verify no automatic Generate (FIFOs empty)
     * 10. Read MAIN_SM_STATE (verify still in auto mode but not dispatching)
     * 11. Reconfigure FIFOs (write new commands)
     * 12. Assert edn_req[0] again
     * 13. Verify automatic Generate resumes
     * 14. Test invalid CMD_FIFO_RST values:
     *     - Write invalid value (e.g., 0x5)
     *     - Verify RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT set (bit [3])
     *     - Verify alert_recov_alert asserted
     *
     * Pass Criteria:
     * - CMD_FIFO_RST=0x6 clears FIFOs
     * - Auto mode stops dispatching after FIFO clear
     * - Reconfiguration restores operation
     * - Invalid values trigger recoverable alert
     * - AUTO_REQ_MODE remains active during FIFO reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_fifo_rst();

    // =========================================================================
    // Test Case 12: Idle to AutoLoadIns Transition
    // =========================================================================
    /**
     * @brief Verify state machine transitions from Idle to AutoLoadIns
     *
     * Test Objective: Validate state machine transition from Idle (0xC1) to
     * AutoLoadIns when CTRL enables auto request mode (EDN_ENABLE=0x6,
     * AUTO_REQ_MODE=0x6).
     *
     * Test Plan Reference: Test case #125 (test_state_idle_to_auto_transition)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_003
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read MAIN_SM_STATE (verify 0xC1 = Idle)
     * 3. Configure auto mode prerequisites
     * 4. Write CTRL with AUTO_REQ_MODE=0x6
     * 5. Wait 10ns
     * 6. Read MAIN_SM_STATE (verify transitioned to AutoLoadIns)
     * 7. Verify HW_CMD_STS.AUTO_MODE = 1
     * 8. Verify state persists (read again after 20ns, still AutoLoadIns)
     * 9. Issue instantiate command
     * 10. Verify state advances from AutoLoadIns
     *
     * Pass Criteria:
     * - Transition occurs on AUTO_REQ_MODE enable
     * - AutoLoadIns state persists until instantiate issued
     * - State value matches expected sparse encoding
     * - No spurious transitions
     *
     * @return true if test passes, false otherwise
     */
    bool test_idle_to_autoloadins();

    // =========================================================================
    // Test Case 13: Auto to SWPortMode Transition
    // =========================================================================
    /**
     * @brief Verify state machine transitions from auto states to SWPortMode
     *
     * Test Objective: Validate transition from any auto request state to
     * SWPortMode when AUTO_REQ_MODE is cleared, waiting for current command
     * completion before transitioning.
     *
     * Test Plan Reference: Test case #126 (test_state_auto_to_swport_transition)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_003
     *
     * Procedure:
     * Part A: From AutoDispatch (idle in auto mode)
     * 1. Enable auto mode and issue instantiate
     * 2. Verify state = AutoDispatch
     * 3. Clear AUTO_REQ_MODE
     * 4. Wait 10ns
     * 5. Verify state = SWPortMode (immediate transition, no command active)
     *
     * Part B: From AutoGenAckWait (command active)
     * 6. Re-enable auto mode
     * 7. Trigger Generate command
     * 8. Verify state = AutoGenAckWait
     * 9. Clear AUTO_REQ_MODE (command still active)
     * 10. Wait 10ns
     * 11. Verify state still = AutoGenAckWait (waits for completion)
     * 12. Complete Generate command (provide CSRNG entropy)
     * 13. Wait 20ns
     * 14. Verify state = SWPortMode (transitioned after completion)
     *
     * Part C: From AutoReseedAckWait
     * 15. Re-enable auto mode and trigger reseed
     * 16. Verify state = AutoReseedAckWait
     * 17. Clear AUTO_REQ_MODE during reseed
     * 18. Verify waits for reseed completion
     * 19. Complete reseed
     * 20. Verify transition to SWPortMode
     *
     * Pass Criteria:
     * - Immediate transition when no command active
     * - Waits for command completion when active
     * - Transitions from all auto states
     * - Final state always SWPortMode
     * - AUTO_MODE bit clears
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_to_swport_transition();

    // =========================================================================
    // Test Case 14: MAX_NUM_REQS_BETWEEN_RESEEDS Zero Disables Generate
    // =========================================================================
    /**
     * @brief Verify setting MAX_NUM_REQS_BETWEEN_RESEEDS=0 disables automatic Generate
     *
     * Test Objective: Validate that configuring MAX_NUM_REQS_BETWEEN_RESEEDS
     * to 0 disables automatic Generate command issuance in auto mode,
     * preventing autonomous operation.
     *
     * Test Plan Reference: Test case #104 (test_error_auto_mode_max_reqs_zero)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Apply reset
     * 2. Configure auto mode with MAX_NUM_REQS_BETWEEN_RESEEDS = 0x0
     * 3. Configure GENERATE_CMD and RESEED_CMD FIFOs normally
     * 4. Enable AUTO_REQ_MODE
     * 5. Issue manual instantiate
     * 6. Verify state = AutoDispatch
     * 7. Assert edn_req[0] (endpoint requests entropy)
     * 8. Wait 50ns
     * 9. Verify no automatic Generate issued (HW_CMD_STS.CMD_TYPE ≠ 0x4)
     * 10. Verify edn_ack[0] not asserted
     * 11. Verify state remains AutoDispatch (stalled)
     * 12. Read HW_CMD_STS (verify AUTO_MODE still = 1)
     * 13. Reconfigure MAX_NUM_REQS_BETWEEN_RESEEDS = 0x10
     * 14. Assert edn_req[0] again
     * 15. Verify automatic Generate now occurs
     * 16. Verify entropy delivered
     *
     * Pass Criteria:
     * - MAX_NUM_REQS_BETWEEN_RESEEDS=0 prevents Generate commands
     * - Endpoint requests not serviced
     * - State machine remains in AutoDispatch (does not error)
     * - Reconfiguration restores functionality
     * - No error interrupts or alerts generated
     *
     * @return true if test passes, false otherwise
     */
    bool test_max_reqs_zero_disables();

    // =========================================================================
    // Test Case 15: MAX_NUM_REQS_BETWEEN_RESEEDS Exceeds CSRNG Limit
    // =========================================================================
    /**
     * @brief Verify exceeding CSRNG RESEED_INTERVAL causes command rejection
     *
     * Test Objective: Validate that setting MAX_NUM_REQS_BETWEEN_RESEEDS to
     * a value exceeding CSRNG's internal RESEED_INTERVAL causes CSRNG to
     * reject Generate commands with non-zero status, triggering recoverable
     * alert and error handling.
     *
     * Test Plan Reference: Test case #105 (test_error_auto_mode_max_reqs_exceeds_csrng)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_004
     *
     * Procedure:
     * 1. Apply reset
     * 2. Configure MAX_NUM_REQS_BETWEEN_RESEEDS = 0xFFFFFFFF (excessive)
     * 3. Configure FIFOs normally
     * 4. Enable AUTO_REQ_MODE and issue instantiate
     * 5. Issue multiple endpoint requests (exceed CSRNG internal limit)
     * 6. After CSRNG limit reached, CSRNG returns error status
     * 7. Read HW_CMD_STS.CMD_ACK (verify = 1)
     * 8. Read HW_CMD_STS.CMD_STS (verify non-zero status, e.g., 0x1)
     * 9. Read RECOV_ALERT_STS (verify CSRNG_ACK_ERR bit [13] set)
     * 10. Verify alert_recov_alert asserted
     * 11. Read ERR_CODE (verify SFIFO_ESRNG_ERR or related bit set)
     * 12. Verify edn_fatal_err interrupt if enabled
     * 13. Read MAIN_SM_STATE (may transition to Error state)
     * 14. Apply reset to clear error
     * 15. Reconfigure with reasonable MAX_NUM_REQS_BETWEEN_RESEEDS
     * 16. Verify normal operation resumes
     *
     * Pass Criteria:
     * - Excessive MAX_NUM_REQS causes CSRNG error
     * - HW_CMD_STS.CMD_STS reflects error status
     * - RECOV_ALERT_STS.CSRNG_ACK_ERR set
     * - Recoverable or fatal alert asserted
     * - Error condition documented in registers
     * - Reset clears error state
     *
     * @return true if test passes, false otherwise
     */
    bool test_max_reqs_exceeds_csrng();

    // =========================================================================
    // Test Case 16: Exit During Active Command (Corner Case)
    // =========================================================================
    /**
     * @brief Verify clearing AUTO_REQ_MODE during active command waits for completion
     *
     * Test Objective: Corner case validation that clearing AUTO_REQ_MODE
     * while Generate or Reseed command is active does not abort command,
     * waits for CSRNG acknowledgment, and then transitions to SWPortMode.
     *
     * Test Plan Reference: Test case #108 (test_corner_auto_mode_exit_during_command)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Enable auto mode and issue instantiate
     * 2. Assert edn_req[0] to trigger Generate
     * 3. Wait for state = AutoGenAckWait (command issued, waiting for CSRNG)
     * 4. Read HW_CMD_STS.CMD_TYPE (verify 0x4 = Generate)
     * 5. While in AutoGenAckWait, clear AUTO_REQ_MODE
     * 6. Wait 10ns
     * 7. Verify state still = AutoGenAckWait (command not aborted)
     * 8. Verify HW_CMD_STS.CMD_ACK = 0 (still waiting)
     * 9. Provide CSRNG entropy (complete Generate)
     * 10. Verify HW_CMD_STS.CMD_ACK = 1
     * 11. Verify edn_ack[0] asserted (command completed)
     * 12. Wait 20ns for state transition
     * 13. Read MAIN_SM_STATE (verify = SWPortMode)
     * 14. Verify HW_CMD_STS.AUTO_MODE = 0
     * 15. Repeat test with Reseed command active
     *
     * Pass Criteria:
     * - Clearing AUTO_REQ_MODE does not abort active command
     * - Command completes normally
     * - Entropy delivered to requesting endpoint
     * - State transitions to SWPortMode after completion
     * - No command truncation or data loss
     *
     * @return true if test passes, false otherwise
     */
    bool test_exit_during_active_command();

    // =========================================================================
    // Test Case 17: CMD_FIFO_RST During Auto Mode (Corner Case)
    // =========================================================================
    /**
     * @brief Verify CMD_FIFO_RST during auto mode clears FIFOs and requires reconfiguration
     *
     * Test Objective: Corner case validation that setting CMD_FIFO_RST=0x6
     * during active auto mode clears command FIFOs, requires reconfiguration,
     * and does not leave auto mode in inconsistent state.
     *
     * Test Plan Reference: Test case #114 (test_corner_cmd_fifo_rst_during_auto_mode)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Enable auto mode with FIFOs configured
     * 2. Issue instantiate and trigger automatic Generate
     * 3. Verify operation normal
     * 4. Set CMD_FIFO_RST=0x6 while in AutoDispatch
     * 5. Verify FIFOs cleared immediately
     * 6. Clear CMD_FIFO_RST (set to 0x9)
     * 7. Assert edn_req[0]
     * 8. Verify no automatic command (FIFOs empty)
     * 9. Reconfigure FIFOs:
     *    - Write new GENERATE_CMD
     *    - Write new RESEED_CMD
     * 10. Assert edn_req[0] again
     * 11. Verify automatic Generate resumes
     * 12. Verify AUTO_MODE still active
     * 13. Test CMD_FIFO_RST during active command:
     *     - Trigger Generate
     *     - Set CMD_FIFO_RST during AutoGenAckWait
     *     - Verify command completes before FIFOs cleared
     * 14. Test invalid CMD_FIFO_RST values trigger alert
     *
     * Pass Criteria:
     * - CMD_FIFO_RST clears FIFOs immediately
     * - Auto mode requires reconfiguration after reset
     * - AUTO_MODE remains active
     * - No state machine corruption
     * - Active commands complete before clear
     *
     * @return true if test passes, false otherwise
     */
    bool test_fifo_rst_during_auto();

    // =========================================================================
    // Test Case 18: Reset During Auto Mode
    // =========================================================================
    /**
     * @brief Verify system reset during auto mode immediately disables EDN
     *
     * Test Objective: Validate that asserting system reset (rst_ni) during
     * auto request mode operation immediately disables EDN, clears state
     * machine, resets all registers, and returns to Idle state.
     *
     * Test Plan Reference: Test case #118 (test_reset_during_auto_mode)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Enable auto mode and issue instantiate
     * 2. Trigger automatic Generate command
     * 3. Verify state = AutoGenAckWait
     * 4. Assert reset (rst_ni = 0) for 100ns while command active
     * 5. Deassert reset (rst_ni = 1)
     * 6. Wait 10ns for reset propagation
     * 7. Read MAIN_SM_STATE (verify 0xC1 = Idle)
     * 8. Read CTRL (verify 0x00009999 = reset value, all modes disabled)
     * 9. Read HW_CMD_STS (verify 0x0, AUTO_MODE cleared)
     * 10. Read ERR_CODE (verify 0x0, all errors cleared)
     * 11. Read INTR_STATE (verify 0x0, interrupts cleared)
     * 12. Verify alert_fatal_alert = 0
     * 13. Verify alert_recov_alert = 0
     * 14. Reconfigure and re-enable auto mode
     * 15. Verify normal operation resumes
     *
     * Pass Criteria:
     * - Reset immediately stops EDN operation
     * - All registers return to reset values
     * - State machine returns to Idle
     * - All interrupts and alerts cleared
     * - Active commands aborted without error
     * - EDN functional after reconfiguration
     *
     * @return true if test passes, false otherwise
     */
    bool test_reset_during_auto_mode();

    // =========================================================================
    // Test Case 19: Buffer Depletion Triggers Generate
    // =========================================================================
    /**
     * @brief Verify entropy buffer depletion triggers automatic Generate in auto mode
     *
     * Test Objective: Validate that internal entropy buffer depletion
     * automatically triggers Generate command to CSRNG in auto mode,
     * ensuring continuous entropy availability for endpoints.
     *
     * Test Plan Reference: Test case #142 (test_buffer_depletion_triggers_generate)
     * Functionality: EDN_FUNC_013
     *
     * Procedure:
     * 1. Enable auto mode and issue instantiate
     * 2. Trigger initial Generate (endpoint request)
     * 3. Verify 128-bit (4x 32-bit) entropy received from CSRNG
     * 4. Request entropy from endpoints sequentially:
     *    - Endpoint 0 request → 32-bit chunk 1
     *    - Endpoint 1 request → 32-bit chunk 2
     *    - Endpoint 2 request → 32-bit chunk 3
     *    - Endpoint 3 request → 32-bit chunk 4 (buffer depleted)
     * 5. Assert edn_req[4] (5th request, buffer empty)
     * 6. Wait 20ns
     * 7. Verify automatic Generate issued (state = AutoGenAckWait)
     * 8. Verify HW_CMD_STS.CMD_TYPE = 0x4
     * 9. Provide new 128-bit entropy
     * 10. Verify edn_ack[4] asserted with fresh entropy
     * 11. Continue endpoint requests
     * 12. Verify buffer refilled and depleted cyclically
     * 13. Verify each depletion triggers new Generate
     *
     * Pass Criteria:
     * - 4 endpoint requests satisfied from single 128-bit block
     * - 5th request triggers new Generate
     * - Fresh entropy delivered
     * - Buffer management transparent to endpoints
     * - No gaps in entropy delivery
     *
     * @return true if test passes, false otherwise
     */
    bool test_buffer_depletion_generate();

    // =========================================================================
    // Test Case 20: Auto to Boot Mode Transition
    // =========================================================================
    /**
     * @brief Verify transition from auto to boot-time mode requires full disable/re-enable
     *
     * Test Objective: Validate that transitioning from auto request mode to
     * boot-time request mode requires full EDN disable and re-enable sequence,
     * not just clearing AUTO_REQ_MODE and setting BOOT_REQ_MODE.
     *
     * Test Plan Reference: Test case #133 (test_auto_to_boot_mode_transition)
     * Functionality: EDN_FUNC_013 + EDN_FUNC_012
     *
     * Procedure:
     * 1. Enable auto mode and issue instantiate
     * 2. Verify AUTO_MODE active
     * 3. Attempt direct transition (incorrect):
     *    - Write CTRL with BOOT_REQ_MODE=0x6, AUTO_REQ_MODE=0x9
     *    - Verify behavior (may not transition properly)
     * 4. Correct transition sequence:
     *    a. Clear AUTO_REQ_MODE (transition to SWPortMode)
     *    b. Wait for transition complete
     *    c. Issue uninstantiate command via SW_CMD_REQ
     *    d. Clear EDN_ENABLE (set to 0x9)
     *    e. Wait 10ns
     *    f. Verify state = Idle
     *    g. Configure BOOT_INS_CMD and BOOT_GEN_CMD
     *    h. Write CTRL with EDN_ENABLE=0x6, BOOT_REQ_MODE=0x6
     *    i. Verify state = BootInsAckWait
     *    j. Verify HW_CMD_STS.BOOT_MODE = 1
     *    k. Verify HW_CMD_STS.AUTO_MODE = 0
     * 5. Verify boot-time mode operates correctly
     * 6. Verify no interference from previous auto mode state
     *
     * Pass Criteria:
     * - Direct mode switch not supported
     * - Full disable/uninstantiate/re-enable sequence required
     * - Boot-time mode activates correctly
     * - No state machine corruption
     * - BOOT_MODE and AUTO_MODE bits mutually exclusive
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_to_boot_transition();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Configure auto mode prerequisites (FIFOs and counter)
     * @param max_reqs_value MAX_NUM_REQS_BETWEEN_RESEEDS value (default 16)
     *
     * Configures GENERATE_CMD FIFO, RESEED_CMD FIFO, and
     * MAX_NUM_REQS_BETWEEN_RESEEDS register for auto mode operation.
     */
    void configure_auto_mode_prerequisites(uint32_t max_reqs_value = 0x00000010);

    /**
     * @brief Enable auto request mode
     *
     * Writes CTRL register to enable auto mode (EDN_ENABLE=0x6, AUTO_REQ_MODE=0x6).
     */
    void enable_auto_mode();

    /**
     * @brief Issue manual instantiate command for auto mode
     * @return true if instantiate successful
     *
     * Issues instantiate command via SW_CMD_REQ and waits for completion.
     */
    bool issue_manual_instantiate();

    /**
     * @brief Simulate endpoint entropy request
     * @param endpoint_id Endpoint index (0-7)
     * @param wait_for_ack Wait for acknowledge before returning
     *
     * Asserts edn_req[endpoint_id] and optionally waits for edn_ack[endpoint_id].
     */
    void simulate_endpoint_request(unsigned int endpoint_id, bool wait_for_ack = true);

    /**
     * @brief Simulate CSRNG providing entropy
     * @param fips_status FIPS compliance indicator
     *
     * Simulates CSRNG responding with 128-bit entropy.
     */
    void simulate_csrng_entropy_response(bool fips_status = true);

    /**
     * @brief Wait for state machine state
     * @param expected_state Expected MAIN_SM_STATE value
     * @param timeout_ns Timeout in nanoseconds
     * @return true if state reached before timeout
     *
     * Polls MAIN_SM_STATE until expected value or timeout.
     */
    bool wait_for_state(uint32_t expected_state, double timeout_ns = 100.0);

    /**
     * @brief Verify HW_CMD_STS fields
     * @param expected_auto_mode Expected AUTO_MODE bit value
     * @param expected_boot_mode Expected BOOT_MODE bit value
     * @param expected_cmd_type Expected CMD_TYPE value (optional)
     * @return true if all fields match
     *
     * Reads HW_CMD_STS and verifies expected field values.
     */
    bool verify_hw_cmd_sts(unsigned int expected_auto_mode,
                          unsigned int expected_boot_mode,
                          int expected_cmd_type = -1);

    /**
     * @brief Verify register bit value
     * @param reg_offset Register offset
     * @param bit_position Bit position to check
     * @param expected_value Expected bit value
     * @param reg_name Register name for diagnostics
     * @return true if bit matches expected value
     */
    bool verify_register_bit(uint32_t reg_offset,
                             unsigned int bit_position,
                             unsigned int expected_value,
                             const std::string& reg_name);

    /**
     * @brief Report test result
     * @param test_name Test case name
     * @param passed Test pass/fail status
     * @param message Optional diagnostic message
     */
    void report_test_result(const std::string& test_name,
                           bool passed,
                           const std::string& message = "");

    // =========================================================================
    // Register Offsets and Constants
    // =========================================================================

    // Command types for HW_CMD_STS.CMD_TYPE
    static constexpr uint32_t CMD_TYPE_INSTANTIATE = 0x1;
    static constexpr uint32_t CMD_TYPE_RESEED = 0x3;
    static constexpr uint32_t CMD_TYPE_GENERATE = 0x4;
    static constexpr uint32_t CMD_TYPE_UNINSTANTIATE = 0x6;

    // CTRL register multi-bit encoded values
    static constexpr uint32_t MBE_ENABLE = 0x6;
    static constexpr uint32_t MBE_DISABLE = 0x9;

    // State machine states (sparse-encoded)
    static constexpr uint32_t STATE_IDLE = 0xC1;
    static constexpr uint32_t STATE_SWPORTMODE = 0x35;  // Software port mode
    static constexpr uint32_t STATE_ERROR = 0x47;

    // CSRNG command formats
    static constexpr uint32_t CMD_INSTANTIATE = 0x00000001;  // cmd_type=1, clen=0
    static constexpr uint32_t CMD_GENERATE = 0x00FFF003;     // cmd_type=3, glen=0xFFF, clen=0
    static constexpr uint32_t CMD_RESEED = 0x00000004;       // cmd_type=4, clen=0
    static constexpr uint32_t CMD_UNINSTANTIATE = 0x00000005; // cmd_type=5, clen=0

    // =========================================================================
    // Test Statistics
    // =========================================================================

    unsigned int m_tests_run;                    ///< Total tests executed
    unsigned int m_tests_passed;                 ///< Tests that passed
    unsigned int m_tests_failed;                 ///< Tests that failed
    std::vector<std::string> m_failed_tests;     ///< List of failed test names

    // =========================================================================
    // Test State Tracking
    // =========================================================================

    bool m_auto_mode_configured;                 ///< Auto mode prerequisites configured
    bool m_auto_mode_enabled;                    ///< Auto mode currently enabled
    bool m_instantiate_issued;                   ///< Instantiate command issued
    unsigned int m_generate_count;               ///< Generate commands issued (for reseed tracking)
};
