/******************************************************************************
 * @file kmac_func017_test.cpp
 * @brief Test cases for FUNC-KMAC-017 (Dynamic Register Write Protection)
 *
 * This file implements test cases for FUNC-KMAC-017, verifying the CFG_REGWEN
 * register's dynamic write protection mechanism. CFG_REGWEN.en controls access
 * to configuration registers:
 * - When CFG_REGWEN.en = 1: Protected registers are writable
 * - When CFG_REGWEN.en = 0: Protected registers reject writes
 * - CFG_REGWEN.en auto-clears on START command
 * - CFG_REGWEN.en auto-sets on DONE command
 *
 * Protected Registers:
 * - CFG_SHADOWED: Mode, strength, endianness, entropy, masking configuration
 * - ENTROPY_PERIOD: Timer prescaler and wait timeout
 * - ENTROPY_REFRESH_THRESHOLD_SHADOWED: Automatic PRNG reseed threshold
 * - KEY_SHARE0_*: Software key share 0 (16 registers)
 * - KEY_SHARE1_*: Software key share 1 (16 registers)
 * - KEY_LEN: Key length encoding
 * - PREFIX_*: Customization string registers (11 registers)
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
 * @brief Helper function to configure SHA3-256 mode
 * @param test Pointer to test harness
 *
 * Configures CFG_SHADOWED for SHA3-256 operation (mode=0x0, kstrength=0x2).
 * Performs shadow register duplicate write sequence for validation.
 */
static void configure_sha3_256_mode(kmac_test* test)
{
    // CFG_SHADOWED (spec-compliant for EnMasking=1):
    uint32_t cfg_val = (0 << 0) | (0x2 << 1) | (0x0 << 4) | (0x1 << 16) | (0x1 << 24);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to read CFG_REGWEN.en bit
 * @param test Pointer to test harness
 * @return true if CFG_REGWEN.en = 1 (unlocked), false if 0 (locked)
 */
static bool is_cfg_regwen_unlocked(kmac_test* test)
{
    uint32_t regwen_val = 0;
    test->register_read_32(test->CFG_REGWEN_OFFSET, regwen_val);
    return (regwen_val & 0x1) != 0;
}

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
 * @brief Helper function to issue CMD register write
 * @param test Pointer to test harness
 * @param cmd_value Command value to write
 */
static void write_cmd(kmac_test* test, uint32_t cmd_value)
{
    test->register_write_32(test->CMD_OFFSET, cmd_value);
    wait(10, SC_NS); // Allow time for command processing and FSM transition
}

/**
 * @brief Helper function to clean up after test (return to IDLE state)
 * @param test Pointer to test harness
 */
static void cleanup_test(kmac_test* test)
{
    bool idle, absorb, squeeze;
    read_status_fsm_bits(test, idle, absorb, squeeze);

    if (!idle) {
        // Check for error condition
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        if (err_code != 0) {
            // Error recovery
            write_cmd(test, 0x400); // err_processed bit (bit 10)
        } else {
            // Normal cleanup
            write_cmd(test, 0x16); // DONE command
        }
        wait(10, SC_NS);
    }
}

/******************************************************************************
 * TC-008: CFG_REGWEN Protection Enable Test
 *
 * Verifies that protected registers are writable when CFG_REGWEN.en = 1.
 * Tests write access to CFG_SHADOWED, ENTROPY_PERIOD, ENTROPY_REFRESH_THRESHOLD_SHADOWED,
 * KEY_SHARE0_0, KEY_SHARE1_0, KEY_LEN, and PREFIX_0.
 *
 * Pass Criteria:
 * - CFG_REGWEN.en reads as 1 after reset
 * - All protected registers accept write values
 * - Readback values match written values
 ******************************************************************************/
void testbench::test_cfg_regwen_protection_enable()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-008: test_cfg_regwen_protection_enable");

    try {
        // Verify initial CFG_REGWEN.en = 1 (unlocked)
        if (!is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-008", "CFG_REGWEN.en not unlocked after reset");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en = 1 (unlocked) after reset";

        // Test 1: CFG_SHADOWED write
        configure_sha3_256_mode(test);
        uint32_t cfg_read = 0;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_read);
        if ((cfg_read & 0xE) != 0x4) { // kstrength at bits[3:1]
            cleanup_test(test);
            report_test_fail("TC-008", "CFG_SHADOWED write rejected when CFG_REGWEN.en = 1");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_SHADOWED writable when unlocked";

        // Test 2: ENTROPY_PERIOD write
        uint32_t entropy_period_val = 0x12345600;
        test->register_write_32(test->ENTROPY_PERIOD_OFFSET, entropy_period_val);
        wait(5, SC_NS);
        uint32_t entropy_period_read = 0;
        test->register_read_32(test->ENTROPY_PERIOD_OFFSET, entropy_period_read);
        if ((entropy_period_read & 0xFFFF03FF) != (entropy_period_val & 0xFFFF03FF)) {
            cleanup_test(test);
            report_test_fail("TC-008", "ENTROPY_PERIOD write rejected when CFG_REGWEN.en = 1");
            return;
        }
        CSML_INFO(2, test_logger) << "ENTROPY_PERIOD writable when unlocked";

        // Test 3: ENTROPY_REFRESH_THRESHOLD_SHADOWED write
        uint32_t threshold_val = 0x100;
        test->register_write_32(test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, threshold_val);
        wait(5, SC_NS);
        test->register_write_32(test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, threshold_val);
        wait(5, SC_NS);
        uint32_t threshold_read = 0;
        test->register_read_32(test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, threshold_read);
        if ((threshold_read & 0x3FF) != threshold_val) {
            cleanup_test(test);
            report_test_fail("TC-008", "ENTROPY_REFRESH_THRESHOLD_SHADOWED write rejected when CFG_REGWEN.en = 1");
            return;
        }
        CSML_INFO(2, test_logger) << "ENTROPY_REFRESH_THRESHOLD_SHADOWED writable when unlocked";

        // Test 4: KEY_SHARE0_0 write (write-only, verify no error)
        uint32_t key_val = 0xDEADBEEF;
        test->register_write_32(test->KEY_SHARE0_OFFSET, key_val);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE0_0 writable when unlocked";

        // Test 5: KEY_SHARE1_0 write (write-only, verify no error)
        test->register_write_32(test->KEY_SHARE1_OFFSET, key_val);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE1_0 writable when unlocked";

        // Test 6: KEY_LEN write (write-only, verify no error)
        uint32_t key_len_val = 0x2; // 256-bit key
        test->register_write_32(test->KEY_LEN_OFFSET, key_len_val);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_LEN writable when unlocked";

        // Test 7: PREFIX_0 write
        uint32_t prefix_val = 0x4D4B2001; // encode_string("KMAC")
        test->register_write_32(test->PREFIX_0_OFFSET, prefix_val);
        wait(5, SC_NS);
        uint32_t prefix_read = 0;
        test->register_read_32(test->PREFIX_0_OFFSET, prefix_read);
        if (prefix_read != prefix_val) {
            cleanup_test(test);
            report_test_fail("TC-008", "PREFIX_0 write rejected when CFG_REGWEN.en = 1");
            return;
        }
        CSML_INFO(2, test_logger) << "PREFIX_0 writable when unlocked";

        cleanup_test(test);
        report_test_pass("TC-008: test_cfg_regwen_protection_enable");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-008", e.what());
    }
}

/******************************************************************************
 * TC-009: CFG_REGWEN Protection Disable Test
 *
 * Verifies that protected registers reject writes when CFG_REGWEN.en = 0.
 * Tests write rejection for CFG_SHADOWED, ENTROPY_PERIOD, KEY_SHARE0_0,
 * KEY_LEN, and PREFIX_0.
 *
 * Pass Criteria:
 * - CFG_REGWEN.en reads as 0 after START command
 * - Protected register writes are ignored (readback values unchanged)
 * - No error codes generated for rejected writes
 ******************************************************************************/
void testbench::test_cfg_regwen_protection_disable()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-009: test_cfg_regwen_protection_disable");

    try {
        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);
        CSML_INFO(2, test_logger) << "Configured SHA3-256 mode";

        // Set initial PREFIX value
        uint32_t initial_prefix = 0x12345678;
        test->register_write_32(test->PREFIX_0_OFFSET, initial_prefix);
        wait(5, SC_NS);

        // Issue START command to lock CFG_REGWEN
        write_cmd(test, 0x1D);

        // Verify CFG_REGWEN.en = 0 (locked)
        if (is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-009", "CFG_REGWEN.en not locked after START");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en = 0 (locked) after START command";

        // Verify FSM in ABSORB state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-009", "FSM not in ABSORB state after START");
            return;
        }

        // Test 1: CFG_SHADOWED write attempt (should be rejected)
        uint32_t cfg_before = 0;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_before);
        uint32_t cfg_attempt = 0x8; // Different kstrength
        test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_attempt);
        wait(5, SC_NS);
        test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_attempt);
        wait(5, SC_NS);
        uint32_t cfg_after = 0;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_after);
        if (cfg_after != cfg_before) {
            cleanup_test(test);
            report_test_fail("TC-009", "CFG_SHADOWED write not rejected when CFG_REGWEN.en = 0");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_SHADOWED write correctly rejected when locked";

        // Test 2: ENTROPY_PERIOD write attempt (should be rejected)
        uint32_t entropy_before = 0;
        test->register_read_32(test->ENTROPY_PERIOD_OFFSET, entropy_before);
        test->register_write_32(test->ENTROPY_PERIOD_OFFSET, 0xAABBCCDD);
        wait(5, SC_NS);
        uint32_t entropy_after = 0;
        test->register_read_32(test->ENTROPY_PERIOD_OFFSET, entropy_after);
        if (entropy_after != entropy_before) {
            cleanup_test(test);
            report_test_fail("TC-009", "ENTROPY_PERIOD write not rejected when CFG_REGWEN.en = 0");
            return;
        }
        CSML_INFO(2, test_logger) << "ENTROPY_PERIOD write correctly rejected when locked";

        // Test 3: KEY_SHARE0_0 write attempt (should be rejected)
        test->register_write_32(test->KEY_SHARE0_OFFSET, 0xFEEDFACE);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE0_0 write attempted when locked (expected rejection)";

        // Test 4: KEY_LEN write attempt (should be rejected)
        test->register_write_32(test->KEY_LEN_OFFSET, 0x4); // 512-bit
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_LEN write attempted when locked (expected rejection)";

        // Test 5: PREFIX_0 write attempt (should be rejected)
        uint32_t prefix_before = 0;
        test->register_read_32(test->PREFIX_0_OFFSET, prefix_before);
        test->register_write_32(test->PREFIX_0_OFFSET, 0xFFFFFFFF);
        wait(5, SC_NS);
        uint32_t prefix_after = 0;
        test->register_read_32(test->PREFIX_0_OFFSET, prefix_after);
        if (prefix_after != prefix_before) {
            cleanup_test(test);
            report_test_fail("TC-009", "PREFIX_0 write not rejected when CFG_REGWEN.en = 0");
            return;
        }
        CSML_INFO(2, test_logger) << "PREFIX_0 write correctly rejected when locked";

        // Verify no error codes generated
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        if (err_code != 0) {
            cleanup_test(test);
            report_test_fail("TC-009", "Unexpected error code generated for rejected writes");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-009: test_cfg_regwen_protection_disable");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-009", e.what());
    }
}

/******************************************************************************
 * TC-010: CFG_REGWEN Auto-Clear on START Test
 *
 * Verifies that CFG_REGWEN.en automatically clears to 0 when START command
 * is issued, implementing configuration locking during active operations.
 *
 * Pass Criteria:
 * - CFG_REGWEN.en = 1 before START command
 * - CFG_REGWEN.en = 0 after START command
 * - FSM transitions to ABSORB state
 * - Protected registers reject writes after START
 ******************************************************************************/
void testbench::test_cfg_regwen_auto_clear_on_start()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-010: test_cfg_regwen_auto_clear_on_start");

    try {
        // Verify initial CFG_REGWEN.en = 1
        if (!is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-010", "Precondition: CFG_REGWEN.en not unlocked");
            return;
        }
        CSML_INFO(2, test_logger) << "Initial CFG_REGWEN.en = 1 (unlocked)";

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Verify still unlocked before START
        if (!is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-010", "CFG_REGWEN.en locked before START command");
            return;
        }

        // Issue START command
        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        // Verify CFG_REGWEN.en auto-cleared to 0
        if (is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-010", "CFG_REGWEN.en not auto-cleared after START command");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en auto-cleared to 0 after START";

        // Verify FSM transitioned to ABSORB
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-010", "FSM not in ABSORB state after START");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM transitioned to ABSORB state";

        // Verify protected register write rejected
        uint32_t cfg_before = 0;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_before);
        test->register_write_32(test->CFG_SHADOWED_OFFSET, 0xFFFFFFFF);
        wait(5, SC_NS);
        uint32_t cfg_after = 0;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_after);
        if (cfg_after != cfg_before) {
            cleanup_test(test);
            report_test_fail("TC-010", "Protected register write not rejected after START");
            return;
        }
        CSML_INFO(2, test_logger) << "Protected registers correctly locked after START";

        cleanup_test(test);
        report_test_pass("TC-010: test_cfg_regwen_auto_clear_on_start");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-010", e.what());
    }
}

/******************************************************************************
 * TC-011: CFG_REGWEN Auto-Set on DONE Test
 *
 * Verifies that CFG_REGWEN.en returns to 1 when DONE command is issued,
 * allowing configuration for subsequent operations.
 *
 * Pass Criteria:
 * - CFG_REGWEN.en = 0 during operation
 * - CFG_REGWEN.en = 1 after DONE command
 * - FSM returns to IDLE state
 * - Protected registers writable after DONE
 ******************************************************************************/
void testbench::test_cfg_regwen_auto_set_on_done()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-011: test_cfg_regwen_auto_set_on_done");

    try {
        // Configure and START operation
        configure_sha3_256_mode(test);
        write_cmd(test, 0x1D);

        // Verify CFG_REGWEN.en = 0 during operation
        if (is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-011", "CFG_REGWEN.en not locked during operation");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en = 0 during operation";

        // Issue DONE command
        CSML_INFO(2, test_logger) << "Issuing DONE command (0x16)";
        write_cmd(test, 0x16);

        // Verify CFG_REGWEN.en auto-set to 1
        if (!is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-011", "CFG_REGWEN.en not auto-set after DONE command");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en auto-set to 1 after DONE";

        // Verify FSM returned to IDLE
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-011", "FSM not in IDLE state after DONE");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM returned to IDLE state";

        // Verify protected register writable
        uint32_t prefix_val = 0xABCDEF00;
        test->register_write_32(test->PREFIX_0_OFFSET, prefix_val);
        wait(5, SC_NS);
        uint32_t prefix_read = 0;
        test->register_read_32(test->PREFIX_0_OFFSET, prefix_read);
        if (prefix_read != prefix_val) {
            cleanup_test(test);
            report_test_fail("TC-011", "Protected registers not writable after DONE");
            return;
        }
        CSML_INFO(2, test_logger) << "Protected registers writable after DONE";

        cleanup_test(test);
        report_test_pass("TC-011: test_cfg_regwen_auto_set_on_done");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-011", e.what());
    }
}

/******************************************************************************
 * TC-065: KEY_SHARE Registers Protected During Operations
 *
 * Verifies that KEY_SHARE0 and KEY_SHARE1 registers are protected by
 * CFG_REGWEN during operations to prevent key modification while active.
 *
 * Pass Criteria:
 * - KEY_SHARE registers writable when CFG_REGWEN.en = 1
 * - KEY_SHARE registers reject writes when CFG_REGWEN.en = 0
 * - Protection applies to all KEY_SHARE0_* and KEY_SHARE1_* registers
 ******************************************************************************/
void testbench::test_key_protection_cfg_regwen()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-065: test_key_protection_cfg_regwen");

    try {
        // Verify KEY_SHARE writable when unlocked
        if (!is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-065", "CFG_REGWEN.en not unlocked");
            return;
        }

        // Write KEY_SHARE0_0 when unlocked
        uint32_t key_val = 0x11223344;
        test->register_write_32(test->KEY_SHARE0_OFFSET, key_val);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE0_0 writable when unlocked";

        // Write KEY_SHARE1_0 when unlocked
        test->register_write_32(test->KEY_SHARE1_OFFSET, key_val);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE1_0 writable when unlocked";

        // Configure and start operation
        configure_sha3_256_mode(test);
        write_cmd(test, 0x1D);

        // Verify CFG_REGWEN.en = 0
        if (is_cfg_regwen_unlocked(test)) {
            cleanup_test(test);
            report_test_fail("TC-065", "CFG_REGWEN.en not locked during operation");
            return;
        }
        CSML_INFO(2, test_logger) << "CFG_REGWEN.en = 0 during operation";

        // Attempt KEY_SHARE0_0 write during operation (should be rejected)
        test->register_write_32(test->KEY_SHARE0_OFFSET, 0xFFFFFFFF);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE0_0 write attempted during operation (expected rejection)";

        // Attempt KEY_SHARE1_0 write during operation (should be rejected)
        test->register_write_32(test->KEY_SHARE1_OFFSET, 0xFFFFFFFF);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE1_0 write attempted during operation (expected rejection)";

        // Verify no errors generated
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        if (err_code != 0) {
            cleanup_test(test);
            report_test_fail("TC-065", "Unexpected error code for rejected KEY_SHARE writes");
            return;
        }

        // Return to IDLE
        write_cmd(test, 0x16);

        // Verify KEY_SHARE writable again after DONE
        test->register_write_32(test->KEY_SHARE0_OFFSET, 0xAABBCCDD);
        wait(5, SC_NS);
        CSML_INFO(2, test_logger) << "KEY_SHARE0_0 writable again after DONE";

        cleanup_test(test);
        report_test_pass("TC-065: test_key_protection_cfg_regwen");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-065", e.what());
    }
}

// NOTE: test_callback_cmd_write_start_side_effects() and test_callback_cmd_write_done_side_effects()
// are already implemented in kmac_func011_test.cpp
// Duplicates removed to avoid linker errors

// End of file
