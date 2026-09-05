// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc.h>
#include "aes.h"
#include "aes_test.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include "csml_logger.h"

// Testbench module
class testbench : public sc_module
{
public:
    CsmlLogger logger;
    // Module instances
    std::unique_ptr<aes_model> m_aes;          // Device Under Test
    std::unique_ptr<aes_test> m_test;          // Test model

    // Interconnect signals
    sc_signal<bool> clk_signal;
    sc_signal<bool> rst_signal;
    sc_signal<bool> alert_recov_signal;
    sc_signal<bool> alert_fatal_signal;
    sc_signal<bool> idle_signal;
    sc_signal<bool> lc_escalate_signal;

    SC_HAS_PROCESS(testbench);
    testbench(sc_module_name name);
    ~testbench() = default;

    // Main test sequence
    void run_tests();

    // Test tracking helper
    void report_test_result(const char* test_name, bool passed);

    // Test counters
    int m_tests_run;
    int m_tests_passed;
    int m_tests_failed;

    // Failed test tracking
    std::vector<std::string> m_failed_tests;

    // =============================================================================
    // FUNC-AES-001: Block Cipher Cryptographic Operations Test Cases
    // =============================================================================

    // Register Field Encodings
    enum operation_e {
        AES_ENC = 0x1,
        AES_DEC = 0x2
    };

    enum mode_e {
        AES_MODE_ECB  = 0x01,
        AES_MODE_CBC  = 0x02,
        AES_MODE_CFB  = 0x04,
        AES_MODE_OFB  = 0x08,
        AES_MODE_CTR  = 0x10,
        AES_MODE_GCM  = 0x20,
        AES_MODE_NONE = 0x3F
    };

    enum key_len_e {
        AES_128 = 0x1,
        AES_192 = 0x2,
        AES_256 = 0x4
    };

    enum status_bits_e {
        STATUS_IDLE_BIT         = 0,
        STATUS_STALL_BIT        = 1,
        STATUS_OUTPUT_LOST_BIT  = 2,
        STATUS_OUTPUT_VALID_BIT = 3,
        STATUS_INPUT_READY_BIT  = 4,
        STATUS_ALERT_RECOV_BIT  = 5,
        STATUS_ALERT_FATAL_BIT  = 6
    };

    // High-Level AES Configuration Methods
    void configure_aes(operation_e op, mode_e mode, key_len_e key_len, bool manual_mode = false, bool sideload = false);
    void write_key_shares(const uint32_t* key_share0, const uint32_t* key_share1, int num_words);
    void write_iv(const uint32_t* iv);
    void write_data_in(const uint32_t* data);
    void read_data_out(uint32_t* data);
    uint32_t read_status();
    void wait_for_idle(int timeout_ns = 10000);
    void wait_for_output_valid(int timeout_ns = 10000);
    void trigger_manual_start();
    void trigger_key_iv_data_in_clear();
    void trigger_prng_reseed();
    uint32_t read_ctrl_aux_shadowed();
    void write_ctrl_aux_shadowed(uint32_t value);
    void write_ctrl_aux_regwen(uint32_t value);

    // OpenSSL Reference Implementation Methods
    bool openssl_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                        const uint8_t* key, size_t key_len,
                        const uint8_t* iv, mode_e mode,
                        uint8_t* ciphertext, size_t& ciphertext_len);
    bool openssl_decrypt(const uint8_t* ciphertext, size_t ciphertext_len,
                        const uint8_t* key, size_t key_len,
                        const uint8_t* iv, mode_e mode,
                        uint8_t* plaintext, size_t& plaintext_len);

    // Test Utility Methods
    void generate_random_data(uint8_t* data, size_t len);
    void generate_two_share_key(uint32_t* key_share0, uint32_t* key_share1,
                                uint32_t* actual_key, int num_words);
    bool compare_data(const uint8_t* data1, const uint8_t* data2, size_t len);
    std::string data_to_hex(const uint8_t* data, size_t len);
    void uint32_to_uint8(const uint32_t* src, uint8_t* dst, size_t num_words);
    void uint8_to_uint32(const uint8_t* src, uint32_t* dst, size_t num_words);


    // --- OpenSSL Equivalence Tests ---
    // Encryption Tests
    void test_openssl_aes128_ecb_equivalence();
    void test_openssl_aes192_ecb_equivalence();
    void test_openssl_aes256_ecb_equivalence();
    void test_openssl_aes128_cbc_equivalence();
    void test_openssl_aes192_cbc_equivalence();
    void test_openssl_aes256_cbc_equivalence();
    void test_openssl_aes128_cfb_equivalence();
    void test_openssl_aes192_cfb_equivalence();
    void test_openssl_aes256_cfb_equivalence();
    void test_openssl_aes128_ofb_equivalence();
    void test_openssl_aes192_ofb_equivalence();
    void test_openssl_aes256_ofb_equivalence();
    void test_openssl_aes128_ctr_equivalence();
    void test_openssl_aes192_ctr_equivalence();
    void test_openssl_aes256_ctr_equivalence();

    // Decryption Tests
    void test_openssl_aes128_ecb_decryption_equivalence();
    void test_openssl_aes192_ecb_decryption_equivalence();
    void test_openssl_aes256_ecb_decryption_equivalence();
    void test_openssl_aes128_cbc_decryption_equivalence();
    void test_openssl_aes192_cbc_decryption_equivalence();
    void test_openssl_aes256_cbc_decryption_equivalence();
    void test_openssl_aes128_cfb_decryption_equivalence();
    void test_openssl_aes192_cfb_decryption_equivalence();
    void test_openssl_aes256_cfb_decryption_equivalence();
    void test_openssl_aes128_ofb_decryption_equivalence();
    void test_openssl_aes192_ofb_decryption_equivalence();
    void test_openssl_aes256_ofb_decryption_equivalence();
    void test_openssl_aes128_ctr_decryption_equivalence();
    void test_openssl_aes192_ctr_decryption_equivalence();
    void test_openssl_aes256_ctr_decryption_equivalence();

    // =============================================================================
    // FUNC-AES-002:
    // =============================================================================

    // Sideload Key Equivalence Tests
    void test_openssl_sideload_aes128_ecb_equivalence();
    void test_openssl_sideload_aes256_cbc_equivalence();
    void test_openssl_sideload_ignores_key_share_writes();

    // Write Protection Validation Tests
    void test_iv_write_protection_when_non_idle();
    void test_ctrl_shadowed_write_protection_when_non_idle();

    // Invalid Key Length Handling Test
    void test_openssl_invalid_key_length_defaults_to_aes256();

    // =============================================================================
    // FUNC-AES-003: Initialization Vector Management Test Cases
    // =============================================================================

    void test_iv_register_read_write();
    void test_iv_write_protection_when_busy();
    void test_cbc_encryption_iv_auto_update();
    void test_cbc_decryption_iv_auto_update();
    void test_cfb_mode_iv_auto_update();
    void test_ofb_mode_iv_auto_update();
    void test_ctr_mode_counter_increment();
    void test_ctr_counter_overflow();
    void test_ecb_mode_ignores_iv();
    void test_iv_required_for_non_ecb_auto_start();
    void test_multi_block_cbc_chaining();

    // =============================================================================
    // FUNC-AES-004: Automatic Operation Mode Test Cases
    // =============================================================================

    void test_auto_start_on_data_in_complete();
    void test_input_ready_status_behavior();
    void test_output_valid_status_behavior();
    void test_output_protection_prevents_overwrite();
    void test_multi_block_pipelining();
    void test_back_to_back_processing();
    void test_auto_start_requires_all_conditions();
    void test_automatic_vs_manual_mode();
    void test_status_transitions_during_operation();

    // =============================================================================
    // FUNC-AES-005: Manual Operation Mode Test Cases
    // =============================================================================

    void test_manual_mode_explicit_start();
    void test_manual_mode_no_back_pressure();
    void test_output_lost_flag_on_overwrite();
    void test_output_lost_cleared_by_ctrl_write();
    void test_manual_mode_precondition_data_in();
    void test_manual_mode_precondition_key();
    void test_manual_mode_precondition_iv();
    void test_manual_mode_multi_block_output_overwrite();

    // =============================================================================
    // FUNC-AES-006: Shadowed Register Fault Detection Test Cases
    // =============================================================================

    void test_ctrl_shadowed_two_write_matching();
    void test_ctrl_shadowed_two_write_mismatch();
    void test_ctrl_aux_shadowed_two_write_matching();
    void test_ctrl_aux_shadowed_two_write_mismatch();
    void test_shadowed_read_resets_sequence();
    void test_alert_cleared_by_successful_write();
    void test_multiple_consecutive_mismatches();
    void test_ctrl_aux_write_protection_when_locked();
    void test_shadowed_write_rejected_when_busy();
    void test_ctrl_aux_regwen_cannot_unlock();
    void test_ctrl_aux_shadowed_read_resets_sequence();
    void test_ctrl_shadowed_per_field_update_error();
    void test_ctrl_aux_shadowed_per_field_update_error();
    void test_ctrl_shadowed_sanitised_readback();

    // =============================================================================
    // Galois/Counter Mode
    // =============================================================================

    void test_gcm_ctrl_reset_and_sanitisation();
    void test_gcm_phase_transition_gating();
    void test_gcm_encrypt_nist_case4();
    void test_gcm_decrypt_nist_case4();
    void test_gcm_save_restore();

    /// Drives one GCM phase write (two-write shadow sequence) and returns readback.
    uint32_t write_gcm_phase(uint32_t phase, uint32_t num_valid_bytes);
    /// Feeds one 16-byte block through the current GCM phase.
    void gcm_feed_block(const uint8_t* block, uint8_t* out, bool expect_output);


    // =============================================================================
    // FUNC-AES-007: Register Interface Tests for Security Features
    // =============================================================================

    void test_ctrl_aux_key_touch_forces_reseed_register();
    void test_trigger_key_iv_data_in_clear();
    void test_trigger_data_out_clear();

    // =============================================================================
    // FUNC-AES-008: Alert Generation and Error Reporting Test Cases
    // =============================================================================

    void test_alert_test_register_recoverable_trigger();
    void test_alert_test_register_fatal_trigger();
    void test_fatal_alert_terminal_error_state();
    void test_fatal_alert_recovery_requires_reset();
    void test_life_cycle_escalation_fatal_alert();
    void test_life_cycle_escalation_register_writes_ignored();
    void test_register_clearing_on_fatal_alert();
    void test_status_register_readable_in_error_state();
    void test_multiple_fatal_alert_triggers();
    void test_persistent_error_state();
    void test_recoverable_alert_no_error_state();
    void test_alert_outputs_match_status_register();

    // =============================================================================
    // FUNC-AES-009: Functional Timing Tests for Encryption/Decryption Operations
    // =============================================================================

    void test_func009_aes128_block_latency();
    void test_func009_aes192_block_latency();
    void test_func009_aes256_block_latency();
    void test_func009_timing_across_modes();

    // Edge paths not exercised by the FUNC suites (coverage gate).
    void test_coverage_keymgr_rejects_non_write();
    void test_coverage_prng_reseed_trigger_and_rates();
    void test_coverage_escalation_aborts_in_flight_cipher();
    void test_coverage_sideload_missing_key_and_manual_start();
    void test_coverage_error_state_and_busy_gcm_writes();
    void test_coverage_gcm_shadow_mismatch_and_init_gates();
    void test_coverage_trigger_readback_and_gcm_manual_init();

    // Test helper reporting
    void report_test_start(const std::string& test_name);
    void report_test_pass(const std::string& test_name);
    void report_test_fail(const std::string& test_name, const std::string& reason);
    void report_test_summary();
};
