// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_edn_func_004.h
 * @brief EDN_FUNC_004 Test Suite - CSRNG Interface and Command Management Verification
 *
 * Comprehensive test suite for EDN_FUNC_004 (CSRNG Interface and Command Management) covering:
 * - SW command FIFO and multi-word command accumulation
 * - Command header parsing (clen field determines command boundaries)
 * - CSRNG command forwarding (Instantiate, Generate, Reseed, Uninstantiate)
 * - SW_CMD_STS and HW_CMD_STS register status tracking
 * - CSRNG acknowledgment handling (success and error cases)
 * - Command completion interrupts
 * - Dual alert mechanism for CSRNG errors (recoverable + fatal)
 * - HW command infrastructure (boot mode and auto mode)
 * - Entropy buffer management (128-bit to 32-bit conversion)
 *
 * Test Coverage (29 Test Cases):
 * - TC_EDN_CSRNG_INF_001: SW Command Request - Single Word (Uninstantiate)
 * - TC_EDN_CSRNG_INF_002: SW Command Request - Multi-Word (Instantiate with seed)
 * - TC_EDN_CSRNG_INF_003: SW Command Request - Generate Command
 * - TC_EDN_CSRNG_INF_004: SW Command Request - Reseed Command
 * - TC_EDN_CSRNG_INF_005: SW_CMD_STS Status Tracking
 * - TC_EDN_CSRNG_INF_006: HW_CMD_STS Status Tracking (Boot Mode)
 * - TC_EDN_CSRNG_INF_007: CSRNG Acknowledgment Success
 * - TC_EDN_CSRNG_INF_008: CSRNG Acknowledgment Error
 * - TC_EDN_CSRNG_INF_009: Command FIFO Boundary Tracking (clen=0)
 * - TC_EDN_CSRNG_INF_010: Command FIFO Boundary Tracking (clen=12, max words)
 * - TC_EDN_CSRNG_INF_011: CMD_REG_RDY Semantics During Multi-Word Command
 * - TC_EDN_CSRNG_INF_012: Generate Command - genbits Reception
 * - TC_EDN_CSRNG_INF_013: FIPS Compliance Propagation
 * - TC_EDN_CSRNG_INF_014: Interrupt on Command Completion
 * - TC_EDN_CSRNG_INF_015: Fatal Error on CSRNG Error
 * - TC_EDN_CSRNG_INF_016: SW Command Rejected When EDN Disabled
 * - TC_EDN_CSRNG_INF_017: Boot Mode Hardware Command Sequence
 * - TC_EDN_CSRNG_INF_018: Auto Mode Hardware Command Sequence
 * - TC_EDN_CSRNG_INF_019: HW Command - Update HW_CMD_STS Register
 * - TC_EDN_CSRNG_INF_020: HW Command - CSRNG Error Handling
 * - TC_EDN_CSRNG_INF_021: Entropy Buffer Management (128-bit to 32-bit)
 * - TC_EDN_CSRNG_INF_022: Multiple SW Commands Sequential
 * - TC_EDN_CSRNG_INF_023: SW Command While Boot Mode Active
 * - TC_EDN_CSRNG_INF_024: Invalid clen Field (exceeds 12 words)
 * - TC_EDN_CSRNG_INF_025: SW Command Status After Reset
 * - TC_EDN_CSRNG_INF_026: HW Command Status After Reset
 * - TC_EDN_CSRNG_INF_027: Recoverable Alert on CSRNG Error
 * - TC_EDN_CSRNG_INF_028: Fatal Alert on CSRNG Error
 * - TC_EDN_CSRNG_INF_029: Command Interruption by Disable
 *
 * @note This test suite implements all 29 test cases mapped to EDN_FUNC_004
 *       in the edn-functionality-testcases.md document.
 *
 * Implementation Strategy:
 * - Each test case is self-contained with setup, execution, and validation phases
 * - Tests use edn_test infrastructure for register access and CSRNG simulation
 * - Comprehensive coverage of command header parsing (cmd, acmd, clen, flags, glen)
 * - Tests validate status register updates (SW_CMD_STS, HW_CMD_STS)
 * - CSRNG error injection and dual alert mechanism validation
 * - Failed tests report detailed diagnostic information
 *
 * @author Claude Code (SystemC Test Case Artisan)
 * @date 2026-01-13
 */

#pragma once
#include "edn_test.h"
#include <vector>
#include <string>
#include <sstream>

/**
 * @class test_edn_func_004
 * @brief Test fixture for EDN_FUNC_004 verification
 *
 * Extends edn_test to provide comprehensive CSRNG interface and command management testing.
 * Implements all 29 test cases for EDN_FUNC_004 covering SW commands, HW commands,
 * status tracking, error handling, and entropy distribution.
 */
class test_edn_func_004 : public edn_test
{
  public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     *
     * Initializes test fixture and prepares for test execution.
     */
    test_edn_func_004(sc_module_name name);

    /**
     * @brief Destructor
     */
    ~test_edn_func_004();

    /**
     * @brief Execute all EDN_FUNC_004 test cases
     * @return Number of failed tests
     *
     * Runs all 29 test cases in sequence with comprehensive result reporting.
     */
    unsigned int run_all_tests();

  private:
    // =========================================================================
    // Test Case 1: SW Command Request - Single Word (Uninstantiate)
    // =========================================================================
    /**
     * @brief Verify single-word SW command (Uninstantiate) via SW_CMD_REQ
     *
     * Test Objective: Validate that SW_CMD_REQ accepts single-word command
     * (clen=0) and forwards to CSRNG without additional data words.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_001
     * Functionality: EDN_FUNC_004 (CSRNG Interface and Command Management)
     *
     * Command Format:
     * - bits[3:0]: cmd=5 (Uninstantiate)
     * - bits[7:4]: acmd=0
     * - bits[11:8]: clen=0 (no additional data)
     * - bits[31:12]: reserved/flags
     *
     * Procedure:
     * 1. Enable EDN in software port mode (CTRL.EDN_ENABLE=0x6)
     * 2. Check SW_CMD_STS.CMD_REG_RDY=1 and CMD_RDY=1
     * 3. Write Uninstantiate command header to SW_CMD_REQ (0x00000005)
     * 4. Wait for CSRNG command forwarding
     * 5. Verify SW_CMD_STS.CMD_ACK=1 after acknowledgment
     * 6. Check SW_CMD_STS.CMD_STS=0 (success)
     *
     * Pass Criteria:
     * - Single-word command accepted and forwarded to CSRNG
     * - No additional word polling required (clen=0)
     * - SW_CMD_STS updated correctly with acknowledgment
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_single_word_uninstantiate();

    // =========================================================================
    // Test Case 2: SW Command Request - Multi-Word (Instantiate with seed)
    // =========================================================================
    /**
     * @brief Verify multi-word SW command (Instantiate with personalization string)
     *
     * Test Objective: Validate multi-word command sequence with clen field
     * parsing to determine command boundaries.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_002
     * Functionality: EDN_FUNC_004
     *
     * Command Format (Header + 4 data words):
     * - Header: cmd=1 (Instantiate), acmd=0, clen=4, flags=0x1 (enable entropy)
     * - Data words: 4x 32-bit personalization string data
     *
     * Procedure:
     * 1. Enable EDN in software port mode
     * 2. Write command header to SW_CMD_REQ (0x00001041)
     * 3. Poll SW_CMD_STS.CMD_REG_RDY before each data word write
     * 4. Write 4 data words sequentially
     * 5. Verify command forwarded after last word
     * 6. Check SW_CMD_STS.CMD_ACK=1
     *
     * Pass Criteria:
     * - Multi-word command accepted with correct boundary detection
     * - CMD_REG_RDY polling required between words
     * - All 5 words (header + 4 data) forwarded to CSRNG
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_multi_word_instantiate();

    // =========================================================================
    // Test Case 3: SW Command Request - Generate Command
    // =========================================================================
    /**
     * @brief Verify Generate command with glen parameter
     *
     * Test Objective: Validate Generate command format with configurable
     * glen (generated length) parameter controlling entropy amount.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_003
     * Functionality: EDN_FUNC_004
     *
     * Command Format:
     * - Header: cmd=3 (Generate), clen=0, glen=0x100 (256 blocks)
     * - Header value: 0x01000003
     *
     * Procedure:
     * 1. Enable EDN and ensure instantiated state
     * 2. Write Generate command to SW_CMD_REQ
     * 3. Wait for CSRNG to provide entropy via genbits interface
     * 4. Verify SW_CMD_STS.CMD_ACK=1
     * 5. Check entropy buffer populated with 128-bit blocks
     *
     * Pass Criteria:
     * - Generate command accepted with glen parameter
     * - CSRNG provides entropy data via genbits interface
     * - SW_CMD_STS reflects successful completion
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_generate();

    // =========================================================================
    // Test Case 4: SW Command Request - Reseed Command
    // =========================================================================
    /**
     * @brief Verify Reseed command with additional data
     *
     * Test Objective: Validate Reseed command format with optional
     * additional data for seed refreshing.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_004
     * Functionality: EDN_FUNC_004
     *
     * Command Format (Header + 2 data words):
     * - Header: cmd=4 (Reseed), clen=2, flags=0x1
     * - Data words: 2x 32-bit additional seed data
     *
     * Procedure:
     * 1. Ensure EDN in instantiated state
     * 2. Write Reseed command header (0x00001024)
     * 3. Write 2 data words
     * 4. Verify command forwarded to CSRNG
     * 5. Check SW_CMD_STS.CMD_ACK=1
     *
     * Pass Criteria:
     * - Reseed command with clen=2 accepted
     * - Additional data words forwarded correctly
     * - Command completes successfully
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_reseed();

    // =========================================================================
    // Test Case 5: SW_CMD_STS Status Tracking
    // =========================================================================
    /**
     * @brief Verify SW_CMD_STS register status field updates
     *
     * Test Objective: Validate all SW_CMD_STS fields correctly reflect
     * command processing state and CSRNG acknowledgment.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_005
     * Functionality: EDN_FUNC_004
     *
     * SW_CMD_STS Fields:
     * - CMD_REG_RDY [0]: Ready to accept next command word
     * - CMD_RDY [1]: Ready for new multi-word command sequence
     * - CMD_ACK [2]: CSRNG acknowledged command completion
     * - CMD_STS [3:10]: CSRNG command status code
     *
     * Procedure:
     * 1. Read SW_CMD_STS reset values
     * 2. Issue SW command and monitor field changes
     * 3. Verify CMD_REG_RDY transitions during multi-word write
     * 4. Check CMD_ACK set after CSRNG acknowledgment
     * 5. Verify CMD_STS reflects CSRNG status code
     *
     * Pass Criteria:
     * - All fields accurately reflect command processing state
     * - Status updates occur at correct phases
     * - CMD_STS matches CSRNG acknowledgment status
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_sts_status_tracking();

    // =========================================================================
    // Test Case 6: HW_CMD_STS Status Tracking (Boot Mode)
    // =========================================================================
    /**
     * @brief Verify HW_CMD_STS register updates during boot mode operation
     *
     * Test Objective: Validate HW_CMD_STS fields reflect hardware-issued
     * command status during boot-time request mode.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_006
     * Functionality: EDN_FUNC_004
     *
     * HW_CMD_STS Fields:
     * - BOOT_MODE [0]: Boot-time request mode active
     * - AUTO_MODE [1]: Auto request mode active
     * - CMD_TYPE [2:5]: Last hardware command type encoding
     * - CMD_ACK [6]: Hardware command acknowledged
     * - CMD_STS [7:14]: Hardware command status code
     *
     * Procedure:
     * 1. Configure boot mode (CTRL.BOOT_REQ_MODE=0x6)
     * 2. Enable EDN (CTRL.EDN_ENABLE=0x6)
     * 3. Monitor HW_CMD_STS.BOOT_MODE=1
     * 4. Wait for automatic Instantiate command
     * 5. Check CMD_TYPE reflects Instantiate encoding
     * 6. Verify CMD_ACK=1 after CSRNG acknowledgment
     *
     * Pass Criteria:
     * - HW_CMD_STS.BOOT_MODE set during boot mode
     * - CMD_TYPE reflects hardware-issued commands
     * - CMD_ACK and CMD_STS updated correctly
     *
     * @return true if test passes, false otherwise
     */
    bool test_hw_cmd_sts_boot_mode();

    // =========================================================================
    // Test Case 7: CSRNG Acknowledgment Success
    // =========================================================================
    /**
     * @brief Verify successful CSRNG acknowledgment (status=0)
     *
     * Test Objective: Validate that CSRNG acknowledgment with status code 0
     * indicates successful command completion without errors.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_007
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue SW command (e.g., Instantiate)
     * 2. Simulate CSRNG acknowledgment with status=0
     * 3. Verify SW_CMD_STS.CMD_STS=0
     * 4. Check no error alerts generated
     * 5. Verify interrupt asserted (if enabled)
     *
     * Pass Criteria:
     * - CMD_STS field shows 0 (success)
     * - No RECOV_ALERT_STS or ERR_CODE bits set
     * - Command completion interrupt fires
     *
     * @return true if test passes, false otherwise
     */
    bool test_csrng_ack_success();

    // =========================================================================
    // Test Case 8: CSRNG Acknowledgment Error
    // =========================================================================
    /**
     * @brief Verify CSRNG error acknowledgment triggers dual alert mechanism
     *
     * Test Objective: Validate that non-zero CSRNG status code triggers
     * both recoverable alert (RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT) and
     * fatal error (ERR_CODE.SFIFO_ESRNG_ERR).
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_008
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue SW command
     * 2. Simulate CSRNG acknowledgment with non-zero status (e.g., 0x3)
     * 3. Verify RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT=1
     * 4. Check ERR_CODE.SFIFO_ESRNG_ERR=1
     * 5. Verify alert_recov_alert asserted
     * 6. Check alert_fatal_alert asserted
     * 7. Verify intr_edn_fatal_err interrupt
     *
     * Pass Criteria:
     * - Dual alert mechanism triggered
     * - Recoverable alert status bit set
     * - Fatal error code bit set (sticky until reset)
     * - Both alert signals asserted
     *
     * @return true if test passes, false otherwise
     */
    bool test_csrng_ack_error();

    // =========================================================================
    // Test Case 9: Command FIFO Boundary Tracking (clen=0)
    // =========================================================================
    /**
     * @brief Verify command boundary detection with clen=0 (single word)
     *
     * Test Objective: Validate that command with clen=0 is recognized as
     * complete after header word, no additional words expected.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_009
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Write command header with clen=0
     * 2. Verify command forwarded immediately
     * 3. Check SW_CMD_STS.CMD_RDY returns to 1 without waiting for data words
     * 4. Verify no CMD_REG_RDY polling required
     *
     * Pass Criteria:
     * - Command recognized as complete after header
     * - No additional word accumulation
     * - Command forwarded to CSRNG with 1 word only
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_boundary_clen_0();

    // =========================================================================
    // Test Case 10: Command FIFO Boundary Tracking (clen=12, max words)
    // =========================================================================
    /**
     * @brief Verify command boundary detection with clen=12 (maximum)
     *
     * Test Objective: Validate that command with clen=12 accumulates
     * 12 additional data words before forwarding (13 total words).
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_010
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Write command header with clen=12 (0x00000C01 for Instantiate)
     * 2. Poll CMD_REG_RDY and write 12 data words
     * 3. Verify command forwarded after 13th word
     * 4. Check all 13 words sent to CSRNG
     *
     * Pass Criteria:
     * - Command accumulates exactly 12 data words
     * - Forwarding occurs after last expected word
     * - No premature or delayed forwarding
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_boundary_clen_12_max();

    // =========================================================================
    // Test Case 11: CMD_REG_RDY Semantics During Multi-Word Command
    // =========================================================================
    /**
     * @brief Verify CMD_REG_RDY behavior during multi-word command assembly
     *
     * Test Objective: Validate that CMD_REG_RDY indicates FIFO readiness
     * to accept next word during multi-word command sequence.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_011
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Write command header with clen>0
     * 2. Monitor CMD_REG_RDY between each word write
     * 3. Verify CMD_REG_RDY=1 before each write is accepted
     * 4. Check that writing without polling may cause protocol errors
     *
     * Pass Criteria:
     * - CMD_REG_RDY indicates readiness for each word
     * - Firmware must poll before each SW_CMD_REQ write
     * - Violating protocol may cause command loss
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_reg_rdy_semantics();

    // =========================================================================
    // Test Case 12: Generate Command - genbits Reception
    // =========================================================================
    /**
     * @brief Verify entropy data reception via genbits interface after Generate
     *
     * Test Objective: Validate that Generate command triggers CSRNG to
     * provide 128-bit entropy blocks via genbits interface.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_012
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue Generate command with glen=0x1 (1 block)
     * 2. Simulate CSRNG providing 128-bit entropy via genbits
     * 3. Verify EDN receives and buffers entropy data
     * 4. Check entropy buffer contains correct data
     * 5. Verify 128-bit to 32-bit conversion for endpoints
     *
     * Pass Criteria:
     * - Entropy data received from CSRNG
     * - Data correctly buffered internally
     * - Available for endpoint distribution
     *
     * @return true if test passes, false otherwise
     */
    bool test_generate_genbits_reception();

    // =========================================================================
    // Test Case 13: FIPS Compliance Propagation
    // =========================================================================
    /**
     * @brief Verify FIPS compliance indicator propagation from CSRNG
     *
     * Test Objective: Validate that FIPS indicator from CSRNG genbits
     * is propagated to endpoint FIPS signals.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_013
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue Generate command
     * 2. Provide entropy with FIPS=true via genbits
     * 3. Verify internal FIPS status stored
     * 4. Request entropy from endpoint
     * 5. Check edn_fips signal reflects FIPS status
     *
     * Pass Criteria:
     * - FIPS indicator received from CSRNG
     * - Stored with entropy data
     * - Propagated to endpoint interfaces
     *
     * @return true if test passes, false otherwise
     */
    bool test_fips_propagation();

    // =========================================================================
    // Test Case 14: Interrupt on Command Completion
    // =========================================================================
    /**
     * @brief Verify intr_edn_cmd_req_done interrupt on SW command completion
     *
     * Test Objective: Validate that SW command completion triggers
     * intr_edn_cmd_req_done interrupt when INTR_ENABLE is set.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_014
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable command completion interrupt (INTR_ENABLE.edn_cmd_req_done=1)
     * 2. Issue SW command
     * 3. Wait for CSRNG acknowledgment
     * 4. Verify INTR_STATE.edn_cmd_req_done=1
     * 5. Check intr_edn_cmd_req_done signal asserted
     * 6. Clear interrupt by writing 1 to INTR_STATE
     *
     * Pass Criteria:
     * - Interrupt status bit set on completion
     * - Interrupt signal asserted when enabled
     * - W1C clearing mechanism works
     *
     * @return true if test passes, false otherwise
     */
    bool test_interrupt_cmd_completion();

    // =========================================================================
    // Test Case 15: Fatal Error on CSRNG Error
    // =========================================================================
    /**
     * @brief Verify fatal error interrupt on CSRNG error status
     *
     * Test Objective: Validate that CSRNG error triggers fatal error
     * interrupt (intr_edn_fatal_err) and sets ERR_CODE.SFIFO_ESRNG_ERR.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_015
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable fatal error interrupt (INTR_ENABLE.edn_fatal_err=1)
     * 2. Issue SW command
     * 3. Simulate CSRNG error acknowledgment (status!=0)
     * 4. Verify ERR_CODE.SFIFO_ESRNG_ERR=1
     * 5. Check INTR_STATE.edn_fatal_err=1
     * 6. Verify intr_edn_fatal_err signal asserted
     * 7. Confirm ERR_CODE is sticky (cannot be cleared by write)
     *
     * Pass Criteria:
     * - Fatal error code bit set on CSRNG error
     * - Fatal error interrupt generated
     * - ERR_CODE sticky until reset
     *
     * @return true if test passes, false otherwise
     */
    bool test_fatal_error_on_csrng_error();

    // =========================================================================
    // Test Case 16: SW Command Rejected When EDN Disabled
    // =========================================================================
    /**
     * @brief Verify SW commands rejected when EDN is disabled
     *
     * Test Objective: Validate that SW_CMD_REQ writes are ignored or
     * rejected when CTRL.EDN_ENABLE!=0x6.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_016
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Ensure EDN disabled (CTRL.EDN_ENABLE=0x9)
     * 2. Attempt to write command to SW_CMD_REQ
     * 3. Check SW_CMD_STS.CMD_RDY=0 (not ready)
     * 4. Verify no command forwarded to CSRNG
     * 5. Enable EDN and retry command
     * 6. Verify command accepted after enable
     *
     * Pass Criteria:
     * - Commands rejected when disabled
     * - CMD_RDY indicates not ready state
     * - No spurious CSRNG transactions
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_rejected_when_disabled();

    // =========================================================================
    // Test Case 17: Boot Mode Hardware Command Sequence
    // =========================================================================
    /**
     * @brief Verify automatic hardware command sequence in boot-time mode
     *
     * Test Objective: Validate boot mode issues Instantiate then Generate
     * commands automatically using BOOT_INS_CMD and BOOT_GEN_CMD registers.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_017
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Configure BOOT_INS_CMD and BOOT_GEN_CMD registers
     * 2. Enable boot mode (CTRL.BOOT_REQ_MODE=0x6)
     * 3. Enable EDN (CTRL.EDN_ENABLE=0x6)
     * 4. Monitor automatic Instantiate command
     * 5. Verify HW_CMD_STS updated for Instantiate
     * 6. Wait for automatic Generate command
     * 7. Check HW_CMD_STS reflects Generate
     *
     * Pass Criteria:
     * - Two commands issued automatically
     * - Sequence: Instantiate → Generate
     * - HW_CMD_STS tracks both commands
     *
     * @return true if test passes, false otherwise
     */
    bool test_boot_mode_hw_cmd_sequence();

    // =========================================================================
    // Test Case 18: Auto Mode Hardware Command Sequence
    // =========================================================================
    /**
     * @brief Verify automatic hardware command sequence in auto request mode
     *
     * Test Objective: Validate auto mode issues Generate commands from
     * GENERATE_CMD FIFO and Reseed commands based on MAX_NUM_REQS_BETWEEN_RESEEDS.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_018
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Configure GENERATE_CMD and RESEED_CMD FIFOs
     * 2. Set MAX_NUM_REQS_BETWEEN_RESEEDS
     * 3. Manually instantiate via SW_CMD_REQ
     * 4. Enable auto mode (CTRL.AUTO_REQ_MODE=0x6)
     * 5. Trigger endpoint request to cause Generate
     * 6. Verify HW_CMD_STS.CMD_TYPE reflects Generate
     * 7. After N generates, verify Reseed issued
     *
     * Pass Criteria:
     * - Hardware issues Generate automatically
     * - Reseed triggered after configured interval
     * - HW_CMD_STS tracks command types
     *
     * @return true if test passes, false otherwise
     */
    bool test_auto_mode_hw_cmd_sequence();

    // =========================================================================
    // Test Case 19: HW Command - Update HW_CMD_STS Register
    // =========================================================================
    /**
     * @brief Verify HW_CMD_STS register updates for hardware-issued commands
     *
     * Test Objective: Validate that hardware commands (boot/auto mode)
     * correctly update HW_CMD_STS fields including CMD_TYPE encoding.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_019
     * Functionality: EDN_FUNC_004
     *
     * CMD_TYPE Encoding:
     * - 0x1: Instantiate
     * - 0x2: Reseed
     * - 0x3: Generate
     * - 0x4: Uninstantiate
     *
     * Procedure:
     * 1. Enable hardware mode (boot or auto)
     * 2. Monitor HW_CMD_STS.CMD_TYPE during commands
     * 3. Verify CMD_TYPE matches issued command type
     * 4. Check CMD_ACK set after acknowledgment
     * 5. Verify CMD_STS reflects CSRNG status
     *
     * Pass Criteria:
     * - CMD_TYPE encoding correct for each command
     * - CMD_ACK and CMD_STS updated properly
     * - Register read-only (writes ignored)
     *
     * @return true if test passes, false otherwise
     */
    bool test_hw_cmd_updates_hw_cmd_sts();

    // =========================================================================
    // Test Case 20: HW Command - CSRNG Error Handling
    // =========================================================================
    /**
     * @brief Verify hardware command CSRNG error handling
     *
     * Test Objective: Validate that CSRNG errors during hardware commands
     * trigger appropriate error mechanisms and state transitions.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_020
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable boot mode
     * 2. Simulate CSRNG error during Instantiate
     * 3. Verify ERR_CODE.SFIFO_ESRNG_ERR=1
     * 4. Check state machine transitions to Error state
     * 5. Verify fatal alert asserted
     * 6. Confirm system requires reset to recover
     *
     * Pass Criteria:
     * - Error detected and recorded
     * - State machine enters Error state
     * - Fatal error mechanisms triggered
     *
     * @return true if test passes, false otherwise
     */
    bool test_hw_cmd_csrng_error_handling();

    // =========================================================================
    // Test Case 21: Entropy Buffer Management (128-bit to 32-bit)
    // =========================================================================
    /**
     * @brief Verify entropy buffer 128-bit to 32-bit data width conversion
     *
     * Test Objective: Validate that 128-bit CSRNG entropy blocks are
     * correctly converted and buffered as four 32-bit words for endpoints.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_021
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue Generate command
     * 2. Provide 128-bit entropy block via genbits
     * 3. Verify internal buffer stores four 32-bit words
     * 4. Request entropy from endpoint 4 times
     * 5. Check each request receives unique 32-bit word
     * 6. Verify correct byte ordering (little-endian)
     *
     * Pass Criteria:
     * - 128-bit block split into four 32-bit words
     * - Words distributed in correct order
     * - Buffer depletion triggers new Generate
     *
     * @return true if test passes, false otherwise
     */
    bool test_entropy_buffer_management();

    // =========================================================================
    // Test Case 22: Multiple SW Commands Sequential
    // =========================================================================
    /**
     * @brief Verify sequential SW command execution without interference
     *
     * Test Objective: Validate that multiple SW commands can be issued
     * sequentially with correct status tracking for each command.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_022
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue Instantiate command
     * 2. Wait for completion (CMD_ACK=1)
     * 3. Clear CMD_ACK by reading or timeout
     * 4. Issue Generate command
     * 5. Wait for completion
     * 6. Issue Reseed command
     * 7. Wait for completion
     * 8. Verify each command executed independently
     *
     * Pass Criteria:
     * - All commands complete successfully
     * - Status tracking correct for each command
     * - No command interference or data corruption
     *
     * @return true if test passes, false otherwise
     */
    bool test_multiple_sw_commands_sequential();

    // =========================================================================
    // Test Case 23: SW Command While Boot Mode Active
    // =========================================================================
    /**
     * @brief Verify SW command behavior when boot mode is active
     *
     * Test Objective: Validate that SW commands are rejected or queued
     * when boot mode hardware commands are in progress.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_023
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable boot mode
     * 2. During boot command sequence, attempt SW command
     * 3. Check SW_CMD_STS.CMD_RDY=0 (not ready)
     * 4. Wait for boot mode completion
     * 5. Verify SW command capability restored after boot exit
     *
     * Pass Criteria:
     * - SW commands blocked during boot mode
     * - CMD_RDY indicates unavailability
     * - No command interference
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_during_boot_mode();

    // =========================================================================
    // Test Case 24: Invalid clen Field (exceeds 12 words)
    // =========================================================================
    /**
     * @brief Verify error detection for invalid clen field value
     *
     * Test Objective: Validate that clen>12 is detected as protocol error
     * since SW_CMD FIFO maximum depth is 13 words (header + 12 data).
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_024
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Write command header with clen=13 (invalid)
     * 2. Attempt to write 13 data words
     * 3. Verify error detection mechanism
     * 4. Check if command rejected or error status set
     *
     * Pass Criteria:
     * - Invalid clen detected
     * - Error mechanism triggered
     * - Command not forwarded to CSRNG
     *
     * @return true if test passes, false otherwise
     */
    bool test_invalid_clen_exceeds_max();

    // =========================================================================
    // Test Case 25: SW Command Status After Reset
    // =========================================================================
    /**
     * @brief Verify SW_CMD_STS register reset values and state
     *
     * Test Objective: Validate that SW_CMD_STS returns to correct reset
     * values after system reset, clearing any pending command state.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_025
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue SW command (partial or complete)
     * 2. Apply system reset
     * 3. Read SW_CMD_STS register
     * 4. Verify CMD_REG_RDY=1, CMD_RDY=1
     * 5. Check CMD_ACK=0, CMD_STS=0
     * 6. Verify no pending command state
     *
     * Pass Criteria:
     * - SW_CMD_STS reset to default values
     * - Command buffer cleared
     * - Ready for new commands
     *
     * @return true if test passes, false otherwise
     */
    bool test_sw_cmd_status_after_reset();

    // =========================================================================
    // Test Case 26: HW Command Status After Reset
    // =========================================================================
    /**
     * @brief Verify HW_CMD_STS register reset values
     *
     * Test Objective: Validate that HW_CMD_STS returns to correct reset
     * values after system reset.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_026
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Enable hardware mode and issue commands
     * 2. Apply system reset
     * 3. Read HW_CMD_STS register
     * 4. Verify all fields reset to 0
     * 5. Check BOOT_MODE=0, AUTO_MODE=0
     *
     * Pass Criteria:
     * - HW_CMD_STS reset to 0x0
     * - All mode and status bits cleared
     *
     * @return true if test passes, false otherwise
     */
    bool test_hw_cmd_status_after_reset();

    // =========================================================================
    // Test Case 27: Recoverable Alert on CSRNG Error
    // =========================================================================
    /**
     * @brief Verify recoverable alert (RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT)
     *
     * Test Objective: Validate that CSRNG error sets recoverable alert
     * status bit allowing firmware diagnosis and recovery.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_027
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue SW command
     * 2. Simulate CSRNG error acknowledgment
     * 3. Verify RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT=1
     * 4. Check alert_recov_alert signal asserted
     * 5. Clear alert by writing 0 to status bit (W0C)
     * 6. Verify alert cleared
     *
     * Pass Criteria:
     * - Recoverable alert status bit set
     * - Alert signal asserted
     * - W0C clearing mechanism works
     *
     * @return true if test passes, false otherwise
     */
    bool test_recoverable_alert_on_csrng_error();

    // =========================================================================
    // Test Case 28: Fatal Alert on CSRNG Error
    // =========================================================================
    /**
     * @brief Verify fatal alert (ERR_CODE.SFIFO_ESRNG_ERR) on CSRNG error
     *
     * Test Objective: Validate that CSRNG error sets fatal error code
     * triggering fatal alert signal (sticky until reset).
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_028
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue SW command
     * 2. Simulate CSRNG error acknowledgment
     * 3. Verify ERR_CODE.SFIFO_ESRNG_ERR=1
     * 4. Check alert_fatal_alert signal asserted
     * 5. Attempt to clear ERR_CODE (should fail)
     * 6. Apply reset and verify ERR_CODE cleared
     *
     * Pass Criteria:
     * - Fatal error code bit set
     * - Fatal alert asserted
     * - ERR_CODE sticky (only reset clears)
     *
     * @return true if test passes, false otherwise
     */
    bool test_fatal_alert_on_csrng_error();

    // =========================================================================
    // Test Case 29: Command Interruption by Disable
    // =========================================================================
    /**
     * @brief Verify command behavior when EDN disabled during execution
     *
     * Test Objective: Validate that disabling EDN during command execution
     * safely aborts command without corruption.
     *
     * Test Plan Reference: TC_EDN_CSRNG_INF_029
     * Functionality: EDN_FUNC_004
     *
     * Procedure:
     * 1. Issue multi-word SW command
     * 2. After partial write, disable EDN (CTRL.EDN_ENABLE=0x9)
     * 3. Verify command aborted
     * 4. Check no spurious CSRNG transactions
     * 5. Re-enable EDN
     * 6. Verify system in clean state
     *
     * Pass Criteria:
     * - Command safely aborted
     * - No state corruption
     * - System recoverable after re-enable
     *
     * @return true if test passes, false otherwise
     */
    bool test_cmd_interruption_by_disable();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * @brief Build CSRNG command header word
     * @param cmd Command type (1=Instantiate, 3=Generate, 4=Reseed, 5=Uninstantiate)
     * @param acmd Application command (typically 0)
     * @param clen Additional data word count (0-12)
     * @param flags Command flags
     * @param glen Generate length (for Generate commands, bits[31:16])
     * @return 32-bit command header word
     */
    uint32_t build_cmd_header(uint8_t cmd, uint8_t acmd, uint8_t clen,
                               uint8_t flags, uint16_t glen);

    /**
     * @brief Write multi-word command to SW_CMD_REQ
     * @param header Command header word
     * @param data Pointer to data words array
     * @param num_data_words Number of data words (must match clen in header)
     * @return true if command written successfully
     */
    bool write_sw_cmd(uint32_t header, const uint32_t* data, uint32_t num_data_words);

    /**
     * @brief Poll SW_CMD_STS.CMD_REG_RDY until ready
     * @param timeout_ns Maximum wait time in nanoseconds
     * @return true if ready within timeout, false if timeout
     */
    bool poll_cmd_reg_rdy(double timeout_ns = 1000.0);

    /**
     * @brief Wait for SW command acknowledgment
     * @param timeout_ns Maximum wait time in nanoseconds
     * @return true if acknowledged within timeout
     */
    bool wait_for_sw_cmd_ack(double timeout_ns = 10000.0);

    /**
     * @brief Simulate CSRNG acknowledgment
     * @param status CSRNG status code (0=success, non-zero=error)
     */
    void simulate_csrng_ack(uint32_t status);

    /**
     * @brief Simulate CSRNG entropy provision via genbits
     * @param genbits 4x 32-bit entropy words (128-bit total)
     * @param fips FIPS compliance indicator
     */
    void simulate_csrng_genbits(const uint32_t genbits[4], bool fips);

    /**
     * @brief Enable EDN in software port mode
     * @return true if enabled successfully
     */
    bool enable_sw_port_mode();

    /**
     * @brief Enable EDN in boot-time request mode
     * @return true if enabled successfully
     */
    bool enable_boot_mode();

    /**
     * @brief Enable EDN in auto request mode
     * @return true if enabled successfully
     */
    bool enable_auto_mode();

    /**
     * @brief Verify register value matches expected value
     * @param reg_name Register name for diagnostics
     * @param expected Expected value
     * @param actual Actual read value
     * @param mask Bit mask for comparison (default=all bits)
     * @return true if values match, false otherwise
     */
    bool verify_register_value(const std::string& reg_name, uint32_t expected,
                                uint32_t actual, uint32_t mask = 0xFFFFFFFF);

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
};
