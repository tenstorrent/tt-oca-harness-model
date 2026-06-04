/******************************************************************************
 * @file kmac_func002_test.cpp
 * @brief Test cases for FUNC-KMAC-002 (SHAKE Extendable Output Function)
 *
 * This file implements test cases for FUNC-KMAC-002, verifying SHAKE XOF
 * (Extendable Output Function) mode operation supporting SHAKE128 and SHAKE256
 * algorithms with arbitrary output lengths.
 *
 * FUNC-KMAC-002 Test Coverage:
 * - SHAKE128 and SHAKE256 algorithm selection and initialization
 * - Fixed-length output generation (single PROCESS command)
 * - Extended output generation via RUN command (0x31) and EVP_DigestFinalXOF
 * - SHAKE padding mechanism (4-bit '1111' followed by pad10*1)
 * - RUN command execution in SQUEEZE state performing 24 Keccak rounds
 * - UnexpectedModeStrength error detection for invalid kstrength values
 * - Multiple RUN commands for extended output sequences
 * - FSM state validation for SHAKE operations
 * - OpenSSL EVP_shake128/EVP_shake256 delegation
 *
 * SHAKE Mode Configuration:
 * - CFG_SHADOWED.mode = 0x2 (SHAKE)
 * - CFG_SHADOWED.kstrength = 0x0 (SHAKE128, rate=168 bytes) or 0x2 (SHAKE256, rate=136 bytes)
 * - CFG_SHADOWED.kmac_en = 0
 * - Invalid kstrength values: 0x1 (L224), 0x3 (L384), 0x4 (L512) trigger UnexpectedModeStrength
 *
 * Command Sequence:
 * - START (0x1D): IDLE → ABSORB
 * - MSG_FIFO writes: Absorb message data
 * - PROCESS (0x2E): ABSORB → SQUEEZE (first output block available)
 * - RUN (0x31): Remain in SQUEEZE, generate next output block (EVP_DigestFinalXOF)
 * - DONE (0x16): SQUEEZE → IDLE
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Functionality Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md
 * Architecture Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-architecture-behaviour-map.json
 * Detailed Design: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-detailed-design.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "kmac_test.h"
#include "testbench.h"
#include "csml_logger.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <openssl/evp.h>

// Logger for test output
static CsmlLogger test_logger;

/******************************************************************************
 * Helper Functions for SHAKE Testing
 ******************************************************************************/

/**
 * @brief Helper function to configure CFG_SHADOWED for SHAKE mode
 * @param test Pointer to test harness
 * @param kstrength Keccak strength value (0x0=SHAKE128, 0x2=SHAKE256)
 *
 * Configures CFG_SHADOWED register with:
 * - mode = 0x2 (SHAKE)
 * - kstrength = specified value (0x0 or 0x2 for valid SHAKE)
 * - kmac_en = 0 (plain hashing, not MAC)
 * - All other fields = 0 (default values)
 *
 * Performs shadow register duplicate write sequence for validation.
 */
static void configure_shake_mode(kmac_test* test, uint32_t kstrength)
{
    // CFG_SHADOWED register format (spec-compliant for EnMasking=1):
    // Bit [0] = kmac_en (0 for SHAKE)
    // Bits [3:1] = kstrength
    // Bits [5:4] = mode (0x2 for SHAKE)
    // Bits [17:16] = entropy_mode (0x1 = edn_mode) - REQUIRED
    // Bit [24] = entropy_ready (1 = ready) - REQUIRED
    uint32_t cfg_val = (0 << 0) |                // kmac_en=0
                       ((kstrength & 0x7) << 1) | // kstrength
                       (0x2 << 4) |               // mode=0x2 (SHAKE)
                       (0x1 << 16) |              // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);               // entropy_ready=1 - REQUIRED

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
 * @brief Helper function to write message data to MSG_FIFO
 * @param test Pointer to test harness
 * @param data Pointer to message data buffer
 * @param len_bytes Length of message data in bytes
 *
 * Writes message data byte-by-byte to avoid zero-padding partial words.
 * This ensures only actual message bytes are absorbed, matching OpenSSL reference behavior.
 */
static void write_msg_fifo(kmac_test* test, const uint8_t* data, size_t len_bytes)
{
    // MSG_FIFO window starts at offset 0x800
    const uint32_t MSG_FIFO_BASE = 0x800;

    // Write all bytes individually to avoid zero-padding issues
    for (size_t i = 0; i < len_bytes; i++) {
        test->register_write_8(MSG_FIFO_BASE + i, data[i]);
        wait(2, SC_NS); // Small delay between writes
    }
}

/**
 * @brief Helper function to read digest from STATE window
 * @param test Pointer to test harness
 * @param digest Output buffer for digest data
 * @param digest_len_bytes Length of digest to read in bytes
 *
 * When EnMasking=1 (testbench default), the digest is masked as two XOR shares:
 * - share0 region: 0x400-0x4FF (random bytes)
 * - share1 region: 0x500-0x5FF (digest XOR random)
 * This function reads both shares and XORs them to reconstruct the actual digest.
 */
static void read_state_digest(kmac_test* test, uint8_t* digest, size_t digest_len_bytes)
{
    // STATE window share regions
    const uint32_t STATE_SHARE0_BASE = 0x400;
    const uint32_t STATE_SHARE1_BASE = 0x500;

    // Read digest in 32-bit words from both share regions
    size_t word_count = (digest_len_bytes + 3) / 4;
    for (size_t i = 0; i < word_count; i++) {
        uint32_t share0_word = 0;
        uint32_t share1_word = 0;

        // Read from share0 region
        test->register_read_32(STATE_SHARE0_BASE + i * 4, share0_word);

        // Read from share1 region
        test->register_read_32(STATE_SHARE1_BASE + i * 4, share1_word);

        // XOR shares to reconstruct digest: digest = share0 XOR share1
        uint32_t digest_word = share0_word ^ share1_word;

        // Unpack word into bytes (little-endian)
        size_t bytes_this_word = (i == word_count - 1) ? (digest_len_bytes - i * 4) : 4;
        for (size_t j = 0; j < bytes_this_word; j++) {
            digest[i * 4 + j] = (digest_word >> (j * 8)) & 0xFF;
        }
    }
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
            // Error recovery: issue err_processed
            write_cmd(test, 0x400); // err_processed bit (bit 10)
        } else {
            // Normal cleanup: issue DONE command
            write_cmd(test, 0x16); // DONE command
        }
        wait(10, SC_NS);
    }
}

/**
 * @brief Helper function to compute reference SHAKE128 output using OpenSSL
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param output_len Desired output length in bytes
 * @return true if computation successful, false otherwise
 *
 * NOTE: This function calls EVP_DigestFinalXOF ONCE with the total output length.
 * This is appropriate for tests that read digest in a single operation (e.g., fixed output tests).
 * For extended output tests that use RUN command, use compute_shake128_reference_blocks() instead.
 */
static bool compute_shake128_reference(const uint8_t* message, size_t msg_len,
                                        uint8_t* output, size_t output_len)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake128(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestFinalXOF(ctx, output, output_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compute reference SHAKE128 output in multiple blocks using OpenSSL
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param block_sizes Array of block sizes to generate
 * @param num_blocks Number of blocks to generate
 * @return true if computation successful, false otherwise
 *
 * This function matches hardware behavior where PROCESS generates the first block,
 * and each RUN command generates a subsequent block. It calls EVP_DigestFinalXOF
 * multiple times on the same context to generate successive output blocks.
 *
 * Example: For SHAKE128 extended output (168 + 100 bytes):
 *   block_sizes[] = {168, 100}
 *   num_blocks = 2
 * This will call EVP_DigestFinalXOF(ctx, output, 168) then EVP_DigestFinalXOF(ctx, output+168, 100)
 */
static bool compute_shake128_reference_blocks(const uint8_t* message, size_t msg_len,
                                               uint8_t* output, const size_t* block_sizes, size_t num_blocks)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake128(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Generate output in multiple blocks (successive calls to EVP_DigestFinalXOF)
    size_t offset = 0;
    for (size_t i = 0; i < num_blocks; i++) {
        if (EVP_DigestFinalXOF(ctx, output + offset, block_sizes[i]) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
        offset += block_sizes[i];
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compute reference SHAKE256 output using OpenSSL
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param output_len Desired output length in bytes
 * @return true if computation successful, false otherwise
 *
 * NOTE: This function calls EVP_DigestFinalXOF ONCE with the total output length.
 * This is appropriate for tests that read digest in a single operation (e.g., fixed output tests).
 * For extended output tests that use RUN command, use compute_shake256_reference_blocks() instead.
 */
static bool compute_shake256_reference(const uint8_t* message, size_t msg_len,
                                        uint8_t* output, size_t output_len)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestFinalXOF(ctx, output, output_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compute reference SHAKE256 output in multiple blocks using OpenSSL
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param block_sizes Array of block sizes to generate
 * @param num_blocks Number of blocks to generate
 * @return true if computation successful, false otherwise
 *
 * This function matches hardware behavior where PROCESS generates the first block,
 * and each RUN command generates a subsequent block. It calls EVP_DigestFinalXOF
 * multiple times on the same context to generate successive output blocks.
 *
 * Example: For SHAKE256 extended output (136 + 100 bytes):
 *   block_sizes[] = {136, 100}
 *   num_blocks = 2
 * This will call EVP_DigestFinalXOF(ctx, output, 136) then EVP_DigestFinalXOF(ctx, output+136, 100)
 */
static bool compute_shake256_reference_blocks(const uint8_t* message, size_t msg_len,
                                               uint8_t* output, const size_t* block_sizes, size_t num_blocks)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Generate output in multiple blocks (successive calls to EVP_DigestFinalXOF)
    size_t offset = 0;
    for (size_t i = 0; i < num_blocks; i++) {
        if (EVP_DigestFinalXOF(ctx, output + offset, block_sizes[i]) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
        offset += block_sizes[i];
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compare two byte buffers
 * @param expected Expected buffer
 * @param actual Actual buffer
 * @param len Length to compare in bytes
 * @return true if buffers match, false otherwise
 */
static bool compare_buffers(const uint8_t* expected, const uint8_t* actual, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            CSML_ERROR(1, test_logger) << "Buffer mismatch at byte " << i
                                << ": expected=0x" << std::hex << (int)expected[i]
                                << ", actual=0x" << (int)actual[i] << std::dec;
            return false;
        }
    }
    return true;
}

/******************************************************************************
 * TC-029: SHAKE128 Fixed Output Test
 *
 * Verifies SHAKE128 operation with fixed 256-bit (32-byte) output.
 * - Configure SHAKE128 mode (mode=0x2, kstrength=0x0)
 * - Absorb test message
 * - Issue PROCESS command to generate digest
 * - Read STATE window for 32-byte output
 * - Validate against OpenSSL EVP_shake128 reference
 ******************************************************************************/
void testbench::test_shake128_fixed_output()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-029: test_shake128_fixed_output");

    try {
        // Test message: "SHAKE128 test message"
        const char* test_msg = "SHAKE128 test message";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 32; // 256 bits

        // Configure SHAKE128 mode (kstrength = 0x0)
        configure_shake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHAKE, kstrength=L128";

        // Verify initial IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-029", "Precondition: FSM not in IDLE state");
            return;
        }

        // Issue START command
        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-029", "Error after START command");
            return;
        }

        // Verify FSM transitioned to ABSORB state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-029", "FSM not in ABSORB state after START");
            return;
        }

        // Write message to MSG_FIFO
        CSML_INFO(2, test_logger) << "Writing message (" << msg_len << " bytes) to MSG_FIFO";
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        // Issue PROCESS command
        CSML_INFO(2, test_logger) << "Issuing PROCESS command (0x2E)";
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-029", "Error after PROCESS command");
            return;
        }

        // Verify FSM transitioned to SQUEEZE state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-029", "FSM not in SQUEEZE state after PROCESS");
            return;
        }

        // Read digest from STATE window
        uint8_t actual_digest[32];
        CSML_INFO(2, test_logger) << "Reading " << output_len << "-byte digest from STATE window";
        read_state_digest(test, actual_digest, output_len);

        // Compute reference digest using OpenSSL
        uint8_t expected_digest[32];
        if (!compute_shake128_reference((const uint8_t*)test_msg, msg_len,
                                        expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-029", "OpenSSL reference computation failed");
            return;
        }

        // Compare digests
        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-029", "Digest mismatch with OpenSSL reference");
            return;
        }

        CSML_INFO(2, test_logger) << "SHAKE128 digest matches OpenSSL reference";

        // Clean up
        cleanup_test(test);
        report_test_pass("TC-029: test_shake128_fixed_output");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-029", e.what());
    }
}

/******************************************************************************
 * TC-030: SHAKE256 Fixed Output Test
 *
 * Verifies SHAKE256 operation with fixed 512-bit (64-byte) output.
 * - Configure SHAKE256 mode (mode=0x2, kstrength=0x2)
 * - Absorb test message
 * - Issue PROCESS command to generate digest
 * - Read STATE window for 64-byte output
 * - Validate against OpenSSL EVP_shake256 reference
 ******************************************************************************/
void testbench::test_shake256_fixed_output()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-030: test_shake256_fixed_output");

    try {
        const char* test_msg = "SHAKE256 test message";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 64; // 512 bits

        configure_shake_mode(test, 0x2);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHAKE, kstrength=L256";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-030", "FSM not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-030", "Error after START");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-030", "Not in ABSORB state");
            return;
        }

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-030", "Error after PROCESS");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-030", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_digest[64];
        read_state_digest(test, actual_digest, output_len);

        uint8_t expected_digest[64];
        if (!compute_shake256_reference((const uint8_t*)test_msg, msg_len,
                                        expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-030", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-030", "Digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "SHAKE256 digest matches OpenSSL reference";

        cleanup_test(test);
        report_test_pass("TC-030: test_shake256_fixed_output");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-030", e.what());
    }
}

/******************************************************************************
 * TC-031: SHAKE128 Extended Output Test
 *
 * Verifies SHAKE128 extended output using RUN command for multiple blocks.
 * - Configure SHAKE128 mode
 * - Absorb message and issue PROCESS
 * - Read first block from STATE (168 bytes max for SHAKE128 rate)
 * - Issue RUN command (0x31) for next block
 * - Verify FSM remains in SQUEEZE state after RUN
 * - Read second block from STATE
 * - Validate extended output against OpenSSL reference
 ******************************************************************************/
void testbench::test_shake128_extended_output()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-031: test_shake128_extended_output");

    try {
        const char* test_msg = "SHAKE128 extended output test";
        const size_t msg_len = strlen(test_msg);
        const size_t block1_len = 168; // SHAKE128 rate
        const size_t block2_len = 100; // Additional extended output
        const size_t total_output = block1_len + block2_len;

        configure_shake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Configured SHAKE128 for extended output";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-031", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-031", "Error after START");
            return;
        }

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-031", "Error after PROCESS");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-031", "Not in SQUEEZE state");
            return;
        }

        // Read first block (up to rate size)
        uint8_t actual_output[300];
        CSML_INFO(2, test_logger) << "Reading first block (" << block1_len << " bytes)";
        read_state_digest(test, actual_output, block1_len);

        // Issue RUN command for extended output
        CSML_INFO(2, test_logger) << "Issuing RUN command (0x31) for extended output";
        write_cmd(test, 0x31);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-031", "Error after RUN command");
            return;
        }

        // Verify FSM still in SQUEEZE state after RUN
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-031", "FSM not in SQUEEZE state after RUN");
            return;
        }

        // Read second block
        CSML_INFO(2, test_logger) << "Reading second block (" << block2_len << " bytes)";
        read_state_digest(test, actual_output + block1_len, block2_len);

        // Compute reference extended output using OpenSSL (block-by-block to match hardware)
        // Hardware: PROCESS generates first block (168 bytes), RUN generates second block (100 bytes)
        uint8_t expected_output[300];
        size_t block_sizes[] = {block1_len, block2_len};
        if (!compute_shake128_reference_blocks((const uint8_t*)test_msg, msg_len,
                                                expected_output, block_sizes, 2)) {
            cleanup_test(test);
            report_test_fail("TC-031", "OpenSSL reference failed");
            return;
        }

        // Compare extended output
        if (!compare_buffers(expected_output, actual_output, total_output)) {
            cleanup_test(test);
            report_test_fail("TC-031", "Extended output mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "SHAKE128 extended output validated: " << total_output << " bytes";

        cleanup_test(test);
        report_test_pass("TC-031: test_shake128_extended_output");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-031", e.what());
    }
}

/******************************************************************************
 * TC-032: SHAKE256 Extended Output Test
 *
 * Verifies SHAKE256 extended output using RUN command for multiple blocks.
 * - Configure SHAKE256 mode
 * - Absorb message and issue PROCESS
 * - Read first block from STATE (136 bytes max for SHAKE256 rate)
 * - Issue RUN command for next block
 * - Read second block from STATE
 * - Validate extended output against OpenSSL reference
 ******************************************************************************/
void testbench::test_shake256_extended_output()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-032: test_shake256_extended_output");

    try {
        const char* test_msg = "SHAKE256 extended output test";
        const size_t msg_len = strlen(test_msg);
        const size_t block1_len = 136; // SHAKE256 rate
        const size_t block2_len = 100;
        const size_t total_output = block1_len + block2_len;

        configure_shake_mode(test, 0x2);
        CSML_INFO(2, test_logger) << "Configured SHAKE256 for extended output";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-032", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-032", "Error during operation");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-032", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_output[250];
        read_state_digest(test, actual_output, block1_len);

        write_cmd(test, 0x31); // RUN command

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-032", "Error after RUN");
            return;
        }

        read_state_digest(test, actual_output + block1_len, block2_len);

        // Compute reference extended output using OpenSSL (block-by-block to match hardware)
        // Hardware: PROCESS generates first block (136 bytes), RUN generates second block (100 bytes)
        uint8_t expected_output[250];
        size_t block_sizes[] = {block1_len, block2_len};
        if (!compute_shake256_reference_blocks((const uint8_t*)test_msg, msg_len,
                                                expected_output, block_sizes, 2)) {
            cleanup_test(test);
            report_test_fail("TC-032", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_output, actual_output, total_output)) {
            cleanup_test(test);
            report_test_fail("TC-032", "Extended output mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "SHAKE256 extended output validated: " << total_output << " bytes";

        cleanup_test(test);
        report_test_pass("TC-032: test_shake256_extended_output");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-032", e.what());
    }
}

/******************************************************************************
 * TC-033: SHAKE Padding Mechanism Test
 *
 * Verifies SHAKE padding pattern (4-bit '1111' followed by pad10*1).
 * Tests that SHAKE padding differs from SHA3 padding (2-bit '10').
 * - Configure SHAKE128 mode
 * - Use message length that requires padding
 * - Verify digest matches OpenSSL (which implements correct padding)
 ******************************************************************************/
void testbench::test_shake_padding_mechanism()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-033: test_shake_padding_mechanism");

    try {
        // Use a message length that exercises padding logic
        // 167 bytes is one byte short of SHAKE128 rate (168 bytes)
        const size_t msg_len = 167;
        uint8_t test_msg[167];
        for (size_t i = 0; i < msg_len; i++) {
            test_msg[i] = (uint8_t)(i & 0xFF);
        }

        const size_t output_len = 32;

        configure_shake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Testing SHAKE padding with " << msg_len << "-byte message";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-033", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-033", "Error during operation");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-033", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_digest[32];
        read_state_digest(test, actual_digest, output_len);

        uint8_t expected_digest[32];
        if (!compute_shake128_reference(test_msg, msg_len, expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-033", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-033", "Padding validation failed - digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "SHAKE padding mechanism validated (4-bit '1111' + pad10*1)";

        cleanup_test(test);
        report_test_pass("TC-033: test_shake_padding_mechanism");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-033", e.what());
    }
}

/******************************************************************************
 * TC-034: SHAKE RUN Command in SQUEEZE State Test
 *
 * Verifies RUN command (0x31) executes 24 Keccak rounds in SQUEEZE state.
 * - Configure SHAKE128 mode
 * - Reach SQUEEZE state via PROCESS
 * - Verify sha3_squeeze = 1
 * - Issue RUN command
 * - Verify FSM remains in SQUEEZE state
 * - Verify STATE window content changes after RUN
 ******************************************************************************/
void testbench::test_shake_run_command_squeeze_state()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-034: test_shake_run_command_squeeze_state");

    try {
        const char* test_msg = "RUN command test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 64;

        configure_shake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Testing RUN command in SQUEEZE state";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-034", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        // Verify SQUEEZE state after PROCESS
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-034", "Not in SQUEEZE state after PROCESS");
            return;
        }
        CSML_INFO(2, test_logger) << "STATUS.sha3_squeeze = 1 after PROCESS";

        // Read first digest
        uint8_t digest_before_run[64];
        read_state_digest(test, digest_before_run, output_len);

        // Issue RUN command
        CSML_INFO(2, test_logger) << "Issuing RUN command (0x31)";
        write_cmd(test, 0x31);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-034", "Error after RUN command");
            return;
        }

        // Verify still in SQUEEZE state after RUN
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-034", "Not in SQUEEZE state after RUN");
            return;
        }
        CSML_INFO(2, test_logger) << "STATUS.sha3_squeeze = 1 after RUN (FSM remains in SQUEEZE)";

        // Read digest after RUN (should be different - next block)
        uint8_t digest_after_run[64];
        read_state_digest(test, digest_after_run, output_len);

        // Verify STATE content changed after RUN
        bool state_changed = false;
        for (size_t i = 0; i < output_len; i++) {
            if (digest_before_run[i] != digest_after_run[i]) {
                state_changed = true;
                break;
            }
        }

        if (!state_changed) {
            cleanup_test(test);
            report_test_fail("TC-034", "STATE content unchanged after RUN command");
            return;
        }

        CSML_INFO(2, test_logger) << "STATE window updated after RUN (24 Keccak rounds executed)";

        cleanup_test(test);
        report_test_pass("TC-034: test_shake_run_command_squeeze_state");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-034", e.what());
    }
}

/******************************************************************************
 * TC-035: SHAKE Invalid Strength L224 Test
 *
 * Verifies UnexpectedModeStrength error when SHAKE mode configured with
 * invalid kstrength = 0x1 (L224). SHAKE only supports L128 and L256.
 ******************************************************************************/
void testbench::test_shake_invalid_strength_l224()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-035: test_shake_invalid_strength_l224");

    try {
        configure_shake_mode(test, 0x1); // L224 - INVALID for SHAKE
        CSML_INFO(2, test_logger) << "Configured SHAKE with kstrength=L224 (INVALID)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-035", "Not in IDLE state");
            return;
        }

        CSML_INFO(2, test_logger) << "Issuing START command - expecting error";
        write_cmd(test, 0x1D);

        // Verify UnexpectedModeStrength error (mode=0x2, kstrength=0x1)
        if (!verify_unexpected_modestrength_error(test, 0x02, 0x01)) {
            cleanup_test(test);
            report_test_fail("TC-035", "UnexpectedModeStrength error not set");
            return;
        }

        CSML_INFO(2, test_logger) << "UnexpectedModeStrength error correctly detected";

        // Verify kmac_err interrupt set
        uint32_t intr_state = 0;
        test->register_read_32(test->INTR_STATE_OFFSET, intr_state);
        if ((intr_state & 0x4) == 0) {
            cleanup_test(test);
            report_test_fail("TC-035", "INTR_STATE.kmac_err not set");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-035: test_shake_invalid_strength_l224");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-035", e.what());
    }
}

/******************************************************************************
 * TC-036: SHAKE Invalid Strength L384 Test
 *
 * Verifies UnexpectedModeStrength error when SHAKE mode configured with
 * invalid kstrength = 0x3 (L384).
 ******************************************************************************/
void testbench::test_shake_invalid_strength_l384()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-036: test_shake_invalid_strength_l384");

    try {
        configure_shake_mode(test, 0x3); // L384 - INVALID for SHAKE
        CSML_INFO(2, test_logger) << "Configured SHAKE with kstrength=L384 (INVALID)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-036", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);

        if (!verify_unexpected_modestrength_error(test, 0x02, 0x03)) {
            cleanup_test(test);
            report_test_fail("TC-036", "UnexpectedModeStrength error not set");
            return;
        }

        CSML_INFO(2, test_logger) << "UnexpectedModeStrength error correctly detected for L384";

        cleanup_test(test);
        report_test_pass("TC-036: test_shake_invalid_strength_l384");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-036", e.what());
    }
}

/******************************************************************************
 * TC-037: SHAKE Invalid Strength L512 Test
 *
 * Verifies UnexpectedModeStrength error when SHAKE mode configured with
 * invalid kstrength = 0x4 (L512).
 ******************************************************************************/
void testbench::test_shake_invalid_strength_l512()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-037: test_shake_invalid_strength_l512");

    try {
        configure_shake_mode(test, 0x4); // L512 - INVALID for SHAKE
        CSML_INFO(2, test_logger) << "Configured SHAKE with kstrength=L512 (INVALID)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-037", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);

        if (!verify_unexpected_modestrength_error(test, 0x02, 0x04)) {
            cleanup_test(test);
            report_test_fail("TC-037", "UnexpectedModeStrength error not set");
            return;
        }

        CSML_INFO(2, test_logger) << "UnexpectedModeStrength error correctly detected for L512";

        cleanup_test(test);
        report_test_pass("TC-037: test_shake_invalid_strength_l512");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-037", e.what());
    }
}

/******************************************************************************
 * TC-182: Corner Case - Extended Output Many RUNs Test
 *
 * Verifies SHAKE extended output requiring many RUN commands (10 iterations).
 * Tests that:
 * - Multiple consecutive RUN commands work correctly
 * - Each RUN produces the next sequential output block
 * - FSM remains stable in SQUEEZE state
 * - Total extended output matches OpenSSL reference
 ******************************************************************************/
void testbench::test_corner_extended_output_many_runs()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-182: test_corner_extended_output_many_runs");

    try {
        const char* test_msg = "Many RUNs test message";
        const size_t msg_len = strlen(test_msg);
        const size_t rate = 168; // SHAKE128 rate
        const size_t num_runs = 10;
        const size_t total_output = rate * num_runs; // 1680 bytes

        configure_shake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Testing extended output with " << num_runs << " RUN commands";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-182", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-182", "Not in SQUEEZE state");
            return;
        }

        // Allocate buffer for extended output
        uint8_t* actual_output = new uint8_t[total_output];
        uint8_t* expected_output = new uint8_t[total_output];

        // Read first block (after PROCESS)
        read_state_digest(test, actual_output, rate);
        CSML_INFO(2, test_logger) << "Read block 0: " << rate << " bytes";

        // Issue RUN commands and read subsequent blocks
        for (size_t i = 1; i < num_runs; i++) {
            write_cmd(test, 0x31); // RUN command

            if (!verify_no_error(test)) {
                delete[] actual_output;
                delete[] expected_output;
                cleanup_test(test);
                report_test_fail("TC-182", "Error after RUN command iteration");
                return;
            }

            read_status_fsm_bits(test, idle, absorb, squeeze);
            if (!squeeze) {
                delete[] actual_output;
                delete[] expected_output;
                cleanup_test(test);
                report_test_fail("TC-182", "FSM left SQUEEZE state");
                return;
            }

            read_state_digest(test, actual_output + i * rate, rate);
            CSML_INFO(2, test_logger) << "Read block " << i << ": " << rate << " bytes";
        }

        // Compute reference extended output using OpenSSL (block-by-block to match hardware)
        // Hardware: PROCESS generates first block (168 bytes), then 9 RUN commands generate 9 more blocks
        // Create array of block sizes (all blocks are rate-sized for this test)
        size_t* block_sizes = new size_t[num_runs];
        for (size_t i = 0; i < num_runs; i++) {
            block_sizes[i] = rate;
        }

        if (!compute_shake128_reference_blocks((const uint8_t*)test_msg, msg_len,
                                                expected_output, block_sizes, num_runs)) {
            delete[] block_sizes;
            delete[] actual_output;
            delete[] expected_output;
            cleanup_test(test);
            report_test_fail("TC-182", "OpenSSL reference failed");
            return;
        }
        delete[] block_sizes;

        // Compare all blocks
        if (!compare_buffers(expected_output, actual_output, total_output)) {
            delete[] actual_output;
            delete[] expected_output;
            cleanup_test(test);
            report_test_fail("TC-182", "Extended output mismatch");
            return;
        }

        delete[] actual_output;
        delete[] expected_output;

        CSML_INFO(2, test_logger) << "Extended output validated: " << num_runs
                            << " RUN commands, " << total_output << " bytes total";

        cleanup_test(test);
        report_test_pass("TC-182: test_corner_extended_output_many_runs");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-182", e.what());
    }
}

// End of file
