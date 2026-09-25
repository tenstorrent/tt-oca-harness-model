// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_func004_test.cpp
 * @brief Test cases for FUNC-KMAC-004 (KMAC Message Authentication Code)
 *
 * This file implements test cases for FUNC-KMAC-004, verifying KMAC (Keyed-Hash
 * Message Authentication Code) mode operation providing cryptographic MAC
 * functionality per NIST SP 800-185.
 *
 * FUNC-KMAC-004 Test Coverage:
 * - KMAC128 and KMAC256 algorithm selection and initialization
 * - Key length configuration (128, 192, 256, 384, 512 bits via KEY_LEN register)
 * - Key block construction (encode_string(key_length) || key_bytes)
 * - Key block prepending and absorption before PREFIX
 * - PREFIX validation for encode_string("KMAC") = 0x01 0x20 0x4B 0x4D 0x41 0x43
 * - IncorrectFunctionName error (0x07) when PREFIX mismatch
 * - Output length encoding (right_encode(output_length_bits) by software)
 * - Extended output using RUN command for MAC longer than rate
 * - OpenSSL SHAKE128/256 delegation with key block and PREFIX prepended
 * - Empty message KMAC support
 * - Minimum (128-bit) and maximum (512-bit) key length corner cases
 *
 * KMAC Mode Configuration:
 * - CFG_SHADOWED.kmac_en = 1 (activates KMAC operation)
 * - CFG_SHADOWED.mode = 0x3 (cSHAKE underlying function)
 * - CFG_SHADOWED.kstrength = 0x0 (KMAC128, rate=168 bytes) or 0x2 (KMAC256, rate=136 bytes)
 * - KEY_SHARE0/KEY_SHARE1: Secret key input (masked or unmasked)
 * - KEY_LEN: 0x0=128b, 0x1=192b, 0x2=256b, 0x3=384b, 0x4=512b
 * - PREFIX_0 through PREFIX_10: Store encode_string("KMAC") || encode_string(S)
 *
 * PREFIX Register Format for KMAC:
 * - MUST start with encode_string("KMAC") = 0x01 0x20 0x4B 0x4D 0x41 0x43
 * - Followed by encode_string(S) for customization string
 * - Hardware validates PREFIX_0 bytes during START command
 * - IncorrectFunctionName error if validation fails
 *
 * Command Sequence:
 * - Configure KEY_SHARE0/KEY_SHARE1 registers with secret key
 * - Configure KEY_LEN register to select key length
 * - Configure PREFIX registers starting with encode_string("KMAC")
 * - START (0x1D): IDLE → ABSORB (key block + PREFIX expansion and absorption)
 * - MSG_FIFO writes: Absorb message data
 * - MSG_FIFO write right_encode(output_length_bits): Required for KMAC
 * - PROCESS (0x2E): ABSORB → SQUEEZE (first MAC output block available)
 * - RUN (0x31): Remain in SQUEEZE, generate next output block (extended MAC)
 * - DONE (0x16): SQUEEZE → IDLE
 *
 * Test Plan Reference: kmac-test-plan.md
 * Test Case Mapping: kmac-functionality-testcases.md (TC-045 to TC-055, TC-172, TC-177, TC-178)
 * Architecture Reference: kmac-architecture-behaviour-map.json
 * Detailed Design: kmac-detailed-design.md
 * Functionality: kmac-functionality_list.md (FUNC-KMAC-004)
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "kmac_test.h"
#include "testbench.h"
#include "reg_logger.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <openssl/evp.h>

using kmac_ref::compute_kmac_reference;

// Logger for test output
static RegLogger test_logger;

/******************************************************************************
 * Helper Functions for KMAC Testing
 ******************************************************************************/

/**
 * @brief Helper function to configure CFG_SHADOWED for KMAC mode
 * @param test Pointer to test harness
 * @param kstrength Keccak strength value (0x0=KMAC128, 0x2=KMAC256)
 *
 * Configures CFG_SHADOWED register with:
 * - mode = 0x3 (cSHAKE underlying function)
 * - kstrength = specified value (0x0 or 0x2 for valid KMAC)
 * - kmac_en = 1 (enable KMAC authentication mode)
 * - All other fields = 0 (default values)
 *
 * Performs shadow register duplicate write sequence for validation.
 */
static void configure_kmac_mode(kmac_test* test, uint32_t kstrength)
{
    // CFG_SHADOWED register format (spec-compliant for EnMasking=1):
    // Bit [0] = kmac_en (1 for KMAC)
    // Bits [3:1] = kstrength
    // Bits [5:4] = mode (0x3 for KMAC)
    // Bits [17:16] = entropy_mode (0x1 = edn_mode) - REQUIRED
    // Bit [24] = entropy_ready (1 = ready) - REQUIRED
    uint32_t cfg_val = (1 << 0) |                // kmac_en=1
                       ((kstrength & 0x7) << 1) | // kstrength
                       (0x3 << 4) |               // mode=0x3 (KMAC)
                       (0x1 << 16) |              // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);               // entropy_ready=1 - REQUIRED

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to write KEY_SHARE0 registers
 * @param test Pointer to test harness
 * @param key_data Pointer to key data buffer
 * @param key_len_bytes Length of key data in bytes
 *
 * Writes KEY_SHARE0_0 through KEY_SHARE0_15 registers with provided key data.
 * KEY registers are protected by CFG_REGWEN and must be written in IDLE state.
 * Each KEY_SHARE register is 32 bits (4 bytes), little-endian storage.
 */
static void write_key_share0(kmac_test* test, const uint8_t* key_data, size_t key_len_bytes)
{
    const uint32_t KEY_SHARE0_BASE = test->KEY_SHARE0_OFFSET;
    const size_t MAX_KEY_BYTES = 64; // 16 registers * 4 bytes/register = 64 bytes (512 bits)

    if (key_len_bytes > MAX_KEY_BYTES) {
        key_len_bytes = MAX_KEY_BYTES;
    }

    // Write key data as 32-bit words (little-endian)
    size_t word_count = (key_len_bytes + 3) / 4;
    for (size_t i = 0; i < word_count; i++) {
        uint32_t word_val = 0;
        size_t bytes_this_word = ((i == word_count - 1) && (key_len_bytes % 4 != 0)) ? (key_len_bytes % 4) : 4;

        // Pack bytes into word (little-endian)
        for (size_t j = 0; j < bytes_this_word; j++) {
            word_val |= ((uint32_t)key_data[i * 4 + j]) << (j * 8);
        }

        test->register_write_32(KEY_SHARE0_BASE + i * 4, word_val);
        wait(2, SC_NS);
    }

    // Zero remaining KEY_SHARE0 registers
    for (size_t i = word_count; i < 16; i++) {
        test->register_write_32(KEY_SHARE0_BASE + i * 4, 0);
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to write KEY_LEN register
 * @param test Pointer to test harness
 * @param key_len_val Key length value (0x0=128b, 0x1=192b, 0x2=256b, 0x3=384b, 0x4=512b)
 */
static void write_key_len(kmac_test* test, uint32_t key_len_val)
{
    test->register_write_32(test->KEY_LEN_OFFSET, key_len_val & 0x7);
    wait(2, SC_NS);
}

/**
 * @brief Helper function to write PREFIX registers for KMAC
 * @param test Pointer to test harness
 * @param customization_string Customization string S (can be NULL or empty)
 * @param customization_len Length of customization string in bytes
 *
 * Constructs PREFIX as: encode_string("KMAC") || encode_string(S)
 * encode_string("KMAC") = 0x01 0x20 0x4B 0x4D 0x41 0x43
 * This is MANDATORY for KMAC mode and will be validated by hardware.
 */
static void write_kmac_prefix_registers(kmac_test* test, const char* customization_string, size_t customization_len)
{
    const uint32_t PREFIX_BASE = test->PREFIX_0_OFFSET;
    const size_t MAX_PREFIX_BYTES = 44; // 11 registers * 4 bytes

    uint8_t prefix_data[44];
    std::memset(prefix_data, 0, sizeof(prefix_data));
    size_t prefix_len = 0;

    // Mandatory: encode_string("KMAC")
    // left_encode(32) = 0x01 0x20 (1 byte length indicator, 32 bits)
    // "KMAC" = 0x4B 0x4D 0x41 0x43
    prefix_data[prefix_len++] = 0x01;  // left_encode(32) part 1
    prefix_data[prefix_len++] = 0x20;  // left_encode(32) part 2 (32 decimal = 0x20)
    prefix_data[prefix_len++] = 0x4B;  // 'K'
    prefix_data[prefix_len++] = 0x4D;  // 'M'
    prefix_data[prefix_len++] = 0x41;  // 'A'
    prefix_data[prefix_len++] = 0x43;  // 'C'

    // Optional: encode_string(S) for customization
    if (customization_string != NULL && customization_len > 0) {
        // left_encode(customization_len * 8) for bit length
        size_t cust_bits = customization_len * 8;
        if (cust_bits <= 255) {
            prefix_data[prefix_len++] = 0x01;  // Length of encoding = 1 byte
            prefix_data[prefix_len++] = (uint8_t)(cust_bits & 0xFF);
        } else {
            prefix_data[prefix_len++] = 0x02;  // Length of encoding = 2 bytes
            prefix_data[prefix_len++] = (uint8_t)((cust_bits >> 8) & 0xFF);
            prefix_data[prefix_len++] = (uint8_t)(cust_bits & 0xFF);
        }

        // Append customization string bytes
        size_t remaining_space = MAX_PREFIX_BYTES - prefix_len;
        size_t copy_len = (customization_len < remaining_space) ? customization_len : remaining_space;
        std::memcpy(prefix_data + prefix_len, customization_string, copy_len);
        prefix_len += copy_len;
    } else {
        // Empty customization string: encode_string("") = 0x01 0x00
        prefix_data[prefix_len++] = 0x01;  // left_encode(0) part 1
        prefix_data[prefix_len++] = 0x00;  // left_encode(0) part 2 (0 bits)
    }

    // Write PREFIX registers as 32-bit words (little-endian)
    for (size_t i = 0; i < 11; i++) {
        uint32_t word_val = 0;
        for (size_t j = 0; j < 4; j++) {
            size_t byte_idx = i * 4 + j;
            if (byte_idx < MAX_PREFIX_BYTES) {
                word_val |= ((uint32_t)prefix_data[byte_idx]) << (j * 8);
            }
        }
        test->register_write_32(PREFIX_BASE + i * 4, word_val);
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to construct right_encode(x) per NIST SP 800-185
 * @param value Value to encode (output length in bits)
 * @param output Output buffer
 * @param output_len Reference to store output length
 *
 * right_encode(x) = enc8(x) || enc8(len(enc8(x)))
 * Example: right_encode(256) = 0x01 0x00 0x01 (value in 2 bytes + length 1)
 */
static void right_encode(uint32_t value, uint8_t* output, size_t& output_len)
{
    output_len = 0;

    // Determine how many bytes needed for value
    if (value == 0) {
        output[output_len++] = 0x00;
    } else if (value <= 0xFF) {
        output[output_len++] = (uint8_t)(value & 0xFF);
    } else if (value <= 0xFFFF) {
        output[output_len++] = (uint8_t)((value >> 8) & 0xFF);
        output[output_len++] = (uint8_t)(value & 0xFF);
    } else if (value <= 0xFFFFFF) {
        output[output_len++] = (uint8_t)((value >> 16) & 0xFF);
        output[output_len++] = (uint8_t)((value >> 8) & 0xFF);
        output[output_len++] = (uint8_t)(value & 0xFF);
    } else {
        output[output_len++] = (uint8_t)((value >> 24) & 0xFF);
        output[output_len++] = (uint8_t)((value >> 16) & 0xFF);
        output[output_len++] = (uint8_t)((value >> 8) & 0xFF);
        output[output_len++] = (uint8_t)(value & 0xFF);
    }

    // Append length byte
    uint8_t len_byte = (uint8_t)output_len;
    output[output_len++] = len_byte;
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
 * @param data_len Length of message data in bytes
 */
static void write_msg_fifo(kmac_test* test, const uint8_t* data, size_t data_len)
{
    const uint32_t MSG_FIFO_BASE = 0x800;

    // Write data in 4-byte chunks, ensuring no zero-padding in partial words
    // by using a custom TLM transaction with proper data_length
    for (size_t i = 0; i < data_len; i += 4) {
        size_t bytes_this_word = ((i + 4) <= data_len) ? 4 : (data_len - i);

        // Pack only the valid bytes into the word
        uint32_t word_val = 0;
        for (size_t j = 0; j < bytes_this_word; j++) {
            word_val |= ((uint32_t)data[i + j]) << (j * 8);
        }

        if (bytes_this_word == 4) {
            // Full word - use normal write
            test->register_write_32(MSG_FIFO_BASE, word_val);
        } else {
            // Partial word - use custom TLM write with correct data_length
            tlm::tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;

            // Pack only the valid bytes
            uint8_t data_buf[4] = {0};
            for (size_t j = 0; j < bytes_this_word; j++) {
                data_buf[j] = data[i + j];
            }

            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(MSG_FIFO_BASE);
            trans.set_data_ptr(data_buf);
            trans.set_data_length(bytes_this_word);  // IMPORTANT: Only valid bytes!
            trans.set_streaming_width(bytes_this_word);
            trans.set_byte_enable_ptr(0);  // Let regmodel generate byte enables from length
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            test->initiator_socket->b_transport(trans, delay);

            if (trans.is_response_error()) {
                std::cerr << "[ERROR] TLM write error to MSG_FIFO" << std::endl;
            }
        }
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to read digest from STATE window
 * @param test Pointer to test harness
 * @param digest_buffer Output buffer for digest
 * @param digest_len Length of digest to read in bytes
 */
static void read_state_digest(kmac_test* test, uint8_t* digest_buffer, size_t digest_len)
{
    const uint32_t STATE_SHARE0_BASE = 0x400;
    const uint32_t STATE_SHARE1_BASE = 0x500;

    // For EnMasking=1: digest = share0 XOR share1
    // Read share0 from 0x400, share1 from 0x500, then XOR
    for (size_t i = 0; i < digest_len; i += 4) {
        uint32_t share0_word = 0;
        uint32_t share1_word = 0;

        test->register_read_32(STATE_SHARE0_BASE + i, share0_word);
        test->register_read_32(STATE_SHARE1_BASE + i, share1_word);

        size_t bytes_to_copy = ((i + 4) <= digest_len) ? 4 : (digest_len - i);
        for (size_t j = 0; j < bytes_to_copy; j++) {
            uint8_t share0_byte = (share0_word >> (j * 8)) & 0xFF;
            uint8_t share1_byte = (share1_word >> (j * 8)) & 0xFF;
            digest_buffer[i + j] = share0_byte ^ share1_byte;  // XOR to unmask
        }
    }
}

/**
 * @brief Helper function to verify no error occurred
 * @param test Pointer to test harness
 * @return true if no error, false if error detected
 */
static bool verify_no_error(kmac_test* test)
{
    uint32_t err_code = 0;
    test->register_read_32(test->ERR_CODE_OFFSET, err_code);

    if (err_code != 0) {
        REG_ERROR(1, test_logger) << "ERR_CODE = 0x" << std::hex << err_code << std::dec;
        return false;
    }
    return true;
}

/**
 * @brief Helper function to cleanup and return to IDLE state
 * @param test Pointer to test harness
 */
static void cleanup_test(kmac_test* test)
{
    // Issue DONE command to return to IDLE
    write_cmd(test, 0x16);
    wait(5, SC_NS);
}

/**
 * @brief Compute KMAC reference using OpenSSL SHAKE with manual key/prefix prepending
 * @param key_data Key material
 * @param key_len_bytes Key length in bytes
 * @param customization_string Customization string (can be NULL)
 * @param customization_len Customization length in bytes
 * @param message Message data
 * @param message_len Message length in bytes
 * @param output_bits Output length in bits
 * @param output_digest Output buffer for KMAC tag
 * @param output_bytes Output length in bytes
 * @param is_kmac256 True for KMAC256, false for KMAC128
 * @return true if successful, false otherwise
 *
 * Per NIST SP 800-185:
 * KMAC(K, X, L, S) = cSHAKE(bytepad(encode_string(K), rate) || X || right_encode(L), rate, "KMAC", S)
 */
bool kmac_ref::compute_kmac_reference(const uint8_t* key_data, size_t key_len_bytes,
                                    const char* customization_string, size_t customization_len,
                                    const uint8_t* message, size_t message_len,
                                    uint32_t output_bits,
                                    uint8_t* output_digest, size_t output_bytes,
                                    bool is_kmac256)
{
    const EVP_MD* md = is_kmac256 ? EVP_shake256() : EVP_shake128();
    size_t rate = is_kmac256 ? 136 : 168;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return false;
    }

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Step 1: Construct and absorb bytepad(encode_string("KMAC") || encode_string(S), rate)
    // This is the PREFIX block that cSHAKE adds - must come FIRST per NIST SP 800-185
    uint8_t prefix_data[256];
    size_t prefix_len = 0;

    // encode_string("KMAC")
    prefix_data[prefix_len++] = 0x01;
    prefix_data[prefix_len++] = 0x20;  // 32 bits
    prefix_data[prefix_len++] = 0x4B;  // 'K'
    prefix_data[prefix_len++] = 0x4D;  // 'M'
    prefix_data[prefix_len++] = 0x41;  // 'A'
    prefix_data[prefix_len++] = 0x43;  // 'C'

    // encode_string(S)
    if (customization_string != NULL && customization_len > 0) {
        size_t cust_bits = customization_len * 8;
        if (cust_bits <= 255) {
            prefix_data[prefix_len++] = 0x01;
            prefix_data[prefix_len++] = (uint8_t)(cust_bits & 0xFF);
        } else {
            prefix_data[prefix_len++] = 0x02;
            prefix_data[prefix_len++] = (uint8_t)((cust_bits >> 8) & 0xFF);
            prefix_data[prefix_len++] = (uint8_t)(cust_bits & 0xFF);
        }
        std::memcpy(prefix_data + prefix_len, customization_string, customization_len);
        prefix_len += customization_len;
    } else {
        prefix_data[prefix_len++] = 0x01;
        prefix_data[prefix_len++] = 0x00;
    }

    // bytepad(prefix_data, rate)
    uint8_t bytepadded_prefix[168];
    size_t bytepad_len = 0;

    bytepadded_prefix[bytepad_len++] = 0x01;
    bytepadded_prefix[bytepad_len++] = (uint8_t)(rate & 0xFF);

    std::memcpy(bytepadded_prefix + bytepad_len, prefix_data, prefix_len);
    bytepad_len += prefix_len;

    while (bytepad_len < rate) {
        bytepadded_prefix[bytepad_len++] = 0x00;
    }

    // Debug: Log first 40 bytes of bytepadded PREFIX
    REG_INFO(2, test_logger) << "TEST REF PREFIX block (first 40 bytes):";
    std::stringstream ss_prefix;
    for (size_t i = 0; i < 40 && i < rate; i++) {
        ss_prefix << std::hex << std::setfill('0') << std::setw(2) << (int)bytepadded_prefix[i];
        if (i < 39) ss_prefix << " ";
    }
    REG_INFO(2, test_logger) << ss_prefix.str();

    // Absorb PREFIX block FIRST
    if (EVP_DigestUpdate(ctx, bytepadded_prefix, rate) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Step 2: Construct and absorb bytepad(encode_string(K), rate)
    // This is the KEY block - must come SECOND after PREFIX per NIST SP 800-185
    size_t key_bits = key_len_bytes * 8;
    uint8_t encoded_key[256];
    size_t encoded_key_len = 0;

    // left_encode(key_bits)
    if (key_bits <= 255) {
        encoded_key[encoded_key_len++] = 0x01;
        encoded_key[encoded_key_len++] = (uint8_t)(key_bits & 0xFF);
    } else {
        encoded_key[encoded_key_len++] = 0x02;
        encoded_key[encoded_key_len++] = (uint8_t)((key_bits >> 8) & 0xFF);
        encoded_key[encoded_key_len++] = (uint8_t)(key_bits & 0xFF);
    }

    // Append key bytes
    std::memcpy(encoded_key + encoded_key_len, key_data, key_len_bytes);
    encoded_key_len += key_len_bytes;

    // bytepad(encode_string(K), rate)
    uint8_t key_block[168];
    size_t key_block_len = 0;

    key_block[key_block_len++] = 0x01;  // left_encode(rate) length
    key_block[key_block_len++] = (uint8_t)(rate & 0xFF);

    std::memcpy(key_block + key_block_len, encoded_key, encoded_key_len);
    key_block_len += encoded_key_len;

    while (key_block_len < rate) {
        key_block[key_block_len++] = 0x00;
    }

    // Absorb KEY block SECOND
    if (EVP_DigestUpdate(ctx, key_block, rate) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Step 3: Absorb message
    if (message != NULL && message_len > 0) {
        if (EVP_DigestUpdate(ctx, message, message_len) != 1) {
            EVP_MD_CTX_free(ctx);
            return false;
        }
    }

    // Step 4: Absorb right_encode(output_bits)
    uint8_t right_enc[8];
    size_t right_enc_len = 0;
    right_encode(output_bits, right_enc, right_enc_len);

    if (EVP_DigestUpdate(ctx, right_enc, right_enc_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    // Step 5: Finalize and extract output
    if (EVP_DigestFinalXOF(ctx, output_digest, output_bytes) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/******************************************************************************
 * FUNC-KMAC-004 Test Cases
 ******************************************************************************/

/**
 * @brief TC-045: test_kmac_128bit_key_256bit_output
 *
 * Verifies KMAC with 128-bit key and 256-bit MAC output.
 * Tests basic KMAC operation with minimum key length and standard output length.
 */
void testbench::test_kmac_128bit_key_256bit_output()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-045: test_kmac_128bit_key_256bit_output");

    try {
        const char* test_msg = "KMAC test with 128-bit key";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        // 128-bit key
        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(0xA0 + i);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);  // 128-bit key
        write_kmac_prefix_registers(test, NULL, 0);  // No customization

        REG_INFO(2, test_logger) << "Testing KMAC128 with 128-bit key, 256-bit output";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-045", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        // Append right_encode(output_bits) as required by hardware spec
        // Model will parse this and pass to EVP_MAC_final
        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-045", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 16, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-045", "OpenSSL reference computation failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KMAC128 MAC matches reference";
            report_test_pass("TC-045");
        } else {
            REG_ERROR(1, test_logger) << "Actual MAC  (first 16): " << std::hex << std::setfill('0')
                                        << std::setw(2) << (int)actual_mac[0] << " "
                                        << std::setw(2) << (int)actual_mac[1] << " "
                                        << std::setw(2) << (int)actual_mac[2] << " "
                                        << std::setw(2) << (int)actual_mac[3] << " "
                                        << std::setw(2) << (int)actual_mac[4] << " "
                                        << std::setw(2) << (int)actual_mac[5] << " "
                                        << std::setw(2) << (int)actual_mac[6] << " "
                                        << std::setw(2) << (int)actual_mac[7] << " "
                                        << std::setw(2) << (int)actual_mac[8] << " "
                                        << std::setw(2) << (int)actual_mac[9] << " "
                                        << std::setw(2) << (int)actual_mac[10] << " "
                                        << std::setw(2) << (int)actual_mac[11] << " "
                                        << std::setw(2) << (int)actual_mac[12] << " "
                                        << std::setw(2) << (int)actual_mac[13] << " "
                                        << std::setw(2) << (int)actual_mac[14] << " "
                                        << std::setw(2) << (int)actual_mac[15] << std::dec;
            REG_ERROR(1, test_logger) << "Expected MAC (first 16): " << std::hex << std::setfill('0')
                                        << std::setw(2) << (int)expected_mac[0] << " "
                                        << std::setw(2) << (int)expected_mac[1] << " "
                                        << std::setw(2) << (int)expected_mac[2] << " "
                                        << std::setw(2) << (int)expected_mac[3] << " "
                                        << std::setw(2) << (int)expected_mac[4] << " "
                                        << std::setw(2) << (int)expected_mac[5] << " "
                                        << std::setw(2) << (int)expected_mac[6] << " "
                                        << std::setw(2) << (int)expected_mac[7] << " "
                                        << std::setw(2) << (int)expected_mac[8] << " "
                                        << std::setw(2) << (int)expected_mac[9] << " "
                                        << std::setw(2) << (int)expected_mac[10] << " "
                                        << std::setw(2) << (int)expected_mac[11] << " "
                                        << std::setw(2) << (int)expected_mac[12] << " "
                                        << std::setw(2) << (int)expected_mac[13] << " "
                                        << std::setw(2) << (int)expected_mac[14] << " "
                                        << std::setw(2) << (int)expected_mac[15] << std::dec;
            report_test_fail("TC-045", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-045", "Exception occurred");
    }
}

/**
 * @brief TC-046: test_kmac_256bit_key_256bit_output
 *
 * Verifies KMAC with 256-bit key and 256-bit MAC output.
 * Tests KMAC operation with standard key length and output length.
 */
void testbench::test_kmac_256bit_key_256bit_output()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-046: test_kmac_256bit_key_256bit_output");

    try {
        const char* test_msg = "KMAC test with 256-bit key";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        // 256-bit key
        uint8_t key[32];
        for (size_t i = 0; i < 32; i++) {
            key[i] = (uint8_t)(0xB0 + (i % 16));
        }

        configure_kmac_mode(test, 0x2);  // KMAC256
        write_key_share0(test, key, 32);
        write_key_len(test, 0x2);  // 256-bit key
        write_kmac_prefix_registers(test, "CustomString", 12);

        REG_INFO(2, test_logger) << "Testing KMAC256 with 256-bit key, 256-bit output, customization string";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-046", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        // Append right_encode(output_bits) as required by hardware spec
        // Model will parse this and pass to EVP_MAC_final
        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-046", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 32, "CustomString", 12, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, true)) {
            cleanup_test(test);
            report_test_fail("TC-046", "OpenSSL reference computation failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KMAC256 MAC matches reference";
            report_test_pass("TC-046");
        } else {
            report_test_fail("TC-046", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-046", "Exception occurred");
    }
}

/**
 * @brief TC-047: test_kmac_key_length_128bit
 *
 * Verifies KMAC with KEY_LEN = 0x0 (128-bit key).
 * Tests KEY_LEN register configuration and key extraction.
 */
void testbench::test_kmac_key_length_128bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-047: test_kmac_key_length_128bit");

    try {
        const char* test_msg = "Test KEY_LEN 128-bit";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(i * 7);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);  // KEY_LEN = 0x0 (128 bits)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KEY_LEN = 0x0 (128-bit key)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-047", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-047", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 16, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-047", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KEY_LEN=0x0 MAC matches reference";
            report_test_pass("TC-047");
        } else {
            report_test_fail("TC-047", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-047", "Exception occurred");
    }
}

/**
 * @brief TC-048: test_kmac_key_length_192bit
 *
 * Verifies KMAC with KEY_LEN = 0x1 (192-bit key).
 * Tests 192-bit key configuration and extraction.
 */
void testbench::test_kmac_key_length_192bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-048: test_kmac_key_length_192bit");

    try {
        const char* test_msg = "Test KEY_LEN 192-bit";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[24];
        for (size_t i = 0; i < 24; i++) {
            key[i] = (uint8_t)(0xC0 + i);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 24);
        write_key_len(test, 0x1);  // KEY_LEN = 0x1 (192 bits)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KEY_LEN = 0x1 (192-bit key)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-048", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-048", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 24, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-048", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KEY_LEN=0x1 (192-bit) MAC matches reference";
            report_test_pass("TC-048");
        } else {
            report_test_fail("TC-048", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-048", "Exception occurred");
    }
}

/**
 * @brief TC-049: test_kmac_key_length_256bit
 *
 * Verifies KMAC with KEY_LEN = 0x2 (256-bit key).
 * Tests 256-bit key configuration and extraction.
 */
void testbench::test_kmac_key_length_256bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-049: test_kmac_key_length_256bit");

    try {
        const char* test_msg = "Test KEY_LEN 256-bit";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[32];
        for (size_t i = 0; i < 32; i++) {
            key[i] = (uint8_t)(0xD0 + (i % 16));
        }

        configure_kmac_mode(test, 0x2);  // KMAC256
        write_key_share0(test, key, 32);
        write_key_len(test, 0x2);  // KEY_LEN = 0x2 (256 bits)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KEY_LEN = 0x2 (256-bit key)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-049", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-049", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 32, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, true)) {
            cleanup_test(test);
            report_test_fail("TC-049", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KEY_LEN=0x2 (256-bit) MAC matches reference";
            report_test_pass("TC-049");
        } else {
            report_test_fail("TC-049", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-049", "Exception occurred");
    }
}

/**
 * @brief TC-050: test_kmac_key_length_384bit
 *
 * Verifies KMAC with KEY_LEN = 0x3 (384-bit key).
 * Tests 384-bit key configuration and extraction.
 */
void testbench::test_kmac_key_length_384bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-050: test_kmac_key_length_384bit");

    try {
        const char* test_msg = "Test KEY_LEN 384-bit";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[48];
        for (size_t i = 0; i < 48; i++) {
            key[i] = (uint8_t)(0xE0 + (i % 16));
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 48);
        write_key_len(test, 0x3);  // KEY_LEN = 0x3 (384 bits)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KEY_LEN = 0x3 (384-bit key)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-050", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-050", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 48, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-050", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KEY_LEN=0x3 (384-bit) MAC matches reference";
            report_test_pass("TC-050");
        } else {
            report_test_fail("TC-050", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-050", "Exception occurred");
    }
}

/**
 * @brief TC-051: test_kmac_key_length_512bit
 *
 * Verifies KMAC with KEY_LEN = 0x4 (512-bit key).
 * Tests maximum key length configuration and extraction.
 */
void testbench::test_kmac_key_length_512bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-051: test_kmac_key_length_512bit");

    try {
        const char* test_msg = "Test KEY_LEN 512-bit";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[64];
        for (size_t i = 0; i < 64; i++) {
            key[i] = (uint8_t)(0xF0 + (i % 16));
        }

        configure_kmac_mode(test, 0x2);  // KMAC256
        write_key_share0(test, key, 64);
        write_key_len(test, 0x4);  // KEY_LEN = 0x4 (512 bits)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KEY_LEN = 0x4 (512-bit key - maximum)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-051", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-051", "Error during KMAC operation");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 64, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, true)) {
            cleanup_test(test);
            report_test_fail("TC-051", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "KEY_LEN=0x4 (512-bit) MAC matches reference";
            report_test_pass("TC-051");
        } else {
            report_test_fail("TC-051", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-051", "Exception occurred");
    }
}

/**
 * @brief TC-052: test_kmac_prefix_validation
 *
 * Verifies PREFIX_0 validation for encode_string("KMAC") = 0x01 0x20 0x4B 0x4D 0x41 0x43.
 * Tests that hardware accepts correct PREFIX format during START command.
 */
void testbench::test_kmac_prefix_validation()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-052: test_kmac_prefix_validation");

    try {
        const char* test_msg = "PREFIX validation test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(i * 11);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);  // 128-bit key

        // Write correct PREFIX starting with encode_string("KMAC")
        write_kmac_prefix_registers(test, "TestCustomization", 17);

        REG_INFO(2, test_logger) << "Testing PREFIX validation with correct encode_string(\"KMAC\")";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-052", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START

        // Check no error after START (PREFIX should be valid)
        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-052", "PREFIX validation failed - error detected");
            return;
        }

        REG_INFO(2, test_logger) << "PREFIX validation passed (no error after START)";

        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-052", "Error during PROCESS");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        cleanup_test(test);

        REG_INFO(2, test_logger) << "PREFIX validation test passed - correct encode_string(\"KMAC\") accepted";
        report_test_pass("TC-052");

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-052", "Exception occurred");
    }
}

/**
 * @brief TC-053: test_kmac_incorrect_function_name_error
 *
 * Verifies IncorrectFunctionName error (0x07) when PREFIX does not match "KMAC".
 * Tests that hardware sets error code when PREFIX is incorrectly formatted.
 */
void testbench::test_kmac_incorrect_function_name_error()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-053: test_kmac_incorrect_function_name_error");

    try {
        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(0xAA);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);  // 128-bit key

        // Write INCORRECT PREFIX (not starting with encode_string("KMAC"))
        // Use arbitrary data that doesn't match 0x01 0x20 0x4B 0x4D 0x41 0x43
        const uint32_t PREFIX_BASE = test->PREFIX_0_OFFSET;
        test->register_write_32(PREFIX_BASE + 0, 0xDEADBEEF);  // Incorrect PREFIX_0
        wait(2, SC_NS);
        test->register_write_32(PREFIX_BASE + 4, 0xCAFEBABE);
        wait(2, SC_NS);
        for (size_t i = 2; i < 11; i++) {
            test->register_write_32(PREFIX_BASE + i * 4, 0);
            wait(2, SC_NS);
        }

        REG_INFO(2, test_logger) << "Testing PREFIX validation with INCORRECT format";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-053", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START

        // Check that IncorrectFunctionName error is set
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        uint8_t error_type = (err_code >> 24) & 0xFF;

        if (error_type == 0x07) {
            REG_INFO(2, test_logger) << "IncorrectFunctionName error (0x07) correctly detected";
            REG_INFO(2, test_logger) << "ERR_CODE = 0x" << std::hex << err_code << std::dec;

            // Check that kmac_err interrupt bit is set
            uint32_t intr_state = 0;
            test->register_read_32(test->INTR_STATE_OFFSET, intr_state);
            bool kmac_err = (intr_state & 0x4) != 0;

            if (kmac_err) {
                REG_INFO(2, test_logger) << "INTR_STATE.kmac_err correctly asserted";
            }

            cleanup_test(test);
            report_test_pass("TC-053");
        } else {
            REG_ERROR(1, test_logger) << "Expected error 0x07, got 0x" << std::hex << (int)error_type << std::dec;
            cleanup_test(test);
            report_test_fail("TC-053", "IncorrectFunctionName error not detected");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-053", "Exception occurred");
    }
}

/**
 * @brief TC-054: test_kmac_output_length_encoding
 *
 * Verifies right_encode(output_length_bits) appended to message.
 * Tests that software must correctly append right_encode before PROCESS command.
 */
void testbench::test_kmac_output_length_encoding()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-054: test_kmac_output_length_encoding");

    try {
        const char* test_msg = "Output length encoding test";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 512;  // Test with non-standard output length
        const size_t output_bytes = 64;

        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(0x55);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing right_encode(" << output_bits << ") appended to message";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-054", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        // Construct right_encode(512) manually
        // 512 = 0x0200, needs 2 bytes
        // right_encode(512) = 0x02 0x00 0x02 (value in 2 bytes + length byte)
        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);

        REG_INFO(2, test_logger) << "right_encode(" << output_bits << ") = "
                                  << right_enc_len << " bytes";

        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-054", "Error during operation");
            return;
        }

        uint8_t actual_mac[64];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[64];
        if (!compute_kmac_reference(key, 16, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-054", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "Output length encoding test passed - MAC matches";
            report_test_pass("TC-054");
        } else {
            report_test_fail("TC-054", "MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-054", "Exception occurred");
    }
}

/**
 * @brief TC-055: test_kmac_extended_output
 *
 * Verifies KMAC extended output (MAC longer than rate size) using RUN command.
 * Tests that RUN command produces additional output blocks for extended MAC.
 */
void testbench::test_kmac_extended_output()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-055: test_kmac_extended_output");

    try {
        const char* test_msg = "Extended output test";
        const size_t msg_len = strlen(test_msg);
        const size_t block1_bytes = 32;
        const size_t total_output_bits = 512;
        const size_t total_output_bytes = 64;

        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(0x77);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128 (rate=168 bytes)
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KMAC extended output (512 bits) using RUN command";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-055", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(total_output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-055", "Error after PROCESS");
            return;
        }

        // Read first block
        uint8_t actual_output[64];
        REG_INFO(2, test_logger) << "Reading first block (256 bits)";
        read_state_digest(test, actual_output, block1_bytes);

        // Issue RUN command for extended output
        REG_INFO(2, test_logger) << "Issuing RUN command for extended output";
        write_cmd(test, 0x31);  // RUN

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-055", "Error after RUN command");
            return;
        }

        // Read second block
        REG_INFO(2, test_logger) << "Reading second block (256 bits)";
        read_state_digest(test, actual_output + block1_bytes, block1_bytes);

        uint8_t expected_output[64];
        if (!compute_kmac_reference(key, 16, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     total_output_bits, expected_output, total_output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-055", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < total_output_bytes; i++) {
            if (actual_output[i] != expected_output[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "Extended output test passed - 512-bit MAC matches";
            report_test_pass("TC-055");
        } else {
            report_test_fail("TC-055", "Extended output mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-055", "Exception occurred");
    }
}

/**
 * @brief TC-172: test_corner_empty_message_kmac
 *
 * Verifies KMAC with empty message (only right_encode(output_length) written).
 * Tests corner case of KMAC authentication with no message data.
 */
void testbench::test_corner_empty_message_kmac()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-172: test_corner_empty_message_kmac");

    try {
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)(0x99);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KMAC with empty message";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-172", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START

        // No message data written - only right_encode(output_length)
        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-172", "Error during empty message KMAC");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 16, NULL, 0, NULL, 0,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-172", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "Empty message KMAC test passed";
            report_test_pass("TC-172");
        } else {
            report_test_fail("TC-172", "Empty message MAC mismatch");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-172", "Exception occurred");
    }
}

/**
 * @brief TC-177: test_corner_maximum_key_length_512bit
 *
 * Verifies KMAC operation with maximum key length 512 bits.
 * Tests corner case of KMAC with maximum supported key length.
 */
void testbench::test_corner_maximum_key_length_512bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-177: test_corner_maximum_key_length_512bit");

    try {
        const char* test_msg = "Maximum key length corner case";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        // 512-bit key (maximum)
        uint8_t key[64];
        for (size_t i = 0; i < 64; i++) {
            key[i] = (uint8_t)(i & 0xFF);
        }

        configure_kmac_mode(test, 0x2);  // KMAC256
        write_key_share0(test, key, 64);
        write_key_len(test, 0x4);  // KEY_LEN = 0x4 (512 bits - maximum)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KMAC with MAXIMUM key length (512 bits)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-177", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-177", "Error with maximum key length");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 64, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, true)) {
            cleanup_test(test);
            report_test_fail("TC-177", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "Maximum key length (512-bit) test passed";
            report_test_pass("TC-177");
        } else {
            report_test_fail("TC-177", "MAC mismatch with maximum key");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-177", "Exception occurred");
    }
}

/**
 * @brief TC-178: test_corner_minimum_key_length_128bit
 *
 * Verifies KMAC operation with minimum key length 128 bits.
 * Tests corner case of KMAC with minimum supported key length.
 */
void testbench::test_corner_minimum_key_length_128bit()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-178: test_corner_minimum_key_length_128bit");

    try {
        const char* test_msg = "Minimum key length corner case";
        const size_t msg_len = strlen(test_msg);
        const size_t output_bits = 256;
        const size_t output_bytes = 32;

        // 128-bit key (minimum)
        uint8_t key[16];
        for (size_t i = 0; i < 16; i++) {
            key[i] = (uint8_t)((i * 17) & 0xFF);
        }

        configure_kmac_mode(test, 0x0);  // KMAC128
        write_key_share0(test, key, 16);
        write_key_len(test, 0x0);  // KEY_LEN = 0x0 (128 bits - minimum)
        write_kmac_prefix_registers(test, NULL, 0);

        REG_INFO(2, test_logger) << "Testing KMAC with MINIMUM key length (128 bits)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-178", "Not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);  // START
        write_msg_fifo(test, (const uint8_t*)test_msg, msg_len);

        uint8_t right_enc[8];
        size_t right_enc_len = 0;
        right_encode(output_bits, right_enc, right_enc_len);
        write_msg_fifo(test, right_enc, right_enc_len);

        write_cmd(test, 0x2E);  // PROCESS

        if (!verify_no_error(test)) {
            cleanup_test(test);
            report_test_fail("TC-178", "Error with minimum key length");
            return;
        }

        uint8_t actual_mac[32];
        read_state_digest(test, actual_mac, output_bytes);

        uint8_t expected_mac[32];
        if (!compute_kmac_reference(key, 16, NULL, 0, (const uint8_t*)test_msg, msg_len,
                                     output_bits, expected_mac, output_bytes, false)) {
            cleanup_test(test);
            report_test_fail("TC-178", "OpenSSL reference failed");
            return;
        }

        bool match = true;
        for (size_t i = 0; i < output_bytes; i++) {
            if (actual_mac[i] != expected_mac[i]) {
                match = false;
                break;
            }
        }

        cleanup_test(test);

        if (match) {
            REG_INFO(2, test_logger) << "Minimum key length (128-bit) test passed";
            report_test_pass("TC-178");
        } else {
            report_test_fail("TC-178", "MAC mismatch with minimum key");
        }

    } catch (const std::exception& e) {
        REG_ERROR(1, test_logger) << "Test exception: " << e.what();
        report_test_fail("TC-178", "Exception occurred");
    }
}
