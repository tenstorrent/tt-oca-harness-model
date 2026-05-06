#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-005: Manual Operation Mode Test Implementations
// =============================================================================

// =============================================================================
// Test Case 1: Manual Mode Explicit Start Required
// =============================================================================

void testbench::test_manual_mode_explicit_start()
{
    report_test_start("test_manual_mode_explicit_start");

    try {
        // Configure AES in manual mode (MANUAL_OPERATION=1)
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // manual_mode=true
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write all DATA_IN registers
        uint32_t plaintext[4] = {0x32438596, 0x12345678, 0xaabbccdd, 0xeeff0011};
        write_data_in(plaintext);
        wait(50, SC_NS);  // Wait longer than typical operation time

        // Verify operation does NOT auto-start (OUTPUT_VALID should be 0)
        uint32_t status;
        m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
        bool output_valid_before_trigger = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;
        bool idle = (status & (1 << STATUS_IDLE_BIT)) != 0;

        if (output_valid_before_trigger) {
            report_test_fail("test_manual_mode_explicit_start",
                           "Operation auto-started in manual mode (OUTPUT_VALID=1)");
            return;
        }

        if (!idle) {
            report_test_fail("test_manual_mode_explicit_start",
                           "AES not idle after writing DATA_IN in manual mode");
            return;
        }

        // Trigger manual start
        trigger_manual_start();
        wait(10, SC_NS);

        // Verify operation now starts and completes
        wait_for_output_valid(1000);

        // Read output
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Verify ciphertext is non-zero (operation succeeded)
        bool non_zero = false;
        for (int i = 0; i < 4; i++) {
            if (ciphertext[i] != 0) {
                non_zero = true;
                break;
            }
        }

        if (!non_zero) {
            report_test_fail("test_manual_mode_explicit_start",
                           "Ciphertext all zeros (operation may have failed)");
            return;
        }

        report_test_pass("test_manual_mode_explicit_start");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_explicit_start", e.what());
    }
}

// =============================================================================
// Test Case 2: Manual Mode No Back-Pressure
// =============================================================================

void testbench::test_manual_mode_no_back_pressure()
{
    report_test_start("test_manual_mode_no_back_pressure");

    try {
        // Configure AES in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // manual_mode=true
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext1[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        // Wait for OUTPUT_VALID=1
        wait_for_output_valid(1000);

        uint32_t status = read_status();
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!output_valid) {
            report_test_fail("test_manual_mode_no_back_pressure",
                           "OUTPUT_VALID not set after first operation");
            return;
        }

        // Do NOT read DATA_OUT (leave output unread)
        // Immediately trigger second operation
        uint32_t plaintext2[4] = {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51};
        write_data_in(plaintext2);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        // Wait for second operation completion
        wait_for_output_valid(1000);

        // Verify no STALL flag set (no back-pressure in manual mode)
        status = read_status();
        bool stall = (status & (1 << STATUS_STALL_BIT)) != 0;

        if (stall) {
            report_test_fail("test_manual_mode_no_back_pressure",
                           "STALL flag set in manual mode (should never stall)");
            return;
        }

        // Verify second operation completed successfully
        output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!output_valid) {
            report_test_fail("test_manual_mode_no_back_pressure",
                           "Second operation did not complete");
            return;
        }

        // Read second output
        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        report_test_pass("test_manual_mode_no_back_pressure");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_no_back_pressure", e.what());
    }
}

// =============================================================================
// Test Case 4: OUTPUT_LOST Flag Set on Output Overwrite
// =============================================================================

void testbench::test_output_lost_flag_on_overwrite()
{
    report_test_start("test_output_lost_flag_on_overwrite");

    try {
        // Configure AES in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // manual_mode=true
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Verify OUTPUT_LOST initially clear
        uint32_t status = read_status();
        bool output_lost_initial = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (output_lost_initial) {
            report_test_fail("test_output_lost_flag_on_overwrite",
                           "OUTPUT_LOST flag set before any operations");
            return;
        }

        // Encrypt first block
        uint32_t plaintext1[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        // Do NOT read first output - trigger second operation to overwrite it
        uint32_t plaintext2[4] = {0x55555555, 0x66666666, 0x77777777, 0x88888888};
        write_data_in(plaintext2);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        // Check OUTPUT_LOST flag is now set (sticky)
        status = read_status();
        bool output_lost_after = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (!output_lost_after) {
            report_test_fail("test_output_lost_flag_on_overwrite",
                           "OUTPUT_LOST flag not set after overwriting unread output");
            return;
        }

        // Read second output
        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        // Verify OUTPUT_LOST remains set (sticky) even after reading output
        status = read_status();
        bool output_lost_still_set = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (!output_lost_still_set) {
            report_test_fail("test_output_lost_flag_on_overwrite",
                           "OUTPUT_LOST flag cleared after reading output (should be sticky)");
            return;
        }

        report_test_pass("test_output_lost_flag_on_overwrite");

    } catch (const std::exception& e) {
        report_test_fail("test_output_lost_flag_on_overwrite", e.what());
    }
}

// =============================================================================
// Test Case 5: OUTPUT_LOST Flag Cleared by CTRL_SHADOWED Write
// =============================================================================

void testbench::test_output_lost_cleared_by_ctrl_write()
{
    report_test_start("test_output_lost_cleared_by_ctrl_write");

    try {
        // Configure AES in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // manual_mode=true
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Create OUTPUT_LOST condition (two operations without reading first output)
        uint32_t plaintext[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

        // First operation
        write_data_in(plaintext);
        wait(10, SC_NS);
        trigger_manual_start();
        wait(10, SC_NS);
        wait_for_output_valid(1000);

        // Second operation (overwrites first output)
        write_data_in(plaintext);
        wait(10, SC_NS);
        trigger_manual_start();
        wait(10, SC_NS);
        wait_for_output_valid(1000);

        // Verify OUTPUT_LOST is set
        uint32_t status = read_status();
        bool output_lost_before = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (!output_lost_before) {
            report_test_fail("test_output_lost_cleared_by_ctrl_write",
                           "OUTPUT_LOST not set after overwrite");
            return;
        }

        // Read output to clear OUTPUT_VALID
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Write CTRL_SHADOWED to signal new message (clears OUTPUT_LOST)
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);  // Reconfigure (writes CTRL)
        wait(10, SC_NS);

        // Verify OUTPUT_LOST is now cleared
        status = read_status();
        bool output_lost_after = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (output_lost_after) {
            report_test_fail("test_output_lost_cleared_by_ctrl_write",
                           "OUTPUT_LOST not cleared by CTRL_SHADOWED write");
            return;
        }

        report_test_pass("test_output_lost_cleared_by_ctrl_write");

    } catch (const std::exception& e) {
        report_test_fail("test_output_lost_cleared_by_ctrl_write", e.what());
    }
}

// =============================================================================
// Test Case 6: Manual Mode Precondition - DATA_IN Required
// =============================================================================

void testbench::test_manual_mode_precondition_data_in()
{
    report_test_start("test_manual_mode_precondition_data_in");

    try {
        // Configure AES in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write only 3 of 4 DATA_IN registers (incomplete)
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 0*4, 0x11111111);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 1*4, 0x22222222);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 2*4, 0x33333333);
        wait(10, SC_NS);

        // Try to trigger start with incomplete DATA_IN
        trigger_manual_start();
        wait(50, SC_NS);

        // Verify operation did not start (IDLE should remain 1)
        uint32_t status = read_status();
        bool idle = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle || output_valid) {
            report_test_fail("test_manual_mode_precondition_data_in",
                           "Operation started with incomplete DATA_IN");
            return;
        }

        // Complete DATA_IN and verify operation can now start
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 3*4, 0x44444444);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        report_test_pass("test_manual_mode_precondition_data_in");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_precondition_data_in", e.what());
    }
}

// =============================================================================
// Test Case 8: Manual Mode Precondition - KEY Required
// =============================================================================

void testbench::test_manual_mode_precondition_key()
{
    report_test_start("test_manual_mode_precondition_key");

    try {
        // Configure AES in manual mode WITHOUT writing key
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);
        wait(10, SC_NS);

        // Write DATA_IN but no key
        uint32_t plaintext[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Try to trigger start without key configured
        trigger_manual_start();
        wait(50, SC_NS);

        // Verify operation did not start
        uint32_t status = read_status();
        bool idle = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle || output_valid) {
            report_test_fail("test_manual_mode_precondition_key",
                           "Operation started without key configured");
            return;
        }

        // Configure key and verify operation can now start
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        report_test_pass("test_manual_mode_precondition_key");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_precondition_key", e.what());
    }
}

// =============================================================================
// Test Case 9: Manual Mode Precondition - IV Required for CBC
// =============================================================================

void testbench::test_manual_mode_precondition_iv()
{
    report_test_start("test_manual_mode_precondition_iv");

    try {
        // Configure AES-128 CBC in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, true);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write DATA_IN but no IV
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Try to trigger start without IV
        trigger_manual_start();
        wait(50, SC_NS);

        // Verify operation did not start
        uint32_t status = read_status();
        bool idle = (status & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid = (status & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!idle || output_valid) {
            report_test_fail("test_manual_mode_precondition_iv",
                           "CBC operation started without IV");
            return;
        }

        // Write IV and verify operation can now start
        uint32_t iv[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv);
        wait(10, SC_NS);

        trigger_manual_start();
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        report_test_pass("test_manual_mode_precondition_iv");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_precondition_iv", e.what());
    }
}

// =============================================================================
// Test Case 10: Manual Mode Multi-Block Without Reading Output
// =============================================================================

void testbench::test_manual_mode_multi_block_output_overwrite()
{
    report_test_start("test_manual_mode_multi_block_output_overwrite");

    try {
        // Configure AES-128 CBC in manual mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, true);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t iv[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv);
        wait(10, SC_NS);

        // Process 3 blocks consecutively WITHOUT reading outputs (except last)
        uint32_t plaintext[3][4] = {
            {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a},
            {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51},
            {0x30c81c46, 0xa35ce411, 0xe5fbc119, 0x1a0a52ef}
        };

        for (int block = 0; block < 3; block++) {
            write_data_in(plaintext[block]);
            wait(10, SC_NS);

            trigger_manual_start();
            wait(10, SC_NS);

            wait_for_output_valid(1000);

            // Only read the last block's output
            if (block == 2) {
                uint32_t ciphertext[4];
                read_data_out(ciphertext);
                wait(10, SC_NS);
            }
        }

        // Verify OUTPUT_LOST flag is set (blocks 0 and 1 were overwritten)
        uint32_t status = read_status();
        bool output_lost = (status & (1 << STATUS_OUTPUT_LOST_BIT)) != 0;

        if (!output_lost) {
            report_test_fail("test_manual_mode_multi_block_output_overwrite",
                           "OUTPUT_LOST not set after overwriting multiple blocks");
            return;
        }

        // Verify no STALL occurred
        bool stall = (status & (1 << STATUS_STALL_BIT)) != 0;

        if (stall) {
            report_test_fail("test_manual_mode_multi_block_output_overwrite",
                           "STALL flag set in manual mode");
            return;
        }

        report_test_pass("test_manual_mode_multi_block_output_overwrite");

    } catch (const std::exception& e) {
        report_test_fail("test_manual_mode_multi_block_output_overwrite", e.what());
    }
}
