// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"
#include <cstdlib>
#include <ctime>
#include <chrono>

// =============================================================================
// FUNC-AES-009: Functional Timing Tests for Encryption/Decryption Operations
// =============================================================================
//
// This test suite validates timing for encryption/decryption operations only.
// Non-encryption operations (clearing, PRNG reseed) are NOT timing-validated
// as they are not functionally relevant for TLM models.
//
// Tests included:
// 1. AES-128/192/256 block encryption latency
// 2. Key expansion latency for ECB/CBC decryption
// 3. CFB mode (no key expansion needed)
// 4. Timing consistency across cipher modes
// 5. Multi-block timing consistency
// =============================================================================

// Test: Block processing latency for AES-128 unmasked (12 cycles)
void testbench::test_func009_aes128_block_latency()
{
    report_test_start("test_func009_aes128_block_latency");

    try {
        // KEY_TOUCH_FORCES_RESEED resets to 1, so writing a key queues a reseed that
        // lands inside the measured window. These tests time the cipher, not the
        // reseed, so clear it first.
        write_ctrl_aux_shadowed(0x0);

        // Configure AES-128 ECB encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        // Write key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        generate_two_share_key(key_share0, key_share1, actual_key, 4);
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Wait for IDLE
        wait_for_idle(1000);

        // Record start time
        sc_time start_time = sc_time_stamp();

        // Write DATA_IN to trigger operation
        uint32_t plaintext[4] = {0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Wait for OUTPUT_VALID
        wait_for_output_valid(1000);

        // Record end time (before reading DATA_OUT to capture cipher timing)
        sc_time end_time = sc_time_stamp();
        sc_time elapsed = end_time - start_time;

        // Read DATA_OUT to clear OUTPUT_VALID for next test
        uint32_t ciphertext[4];
        read_data_out(ciphertext);

        // Verify encryption took functional time (12 cycles for AES-128 unmasked)
        // At 100MHz: 12 cycles = 120ns
        double elapsed_ns = elapsed.to_seconds() * 1e9;
        double expected_ns = 120.0;
        double tolerance = 50.0; // TLM temporal decoupling tolerance

        if (elapsed_ns < (expected_ns - tolerance) || elapsed_ns > (expected_ns + tolerance)) {
            std::string msg = "AES-128 block latency mismatch. Expected ~" +
                            std::to_string(expected_ns) + "ns, got " +
                            std::to_string(elapsed_ns) + "ns";
            report_test_fail("test_func009_aes128_block_latency", msg);
            return;
        }

        report_test_pass("test_func009_aes128_block_latency");

    } catch (const std::exception& e) {
        report_test_fail("test_func009_aes128_block_latency", e.what());
    }
}

// Test: Block processing latency for AES-192 unmasked (14 cycles)
void testbench::test_func009_aes192_block_latency()
{
    report_test_start("test_func009_aes192_block_latency");

    try {
        // See test_func009_aes128_block_latency: keep the key-touch reseed out of
        // the measured window.
        write_ctrl_aux_shadowed(0x0);

        // Configure AES-192 ECB encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_192, false);
        wait(10, SC_NS);

        // Write key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        generate_two_share_key(key_share0, key_share1, actual_key, 6);
        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Record start time
        sc_time start_time = sc_time_stamp();

        // Write DATA_IN to trigger operation
        uint32_t plaintext[4] = {0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF};
        write_data_in(plaintext);

        // Don't wait here - let wait_for_output_valid() handle the polling
        wait_for_output_valid(1000);

        sc_time end_time = sc_time_stamp();
        sc_time elapsed = end_time - start_time;

        // Read DATA_OUT to clear OUTPUT_VALID for next test
        uint32_t ciphertext[4];
        read_data_out(ciphertext);

        // Verify encryption took functional time (14 cycles for AES-192 unmasked)
        // At 100MHz: 14 cycles = 140ns
        double elapsed_ns = elapsed.to_seconds() * 1e9;
        double expected_ns = 140.0;
        double tolerance = 50.0;

        if (elapsed_ns < (expected_ns - tolerance) || elapsed_ns > (expected_ns + tolerance)) {
            std::string msg = "AES-192 block latency mismatch. Expected ~" +
                            std::to_string(expected_ns) + "ns, got " +
                            std::to_string(elapsed_ns) + "ns";
            report_test_fail("test_func009_aes192_block_latency", msg);
            return;
        }

        report_test_pass("test_func009_aes192_block_latency");

    } catch (const std::exception& e) {
        report_test_fail("test_func009_aes192_block_latency", e.what());
    }
}

// Test: Block processing latency for AES-256 unmasked (16 cycles)
void testbench::test_func009_aes256_block_latency()
{
    report_test_start("test_func009_aes256_block_latency");

    try {
        // See test_func009_aes128_block_latency: keep the key-touch reseed out of
        // the measured window.
        write_ctrl_aux_shadowed(0x0);

        // Configure AES-256 ECB encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_256, false);
        wait(10, SC_NS);

        // Write key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        generate_two_share_key(key_share0, key_share1, actual_key, 8);
        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Record start time
        sc_time start_time = sc_time_stamp();

        // Write DATA_IN to trigger operation
        uint32_t plaintext[4] = {0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF};
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        sc_time end_time = sc_time_stamp();
        sc_time elapsed = end_time - start_time;

        // Read DATA_OUT to clear OUTPUT_VALID for next test
        uint32_t ciphertext[4];
        read_data_out(ciphertext);

        // Verify encryption took functional time (16 cycles for AES-256 unmasked)
        // At 100MHz: 16 cycles = 160ns
        double elapsed_ns = elapsed.to_seconds() * 1e9;
        double expected_ns = 160.0;
        double tolerance = 50.0;

        if (elapsed_ns < (expected_ns - tolerance) || elapsed_ns > (expected_ns + tolerance)) {
            std::string msg = "AES-256 block latency mismatch. Expected ~" +
                            std::to_string(expected_ns) + "ns, got " +
                            std::to_string(elapsed_ns) + "ns";
            report_test_fail("test_func009_aes256_block_latency", msg);
            return;
        }

        report_test_pass("test_func009_aes256_block_latency");

    } catch (const std::exception& e) {
        report_test_fail("test_func009_aes256_block_latency", e.what());
    }
}

// Test: Timing consistency across different cipher modes
void testbench::test_func009_timing_across_modes()
{
    report_test_start("test_func009_timing_across_modes");

    try {
        // See test_func009_aes128_block_latency: keep the key-touch reseed out of
        // the measured window.
        write_ctrl_aux_shadowed(0x0);

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4] = {0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00};
        uint32_t plaintext[4] = {0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF};

        std::vector<std::pair<std::string, mode_e>> modes = {
            {"ECB", AES_MODE_ECB},
            {"CBC", AES_MODE_CBC},
            {"CFB", AES_MODE_CFB},
            {"OFB", AES_MODE_OFB},
            {"CTR", AES_MODE_CTR}
        };

        std::vector<double> latencies;

        for (const auto& mode_pair : modes) {
            // Configure mode
            configure_aes(AES_ENC, mode_pair.second, AES_128, false);
            wait(10, SC_NS);

            write_key_shares(key_share0, key_share1, 4);
            wait(10, SC_NS);

            if (mode_pair.second != AES_MODE_ECB) {
                write_iv(iv);
                wait(10, SC_NS);
            }

            wait_for_idle(1000);

            // Measure block processing time
            sc_time start = sc_time_stamp();
            write_data_in(plaintext);
            wait(10, SC_NS);
            wait_for_output_valid(1000);
            sc_time end = sc_time_stamp();

            double latency_ns = (end - start).to_seconds() * 1e9;
            latencies.push_back(latency_ns);

            // Read output to clear for next iteration
            uint32_t output[4];
            read_data_out(output);
            wait(10, SC_NS);
        }

        // All modes with same key length should have similar latencies (12 cycles for AES-128)
        // Check that all latencies are within reasonable range
        double expected_ns = 120.0; // 12 cycles
        double tolerance = 50.0;

        for (size_t i = 0; i < modes.size(); i++) {
            if (latencies[i] < (expected_ns - tolerance) || latencies[i] > (expected_ns + tolerance)) {
                std::string msg = "Mode " + modes[i].first + " latency out of range: " +
                                std::to_string(latencies[i]) + "ns (expected ~" +
                                std::to_string(expected_ns) + "ns)";
                report_test_fail("test_func009_timing_across_modes", msg);
                return;
            }
        }

        report_test_pass("test_func009_timing_across_modes");

    } catch (const std::exception& e) {
        report_test_fail("test_func009_timing_across_modes", e.what());
    }
}