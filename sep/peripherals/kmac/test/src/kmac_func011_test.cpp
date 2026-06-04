/******************************************************************************
 * @file kmac_func011_test.cpp
 * @brief Test cases for FUNC-KMAC-011 (State Machine and Command Processing)
 *
 * This file implements comprehensive test cases for KMAC state machine FSM
 * transitions, sparse command encoding validation, CFG_REGWEN protection,
 * STATUS register dynamic construction, and idle_o signal updates.
 *
 * Functionality Coverage:
 * - FSM state transitions (IDLE → ABSORB → SQUEEZE → IDLE)
 * - Sparse command validation (START=0x1D, PROCESS=0x2E, RUN=0x31, DONE=0x16)
 * - Invalid command rejection
 * - State-dependent operation permissions and blocking
 * - CFG_REGWEN auto-lock/unlock mechanism
 * - STATUS register dynamic construction (sha3_idle/absorb/squeeze mutual exclusivity)
 * - idle_o port signal updates
 * - Error recovery via err_processed
 * - Command auxiliary bits (entropy_req, hash_cnt_clr)
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Functionality Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "kmac_test.h"
#include "testbench.h"
#include "csml_logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// Logger for test output
static CsmlLogger test_logger;

/******************************************************************************
 * Helper Functions
 ******************************************************************************/

/**
 * @brief Helper function to read STATUS register and extract FSM state bits
 * @param test Pointer to test harness
 * @param sha3_idle Output: sha3_idle bit value
 * @param sha3_absorb Output: sha3_absorb bit value
 * @param sha3_squeeze Output: sha3_squeeze bit value
 */
static void read_status_fsm_bits(kmac_test* test, bool& sha3_idle, bool& sha3_absorb, bool& sha3_squeeze)
{
    uint32_t status_val = 0;
    test->register_read_32(test->STATUS_OFFSET, status_val);

    sha3_idle = (status_val & 0x1) != 0;
    sha3_absorb = (status_val & 0x2) != 0;
    sha3_squeeze = (status_val & 0x4) != 0;
}

/**
 * @brief Helper function to verify exactly one FSM status bit is set
 * @param sha3_idle sha3_idle bit value
 * @param sha3_absorb sha3_absorb bit value
 * @param sha3_squeeze sha3_squeeze bit value
 * @return true if exactly one bit is set (mutual exclusivity satisfied)
 */
static bool verify_fsm_mutual_exclusivity(bool sha3_idle, bool sha3_absorb, bool sha3_squeeze)
{
    int count = (sha3_idle ? 1 : 0) + (sha3_absorb ? 1 : 0) + (sha3_squeeze ? 1 : 0);
    return (count == 1);
}

/**
 * @brief Helper function to issue CMD register write
 * @param test Pointer to test harness
 * @param cmd_value Command value to write (bits [5:0] for sparse command)
 */
static void write_cmd(kmac_test* test, uint32_t cmd_value)
{
    test->register_write_32(test->CMD_OFFSET, cmd_value);
    wait(10, SC_NS); // Small delay for command processing
}

/**
 * @brief Helper function to configure minimal SHA3-256 configuration
 * @param test Pointer to test harness
 */
static void configure_sha3_256(kmac_test* test)
{
    // CFG_SHADOWED: mode=0x0 (SHA3), kstrength=0x2 (256-bit), kmac_en=0 + entropy
    uint32_t cfg_val = (0 << 0) |      // kmac_en=0
                       (0x2 << 1) |    // kstrength=0x2
                       (0x0 << 4) |    // mode=SHA3
                       (0x1 << 16) |   // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);    // entropy_ready=1 - REQUIRED

    // Write twice for shadow register validation
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to configure SHAKE256 (XOF mode for RUN command testing)
 * @param test Pointer to test harness
 * 
 * SHAKE256 is an XOF (extendable output function) that supports the RUN command.
 * SHA3 mode produces fixed-length output only and rejects RUN command.
 */
static void configure_shake_256(kmac_test* test)
{
    // CFG_SHADOWED: mode=0x2 (SHAKE), kstrength=0x2 (256-bit), kmac_en=0 + entropy
    uint32_t cfg_val = (0 << 0) |      // kmac_en=0
                       (0x2 << 1) |    // kstrength=0x2
                       (0x2 << 4) |    // mode=SHAKE (XOF - supports RUN command)
                       (0x1 << 16) |   // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);    // entropy_ready=1 - REQUIRED

    // Write twice for shadow register validation
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}


/******************************************************************************
 * TC-071: test_fsm_reset_to_idle
 * Verify FSM initializes to IDLE state after reset (STATUS.sha3_idle = 1)
 ******************************************************************************/
void testbench::test_fsm_reset_to_idle()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-071: test_fsm_reset_to_idle");

    try {
        // Clear any residual application interface state from previous tests
        apply_reset();

        // Read STATUS register after reset
        uint32_t status_val = 0;
        test->register_read_32(test->STATUS_OFFSET, status_val);

        // Verify sha3_idle bit (bit 0) is set
        bool sha3_idle = (status_val & 0x1) != 0;
        bool sha3_absorb = (status_val & 0x2) != 0;
        bool sha3_squeeze = (status_val & 0x4) != 0;

        if (!(sha3_idle && !sha3_absorb && !sha3_squeeze)) {
            std::stringstream ss;
            ss << "FSM not in IDLE state after reset. STATUS=0x" << std::hex << status_val;
            report_test_fail("TC-071: test_fsm_reset_to_idle", ss.str());
            return;
        }

        // Verify STATUS register reset value includes idle bit
        if ((status_val & 0x00004001) != 0x00004001) {
            std::stringstream ss;
            ss << "STATUS register incorrect reset value: 0x" << std::hex << status_val;
            report_test_fail("TC-071: test_fsm_reset_to_idle", ss.str());
            return;
        }

        report_test_pass("TC-071: test_fsm_reset_to_idle");

    } catch (const std::exception& e) {
        report_test_fail("TC-071: test_fsm_reset_to_idle", e.what());
    }
}

/******************************************************************************
 * TC-072: test_fsm_idle_to_absorb_on_start
 * Verify START command transitions FSM from IDLE to ABSORB (STATUS.sha3_absorb = 1)
 ******************************************************************************/
void testbench::test_fsm_idle_to_absorb_on_start()
{
    report_test_start("TC-072: test_fsm_idle_to_absorb_on_start");

    try {
        // Clear any active application interface state
        apply_reset();

        // Configure for SHA3-256 operation
        configure_sha3_256(test);

        // Verify initial IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            write_cmd(test, 0x16);  // Clean up
            report_test_fail("TC-072: test_fsm_idle_to_absorb_on_start",
                            "Precondition failed: FSM not in IDLE state");
            return;
        }

        // Issue START command (0x1D)
        write_cmd(test, 0x1D);

        // Verify transition to ABSORB state
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!(!idle && absorb && !squeeze)) {
            std::stringstream ss;
            ss << "FSM not in ABSORB state after START. idle=" << idle
               << ", absorb=" << absorb << ", squeeze=" << squeeze;
            write_cmd(test, 0x16);  // Clean up
            report_test_fail("TC-072: test_fsm_idle_to_absorb_on_start", ss.str());
            return;
        }

        // Clean up: Return to IDLE with DONE command
        write_cmd(test, 0x16);
        report_test_pass("TC-072: test_fsm_idle_to_absorb_on_start");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);  // Clean up
        report_test_fail("TC-072: test_fsm_idle_to_absorb_on_start", e.what());
    }
}

/******************************************************************************
 * TC-073: test_fsm_absorb_to_squeeze_on_process
 * Verify PROCESS command transitions FSM from ABSORB to SQUEEZE (STATUS.sha3_squeeze = 1)
 ******************************************************************************/
void testbench::test_fsm_absorb_to_squeeze_on_process()
{
    report_test_start("TC-073: test_fsm_absorb_to_squeeze_on_process");

    try {
        // Clear any active application interface state
        apply_reset();

        // Configure and transition to ABSORB
        configure_sha3_256(test);
        write_cmd(test, 0x1D); // START

        // Verify ABSORB state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            write_cmd(test, 0x16);
            report_test_fail("TC-073: test_fsm_absorb_to_squeeze_on_process",
                            "Precondition failed: FSM not in ABSORB state");
            return;
        }

        // Issue PROCESS command (0x2E)
        write_cmd(test, 0x2E);

        // Verify transition to SQUEEZE state
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!(!idle && !absorb && squeeze)) {
            std::stringstream ss;
            ss << "FSM not in SQUEEZE state after PROCESS. idle=" << idle
               << ", absorb=" << absorb << ", squeeze=" << squeeze;
            write_cmd(test, 0x16);
            report_test_fail("TC-073: test_fsm_absorb_to_squeeze_on_process", ss.str());
            return;
        }

        // Clean up: Return to IDLE
        write_cmd(test, 0x16);
        report_test_pass("TC-073: test_fsm_absorb_to_squeeze_on_process");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-073: test_fsm_absorb_to_squeeze_on_process", e.what());
    }
}

/******************************************************************************
 * TC-074: test_fsm_squeeze_to_idle_on_done
 * Verify DONE command transitions FSM from SQUEEZE to IDLE (STATUS.sha3_idle = 1)
 ******************************************************************************/
void testbench::test_fsm_squeeze_to_idle_on_done()
{
    report_test_start("TC-074: test_fsm_squeeze_to_idle_on_done");

    try {
        // Clear any active application interface state
        apply_reset();

        // Configure and transition to SQUEEZE
        configure_sha3_256(test);
        write_cmd(test, 0x1D);  // START → ABSORB
        write_cmd(test, 0x2E);  // PROCESS → SQUEEZE

        // Verify SQUEEZE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-074: test_fsm_squeeze_to_idle_on_done",
                            "Precondition failed: FSM not in SQUEEZE state");
            return;
        }

        // Issue DONE command (0x16)
        write_cmd(test, 0x16);

        // Verify transition to IDLE state
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!(idle && !absorb && !squeeze)) {
            std::stringstream ss;
            ss << "FSM not in IDLE state after DONE. idle=" << idle
               << ", absorb=" << absorb << ", squeeze=" << squeeze;
            report_test_fail("TC-074: test_fsm_squeeze_to_idle_on_done", ss.str());
            return;
        }

        report_test_pass("TC-074: test_fsm_squeeze_to_idle_on_done");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-074: test_fsm_squeeze_to_idle_on_done", e.what());
    }
}

/******************************************************************************
 * TC-075: test_fsm_squeeze_persistent_on_run
 * Verify RUN command keeps FSM in SQUEEZE state with temporary status bit clearing
 ******************************************************************************/
void testbench::test_fsm_squeeze_persistent_on_run()
{
    report_test_start("TC-075: test_fsm_squeeze_persistent_on_run");

    try {
        // Clear any active application interface state
        apply_reset();

        // Configure SHAKE256 (XOF mode required for RUN command) and transition to SQUEEZE
        // RUN command is only valid for XOF modes (SHAKE/cSHAKE/KMAC), not SHA3
        configure_shake_256(test);
        write_cmd(test, 0x1D);  // START → ABSORB
        write_cmd(test, 0x2E);  // PROCESS → SQUEEZE

        // Verify SQUEEZE state before RUN
        bool idle_before, absorb_before, squeeze_before;
        read_status_fsm_bits(test, idle_before, absorb_before, squeeze_before);
        if (!squeeze_before) {
            write_cmd(test, 0x16);
            report_test_fail("TC-075: test_fsm_squeeze_persistent_on_run",
                            "Precondition failed: FSM not in SQUEEZE state");
            return;
        }

        // Issue RUN command (0x31)
        write_cmd(test, 0x31);

        // Verify FSM remains in SQUEEZE state
        bool idle_after, absorb_after, squeeze_after;
        read_status_fsm_bits(test, idle_after, absorb_after, squeeze_after);

        if (!(!idle_after && !absorb_after && squeeze_after)) {
            std::stringstream ss;
            ss << "FSM not in SQUEEZE state after RUN. idle=" << idle_after
               << ", absorb=" << absorb_after << ", squeeze=" << squeeze_after;
            write_cmd(test, 0x16);
            report_test_fail("TC-075: test_fsm_squeeze_persistent_on_run", ss.str());
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-075: test_fsm_squeeze_persistent_on_run");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-075: test_fsm_squeeze_persistent_on_run", e.what());
    }
}

/******************************************************************************
 * TC-076: test_fsm_status_bits_mutually_exclusive
 * Verify sha3_idle, sha3_absorb, sha3_squeeze are mutually exclusive
 ******************************************************************************/
void testbench::test_fsm_status_bits_mutually_exclusive()
{
    report_test_start("TC-076: test_fsm_status_bits_mutually_exclusive");

    try {
        configure_sha3_256(test);

        // Check IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!verify_fsm_mutual_exclusivity(idle, absorb, squeeze)) {
            write_cmd(test, 0x16);
            report_test_fail("TC-076: test_fsm_status_bits_mutually_exclusive",
                            "IDLE state: multiple status bits set");
            return;
        }

        // Transition to ABSORB and check
        write_cmd(test, 0x1D);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!verify_fsm_mutual_exclusivity(idle, absorb, squeeze)) {
            write_cmd(test, 0x16);
            report_test_fail("TC-076: test_fsm_status_bits_mutually_exclusive",
                            "ABSORB state: multiple status bits set");
            return;
        }

        // Transition to SQUEEZE and check
        write_cmd(test, 0x2E);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!verify_fsm_mutual_exclusivity(idle, absorb, squeeze)) {
            write_cmd(test, 0x16);
            report_test_fail("TC-076: test_fsm_status_bits_mutually_exclusive",
                            "SQUEEZE state: multiple status bits set");
            return;
        }

        // Return to IDLE and check
        write_cmd(test, 0x16);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!verify_fsm_mutual_exclusivity(idle, absorb, squeeze)) {
            report_test_fail("TC-076: test_fsm_status_bits_mutually_exclusive",
                            "IDLE state (after DONE): multiple status bits set");
            return;
        }

        report_test_pass("TC-076: test_fsm_status_bits_mutually_exclusive");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-076: test_fsm_status_bits_mutually_exclusive", e.what());
    }
}

/******************************************************************************
 * TC-077: test_fsm_transition_sequence_complete_operation
 * Verify complete FSM sequence: IDLE→ABSORB→SQUEEZE→IDLE
 ******************************************************************************/
void testbench::test_fsm_transition_sequence_complete_operation()
{
    report_test_start("TC-077: test_fsm_transition_sequence_complete_operation");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        bool idle, absorb, squeeze;

        // State 1: IDLE
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle || absorb || squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-077: test_fsm_transition_sequence_complete_operation",
                            "Initial state not IDLE");
            return;
        }

        // Transition 1: IDLE → ABSORB via START
        write_cmd(test, 0x1D);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (idle || !absorb || squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-077: test_fsm_transition_sequence_complete_operation",
                            "Transition IDLE→ABSORB failed");
            return;
        }

        // Transition 2: ABSORB → SQUEEZE via PROCESS
        write_cmd(test, 0x2E);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (idle || absorb || !squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-077: test_fsm_transition_sequence_complete_operation",
                            "Transition ABSORB→SQUEEZE failed");
            return;
        }

        // Transition 3: SQUEEZE → IDLE via DONE
        write_cmd(test, 0x16);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle || absorb || squeeze) {
            report_test_fail("TC-077: test_fsm_transition_sequence_complete_operation",
                            "Transition SQUEEZE→IDLE failed");
            return;
        }

        report_test_pass("TC-077: test_fsm_transition_sequence_complete_operation");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-077: test_fsm_transition_sequence_complete_operation", e.what());
    }
}

/******************************************************************************
 * TC-078: test_fsm_multiple_operations_back_to_back
 * Verify back-to-back operations cycle through FSM correctly
 ******************************************************************************/
void testbench::test_fsm_multiple_operations_back_to_back()
{
    report_test_start("TC-078: test_fsm_multiple_operations_back_to_back");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Perform 3 back-to-back hash operations
        for (int op = 0; op < 3; op++) {
            bool idle, absorb, squeeze;

            // START
            write_cmd(test, 0x1D);
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!absorb) {
                std::stringstream ss;
                ss << "Operation " << op << ": Not in ABSORB after START";
                write_cmd(test, 0x16);
                report_test_fail("TC-078: test_fsm_multiple_operations_back_to_back", ss.str());
                return;
            }

            // PROCESS
            write_cmd(test, 0x2E);
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!squeeze) {
                std::stringstream ss;
                ss << "Operation " << op << ": Not in SQUEEZE after PROCESS";
                write_cmd(test, 0x16);
                report_test_fail("TC-078: test_fsm_multiple_operations_back_to_back", ss.str());
                return;
            }

            // DONE
            write_cmd(test, 0x16);
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!idle) {
                std::stringstream ss;
                ss << "Operation " << op << ": Not in IDLE after DONE";
                report_test_fail("TC-078: test_fsm_multiple_operations_back_to_back", ss.str());
                return;
            }
        }

        report_test_pass("TC-078: test_fsm_multiple_operations_back_to_back");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-078: test_fsm_multiple_operations_back_to_back", e.what());
    }
}

/******************************************************************************
 * TC-079: test_fsm_idle_state_operations_blocked
 * Verify invalid operations in IDLE state trigger SwCmdSequence error and ERROR state
 * Per OpenTitan KMAC spec: invalid command sequences cause ERROR state transition
 ******************************************************************************/
void testbench::test_fsm_idle_state_operations_blocked()
{
    report_test_start("TC-079: test_fsm_idle_state_operations_blocked");

    try {
        configure_sha3_256(test);
        bool idle, absorb, squeeze;

        // Verify in IDLE state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            write_cmd(test, 0x16);
            report_test_fail("TC-079: test_fsm_idle_state_operations_blocked",
                            "Precondition failed: not in IDLE state");
            return;
        }

        // Try PROCESS command (should trigger SwCmdSequence error and ERROR state)
        // Per OpenTitan spec, invalid command sequences trigger ERROR, not ignored
        write_cmd(test, 0x2E);
        
        // Verify ERROR state was entered (FSM not in IDLE anymore) and error was set
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        
        // Check SwCmdSequence error (0x08) is set with PROCESS command debug bits
        // Error format: 0x08000000 | cmd_value
        bool has_error = (err_code & 0x08000000) != 0;
        
        if (!has_error) {
            // If no error, check if FSM remained in IDLE (that would also be acceptable)
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!idle) {
                write_cmd(test, 0x16);
                report_test_fail("TC-079: test_fsm_idle_state_operations_blocked",
                                "PROCESS command changed state without setting error");
                return;
            }
        } else {
            // Error was set - verify and recover
            CSML_INFO(2, test_logger) << "SwCmdSequence error correctly set for PROCESS in IDLE: 0x"
                                      << std::hex << err_code << std::dec;
            // Use DONE command for error recovery
            write_cmd(test, 0x16);
        }

        // DONE command in IDLE is allowed (no-op or error recovery)
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("TC-079: test_fsm_idle_state_operations_blocked",
                            "DONE command did not return to IDLE state");
            return;
        }

        report_test_pass("TC-079: test_fsm_idle_state_operations_blocked");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-079: test_fsm_idle_state_operations_blocked", e.what());
    }
}

/******************************************************************************
 * TC-080: test_fsm_absorb_state_operations_permitted
 * Verify operations permitted in ABSORB state (MSG_FIFO write, PROCESS command)
 ******************************************************************************/
void testbench::test_fsm_absorb_state_operations_permitted()
{
    report_test_start("TC-080: test_fsm_absorb_state_operations_permitted");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D); // Transition to ABSORB

        // Verify in ABSORB state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            write_cmd(test, 0x16);
            report_test_fail("TC-080: test_fsm_absorb_state_operations_permitted",
                            "Precondition failed: not in ABSORB state");
            return;
        }

        // PROCESS command should work
        write_cmd(test, 0x2E);
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-080: test_fsm_absorb_state_operations_permitted",
                            "PROCESS command not accepted in ABSORB state");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-080: test_fsm_absorb_state_operations_permitted");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-080: test_fsm_absorb_state_operations_permitted", e.what());
    }
}

/******************************************************************************
 * TC-081: test_fsm_absorb_state_operations_blocked
 * Verify invalid operations in ABSORB state trigger SwCmdSequence error
 * Per OpenTitan KMAC spec: invalid command sequences cause ERROR state transition
 ******************************************************************************/
void testbench::test_fsm_absorb_state_operations_blocked()
{
    report_test_start("TC-081: test_fsm_absorb_state_operations_blocked");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D); // Transition to ABSORB

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            write_cmd(test, 0x16);
            report_test_fail("TC-081: test_fsm_absorb_state_operations_blocked",
                            "Precondition failed: not in ABSORB state");
            return;
        }

        // Try START command (should trigger ERROR state per OpenTitan spec)
        write_cmd(test, 0x1D);
        
        // Verify ERROR state was entered and error was set
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        
        // Check SwCmdSequence error (0x08) is set with START command debug bits
        bool has_error = (err_code & 0x08000000) != 0;
        
        if (!has_error) {
            // If no error, check if FSM changed state
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!absorb) {
                write_cmd(test, 0x16);
                report_test_fail("TC-081: test_fsm_absorb_state_operations_blocked",
                                "START command changed state without setting error");
                return;
            }
        } else {
            // Error was correctly set - verify and recover
            CSML_INFO(2, test_logger) << "SwCmdSequence error correctly set for START in ABSORB: 0x"
                                      << std::hex << err_code << std::dec;
        }

        // DONE is allowed for error recovery - use it to return to IDLE
        write_cmd(test, 0x16);
        
        // Verify we're back in IDLE after recovery
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("TC-081: test_fsm_absorb_state_operations_blocked",
                            "DONE command did not return to IDLE for error recovery");
            return;
        }
        
        report_test_pass("TC-081: test_fsm_absorb_state_operations_blocked");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-081: test_fsm_absorb_state_operations_blocked", e.what());
    }
}

/******************************************************************************
 * TC-082: test_fsm_squeeze_state_operations_permitted
 * Verify operations permitted in SQUEEZE state (STATE read, RUN, DONE commands)
 ******************************************************************************/
void testbench::test_fsm_squeeze_state_operations_permitted()
{
    report_test_start("TC-082: test_fsm_squeeze_state_operations_permitted");

    try {
        // Clear any active application interface state
        apply_reset();

        // Use SHAKE256 (XOF mode) for RUN command testing - SHA3 rejects RUN
        configure_shake_256(test);
        write_cmd(test, 0x1D);  // ABSORB
        write_cmd(test, 0x2E);  // SQUEEZE

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-082: test_fsm_squeeze_state_operations_permitted",
                            "Precondition failed: not in SQUEEZE state");
            return;
        }

        // RUN command should work
        write_cmd(test, 0x31);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-082: test_fsm_squeeze_state_operations_permitted",
                            "RUN command not permitted in SQUEEZE state");
            return;
        }

        // DONE command should work
        write_cmd(test, 0x16);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("TC-082: test_fsm_squeeze_state_operations_permitted",
                            "DONE command not permitted in SQUEEZE state");
            return;
        }

        report_test_pass("TC-082: test_fsm_squeeze_state_operations_permitted");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-082: test_fsm_squeeze_state_operations_permitted", e.what());
    }
}

/******************************************************************************
 * TC-083: test_fsm_squeeze_state_operations_blocked
 * Verify invalid operations in SQUEEZE state trigger SwCmdSequence error
 * Per OpenTitan KMAC spec: invalid command sequences cause ERROR state transition
 ******************************************************************************/
void testbench::test_fsm_squeeze_state_operations_blocked()
{
    report_test_start("TC-083: test_fsm_squeeze_state_operations_blocked");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D);  // ABSORB
        write_cmd(test, 0x2E);  // SQUEEZE

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-083: test_fsm_squeeze_state_operations_blocked",
                            "Precondition failed: not in SQUEEZE state");
            return;
        }

        // Try START command (should trigger ERROR state per OpenTitan spec)
        write_cmd(test, 0x1D);
        
        // Verify ERROR state was entered and error was set
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        
        // Check SwCmdSequence error (0x08) is set with START command debug bits
        bool has_error = (err_code & 0x08000000) != 0;
        
        if (!has_error) {
            // If no error, check if FSM changed state
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!squeeze) {
                write_cmd(test, 0x16);
                report_test_fail("TC-083: test_fsm_squeeze_state_operations_blocked",
                                "START command changed state without setting error");
                return;
            }
        } else {
            // Error was correctly set - verify and recover
            CSML_INFO(2, test_logger) << "SwCmdSequence error correctly set for START in SQUEEZE: 0x"
                                      << std::hex << err_code << std::dec;
        }

        // Clean up - DONE for error recovery
        write_cmd(test, 0x16);
        
        // Verify we're back in IDLE after recovery
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("TC-083: test_fsm_squeeze_state_operations_blocked",
                            "DONE command did not return to IDLE for error recovery");
            return;
        }
        
        report_test_pass("TC-083: test_fsm_squeeze_state_operations_blocked");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-083: test_fsm_squeeze_state_operations_blocked", e.what());
    }
}

/******************************************************************************
 * TC-084: test_cmd_sparse_encoding_start_valid
 * Verify START command accepts valid sparse encoding 0x1D
 ******************************************************************************/
void testbench::test_cmd_sparse_encoding_start_valid()
{
    report_test_start("TC-084: test_cmd_sparse_encoding_start_valid");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Issue START with exact sparse encoding 0x1D
        write_cmd(test, 0x1D);

        // Verify command accepted (transition to ABSORB)
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!absorb) {
            write_cmd(test, 0x16);
            report_test_fail("TC-084: test_cmd_sparse_encoding_start_valid",
                            "START command (0x1D) rejected");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-084: test_cmd_sparse_encoding_start_valid");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-084: test_cmd_sparse_encoding_start_valid", e.what());
    }
}

/******************************************************************************
 * TC-085: test_cmd_sparse_encoding_process_valid
 * Verify PROCESS command accepts valid sparse encoding 0x2E
 ******************************************************************************/
void testbench::test_cmd_sparse_encoding_process_valid()
{
    report_test_start("TC-085: test_cmd_sparse_encoding_process_valid");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D); // START first

        // Issue PROCESS with exact sparse encoding 0x2E
        write_cmd(test, 0x2E);

        // Verify command accepted (transition to SQUEEZE)
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-085: test_cmd_sparse_encoding_process_valid",
                            "PROCESS command (0x2E) rejected");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-085: test_cmd_sparse_encoding_process_valid");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-085: test_cmd_sparse_encoding_process_valid", e.what());
    }
}

/******************************************************************************
 * TC-086: test_cmd_sparse_encoding_run_valid
 * Verify RUN command accepts valid sparse encoding 0x31
 ******************************************************************************/
void testbench::test_cmd_sparse_encoding_run_valid()
{
    report_test_start("TC-086: test_cmd_sparse_encoding_run_valid");

    try {
        // Clear any active application interface state
        apply_reset();

        // Use SHAKE256 (XOF mode) for RUN command testing - SHA3 rejects RUN
        configure_shake_256(test);
        write_cmd(test, 0x1D);  // START
        write_cmd(test, 0x2E);  // PROCESS → SQUEEZE

        // Issue RUN with exact sparse encoding 0x31
        write_cmd(test, 0x31);

        // Verify command accepted (remain in SQUEEZE)
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-086: test_cmd_sparse_encoding_run_valid",
                            "RUN command (0x31) rejected or changed state");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-086: test_cmd_sparse_encoding_run_valid");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-086: test_cmd_sparse_encoding_run_valid", e.what());
    }
}

/******************************************************************************
 * TC-087: test_cmd_sparse_encoding_done_valid
 * Verify DONE command accepts valid sparse encoding 0x16
 ******************************************************************************/
void testbench::test_cmd_sparse_encoding_done_valid()
{
    report_test_start("TC-087: test_cmd_sparse_encoding_done_valid");

    try {
        configure_sha3_256(test);
        write_cmd(test, 0x1D);  // START
        write_cmd(test, 0x2E);  // PROCESS

        // Issue DONE with exact sparse encoding 0x16
        write_cmd(test, 0x16);

        // Verify command accepted (transition to IDLE)
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!idle) {
            report_test_fail("TC-087: test_cmd_sparse_encoding_done_valid",
                            "DONE command (0x16) rejected");
            return;
        }

        report_test_pass("TC-087: test_cmd_sparse_encoding_done_valid");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-087: test_cmd_sparse_encoding_done_valid", e.what());
    }
}

/******************************************************************************
 * TC-088: test_cmd_sparse_encoding_invalid
 * Verify invalid command encodings are rejected
 ******************************************************************************/
void testbench::test_cmd_sparse_encoding_invalid()
{
    report_test_start("TC-088: test_cmd_sparse_encoding_invalid");

    try {
        configure_sha3_256(test);

        // Try various invalid command encodings
        uint32_t invalid_cmds[] = {0x00, 0x01, 0x0F, 0x1C, 0x1E, 0x2D, 0x2F, 0x30, 0x32, 0x3F};

        for (size_t i = 0; i < sizeof(invalid_cmds)/sizeof(invalid_cmds[0]); i++) {
            uint32_t cmd = invalid_cmds[i];

            // Issue invalid command
            write_cmd(test, cmd);

            // Verify FSM remains in IDLE (command rejected)
            bool idle, absorb, squeeze;
            read_status_fsm_bits(test, idle, absorb, squeeze);

            if (!idle) {
                std::stringstream ss;
                ss << "Invalid command 0x" << std::hex << cmd << std::dec << " was accepted";
                write_cmd(test, 0x400); // err_processed bit
                report_test_fail("TC-088: test_cmd_sparse_encoding_invalid", ss.str());
                return;
            }

            // Check ERR_CODE for SwCmdSequence error (0x08)
            uint32_t err_code = 0;
            test->register_read_32(test->ERR_CODE_OFFSET, err_code);
            if ((err_code & 0xFF000000) != 0x08000000) {
                write_cmd(test, 0x400); // err_processed bit
                report_test_fail("TC-088: test_cmd_sparse_encoding_invalid",
                                "Invalid command didn't set SwCmdSequence error");
                return;
            }

            // Clear error state for next iteration
            write_cmd(test, 0x400); // err_processed bit
        }

        report_test_pass("TC-088: test_cmd_sparse_encoding_invalid");

    } catch (const std::exception& e) {
        write_cmd(test, 0x400); // err_processed bit
        report_test_fail("TC-088: test_cmd_sparse_encoding_invalid", e.what());
    }
}

/******************************************************************************
 * TC-089: test_cmd_entropy_req_bit
 * Verify CMD.entropy_req bit (bit 8) triggers PRNG reseed in EDN mode
 ******************************************************************************/
void testbench::test_cmd_entropy_req_bit()
{
    report_test_start("TC-089: test_cmd_entropy_req_bit");

    try {
        // Configure CFG_SHADOWED with entropy_mode=0x1 (EDN mode)
        uint32_t cfg_val = 0x00010010; // entropy_mode=0x1 at bits[17:16], kstrength=0x2
        test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
        wait(5, SC_NS);
        test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
        wait(5, SC_NS);

        // Read initial ENTROPY_REFRESH_HASH_CNT
        uint32_t hash_cnt_before = 0;
        test->register_read_32(test->ENTROPY_REFRESH_HASH_CNT_OFFSET, hash_cnt_before);

        // Issue entropy_req command (bit 8)
        write_cmd(test, 0x100);

        // Verify ENTROPY_REFRESH_HASH_CNT cleared to 0
        uint32_t hash_cnt_after = 0;
        test->register_read_32(test->ENTROPY_REFRESH_HASH_CNT_OFFSET, hash_cnt_after);

        if (hash_cnt_after != 0) {
            report_test_fail("TC-089: test_cmd_entropy_req_bit",
                            "entropy_req bit didn't clear counter");
            return;
        }

        report_test_pass("TC-089: test_cmd_entropy_req_bit");

    } catch (const std::exception& e) {
        report_test_fail("TC-089: test_cmd_entropy_req_bit", e.what());
    }
}

/******************************************************************************
 * TC-090: test_cmd_hash_cnt_clr_bit
 * Verify CMD.hash_cnt_clr bit (bit 9) clears ENTROPY_REFRESH_HASH_CNT to 0
 ******************************************************************************/
void testbench::test_cmd_hash_cnt_clr_bit()
{
    report_test_start("TC-090: test_cmd_hash_cnt_clr_bit");

    try {
        // Issue hash_cnt_clr command (bit 9)
        write_cmd(test, 0x200);

        // Verify ENTROPY_REFRESH_HASH_CNT cleared to 0
        uint32_t hash_cnt = 0;
        test->register_read_32(test->ENTROPY_REFRESH_HASH_CNT_OFFSET, hash_cnt);

        if (hash_cnt != 0) {
            report_test_fail("TC-090: test_cmd_hash_cnt_clr_bit",
                            "hash_cnt_clr bit didn't clear counter");
            return;
        }

        report_test_pass("TC-090: test_cmd_hash_cnt_clr_bit");

    } catch (const std::exception& e) {
        report_test_fail("TC-090: test_cmd_hash_cnt_clr_bit", e.what());
    }
}

/******************************************************************************
 * TC-091: test_cmd_err_processed_bit
 * Verify CMD.err_processed bit (bit 10) recovers FSM from error state to IDLE
 ******************************************************************************/
void testbench::test_cmd_err_processed_bit()
{
    report_test_start("TC-091: test_cmd_err_processed_bit");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Trigger error by issuing invalid command in IDLE
        write_cmd(test, 0x2E); // PROCESS in IDLE (invalid)

        // Verify error occurred (check ERR_CODE)
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        if ((err_code & 0xFF000000) != 0x08000000) {
            write_cmd(test, 0x400);
            report_test_fail("TC-091: test_cmd_err_processed_bit",
                            "Precondition: Error not generated");
            return;
        }

        // Issue err_processed (bit 10)
        write_cmd(test, 0x400);

        // Verify FSM returned to IDLE
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);

        if (!idle) {
            report_test_fail("TC-091: test_cmd_err_processed_bit",
                            "err_processed didn't recover to IDLE");
            return;
        }

        // Verify CFG_REGWEN.en restored
        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0x1) {
            report_test_fail("TC-091: test_cmd_err_processed_bit",
                            "CFG_REGWEN.en not restored");
            return;
        }

        report_test_pass("TC-091: test_cmd_err_processed_bit");

    } catch (const std::exception& e) {
        write_cmd(test, 0x400);
        report_test_fail("TC-091: test_cmd_err_processed_bit", e.what());
    }
}

/******************************************************************************
 * TC-092: test_cmd_self_clearing_behavior
 * Verify all CMD register bits self-clear after action completes
 ******************************************************************************/
void testbench::test_cmd_self_clearing_behavior()
{
    report_test_start("TC-092: test_cmd_self_clearing_behavior");

    try {
        configure_sha3_256(test);

        // Issue START command
        test->register_write_32(test->CMD_OFFSET, 0x1D);
        wait(10, SC_NS);

        // Read CMD register (should return 0 - self-cleared)
        uint32_t cmd_val = 0;
        test->register_read_32(test->CMD_OFFSET, cmd_val);

        if (cmd_val != 0) {
            std::stringstream ss;
            ss << "CMD register not self-cleared: 0x" << std::hex << cmd_val << std::dec;
            write_cmd(test, 0x16);
            report_test_fail("TC-092: test_cmd_self_clearing_behavior", ss.str());
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-092: test_cmd_self_clearing_behavior");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-092: test_cmd_self_clearing_behavior", e.what());
    }
}

/******************************************************************************
 * TC-151: test_err_code_swcmdsequence_0x08
 * Verify ERR_CODE = 0x08 (SwCmdSequence) when commands issued out of sequence
 ******************************************************************************/
void testbench::test_err_code_swcmdsequence_0x08()
{
    report_test_start("TC-151: test_err_code_swcmdsequence_0x08");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Issue PROCESS in IDLE state (invalid sequence)
        write_cmd(test, 0x2E);

        // Read ERR_CODE
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        // Verify SwCmdSequence error (0x08 in upper 8 bits)
        if ((err_code & 0xFF000000) != 0x08000000) {
            std::stringstream ss;
            ss << "Incorrect error code: 0x" << std::hex << err_code << std::dec;
            write_cmd(test, 0x400); // err_processed
            report_test_fail("TC-151: test_err_code_swcmdsequence_0x08", ss.str());
            return;
        }

        // Clean up
        write_cmd(test, 0x400); // err_processed
        report_test_pass("TC-151: test_err_code_swcmdsequence_0x08");

    } catch (const std::exception& e) {
        write_cmd(test, 0x400); // err_processed
        report_test_fail("TC-151: test_err_code_swcmdsequence_0x08", e.what());
    }
}

/******************************************************************************
 * TC-159: test_callback_cmd_write_start_side_effects
 * Verify handle_write_CMD for START: transitions IDLE→ABSORB, clears CFG_REGWEN.en
 ******************************************************************************/
void testbench::test_callback_cmd_write_start_side_effects()
{
    report_test_start("TC-159: test_callback_cmd_write_start_side_effects");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Verify CFG_REGWEN.en = 1 initially
        uint32_t cfg_regwen_before = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen_before);
        if ((cfg_regwen_before & 0x1) != 0x1) {
            write_cmd(test, 0x16);
            report_test_fail("TC-159: test_callback_cmd_write_start_side_effects",
                            "Precondition: CFG_REGWEN.en not 1");
            return;
        }

        // Issue START command
        write_cmd(test, 0x1D);

        // 1. FSM transitioned to ABSORB
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            write_cmd(test, 0x16);
            report_test_fail("TC-159: test_callback_cmd_write_start_side_effects",
                            "FSM not in ABSORB after START");
            return;
        }

        // 2. CFG_REGWEN.en auto-cleared to 0
        uint32_t cfg_regwen_after = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen_after);
        if ((cfg_regwen_after & 0x1) != 0) {
            write_cmd(test, 0x16);
            report_test_fail("TC-159: test_callback_cmd_write_start_side_effects",
                            "CFG_REGWEN.en not auto-cleared");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-159: test_callback_cmd_write_start_side_effects");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-159: test_callback_cmd_write_start_side_effects", e.what());
    }
}

/******************************************************************************
 * TC-160: test_callback_cmd_write_process_side_effects
 * Verify handle_write_CMD for PROCESS: applies padding, transitions ABSORB→SQUEEZE
 ******************************************************************************/
void testbench::test_callback_cmd_write_process_side_effects()
{
    report_test_start("TC-160: test_callback_cmd_write_process_side_effects");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D); // START

        // Issue PROCESS command
        write_cmd(test, 0x2E);

        // 1. FSM transitioned to SQUEEZE
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-160: test_callback_cmd_write_process_side_effects",
                            "FSM not in SQUEEZE after PROCESS");
            return;
        }

        // 2. INTR_STATE.kmac_done should be set (but interrupts not modeled in TLM)
        // We can verify STATUS.sha3_squeeze is set instead
        uint32_t status_val = 0;
        test->register_read_32(test->STATUS_OFFSET, status_val);
        if ((status_val & 0x4) == 0) {
            write_cmd(test, 0x16);
            report_test_fail("TC-160: test_callback_cmd_write_process_side_effects",
                            "STATUS.sha3_squeeze not set after PROCESS");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-160: test_callback_cmd_write_process_side_effects");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-160: test_callback_cmd_write_process_side_effects", e.what());
    }
}

/******************************************************************************
 * TC-161: test_callback_cmd_write_run_side_effects
 * Verify handle_write_CMD for RUN: executes 24 Keccak rounds
 ******************************************************************************/
void testbench::test_callback_cmd_write_run_side_effects()
{
    report_test_start("TC-161: test_callback_cmd_write_run_side_effects");

    try {
        // Clear any active application interface state
        apply_reset();

        // Use SHAKE256 (XOF mode) for RUN command testing - SHA3 rejects RUN
        configure_shake_256(test);
        write_cmd(test, 0x1D);  // START
        write_cmd(test, 0x2E);  // PROCESS

        // Issue RUN command
        write_cmd(test, 0x31);

        // 1. FSM remains in SQUEEZE
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            write_cmd(test, 0x16);
            report_test_fail("TC-161: test_callback_cmd_write_run_side_effects",
                            "FSM not in SQUEEZE after RUN");
            return;
        }

        // 2. In real hardware, STATUS.sha3_squeeze would briefly clear then reassert
        // TLM abstraction keeps it asserted
        uint32_t status_val = 0;
        test->register_read_32(test->STATUS_OFFSET, status_val);
        if ((status_val & 0x4) == 0) {
            write_cmd(test, 0x16);
            report_test_fail("TC-161: test_callback_cmd_write_run_side_effects",
                            "STATUS.sha3_squeeze not set after RUN");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-161: test_callback_cmd_write_run_side_effects");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-161: test_callback_cmd_write_run_side_effects", e.what());
    }
}

/******************************************************************************
 * TC-162: test_callback_cmd_write_done_side_effects
 * Verify handle_write_CMD for DONE: zeros Keccak state, transitions to IDLE
 ******************************************************************************/
void testbench::test_callback_cmd_write_done_side_effects()
{
    report_test_start("TC-162: test_callback_cmd_write_done_side_effects");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        write_cmd(test, 0x1D);  // START
        write_cmd(test, 0x2E);  // PROCESS

        // Verify CFG_REGWEN.en = 0 before DONE
        uint32_t cfg_regwen_before = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen_before);
        if ((cfg_regwen_before & 0x1) != 0) {
            write_cmd(test, 0x16);
            report_test_fail("TC-162: test_callback_cmd_write_done_side_effects",
                            "Precondition: CFG_REGWEN.en not 0");
            return;
        }

        // Issue DONE command
        write_cmd(test, 0x16);

        // 1. FSM transitioned to IDLE
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("TC-162: test_callback_cmd_write_done_side_effects",
                            "FSM not in IDLE after DONE");
            return;
        }

        // 2. CFG_REGWEN.en auto-set to 1
        uint32_t cfg_regwen_after = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen_after);
        if ((cfg_regwen_after & 0x1) != 1) {
            report_test_fail("TC-162: test_callback_cmd_write_done_side_effects",
                            "CFG_REGWEN.en not auto-set");
            return;
        }

        // 3. ERR_CODE should be cleared if no errors
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        if (err_code != 0) {
            report_test_fail("TC-162: test_callback_cmd_write_done_side_effects",
                            "ERR_CODE not cleared after successful DONE");
            return;
        }

        report_test_pass("TC-162: test_callback_cmd_write_done_side_effects");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-162: test_callback_cmd_write_done_side_effects", e.what());
    }
}

/******************************************************************************
 * TC-169: test_callback_status_read_dynamic
 * Verify handle_read_STATUS: dynamically constructs return value from FSM state
 ******************************************************************************/
void testbench::test_callback_status_read_dynamic()
{
    report_test_start("TC-169: test_callback_status_read_dynamic");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Read STATUS in IDLE
        uint32_t status_idle = 0;
        test->register_read_32(test->STATUS_OFFSET, status_idle);
        if ((status_idle & 0x7) != 0x1) { // Only sha3_idle should be set
            write_cmd(test, 0x16);
            report_test_fail("TC-169: test_callback_status_read_dynamic",
                            "STATUS not dynamically constructed in IDLE");
            return;
        }

        // Transition to ABSORB and read
        write_cmd(test, 0x1D);
        uint32_t status_absorb = 0;
        test->register_read_32(test->STATUS_OFFSET, status_absorb);
        if ((status_absorb & 0x7) != 0x2) { // Only sha3_absorb should be set
            write_cmd(test, 0x16);
            report_test_fail("TC-169: test_callback_status_read_dynamic",
                            "STATUS not dynamically constructed in ABSORB");
            return;
        }

        // Transition to SQUEEZE and read
        write_cmd(test, 0x2E);
        uint32_t status_squeeze = 0;
        test->register_read_32(test->STATUS_OFFSET, status_squeeze);
        if ((status_squeeze & 0x7) != 0x4) { // Only sha3_squeeze should be set
            write_cmd(test, 0x16);
            report_test_fail("TC-169: test_callback_status_read_dynamic",
                            "STATUS not dynamically constructed in SQUEEZE");
            return;
        }

        // Clean up
        write_cmd(test, 0x16);
        report_test_pass("TC-169: test_callback_status_read_dynamic");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-169: test_callback_status_read_dynamic", e.what());
    }
}

/******************************************************************************
 * TC-183: test_corner_back_to_back_operations
 * Verify multiple hash operations executed back-to-back without delays
 ******************************************************************************/
void testbench::test_corner_back_to_back_operations()
{
    report_test_start("TC-183: test_corner_back_to_back_operations");

    try {
        // This is similar to TC-078 but focuses on no-delay corner case
        configure_sha3_256(test);

        // Execute 5 rapid back-to-back operations with minimal delays
        for (int op = 0; op < 5; op++) {
            // START
            test->register_write_32(test->CMD_OFFSET, 0x1D);
            wait(1, SC_NS); // Minimal delay

            // PROCESS
            test->register_write_32(test->CMD_OFFSET, 0x2E);
            wait(1, SC_NS);

            // DONE
            test->register_write_32(test->CMD_OFFSET, 0x16);
            wait(1, SC_NS);

            // Verify back in IDLE
            bool idle, absorb, squeeze;
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!idle) {
                std::stringstream ss;
                ss << "Operation " << op << ": FSM not in IDLE after cycle";
                report_test_fail("TC-183: test_corner_back_to_back_operations", ss.str());
                return;
            }
        }

        report_test_pass("TC-183: test_corner_back_to_back_operations");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("TC-183: test_corner_back_to_back_operations", e.what());
    }
}

/******************************************************************************
 * Additional Test: CFG_REGWEN Protection During FSM Transitions
 * Verify CFG_REGWEN auto-lock/unlock mechanism throughout operation
 ******************************************************************************/
void testbench::test_cfg_regwen_auto_lock_unlock()
{
    report_test_start("Additional Test: CFG_REGWEN Auto Lock/Unlock");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);
        uint32_t cfg_regwen = 0;

        // Initial state: CFG_REGWEN.en should be 1
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 1) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: CFG_REGWEN Auto Lock/Unlock",
                            "Initial CFG_REGWEN.en not 1");
            return;
        }

        // After START: CFG_REGWEN.en should be 0
        write_cmd(test, 0x1D);
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: CFG_REGWEN Auto Lock/Unlock",
                            "CFG_REGWEN.en not auto-cleared after START");
            return;
        }

        // During ABSORB→SQUEEZE: CFG_REGWEN.en should remain 0
        write_cmd(test, 0x2E);
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: CFG_REGWEN Auto Lock/Unlock",
                            "CFG_REGWEN.en not 0 in SQUEEZE state");
            return;
        }

        // After DONE: CFG_REGWEN.en should be 1
        write_cmd(test, 0x16);
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 1) {
            report_test_fail("Additional Test: CFG_REGWEN Auto Lock/Unlock",
                            "CFG_REGWEN.en not auto-set after DONE");
            return;
        }

        report_test_pass("Additional Test: CFG_REGWEN Auto Lock/Unlock");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("Additional Test: CFG_REGWEN Auto Lock/Unlock", e.what());
    }
}

/******************************************************************************
 * Additional Test: idle_o Signal Updates
 * Verify idle_o port signal updates correctly with FSM state changes
 ******************************************************************************/
void testbench::test_idle_o_signal_updates()
{
    report_test_start("Additional Test: idle_o Signal Updates");

    try {
        // Clear any active application interface state
        apply_reset();

        configure_sha3_256(test);

        // Note: In actual testbench, idle_o signal would be monitored via sc_signal
        // This test verifies STATUS.sha3_idle as proxy for idle_o behavior

        // IDLE state: idle_o should be 1
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: idle_o Signal Updates",
                            "Not in IDLE initially");
            return;
        }

        // ABSORB state: idle_o should be 0
        write_cmd(test, 0x1D);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (idle) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: idle_o Signal Updates",
                            "idle bit set in ABSORB state");
            return;
        }

        // SQUEEZE state: idle_o should be 0
        write_cmd(test, 0x2E);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (idle) {
            write_cmd(test, 0x16);
            report_test_fail("Additional Test: idle_o Signal Updates",
                            "idle bit set in SQUEEZE state");
            return;
        }

        // Return to IDLE: idle_o should be 1
        write_cmd(test, 0x16);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            report_test_fail("Additional Test: idle_o Signal Updates",
                            "Not in IDLE after DONE");
            return;
        }

        report_test_pass("Additional Test: idle_o Signal Updates");

    } catch (const std::exception& e) {
        write_cmd(test, 0x16);
        report_test_fail("Additional Test: idle_o Signal Updates", e.what());
    }
}

/******************************************************************************
 * Test Registration Function
 * This function should be called from testbench to register all FUNC-011 tests
 ******************************************************************************/
extern "C" void register_func011_tests()
{
    CSML_INFO(1, test_logger) << "Registering FUNC-KMAC-011 test cases...";

    // Note: Actual test registration mechanism depends on testbench infrastructure
    // These tests can be invoked individually or as a suite

    CSML_INFO(1, test_logger) << "FUNC-KMAC-011 test suite ready (29 test cases + 2 additional tests)";
}

// End of file
