#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-007: Register Interface Tests for Security Features
// =============================================================================
//
// This test suite validates REGISTER INTERFACES for security features:
// 1. CTRL_AUX_SHADOWED.KEY_TOUCH_FORCES_RESEED register access
// 2. TRIGGER.KEY_IV_DATA_IN_CLEAR register interface and clearing behavior
// 3. TRIGGER.DATA_OUT_CLEAR register interface and clearing behavior
//
// NOTE: These tests verify FUNCTIONAL INTERFACES only, not actual security
// mechanisms like PRNG randomness or masking (which are hardware-specific).
// =============================================================================

// =============================================================================
// Test Case 1: CTRL_AUX_SHADOWED.KEY_TOUCH_FORCES_RESEED Register Access
// =============================================================================
// Validates: CTRL_AUX_SHADOWED.KEY_TOUCH_FORCES_RESEED can be written and read
// Expected: Register can be set to 1 and 0, values persist correctly

void testbench::test_ctrl_aux_key_touch_forces_reseed_register()
{
    report_test_start("test_ctrl_aux_key_touch_forces_reseed_register");

    try {
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        // Test 1: Write KEY_TOUCH_FORCES_RESEED=1 and verify
        uint32_t ctrl_aux_value = 0x00000001;
        write_ctrl_aux_shadowed(ctrl_aux_value);
        wait(10, SC_NS);

        uint32_t ctrl_aux_readback = read_ctrl_aux_shadowed();
        wait(10, SC_NS);

        if ((ctrl_aux_readback & 0x1) != 0x1) {
            report_test_fail("test_ctrl_aux_key_touch_forces_reseed_register",
                           "KEY_TOUCH_FORCES_RESEED not set to 1 correctly");
            return;
        }

        // Test 2: Write KEY_TOUCH_FORCES_RESEED=0 and verify
        ctrl_aux_value = 0x00000000;
        write_ctrl_aux_shadowed(ctrl_aux_value);
        wait(10, SC_NS);

        ctrl_aux_readback = read_ctrl_aux_shadowed();
        wait(10, SC_NS);

        if ((ctrl_aux_readback & 0x1) != 0x0) {
            report_test_fail("test_ctrl_aux_key_touch_forces_reseed_register",
                           "KEY_TOUCH_FORCES_RESEED not cleared to 0 correctly");
            return;
        }

        report_test_pass("test_ctrl_aux_key_touch_forces_reseed_register");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_key_touch_forces_reseed_register", e.what());
    }
}

// =============================================================================
// Test Case 2: TRIGGER.KEY_IV_DATA_IN_CLEAR Register Interface
// =============================================================================
// Validates: TRIGGER.KEY_IV_DATA_IN_CLEAR clears KEY, IV, and DATA_IN registers
// Expected: All three register sets are modified after trigger

void testbench::test_trigger_key_iv_data_in_clear()
{
    report_test_start("test_trigger_key_iv_data_in_clear");

    try {
        // Configure and write data to KEY, IV, and DATA_IN registers
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t iv[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv);
        wait(10, SC_NS);

        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Trigger KEY_IV_DATA_IN_CLEAR (TRIGGER bit 1)
        // Note: TRIGGER is write-only
        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x00000002);
        wait(5, SC_NS);

        // Wait for clearing operation to complete
        wait(50, SC_NS);
        wait_for_idle(1000);

        // Verify IV registers were cleared (changed from original values)
        uint32_t iv_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after[i]);
        }
        wait(10, SC_NS);

        bool iv_changed = false;
        for (int i = 0; i < 4; i++) {
            if (iv_after[i] != iv[i]) {
                iv_changed = true;
                break;
            }
        }

        if (!iv_changed) {
            report_test_fail("test_trigger_key_iv_data_in_clear",
                           "IV registers not cleared (values unchanged)");
            return;
        }

        // Verify DATA_IN registers were cleared
        uint32_t data_in_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::DATA_IN_OFFSET + i*4, data_in_after[i]);
        }
        wait(10, SC_NS);

        bool data_in_changed = false;
        for (int i = 0; i < 4; i++) {
            if (data_in_after[i] != plaintext[i]) {
                data_in_changed = true;
                break;
            }
        }

        if (!data_in_changed) {
            report_test_fail("test_trigger_key_iv_data_in_clear",
                           "DATA_IN registers not cleared (values unchanged)");
            return;
        }

        // Note: KEY_SHARE registers are not readable, so we can't verify them
        // But the trigger was accepted and IV/DATA_IN were cleared, so it worked

        report_test_pass("test_trigger_key_iv_data_in_clear");

    } catch (const std::exception& e) {
        report_test_fail("test_trigger_key_iv_data_in_clear", e.what());
    }
}

// =============================================================================
// Test Case 3: TRIGGER.DATA_OUT_CLEAR Register Interface
// =============================================================================
// Validates: TRIGGER.DATA_OUT_CLEAR clears DATA_OUT registers
// Expected: DATA_OUT registers are modified after trigger

void testbench::test_trigger_data_out_clear()
{
    report_test_start("test_trigger_data_out_clear");

    try {
        // Perform an encryption to populate DATA_OUT registers
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        // Read DATA_OUT before clearing
        uint32_t ciphertext_before[4];
        read_data_out(ciphertext_before);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Trigger DATA_OUT_CLEAR (TRIGGER bit 2)
        // Note: TRIGGER is write-only
        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x00000004);
        wait(5, SC_NS);

        // Wait for clearing operation to complete
        wait(50, SC_NS);
        wait_for_idle(1000);

        // Read DATA_OUT after clearing (should be modified)
        uint32_t ciphertext_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::DATA_OUT_OFFSET + i*4, ciphertext_after[i]);
        }
        wait(10, SC_NS);

        // Verify DATA_OUT registers changed
        bool data_out_changed = false;
        for (int i = 0; i < 4; i++) {
            if (ciphertext_after[i] != ciphertext_before[i]) {
                data_out_changed = true;
                break;
            }
        }

        if (!data_out_changed) {
            report_test_fail("test_trigger_data_out_clear",
                           "DATA_OUT registers not cleared (values unchanged)");
            return;
        }

        report_test_pass("test_trigger_data_out_clear");

    } catch (const std::exception& e) {
        report_test_fail("test_trigger_data_out_clear", e.what());
    }
}
