/******************************************************************************
 * @file kmac_func022_test.cpp
 * @brief Test cases for FUNC-KMAC-022 (Idle Status Signaling)
 *
 * This file implements test cases for FUNC-KMAC-022, verifying proper
 * idle_o signal behavior. Tests ensure accurate idle status signaling
 * for external hardware dependency checking.
 *
 * Test Coverage:
 * - TC-071: idle_o = 1 when FSM in IDLE state after reset
 * - TC-072: idle_o transitions to 0 when entering ABSORB state
 * - TC-074: idle_o returns to 1 when FSM returns to IDLE
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Architecture Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-architecture-behaviour-map.json
 * Detailed Design: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-detailed-design.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "csml_logger.h"
#include <iostream>
#include <iomanip>

// Logger for test output
static CsmlLogger test_logger;

/******************************************************************************
 * Helper Functions
 ******************************************************************************/

/**
 * @brief Helper function to read STATUS register FSM state bits
 * @param test Pointer to test harness
 * @param sha3_idle Output: sha3_idle bit value (bit 0)
 * @param sha3_absorb Output: sha3_absorb bit value (bit 1)
 * @param sha3_squeeze Output: sha3_squeeze bit value (bit 2)
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
 * @brief Helper function to configure CFG_SHADOWED for SHA3-256 mode
 * @param test Pointer to test harness
 */
static void configure_sha3_256_mode(kmac_test* test)
{
    // CFG_SHADOWED: mode=0x0 (SHA3), kstrength=0x2 (L256), kmac_en=0
    uint32_t cfg_val = (0 << 0) | (0x2 << 1) | (0x0 << 4) | (0x1 << 16) | (0x1 << 24);

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to issue CMD register write
 * @param test Pointer to test harness
 * @param cmd_value Command value to write
 */
static void write_cmd(kmac_test* test, uint32_t cmd_value)
{
    test->register_write_32(test->CMD_OFFSET, cmd_value);
    wait(10, SC_NS); // Allow time for command processing
}

/******************************************************************************
 * TC-071: idle_o = 1 When FSM in IDLE State After Reset
 *
 * Verifies that the idle_o output signal is correctly asserted (high) when
 * the KMAC FSM is in IDLE state after reset.
 ******************************************************************************/
void testbench::test_idle_o_high_after_reset()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-071: test_idle_o_high_after_reset");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        // Read idle_o signal
        bool idle_o_value = test->idle_i.read();

        // Verify idle_o is high
        if (idle_o_value) {
            CSML_INFO(2, test_logger) << "idle_o correctly asserted (1) after reset";
        } else {
            report_test_fail("TC-071: test_idle_o_high_after_reset",
                           "idle_o not asserted after reset");
            return;
        }

        // Verify STATUS.sha3_idle matches idle_o
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);

        if (sha3_idle && idle_o_value) {
            CSML_INFO(2, test_logger) << "STATUS.sha3_idle and idle_o both correctly set";
            report_test_pass("TC-071: test_idle_o_high_after_reset");
        } else {
            std::stringstream reason;
            reason << "Mismatch: STATUS.sha3_idle=" << sha3_idle
                   << ", idle_o=" << idle_o_value;
            report_test_fail("TC-071: test_idle_o_high_after_reset", reason.str());
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-071: test_idle_o_high_after_reset", e.what());
    }
}

/******************************************************************************
 * TC-072: idle_o Transitions to 0 When Entering ABSORB State
 *
 * Verifies that the idle_o output signal correctly transitions from 1 to 0
 * when the KMAC FSM transitions from IDLE to ABSORB state via START command.
 ******************************************************************************/
void testbench::test_idle_o_low_during_absorb()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-072: test_idle_o_low_during_absorb");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        // Verify initial idle_o = 1
        bool idle_o_initial = test->idle_i.read();
        if (!idle_o_initial) {
            report_test_fail("TC-072: test_idle_o_low_during_absorb",
                           "idle_o not high initially");
            return;
        }
        CSML_INFO(2, test_logger) << "Initial state: idle_o = 1 (correct)";

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Issue START command to enter ABSORB state
        CSML_INFO(2, test_logger) << "Issuing START command to enter ABSORB state";
        write_cmd(test, 0x1D); // START command
        wait(5, SC_NS); // Allow time for idle_o update

        // Read idle_o signal
        bool idle_o_after_start = test->idle_i.read();

        // Verify idle_o is low
        if (!idle_o_after_start) {
            CSML_INFO(2, test_logger) << "idle_o correctly deasserted (0) during ABSORB state";
        } else {
            report_test_fail("TC-072: test_idle_o_low_during_absorb",
                           "idle_o not deasserted during ABSORB");
            write_cmd(test, 0x16); // DONE command for cleanup
            return;
        }

        // Verify STATUS.sha3_absorb is set and matches idle_o state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);

        if (sha3_absorb && !idle_o_after_start && !sha3_idle) {
            CSML_INFO(2, test_logger) << "FSM state and idle_o signal correctly synchronized";
            report_test_pass("TC-072: test_idle_o_low_during_absorb");
        } else {
            std::stringstream reason;
            reason << "State mismatch: sha3_absorb=" << sha3_absorb
                   << ", sha3_idle=" << sha3_idle
                   << ", idle_o=" << idle_o_after_start;
            report_test_fail("TC-072: test_idle_o_low_during_absorb", reason.str());
        }

        // Cleanup
        write_cmd(test, 0x16); // DONE command

    } catch (const std::exception& e) {
        report_test_fail("TC-072: test_idle_o_low_during_absorb", e.what());
    }
}

/******************************************************************************
 * TC-074: idle_o Returns to 1 When FSM Returns to IDLE
 *
 * Verifies that the idle_o output signal correctly returns to 1 when the
 * KMAC FSM transitions back to IDLE state via DONE command.
 ******************************************************************************/
void testbench::test_idle_o_returns_high_on_done()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-074: test_idle_o_returns_high_on_done");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Issue START command
        CSML_INFO(2, test_logger) << "Starting hash operation";
        write_cmd(test, 0x1D); // START command

        // Verify idle_o is low
        bool idle_o_during_op = test->idle_i.read();
        if (idle_o_during_op) {
            report_test_fail("TC-074: test_idle_o_returns_high_on_done",
                           "idle_o not low during operation");
            return;
        }
        CSML_INFO(2, test_logger) << "During operation: idle_o = 0 (correct)";

        // Issue PROCESS command
        write_cmd(test, 0x2E); // PROCESS command
        wait(20, SC_NS); // Allow time for Keccak rounds

        // Verify still not idle
        idle_o_during_op = test->idle_i.read();
        if (idle_o_during_op) {
            report_test_fail("TC-074: test_idle_o_returns_high_on_done",
                           "idle_o high during SQUEEZE state");
            return;
        }
        CSML_INFO(2, test_logger) << "During SQUEEZE: idle_o = 0 (correct)";

        // Issue DONE command to return to IDLE
        CSML_INFO(2, test_logger) << "Issuing DONE command to return to IDLE";
        write_cmd(test, 0x16); // DONE command
        wait(5, SC_NS); // Allow time for idle_o update

        // Read idle_o signal
        bool idle_o_after_done = test->idle_i.read();

        // Verify idle_o is high again
        if (idle_o_after_done) {
            CSML_INFO(2, test_logger) << "idle_o correctly reasserted (1) after DONE command";
        } else {
            report_test_fail("TC-074: test_idle_o_returns_high_on_done",
                           "idle_o not reasserted after DONE");
            return;
        }

        // Verify STATUS.sha3_idle is set and matches idle_o
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);

        if (sha3_idle && idle_o_after_done && !sha3_absorb && !sha3_squeeze) {
            CSML_INFO(2, test_logger) << "FSM returned to IDLE, idle_o correctly synchronized";
            report_test_pass("TC-074: test_idle_o_returns_high_on_done");
        } else {
            std::stringstream reason;
            reason << "State mismatch: sha3_idle=" << sha3_idle
                   << ", idle_o=" << idle_o_after_done
                   << ", sha3_absorb=" << sha3_absorb
                   << ", sha3_squeeze=" << sha3_squeeze;
            report_test_fail("TC-074: test_idle_o_returns_high_on_done", reason.str());
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-074: test_idle_o_returns_high_on_done", e.what());
    }
}

// End of file
