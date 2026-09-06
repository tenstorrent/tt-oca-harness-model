// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"
#include <cstdlib>
#include <ctime>

// Seed random number generator once
static struct RandomInit {
    RandomInit() { srand(static_cast<unsigned>(time(nullptr))); }
} random_init;

// =============================================================================
// High-Level AES Configuration Methods
// =============================================================================

void testbench::configure_aes(operation_e op, mode_e mode, key_len_e key_len,
                              bool manual_mode, bool sideload)
{
    // Wait for AES to be idle before configuration
    wait_for_idle();

    // Build CTRL_SHADOWED register value
    // Bit layout: [15:MANUAL_OPERATION][14:12:PRNG_RESEED_RATE][11:SIDELOAD]
    //             [10:8:KEY_LEN][7:2:MODE][1:0:OPERATION]
    uint32_t ctrl_val = 0;
    ctrl_val |= (op & 0x3);                      // OPERATION [1:0]
    ctrl_val |= ((mode & 0x3F) << 2);            // MODE [7:2]
    ctrl_val |= ((key_len & 0x7) << 8);          // KEY_LEN [10:8]
    ctrl_val |= (sideload ? (1 << 11) : 0);      // SIDELOAD [11]
    ctrl_val |= (0x1 << 12);                     // PRNG_RESEED_RATE = PER_1 (default)
    ctrl_val |= (manual_mode ? (1 << 15) : 0);   // MANUAL_OPERATION [15]

    // Shadowed register requires two consecutive matching writes
    m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    wait(1, SC_NS);
    m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    wait(1, SC_NS);
}

void testbench::write_key_shares(const uint32_t* key_share0,
                                         const uint32_t* key_share1,
                                         int num_words)
{
    // Write KEY_SHARE0 registers (0x04-0x20, 8 registers)
    for (int i = 0; i < 8; i++) {
        uint32_t value = (i < num_words) ? key_share0[i] : 0;
        m_test->register_write_32(aes_basetest::KEY_SHARE0_OFFSET + (i * 4), value);
        wait(1, SC_NS);
    }

    // Write KEY_SHARE1 registers (0x24-0x40, 8 registers)
    for (int i = 0; i < 8; i++) {
        uint32_t value = (i < num_words) ? key_share1[i] : 0;
        m_test->register_write_32(aes_basetest::KEY_SHARE1_OFFSET + (i * 4), value);
        wait(1, SC_NS);
    }
}

void testbench::write_iv(const uint32_t* iv)
{
    // Write IV registers (0x44-0x50, 4 registers)
    for (int i = 0; i < 4; i++) {
        m_test->register_write_32(aes_basetest::IV_OFFSET + (i * 4), iv[i]);
        wait(1, SC_NS);
    }
}

void testbench::write_data_in(const uint32_t* data)
{
    // Write DATA_IN registers (0x54-0x60, 4 registers)
    for (int i = 0; i < 4; i++) {
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + (i * 4), data[i]);
        wait(1, SC_NS);
    }
}

void testbench::read_data_out(uint32_t* data)
{
    // Read DATA_OUT registers (0x64-0x70, 4 registers)
    for (int i = 0; i < 4; i++) {
        m_test->register_read_32(aes_basetest::DATA_OUT_OFFSET + (i * 4), data[i]);
        wait(1, SC_NS);
    }
}

uint32_t testbench::read_status()
{
    uint32_t status = 0;
    m_test->register_read_32(aes_basetest::STATUS_OFFSET, status);
    return status;
}

void testbench::wait_for_idle(int timeout_ns)
{
    int elapsed = 0;
    while (elapsed < timeout_ns) {
        uint32_t status = read_status();
        if (status & (1 << STATUS_IDLE_BIT)) {
            return;
        }
        wait(10, SC_NS);
        elapsed += 10;
    }
    REG_WARN(1, logger) << "Timeout waiting for AES IDLE" << std::endl;
}

void testbench::wait_for_output_valid(int timeout_ns)
{
    int elapsed = 0;
    while (elapsed < timeout_ns) {
        uint32_t status = read_status();
        if (status & (1 << STATUS_OUTPUT_VALID_BIT)) {
            return;
        }
        wait(10, SC_NS);
        elapsed += 10;
    }
    REG_WARN(1, logger) << "Timeout waiting for OUTPUT_VALID" << std::endl;
}

void testbench::trigger_manual_start()
{
    // Write 1 to TRIGGER.START (bit 0)
    m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x1);
    wait(1, SC_NS);
}

void testbench::trigger_key_iv_data_in_clear()
{
    // Write 1 to TRIGGER.KEY_IV_DATA_IN_CLEAR (bit 1)
    m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x2);
    wait(1, SC_NS);
}

void testbench::trigger_prng_reseed()
{
    // Write 1 to TRIGGER.PRNG_RESEED (bit 3)
    m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x8);
    wait(1, SC_NS);
}

uint32_t testbench::read_ctrl_aux_shadowed()
{
    uint32_t value;
    m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, value);
    return value;
}

void testbench::write_ctrl_aux_shadowed(uint32_t value)
{
    // Shadowed register requires two consecutive matching writes
    m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, value);
    wait(1, SC_NS);
    m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, value);
    wait(1, SC_NS);
}

void testbench::write_ctrl_aux_regwen(uint32_t value)
{
    m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, value);
    wait(1, SC_NS);
}

// =============================================================================
// OpenSSL Reference Implementation Methods
// =============================================================================

bool testbench::openssl_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, mode_e mode,
                                        uint8_t* ciphertext, size_t& ciphertext_len)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }

    const EVP_CIPHER* cipher = nullptr;

    // Select OpenSSL cipher based on mode and key length
    switch (mode) {
        case AES_MODE_ECB:
            if (key_len == 16) cipher = EVP_aes_128_ecb();
            else if (key_len == 24) cipher = EVP_aes_192_ecb();
            else if (key_len == 32) cipher = EVP_aes_256_ecb();
            break;
        case AES_MODE_CBC:
            if (key_len == 16) cipher = EVP_aes_128_cbc();
            else if (key_len == 24) cipher = EVP_aes_192_cbc();
            else if (key_len == 32) cipher = EVP_aes_256_cbc();
            break;
        case AES_MODE_CFB:
            if (key_len == 16) cipher = EVP_aes_128_cfb128();
            else if (key_len == 24) cipher = EVP_aes_192_cfb128();
            else if (key_len == 32) cipher = EVP_aes_256_cfb128();
            break;
        case AES_MODE_OFB:
            if (key_len == 16) cipher = EVP_aes_128_ofb();
            else if (key_len == 24) cipher = EVP_aes_192_ofb();
            else if (key_len == 32) cipher = EVP_aes_256_ofb();
            break;
        case AES_MODE_CTR:
            if (key_len == 16) cipher = EVP_aes_128_ctr();
            else if (key_len == 24) cipher = EVP_aes_192_ctr();
            else if (key_len == 32) cipher = EVP_aes_256_ctr();
            break;
        default:
            EVP_CIPHER_CTX_free(ctx);
            return false;
    }

    if (!cipher) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Initialize encryption operation
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Disable padding for block cipher modes
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    int ciphertext_len_int = 0;

    // Perform encryption
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len_int = len;

    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len_int += len;

    ciphertext_len = ciphertext_len_int;
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool testbench::openssl_decrypt(const uint8_t* ciphertext, size_t ciphertext_len,
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, mode_e mode,
                                        uint8_t* plaintext, size_t& plaintext_len)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }

    const EVP_CIPHER* cipher = nullptr;

    // Select OpenSSL cipher based on mode and key length
    switch (mode) {
        case AES_MODE_ECB:
            if (key_len == 16) cipher = EVP_aes_128_ecb();
            else if (key_len == 24) cipher = EVP_aes_192_ecb();
            else if (key_len == 32) cipher = EVP_aes_256_ecb();
            break;
        case AES_MODE_CBC:
            if (key_len == 16) cipher = EVP_aes_128_cbc();
            else if (key_len == 24) cipher = EVP_aes_192_cbc();
            else if (key_len == 32) cipher = EVP_aes_256_cbc();
            break;
        case AES_MODE_CFB:
            if (key_len == 16) cipher = EVP_aes_128_cfb128();
            else if (key_len == 24) cipher = EVP_aes_192_cfb128();
            else if (key_len == 32) cipher = EVP_aes_256_cfb128();
            break;
        case AES_MODE_OFB:
            if (key_len == 16) cipher = EVP_aes_128_ofb();
            else if (key_len == 24) cipher = EVP_aes_192_ofb();
            else if (key_len == 32) cipher = EVP_aes_256_ofb();
            break;
        case AES_MODE_CTR:
            if (key_len == 16) cipher = EVP_aes_128_ctr();
            else if (key_len == 24) cipher = EVP_aes_192_ctr();
            else if (key_len == 32) cipher = EVP_aes_256_ctr();
            break;
        default:
            EVP_CIPHER_CTX_free(ctx);
            return false;
    }

    if (!cipher) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Initialize decryption operation
    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    // Disable padding for block cipher modes
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    int plaintext_len_int = 0;

    // Perform decryption
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len_int = len;

    // Finalize decryption
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len_int += len;

    plaintext_len = plaintext_len_int;
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// =============================================================================
// Test Utility Methods
// =============================================================================

void testbench::generate_random_data(uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        data[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
}

void testbench::generate_two_share_key(uint32_t* key_share0,
                                               uint32_t* key_share1,
                                               uint32_t* actual_key,
                                               int num_words)
{
    // NOTE: This function can work in two modes:
    // 1. If actual_key contains zeros, generate a random key
    // 2. If actual_key is pre-initialized, use it (for equivalence tests)

    // Check if actual_key is initialized (non-zero)
    bool key_initialized = false;
    for (int i = 0; i < num_words; i++) {
        if (actual_key[i] != 0) {
            key_initialized = true;
            break;
        }
    }

    // If not initialized, generate random key
    if (!key_initialized) {
        for (int i = 0; i < num_words; i++) {
            actual_key[i] = rand();
        }
    }

    // Generate random key_share0
    for (int i = 0; i < num_words; i++) {
        key_share0[i] = rand();
    }

    // Compute key_share1 = actual_key XOR key_share0
    // This ensures: key_share0 XOR key_share1 = actual_key
    for (int i = 0; i < num_words; i++) {
        key_share1[i] = actual_key[i] ^ key_share0[i];
    }

    // Fill remaining words with zeros if array is larger than num_words
    for (int i = num_words; i < 8; i++) {
        key_share0[i] = 0;
        key_share1[i] = 0;
    }
}

bool testbench::compare_data(const uint8_t* data1, const uint8_t* data2, size_t len)
{
    return memcmp(data1, data2, len) == 0;
}

std::string testbench::data_to_hex(const uint8_t* data, size_t len)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        ss << std::setw(2) << static_cast<int>(data[i]);
        if ((i + 1) % 16 == 0 && i != len - 1) {
            ss << "\n";
        } else if (i != len - 1) {
            ss << " ";
        }
    }
    return ss.str();
}

void testbench::uint32_to_uint8(const uint32_t* src, uint8_t* dst, size_t num_words)
{
    // Little-endian conversion
    for (size_t i = 0; i < num_words; i++) {
        dst[i * 4 + 0] = (src[i] >> 0) & 0xFF;
        dst[i * 4 + 1] = (src[i] >> 8) & 0xFF;
        dst[i * 4 + 2] = (src[i] >> 16) & 0xFF;
        dst[i * 4 + 3] = (src[i] >> 24) & 0xFF;
    }
}

void testbench::uint8_to_uint32(const uint8_t* src, uint32_t* dst, size_t num_words)
{
    // Little-endian conversion
    for (size_t i = 0; i < num_words; i++) {
        dst[i] = (static_cast<uint32_t>(src[i * 4 + 0]) << 0) |
                 (static_cast<uint32_t>(src[i * 4 + 1]) << 8) |
                 (static_cast<uint32_t>(src[i * 4 + 2]) << 16) |
                 (static_cast<uint32_t>(src[i * 4 + 3]) << 24);
    }
}


// =============================================================================
// OpenSSL Equivalence Test Cases
// =============================================================================

void testbench::test_openssl_aes128_ecb_equivalence()
{
    report_test_start("test_openssl_aes128_ecb_equivalence");

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
            report_test_fail("test_openssl_aes128_ecb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares (key = share0 XOR share1)
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
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
            report_test_pass("test_openssl_aes128_ecb_equivalence");
        } else {
            std::string msg = "OpenSSL vs Model mismatch\n";
            msg += "OpenSSL: " + data_to_hex(openssl_ciphertext, 16) + "\n";
            msg += "Model:   " + data_to_hex(model_ciphertext, 16);
            report_test_fail("test_openssl_aes128_ecb_equivalence", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ecb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ecb_equivalence()
{
    report_test_start("test_openssl_aes192_ecb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ecb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
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
            report_test_pass("test_openssl_aes192_ecb_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ecb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ecb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ecb_equivalence()
{
    report_test_start("test_openssl_aes256_ecb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ecb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_ECB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
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
            report_test_pass("test_openssl_aes256_ecb_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ecb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ecb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_cbc_equivalence()
{
    report_test_start("test_openssl_aes128_cbc_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_cbc_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes128_cbc_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_cbc_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_cbc_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_cfb_equivalence()
{
    report_test_start("test_openssl_aes128_cfb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_cfb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes128_cfb_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_cfb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_cfb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_ofb_equivalence()
{
    report_test_start("test_openssl_aes128_ofb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_ofb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_OFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes128_ofb_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_ofb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ofb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_ctr_equivalence()
{
    report_test_start("test_openssl_aes128_ctr_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_ctr_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CTR, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes128_ctr_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_ctr_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ctr_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_cbc_equivalence()
{
    report_test_start("test_openssl_aes192_cbc_equivalence");

    try {
        // Generate test vectors
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_cbc_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CBC, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes192_cbc_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_cbc_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_cbc_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_cbc_equivalence()
{
    report_test_start("test_openssl_aes256_cbc_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_cbc_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CBC, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes256_cbc_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_cbc_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_cbc_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_cfb_equivalence()
{
    report_test_start("test_openssl_aes192_cfb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_cfb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CFB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes192_cfb_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_cfb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_cfb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_cfb_equivalence()
{
    report_test_start("test_openssl_aes256_cfb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_cfb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CFB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes256_cfb_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_cfb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_cfb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ofb_equivalence()
{
    report_test_start("test_openssl_aes192_ofb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ofb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_OFB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes192_ofb_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ofb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ofb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ofb_equivalence()
{
    report_test_start("test_openssl_aes256_ofb_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ofb_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_OFB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes256_ofb_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ofb_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ofb_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ctr_equivalence()
{
    report_test_start("test_openssl_aes192_ctr_equivalence");

    try {
        // Generate test vectors
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ctr_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CTR, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes192_ctr_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ctr_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ctr_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ctr_equivalence()
{
    report_test_start("test_openssl_aes256_ctr_equivalence");

    try {
        // Generate test vectors
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ctr_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        // SystemC model encryption
        configure_aes(AES_ENC, AES_MODE_CTR, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
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
            report_test_pass("test_openssl_aes256_ctr_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ctr_equivalence",
                           "OpenSSL vs Model mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ctr_equivalence", e.what());
    }
}

// =============================================================================
// OpenSSL Decryption Equivalence Test Cases
// =============================================================================

void testbench::test_openssl_aes128_ecb_decryption_equivalence()
{
    report_test_start("test_openssl_aes128_ecb_decryption_equivalence");

    try {
        // Generate test vectors
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        // OpenSSL encryption to get ciphertext
        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_ecb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        // Prepare key shares
        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        // SystemC model decryption
        configure_aes(AES_DEC, AES_MODE_ECB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Wait for key expansion in decryption mode
        wait_for_idle(2000);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        // Compare decrypted result with original plaintext
        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes128_ecb_decryption_equivalence");
        } else {
            std::string msg = "Decryption mismatch\\n";
            msg += "Original:  " + data_to_hex(plaintext, 16) + "\\n";
            msg += "Decrypted: " + data_to_hex(model_plaintext, 16);
            report_test_fail("test_openssl_aes128_ecb_decryption_equivalence", msg);
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ecb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ecb_decryption_equivalence()
{
    report_test_start("test_openssl_aes192_ecb_decryption_equivalence");

    try {
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ecb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        configure_aes(AES_DEC, AES_MODE_ECB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        wait_for_idle(2000);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes192_ecb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ecb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ecb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ecb_decryption_equivalence()
{
    report_test_start("test_openssl_aes256_ecb_decryption_equivalence");

    try {
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, nullptr,
                             AES_MODE_ECB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ecb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        configure_aes(AES_DEC, AES_MODE_ECB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        wait_for_idle(2000);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes256_ecb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ecb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ecb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_cbc_decryption_equivalence()
{
    report_test_start("test_openssl_aes128_cbc_decryption_equivalence");

    try {
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_cbc_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CBC, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        wait_for_idle(2000);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes128_cbc_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_cbc_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_cbc_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_cbc_decryption_equivalence()
{
    report_test_start("test_openssl_aes192_cbc_decryption_equivalence");

    try {
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_cbc_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CBC, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        wait_for_idle(2000);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes192_cbc_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_cbc_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_cbc_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_cbc_decryption_equivalence()
{
    report_test_start("test_openssl_aes256_cbc_decryption_equivalence");

    try {
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CBC, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_cbc_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CBC, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        wait_for_idle(2000);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes256_cbc_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_cbc_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_cbc_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_cfb_decryption_equivalence()
{
    report_test_start("test_openssl_aes128_cfb_decryption_equivalence");

    try {
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_cfb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes128_cfb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_cfb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_cfb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_cfb_decryption_equivalence()
{
    report_test_start("test_openssl_aes192_cfb_decryption_equivalence");

    try {
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_cfb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CFB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes192_cfb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_cfb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_cfb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_cfb_decryption_equivalence()
{
    report_test_start("test_openssl_aes256_cfb_decryption_equivalence");

    try {
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_cfb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CFB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes256_cfb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_cfb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_cfb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_ofb_decryption_equivalence()
{
    report_test_start("test_openssl_aes128_ofb_decryption_equivalence");

    try {
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_ofb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_OFB, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes128_ofb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_ofb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ofb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ofb_decryption_equivalence()
{
    report_test_start("test_openssl_aes192_ofb_decryption_equivalence");

    try {
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ofb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_OFB, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes192_ofb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ofb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ofb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ofb_decryption_equivalence()
{
    report_test_start("test_openssl_aes256_ofb_decryption_equivalence");

    try {
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_OFB, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ofb_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_OFB, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes256_ofb_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ofb_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ofb_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes128_ctr_decryption_equivalence()
{
    report_test_start("test_openssl_aes128_ctr_decryption_equivalence");

    try {
        uint8_t key[16];
        generate_random_data(key, 16);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 16, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes128_ctr_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[4];
        uint8_to_uint32(key, actual_key, 4);
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CTR, AES_128, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes128_ctr_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes128_ctr_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes128_ctr_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes192_ctr_decryption_equivalence()
{
    report_test_start("test_openssl_aes192_ctr_decryption_equivalence");

    try {
        uint8_t key[24];
        generate_random_data(key, 24);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 24, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes192_ctr_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[6];
        uint8_to_uint32(key, actual_key, 6);
        generate_two_share_key(key_share0, key_share1, actual_key, 6);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CTR, AES_192, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 6);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes192_ctr_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes192_ctr_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes192_ctr_decryption_equivalence", e.what());
    }
}

void testbench::test_openssl_aes256_ctr_decryption_equivalence()
{
    report_test_start("test_openssl_aes256_ctr_decryption_equivalence");

    try {
        uint8_t key[32];
        generate_random_data(key, 32);

        uint8_t iv_bytes[16];
        generate_random_data(iv_bytes, 16);

        uint8_t plaintext[16];
        generate_random_data(plaintext, 16);

        uint8_t openssl_ciphertext[16];
        size_t openssl_len = 0;
        if (!openssl_encrypt(plaintext, 16, key, 32, iv_bytes,
                             AES_MODE_CTR, openssl_ciphertext, openssl_len)) {
            report_test_fail("test_openssl_aes256_ctr_decryption_equivalence",
                           "OpenSSL encryption failed");
            return;
        }

        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        uint8_to_uint32(key, actual_key, 8);
        generate_two_share_key(key_share0, key_share1, actual_key, 8);

        uint32_t iv[4];
        uint8_to_uint32(iv_bytes, iv, 4);

        configure_aes(AES_DEC, AES_MODE_CTR, AES_256, false);
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 8);
        wait(10, SC_NS);

        write_iv(iv);
        wait(10, SC_NS);

        uint32_t ciphertext_u32[4];
        uint8_to_uint32(openssl_ciphertext, ciphertext_u32, 4);
        write_data_in(ciphertext_u32);
        wait(10, SC_NS);

        wait_for_output_valid(1000);

        uint32_t model_plaintext_u32[4];
        read_data_out(model_plaintext_u32);
        wait(10, SC_NS);

        uint8_t model_plaintext[16];
        uint32_to_uint8(model_plaintext_u32, model_plaintext, 4);

        if (compare_data(plaintext, model_plaintext, 16)) {
            report_test_pass("test_openssl_aes256_ctr_decryption_equivalence");
        } else {
            report_test_fail("test_openssl_aes256_ctr_decryption_equivalence",
                           "Decryption mismatch");
        }

    } catch (const std::exception& e) {
        report_test_fail("test_openssl_aes256_ctr_decryption_equivalence", e.what());
    }
}

