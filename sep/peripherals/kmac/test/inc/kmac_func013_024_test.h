// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file kmac_func013_024_test.h
 * @brief Test declarations for FUNC-KMAC-013 through FUNC-KMAC-024
 *
 * This header consolidates test function declarations for the remaining KMAC
 * functionalities (013-024), covering entropy management, configuration
 * protection, error detection, system integration, and TLM-specific features.
 *
 * Functionality Coverage:
 * - FUNC-KMAC-013: Entropy Management - EDN Mode (11 tests)
 * - FUNC-KMAC-014: Entropy Management - Software Mode (4 tests)
 * - FUNC-KMAC-015: Entropy Management - Idle Mode (5 tests)
 * - FUNC-KMAC-016: Configuration Shadow Register Protection (7 tests)
 * - FUNC-KMAC-017: Dynamic Register Write Protection (7 tests)
 * - FUNC-KMAC-018: STATE Window Access Control (9 tests)
 * - FUNC-KMAC-019: Error Detection and Reporting (15 tests)
 * - FUNC-KMAC-020: Reset and Initialization (6 tests)
 * - FUNC-KMAC-021: Life Cycle Escalation Response (5 tests)
 * - FUNC-KMAC-022: Idle Status Signaling (3 tests)
 * - FUNC-KMAC-023: OpenSSL Cryptographic Delegation (12 tests)
 * - FUNC-KMAC-024: Temporal Decoupling and Timing Abstraction (4 tests)
 *
 * Test Plan Reference: kmac-test-plan.md
 * Functionality Reference: kmac-functionality-testcases.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "kmac_test.h"

// =============================================================================
// FUNC-KMAC-016: Configuration Shadow Register Protection
// =============================================================================

/**
 * @brief TC-012: Shadow register CFG_SHADOWED duplicate write test
 *
 * Verifies CFG_SHADOWED requires two consecutive identical writes for
 * successful update as part of fault-detection mechanism.
 *
 * @param test Pointer to KMAC test harness
 */
void test_shadow_register_cfg_shadowed_duplicate_write(kmac_test* test);

/**
 * @brief TC-013: Shadow register CFG_SHADOWED mismatch test
 *
 * Verifies ALERT_RECOV_CTRL_UPDATE_ERR when CFG_SHADOWED write values mismatch.
 *
 * @param test Pointer to KMAC test harness
 */
void test_shadow_register_cfg_shadowed_mismatch(kmac_test* test);

/**
 * @brief TC-014: Shadow register ENTROPY_REFRESH_THRESHOLD_SHADOWED duplicate write test
 *
 * @param test Pointer to KMAC test harness
 */
void test_shadow_register_entropy_threshold_duplicate_write(kmac_test* test);

/**
 * @brief TC-015: Shadow register ENTROPY_REFRESH_THRESHOLD_SHADOWED mismatch test
 *
 * @param test Pointer to KMAC test harness
 */
void test_shadow_register_entropy_threshold_mismatch(kmac_test* test);

/**
 * @brief TC-158: Alert recoverable control update error bit test
 *
 * @param test Pointer to KMAC test harness
 */
void test_alert_recov_ctrl_update_err_bit(kmac_test* test);

/**
 * @brief TC-163: Callback CFG_SHADOWED write validation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_cfg_shadowed_write_validation(kmac_test* test);

/**
 * @brief TC-165: Callback ENTROPY_REFRESH_THRESHOLD validation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_entropy_refresh_threshold_validation(kmac_test* test);

// =============================================================================
// FUNC-KMAC-017: Dynamic Register Write Protection (CFG_REGWEN)
// =============================================================================

/**
 * @brief TC-008: CFG_REGWEN protection enable test
 *
 * Verifies protected registers are writable when CFG_REGWEN.en = 1.
 *
 * @param test Pointer to KMAC test harness
 */
void test_cfg_regwen_protection_enable(kmac_test* test);

/**
 * @brief TC-009: CFG_REGWEN protection disable test
 *
 * Verifies protected registers reject writes when CFG_REGWEN.en = 0.
 *
 * @param test Pointer to KMAC test harness
 */
void test_cfg_regwen_protection_disable(kmac_test* test);

/**
 * @brief TC-010: CFG_REGWEN auto-clear on START command test
 *
 * @param test Pointer to KMAC test harness
 */
void test_cfg_regwen_auto_clear_on_start(kmac_test* test);

/**
 * @brief TC-011: CFG_REGWEN auto-set on DONE command test
 *
 * @param test Pointer to KMAC test harness
 */
void test_cfg_regwen_auto_set_on_done(kmac_test* test);

/**
 * @brief TC-065: Key CFG_REGWEN protection test
 *
 * @param test Pointer to KMAC test harness
 */
void test_key_cfg_regwen_protection_complete(kmac_test* test);

/**
 * @brief TC-159: Callback CMD write START side effects test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_cmd_write_start_cfg_regwen(kmac_test* test);

/**
 * @brief TC-162: Callback CMD write DONE side effects test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_cmd_write_done_cfg_regwen(kmac_test* test);

// =============================================================================
// FUNC-KMAC-018: STATE Window Access Control
// =============================================================================

/**
 * @brief TC-135: STATE read in SQUEEZE state test
 *
 * Verifies STATE window contains valid digest when STATUS.sha3_squeeze = 1.
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_read_in_squeeze_state(kmac_test* test);

/**
 * @brief TC-136: STATE read in IDLE returns zero test
 *
 * Verifies STATE window returns 0 in IDLE state (key protection).
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_read_in_idle_returns_zero(kmac_test* test);

/**
 * @brief TC-137: STATE read in ABSORB returns zero test
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_read_in_absorb_returns_zero(kmac_test* test);

/**
 * @brief TC-138: STATE two-share layout EnMasking=1 test
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_two_share_layout_enmasking_1(kmac_test* test);

/**
 * @brief TC-139: STATE single-share layout EnMasking=0 test
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_single_share_layout_enmasking_0(kmac_test* test);

/**
 * @brief TC-140: STATE software XOR responsibility test
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_software_xor_responsibility(kmac_test* test);

/**
 * @brief TC-141: STATE read granularity byte/word test
 *
 * @param test Pointer to KMAC test harness
 */
void test_state_read_granularity_byte_word(kmac_test* test);

/**
 * @brief TC-101: Application STATE read blocked during app active test
 *
 * @param test Pointer to KMAC test harness
 */
void test_app_state_read_blocked_during_app_active_detailed(kmac_test* test);

/**
 * @brief TC-170: Callback STATE read conditional access test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_state_read_conditional_access(kmac_test* test);

// =============================================================================
// FUNC-KMAC-019: Error Detection and Reporting
// =============================================================================

/**
 * @brief TC-144: Error code KeyNotValid (0x01) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_keynotvalid_0x01(kmac_test* test);

/**
 * @brief TC-145: Error code SwIssuedCmdInAppActive (0x03) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_swissuedcmdinappactive_0x03_detailed(kmac_test* test);

/**
 * @brief TC-146: Error code SwPushedMsgFifo (0x02) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_swpushedmsgfifo_0x02(kmac_test* test);

/**
 * @brief TC-147: Error code WaitTimerExpired (0x04) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_waittimerexpired_0x04(kmac_test* test);

/**
 * @brief TC-148: Error code IncorrectEntropyMode (0x05) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_incorrectentropymode_0x05(kmac_test* test);

/**
 * @brief TC-149: Error code UnexpectedModeStrength (0x06) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_unexpectedmodestrength_0x06(kmac_test* test);

/**
 * @brief TC-150: Error code IncorrectFunctionName (0x07) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_incorrectfunctionname_0x07(kmac_test* test);

/**
 * @brief TC-151: Error code SwCmdSequence (0x08) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_swcmdsequence_0x08_detailed(kmac_test* test);

/**
 * @brief TC-152: Error code SwHashingWithoutEntropyReady (0x09) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_swhashingwithoutentropy_0x09(kmac_test* test);

/**
 * @brief TC-153: Error code Sha3Control (0x80) test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_sha3control_0x80(kmac_test* test);

/**
 * @brief TC-154: Error code persistence across interrupt clear test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_persistence_across_interrupt_clear(kmac_test* test);

/**
 * @brief TC-155: Error code cleared on DONE test
 *
 * @param test Pointer to KMAC test harness
 */
void test_err_code_cleared_on_done(kmac_test* test);

/**
 * @brief TC-156: Error recovery sequence test
 *
 * @param test Pointer to KMAC test harness
 */
void test_error_recovery_sequence(kmac_test* test);

/**
 * @brief TC-157: Alert fatal fault bit test
 *
 * @param test Pointer to KMAC test harness
 */
void test_alert_fatal_fault_bit(kmac_test* test);

/**
 * @brief TC-158: Alert recoverable control update error bit test (duplicate in FUNC-016)
 *
 * @param test Pointer to KMAC test harness
 */
void test_alert_recov_ctrl_update_err_bit_detailed(kmac_test* test);

// =============================================================================
// FUNC-KMAC-020: Reset and Initialization
// =============================================================================

/**
 * @brief TC-001: Register reset values test
 *
 * Verifies all registers reset to correct default values on initialization.
 *
 * @param test Pointer to KMAC test harness
 */
void test_reg_reset_values(kmac_test* test);

/**
 * @brief TC-062: Key zeroization on reset test
 *
 * @param test Pointer to KMAC test harness
 */
void test_key_zeroization_on_reset_detailed(kmac_test* test);

/**
 * @brief TC-071: FSM reset to IDLE test
 *
 * @param test Pointer to KMAC test harness
 */
void test_fsm_reset_to_idle_detailed(kmac_test* test);

/**
 * @brief TC-121: FIFO empty status on reset test
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_empty_status_on_reset_detailed(kmac_test* test);

/**
 * @brief TC-184: Corner case reset during ABSORB state test
 *
 * @param test Pointer to KMAC test harness
 */
void test_corner_reset_during_absorb_state(kmac_test* test);

/**
 * @brief TC-185: Corner case reset during SQUEEZE state test
 *
 * @param test Pointer to KMAC test harness
 */
void test_corner_reset_during_squeeze_state(kmac_test* test);

// =============================================================================
// FUNC-KMAC-021: Life Cycle Escalation Response
// =============================================================================

/**
 * @brief TC-190: Security escalation key zeroization test
 *
 * Verifies lc_escalate_en_i assertion immediately zeros KEY_SHARE and buffers.
 *
 * @param test Pointer to KMAC test harness
 */
void test_security_escalation_key_zeroization(kmac_test* test);

/**
 * @brief TC-191: Security escalation FSM invalid state test
 *
 * @param test Pointer to KMAC test harness
 */
void test_security_escalation_fsm_invalid_state(kmac_test* test);

/**
 * @brief TC-192: Security key zeroization on escalation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_security_key_zeroization_on_escalation_detailed(kmac_test* test);

/**
 * @brief TC-193: Security escalation operation abort test
 *
 * @param test Pointer to KMAC test harness
 */
void test_security_escalation_operation_abort(kmac_test* test);

/**
 * @brief TC-194: Security escalation reset-only recovery test
 *
 * @param test Pointer to KMAC test harness
 */
void test_security_escalation_reset_only_recovery(kmac_test* test);

// =============================================================================
// FUNC-KMAC-022: Idle Status Signaling
// =============================================================================

/**
 * @brief TC-071: FSM reset to IDLE with idle_o=1 test
 *
 * @param test Pointer to KMAC test harness
 */
void test_fsm_reset_to_idle_with_idle_o(kmac_test* test);

/**
 * @brief TC-072: FSM IDLE to ABSORB with idle_o transition test
 *
 * @param test Pointer to KMAC test harness
 */
void test_fsm_idle_to_absorb_idle_o_transition(kmac_test* test);

/**
 * @brief TC-074: FSM SQUEEZE to IDLE with idle_o return test
 *
 * @param test Pointer to KMAC test harness
 */
void test_fsm_squeeze_to_idle_idle_o_return(kmac_test* test);

// =============================================================================
// FUNC-KMAC-023: OpenSSL Cryptographic Delegation
// =============================================================================

/**
 * @brief TC-016: SHA3-224 single block OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_224_single_block_openssl(kmac_test* test);

/**
 * @brief TC-017: SHA3-256 single block OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_256_single_block_openssl(kmac_test* test);

/**
 * @brief TC-018: SHA3-384 single block OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_384_single_block_openssl(kmac_test* test);

/**
 * @brief TC-019: SHA3-512 single block OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_sha3_512_single_block_openssl(kmac_test* test);

/**
 * @brief TC-029: SHAKE128 fixed output OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_shake128_fixed_output_openssl(kmac_test* test);

/**
 * @brief TC-030: SHAKE256 fixed output OpenSSL delegation test
 *
 * @param test Pointer to KMAC test harness
 */
void test_shake256_fixed_output_openssl(kmac_test* test);

/**
 * @brief TC-038: cSHAKE128 with empty customization OpenSSL test
 *
 * @param test Pointer to KMAC test harness
 */
void test_cshake128_empty_customization_openssl(kmac_test* test);

/**
 * @brief TC-039: cSHAKE256 with empty customization OpenSSL test
 *
 * @param test Pointer to KMAC test harness
 */
void test_cshake256_empty_customization_openssl(kmac_test* test);

/**
 * @brief TC-045: KMAC 128-bit key 256-bit output OpenSSL test
 *
 * @param test Pointer to KMAC test harness
 */
void test_kmac_128bit_key_256bit_output_openssl(kmac_test* test);

/**
 * @brief TC-046: KMAC 256-bit key 256-bit output OpenSSL test
 *
 * @param test Pointer to KMAC test harness
 */
void test_kmac_256bit_key_256bit_output_openssl(kmac_test* test);

/**
 * @brief TC-160: Callback CMD write PROCESS side effects test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_cmd_write_process_openssl(kmac_test* test);

/**
 * @brief TC-161: Callback CMD write RUN side effects test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_cmd_write_run_openssl(kmac_test* test);

// =============================================================================
// FUNC-KMAC-024: Temporal Decoupling and Timing Abstraction
// =============================================================================

/**
 * @brief TC-125: FIFO full backpressure blocking test
 *
 * Verifies temporal decoupling wait models functional FIFO full stall.
 *
 * @param test Pointer to KMAC test harness
 */
void test_fifo_full_backpressure_blocking_temporal(kmac_test* test);

/**
 * @brief TC-167: Callback MSG_FIFO write backpressure test
 *
 * @param test Pointer to KMAC test harness
 */
void test_callback_msg_fifo_write_backpressure_temporal(kmac_test* test);

/**
 * @brief TC-106: Entropy EDN mode timeout test
 *
 * @param test Pointer to KMAC test harness
 */
void test_entropy_edn_mode_timeout_temporal(kmac_test* test);

/**
 * @brief TC-186: Corner case rapid command sequence test
 *
 * @param test Pointer to KMAC test harness
 */
void test_corner_rapid_command_sequence(kmac_test* test);

