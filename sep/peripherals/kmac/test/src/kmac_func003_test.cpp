/******************************************************************************
 * @file kmac_func003_test.cpp
 * @brief Test cases for FUNC-KMAC-003 (cSHAKE Customizable Hash Function)
 *
 * This file implements test cases for FUNC-KMAC-003, verifying cSHAKE
 * (Customizable SHAKE) mode operation supporting function name (N) and
 * customization string (S) prefix for domain separation per NIST SP 800-185.
 *
 * FUNC-KMAC-003 Test Coverage:
 * - cSHAKE128 and cSHAKE256 algorithm selection and initialization
 * - Empty N and S (functionally equivalent to SHAKE)
 * - Non-empty function name N and customization string S
 * - PREFIX register encoding (encode_string format per NIST SP 800-185)
 * - PREFIX expansion to full block size (168 bytes for cSHAKE128, 136 for cSHAKE256)
 * - PREFIX absorption in START command before message processing
 * - cSHAKE padding mechanism (2-bit '00' followed by pad10*1)
 * - Extended output using RUN command
 * - OpenSSL EVP_shake128/256 delegation with PREFIX prepended
 *
 * cSHAKE Mode Configuration:
 * - CFG_SHADOWED.mode = 0x3 (cSHAKE)
 * - CFG_SHADOWED.kstrength = 0x0 (cSHAKE128, rate=168 bytes) or 0x2 (cSHAKE256, rate=136 bytes)
 * - CFG_SHADOWED.kmac_en = 0 (plain hashing, not MAC)
 * - PREFIX_0 through PREFIX_10: Store encode_string(N) || encode_string(S)
 *
 * PREFIX Register Format:
 * - Total storage: 320 bits (40 bytes) across eleven 32-bit registers
 * - Encoding per NIST SP 800-185 Section 2.3.2:
 *   encode_string(X) = left_encode(len(X)) || X
 *   left_encode(x) = o || enc8(x) where o = enc8(len(enc8(x)))
 * - Example: encode_string("Test") = 0x01 0x20 "Test" (1 byte length, 32-bit length encoding)
 * - Empty string: encode_string("") = 0x01 0x00
 *
 * Command Sequence:
 * - Configure PREFIX registers (protected by CFG_REGWEN)
 * - START (0x1D): IDLE → ABSORB (PREFIX expansion and absorption occurs)
 * - MSG_FIFO writes: Absorb message data
 * - PROCESS (0x2E): ABSORB → SQUEEZE (first output block available)
 * - RUN (0x31): Remain in SQUEEZE, generate next output block
 * - DONE (0x16): SQUEEZE → IDLE
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Test Case Mapping: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md (TC-038 to TC-044)
 * Architecture Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-architecture-behaviour-map.json
 * Detailed Design: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-detailed-design.md
 * Functionality: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality_list.md (FUNC-KMAC-003)
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
 * Helper Functions for cSHAKE Testing
 ******************************************************************************/

/**
 * @brief Helper function to configure CFG_SHADOWED for cSHAKE mode
 * @param test Pointer to test harness
 * @param kstrength Keccak strength value (0x0=cSHAKE128, 0x2=cSHAKE256)
 *
 * Configures CFG_SHADOWED register with:
 * - mode = 0x3 (cSHAKE)
 * - kstrength = specified value (0x0 or 0x2 for valid cSHAKE)
 * - kmac_en = 0 (plain hashing, not MAC)
 * - All other fields = 0 (default values)
 *
 * Performs shadow register duplicate write sequence for validation.
 */
static void configure_cshake_mode(kmac_test* test, uint32_t kstrength)
{
    // CFG_SHADOWED register format (spec-compliant for EnMasking=1):
    // Bit [0] = kmac_en (0 for cSHAKE)
    // Bits [3:1] = kstrength
    // Bits [5:4] = mode (0x3 for cSHAKE)
    // Bits [17:16] = entropy_mode (0x1 = edn_mode) - REQUIRED
    // Bit [24] = entropy_ready (1 = ready) - REQUIRED
    uint32_t cfg_val = (0 << 0) |                // kmac_en=0
                       ((kstrength & 0x7) << 1) | // kstrength
                       (0x3 << 4) |               // mode=0x3 (cSHAKE)
                       (0x1 << 16) |              // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);               // entropy_ready=1 - REQUIRED

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to write PREFIX registers
 * @param test Pointer to test harness
 * @param prefix_data Pointer to prefix data buffer (up to 40 bytes)
 * @param prefix_len_bytes Length of prefix data in bytes (max 40)
 *
 * Writes PREFIX_0 through PREFIX_10 registers with provided data.
 * PREFIX registers are protected by CFG_REGWEN and must be written in IDLE state.
 * Each PREFIX register is 32 bits (4 bytes).
 */
static void write_prefix_registers(kmac_test* test, const uint8_t* prefix_data, size_t prefix_len_bytes)
{
    const uint32_t PREFIX_BASE = test->PREFIX_0_OFFSET;
    const size_t MAX_PREFIX_BYTES = 40; // 11 registers * 4 bytes/register = 44, but spec limits to 40

    if (prefix_len_bytes > MAX_PREFIX_BYTES) {
        prefix_len_bytes = MAX_PREFIX_BYTES;
    }

    // Clear all PREFIX registers first (zero-pad)
    for (size_t i = 0; i < 11; i++) {
        test->register_write_32(PREFIX_BASE + i * 4, 0);
        wait(2, SC_NS);
    }

    // Write prefix data as 32-bit words (little-endian)
    size_t word_count = (prefix_len_bytes + 3) / 4;
    for (size_t i = 0; i < word_count; i++) {
        uint32_t word_val = 0;
        size_t bytes_this_word = (i == word_count - 1) ? (prefix_len_bytes - i * 4) : 4;

        // Pack bytes into word (little-endian)
        for (size_t j = 0; j < bytes_this_word; j++) {
            word_val |= ((uint32_t)prefix_data[i * 4 + j]) << (j * 8);
        }

        test->register_write_32(PREFIX_BASE + i * 4, word_val);
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to encode string per NIST SP 800-185
 * @param input Input string bytes
 * @param input_len Length of input string
 * @param output Output buffer for encoded string
 * @param output_len_bytes Reference to store output length
 *
 * Implements encode_string(X) = left_encode(len(X)) || X
 * where left_encode(x) encodes length before the length value.
 *
 * Example: encode_string("Test") = 0x01 0x20 "Test"
 *   - left_encode(32) = 0x01 0x20 (1 byte length indicator, 32-bit length in bits)
 *   - String bytes: "Test"
 */
static void encode_string(const uint8_t* input, size_t input_len,
                           uint8_t* output, size_t& output_len_bytes)
{
    // Calculate bit length
    size_t bit_length = input_len * 8;

    // Encode bit length as left_encode(bit_length)
    // For lengths < 256 bits, we need 1 byte: 0x01 <length>
    // For lengths < 65536 bits, we need 2 bytes: 0x02 <length_high> <length_low>

    size_t offset = 0;
    if (bit_length < 256) {
        // Single byte length
        output[offset++] = 0x01; // Length of encoding = 1 byte
        output[offset++] = (uint8_t)(bit_length & 0xFF);
    } else if (bit_length < 65536) {
        // Two byte length
        output[offset++] = 0x02; // Length of encoding = 2 bytes
        output[offset++] = (uint8_t)((bit_length >> 8) & 0xFF);
        output[offset++] = (uint8_t)(bit_length & 0xFF);
    } else {
        // For larger lengths, use 4 bytes
        output[offset++] = 0x04; // Length of encoding = 4 bytes
        output[offset++] = (uint8_t)((bit_length >> 24) & 0xFF);
        output[offset++] = (uint8_t)((bit_length >> 16) & 0xFF);
        output[offset++] = (uint8_t)((bit_length >> 8) & 0xFF);
        output[offset++] = (uint8_t)(bit_length & 0xFF);
    }

    // Append string bytes
    if (input_len > 0 && input != nullptr) {
        memcpy(output + offset, input, input_len);
    }
    offset += input_len;

    output_len_bytes = offset;
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
 */
static void write_msg_fifo(kmac_test* test, const uint8_t* data, size_t len_bytes)
{
    const uint32_t MSG_FIFO_BASE = 0x800;

    for (size_t i = 0; i < len_bytes; i++) {
        test->register_write_8(MSG_FIFO_BASE + i, data[i]);
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to read digest from STATE window
 * @param test Pointer to test harness
 * @param digest Output buffer for digest data
 * @param digest_len_bytes Length of digest to read in bytes
 *
 * When EnMasking=1 (testbench default), XORs share0 and share1 to reconstruct digest.
 */
static void read_state_digest(kmac_test* test, uint8_t* digest, size_t digest_len_bytes)
{
    const uint32_t STATE_SHARE0_BASE = 0x400;
    const uint32_t STATE_SHARE1_BASE = 0x500;

    size_t word_count = (digest_len_bytes + 3) / 4;
    for (size_t i = 0; i < word_count; i++) {
        uint32_t share0_word = 0;
        uint32_t share1_word = 0;

        test->register_read_32(STATE_SHARE0_BASE + i * 4, share0_word);
        test->register_read_32(STATE_SHARE1_BASE + i * 4, share1_word);

        // XOR shares to reconstruct digest
        uint32_t digest_word = share0_word ^ share1_word;

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
 * @brief Helper function to clean up after test (return to IDLE state)
 * @param test Pointer to test harness
 */
static void cleanup_test(kmac_test* test)
{
    bool idle, absorb, squeeze;
    read_status_fsm_bits(test, idle, absorb, squeeze);

    if (!idle) {
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        if (err_code != 0) {
            write_cmd(test, 0x400); // err_processed bit (bit 10)
        } else {
            write_cmd(test, 0x16); // DONE command
        }
        wait(10, SC_NS);
    }
}

/**
 * @brief Helper function to apply bytepad encoding per NIST SP 800-185
 * @param input Input bytes (encode_string(N) || encode_string(S))
 * @param input_len Input length in bytes
 * @param rate Rate in bytes (168 for cSHAKE128, 136 for cSHAKE256)
 * @param output Output buffer for bytepad result
 * @param output_len Reference to store output length (will be equal to rate)
 *
 * Implements bytepad(X, w) = left_encode(w) || X || 0x00... (padded to w bytes)
 */
static void apply_bytepad(const uint8_t* input, size_t input_len,
                           size_t rate, uint8_t* output, size_t& output_len)
{
    size_t offset = 0;

    // Add left_encode(rate) at the beginning
    // For rate=168: left_encode(168) = 0x01 0xA8
    // For rate=136: left_encode(136) = 0x01 0x88
    if (rate < 256) {
        output[offset++] = 0x01;  // Length of encoding = 1 byte
        output[offset++] = (uint8_t)(rate & 0xFF);
    } else {
        output[offset++] = 0x02;  // Length of encoding = 2 bytes
        output[offset++] = (uint8_t)((rate >> 8) & 0xFF);
        output[offset++] = (uint8_t)(rate & 0xFF);
    }

    // Append input bytes
    memcpy(output + offset, input, input_len);
    offset += input_len;

    // Zero-pad to rate bytes
    while (offset < rate) {
        output[offset++] = 0x00;
    }

    output_len = rate;
}

/**
 * @brief Helper function to compute reference cSHAKE128 output using OpenSSL
 * @param prefix Prefix bytes (encode_string(N) || encode_string(S))
 * @param prefix_len Prefix length in bytes
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param output_len Desired output length in bytes
 * @return true if computation successful, false otherwise
 *
 * OpenSSL does not provide native cSHAKE, so we simulate by prepending
 * bytepad(PREFIX, rate) to message and using SHAKE128.
 */
static bool compute_cshake128_reference(const uint8_t* prefix, size_t prefix_len,
                                         const uint8_t* message, size_t msg_len,
                                         uint8_t* output, size_t output_len)
{
    const size_t rate = 168;  // cSHAKE128 rate in bytes

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake128(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Apply bytepad to prefix if non-empty
    if (prefix_len > 0) {
        uint8_t bytepadded[168];
        size_t bytepad_len = 0;
        apply_bytepad(prefix, prefix_len, rate, bytepadded, bytepad_len);

        if (EVP_DigestUpdate(ctx, bytepadded, bytepad_len) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
    }

    // Absorb message
    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Generate output
    if (EVP_DigestFinalXOF(ctx, output, output_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compute reference cSHAKE256 output using OpenSSL
 * @param prefix Prefix bytes (encode_string(N) || encode_string(S))
 * @param prefix_len Prefix length in bytes
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param output_len Desired output length in bytes
 * @return true if computation successful, false otherwise
 *
 * OpenSSL does not provide native cSHAKE, so we simulate by prepending
 * bytepad(PREFIX, rate) to message and using SHAKE256.
 */
static bool compute_cshake256_reference(const uint8_t* prefix, size_t prefix_len,
                                         const uint8_t* message, size_t msg_len,
                                         uint8_t* output, size_t output_len)
{
    const size_t rate = 136;  // cSHAKE256 rate in bytes

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_shake256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Apply bytepad to prefix if non-empty
    if (prefix_len > 0) {
        uint8_t bytepadded[136];
        size_t bytepad_len = 0;
        apply_bytepad(prefix, prefix_len, rate, bytepadded, bytepad_len);

        if (EVP_DigestUpdate(ctx, bytepadded, bytepad_len) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
    }

    // Absorb message
    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Generate output
    if (EVP_DigestFinalXOF(ctx, output, output_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to compute reference cSHAKE output in multiple blocks
 * @param use_cshake256 Use cSHAKE256 if true, cSHAKE128 if false
 * @param prefix Prefix bytes
 * @param prefix_len Prefix length in bytes
 * @param message Input message buffer
 * @param msg_len Message length in bytes
 * @param output Output buffer for digest
 * @param block_sizes Array of block sizes to generate
 * @param num_blocks Number of blocks to generate
 * @return true if computation successful, false otherwise
 */
static bool compute_cshake_reference_blocks(bool use_cshake256,
                                             const uint8_t* prefix, size_t prefix_len,
                                             const uint8_t* message, size_t msg_len,
                                             uint8_t* output, const size_t* block_sizes, size_t num_blocks)
{
    const size_t rate = use_cshake256 ? 136 : 168;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    const EVP_MD* md = use_cshake256 ? EVP_shake256() : EVP_shake128();
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Apply bytepad to prefix if non-empty
    if (prefix_len > 0) {
        uint8_t bytepadded[168];  // Max rate is 168
        size_t bytepad_len = 0;
        apply_bytepad(prefix, prefix_len, rate, bytepadded, bytepad_len);

        if (EVP_DigestUpdate(ctx, bytepadded, bytepad_len) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
    }

    // Absorb message
    if (EVP_DigestUpdate(ctx, message, msg_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // cSHAKE is an XOF: its output is a single continuous, deterministic byte
    // stream, so squeezing blocks of sizes s0, s1, ... produces exactly the same
    // bytes as one finalize of length (s0 + s1 + ...).  EVP_DigestFinalXOF may
    // only be called ONCE per context (a second call fails), so we sum the block
    // sizes and finalize once into the contiguous output buffer.
    size_t total_len = 0;
    for (size_t i = 0; i < num_blocks; i++) {
        total_len += block_sizes[i];
    }
    if (EVP_DigestFinalXOF(ctx, output, total_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
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
 * TC-038: cSHAKE128 with Empty Customization Test
 *
 * Verifies cSHAKE128 operation with empty N and S (functionally equivalent to SHAKE128).
 * - Configure cSHAKE128 mode (mode=0x3, kstrength=0x0, kmac_en=0)
 * - Write zeros to all PREFIX registers (empty customization)
 * - Absorb test message
 * - Issue PROCESS command to generate digest
 * - Read STATE window for 32-byte output
 * - Validate against SHAKE128 reference (cSHAKE with empty N/S == SHAKE per NIST)
 ******************************************************************************/
void testbench::test_cshake128_with_empty_customization()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-038: test_cshake128_with_empty_customization");

    try {
        const char* test_msg = "cSHAKE128 empty customization test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 32; // 256 bits

        // Configure cSHAKE128 mode
        configure_cshake_mode(test, 0x0);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=cSHAKE, kstrength=L128, kmac_en=0";

        // Write empty PREFIX (all zeros)
        uint8_t empty_prefix[40] = {0};
        write_prefix_registers(test, empty_prefix, 0);
        CSML_INFO(2, test_logger) << "Wrote empty PREFIX registers (all zeros)";

        // Verify initial IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-038", "Precondition: FSM not in IDLE state");
            return;
        }

        // Issue START command
        CSML_INFO(2, test_logger) << "Issuing START command (0x1D)";
        write_cmd(test, 0x1D);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-038", "Error after START command");
            return;
        }

        // Verify FSM transitioned to ABSORB state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-038", "FSM not in ABSORB state after START");
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
            report_test_fail("TC-038", "Error after PROCESS command");
            return;
        }

        // Verify FSM transitioned to SQUEEZE state
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-038", "FSM not in SQUEEZE state after PROCESS");
            return;
        }

        // Read digest from STATE window
        uint8_t actual_digest[32];
        CSML_INFO(2, test_logger) << "Reading " << output_len << "-byte digest from STATE window";
        read_state_digest(test, actual_digest, output_len);

        // Compute reference digest (empty cSHAKE == SHAKE per NIST spec)
        uint8_t expected_digest[32];
        if (!compute_cshake128_reference(NULL, 0, (const uint8_t*)test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-038", "OpenSSL reference computation failed");
            return;
        }

        // Compare digests
        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-038", "Digest mismatch with OpenSSL reference");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE128 empty customization digest matches reference";

        cleanup_test(test);
        report_test_pass("TC-038: test_cshake128_with_empty_customization");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-038", e.what());
    }
}

/******************************************************************************
 * TC-039: cSHAKE256 with Empty Customization Test
 *
 * Verifies cSHAKE256 operation with empty N and S (functionally equivalent to SHAKE256).
 * - Configure cSHAKE256 mode (mode=0x3, kstrength=0x2, kmac_en=0)
 * - Write zeros to all PREFIX registers (empty customization)
 * - Absorb test message
 * - Issue PROCESS command to generate digest
 * - Read STATE window for 64-byte output
 * - Validate against SHAKE256 reference (cSHAKE with empty N/S == SHAKE per NIST)
 ******************************************************************************/
void testbench::test_cshake256_with_empty_customization()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-039: test_cshake256_with_empty_customization");

    try {
        const char* test_msg = "cSHAKE256 empty customization test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 64; // 512 bits

        configure_cshake_mode(test, 0x2);
        CSML_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=cSHAKE, kstrength=L256, kmac_en=0";

        uint8_t empty_prefix[40] = {0};
        write_prefix_registers(test, empty_prefix, 0);
        CSML_INFO(2, test_logger) << "Wrote empty PREFIX registers";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-039", "FSM not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-039", "Error after START");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!absorb) {
            cleanup_test(test);
            report_test_fail("TC-039", "Not in ABSORB state");
            return;
        }

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-039", "Error after PROCESS");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-039", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_digest[64];
        read_state_digest(test, actual_digest, output_len);

        uint8_t expected_digest[64];
        if (!compute_cshake256_reference(NULL, 0, (const uint8_t*)test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-039", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-039", "Digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE256 empty customization digest matches reference";

        cleanup_test(test);
        report_test_pass("TC-039: test_cshake256_with_empty_customization");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-039", e.what());
    }
}

/******************************************************************************
 * TC-040: cSHAKE128 with Function Name Test
 *
 * Verifies cSHAKE128 operation with non-empty function name N.
 * - Configure cSHAKE128 mode
 * - Write PREFIX registers with encode_string(N) where N = "Email Signature"
 * - Absorb test message
 * - Issue PROCESS command
 * - Validate digest against reference with same PREFIX prepended
 ******************************************************************************/
void testbench::test_cshake128_with_function_name()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-040: test_cshake128_with_function_name");

    try {
        const char* function_name = "Email Signature";
        const char* test_msg = "Message to hash with function name";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 32;

        // Encode function name N
        uint8_t encoded_N[100];
        size_t encoded_N_len = 0;
        encode_string((const uint8_t*)function_name, strlen(function_name),
                       encoded_N, encoded_N_len);

        // Encode empty customization string S
        uint8_t encoded_S[10];
        size_t encoded_S_len = 0;
        encode_string(NULL, 0, encoded_S, encoded_S_len);

        // Construct PREFIX: encode_string(N) || encode_string(S)
        uint8_t prefix_data[40];
        size_t prefix_len = 0;
        memcpy(prefix_data, encoded_N, encoded_N_len);
        prefix_len += encoded_N_len;
        memcpy(prefix_data + prefix_len, encoded_S, encoded_S_len);
        prefix_len += encoded_S_len;

        CSML_INFO(2, test_logger) << "Encoded PREFIX: N=\"" << function_name
                            << "\", S=\"\" (total " << prefix_len << " bytes)";

        configure_cshake_mode(test, 0x0);
        write_prefix_registers(test, prefix_data, prefix_len);

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-040", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-040", "Error after START");
            return;
        }

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-040", "Error after PROCESS");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-040", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_digest[32];
        read_state_digest(test, actual_digest, output_len);

        // Compute reference with PREFIX prepended
        uint8_t expected_digest[32];
        if (!compute_cshake128_reference(prefix_data, prefix_len,
                                          (const uint8_t*)test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-040", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-040", "Digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE128 with function name verified";

        cleanup_test(test);
        report_test_pass("TC-040: test_cshake128_with_function_name");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-040", e.what());
    }
}

/******************************************************************************
 * TC-041: cSHAKE256 with Customization String Test
 *
 * Verifies cSHAKE256 operation with customization string S.
 * - Configure cSHAKE256 mode
 * - Write PREFIX registers with encode_string(N) || encode_string(S)
 *   where N = "" and S = "My Tagged Application"
 * - Absorb test message
 * - Issue PROCESS command
 * - Validate digest against reference with same PREFIX prepended
 ******************************************************************************/
void testbench::test_cshake256_with_customization_string()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-041: test_cshake256_with_customization_string");

    try {
        const char* customization_string = "My Tagged Application";
        const char* test_msg = "Message with customization string";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 64;

        // Encode empty function name N
        uint8_t encoded_N[10];
        size_t encoded_N_len = 0;
        encode_string(NULL, 0, encoded_N, encoded_N_len);

        // Encode customization string S
        uint8_t encoded_S[100];
        size_t encoded_S_len = 0;
        encode_string((const uint8_t*)customization_string, strlen(customization_string),
                       encoded_S, encoded_S_len);

        // Construct PREFIX: encode_string(N) || encode_string(S)
        uint8_t prefix_data[40];
        size_t prefix_len = 0;
        memcpy(prefix_data, encoded_N, encoded_N_len);
        prefix_len += encoded_N_len;
        memcpy(prefix_data + prefix_len, encoded_S, encoded_S_len);
        prefix_len += encoded_S_len;

        CSML_INFO(2, test_logger) << "Encoded PREFIX: N=\"\", S=\"" << customization_string
                            << "\" (total " << prefix_len << " bytes)";

        configure_cshake_mode(test, 0x2);
        write_prefix_registers(test, prefix_data, prefix_len);

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-041", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-041", "Error during operation");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-041", "Not in SQUEEZE state");
            return;
        }

        uint8_t actual_digest[64];
        read_state_digest(test, actual_digest, output_len);

        uint8_t expected_digest[64];
        if (!compute_cshake256_reference(prefix_data, prefix_len,
                                          (const uint8_t*)test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-041", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-041", "Digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE256 with customization string verified";

        cleanup_test(test);
        report_test_pass("TC-041: test_cshake256_with_customization_string");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-041", e.what());
    }
}

/******************************************************************************
 * TC-042: cSHAKE PREFIX Expansion Test
 *
 * Verifies PREFIX register encoding and expansion to full block size.
 * - Configure cSHAKE128 (168-byte rate)
 * - Write small PREFIX data (10 bytes)
 * - Verify hardware expands PREFIX to full 168-byte block during START
 * - Validate digest against reference (PREFIX prepended to message)
 *
 * This test validates that hardware correctly expands PREFIX to full block
 * size before message absorption, as required by NIST SP 800-185.
 ******************************************************************************/
void testbench::test_cshake_prefix_expansion()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-042: test_cshake_prefix_expansion");

    try {
        // Small PREFIX to test expansion
        const char* function_name = "Test";
        const char* test_msg = "Short message for expansion test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_len = 32;

        // Encode minimal PREFIX
        uint8_t encoded_N[20];
        size_t encoded_N_len = 0;
        encode_string((const uint8_t*)function_name, strlen(function_name),
                       encoded_N, encoded_N_len);

        uint8_t encoded_S[10];
        size_t encoded_S_len = 0;
        encode_string(NULL, 0, encoded_S, encoded_S_len);

        uint8_t prefix_data[40];
        size_t prefix_len = 0;
        memcpy(prefix_data, encoded_N, encoded_N_len);
        prefix_len += encoded_N_len;
        memcpy(prefix_data + prefix_len, encoded_S, encoded_S_len);
        prefix_len += encoded_S_len;

        CSML_INFO(2, test_logger) << "Small PREFIX: " << prefix_len
                            << " bytes (hardware will expand to 168-byte block)";

        configure_cshake_mode(test, 0x0);
        write_prefix_registers(test, prefix_data, prefix_len);

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-042", "Not in IDLE state");
            return;
        }

        // START command triggers PREFIX expansion
        write_cmd(test, 0x1D);
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-042", "Error after START (PREFIX expansion)");
            return;
        }

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-042", "Error after PROCESS");
            return;
        }

        uint8_t actual_digest[32];
        read_state_digest(test, actual_digest, output_len);

        // Reference computation with PREFIX prepended
        uint8_t expected_digest[32];
        if (!compute_cshake128_reference(prefix_data, prefix_len,
                                          (const uint8_t*)test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-042", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-042", "Digest mismatch - PREFIX expansion failed");
            return;
        }

        CSML_INFO(2, test_logger) << "PREFIX expansion to full block verified";

        cleanup_test(test);
        report_test_pass("TC-042: test_cshake_prefix_expansion");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-042", e.what());
    }
}

/******************************************************************************
 * TC-043: cSHAKE Padding Mechanism Test
 *
 * Verifies cSHAKE padding pattern (2-bit '00' followed by pad10*1).
 * This differs from SHA3 (2-bit '10') and SHAKE (4-bit '1111').
 *
 * Tests that cSHAKE padding is correctly applied by comparing digest with
 * OpenSSL reference (which implements correct padding).
 ******************************************************************************/
void testbench::test_cshake_padding_mechanism()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-043: test_cshake_padding_mechanism");

    try {
        // Use message length that exercises padding logic
        // 167 bytes is one byte short of cSHAKE128 rate (168 bytes)
        const size_t msg_len = 167;
        uint8_t test_msg[167];
        for (size_t i = 0; i < msg_len; i++) {
            test_msg[i] = (uint8_t)(i & 0xFF);
        }

        const size_t output_len = 32;

        // Use non-empty PREFIX to distinguish from SHAKE
        const char* function_name = "Pad";
        uint8_t encoded_N[20];
        size_t encoded_N_len = 0;
        encode_string((const uint8_t*)function_name, strlen(function_name),
                       encoded_N, encoded_N_len);

        uint8_t encoded_S[10];
        size_t encoded_S_len = 0;
        encode_string(NULL, 0, encoded_S, encoded_S_len);

        uint8_t prefix_data[40];
        size_t prefix_len = 0;
        memcpy(prefix_data, encoded_N, encoded_N_len);
        prefix_len += encoded_N_len;
        memcpy(prefix_data + prefix_len, encoded_S, encoded_S_len);
        prefix_len += encoded_S_len;

        configure_cshake_mode(test, 0x0);
        write_prefix_registers(test, prefix_data, prefix_len);
        CSML_INFO(2, test_logger) << "Testing cSHAKE padding with " << msg_len << "-byte message";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-043", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-043", "Error during operation");
            return;
        }

        uint8_t actual_digest[32];
        read_state_digest(test, actual_digest, output_len);

        uint8_t expected_digest[32];
        if (!compute_cshake128_reference(prefix_data, prefix_len,
                                          test_msg, msg_len,
                                          expected_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-043", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_digest, actual_digest, output_len)) {
            cleanup_test(test);
            report_test_fail("TC-043", "Padding validation failed - digest mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE padding mechanism validated (2-bit '00' + pad10*1)";

        cleanup_test(test);
        report_test_pass("TC-043: test_cshake_padding_mechanism");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-043", e.what());
    }
}

/******************************************************************************
 * TC-044: cSHAKE Extended Output Test
 *
 * Verifies cSHAKE extended output using RUN command for arbitrary length.
 * - Configure cSHAKE128 mode with non-empty PREFIX
 * - Absorb message and issue PROCESS
 * - Read first block from STATE (168 bytes for cSHAKE128)
 * - Issue RUN command for next block
 * - Verify FSM remains in SQUEEZE state after RUN
 * - Read second block from STATE
 * - Validate extended output against OpenSSL reference
 ******************************************************************************/
void testbench::test_cshake_extended_output()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-044: test_cshake_extended_output");

    try {
        const char* function_name = "ExtOut";
        const char* test_msg = "cSHAKE extended output test message";
        const size_t msg_len = strlen(test_msg);
        const size_t block1_len = 168; // cSHAKE128 rate
        const size_t block2_len = 100;
        const size_t total_output = block1_len + block2_len;

        // Encode PREFIX
        uint8_t encoded_N[20];
        size_t encoded_N_len = 0;
        encode_string((const uint8_t*)function_name, strlen(function_name),
                       encoded_N, encoded_N_len);

        uint8_t encoded_S[10];
        size_t encoded_S_len = 0;
        encode_string(NULL, 0, encoded_S, encoded_S_len);

        uint8_t prefix_data[40];
        size_t prefix_len = 0;
        memcpy(prefix_data, encoded_N, encoded_N_len);
        prefix_len += encoded_N_len;
        memcpy(prefix_data + prefix_len, encoded_S, encoded_S_len);
        prefix_len += encoded_S_len;

        configure_cshake_mode(test, 0x0);
        write_prefix_registers(test, prefix_data, prefix_len);
        CSML_INFO(2, test_logger) << "Configured cSHAKE128 for extended output";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-044", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);
        write_cmd(test, 0x2E);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-044", "Error after PROCESS");
            return;
        }

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-044", "Not in SQUEEZE state");
            return;
        }

        // Read first block
        uint8_t actual_output[300];
        CSML_INFO(2, test_logger) << "Reading first block (" << block1_len << " bytes)";
        read_state_digest(test, actual_output, block1_len);

        // Issue RUN command for extended output
        CSML_INFO(2, test_logger) << "Issuing RUN command (0x31) for extended output";
        write_cmd(test, 0x31);

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-044", "Error after RUN command");
            return;
        }

        // Verify FSM still in SQUEEZE state after RUN
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-044", "FSM not in SQUEEZE state after RUN");
            return;
        }

        // Read second block
        CSML_INFO(2, test_logger) << "Reading second block (" << block2_len << " bytes)";
        read_state_digest(test, actual_output + block1_len, block2_len);

        // Compute reference extended output
        uint8_t expected_output[300];
        size_t block_sizes[] = {block1_len, block2_len};
        if (!compute_cshake_reference_blocks(false, prefix_data, prefix_len,
                                              (const uint8_t*)test_msg, msg_len,
                                              expected_output, block_sizes, 2)) {
            cleanup_test(test);
            report_test_fail("TC-044", "OpenSSL reference failed");
            return;
        }

        if (!compare_buffers(expected_output, actual_output, total_output)) {
            cleanup_test(test);
            report_test_fail("TC-044", "Extended output mismatch");
            return;
        }

        CSML_INFO(2, test_logger) << "cSHAKE extended output validated: " << total_output << " bytes";

        cleanup_test(test);
        report_test_pass("TC-044: test_cshake_extended_output");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-044", e.what());
    }
}

// End of file
