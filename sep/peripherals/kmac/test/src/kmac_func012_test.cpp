// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_func012_test.cpp
 * @brief Test cases for FUNC-KMAC-012 (Endianness Configuration)
 *
 * This file implements test cases for FUNC-KMAC-012, verifying configurable
 * byte-order transformation for message input and digest output via CFG_SHADOWED
 * register fields (msg_endianness bit 6, state_endianness bit 7).
 *
 * Implementation Coverage:
 * - TC-024: test_sha3_msg_endianness_little
 * - TC-025: test_sha3_msg_endianness_big
 * - TC-026: test_sha3_state_endianness_little
 * - TC-027: test_sha3_state_endianness_big
 * - TC-142: test_state_endianness_word_granularity
 * - TC-143: test_state_msg_endianness_independent
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
 * Architecture Reference: kmac-architecture-behaviour-map.json
 * Detailed Design: kmac-detailed-design.md
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "testbench.h"
#include "reg_logger.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <openssl/evp.h>

// Logger for test output
static RegLogger test_logger;

/******************************************************************************
 * Helper Functions
 ******************************************************************************/

/**
 * @brief Helper function to configure CFG_SHADOWED for SHA3 with endianness
 * @param test Pointer to test harness
 * @param kstrength Keccak strength (0x2 for L256)
 * @param msg_endianness Message endianness (0=little, 1=big)
 * @param state_endianness State endianness (0=little, 1=big)
 *
 * CFG_SHADOWED register format:
 * - Bits [3:1] = kstrength
 * - Bits [5:4] = mode (0x0 for SHA3)
 * - Bit [0] = kmac_en (0 for SHA3)
 * - Bit [6] = msg_endianness
 * - Bit [7] = state_endianness
 */
static void configure_sha3_with_endianness(kmac_test* test, uint32_t kstrength,
                                           uint32_t msg_endianness, uint32_t state_endianness)
{
    uint32_t cfg_val = (0 << 0) |                          // kmac_en=0
                       ((kstrength & 0x7) << 1) |          // kstrength
                       (0x0 << 4) |                        // mode=SHA3
                       ((msg_endianness & 0x1) << 8) |     // msg_endianness (bit 8, not 6)
                       ((state_endianness & 0x1) << 9) |   // state_endianness (bit 9, not 7)
                       (0x1 << 16) |                       // entropy_mode=0x1 - REQUIRED
                       (0x1 << 24);                        // entropy_ready=1 - REQUIRED

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to read STATUS register FSM state bits
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
 */
static void write_cmd(kmac_test* test, uint32_t cmd_value)
{
    test->register_write_32(test->CMD_OFFSET, cmd_value);
    wait(10, SC_NS);
}

/**
 * @brief Helper function to write message to MSG_FIFO
 */
static void write_msg_fifo(kmac_test* test, const uint8_t* msg, size_t len)
{
    const uint32_t MSG_FIFO_BASE = 0x800;

    // Write complete 32-bit words using word writes (needed for msg_endianness byte swap)
    size_t complete_words = len / 4;
    for (size_t i = 0; i < complete_words; i++) {
        size_t k = i * 4;
        uint32_t word = (static_cast<uint32_t>(msg[k + 0]) << 0) |
                        (static_cast<uint32_t>(msg[k + 1]) << 8) |
                        (static_cast<uint32_t>(msg[k + 2]) << 16) |
                        (static_cast<uint32_t>(msg[k + 3]) << 24);
        test->register_write_32(MSG_FIFO_BASE, word);
        wait(5, SC_NS);
    }

    // Write remaining bytes (partial last word) using byte writes
    // This ensures only actual message bytes are absorbed, not zeros
    for (size_t i = complete_words * 4; i < len; i++) {
        test->register_write_8(MSG_FIFO_BASE, msg[i]);
        wait(2, SC_NS);
    }
}

/**
 * @brief Helper function to read digest from STATE window
 */
static void read_state_digest(kmac_test* test, uint8_t* digest, size_t len)
{
    const uint32_t STATE_BASE = 0x400;
    const uint32_t SHARE1_OFFSET = 0x100;  // share1 starts at 0x500 (0x400 + 0x100)

    // When EnMasking=1, actual digest = share0 XOR share1
    // Read both shares and XOR them to recover the actual digest
    for (size_t i = 0; i < len; i += 4) {
        uint32_t share0_word = 0;
        uint32_t share1_word = 0;
        test->register_read_32(STATE_BASE + i, share0_word);
        test->register_read_32(STATE_BASE + SHARE1_OFFSET + i, share1_word);
        
        // XOR to get actual digest word
        uint32_t digest_word = share0_word ^ share1_word;

        for (size_t j = 0; j < 4 && (i + j) < len; j++) {
            digest[i + j] = static_cast<uint8_t>((digest_word >> (j * 8)) & 0xFF);
        }
    }
}

/**
 * @brief Helper function to compute SHA3-256 reference using OpenSSL
 */
static bool compute_sha3_256_reference(const uint8_t* msg, size_t len, uint8_t* digest)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;

    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestUpdate(ctx, msg, len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    unsigned int digest_len = 32;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    return true;
}

/**
 * @brief Helper function to perform 32-bit word byte swap
 */
static uint32_t byte_swap_32(uint32_t word)
{
    return ((word & 0x000000FF) << 24) |
           ((word & 0x0000FF00) << 8) |
           ((word & 0x00FF0000) >> 8) |
           ((word & 0xFF000000) >> 24);
}

/**
 * @brief Helper function to byte-swap a buffer at 32-bit word granularity
 */
static void byte_swap_buffer(uint8_t* buffer, size_t len)
{
    uint32_t* words = reinterpret_cast<uint32_t*>(buffer);
    size_t num_words = len / 4;

    for (size_t i = 0; i < num_words; i++) {
        words[i] = byte_swap_32(words[i]);
    }
}

/**
 * @brief Helper function to compare two buffers
 */
static bool compare_buffers(const uint8_t* buf1, const uint8_t* buf2, size_t len)
{
    return std::memcmp(buf1, buf2, len) == 0;
}

/**
 * @brief Helper function to clean up after test
 */
static void cleanup_test(kmac_test* test)
{
    bool idle, absorb, squeeze;
    read_status_fsm_bits(test, idle, absorb, squeeze);

    if (!idle) {
        uint32_t err_code = 0;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);

        if (err_code != 0) {
            write_cmd(test, 0x400); // err_processed
        } else {
            write_cmd(test, 0x16); // DONE
        }
        wait(10, SC_NS);
    }
}

/**
 * @brief Helper function to print buffer in hex
 */
static void print_hex_buffer(RegLogger& logger, const char* label, const uint8_t* buf, size_t len)
{
    std::ostringstream oss;
    oss << label << ": ";
    for (size_t i = 0; i < len; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(buf[i]);
        if (i < len - 1) oss << " ";
    }
    REG_INFO(2, logger) << oss.str();
}

/******************************************************************************
 * TC-024: SHA3 Message Endianness Little-Endian Test
 *
 * Verifies SHA3 operation with msg_endianness = 0 (little-endian).
 * Message data written to MSG_FIFO without byte swapping.
 ******************************************************************************/
void testbench::test_sha3_msg_endianness_little()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-024: test_sha3_msg_endianness_little");

    try {
        // Test message: "abc"
        const uint8_t msg[] = {'a', 'b', 'c'};
        const size_t msg_len = sizeof(msg);
        uint8_t digest[32] = {0};
        uint8_t reference[32] = {0};

        // Configure SHA3-256 with msg_endianness=0 (little-endian)
        configure_sha3_with_endianness(test, 0x2, 0, 0);
        REG_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3-256, msg_endianness=0 (little)";

        // Verify IDLE state
        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-024", "FSM not in IDLE state");
            return;
        }

        // Issue START command
        write_cmd(test, 0x1D);
        REG_INFO(2, test_logger) << "Issued START command";

        // Write message to MSG_FIFO
        write_msg_fifo(test, msg, msg_len);
        REG_INFO(2, test_logger) << "Wrote message to MSG_FIFO: \"abc\"";

        // Issue PROCESS command
        write_cmd(test, 0x2E);
        REG_INFO(2, test_logger) << "Issued PROCESS command";

        // Wait for SQUEEZE state
        wait(10, SC_NS);
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-024", "FSM not in SQUEEZE state");
            return;
        }

        // Read digest from STATE window
        read_state_digest(test, digest, 32);
        print_hex_buffer(test_logger, "KMAC digest", digest, 32);

        // Compute OpenSSL reference
        if (!compute_sha3_256_reference(msg, msg_len, reference)) {
            cleanup_test(test);
            report_test_fail("TC-024", "OpenSSL reference computation failed");
            return;
        }
        print_hex_buffer(test_logger, "OpenSSL reference", reference, 32);

        // Compare digests
        if (!compare_buffers(digest, reference, 32)) {
            cleanup_test(test);
            report_test_fail("TC-024", "Digest mismatch with OpenSSL reference");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-024: test_sha3_msg_endianness_little");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-024", e.what());
    }
}

/******************************************************************************
 * TC-025: SHA3 Message Endianness Big-Endian Test
 *
 * Verifies SHA3 operation with msg_endianness = 1 (big-endian byte-swap).
 * Each 32-bit word written to MSG_FIFO undergoes byte swap before absorption.
 ******************************************************************************/
void testbench::test_sha3_msg_endianness_big()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-025: test_sha3_msg_endianness_big");

    try {
        // Test message: "abcd" (4 bytes = 1 full 32-bit word)
        const uint8_t msg[] = {'a', 'b', 'c', 'd'};
        const size_t msg_len = sizeof(msg);
        uint8_t digest[32] = {0};
        uint8_t reference[32] = {0};

        // Configure SHA3-256 with msg_endianness=1 (big-endian)
        configure_sha3_with_endianness(test, 0x2, 1, 0);
        REG_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3-256, msg_endianness=1 (big)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-025", "FSM not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        REG_INFO(2, test_logger) << "Wrote message to MSG_FIFO: \"abcd\"";

        write_cmd(test, 0x2E);
        wait(10, SC_NS);

        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!squeeze) {
            cleanup_test(test);
            report_test_fail("TC-025", "FSM not in SQUEEZE state");
            return;
        }

        read_state_digest(test, digest, 32);
        print_hex_buffer(test_logger, "KMAC digest", digest, 32);

        // Create byte-swapped reference message
        uint8_t swapped_msg[4];
        std::memcpy(swapped_msg, msg, 4);
        byte_swap_buffer(swapped_msg, 4);
        print_hex_buffer(test_logger, "Byte-swapped message", swapped_msg, 4);

        // Compute OpenSSL reference with swapped message
        if (!compute_sha3_256_reference(swapped_msg, msg_len, reference)) {
            cleanup_test(test);
            report_test_fail("TC-025", "OpenSSL reference computation failed");
            return;
        }
        print_hex_buffer(test_logger, "OpenSSL reference", reference, 32);

        if (!compare_buffers(digest, reference, 32)) {
            cleanup_test(test);
            report_test_fail("TC-025", "Digest mismatch with byte-swapped reference");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-025: test_sha3_msg_endianness_big");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-025", e.what());
    }
}

/******************************************************************************
 * TC-026: SHA3 State Endianness Little-Endian Test
 *
 * Verifies STATE window read with state_endianness = 0 (little-endian).
 * Digest returned in native little-endian byte order (no swap).
 ******************************************************************************/
void testbench::test_sha3_state_endianness_little()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-026: test_sha3_state_endianness_little");

    try {
        const uint8_t msg[] = {'a', 'b', 'c'};
        const size_t msg_len = sizeof(msg);
        uint8_t digest[32] = {0};
        uint8_t reference[32] = {0};

        // Configure SHA3-256 with state_endianness=0 (little-endian)
        configure_sha3_with_endianness(test, 0x2, 0, 0);
        REG_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3-256, state_endianness=0 (little)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-026", "FSM not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);

        read_state_digest(test, digest, 32);
        print_hex_buffer(test_logger, "KMAC digest (state_endianness=0)", digest, 32);

        if (!compute_sha3_256_reference(msg, msg_len, reference)) {
            cleanup_test(test);
            report_test_fail("TC-026", "OpenSSL reference computation failed");
            return;
        }
        print_hex_buffer(test_logger, "OpenSSL reference", reference, 32);

        if (!compare_buffers(digest, reference, 32)) {
            cleanup_test(test);
            report_test_fail("TC-026", "Digest mismatch - state_endianness should be 0");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-026: test_sha3_state_endianness_little");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-026", e.what());
    }
}

/******************************************************************************
 * TC-027: SHA3 State Endianness Big-Endian Test
 *
 * Verifies STATE window read with state_endianness = 1 (big-endian byte-swap).
 * Each 32-bit word in digest undergoes byte swap before return.
 ******************************************************************************/
void testbench::test_sha3_state_endianness_big()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-027: test_sha3_state_endianness_big");

    try {
        const uint8_t msg[] = {'a', 'b', 'c'};
        const size_t msg_len = sizeof(msg);
        uint8_t digest[32] = {0};
        uint8_t reference[32] = {0};
        uint8_t swapped_digest[32] = {0};

        // Configure SHA3-256 with state_endianness=1 (big-endian)
        configure_sha3_with_endianness(test, 0x2, 0, 1);
        REG_INFO(2, test_logger) << "Configured CFG_SHADOWED: mode=SHA3-256, state_endianness=1 (big)";

        bool idle, absorb, squeeze;
        read_status_fsm_bits(test, idle, absorb, squeeze);
        if (!idle) {
            cleanup_test(test);
            report_test_fail("TC-027", "FSM not in IDLE state");
            return;
        }

        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);

        read_state_digest(test, digest, 32);
        print_hex_buffer(test_logger, "KMAC digest (state_endianness=1)", digest, 32);

        // Compute OpenSSL reference
        if (!compute_sha3_256_reference(msg, msg_len, reference)) {
            cleanup_test(test);
            report_test_fail("TC-027", "OpenSSL reference computation failed");
            return;
        }

        // Apply byte swap to reference for comparison
        std::memcpy(swapped_digest, reference, 32);
        byte_swap_buffer(swapped_digest, 32);
        print_hex_buffer(test_logger, "OpenSSL reference (byte-swapped)", swapped_digest, 32);

        if (!compare_buffers(digest, swapped_digest, 32)) {
            cleanup_test(test);
            report_test_fail("TC-027", "Digest mismatch - state_endianness byte swap not applied");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-027: test_sha3_state_endianness_big");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-027", e.what());
    }
}

/******************************************************************************
 * TC-142: State Endianness Word Granularity Test
 *
 * Verifies that state_endianness performs byte swap on 32-bit word granularity,
 * not byte-by-byte or 64-bit word granularity.
 ******************************************************************************/
void testbench::test_state_endianness_word_granularity()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-142: test_state_endianness_word_granularity");

    try {
        const uint8_t msg[] = {'t', 'e', 's', 't'};
        const size_t msg_len = sizeof(msg);
        uint32_t word0_le = 0;
        uint32_t word0_be = 0;
        const uint32_t STATE_BASE = 0x400;
        const uint32_t SHARE1_OFFSET = 0x100;  // share1 at 0x500

        // Test 1: state_endianness=0 (little-endian)
        configure_sha3_with_endianness(test, 0x2, 0, 0);
        REG_INFO(2, test_logger) << "Test 1: state_endianness=0";

        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);

        // Read first 32-bit word of digest (XOR share0 and share1 for EnMasking=1)
        uint32_t share0_le = 0, share1_le = 0;
        test->register_read_32(STATE_BASE, share0_le);
        test->register_read_32(STATE_BASE + SHARE1_OFFSET, share1_le);
        word0_le = share0_le ^ share1_le;
        REG_INFO(2, test_logger) << "First word (LE): 0x" << std::hex << word0_le;

        cleanup_test(test);
        wait(10, SC_NS);

        // Test 2: state_endianness=1 (big-endian)
        configure_sha3_with_endianness(test, 0x2, 0, 1);
        REG_INFO(2, test_logger) << "Test 2: state_endianness=1";

        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);

        // Read first 32-bit word of digest (XOR share0 and share1 for EnMasking=1)
        uint32_t share0_be = 0, share1_be = 0;
        test->register_read_32(STATE_BASE, share0_be);
        test->register_read_32(STATE_BASE + SHARE1_OFFSET, share1_be);
        word0_be = share0_be ^ share1_be;
        REG_INFO(2, test_logger) << "First word (BE): 0x" << std::hex << word0_be;

        // Verify byte swap relationship
        uint32_t word0_swapped = byte_swap_32(word0_le);
        REG_INFO(2, test_logger) << "Expected (byte_swap_32(LE)): 0x" << std::hex << word0_swapped;

        if (word0_be != word0_swapped) {
            cleanup_test(test);
            report_test_fail("TC-142", "32-bit word byte swap not correctly applied");
            return;
        }

        cleanup_test(test);
        report_test_pass("TC-142: test_state_endianness_word_granularity");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-142", e.what());
    }
}

/******************************************************************************
 * TC-143: Message and State Endianness Independence Test
 *
 * Verifies that msg_endianness and state_endianness operate independently.
 * All four combinations should produce consistent results.
 ******************************************************************************/
void testbench::test_state_msg_endianness_independent()
{
    test_logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    report_test_start("TC-143: test_state_msg_endianness_independent");

    try {
        const uint8_t msg[] = {'a', 'b', 'c', 'd'};
        const size_t msg_len = sizeof(msg);
        uint8_t digest_00[32] = {0};  // msg=0, state=0
        uint8_t digest_10[32] = {0};  // msg=1, state=0
        uint8_t digest_01[32] = {0};  // msg=0, state=1
        uint8_t digest_11[32] = {0};  // msg=1, state=1

        // Configuration 1: msg=0, state=0 (baseline)
        REG_INFO(2, test_logger) << "Configuration 1: msg_endianness=0, state_endianness=0";
        configure_sha3_with_endianness(test, 0x2, 0, 0);
        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);
        read_state_digest(test, digest_00, 32);
        print_hex_buffer(test_logger, "Digest (msg=0, state=0)", digest_00, 32);
        cleanup_test(test);
        wait(10, SC_NS);

        // Configuration 2: msg=1, state=0 (input swapped)
        REG_INFO(2, test_logger) << "Configuration 2: msg_endianness=1, state_endianness=0";
        configure_sha3_with_endianness(test, 0x2, 1, 0);
        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);
        read_state_digest(test, digest_10, 32);
        print_hex_buffer(test_logger, "Digest (msg=1, state=0)", digest_10, 32);
        cleanup_test(test);
        wait(10, SC_NS);

        // Configuration 3: msg=0, state=1 (output swapped)
        REG_INFO(2, test_logger) << "Configuration 3: msg_endianness=0, state_endianness=1";
        configure_sha3_with_endianness(test, 0x2, 0, 1);
        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);
        read_state_digest(test, digest_01, 32);
        print_hex_buffer(test_logger, "Digest (msg=0, state=1)", digest_01, 32);
        cleanup_test(test);
        wait(10, SC_NS);

        // Configuration 4: msg=1, state=1 (both swapped)
        REG_INFO(2, test_logger) << "Configuration 4: msg_endianness=1, state_endianness=1";
        configure_sha3_with_endianness(test, 0x2, 1, 1);
        write_cmd(test, 0x1D);
        write_msg_fifo(test, msg, msg_len);
        write_cmd(test, 0x2E);
        wait(10, SC_NS);
        read_state_digest(test, digest_11, 32);
        print_hex_buffer(test_logger, "Digest (msg=1, state=1)", digest_11, 32);
        cleanup_test(test);

        // Verification 1: digest_10 should differ from digest_00
        if (compare_buffers(digest_10, digest_00, 32)) {
            report_test_fail("TC-143", "msg_endianness=1 produced same result as msg_endianness=0");
            return;
        }
        REG_INFO(2, test_logger) << "PASS: msg_endianness change affects digest";

        // Verification 2: digest_01 should differ from digest_00
        if (compare_buffers(digest_01, digest_00, 32)) {
            report_test_fail("TC-143", "state_endianness=1 produced same result as state_endianness=0");
            return;
        }
        REG_INFO(2, test_logger) << "PASS: state_endianness change affects digest";

        // Verification 3: digest_01 should be byte-swapped version of digest_00
        uint8_t digest_00_swapped[32];
        std::memcpy(digest_00_swapped, digest_00, 32);
        byte_swap_buffer(digest_00_swapped, 32);
        if (!compare_buffers(digest_01, digest_00_swapped, 32)) {
            report_test_fail("TC-143", "state_endianness swap relationship incorrect");
            return;
        }
        REG_INFO(2, test_logger) << "PASS: state_endianness applies correct byte swap";

        // Verification 4: Settings operate independently
        uint8_t digest_10_swapped[32];
        std::memcpy(digest_10_swapped, digest_10, 32);
        byte_swap_buffer(digest_10_swapped, 32);
        if (!compare_buffers(digest_11, digest_10_swapped, 32)) {
            report_test_fail("TC-143", "msg_endianness and state_endianness not independent");
            return;
        }
        REG_INFO(2, test_logger) << "PASS: msg_endianness and state_endianness operate independently";

        report_test_pass("TC-143: test_state_msg_endianness_independent");

    } catch (const std::exception& e) {
        cleanup_test(test);
        report_test_fail("TC-143", e.what());
    }
}

// End of file
