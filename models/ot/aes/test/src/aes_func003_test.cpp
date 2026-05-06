#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-003: Initialization Vector Management Test Implementations
// =============================================================================

// =============================================================================
// Test Case 1: IV Register Read/Write Interface
// =============================================================================

void testbench::test_iv_register_read_write()
{
    report_test_start("test_iv_register_read_write");

    try {
        // Write known values to all four IV registers
        uint32_t iv_write[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
        write_iv(iv_write);
        wait(10, SC_NS);

        // Read back IV registers
        uint32_t iv_read[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_read[i]);
        }
        wait(10, SC_NS);

        // Verify all four registers match
        bool match = true;
        for (int i = 0; i < 4; i++) {
            if (iv_read[i] != iv_write[i]) {
                match = false;
                std::stringstream ss;
                ss << "IV_" << i << " mismatch: wrote 0x" << std::hex << iv_write[i]
                   << ", read 0x" << iv_read[i];
                report_test_fail("test_iv_register_read_write", ss.str());
                return;
            }
        }

        if (match) {
            report_test_pass("test_iv_register_read_write");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_iv_register_read_write", e.what());
    }
}


// =============================================================================
// Test Case 3: CBC Encryption IV Auto-Update
// =============================================================================

void testbench::test_cbc_encryption_iv_auto_update()
{
    report_test_start("test_cbc_encryption_iv_auto_update");

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

        // Encrypt first block
        uint32_t plaintext1[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV registers - should equal first ciphertext block
        uint32_t iv_after_block1[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block1[i]);
        }
        wait(10, SC_NS);

        // Verify IV updated to ciphertext
        for (int i = 0; i < 4; i++) {
            if (iv_after_block1[i] != ciphertext1[i]) {
                std::stringstream ss;
                ss << "IV_" << i << " not updated: ciphertext=0x" << std::hex << ciphertext1[i]
                   << ", IV=0x" << iv_after_block1[i];
                report_test_fail("test_cbc_encryption_iv_auto_update", ss.str());
                return;
            }
        }

        // Encrypt second block with auto-updated IV
        uint32_t plaintext2[4] = {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51};
        write_data_in(plaintext2);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV again - should equal second ciphertext block
        uint32_t iv_after_block2[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block2[i]);
        }
        wait(10, SC_NS);

        // Verify second IV update
        for (int i = 0; i < 4; i++) {
            if (iv_after_block2[i] != ciphertext2[i]) {
                std::stringstream ss;
                ss << "IV_" << i << " not updated after block 2: ciphertext=0x" << std::hex
                   << ciphertext2[i] << ", IV=0x" << iv_after_block2[i];
                report_test_fail("test_cbc_encryption_iv_auto_update", ss.str());
                return;
            }
        }

        report_test_pass("test_cbc_encryption_iv_auto_update");

    } catch (const std::exception& e) {
        report_test_fail("test_cbc_encryption_iv_auto_update", e.what());
    }
}

// =============================================================================
// Test Case 4: CBC Decryption IV Auto-Update
// =============================================================================

void testbench::test_cbc_decryption_iv_auto_update()
{
    report_test_start("test_cbc_decryption_iv_auto_update");

    try {
        // Configure AES-128 CBC decryption
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_DEC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        wait_for_idle(2000); // Wait for key expansion

        // Write initial IV
        uint32_t iv_initial[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Decrypt first ciphertext block
        uint32_t ciphertext1[4] = {0x76493726, 0x0dabf3fd, 0x21c72ba1, 0x6d0c9f8e};
        write_data_in(ciphertext1);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t plaintext1[4];
        read_data_out(plaintext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV - should equal first input ciphertext block
        uint32_t iv_after_block1[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block1[i]);
        }
        wait(10, SC_NS);

        // Verify IV updated to input ciphertext
        bool iv_updated = true;
        for (int i = 0; i < 4; i++) {
            if (iv_after_block1[i] != ciphertext1[i]) {
                iv_updated = false;
                std::stringstream ss;
                ss << "IV_" << i << " not updated: input_ciphertext=0x" << std::hex
                   << ciphertext1[i] << ", IV=0x" << iv_after_block1[i];
                report_test_fail("test_cbc_decryption_iv_auto_update", ss.str());
                return;
            }
        }

        // Decrypt second block
        uint32_t ciphertext2[4] = {0x51527392, 0xe1a0f5c6, 0x8d4bafb7, 0x9c0ee293};
        write_data_in(ciphertext2);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t plaintext2[4];
        read_data_out(plaintext2);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Verify second IV update
        uint32_t iv_after_block2[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block2[i]);
        }
        wait(10, SC_NS);

        for (int i = 0; i < 4; i++) {
            if (iv_after_block2[i] != ciphertext2[i]) {
                iv_updated = false;
                std::stringstream ss;
                ss << "IV_" << i << " not updated after block 2";
                report_test_fail("test_cbc_decryption_iv_auto_update", ss.str());
                return;
            }
        }

        if (iv_updated) {
            report_test_pass("test_cbc_decryption_iv_auto_update");
        } else {
            report_test_fail("test_cbc_decryption_iv_auto_update", "IV not updated");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_cbc_decryption_iv_auto_update", e.what());
    }
}

// =============================================================================
// Test Case 5: CFB Mode IV Auto-Update
// =============================================================================

void testbench::test_cfb_mode_iv_auto_update()
{
    report_test_start("test_cfb_mode_iv_auto_update");

    try {
        // Configure AES-128 CFB mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write initial IV
        uint32_t iv_initial[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext1[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV - should equal output ciphertext
        uint32_t iv_after_block1[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block1[i]);
        }
        wait(10, SC_NS);

        bool iv_updated = true;
        // Verify IV updated to ciphertext
        for (int i = 0; i < 4; i++) {
            if (iv_after_block1[i] != ciphertext1[i]) {
                iv_updated = false;
                std::stringstream ss;
                ss << "CFB IV_" << i << " not updated to ciphertext";
                report_test_fail("test_cfb_mode_iv_auto_update", ss.str());
                return;
            }
        }

        // Encrypt second block
        uint32_t plaintext2[4] = {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51};
        write_data_in(plaintext2);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV - should equal output ciphertext of block 2
        uint32_t iv_after_block2[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block2[i]);
        }
        wait(10, SC_NS);

        // Verify IV updated to ciphertext
        for (int i = 0; i < 4; i++) {
            if (iv_after_block2[i] != ciphertext2[i]) {
                iv_updated = false;
                std::stringstream ss;
                ss << "CFB IV_" << i << " not updated to ciphertext";
                report_test_fail("test_cfb_mode_iv_auto_update", ss.str());
                return;
            }
        }

        if (iv_updated) {
            report_test_pass("test_cfb_mode_iv_auto_update");
        } else {
            report_test_fail("test_cfb_mode_iv_auto_update", "IV not updated");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_cfb_mode_iv_auto_update", e.what());
    }
}

// =============================================================================
// Test Case 6: OFB Mode IV Auto-Update
// =============================================================================

void testbench::test_ofb_mode_iv_auto_update()
{
    report_test_start("test_ofb_mode_iv_auto_update");

    try {
        // Configure AES-128 OFB mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_OFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write initial IV
        uint32_t iv_initial[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Encrypt first block with plaintext pattern
        uint32_t plaintext1[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext1);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV after first block - should be updated (cipher output before XOR)
        uint32_t iv_after_block1[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after_block1[i]);
        }
        wait(10, SC_NS);

        // IV should be different from initial IV (proving it was updated)
        bool iv_changed = false;
        for (int i = 0; i < 4; i++) {
            if (iv_after_block1[i] != iv_initial[i]) {
                iv_changed = true;
                break;
            }
        }

        if (!iv_changed) {
            report_test_fail("test_ofb_mode_iv_auto_update",
                           "IV not updated after first block");
            return;
        }

        // Encrypt second block with SAME plaintext
        write_data_in(plaintext1);  // Same plaintext as block 1
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        // Verify outputs are different (proving IV changed between blocks)
        bool outputs_differ = false;
        for (int i = 0; i < 4; i++) {
            if (ciphertext1[i] != ciphertext2[i]) {
                outputs_differ = true;
                break;
            }
        }

        if (!outputs_differ) {
            report_test_fail("test_ofb_mode_iv_auto_update",
                           "Same plaintext produced same output - IV not changing");
            return;
        }

        report_test_pass("test_ofb_mode_iv_auto_update");

    } catch (const std::exception& e) {
        report_test_fail("test_ofb_mode_iv_auto_update", e.what());
    }
}

// =============================================================================
// Test Case 7: CTR Mode Counter Increment
// =============================================================================

void testbench::test_ctr_mode_counter_increment()
{
    report_test_start("test_ctr_mode_counter_increment");

    try {
        // Configure AES-128 CTR mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CTR, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write initial counter (big-endian representation: 0x00000000000000000000000000000001)
        // In little-endian register layout: IV_0=0x01, IV_1=0x00, IV_2=0x00, IV_3=0x00
        uint32_t counter_initial[4] = {0x00000001, 0x00000000, 0x00000000, 0x00000000};
        write_iv(counter_initial);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read counter - should be incremented to 2
        uint32_t counter_after_block1[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, counter_after_block1[i]);
        }
        wait(10, SC_NS);

        // Expected: counter = 2 (0x00000002 in IV_0, rest zeros)
        if (counter_after_block1[0] != 0x00000002) {
            std::stringstream ss;
            ss << "Counter not incremented correctly: expected 0x00000002, got 0x"
               << std::hex << counter_after_block1[0];
            report_test_fail("test_ctr_mode_counter_increment", ss.str());
            return;
        }

        // Encrypt second block
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read counter - should be incremented to 3
        uint32_t counter_after_block2[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, counter_after_block2[i]);
        }
        wait(10, SC_NS);

        if (counter_after_block2[0] != 0x00000003) {
            std::stringstream ss;
            ss << "Counter not incremented to 3: got 0x" << std::hex << counter_after_block2[0];
            report_test_fail("test_ctr_mode_counter_increment", ss.str());
            return;
        }

        report_test_pass("test_ctr_mode_counter_increment");

    } catch (const std::exception& e) {
        report_test_fail("test_ctr_mode_counter_increment", e.what());
    }
}

// =============================================================================
// Test Case 8: CTR Counter Overflow/Carry Propagation
// =============================================================================

void testbench::test_ctr_counter_overflow()
{
    report_test_start("test_ctr_counter_overflow");

    try {
        // Configure AES-128 CTR mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CTR, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Set counter near overflow: 0x000000000000000000000000FFFFFFFF
        // Little-endian: IV_0=0xFFFFFFFF, IV_1=0x00, IV_2=0x00, IV_3=0x00
        uint32_t counter_near_overflow[4] = {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000};
        write_iv(counter_near_overflow);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read counter - should wrap with carry propagation
        // Expected: 0x0000000000000000000000010000000
        // Little-endian: IV_0=0x00, IV_1=0x01, IV_2=0x00, IV_3=0x00
        uint32_t counter_after_overflow[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, counter_after_overflow[i]);
        }
        wait(10, SC_NS);

        // Verify big-endian carry propagation
        if (counter_after_overflow[0] != 0x00000000 || counter_after_overflow[1] != 0x00000001) {
            std::stringstream ss;
            ss << "Counter overflow incorrect: IV_0=0x" << std::hex << counter_after_overflow[0]
               << ", IV_1=0x" << counter_after_overflow[1]
               << " (expected IV_0=0x0, IV_1=0x1)";
            report_test_fail("test_ctr_counter_overflow", ss.str());
            return;
        }

        report_test_pass("test_ctr_counter_overflow");

    } catch (const std::exception& e) {
        report_test_fail("test_ctr_counter_overflow", e.what());
    }
}

// =============================================================================
// Test Case 9: ECB Mode Ignores IV
// =============================================================================

void testbench::test_ecb_mode_ignores_iv()
{
    report_test_start("test_ecb_mode_ignores_iv");

    try {
        // Configure AES-128 ECB mode
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write IV with known value
        uint32_t iv_value[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
        write_iv(iv_value);
        wait(10, SC_NS);

        // Encrypt first block
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext1[4];
        read_data_out(ciphertext1);
        wait(10, SC_NS);

        wait_for_idle(1000);

        // Read IV - should be unchanged in ECB mode
        uint32_t iv_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after[i]);
        }
        wait(10, SC_NS);

        // Verify IV unchanged
        for (int i = 0; i < 4; i++) {
            if (iv_after[i] != iv_value[i]) {
                std::stringstream ss;
                ss << "ECB mode modified IV_" << i << ": was 0x" << std::hex << iv_value[i]
                   << ", now 0x" << iv_after[i];
                report_test_fail("test_ecb_mode_ignores_iv", ss.str());
                return;
            }
        }

        // Encrypt second block with same plaintext
        write_data_in(plaintext);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t ciphertext2[4];
        read_data_out(ciphertext2);
        wait(10, SC_NS);

        // Verify identical output (no IV involvement)
        for (int i = 0; i < 4; i++) {
            if (ciphertext1[i] != ciphertext2[i]) {
                report_test_fail("test_ecb_mode_ignores_iv",
                               "Same plaintext produced different output in ECB mode");
                return;
            }
        }

        report_test_pass("test_ecb_mode_ignores_iv");

    } catch (const std::exception& e) {
        report_test_fail("test_ecb_mode_ignores_iv", e.what());
    }
}

// =============================================================================
// Test Case 10: IV Required for Non-ECB Auto-Start
// =============================================================================

void testbench::test_iv_required_for_non_ecb_auto_start()
{
    report_test_start("test_iv_required_for_non_ecb_auto_start");

    try {
        // Configure AES-128 CBC mode (requires IV)
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);  // automatic mode
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write only 3 of 4 IV registers (incomplete IV)
        for (int i = 0; i < 3; i++) {
            m_test->register_write_32(aes_basetest::IV_OFFSET + i*4, 0x11111111 * (i+1));
        }
        wait(10, SC_NS);

        // Write DATA_IN (should NOT auto-start yet because IV is incomplete)
        uint32_t plaintext[4] = {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a};
        write_data_in(plaintext);
        wait(20, SC_NS);

        // Verify operation did NOT start - check idle and output_valid status
        uint32_t status_before = read_status();
        bool is_idle_before = (status_before & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_before = (status_before & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (!is_idle_before) {
            report_test_fail("test_iv_required_for_non_ecb_auto_start",
                           "AES started operation with incomplete IV (should remain idle)");
            return;
        }

        if (output_valid_before) {
            report_test_fail("test_iv_required_for_non_ecb_auto_start",
                           "OUTPUT_VALID set with incomplete IV (should be 0)");
            return;
        }

        // Now write the fourth IV register to complete the IV
        // This should trigger auto-start since all conditions are now met
        m_test->register_write_32(aes_basetest::IV_OFFSET + 3*4, 0x44444444);
        wait(10, SC_NS);

        // Operation should now auto-start (triggered by completing IV)
        // Wait for output to become valid
        wait_for_output_valid(1000);

        // Verify operation started - check status
        uint32_t status_after = read_status();
        bool is_idle_after = (status_after & (1 << STATUS_IDLE_BIT)) != 0;
        bool output_valid_after = (status_after & (1 << STATUS_OUTPUT_VALID_BIT)) != 0;

        if (is_idle_after) {
            report_test_fail("test_iv_required_for_non_ecb_auto_start",
                           "AES still idle after completing IV (should have started)");
            return;
        }

        if (!output_valid_after) {
            report_test_fail("test_iv_required_for_non_ecb_auto_start",
                           "OUTPUT_VALID not set after operation (should be 1)");
            return;
        }

        // Read output to complete the test
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        report_test_pass("test_iv_required_for_non_ecb_auto_start");

    } catch (const std::exception& e) {
        report_test_fail("test_iv_required_for_non_ecb_auto_start", e.what());
    }
}

// =============================================================================
// Test Case 11: Multi-Block CBC Chaining
// =============================================================================

void testbench::test_multi_block_cbc_chaining()
{
    report_test_start("test_multi_block_cbc_chaining");

    try {
        // Configure AES-256 CBC encryption
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {
            0x603deb10, 0x15ca71be, 0x2b73aef0, 0x857d7781,
            0x1f352c07, 0x3b6108d7, 0x2d9810a3, 0x0914dff4
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        configure_aes(AES_ENC, AES_MODE_CBC, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        // Write initial IV
        uint32_t iv_initial[4] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f};
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Encrypt 4 blocks
        uint32_t plaintext_blocks[4][4] = {
            {0x6bc1bee2, 0x2e409f96, 0xe93d7e11, 0x7393172a},
            {0xae2d8a57, 0x1e03ac9c, 0x9eb76fac, 0x45af8e51},
            {0x30c81c46, 0xa35ce411, 0xe5fbc119, 0x1a0a52ef},
            {0xf69f2445, 0xdf4f9b17, 0xad2b417b, 0xe66c3710}
        };

        uint32_t ciphertext_blocks[4][4];

        for (int block = 0; block < 4; block++) {
            write_data_in(plaintext_blocks[block]);
            wait(10, SC_NS);

            wait_for_output_valid(1000);

            read_data_out(ciphertext_blocks[block]);
            wait(10, SC_NS);

            wait_for_idle(1000);
        }

        // Now decrypt the ciphertext chain with same initial IV
        configure_aes(AES_DEC, AES_MODE_CBC, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        wait_for_idle(2000); // Wait for key expansion

        // Write same initial IV
        write_iv(iv_initial);
        wait(10, SC_NS);

        // Decrypt all blocks
        uint32_t decrypted_blocks[4][4];

        for (int block = 0; block < 4; block++) {
            write_data_in(ciphertext_blocks[block]);
            wait(10, SC_NS);

            wait_for_output_valid(1000);

            read_data_out(decrypted_blocks[block]);
            wait(10, SC_NS);

            wait_for_idle(1000);
        }

        // Verify plaintext recovery
        for (int block = 0; block < 4; block++) {
            for (int word = 0; word < 4; word++) {
                if (decrypted_blocks[block][word] != plaintext_blocks[block][word]) {
                    std::stringstream ss;
                    ss << "Block " << block << " word " << word << " mismatch after decrypt";
                    report_test_fail("test_multi_block_cbc_chaining", ss.str());
                    return;
                }
            }
        }

        report_test_pass("test_multi_block_cbc_chaining");

    } catch (const std::exception& e) {
        report_test_fail("test_multi_block_cbc_chaining", e.what());
    }
}