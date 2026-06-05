#include "testbench.h"
#include "aes_basetest.h"

// =============================================================================
// OpenSSL Equivalence Tests for Sideload Key Functionality
// =============================================================================

void testbench::test_openssl_sideload_aes128_ecb_equivalence()
{
    report_test_start("test_openssl_sideload_aes128_ecb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_sideload_aes128_ecb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Setup sideload key in key manager
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        aes_if::keymgr_sideload_key_t sideload_key;
        for (int i = 0; i < 8; i++) {
            sideload_key.key_share0[i] = key_share0[i];
            sideload_key.key_share1[i] = key_share1[i];
        }
        sideload_key.valid = true;
        m_test->set_keymgr_key(sideload_key);

        // SystemC model encryption with sideload
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, true);
        wait(10, SC_NS);

        uint32_t plaintext_u32[4];
        uint8_to_uint32(plaintext, plaintext_u32, 4);
        write_data_in(plaintext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_ciphertext_u32[4];
        read_data_out(model_ciphertext_u32);
        wait(10, SC_NS);

        uint8_t model_ciphertext[16];
        uint32_to_uint8(model_ciphertext_u32, model_ciphertext, 4);

        // Compare results
        if (compare_data(openssl_ciphertext, model_ciphertext, 16)) {
            report_test_pass("test_openssl_sideload_aes128_ecb_equivalence");
        } else {
            std::string msg = "OpenSSL vs Model mismatch\n";
            msg += "OpenSSL: " + data_to_hex(openssl_ciphertext, 16) + "\n";
            msg += "Model:   " + data_to_hex(model_ciphertext, 16);
            report_test_fail("test_openssl_sideload_aes128_ecb_equivalence", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_sideload_aes128_ecb_equivalence", e.what());
    }
}

void testbench::test_openssl_sideload_aes256_cbc_equivalence()
{
    report_test_start("test_openssl_sideload_aes256_cbc_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv[16];
        generate_random_data(iv, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_sideload_aes256_cbc_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Setup sideload key in key manager
        uint32_t key_share0[8], key_share1[8], actual_key[8];
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        aes_if::keymgr_sideload_key_t sideload_key;
        for (int i = 0; i < 8; i++) {
            sideload_key.key_share0[i] = key_share0[i];
            sideload_key.key_share1[i] = key_share1[i];
        }
        sideload_key.valid = true;
        m_test->set_keymgr_key(sideload_key);

        // SystemC model encryption with sideload
        configure_aes(AES_ENC, AES_MODE_CBC, AES_256, false, true);
        wait(10, SC_NS);

        // Write IV
        uint32_t iv_u32[4];
        uint8_to_uint32(iv, iv_u32, 4);
        write_iv(iv_u32);
        wait(10, SC_NS);

        uint32_t plaintext_u32[4];
        uint8_to_uint32(plaintext, plaintext_u32, 4);
        write_data_in(plaintext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_ciphertext_u32[4];
        read_data_out(model_ciphertext_u32);
        wait(10, SC_NS);

        uint8_t model_ciphertext[16];
        uint32_to_uint8(model_ciphertext_u32, model_ciphertext, 4);

        // Compare results
        if (compare_data(openssl_ciphertext, model_ciphertext, 16)) {
            report_test_pass("test_openssl_sideload_aes256_cbc_equivalence");
        } else {
            std::string msg = "OpenSSL vs Model mismatch\n";
            msg += "OpenSSL: " + data_to_hex(openssl_ciphertext, 16) + "\n";
            msg += "Model:   " + data_to_hex(model_ciphertext, 16);
            report_test_fail("test_openssl_sideload_aes256_cbc_equivalence", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_sideload_aes256_cbc_equivalence", e.what());
    }
}

void testbench::test_openssl_sideload_ignores_key_share_writes()
{
    report_test_start("test_openssl_sideload_ignores_key_share_writes");

    try {
        // Generate sideload key (K1)
        uint8_t sideload_key_bytes[16];
        generate_random_data(sideload_key_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption with sideload key K1
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, sideload_key_bytes, 16, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_sideload_ignores_key_share_writes",
                           "OpenSSL encryption failed");
            return;
        }

        // Setup sideload key K1 in key manager
        uint32_t sideload_share0[8], sideload_share1[8], sideload_key[4];
        uint8_to_uint32(sideload_key_bytes, sideload_key, 4);
        generate_two_share_key(sideload_share0, sideload_share1, sideload_key, 4);

        aes_if::keymgr_sideload_key_t km_key;
        for (int i = 0; i < 8; i++) {
            km_key.key_share0[i] = sideload_share0[i];
            km_key.key_share1[i] = sideload_share1[i];
        }
        km_key.valid = true;
        m_test->set_keymgr_key(km_key);

        // Configure with SIDELOAD=1
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, true);
        wait(10, SC_NS);

        // Attempt to write different KEY_SHARE key K2 (should be ignored)
        uint32_t ignored_share0[8], ignored_share1[8], ignored_key[4] = {
            0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xBADDCAFE
        };
        generate_two_share_key(ignored_share0, ignored_share1, ignored_key, 4);
        write_key_shares(ignored_share0, ignored_share1, 4);
        wait(10, SC_NS);

        // Perform encryption
        uint32_t plaintext_u32[4];
        uint8_to_uint32(plaintext, plaintext_u32, 4);
        write_data_in(plaintext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_ciphertext_u32[4];
        read_data_out(model_ciphertext_u32);
        wait(10, SC_NS);

        uint8_t model_ciphertext[16];
        uint32_to_uint8(model_ciphertext_u32, model_ciphertext, 4);

        // Compare results - should match OpenSSL with K1, not K2
        if (compare_data(openssl_ciphertext, model_ciphertext, 16)) {
            report_test_pass("test_openssl_sideload_ignores_key_share_writes");
        } else {
            std::string msg = "Sideload key not used (KEY_SHARE writes not ignored)\n";
            msg += "Expected (K1): " + data_to_hex(openssl_ciphertext, 16) + "\n";
            msg += "Model:         " + data_to_hex(model_ciphertext, 16);
            report_test_fail("test_openssl_sideload_ignores_key_share_writes", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_sideload_ignores_key_share_writes", e.what());
    }
}

// =============================================================================
// Test Case 2: IV Write Protection When Busy
// =============================================================================

void testbench::test_iv_write_protection_when_non_idle()
{
    report_test_start("test_iv_write_protection_when_non_idle");

    try {
        // Configure AES for AUTOMATIC mode (key insight: stays non-idle until output read)
        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);  // automatic mode
        wait(10, SC_NS);

        // Write key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
        generate_two_share_key(key_share0, key_share1, actual_key, 4);
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Write initial IV values
        uint32_t initial_iv[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        for (int i = 0; i < 4; i++) {
            m_test->register_write_32(aes_basetest::IV_OFFSET + (i * 4), initial_iv[i]);
        }
        wait(10, SC_NS);

        // Read back to confirm initial values
        uint32_t iv_before[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + (i * 4), iv_before[i]);
        }

        // Perform encryption - in automatic mode, module stays non-idle until output is read
        uint32_t plaintext[4] = {0xAABBCCDD, 0xEEFF0011, 0x22334455, 0x66778899};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Wait for encryption to complete (output valid, but not yet read)
        wait_for_output_valid(1000);

        // Verify non-idle state (automatic mode keeps non-idle until output read)
        uint32_t status = read_status();
        bool is_idle = (status & (1 << STATUS_IDLE_BIT)) != 0;
        if (is_idle) {
            report_test_fail("test_iv_write_protection_when_non_idle",
                           "Module is idle (should be non-idle until output read in automatic mode)");
            return;
        }

        // Attempt to write different IV values (should be ignored due to non-idle)
        uint32_t new_iv[4] = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};
        for (int i = 0; i < 4; i++) {
            m_test->register_write_32(aes_basetest::IV_OFFSET + (i * 4), new_iv[i]);
        }
        wait(10, SC_NS);

        // Read back IV and verify values unchanged (writes were ignored)
        uint32_t iv_during_nonidle[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + (i * 4), iv_during_nonidle[i]);
        }

        bool all_unchanged = true;
        for (int i = 0; i < 4; i++) {
            // Note: IV auto-updates in CBC mode, so we check that our writes were ignored
            // If IV matches our attempted write, write protection failed
            if (iv_during_nonidle[i] == new_iv[i]) {
                all_unchanged = false;
                break;
            }
        }

        // Read output to return module to idle
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Verify module returned to idle after output read
        status = read_status();
        is_idle = (status & (1 << STATUS_IDLE_BIT)) != 0;

        if (all_unchanged && is_idle) {
            report_test_pass("test_iv_write_protection_when_non_idle");
        } else {
            if (!all_unchanged) {
                report_test_fail("test_iv_write_protection_when_non_idle",
                               "IV was modified when non-idle (write protection failed)");
            } else {
                report_test_fail("test_iv_write_protection_when_non_idle",
                               "Module did not return to idle after output read");
            }
        }

    } catch (const std::exception& e) {
        report_test_fail("test_iv_write_protection_when_non_idle", e.what());
    }
}


void testbench::test_ctrl_shadowed_write_protection_when_non_idle()
{
    report_test_start("test_ctrl_shadowed_write_protection_when_non_idle");

    try {
        // Configure AES for AUTOMATIC mode with initial settings
        uint32_t initial_ctrl = 0;
        initial_ctrl |= (AES_ENC & 0x3);           // OPERATION [1:0]
        initial_ctrl |= ((AES_MODE_ECB & 0x3F) << 2);  // MODE [7:2]
        initial_ctrl |= (0x1 << 8);                // KEY_LEN [10:8] = AES_128
        initial_ctrl |= (0x0 << 11);               // MANUAL_OPERATION = 0 (automatic)
        initial_ctrl |= (0x1 << 12);               // PRNG_RESEED_RATE = PER_1

        // Two-write protocol
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, initial_ctrl);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, initial_ctrl);
        wait(10, SC_NS);

        // Read back to confirm
        uint32_t ctrl_before;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_before);

        // Write key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
        generate_two_share_key(key_share0, key_share1, actual_key, 4);
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Perform encryption - in automatic mode, module stays non-idle until output is read
        uint32_t plaintext[4] = {0xAABBCCDD, 0xEEFF0011, 0x22334455, 0x66778899};
        write_data_in(plaintext);
        wait(10, SC_NS);

        // Wait for encryption to complete (output valid, but not yet read)
        wait_for_output_valid(1000);

        // Verify non-idle state
        uint32_t status = read_status();
        bool is_idle = (status & (1 << STATUS_IDLE_BIT)) != 0;
        if (is_idle) {
            report_test_fail("test_ctrl_shadowed_write_protection_when_non_idle",
                           "Module is idle (should be non-idle until output read in automatic mode)");
            return;
        }

        // Attempt to write different configuration (should be ignored due to non-idle)
        uint32_t new_ctrl = 0;
        new_ctrl |= (AES_DEC & 0x3);           // OPERATION [1:0] = DEC (different)
        new_ctrl |= ((AES_MODE_CBC & 0x3F) << 2);  // MODE [7:2] = CBC (different)
        new_ctrl |= (0x4 << 8);                // KEY_LEN [10:8] = AES_256 (different)
        new_ctrl |= (0x1 << 11);               // MANUAL_OPERATION = 1 (different)
        new_ctrl |= (0x2 << 12);               // PRNG_RESEED_RATE = PER_64 (different)

        // Attempt two-write protocol
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, new_ctrl);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, new_ctrl);
        wait(10, SC_NS);

        // Read back and verify value unchanged (write was ignored)
        uint32_t ctrl_during_nonidle;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_during_nonidle);

        // Read output to return module to idle
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Verify module returned to idle
        status = read_status();
        is_idle = (status & (1 << STATUS_IDLE_BIT)) != 0;

        if (ctrl_during_nonidle == initial_ctrl && is_idle) {
            report_test_pass("test_ctrl_shadowed_write_protection_when_non_idle");
        } else {
            if (ctrl_during_nonidle != initial_ctrl) {
                report_test_fail("test_ctrl_shadowed_write_protection_when_non_idle",
                               "CTRL_SHADOWED was modified when non-idle (write protection failed)");
            } else {
                report_test_fail("test_ctrl_shadowed_write_protection_when_non_idle",
                               "Module did not return to idle after output read");
            }
        }

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_shadowed_write_protection_when_non_idle", e.what());
    }
}


// =============================================================================
// OpenSSL Equivalence Test for Invalid Key Length Handling
// =============================================================================

void testbench::test_openssl_invalid_key_length_defaults_to_aes256()
{
    report_test_start("test_openssl_invalid_key_length_defaults_to_aes256");

    try {
        // Generate test vectors for AES-256
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption with AES-256
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_invalid_key_length_defaults_to_aes256",
                           "OpenSSL encryption failed");
            return;
        }

        // Configure with INVALID KEY_LEN encoding (0x3)
        wait_for_idle();

        uint32_t ctrl_val = 0;
        ctrl_val |= (AES_ENC & 0x3);           // OPERATION [1:0]
        ctrl_val |= ((AES_MODE_ECB & 0x3F) << 2);  // MODE [7:2]
        ctrl_val |= (0x3 << 8);                // KEY_LEN [10:8] = 0x3 (invalid, should default to AES-256)
        ctrl_val |= (0x1 << 12);               // PRNG_RESEED_RATE = PER_1

        // Write CTRL_SHADOWED twice
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
        wait(10, SC_NS);

        // Write 256-bit key (all 8 registers)
        uint32_t key_share0[8], key_share1[8], actual_key[8];
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);
        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        // Perform encryption
        uint32_t plaintext_u32[4];
        uint8_to_uint32(plaintext, plaintext_u32, 4);
        write_data_in(plaintext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_ciphertext_u32[4];
        read_data_out(model_ciphertext_u32);
        wait(10, SC_NS);

        uint8_t model_ciphertext[16];
        uint32_to_uint8(model_ciphertext_u32, model_ciphertext, 4);

        // Compare results - should match AES-256 encryption
        if (compare_data(openssl_ciphertext, model_ciphertext, 16)) {
            report_test_pass("test_openssl_invalid_key_length_defaults_to_aes256");
        } else {
            std::string msg = "Invalid KEY_LEN did not default to AES-256\n";
            msg += "Expected (AES-256): " + data_to_hex(openssl_ciphertext, 16) + "\n";
            msg += "Model:              " + data_to_hex(model_ciphertext, 16);
            report_test_fail("test_openssl_invalid_key_length_defaults_to_aes256", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_invalid_key_length_defaults_to_aes256", e.what());
    }
}
