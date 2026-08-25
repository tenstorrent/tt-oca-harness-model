// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-004: Automatic Operation Mode Test Implementations
// =============================================================================

// =============================================================================
// Test Case 1: Auto-Start on DATA_IN Complete
// =============================================================================

void testbench::test_auto_start_on_data_in_complete()
{
    report_test_start("test_auto_start_on_data_in_complete");

    try {
        // Configure AES in automatic mode (MANUAL_OPERATION=0)
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);  // manual_mode=false
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Check STATUS.INPUT_READY is 1 before writing data
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;

        if (!input_ready) {
            report_test_fail("test_auto_start_on_data_in_complete",
                           "INPUT_READY not set before writing DATA_IN");
            return;
        }

        // Write DATA_IN registers one by one
        uint32_t plaintext[4] = {0x32438596, 0x12345678, 0xaabbccdd, 0xeeff0011};
        for (int i = 0; i < 4; i++) {
            m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + i*4, plaintext[i]);
            wait(5, SC_NS);
        }

        // After writing all 4 DATA_IN registers, verify operation auto-starts
        wait(10, SC_NS);
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
       // bool idle = (status & (1 << STATUS_IDLE_BIT)) != 0;

        // Operation should have started (IDLE=0) or completed quickly (IDLE=1)
        // Either way, wait for OUTPUT_VALID
        wait_for_output_valid(1000);

        // Verify operation started - check status
        uint32_t status_after = read_status();
        bool is_idle_after = (status_after & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_after = (status_after & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (is_idle_after || !output_valid_after) {
            report_test_fail("test_auto_start_on_data_in_complete",
                           "Operation did not start or output not valid after writing DATA_IN");
            return;
        }

        // Read output to verify operation completed
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        report_test_pass("test_auto_start_on_data_in_complete");

    } catch (const std::exception& e) {
        report_test_fail("test_auto_start_on_data_in_complete", e.what());
    }
}

// =============================================================================
// Test Case 2: INPUT_READY Status Behavior
// =============================================================================

void testbench::test_input_ready_status_behavior()
{
    report_test_start("test_input_ready_status_behavior");

    try {
        // Configure AES in automatic mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Check STATUS.INPUT_READY is 1 when idle and no unread output
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_initial = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool idle_initial = (status & (1 << STATUS_IDLE_BIT)) != 0;

        if (!input_ready_initial || !idle_initial) {
            report_test_fail("test_input_ready_status_behavior",
                           "INPUT_READY not set initially when IDLE");
            return;
        }

        // Write all required inputs
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Wait for operation completion
        wait_for_output_valid(1000);

        // Verify INPUT_READY remains 0 while OUTPUT_VALID=1
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_before_read = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (input_ready_before_read && output_valid) {
            report_test_fail("test_input_ready_status_behavior",
                           "INPUT_READY set while OUTPUT_VALID=1 (should be 0)");
            return;
        }

        // Read all DATA_OUT registers
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Verify INPUT_READY becomes 1 after output consumed
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_after_read = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;

        if (!input_ready_after_read) {
            report_test_fail("test_input_ready_status_behavior",
                           "INPUT_READY not set after consuming output");
            return;
        }

        report_test_pass("test_input_ready_status_behavior");

    } catch (const std::exception& e) {
        report_test_fail("test_input_ready_status_behavior", e.what());
    }
}

// =============================================================================
// Test Case 3: OUTPUT_VALID Status Behavior
// =============================================================================

void testbench::test_output_valid_status_behavior()
{
    report_test_start("test_output_valid_status_behavior");

    try {
        // Configure AES and start operation
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Check STATUS.OUTPUT_VALID is 0 before operation
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_initial = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (output_valid_initial) {
            report_test_fail("test_output_valid_status_behavior",
                           "OUTPUT_VALID set before operation (should be 0)");
            return;
        }

        // Write data to start operation
        uint32_t plaintext[4] = {0x32438596, 0x12345678, 0xaabbccdd, 0xeeff0011};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Wait for operation completion
        wait_for_output_valid(1000);

        // Check STATUS.OUTPUT_VALID becomes 1 when output ready
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_after_op = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!output_valid_after_op) {
            report_test_fail("test_output_valid_status_behavior",
                           "OUTPUT_VALID not set after operation completion");
            return;
        }

        // Read first 3 DATA_OUT registers (not all 4)
        uint32_t data_out_temp;
        for (int i = 0; i < 3; i++) {
            m_test->register_read_32(aes_basetest::DATA_OUT_OFFSET + i*4, data_out_temp);
            wait(5, SC_NS);
        }

        // Verify OUTPUT_VALID remains 1 after reading only 3 registers
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_partial = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!output_valid_partial) {
            report_test_fail("test_output_valid_status_behavior",
                           "OUTPUT_VALID cleared before reading all DATA_OUT registers");
            return;
        }

        // Read 4th DATA_OUT register
        m_test->register_read_32(aes_basetest::DATA_OUT_OFFSET + 3*4, data_out_temp);
        wait(10, SC_NS);

        // Verify OUTPUT_VALID becomes 0 after all output read
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_after_read = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (output_valid_after_read) {
            report_test_fail("test_output_valid_status_behavior",
                           "OUTPUT_VALID still set after reading all DATA_OUT registers");
            return;
        }

        report_test_pass("test_output_valid_status_behavior");

    } catch (const std::exception& e) {
        report_test_fail("test_output_valid_status_behavior", e.what());
    }
}

// =============================================================================
// Test Case 4: Output Protection Prevents Overwrite
// =============================================================================

void testbench::test_output_protection_prevents_overwrite()
{
    report_test_start("test_output_protection_prevents_overwrite");

    try {
        // Configure AES and encrypt first block
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext1[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        // Wait for OUTPUT_VALID=1
        wait_for_output_valid(1000);

        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!output_valid) {
            report_test_fail("test_output_protection_prevents_overwrite",
                           "OUTPUT_VALID not set after first operation");
            return;
        }

        // Do NOT read DATA_OUT (leave output unread)
        // Write new DATA_IN for second block
        uint32_t plaintext2[4] = {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51};
        write_data_in(plaintext2);
        wait(50, SC_NS);  // Give time for potential auto-start

        // Verify operation does NOT auto-start (IDLE should remain 1 or INPUT_READY=0)
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_stalled = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool stall = (status & (1 << STATUS_STALL_BIT)) != 0;

        // In stall condition, INPUT_READY should be 0
        if (input_ready_stalled || !stall) {
            report_test_fail("test_output_protection_prevents_overwrite",
                           "INPUT_READY=1 despite unread output (back-pressure failed)");
            return;
        }

        // Read all DATA_OUT (consume first output)
        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        // Verify INPUT_READY becomes 1 after consuming output
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_after_read = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;

        if (!input_ready_after_read) {
            report_test_fail("test_output_protection_prevents_overwrite",
                           "INPUT_READY not restored after consuming output");
            return;
        }

        // Write DATA_IN again (should now auto-start)
        write_data_in(plaintext2);
        wait(10, SC_NS);

        // Verify operation now auto-starts
        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        report_test_pass("test_output_protection_prevents_overwrite");

    } catch (const std::exception& e) {
        report_test_fail("test_output_protection_prevents_overwrite", e.what());
    }
}

// =============================================================================
// Test Case 5: Multi-Block Pipelining
// =============================================================================

void testbench::test_multi_block_pipelining()
{
    report_test_start("test_multi_block_pipelining");

    try {
        // Configure AES-128 CBC encryption
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write initial IV
        uint32_t iv_initial[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Process 4 consecutive blocks
        uint32_t plaintext[4][4] = {
            {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a},
            {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51},
            {0x30c81c46, 0xa35ce411, 0xe5fbc119, 0x1a0a52ef},
            {0xf69f2445, 0xdf4f9b17, 0xad2b417b, 0xe66c3710}
        };
        uint32_t ciphertext[4][4];
        uint32_t iv_after_block[4][4];

        for (int block = 0; block < 4; block++) {
            // Encrypt block
            write_data_in(plaintext[block]);
            wait(10, SC_NS);

            // Wait for OUTPUT_VALID
            wait_for_output_valid(1000);

            // Read output (consume)
            read_data_out(ciphertext[block]);
            wait(10, SC_NS);

            // Read IV to verify auto-update
            for (int i = 0; i < 4; i++) {
                m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block[block][i]);
            }
            wait(10, SC_NS);

            // Verify IV auto-updated to last ciphertext block
            for (int i = 0; i < 4; i++) {
                if (iv_after_block[block][i] != ciphertext[block][i]) {
                    std::stringstream ss;
                    ss << "Block " << block << " IV not updated correctly";
                    report_test_fail("test_multi_block_pipelining", ss.str());
                    return;
                }
            }

            // Verify INPUT_READY becomes 1 for next block
            if (block < 3) {
                uint32_t status;
                m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
                bool input_ready = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;

                if (!input_ready) {
                    std::stringstream ss;
                    ss << "INPUT_READY not set after block " << block;
                    report_test_fail("test_multi_block_pipelining", ss.str());
                    return;
                }
            }
        }

        report_test_pass("test_multi_block_pipelining");

    } catch (const std::exception& e) {
        report_test_fail("test_multi_block_pipelining", e.what());
    }
}

// =============================================================================
// Test Case 6: Back-to-Back Processing
// =============================================================================

void testbench::test_back_to_back_processing()
{
    report_test_start("test_back_to_back_processing");

    try {
        // Configure AES-128 ECB encryption
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Process 3 consecutive blocks back-to-back (aligned with TLM test plan recommendations)
        const int NUM_BLOCKS = 3;
        uint32_t plaintext[NUM_BLOCKS][4];
        uint32_t ciphertext[NUM_BLOCKS][4];

        // Generate test data
        for (int block = 0; block < NUM_BLOCKS; block++) {
            for (int i = 0; i < 4; i++) {
                plaintext[block][i] = 0x11111111 * (block + 1) + i;
            }
        }

        // Process each block: write DATA_IN → wait OUTPUT_VALID → read DATA_OUT
        for (int block = 0; block < NUM_BLOCKS; block++) {
            // Write DATA_IN
            write_data_in(plaintext[block]);
            wait(10, SC_NS);

            // Wait for OUTPUT_VALID
            wait_for_output_valid(1000);

            // Read DATA_OUT
            read_data_out(ciphertext[block]);
            wait(10, SC_NS);
        }

        // Verify all blocks were processed (ciphertext is non-zero)
        for (int block = 0; block < NUM_BLOCKS; block++) {
            bool non_zero = true;
            for (int i = 0; i < 4; i++) {
                if (ciphertext[block][i] == 0) {
                    non_zero = false;
                    break;
                }
            }
            if (!non_zero) {  // Fail if ALL zeros (not encrypted)
                std::stringstream ss;
                ss << "Block " << block << " not encrypted (all zeros)";
                report_test_fail("test_back_to_back_processing", ss.str());
                return;
            }
        }

        report_test_pass("test_back_to_back_processing");

    } catch (const std::exception& e) {
        report_test_fail("test_back_to_back_processing", e.what());
    }
}

// =============================================================================
// Test Case 7: Auto-Start Requires All Conditions
// =============================================================================

void testbench::test_auto_start_requires_all_conditions()
{
    report_test_start("test_auto_start_requires_all_conditions");

    try {
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        // Case A: Write only 3 of 4 DATA_IN registers → no auto-start
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write only 3 DATA_IN registers
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 0*4, 0x11111111);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 1*4, 0x22222222);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 2*4, 0x33333333);
        wait(50, SC_NS);

        // Verify no auto-start (IDLE should be 1, OUTPUT_VALID should be 0)
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_case_a = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_case_a = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle_case_a || output_valid_case_a) {
            report_test_fail("test_auto_start_requires_all_conditions",
                           "Case A: Operation started with only 3 DATA_IN registers");
            return;
        }

        // Case B: Write DATA_IN but no key configured - no auto-start
        // Clear and reconfigure without key
        trigger_key_iv_data_in_clear();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        // Write DATA_IN without key
        uint32_t plaintext[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        write_data_in(plaintext);
        wait(50, SC_NS);

        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_case_b = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_case_b = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle_case_b || output_valid_case_b) {
            report_test_fail("test_auto_start_requires_all_conditions",
                           "Case B: Operation started without key configured");
            return;
        }

        // Case C: CBC mode with DATA_IN but no IV → no auto-start
        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write DATA_IN without IV
        write_data_in(plaintext);
        wait(50, SC_NS);

        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_case_c = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_case_c = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle_case_c || output_valid_case_c) {
            report_test_fail("test_auto_start_requires_all_conditions",
                           "Case C: CBC operation started without IV");
            return;
        }

        // Case D: Write DATA_IN while OUTPUT_VALID=1 → no auto-start (back-pressure)
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Encrypt first block
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        // Do NOT read output, write new DATA_IN
        uint32_t plaintext2[4] = {0x55555555, 0x66666666, 0x77777777, 0x88888888};
        write_data_in(plaintext2);
        wait(50, SC_NS);

        // Verify INPUT_READY=0 (stall condition)
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool input_ready_case_d = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;

        if (input_ready_case_d) {
            report_test_fail("test_auto_start_requires_all_conditions",
                           "Case D: INPUT_READY=1 despite OUTPUT_VALID=1");
            return;
        }

        // Cleanup: Read the pending output to clear OUTPUT_VALID for next test
        uint32_t dummy_output[4];
        read_data_out(dummy_output);
        wait(10, SC_NS);

        report_test_pass("test_auto_start_requires_all_conditions");

    } catch (const std::exception& e) {
        report_test_fail("test_auto_start_requires_all_conditions", e.what());
    }
}

// =============================================================================
// Test Case 8: Automatic vs Manual Mode
// =============================================================================

void testbench::test_automatic_vs_manual_mode()
{
    report_test_start("test_automatic_vs_manual_mode");

    try {
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        // Configure AES with MANUAL_OPERATION=0 (automatic)
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write all inputs
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Verify operation auto-starts
        wait_for_output_valid(1000);

        uint32_t ciphertext_auto[4];
        read_data_out(ciphertext_auto);
        wait(10, SC_NS);

        // Reconfigure with MANUAL_OPERATION=1 (manual)
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // manual_mode=true
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write all inputs
        write_data_in(plaintext);
        wait(50, SC_NS);  // Wait longer than auto mode

        // Verify operation does NOT auto-start (no OUTPUT_VALID yet)
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_manual_before = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (output_valid_manual_before) {
            report_test_fail("test_automatic_vs_manual_mode",
                           "Operation auto-started in manual mode");
            return;
        }

        // Write TRIGGER.START bit to start manually
        trigger_manual_start();
        wait(10, SC_NS);

        // Verify operation now starts in manual mode
        wait_for_output_valid(1000);

        uint32_t ciphertext_manual[4];
        read_data_out(ciphertext_manual);
        wait(10, SC_NS);

        // Verify both modes produce same ciphertext
        bool mismatch = false;
        for (int i = 0; i < 4; i++) {
            if (ciphertext_auto[i] != ciphertext_manual[i]) {
                mismatch = true;
                break;
            }
        }

        if (mismatch) {
            std::stringstream ss;
            ss << "Ciphertext mismatch between automatic and manual modes\n";
            ss << "  Auto:   [" << std::hex
               << "0x" << ciphertext_auto[0] << ", "
               << "0x" << ciphertext_auto[1] << ", "
               << "0x" << ciphertext_auto[2] << ", "
               << "0x" << ciphertext_auto[3] << "]\n";
            ss << "  Manual: ["
               << "0x" << ciphertext_manual[0] << ", "
               << "0x" << ciphertext_manual[1] << ", "
               << "0x" << ciphertext_manual[2] << ", "
               << "0x" << ciphertext_manual[3] << "]";
            report_test_fail("test_automatic_vs_manual_mode", ss.str());
            return;
        }

        report_test_pass("test_automatic_vs_manual_mode");

    } catch (const std::exception& e) {
        report_test_fail("test_automatic_vs_manual_mode", e.what());
    }
}

// =============================================================================
// Test Case 9: Status Transitions During Operation
// =============================================================================

void testbench::test_status_transitions_during_operation()
{
    report_test_start("test_status_transitions_during_operation");

    try {
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Initial state: IDLE=1, INPUT_READY=1, OUTPUT_VALID=0
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_initial = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool input_ready_initial = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool output_valid_initial = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle_initial || !input_ready_initial || output_valid_initial) {
            report_test_fail("test_status_transitions_during_operation",
                           "Initial state incorrect: expected IDLE=1, INPUT_READY=1, OUTPUT_VALID=0");
            return;
        }

        // After writing DATA_IN: operation should start or complete quickly
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // During/after operation: check status
        // (TLM model may complete instantly, so we check after OUTPUT_VALID)
        wait_for_output_valid(1000);

        // After completion in automatic mode: IDLE=0 (until output read), INPUT_READY=0, OUTPUT_VALID=1, STALL=1
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_after_op = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool input_ready_after_op = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool output_valid_after_op = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;
        bool stall_after_op = (status & (1 << STATUS_STALL_BIT)) != 0;

        if (idle_after_op || input_ready_after_op || !output_valid_after_op || !stall_after_op) {
            report_test_fail("test_status_transitions_during_operation",
                           "After operation: expected IDLE=0, INPUT_READY=0, OUTPUT_VALID=1, STALL=1");
            return;
        }

        // After reading output: IDLE=1, INPUT_READY=1, OUTPUT_VALID=0
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool idle_after_read = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool input_ready_after_read = (status & (1 << STATUS_INPUT_READY_BIT)) != 0;
        bool output_valid_after_read = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle_after_read || !input_ready_after_read || output_valid_after_read) {
            report_test_fail("test_status_transitions_during_operation",
                           "After reading output: expected IDLE=1, INPUT_READY=1, OUTPUT_VALID=0");
            return;
        }

        report_test_pass("test_status_transitions_during_operation");

    } catch (const std::exception& e) {
        report_test_fail("test_status_transitions_during_operation", e.what());
    }
}
