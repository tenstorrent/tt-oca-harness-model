// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_func023_test.h
 * @brief Test declarations for FUNC-KMAC-023 (OpenSSL Cryptographic Delegation)
 *
 * This header declares test functions for FUNC-KMAC-023 which validates
 * complete abstraction of internal Keccak round implementation by delegating
 * all cryptographic operations to the OpenSSL library.
 *
 * Functionality Scope:
 * - SHA3 mode: EVP_sha3_224, EVP_sha3_256, EVP_sha3_384, EVP_sha3_512
 * - SHAKE mode: EVP_shake128, EVP_shake256
 * - cSHAKE implementation using EVP_shake primitives
 * - KMAC implementation using custom implementation with OpenSSL primitives
 * - EVP_DigestFinal_ex producing digest after PROCESS command
 * - EVP_DigestFinalXOF providing additional output for RUN commands
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
 * Detailed Design: kmac-detailed-design.md (Section 7.3)
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"
#include <openssl/evp.h>
#include <openssl/sha.h>

// =============================================================================
// FUNC-KMAC-023 Test Function Declarations
// =============================================================================

/**
 * @brief TC-016: Verify OpenSSL EVP_sha3_224 delegation
 *
 * Validates that KMAC model correctly delegates SHA3-224 hash computation to
 * OpenSSL EVP_sha3_224 function. Verifies CFG_SHADOWED configuration
 * (mode=0x0, kstrength=0x1) correctly maps to EVP_sha3_224 algorithm,
 * START command initializes EVP_MD_CTX context, MSG_FIFO writes trigger
 * EVP_DigestUpdate calls, and PROCESS command triggers EVP_DigestFinal_ex
 * producing correct 224-bit digest.
 *
 * Pass Criteria:
 * - STATE window contains digest matching OpenSSL EVP_sha3_224 reference
 * - Digest length equals 28 bytes (224 bits)
 * - No UnexpectedModeStrength error (ERR_CODE = 0)
 * - FSM transitions correctly: IDLE → ABSORB → SQUEEZE
 *
 * Test Vector: Input message "abc"
 * Expected SHA3-224: e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_224_openssl_delegation(kmac_test* test);

/**
 * @brief TC-017: Verify OpenSSL EVP_sha3_256 delegation
 *
 * Validates that KMAC model correctly delegates SHA3-256 hash computation to
 * OpenSSL EVP_sha3_256 function. Verifies CFG_SHADOWED configuration
 * (mode=0x0, kstrength=0x2) correctly maps to EVP_sha3_256 algorithm.
 *
 * Pass Criteria:
 * - STATE window contains digest matching OpenSSL EVP_sha3_256 reference
 * - Digest length equals 32 bytes (256 bits)
 * - No errors during operation
 * - Correct FSM state transitions
 *
 * Test Vector: Input message "abc"
 * Expected SHA3-256: 3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_256_openssl_delegation(kmac_test* test);

/**
 * @brief TC-018: Verify OpenSSL EVP_sha3_384 delegation
 *
 * Validates that KMAC model correctly delegates SHA3-384 hash computation to
 * OpenSSL EVP_sha3_384 function. Verifies CFG_SHADOWED configuration
 * (mode=0x0, kstrength=0x3) correctly maps to EVP_sha3_384 algorithm.
 *
 * Pass Criteria:
 * - STATE window contains digest matching OpenSSL EVP_sha3_384 reference
 * - Digest length equals 48 bytes (384 bits)
 * - No errors during operation
 * - Correct FSM state transitions
 *
 * Test Vector: Input message "abc"
 * Expected SHA3-384: ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_384_openssl_delegation(kmac_test* test);

/**
 * @brief TC-019: Verify OpenSSL EVP_sha3_512 delegation
 *
 * Validates that KMAC model correctly delegates SHA3-512 hash computation to
 * OpenSSL EVP_sha3_512 function. Verifies CFG_SHADOWED configuration
 * (mode=0x0, kstrength=0x4) correctly maps to EVP_sha3_512 algorithm.
 *
 * Pass Criteria:
 * - STATE window contains digest matching OpenSSL EVP_sha3_512 reference
 * - Digest length equals 64 bytes (512 bits)
 * - No errors during operation
 * - Correct FSM state transitions
 *
 * Test Vector: Input message "abc"
 * Expected SHA3-512: b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_512_openssl_delegation(kmac_test* test);

/**
 * @brief TC-029: Verify OpenSSL EVP_shake128 delegation
 *
 * Validates that KMAC model correctly delegates SHAKE128 XOF computation to
 * OpenSSL EVP_shake128 function. Verifies CFG_SHADOWED configuration
 * (mode=0x2, kstrength=0x0) correctly maps to EVP_shake128 algorithm.
 * Tests fixed 256-bit output generation.
 *
 * Pass Criteria:
 * - STATE window contains output matching OpenSSL EVP_shake128 reference
 * - Output length equals 32 bytes (256 bits) after single PROCESS
 * - No errors during operation
 * - FSM enters SQUEEZE state allowing additional output via RUN
 *
 * Test Vector: Input message "abc", output length 32 bytes
 * Expected SHAKE128: 5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8
 *
 * @param test Pointer to KMAC test harness
 */
void test_shake128_openssl_delegation(kmac_test* test);

/**
 * @brief TC-030: Verify OpenSSL EVP_shake256 delegation
 *
 * Validates that KMAC model correctly delegates SHAKE256 XOF computation to
 * OpenSSL EVP_shake256 function. Verifies CFG_SHADOWED configuration
 * (mode=0x2, kstrength=0x2) correctly maps to EVP_shake256 algorithm.
 * Tests fixed 512-bit output generation.
 *
 * Pass Criteria:
 * - STATE window contains output matching OpenSSL EVP_shake256 reference
 * - Output length equals 64 bytes (512 bits) after single PROCESS
 * - No errors during operation
 * - FSM enters SQUEEZE state
 *
 * Test Vector: Input message "abc", output length 64 bytes
 * Expected SHAKE256: 483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4
 *
 * @param test Pointer to KMAC test harness
 */
void test_shake256_openssl_delegation(kmac_test* test);

/**
 * @brief TC-038: Verify cSHAKE128 using EVP_shake primitives
 *
 * Validates that KMAC model implements cSHAKE128 using OpenSSL EVP_shake128
 * primitives with custom prefix handling. Tests empty customization string
 * (functionally equivalent to SHAKE128 but with different padding).
 *
 * Pass Criteria:
 * - Output matches reference cSHAKE128 calculation with empty N and S
 * - PREFIX register values (all zeros) correctly processed
 * - CFG_SHADOWED (mode=0x3, kstrength=0x0) maps to cSHAKE128 implementation
 * - Padding uses 00 suffix pattern instead of SHAKE's 1111 pattern
 *
 * Test Vector: Input message "abc", N="", S="", output length 32 bytes
 *
 * @param test Pointer to KMAC test harness
 */
void test_cshake128_openssl_primitives(kmac_test* test);

/**
 * @brief TC-039: Verify cSHAKE256 using EVP_shake primitives
 *
 * Validates that KMAC model implements cSHAKE256 using OpenSSL EVP_shake256
 * primitives with custom prefix handling. Tests empty customization string.
 *
 * Pass Criteria:
 * - Output matches reference cSHAKE256 calculation with empty N and S
 * - PREFIX register values correctly processed
 * - CFG_SHADOWED (mode=0x3, kstrength=0x2) maps to cSHAKE256 implementation
 * - Padding uses 00 suffix pattern
 *
 * Test Vector: Input message "abc", N="", S="", output length 64 bytes
 *
 * @param test Pointer to KMAC test harness
 */
void test_cshake256_openssl_primitives(kmac_test* test);

/**
 * @brief TC-045: Verify KMAC with 128-bit key using custom OpenSSL implementation
 *
 * Validates that KMAC model implements KMAC128 using custom implementation
 * built on OpenSSL EVP_shake128 primitives. Tests 128-bit key, KMAC prefix
 * validation, key block construction, and 256-bit MAC output generation.
 *
 * Pass Criteria:
 * - MAC output matches reference KMAC128 calculation
 * - KEY_SHARE0 registers correctly read and encoded per NIST SP 800-185
 * - PREFIX validates encode_string("KMAC") header (0x01 0x20 0x4B 0x4D 0x41 0x43)
 * - right_encode(256) correctly processed from MSG_FIFO
 * - No IncorrectFunctionName error (ERR_CODE[31:24] != 0x07)
 *
 * Test Vector: Key = 128-bit value, Message = "abc", Output = 256 bits
 *
 * @param test Pointer to KMAC test harness
 */
void test_kmac_128bit_openssl_implementation(kmac_test* test);

/**
 * @brief TC-046: Verify KMAC with 256-bit key using custom OpenSSL implementation
 *
 * Validates that KMAC model implements KMAC256 using custom implementation
 * built on OpenSSL EVP_shake256 primitives. Tests 256-bit key and 256-bit
 * MAC output generation.
 *
 * Pass Criteria:
 * - MAC output matches reference KMAC256 calculation
 * - KEY_LEN = 0x2 correctly selects 256-bit key length (8 words)
 * - KEY_SHARE0 registers correctly processed
 * - CFG_SHADOWED (mode=0x3, kstrength=0x2, kmac_en=1) correctly mapped
 *
 * Test Vector: Key = 256-bit value, Message = "abc", Output = 256 bits
 *
 * @param test Pointer to KMAC test harness
 */
void test_kmac_256bit_openssl_implementation(kmac_test* test);

/**
 * @brief TC-160: Verify EVP_DigestFinal_ex produces digest after PROCESS command
 *
 * Validates that PROCESS command callback correctly invokes OpenSSL
 * EVP_DigestFinal_ex function to finalize hash computation and produce
 * digest. Tests SHA3-256 mode as representative case.
 *
 * Pass Criteria:
 * - STATE window populated immediately after PROCESS command completes
 * - STATUS.sha3_squeeze = 1 after PROCESS
 * - Digest matches EVP_DigestFinal_ex output for accumulated message
 * - No additional Keccak rounds executed beyond OpenSSL library
 *
 * Test Vector: Multi-block message spanning several MSG_FIFO writes
 *
 * @param test Pointer to KMAC test harness
 */
void test_evp_digestfinal_ex_after_process(kmac_test* test);

/**
 * @brief TC-161: Verify EVP_DigestFinalXOF provides additional output for RUN commands
 *
 * Validates that RUN command callback correctly invokes OpenSSL
 * EVP_DigestFinalXOF function (SHAKE-specific XOF API) to produce
 * additional output blocks. Tests extended output generation.
 *
 * Pass Criteria:
 * - Each RUN command produces additional rate-size output
 * - STATE window updated with new output block after each RUN
 * - Output sequence matches consecutive EVP_DigestFinalXOF calls
 * - FSM remains in SQUEEZE state (STATUS.sha3_squeeze = 1)
 * - Multiple RUN commands (e.g., 3-5) produce continuous output stream
 *
 * Test Vector: SHAKE128 with message "abc", extract 5 blocks (840 bytes total)
 *
 * @param test Pointer to KMAC test harness
 */
void test_evp_digestfinalxof_for_run_commands(kmac_test* test);
