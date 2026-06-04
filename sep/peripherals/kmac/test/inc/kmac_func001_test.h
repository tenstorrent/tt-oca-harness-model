/******************************************************************************
 * @file kmac_func001_test.h
 * @brief Test declarations for FUNC-KMAC-001 (SHA3 Hash Operation - Phase 1)
 *
 * This header declares test functions for FUNC-KMAC-001 Phase 1 implementation.
 * Phase 1 focuses on OpenSSL context initialization in START command and
 * algorithm selection validation.
 *
 * Phase 1 Scope:
 * - OpenSSL EVP_MD context initialization with correct SHA3 algorithm
 * - Algorithm selection based on CFG_SHADOWED.mode and kstrength
 * - UnexpectedModeStrength error detection (0x06)
 * - Configuration validation tests
 *
 * Deferred to Later Phases:
 * - Full hash computation (requires MSG_FIFO and STATE implementation)
 * - Message absorption and padding
 * - Digest output verification
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Functionality Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-functionality-testcases.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"

// =============================================================================
// FUNC-KMAC-001 Phase 1 Test Function Declarations
// =============================================================================

/**
 * @brief TC-016: SHA3-224 algorithm selection test
 *
 * Verifies that START command correctly initializes OpenSSL EVP context
 * with EVP_sha3_224() algorithm when CFG_SHADOWED is configured with:
 * - mode = 0x0 (SHA3)
 * - kstrength = 0x1 (L224)
 * - kmac_en = 0
 *
 * Pass Criteria:
 * - FSM transitions from IDLE to ABSORB
 * - No UnexpectedModeStrength error (ERR_CODE != 0x06xxxxxx)
 * - CFG_REGWEN.en auto-cleared to 0
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_224_algorithm_selection(kmac_test* test);

/**
 * @brief TC-017: SHA3-256 algorithm selection test
 *
 * Verifies that START command correctly initializes OpenSSL EVP context
 * with EVP_sha3_256() algorithm when CFG_SHADOWED is configured with:
 * - mode = 0x0 (SHA3)
 * - kstrength = 0x2 (L256)
 * - kmac_en = 0
 *
 * Pass Criteria:
 * - FSM transitions from IDLE to ABSORB
 * - No UnexpectedModeStrength error (ERR_CODE != 0x06xxxxxx)
 * - CFG_REGWEN.en auto-cleared to 0
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_256_algorithm_selection(kmac_test* test);

/**
 * @brief TC-018: SHA3-384 algorithm selection test
 *
 * Verifies that START command correctly initializes OpenSSL EVP context
 * with EVP_sha3_384() algorithm when CFG_SHADOWED is configured with:
 * - mode = 0x0 (SHA3)
 * - kstrength = 0x3 (L384)
 * - kmac_en = 0
 *
 * Pass Criteria:
 * - FSM transitions from IDLE to ABSORB
 * - No UnexpectedModeStrength error (ERR_CODE != 0x06xxxxxx)
 * - CFG_REGWEN.en auto-cleared to 0
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_384_algorithm_selection(kmac_test* test);

/**
 * @brief TC-019: SHA3-512 algorithm selection test
 *
 * Verifies that START command correctly initializes OpenSSL EVP context
 * with EVP_sha3_512() algorithm when CFG_SHADOWED is configured with:
 * - mode = 0x0 (SHA3)
 * - kstrength = 0x4 (L512)
 * - kmac_en = 0
 *
 * Pass Criteria:
 * - FSM transitions from IDLE to ABSORB
 * - No UnexpectedModeStrength error (ERR_CODE != 0x06xxxxxx)
 * - CFG_REGWEN.en auto-cleared to 0
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_512_algorithm_selection(kmac_test* test);

/**
 * @brief TC-028: SHA3 invalid strength L128 test
 *
 * Verifies that UnexpectedModeStrength error (0x06) is generated when
 * SHA3 mode is configured with invalid kstrength = 0x0 (L128).
 * SHA3 only supports L224, L256, L384, L512.
 *
 * Pass Criteria:
 * - ERR_CODE[31:24] = 0x06 (UnexpectedModeStrength)
 * - ERR_CODE[15:8] = mode value (0x00 for SHA3)
 * - ERR_CODE[7:0] = kstrength value (0x00 for L128)
 * - INTR_STATE.kmac_err = 1
 * - FSM remains in IDLE (operation rejected)
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_invalid_strength_l128(kmac_test* test);

/**
 * @brief Additional Test: SHA3 mode configuration validation
 *
 * Verifies that CFG_SHADOWED.mode and kstrength fields are correctly
 * read and used for algorithm selection across all valid SHA3 variants.
 *
 * Pass Criteria:
 * - All four valid SHA3 variants (224/256/384/512) initialize successfully
 * - Shadow register duplicate write mechanism works correctly
 * - Configuration locked during operation (CFG_REGWEN.en = 0)
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_configuration_validation(kmac_test* test);

/**
 * @brief Additional Test: UnexpectedModeStrength error with reserved kstrength
 *
 * Verifies that UnexpectedModeStrength error is generated for reserved
 * kstrength values (0x5, 0x6, 0x7) in SHA3 mode.
 *
 * Pass Criteria:
 * - ERR_CODE[31:24] = 0x06 for all reserved kstrength values
 * - Error recovery via err_processed command works correctly
 * - FSM returns to IDLE after error recovery
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_reserved_kstrength_error(kmac_test* test);

/**
 * @brief Additional Test: Back-to-back SHA3 algorithm switching
 *
 * Verifies that multiple consecutive operations with different SHA3
 * algorithms work correctly (e.g., SHA3-256 → SHA3-512 → SHA3-224).
 * Tests OpenSSL EVP context reinitialization.
 *
 * Pass Criteria:
 * - Each algorithm selection succeeds
 * - FSM cycles correctly through IDLE → ABSORB → IDLE
 * - No spurious errors between operations
 * - CFG_REGWEN protection cycles correctly
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_back_to_back_algorithm_switching(kmac_test* test);
