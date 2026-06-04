/******************************************************************************
 * @file kmac_func012_test.h
 * @brief Test declarations for FUNC-KMAC-012 (Endianness Configuration)
 *
 * This header declares test functions for FUNC-KMAC-012 implementation,
 * verifying configurable byte-order transformation for message input and
 * digest output via CFG_SHADOWED register fields.
 *
 * Functionality Coverage:
 * - msg_endianness configuration (bit 6 of CFG_SHADOWED)
 * - state_endianness configuration (bit 7 of CFG_SHADOWED)
 * - Little-endian mode (value = 0): no byte swapping
 * - Big-endian mode (value = 1): 32-bit word-granularity byte swap
 * - Independent operation of msg_endianness and state_endianness
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Functionality Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"

// =============================================================================
// FUNC-KMAC-012 Test Function Declarations
// =============================================================================

/**
 * @brief TC-024: SHA3 message endianness little-endian test
 *
 * Verifies SHA3 operation with msg_endianness = 0 (little-endian).
 * Message data is written to MSG_FIFO without byte swapping.
 *
 * Test Sequence:
 * 1. Configure CFG_SHADOWED with mode=SHA3, kstrength=L256, msg_endianness=0
 * 2. Issue START command
 * 3. Write known message to MSG_FIFO (e.g., "abc")
 * 4. Issue PROCESS command
 * 5. Read digest from STATE window
 * 6. Compare against OpenSSL EVP_sha3_256("abc") reference
 *
 * Pass Criteria:
 * - Digest matches OpenSSL reference (no byte swapping on input)
 * - Message absorbed in little-endian byte order
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_msg_endianness_little(kmac_test* test);

/**
 * @brief TC-025: SHA3 message endianness big-endian test
 *
 * Verifies SHA3 operation with msg_endianness = 1 (big-endian byte-swap).
 * Each 32-bit word written to MSG_FIFO undergoes byte swap before absorption.
 *
 * Test Sequence:
 * 1. Configure CFG_SHADOWED with mode=SHA3, kstrength=L256, msg_endianness=1
 * 2. Issue START command
 * 3. Write known message to MSG_FIFO (e.g., "abcd")
 * 4. Issue PROCESS command
 * 5. Read digest from STATE window
 * 6. Compare against reference with manual byte-swap applied to input
 *
 * Pass Criteria:
 * - Digest reflects byte-swapped message input
 * - Word-granularity (32-bit) byte swap correctly applied
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_msg_endianness_big(kmac_test* test);

/**
 * @brief TC-026: SHA3 state endianness little-endian test
 *
 * Verifies STATE window read with state_endianness = 0 (little-endian).
 * Digest is returned in native little-endian byte order (no swap).
 *
 * Test Sequence:
 * 1. Configure CFG_SHADOWED with mode=SHA3, kstrength=L256, state_endianness=0
 * 2. Execute complete SHA3-256 hash operation
 * 3. Read digest from STATE window
 * 4. Verify digest byte order matches OpenSSL native output
 *
 * Pass Criteria:
 * - Digest bytes in little-endian order (no transformation)
 * - Matches OpenSSL EVP_DigestFinal_ex output directly
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_state_endianness_little(kmac_test* test);

/**
 * @brief TC-027: SHA3 state endianness big-endian test
 *
 * Verifies STATE window read with state_endianness = 1 (big-endian byte-swap).
 * Each 32-bit word in the digest undergoes byte swap before return.
 *
 * Test Sequence:
 * 1. Configure CFG_SHADOWED with mode=SHA3, kstrength=L256, state_endianness=1
 * 2. Execute complete SHA3-256 hash operation
 * 3. Read digest from STATE window
 * 4. Apply software byte-swap to each 32-bit word
 * 5. Compare swapped result against OpenSSL reference
 *
 * Pass Criteria:
 * - Each 32-bit word in digest is byte-swapped relative to OpenSSL output
 * - Word-granularity byte swap correctly applied
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_state_endianness_big(kmac_test* test);

/**
 * @brief TC-142: State endianness word granularity test
 *
 * Verifies that state_endianness performs byte swap on 32-bit word granularity,
 * not byte-by-byte or 64-bit word granularity.
 *
 * Test Sequence:
 * 1. Execute SHA3-256 hash with state_endianness=0
 * 2. Read first 32-bit word of digest: W0_LE
 * 3. Execute same hash with state_endianness=1
 * 4. Read first 32-bit word of digest: W0_BE
 * 5. Verify W0_BE = byte_swap_32(W0_LE)
 *
 * Pass Criteria:
 * - Byte swap operates on 32-bit word boundaries
 * - Byte[0] ↔ Byte[3], Byte[1] ↔ Byte[2] within each word
 * - Adjacent words maintain independent swap
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_endianness_word_granularity(kmac_test* test);

/**
 * @brief TC-143: Message and state endianness independence test
 *
 * Verifies that msg_endianness and state_endianness operate independently.
 * All four combinations should produce consistent results.
 *
 * Test Sequence:
 * 1. Run SHA3-256 hash with all four combinations:
 *    - (msg=0, state=0): baseline
 *    - (msg=1, state=0): input swapped
 *    - (msg=0, state=1): output swapped
 *    - (msg=1, state=1): both swapped
 * 2. Verify (msg=0, state=0) and (msg=1, state=1) produce equivalent digests
 *    after accounting for input/output swap
 * 3. Verify (msg=1, state=0) differs from (msg=0, state=0)
 * 4. Verify (msg=0, state=1) differs from (msg=0, state=0)
 *
 * Pass Criteria:
 * - msg_endianness does not affect state_endianness behavior
 * - state_endianness does not affect msg_endianness behavior
 * - Both settings can be changed independently
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_msg_endianness_independent(kmac_test* test);

