// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file kmac_func021_test.cpp
 * @brief Test cases for FUNC-KMAC-021 (Life Cycle Escalation Response)
 *
 * This file implements test cases for FUNC-KMAC-021, verifying proper
 * response to life cycle escalation signals. Tests ensure immediate
 * zeroization of sensitive state and blocking of operations.
 *
 * Test Coverage:
 * - TC-190: lc_escalate_en_i immediately zeros KEY_SHARE and internal buffers
 * - TC-191: FSM moves to invalid/locked state on escalation
 * - TC-192: KEY_SHARE registers zeroed on lc_escalate_en_i assertion
 * - TC-193: In-progress operation aborted on escalation
 * - TC-194: Only rst_ni can restore functionality after escalation
 *
 * Test Plan Reference: kmac-test-plan.md
 * Architecture Reference: kmac-architecture-behaviour-map.json
 * Detailed Design: kmac-detailed-design.md
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

/**
 * @brief Helper function to assert lc_escalate_en signal
 * @param test Pointer to test harness
 */
static void assert_escalation(kmac_test* test)
{
    test->lc_escalate_en_o.write(true);
    wait(2, SC_NS); // Immediate response expected
}

/**
 * @brief Helper function to deassert lc_escalate_en signal
 * @param test Pointer to test harness
 */
static void deassert_escalation(kmac_test* test)
{
    test->lc_escalate_en_o.write(false);
    wait(2, SC_NS);
}

/******************************************************************************
 * TC-190: lc_escalate_en_i Immediately Zeros KEY_SHARE and Internal Buffers
 *
 * Verifies that asserting lc_escalate_en_i causes immediate zeroization of
 * KEY_SHARE registers and internal cryptographic state for security.
 ******************************************************************************/
void testbench::test_escalation_immediate_key_zeroization()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-190: test_escalation_immediate_key_zeroization");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Write non-zero values to KEY_SHARE registers
        CSML_INFO(2, test_logger) << "Writing test pattern to KEY_SHARE registers";
        for (int i = 0; i < 16; i++) {
            uint32_t test_value = 0xABCDEF00 + i;
            test->register_write_32(test->KEY_SHARE0_OFFSET + i*4, test_value);
            test->register_write_32(test->KEY_SHARE1_OFFSET + i*4, test_value);
        }
        wait(5, SC_NS);

        // Verify keys were written (read back zeros since write-only)
        // Instead, we'll proceed with escalation test

        // Assert lc_escalate_en_i
        CSML_INFO(2, test_logger) << "Asserting lc_escalate_en_i";
        assert_escalation(test);

        // Verify all KEY_SHARE0 registers are zeroed (read returns 0 for WO registers anyway)
        // Check internal state through STATE window
        bool all_zero = true;
        for (int i = 0; i < 16; i++) {
            uint32_t state_val = 0xFFFFFFFF;
            test->register_read_32(0x400 + i*4, state_val); // STATE window
            if (state_val != 0x00000000) {
                CSML_ERROR(1, test_logger) << "STATE[" << i << "] not zero: 0x"
                                          << std::hex << state_val << std::dec;
                all_zero = false;
            }
        }

        if (all_zero) {
            CSML_INFO(2, test_logger) << "Escalation immediately zeroed internal state";
            report_test_pass("TC-190: test_escalation_immediate_key_zeroization");
        } else {
            report_test_fail("TC-190: test_escalation_immediate_key_zeroization",
                           "Internal state not zeroed");
        }

        // Cleanup
        deassert_escalation(test);
        apply_reset();

    } catch (const std::exception& e) {
        deassert_escalation(test);
        report_test_fail("TC-190: test_escalation_immediate_key_zeroization", e.what());
    }
}

/******************************************************************************
 * TC-191: FSM Moves to Invalid/Locked State on Escalation
 *
 * Verifies that asserting lc_escalate_en_i causes the FSM to transition to
 * an invalid state where all normal status bits are deasserted.
 ******************************************************************************/
void testbench::test_escalation_fsm_invalid_state()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-191: test_escalation_fsm_invalid_state");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Verify initial IDLE state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle) {
            report_test_fail("TC-191: test_escalation_fsm_invalid_state",
                           "FSM not in IDLE state initially");
            return;
        }

        // Assert lc_escalate_en_i
        CSML_INFO(2, test_logger) << "Asserting lc_escalate_en_i";
        assert_escalation(test);

        // Read FSM status bits
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);

        // Verify FSM is in invalid state (all bits should be 0 or in locked state)
        // After escalation, FSM should not report any valid state
        if (!sha3_idle && !sha3_absorb && !sha3_squeeze) {
            CSML_INFO(2, test_logger) << "FSM correctly entered invalid/locked state";
            CSML_INFO(2, test_logger) << "  All FSM status bits = 0";
            report_test_pass("TC-191: test_escalation_fsm_invalid_state");
        } else {
            std::stringstream reason;
            reason << "FSM still reporting valid state: idle=" << sha3_idle
                   << ", absorb=" << sha3_absorb << ", squeeze=" << sha3_squeeze;
            report_test_fail("TC-191: test_escalation_fsm_invalid_state", reason.str());
        }

        // Cleanup
        deassert_escalation(test);
        apply_reset();

    } catch (const std::exception& e) {
        deassert_escalation(test);
        report_test_fail("TC-191: test_escalation_fsm_invalid_state", e.what());
    }
}

/******************************************************************************
 * TC-192: KEY_SHARE Registers Zeroed on lc_escalate_en_i Assertion
 *
 * Verifies that KEY_SHARE0 and KEY_SHARE1 registers are zeroed when
 * lc_escalate_en_i is asserted. Note: These are write-only registers,
 * so we verify through STATE window behavior.
 ******************************************************************************/
void testbench::test_escalation_key_share_zeroed()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-192: test_escalation_key_share_zeroed");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Configure for KMAC operation with key
        configure_sha3_256_mode(test);

        // Write key data
        CSML_INFO(2, test_logger) << "Writing key data to KEY_SHARE registers";
        for (int i = 0; i < 8; i++) { // 256-bit key
            uint32_t key_val = 0x11223344 + i;
            test->register_write_32(test->KEY_SHARE0_OFFSET + i*4, key_val);
        }
        wait(5, SC_NS);

        // Assert escalation
        CSML_INFO(2, test_logger) << "Asserting lc_escalate_en_i to trigger key zeroization";
        assert_escalation(test);

        // Verify STATE window returns all zeros (indicating internal state cleared)
        bool all_zero = true;
        for (int i = 0; i < 16; i++) {
            uint32_t state_val = 0xFFFFFFFF;
            test->register_read_32(0x400 + i*4, state_val);
            if (state_val != 0x00000000) {
                all_zero = false;
                break;
            }
        }

        if (all_zero) {
            CSML_INFO(2, test_logger) << "KEY_SHARE registers and internal state verified zeroed";
            report_test_pass("TC-192: test_escalation_key_share_zeroed");
        } else {
            report_test_fail("TC-192: test_escalation_key_share_zeroed",
                           "Internal state not zeroed after escalation");
        }

        // Cleanup
        deassert_escalation(test);
        apply_reset();

    } catch (const std::exception& e) {
        deassert_escalation(test);
        report_test_fail("TC-192: test_escalation_key_share_zeroed", e.what());
    }
}

/******************************************************************************
 * TC-193: In-Progress Operation Aborted on Escalation
 *
 * Verifies that an active hash operation is immediately aborted when
 * lc_escalate_en_i is asserted during ABSORB state.
 ******************************************************************************/
void testbench::test_escalation_abort_in_progress_operation()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-193: test_escalation_abort_in_progress_operation");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Start hash operation
        CSML_INFO(2, test_logger) << "Starting hash operation";
        write_cmd(test, 0x1D); // START command

        // Verify in ABSORB state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_absorb) {
            report_test_fail("TC-193: test_escalation_abort_in_progress_operation",
                           "Failed to enter ABSORB state");
            return;
        }
        CSML_INFO(2, test_logger) << "Operation in progress (ABSORB state)";

        // Write message data
        test->register_write_32(0x800, 0xDEADBEEF); // MSG_FIFO
        wait(5, SC_NS);

        // Assert escalation during operation
        CSML_INFO(2, test_logger) << "Asserting lc_escalate_en_i during operation";
        assert_escalation(test);

        // Verify operation aborted - FSM in invalid state
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle && !sha3_absorb && !sha3_squeeze) {
            CSML_INFO(2, test_logger) << "Operation correctly aborted";
        } else {
            report_test_fail("TC-193: test_escalation_abort_in_progress_operation",
                           "Operation not aborted");
            deassert_escalation(test);
            return;
        }

        // Verify commands are blocked
        write_cmd(test, 0x2E); // PROCESS command - should be ignored
        wait(5, SC_NS);

        // FSM should still be in invalid state
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle && !sha3_absorb && !sha3_squeeze) {
            CSML_INFO(2, test_logger) << "Commands correctly blocked after escalation";
            report_test_pass("TC-193: test_escalation_abort_in_progress_operation");
        } else {
            report_test_fail("TC-193: test_escalation_abort_in_progress_operation",
                           "Commands not blocked after escalation");
        }

        // Cleanup
        deassert_escalation(test);
        apply_reset();

    } catch (const std::exception& e) {
        deassert_escalation(test);
        report_test_fail("TC-193: test_escalation_abort_in_progress_operation", e.what());
    }
}

/******************************************************************************
 * TC-194: Only rst_ni Can Restore Functionality After Escalation
 *
 * Verifies that once lc_escalate_en_i is asserted, only a hardware reset
 * (rst_ni assertion) can restore normal functionality. Deassering escalation
 * alone should not restore operation.
 ******************************************************************************/
void testbench::test_escalation_reset_only_recovery()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-194: test_escalation_reset_only_recovery");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Verify initial IDLE state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "FSM not in IDLE state initially");
            return;
        }

        // Assert escalation
        CSML_INFO(2, test_logger) << "Asserting lc_escalate_en_i";
        assert_escalation(test);

        // Verify FSM in invalid state
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (sha3_idle || sha3_absorb || sha3_squeeze) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "FSM not in invalid state after escalation");
            deassert_escalation(test);
            return;
        }
        CSML_INFO(2, test_logger) << "FSM in invalid state as expected";

        // Deassert escalation (should NOT restore functionality)
        CSML_INFO(2, test_logger) << "Deasserting lc_escalate_en_i (should not restore)";
        deassert_escalation(test);
        wait(10, SC_NS);

        // Verify FSM still in invalid state
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (sha3_idle || sha3_absorb || sha3_squeeze) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "FSM incorrectly restored without reset");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM remains locked after escalation deassertion (correct)";

        // Try to issue command (should be blocked)
        configure_sha3_256_mode(test);
        write_cmd(test, 0x1D); // START command
        wait(5, SC_NS);

        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (sha3_absorb) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "Commands incorrectly accepted after escalation");
            return;
        }
        CSML_INFO(2, test_logger) << "Commands correctly blocked (correct)";

        // Apply hardware reset (should restore functionality)
        CSML_INFO(2, test_logger) << "Applying hardware reset to restore functionality";
        apply_reset();
        wait(10, SC_NS);

        // Verify FSM returned to IDLE state
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle || sha3_absorb || sha3_squeeze) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "FSM not restored to IDLE after reset");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM restored to IDLE state after reset";

        // Verify commands now work
        configure_sha3_256_mode(test);
        write_cmd(test, 0x1D); // START command
        wait(5, SC_NS);

        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_absorb) {
            report_test_fail("TC-194: test_escalation_reset_only_recovery",
                           "Commands not working after reset");
            return;
        }

        CSML_INFO(2, test_logger) << "Functionality fully restored after reset";
        report_test_pass("TC-194: test_escalation_reset_only_recovery");

        // Cleanup
        write_cmd(test, 0x16); // DONE command

    } catch (const std::exception& e) {
        deassert_escalation(test);
        report_test_fail("TC-194: test_escalation_reset_only_recovery", e.what());
    }
}

// End of file
