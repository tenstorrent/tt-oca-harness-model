// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_014.h
 * @brief EDN_FUNC_014 Test Suite - Software Port Mode Operation
 *
 * Comprehensive test suite for EDN_FUNC_014 (Software Port Mode Operation) covering:
 * - Firmware-controlled CSRNG command forwarding
 * - Complete lifecycle management (Instantiate, Generate, Reseed, Uninstantiate)
 * - SW_CMD_REQ/SW_CMD_STS interface operation
 * - Command readiness polling (CMD_REG_RDY, CMD_RDY)
 * - Command acknowledgment and status tracking
 * - Multi-word command sequences (header + up to 12 data words)
 * - Personalization string and additional data support
 * - Glen parameter control for Generate commands
 * - Interrupt generation (edn_cmd_req_done)
 * - Error handling (command sequencing violations, status codes)
 * - Mode entry conditions and state transitions
 * - Integration with endpoint entropy distribution
 * - FIPS indicator propagation
 *
 * Test Coverage (32 test cases mapped to EDN_FUNC_014):
 * - T1:  Software port mode enable (EDN_ENABLE=0x6 without other mode bits)
 * - T2:  Instantiate command with full parameter control
 * - T3:  Generate command with configurable glen parameter
 * - T4:  Reseed command with additional data
 * - T5:  Uninstantiate command before disabling
 * - T6:  Multi-word command sequences (header + data words)
 * - T7:  Command completion interrupt (edn_cmd_req_done)
 * - T8:  Entropy distribution to endpoints with FIPS indicator
 * - T9:  SW_CMD_REQ write-only FIFO interface
 * - T10: CMD_REG_RDY polling before each word write
 * - T11: CMD_RDY indicates readiness for new command sequence
 * - T12: CMD_ACK bit set on CSRNG acknowledgment
 * - T13: CMD_STS field reflects CSRNG status code
 * - T14: State machine remains in SWPortMode during commands
 * - T15: Command header field parsing (type, clen, flags, glen)
 * - T16: Clen field validation matches actual word count
 * - T17: Clen mismatch error detection
 * - T18: Personalization string support via additional data
 * - T19: Glen parameter controls entropy amount
 * - T20: Maximum glen value (0xFFF) per NIST SP 800-90A
 * - T21: Instantiate command format and protocol
 * - T22: Generate command with entropy data reception
 * - T23: Reseed command format with additional data
 * - T24: Uninstantiate command lifecycle management
 * - T25: NIST SP 800-90A command sequencing requirements
 * - T26: Successful command completion (CMD_STS=0)
 * - T27: Error handling on non-zero CMD_STS
 * - T28: Proper shutdown sequence (Uninstantiate → clear EDN_ENABLE)
 * - T29: Mode reconfiguration sequence requirements
 * - T30: Reset during command execution aborts and resets
 * - T31: Writing without polling CMD_REG_RDY causes protocol error
 * - T32: Concurrent register writes and endpoint requests handled
 *
 * @note This test suite implements all 32 test cases mapped to EDN_FUNC_014
 *       in the edn-functionality-testcases.md document.
 *
 * Software Port Mode Architecture:
 * - Mode Entry: Set EDN_ENABLE=0x6 without BOOT_REQ_MODE or AUTO_REQ_MODE
 * - Firmware Control: All CSRNG commands issued via SW_CMD_REQ register
 * - Command Format: Header word (32-bit) + 0-12 additional data words
 * - Header Fields: [31:4]=acmd/flags/glen, [3:0]=clen
 * - Command Types: Instantiate (0x1), Reseed (0x2), Generate (0x3), Uninstantiate (0x5)
 * - Polling Requirements: CMD_REG_RDY before each word, CMD_RDY for new command
 * - Acknowledgment: CMD_ACK set when CSRNG responds, CMD_STS contains status code
 * - Interrupt: edn_cmd_req_done fires on command completion (if enabled)
 * - NIST Compliance: Instantiate before Generate/Reseed, Uninstantiate before reconfigure
 *
 * State Machine Behavior:
 * - Idle → SWPortMode: When EDN_ENABLE=0x6 without mode bits
 * - BootMode → SWPortMode: After clearing BOOT_REQ_MODE
 * - AutoMode → SWPortMode: After clearing AUTO_REQ_MODE
 * - SWPortMode persistent: Remains in SWPortMode for all software commands
 * - SWPortMode → Idle: When EDN_ENABLE cleared to 0x9
 *
 * Command Sequencing (NIST SP 800-90A):
 * 1. Instantiate: Initialize DRBG instance with seed material
 * 2. Generate/Reseed: Only after successful Instantiate
 * 3. Uninstantiate: Destroy instance before mode changes
 * 4. Sequencing violations: CSRNG returns error status
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-15
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>

/**
 * @class test_edn_func_014
 * @brief Test fixture for EDN_FUNC_014 verification
 *
 * Extends edn_test to provide comprehensive Software Port Mode operation testing.
 * Implements all 32 test cases for EDN_FUNC_014 covering mode activation,
 * all CSRNG command types, command interface protocol, interrupt generation,
 * error handling, and integration with endpoint distribution.
 */
class test_edn_func_014 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for software port mode test execution.
     */
    test_edn_func_014(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_014();

    /**
     * @brief Execute all EDN_FUNC_014 test cases
     * @return Number of failed tests
     *
     * Runs all 32 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: Software Port Mode Enable
    // =========================================================================
    /**
     * @brief Verify software port mode activation by setting EDN_ENABLE without mode bits
     *
     * Test Objective: Validate activation of software port mode by setting
     * EDN_ENABLE=0x6 in CTRL register without setting BOOT_REQ_MODE or
     * AUTO_REQ_MODE. Confirm state machine transitions to SWPortMode and
     * firmware has full control over CSRNG command issuance.
     *
     * Test Plan Reference: Test case #50 (test_sw_port_mode_enable)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Apply reset
     * 2. Read MAIN_SM_STATE (verify Idle state 0xC1)
     * 3. Read CTRL (verify reset value 0x00009999, all modes disabled)
     * 4. Write CTRL register:
     *    - EDN_ENABLE=0x6 (bits [3:0])
     *    - BOOT_REQ_MODE=0x9 (bits [7:4], disabled)
     *    - AUTO_REQ_MODE=0x9 (bits [11:8], disabled)
     *    - Value: 0x00009996
     * 5. Wait 10ns for state transition
     * 6. Read MAIN_SM_STATE (verify SWPortMode state 0x35)
     * 7. Read HW_CMD_STS (verify BOOT_MODE bit [0] = 0, AUTO_MODE bit [1] = 0)
     * 8. Read SW_CMD_STS (verify CMD_RDY bit [1] = 1, ready for commands)
     * 9. Verify CTRL configuration persisted
     * 10. Verify no automatic commands issued (hardware remains idle)
     *
     * Pass Criteria:
     * - CTRL register accepts EDN_ENABLE=0x6 without mode bits
     * - State machine transitions to SWPortMode
     * - HW_CMD_STS indicates no hardware modes active
     * - SW_CMD_STS.CMD_RDY indicates readiness for firmware commands
     * - EDN waits for firmware command issuance
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_port_mode_enable();

    // =========================================================================
    // Test Case 2: Instantiate Command
    // =========================================================================
    /**
     * @brief Verify firmware issues Instantiate command with full parameter control
     *
     * Test Objective: Validate firmware-controlled Instantiate command
     * issuance via SW_CMD_REQ in software port mode with full control over
     * command parameters including personalization string support.
     *
     * Test Plan Reference: Test case #51 (test_sw_port_instantiate_command)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Apply reset and enable software port mode
     * 2. Poll SW_CMD_STS.CMD_RDY until ready (bit [1] = 1)
     * 3. Issue Instantiate command via SW_CMD_REQ:
     *    - Write header 0x00000901 (acmd=1, clen=0, flags=0, glen=0)
     * 4. Wait for command processing
     * 5. Poll SW_CMD_STS.CMD_ACK until set (bit [2] = 1)
     * 6. Read SW_CMD_STS.CMD_STS (bits [5:3], verify 0x0 = success)
     * 7. Verify INTR_STATE.edn_cmd_req_done set (bit [0] = 1) if interrupt enabled
     * 8. Clear interrupt (write 0x1 to INTR_STATE)
     * 9. Read MAIN_SM_STATE (verify remains SWPortMode)
     * 10. Test with personalization string:
     *     - Issue Instantiate with clen=3 (3 data words)
     *     - Poll CMD_REG_RDY before each data word write
     *     - Write 3 personalization data words
     *     - Verify successful completion
     *
     * Pass Criteria:
     * - SW_CMD_REQ accepts Instantiate command header
     * - CMD_ACK asserted on completion
     * - CMD_STS = 0x0 (success)
     * - edn_cmd_req_done interrupt fires
     * - State remains SWPortMode
     * - Personalization string support functional
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_instantiate_command();

    // =========================================================================
    // Test Case 3: Generate Command
    // =========================================================================
    /**
     * @brief Verify firmware issues Generate command with configurable glen
     *
     * Test Objective: Validate firmware-controlled Generate command with
     * configurable glen parameter to control entropy amount requested from
     * CSRNG and received via genbits interface.
     *
     * Test Plan Reference: Test case #52 (test_sw_port_generate_command)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Enable software port mode and issue Instantiate
     * 2. Poll SW_CMD_STS.CMD_RDY
     * 3. Issue Generate command:
     *    - Write header 0x00FFF003 (acmd=3, glen=0xFFF, clen=0)
     * 4. Wait for CSRNG to provide entropy
     * 5. Poll SW_CMD_STS.CMD_ACK
     * 6. Verify CMD_STS = 0x0
     * 7. Verify 128-bit entropy received via csrng_genbits_target_socket
     * 8. Verify entropy available for endpoint distribution
     * 9. Test variable glen values:
     *    - Glen=1 (128 bits)
     *    - Glen=4 (512 bits)
     *    - Glen=16 (2048 bits)
     * 10. Verify interrupt generated on each completion
     *
     * Pass Criteria:
     * - Generate command accepted
     * - Glen parameter controls entropy amount
     * - Entropy received from CSRNG
     * - CMD_ACK and CMD_STS correct
     * - Interrupt fires on completion
     * - State remains SWPortMode
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_generate_command();

    // =========================================================================
    // Test Case 4: Reseed Command
    // =========================================================================
    /**
     * @brief Verify firmware issues Reseed command with additional data
     *
     * Test Objective: Validate firmware-controlled Reseed command with
     * additional data support for refreshing DRBG entropy seed without
     * full re-instantiation.
     *
     * Test Plan Reference: Test case #53 (test_sw_port_reseed_command)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Enable software port mode, issue Instantiate and Generate
     * 2. Poll SW_CMD_STS.CMD_RDY
     * 3. Issue Reseed command without additional data:
     *    - Write header 0x00000402 (acmd=2, clen=0)
     * 4. Verify successful completion
     * 5. Issue Reseed with additional data:
     *    - Write header 0x00000432 (acmd=2, clen=3)
     *    - Poll CMD_REG_RDY before each data word
     *    - Write 3 additional data words
     * 6. Verify CMD_ACK and CMD_STS
     * 7. Issue Generate after Reseed
     * 8. Verify fresh entropy available
     * 9. Verify interrupt fires
     *
     * Pass Criteria:
     * - Reseed command accepted
     * - Additional data support functional
     * - CMD_ACK and CMD_STS correct
     * - Generate works after Reseed
     * - Interrupt generated
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_reseed_command();

    // =========================================================================
    // Test Case 5: Uninstantiate Command
    // =========================================================================
    /**
     * @brief Verify firmware issues Uninstantiate command before disabling
     *
     * Test Objective: Validate firmware-controlled Uninstantiate command
     * to destroy CSRNG instance before EDN reconfiguration or disable,
     * maintaining synchronization between EDN and CSRNG.
     *
     * Test Plan Reference: Test case #54 (test_sw_port_uninstantiate_command)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Enable software port mode and issue Instantiate
     * 2. Issue several Generate commands
     * 3. Poll SW_CMD_STS.CMD_RDY
     * 4. Issue Uninstantiate command:
     *    - Write header 0x00000005 (acmd=5, clen=0)
     * 5. Poll CMD_ACK
     * 6. Verify CMD_STS = 0x0
     * 7. Attempt Generate after Uninstantiate (should fail)
     * 8. Clear EDN_ENABLE (disable EDN)
     * 9. Verify state returns to Idle
     * 10. Re-enable and verify requires new Instantiate
     *
     * Pass Criteria:
     * - Uninstantiate command accepted
     * - CMD_ACK and CMD_STS correct
     * - Generate fails after Uninstantiate
     * - Clean disable after Uninstantiate
     * - Re-enable requires new Instantiate
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_uninstantiate_command();

    // =========================================================================
    // Test Case 6: Multi-Word Command Sequences
    // =========================================================================
    /**
     * @brief Verify multi-word command sequences (header + up to 12 data words)
     *
     * Test Objective: Validate firmware can issue multi-word commands
     * consisting of header word plus up to 12 additional data words by
     * polling CMD_REG_RDY before each word write.
     *
     * Test Plan Reference: Test case #55 (test_sw_port_multiword_command)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Enable software port mode and issue Instantiate
     * 2. Poll SW_CMD_STS.CMD_RDY
     * 3. Issue Instantiate with maximum additional data (clen=12):
     *    - Write header 0x0000090C (acmd=1, clen=12)
     *    - For i = 0 to 11:
     *      a. Poll CMD_REG_RDY (bit [0])
     *      b. Verify CMD_REG_RDY = 1
     *      c. Write data word i to SW_CMD_REQ
     *      d. Wait 5ns
     * 4. Poll CMD_ACK
     * 5. Verify CMD_STS = 0x0
     * 6. Test boundary cases:
     *    - Clen=1 (minimum with data)
     *    - Clen=6 (mid-range)
     *    - Clen=12 (maximum)
     * 7. Verify each completes successfully
     *
     * Pass Criteria:
     * - Up to 12 data words accepted
     * - CMD_REG_RDY polling functional
     * - All word writes successful
     * - Command completes correctly
     * - CMD_ACK and CMD_STS correct
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_multiword_command();

    // =========================================================================
    // Test Case 7: Command Completion Interrupt
    // =========================================================================
    /**
     * @brief Verify edn_cmd_req_done interrupt on software command completion
     *
     * Test Objective: Validate edn_cmd_req_done interrupt asserts when
     * software command completes (CSRNG acknowledges) and INTR_ENABLE is set,
     * providing asynchronous completion notification.
     *
     * Test Plan Reference: Test case #56 (test_sw_port_cmd_completion_interrupt)
     * Functionality: EDN_FUNC_014
     *
     * Procedure:
     * 1. Enable software port mode
     * 2. Enable interrupt: Write 0x1 to INTR_ENABLE.edn_cmd_req_done (bit [0])
     * 3. Issue Instantiate command
     * 4. Monitor intr_edn_cmd_req_done signal
     * 5. Verify interrupt asserts when CMD_ACK set
     * 6. Read INTR_STATE (verify bit [0] = 1)
     * 7. Clear interrupt (write 0x1 to INTR_STATE bit [0])
     * 8. Verify interrupt deasserts
     * 9. Test interrupt disabled:
     *    - Write 0x0 to INTR_ENABLE
     *    - Issue Generate command
     *    - Verify interrupt does not assert
     *    - Verify INTR_STATE.edn_cmd_req_done still set
     * 10. Test interrupt for all command types
     *
     * Pass Criteria:
     * - Interrupt fires on command completion when enabled
     * - INTR_STATE bit set correctly
     * - W1C clearing functional
     * - Interrupt masked when disabled
     * - Works for all command types
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_completion_interrupt();

    // =========================================================================
    // Test Case 8: Entropy Distribution to Endpoints
    // =========================================================================
    /**
     * @brief Verify software port mode distributes entropy with FIPS indicator
     *
     * Test Objective: Validate software port mode correctly distributes
     * entropy to endpoint interfaces with proper FIPS compliance indicator
     * propagation after Generate commands.
     *
     * Test Plan Reference: Test case #57 (test_sw_port_entropy_distribution)
     * Functionality: EDN_FUNC_014 + EDN_FUNC_005 + EDN_FUNC_011
     *
     * Procedure:
     * 1. Enable software port mode
     * 2. Issue Instantiate command
     * 3. Issue Generate command (glen=0xFFF)
     * 4. Wait for CSRNG entropy (128-bit, FIPS=true)
     * 5. Request entropy from endpoint 0 (edn_req[0])
     * 6. Verify edn_ack[0] asserted
     * 7. Read edn_bus[0] (verify 32-bit entropy data)
     * 8. Read edn_fips[0] (verify = 1, FIPS-approved)
     * 9. Request from all 8 endpoints sequentially
     * 10. Verify each receives entropy with FIPS indicator
     * 11. Test pre-FIPS entropy (simulate FIPS=false from CSRNG)
     * 12. Verify edn_fips signals de-asserted
     * 13. Test data persistence on edn_bus
     * 14. Verify 128-to-32 bit conversion
     *
     * Pass Criteria:
     * - Entropy distributed to all endpoints
     * - FIPS indicator propagated correctly
     * - 32-bit data width conversion accurate
     * - Data persists until next request
     * - Fresh entropy for each Generate
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_entropy_distribution();

    // Additional test cases continue...
    // (T9-T32 declarations follow similar pattern)

    bool test_sw_cmd_req_interface();           // T9
    bool test_cmd_reg_rdy_polling();            // T10
    bool test_cmd_rdy_indication();             // T11
    bool test_cmd_ack_mechanism();              // T12
    bool test_cmd_sts_field();                  // T13
    bool test_state_swportmode_stable();        // T14
    bool test_command_header_parsing();         // T15
    bool test_clen_validation();                // T16
    bool test_clen_mismatch_error();            // T17
    bool test_personalization_string();         // T18
    bool test_glen_parameter();                 // T19
    bool test_glen_maximum();                   // T20
    bool test_csrng_instantiate_sequence();     // T21
    bool test_csrng_generate_sequence();        // T22
    bool test_csrng_reseed_sequence();          // T23
    bool test_csrng_uninstantiate_sequence();   // T24
    bool test_command_ordering_nist();          // T25
    bool test_acknowledgment_success();         // T26
    bool test_acknowledgment_error();           // T27
    bool test_disable_sequence();               // T28
    bool test_reconfiguration_sequence();       // T29
    bool test_reset_during_command();           // T30
    bool test_write_without_polling();          // T31
    bool test_concurrent_access();              // T32

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Enable software port mode
     *
     * Writes CTRL register to enable software port mode (EDN_ENABLE=0x6,
     * BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9).
     */
    void enable_sw_port_mode();

    /**
     * @brief Issue CSRNG command via SW_CMD_REQ
     * @param cmd_header Command header word
     * @param data_words Pointer to additional data words (NULL if none)
     * @param num_data_words Number of additional data words
     * @return true if command issued successfully
     *
     * Polls CMD_REG_RDY before each word and issues complete command.
     */
    bool issue_sw_command(uint32_t cmd_header,
                          const uint32_t* data_words = nullptr,
                          unsigned int num_data_words = 0);

    /**
     * @brief Wait for software command completion
     * @param timeout_ns Timeout in nanoseconds
     * @return true if command completed before timeout
     *
     * Polls SW_CMD_STS.CMD_ACK until set or timeout.
     */
    bool wait_for_sw_command_completion(double timeout_ns = 1000.0);

    /**
     * @brief Read SW_CMD_STS fields
     * @param cmd_reg_rdy Output: CMD_REG_RDY bit value
     * @param cmd_rdy Output: CMD_RDY bit value
     * @param cmd_ack Output: CMD_ACK bit value
     * @param cmd_sts Output: CMD_STS field value
     */
    void read_sw_cmd_sts(unsigned int& cmd_reg_rdy,
                         unsigned int& cmd_rdy,
                         unsigned int& cmd_ack,
                         unsigned int& cmd_sts);

    /**
     * @brief Simulate endpoint entropy request and verify delivery
     * @param endpoint_id Endpoint index (0-7)
     * @param expected_fips Expected FIPS indicator value
     * @return true if entropy delivered correctly
     */
    bool request_and_verify_entropy(unsigned int endpoint_id,
                                    bool expected_fips = true);

    /**
     * @brief Verify state machine state
     * @param expected_state Expected MAIN_SM_STATE value
     * @return true if state matches
     */
    bool verify_state(uint32_t expected_state);

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
    // Register Offsets (inherited from edn_test base)
    // =========================================================================

    // CSRNG command types (acmd field in command header)
    static constexpr uint32_t ACMD_INSTANTIATE = 0x1;
    static constexpr uint32_t ACMD_RESEED = 0x2;
    static constexpr uint32_t ACMD_GENERATE = 0x3;
    static constexpr uint32_t ACMD_UPDATE = 0x4;
    static constexpr uint32_t ACMD_UNINSTANTIATE = 0x5;

    // Multi-bit encoded values
    static constexpr uint32_t MBE_ENABLE = 0x6;
    static constexpr uint32_t MBE_DISABLE = 0x9;

    // State machine states
    static constexpr uint32_t STATE_IDLE = 0xC1;
    static constexpr uint32_t STATE_SWPORTMODE = 0x96;

    // Maximum command parameters
    static constexpr unsigned int MAX_CLEN = 12;   // Maximum additional data words
    static constexpr uint32_t MAX_GLEN = 0xFFF;    // Maximum glen value (4096 blocks)

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

    bool m_sw_port_mode_enabled;                 ///< Software port mode currently enabled
    bool m_instantiate_issued;                   ///< Instantiate command issued
    bool m_csrng_ready;                          ///< CSRNG ready for commands
};
