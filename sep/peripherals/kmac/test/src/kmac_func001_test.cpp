// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file kmac_func001_test.cpp
 * @brief Test cases for FUNC-KMAC-001 (SHA3 Hash Operation - Phase 1)
 *
 * This file implements test cases for FUNC-KMAC-001 Phase 1, focusing on
 * OpenSSL context initialization and SHA3 algorithm selection validation
 * in the START command implementation.
 *
 * Phase 1 Implementation Coverage:
 * - OpenSSL EVP_MD context initialization with correct SHA3 algorithm variant
 * - Algorithm selection based on CFG_SHADOWED.mode (0x0=SHA3) and kstrength
 * - UnexpectedModeStrength error detection and reporting (ERR_CODE = 0x06)
 * - Valid kstrength values for SHA3: 0x1 (L224), 0x2 (L256), 0x3 (L384), 0x4 (L512)
 * - Invalid kstrength values for SHA3: 0x0 (L128), 0x5-0x7 (reserved)
 * - FSM transition validation: IDLE → ABSORB on successful START
 * - Configuration locking: CFG_REGWEN.en auto-clear on START
 * - Shadow register duplicate write validation
 *
 * Deferred to Later Phases:
 * - Full hash computation with MSG_FIFO message absorption
 * - Digest extraction from STATE window
 * - Multi-block message handling
 * - Padding mechanism verification
 * - Endianness configuration testing
 * - OpenSSL cryptographic output validation against test vectors
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
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
 * @brief Helper function to configure CFG_SHADOWED for SHA3 mode
 * @param test Pointer to test harness
 * @param kstrength Keccak strength value (0x1=L224, 0x2=L256, 0x3=L384, 0x4=L512)
 *
 * Configures CFG_SHADOWED register with:
 * - mode = 0x0 (SHA3)
 * - kstrength = specified value
 * - kmac_en = 0 (plain hashing, not MAC)
 * - entropy_mode = 0x1 (edn_mode) - REQUIRED when EnMasking=1
 * - entropy_ready = 1 - REQUIRED when EnMasking=1
 *
 * Per KMAC spec: When model instantiated with EnMasking=true, all CFG_SHADOWED
 * writes must include entropy_mode=0x1 and entropy_ready=1 to prevent error
 * 0x09 (SwHashingWithoutEntropyReady).
 *
 * Performs shadow register duplicate write sequence for validation.
 */
static void configure_sha3_mode(kmac_test* test, uint32_t kstrength)
{
    // CFG_SHADOWED register format (ALL bits spec-compliant for EnMasking=1):
    // Bit [0] = kmac_en (0 for SHA3)
    // Bits [3:1] = kstrength
    // Bits [5:4] = mode (0x0 for SHA3)
    // Bits [17:16] = entropy_mode (0x1 = edn_mode)
    // Bit [24] = entropy_ready (1 = ready)
    uint32_t cfg_val = (0 << 0) |           // kmac_en=0 (SHA3 mode)
                       ((kstrength & 0x7) << 1) |  // kstrength
                       (0x0 << 4) |          // mode=0x0 (SHA3)
                       (0x1 << 16) |         // entropy_mode=0x1 (edn_mode) - REQUIRED
                       (0x1 << 24);          // entropy_ready=1 - REQUIRED

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
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
 * @brief Helper function to verify no error in ERR_CODE register
 * @param test Pointer to test harness
 * @return true if no error (ERR_CODE = 0), false otherwise
 */
static bool verify_no_error(kmac_test* test)
{
    uint32_t err_code = 0;
    test->register_read_32(test->ERR_CODE_OFFSET, err_code);
    if (err_code != 0) {
        CSML_INFO(2, test_logger) << "verify_no_error: ERR_CODE=0x" << std::hex << err_code << std::dec;
    }
    return (err_code == 0);
}

/**
 * @brief Helper function to verify UnexpectedModeStrength error
 * @param test Pointer to test harness
 * @param expected_mode Expected mode value in error code
 * @param expected_kstrength Expected kstrength value in error code
 * @return true if error code matches expected format
 */
static bool verify_unexpected_modestrength_error(kmac_test* test, uint32_t expected_mode, uint32_t expected_kstrength)
{
    uint32_t err_code = 0;
    test->register_read_32(test->ERR_CODE_OFFSET, err_code);

    // ERR_CODE format for UnexpectedModeStrength (0x06):
    // Bits [31:24] = 0x06 (error code)
    // Bits [23:16] = reserved/additional context
    // Bits [15:8]  = mode value
    // Bits [7:0]   = kstrength value
    uint32_t error_code_field = (err_code >> 24) & 0xFF;
    uint32_t mode_field = (err_code >> 8) & 0xFF;
    uint32_t kstrength_field = err_code & 0xFF;

    return (error_code_field == 0x06) &&
           (mode_field == expected_mode) &&
           (kstrength_field == expected_kstrength);
}

/**
 * @brief Helper function to clean up after test (return to IDLE state)
 * @param test Pointer to test harness
 *
 * Issues DONE command if not in IDLE, or err_processed if in error state.
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
 * TC-016: SHA3-224 Algorithm Selection Test
 *
 * Verifies OpenSSL EVP context initialization with EVP_sha3_224() when
 * CFG_SHADOWED is configured for SHA3 mode with L224 strength.
 ******************************************************************************/
void testbench::test_sha3_224_algorithm_selection()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-016: test_sha3_224_algorithm_selection");

    try {
        // Configure SHA3 mode with L224 strength (kstrength = 0x1)
        configure_sha3_mode(test, 0x1);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3, kstrength=L224";

        // Verify initial IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-016: test_sha3_224_algorithm_selection",
                            "Precondition: FSM not in IDLE state");
            return;
        }

        // Issue START command (0x1D) to trigger OpenSSL context initialization
        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        // Verify no UnexpectedModeStrength error
        if (!verify_no_error(test)) {
            uint32_t err_code = 0;
            test->register_read_32(test->ERR_CODE_OFFSET, err_code);
            cleanup_test(test);
            report_test_fail("TC-016: test_sha3_224_algorithm_selection",
                            "UnexpectedModeStrength error occurred");
            return;
        }

        // Verify FSM transitioned to ABSORB state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-016: test_sha3_224_algorithm_selection",
                            "FSM not in ABSORB state after START");
            return;
        }

        // Verify CFG_REGWEN.en auto-cleared to 0 (configuration locked)
        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            cleanup_test(test);
            report_test_fail("TC-016: test_sha3_224_algorithm_selection",
                            "CFG_REGWEN.en not auto-cleared after START");
            return;
        }

        // Clean up
        cleanup_test(test);
        report_test_pass("TC-016: test_sha3_224_algorithm_selection");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-016: test_sha3_224_algorithm_selection", e.what());
    }
}

/******************************************************************************
 * TC-017: SHA3-256 Algorithm Selection Test
 *
 * Verifies OpenSSL EVP context initialization with EVP_sha3_256() when
 * CFG_SHADOWED is configured for SHA3 mode with L256 strength.
 ******************************************************************************/
void testbench::test_sha3_256_algorithm_selection()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-017: test_sha3_256_algorithm_selection");

    try {
        configure_sha3_mode(test, 0x2);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3, kstrength=L256";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-017", "FSM not in IDLE state");
            return;
        }

        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-017", "Error occurred after START");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-017", "FSM not in ABSORB state");
            return;
        }

        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            cleanup_test(test);
            report_test_fail("TC-017", "CFG_REGWEN.en not locked");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-017: test_sha3_256_algorithm_selection");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-017", e.what());
    }
}

/******************************************************************************
 * TC-018: SHA3-384 Algorithm Selection Test
 *
 * Verifies OpenSSL EVP context initialization with EVP_sha3_384() when
 * CFG_SHADOWED is configured for SHA3 mode with L384 strength.
 ******************************************************************************/
void testbench::test_sha3_384_algorithm_selection()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-018: test_sha3_384_algorithm_selection");

    try {
        configure_sha3_mode(test, 0x3);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3, kstrength=L384";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-018", "FSM not in IDLE state");
            return;
        }

        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-018", "Error occurred after START");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-018", "FSM not in ABSORB state");
            return;
        }

        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            cleanup_test(test);
            report_test_fail("TC-018", "CFG_REGWEN.en not locked");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-018: test_sha3_384_algorithm_selection");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-018", e.what());
    }
}

/******************************************************************************
 * TC-019: SHA3-512 Algorithm Selection Test
 *
 * Verifies OpenSSL EVP context initialization with EVP_sha3_512() when
 * CFG_SHADOWED is configured for SHA3 mode with L512 strength.
 ******************************************************************************/
void testbench::test_sha3_512_algorithm_selection()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-019: test_sha3_512_algorithm_selection");

    try {
        configure_sha3_mode(test, 0x4);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3, kstrength=L512";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-019", "FSM not in IDLE state");
            return;
        }

        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-019", "Error occurred after START");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-019", "FSM not in ABSORB state");
            return;
        }

        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) != 0) {
            cleanup_test(test);
            report_test_fail("TC-019", "CFG_REGWEN.en not locked");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-019: test_sha3_512_algorithm_selection");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-019", e.what());
    }
}

/******************************************************************************
 * TC-028: SHA3 Invalid Strength L128 Test
 *
 * Verifies that UnexpectedModeStrength error (0x06) is generated when
 * SHA3 mode is configured with invalid kstrength = 0x0 (L128).
 * SHA3 only supports L224, L256, L384, L512.
 ******************************************************************************/
void testbench::test_sha3_invalid_strength_l128()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-028: test_sha3_invalid_strength_l128");

    try {
        configure_sha3_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3, kstrength=L128 (INVALID)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-028", "FSM not in IDLE state");
            return;
        }

        CSML_INFO(2, test_logger) << "Issuing START command (0x1D) - expecting error";
        write_cmd(test, 0x1D);

        if (!verify_unexpected_modestrength_error(test, 0x00, 0x00)) {
            cleanup_test(test);
            report_test_fail("TC-028", "UnexpectedModeStrength error not set");
            return;
        }

        CSML_INFO(2, test_logger) << "UnexpectedModeStrength error correctly detected";

        uint32_t intr_state = 0;
        test->register_read_32(test->INTR_STATE_OFFSET, intr_state);
        if ((intr_state & 0x4) == 0) {
            cleanup_test(test);
            report_test_fail("TC-028", "INTR_STATE.kmac_err not set");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (idle || absorb || squeeze) {
            cleanup_test(test);
            report_test_fail("TC-028", "FSM not in ERROR state");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-028: test_sha3_invalid_strength_l128");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-028", e.what());
    }
}

/******************************************************************************
 * Additional Test: SHA3 Configuration Validation
 *
 * Verifies CFG_SHADOWED configuration mechanism and shadow register duplicate
 * write validation for all valid SHA3 variants.
 ******************************************************************************/
void testbench::test_sha3_configuration_validation()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("test_sha3_configuration_validation");

    try {
        bool all_configs_valid = true;
        uint32_t valid_kstrengths[] = {0x1, 0x2, 0x3, 0x4};
        const char* strength_names[] = {"L224", "L256", "L384", "L512"};

        for (size_t i = 0; i < sizeof(valid_kstrengths) / sizeof(valid_kstrengths[0]); i++) {
            uint32_t kstrength = valid_kstrengths[i];
            CSML_INFO(2, test_logger) << "Testing SHA3 configuration with kstrength=" << strength_names[i];

            configure_sha3_mode(test, kstrength);

            uint32_t cfg_read = 0;
            test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_read);
            uint32_t kstrength_read = (cfg_read >> 1) & 0x7;
            uint32_t mode_read = (cfg_read >> 4) & 0x3;

            if (kstrength_read != kstrength || mode_read != 0x0) {
                CSML_ERROR(1, test_logger) << "[FAIL] Configuration readback mismatch for " << strength_names[i];
                all_configs_valid = false;
                continue;
            }

            write_cmd(test, 0x1D);

            if (!verify_no_error(test)) {
                CSML_ERROR(1, test_logger) << "[FAIL] Error during START for " << strength_names[i];
                all_configs_valid = false;
                cleanup_test(test);
                continue;
            }

            uint32_t cfg_regwen = 0;
            test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
            if ((cfg_regwen & 0x1) != 0) {
                CSML_ERROR(1, test_logger) << "[FAIL] CFG_REGWEN not locked for " << strength_names[i];
                all_configs_valid = false;
            }

            CSML_INFO(2, test_logger) << "  " << strength_names[i] << " configuration validated";

            cleanup_test(test);
            wait(10, SC_NS);
        }

        if (all_configs_valid) {
            report_test_pass("test_sha3_configuration_validation");
        } else {
            report_test_fail("test_sha3_configuration_validation", "Some configurations failed");
        }

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("test_sha3_configuration_validation", e.what());
    }
}

/******************************************************************************
 * Additional Test: SHA3 Reserved kstrength Error
 *
 * Verifies UnexpectedModeStrength error for reserved kstrength values
 * (0x5, 0x6, 0x7) in SHA3 mode.
 ******************************************************************************/
void testbench::test_sha3_reserved_kstrength_error()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("test_sha3_reserved_kstrength_error");

    try {
        bool all_errors_detected = true;
        uint32_t reserved_kstrengths[] = {0x5, 0x6, 0x7};

        for (size_t i = 0; i < sizeof(reserved_kstrengths) / sizeof(reserved_kstrengths[0]); i++) {
            uint32_t kstrength = reserved_kstrengths[i];
            CSML_INFO(2, test_logger) << "Testing reserved kstrength=0x" << std::hex << kstrength << std::dec;

            configure_sha3_mode(test, kstrength);
            write_cmd(test, 0x1D);

            if (!verify_unexpected_modestrength_error(test, 0x00, kstrength)) {
                CSML_ERROR(1, test_logger) << "[FAIL] Expected error not detected for kstrength=0x"
                                     << std::hex << kstrength << std::dec;
                all_errors_detected = false;
            } else {
                CSML_INFO(2, test_logger) << "  Error correctly detected for kstrength=0x"
                                    << std::hex << kstrength << std::dec;
            }

            write_cmd(test, 0x400);
            wait(10, SC_NS);

            bool idle, absorb, squeeze;
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!idle) {
                CSML_ERROR(1, test_logger) << "[FAIL] FSM not in IDLE after error recovery";
                all_errors_detected = false;
            }

            wait(10, SC_NS);
        }

        if (all_errors_detected) {
            report_test_pass("test_sha3_reserved_kstrength_error");
        } else {
            report_test_fail("test_sha3_reserved_kstrength_error", "Some errors not detected");
        }

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("test_sha3_reserved_kstrength_error", e.what());
    }
}

/******************************************************************************
 * Additional Test: Back-to-Back SHA3 Algorithm Switching
 *
 * Verifies multiple consecutive operations with different SHA3 algorithms.
 * Tests OpenSSL EVP context reinitialization.
 ******************************************************************************/
void testbench::test_sha3_back_to_back_algorithm_switching()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("test_sha3_back_to_back_algorithm_switching");

    try {
        // Test sequence: SHA3-256 → SHA3-512 → SHA3-224 → SHA3-384
        uint32_t test_sequence[] = {0x2, 0x4, 0x1, 0x3};
        const char* algo_names[] = {"SHA3-256", "SHA3-512", "SHA3-224", "SHA3-384"};

        for (size_t i = 0; i < sizeof(test_sequence) / sizeof(test_sequence[0]); i++) {
            uint32_t kstrength = test_sequence[i];
            CSML_INFO(2, test_logger) << "Operation " << (i+1) << ": Testing " << algo_names[i];

            // Configure
            configure_sha3_mode(test, kstrength);

            // START
            write_cmd(test, 0x1D);

            // Verify success
            if (!verify_no_error(test)) {
                cleanup_test(test);
                std::string reason = "Error during ";
                reason += algo_names[i];
                report_test_fail("test_sha3_back_to_back_algorithm_switching", reason);
                return;
            }

            // Verify ABSORB state
            bool idle, absorb, squeeze;
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!absorb) {
                cleanup_test(test);
                std::string reason = "Not in ABSORB state for ";
                reason += algo_names[i];
                report_test_fail("test_sha3_back_to_back_algorithm_switching", reason);
                return;
            }

            // Return to IDLE for next operation
            write_cmd(test, 0x16); // DONE
            wait(10, SC_NS);

            // Verify back in IDLE
            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!idle) {
                cleanup_test(test);
                report_test_fail("test_sha3_back_to_back_algorithm_switching",
                                    "Not in IDLE state after DONE command");
                return;
            }

            // Verify CFG_REGWEN unlocked
            uint32_t cfg_regwen = 0;
            test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
            if ((cfg_regwen & 0x1) == 0) {
                cleanup_test(test);
                report_test_fail("test_sha3_back_to_back_algorithm_switching",
                                    "CFG_REGWEN not unlocked after DONE");
                return;
            }

            CSML_INFO(2, test_logger) << "  " << algo_names[i] << " operation successful";
            wait(10, SC_NS);
        }

        // All operations successful
        cleanup_test(test);
        CSML_INFO(2, test_logger) << "  - 4 consecutive operations completed";
        CSML_INFO(2, test_logger) << "  - OpenSSL context reinitialization works";
        CSML_INFO(2, test_logger) << "  - FSM cycling correct";
        CSML_INFO(2, test_logger) << "  - CFG_REGWEN protection cycles correctly";
        report_test_pass("test_sha3_back_to_back_algorithm_switching");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("test_sha3_back_to_back_algorithm_switching", e.what());
    }
}

// End of file
