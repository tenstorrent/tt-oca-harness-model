// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.

#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>
#include <sstream>

// =============================================================================
// Galois/Counter Mode
//
// GCM is driven through CTRL_GCM_SHADOWED.PHASE rather than as a single cipher
// call: software walks GCM_INIT -> GCM_AAD -> GCM_TEXT -> GCM_TAG, feeding one
// block per phase step, and the hardware keeps the GHASH accumulator between
// them. GCM_SAVE and GCM_RESTORE let a message be suspended and resumed.
//
// The end-to-end tests use test case 4 from the GCM specification, which
// exercises both a partial AAD block and a partial payload block.
// =============================================================================

namespace {

// GCM specification test case 4 (AES-128).
const uint8_t GCM_KEY[16] = {
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
    0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08
};

// J0 for a 96-bit IV is IV || 0^31 || 1.
const uint8_t GCM_J0[16] = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
    0xde, 0xca, 0xf8, 0x88, 0x00, 0x00, 0x00, 0x01
};

const uint8_t GCM_AAD[20] = {
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
    0xab, 0xad, 0xda, 0xd2
};

const uint8_t GCM_PLAINTEXT[60] = {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
    0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
    0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
    0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
    0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
    0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
    0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
    0xba, 0x63, 0x7b, 0x39
};

const uint8_t GCM_CIPHERTEXT[60] = {
    0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24,
    0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c,
    0xe3, 0xaa, 0x21, 0x2f, 0x2c, 0x02, 0xa4, 0xe0,
    0x35, 0xc1, 0x7e, 0x23, 0x29, 0xac, 0xa1, 0x2e,
    0x21, 0xd5, 0x14, 0xb2, 0x54, 0x66, 0x93, 0x1c,
    0x7d, 0x8f, 0x6a, 0x5a, 0xac, 0x84, 0xaa, 0x05,
    0x1b, 0xa3, 0x0b, 0x39, 0x6a, 0x0a, 0xac, 0x97,
    0x3d, 0x58, 0xe0, 0x91
};

const uint8_t GCM_TAG[16] = {
    0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb,
    0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47
};

// [len(AAD)]64 || [len(C)]64, in bits: 20 bytes -> 160, 60 bytes -> 480.
const uint8_t GCM_LENGTH_BLOCK[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xe0
};

enum : uint32_t {
    PHASE_INIT    = 0x01,
    PHASE_RESTORE = 0x02,
    PHASE_AAD     = 0x04,
    PHASE_TEXT    = 0x08,
    PHASE_SAVE    = 0x10,
    PHASE_TAG     = 0x20
};

std::string hex(const uint8_t* data, size_t len)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < len; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0xF];
    }
    return out;
}

} // namespace

/**
 * @brief Drives one CTRL_GCM_SHADOWED write and returns the readback
 */
uint32_t testbench::write_gcm_phase(uint32_t phase, uint32_t num_valid_bytes)
{
    const uint32_t value = (phase & 0x3F) | ((num_valid_bytes & 0x1F) << 6);
    m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, value);
    wait(1, SC_NS);
    m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, value);
    wait(1, SC_NS);

    uint32_t readback = 0;
    m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, readback);
    wait(1, SC_NS);
    return readback;
}

/**
 * @brief Feeds one 16-byte block through the current GCM phase
 */
void testbench::gcm_feed_block(const uint8_t* block, uint8_t* out, bool expect_output)
{
    uint32_t words[4];
    for (int i = 0; i < 4; ++i) {
        words[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 0)  |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 8)  |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    write_data_in(words);
    wait(10, SC_NS);

    if (!expect_output) {
        wait_for_idle(2000);
        return;
    }

    wait_for_output_valid(2000);

    uint32_t result[4];
    read_data_out(result);
    for (int i = 0; i < 4; ++i) {
        out[i * 4 + 0] = (result[i] >> 0)  & 0xFF;
        out[i * 4 + 1] = (result[i] >> 8)  & 0xFF;
        out[i * 4 + 2] = (result[i] >> 16) & 0xFF;
        out[i * 4 + 3] = (result[i] >> 24) & 0xFF;
    }
}

// =============================================================================
// Test Case: CTRL_GCM_SHADOWED reset value and field sanitisation
// =============================================================================
void testbench::test_gcm_ctrl_reset_and_sanitisation()
{
    report_test_start("test_gcm_ctrl_reset_and_sanitisation");

    try {
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        // Reset is GCM_INIT with a full 16-byte block.
        uint32_t readback = 0;
        m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, readback);
        wait(10, SC_NS);
        if (readback != 0x401) {
            std::stringstream ss;
            ss << "reset value wrong: expected 0x401, got 0x" << std::hex << readback;
            report_test_fail("test_gcm_ctrl_reset_and_sanitisation", ss.str());
            return;
        }

        // NUM_VALID_BYTES of 0 and of 17 both map to 16. PHASE stays at GCM_INIT
        // because the transition filter needs init_done to leave it.
        readback = write_gcm_phase(PHASE_INIT, 0);
        if (readback != 0x401) {
            std::stringstream ss;
            ss << "NUM_VALID_BYTES=0 not mapped to 16: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_ctrl_reset_and_sanitisation", ss.str());
            return;
        }

        readback = write_gcm_phase(PHASE_INIT, 17);
        if (readback != 0x401) {
            std::stringstream ss;
            ss << "NUM_VALID_BYTES=17 not mapped to 16: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_ctrl_reset_and_sanitisation", ss.str());
            return;
        }

        // A multi-bit PHASE is not a legal one-hot value and maps to GCM_INIT.
        readback = write_gcm_phase(0x0C, 16);
        if ((readback & 0x3F) != PHASE_INIT) {
            std::stringstream ss;
            ss << "illegal PHASE not mapped to GCM_INIT: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_ctrl_reset_and_sanitisation", ss.str());
            return;
        }

        report_test_pass("test_gcm_ctrl_reset_and_sanitisation");

    } catch (const std::exception& e) {
        report_test_fail("test_gcm_ctrl_reset_and_sanitisation", e.what());
    }
}

// =============================================================================
// Test Case: only the permitted phase transitions are accepted
//
// A rejected request is not an error; the phase simply stays where it was.
// =============================================================================
void testbench::test_gcm_phase_transition_gating()
{
    report_test_start("test_gcm_phase_transition_gating");

    try {
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false);
        wait(10, SC_NS);

        // Before the key and IV are loaded the initialisation cannot complete,
        // so GCM_INIT holds regardless of what is requested.
        uint32_t readback = write_gcm_phase(PHASE_AAD, 16);
        if ((readback & 0x3F) != PHASE_INIT) {
            std::stringstream ss;
            ss << "left GCM_INIT before init completed: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_phase_transition_gating", ss.str());
            return;
        }

        uint32_t key_share0[8] = {0}, key_share1[8] = {0};
        for (int i = 0; i < 4; ++i) {
            key_share0[i] = (static_cast<uint32_t>(GCM_KEY[i * 4 + 0]) << 0)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 1]) << 8)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 2]) << 16) |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 3]) << 24);
        }
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t j0[4];
        for (int i = 0; i < 4; ++i) {
            j0[i] = (static_cast<uint32_t>(GCM_J0[i * 4 + 0]) << 0)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 1]) << 8)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 2]) << 16) |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 3]) << 24);
        }
        write_iv(j0);
        wait(10, SC_NS);

        // Run the initialisation, then the same request is accepted.
        write_gcm_phase(PHASE_INIT, 16);
        readback = write_gcm_phase(PHASE_AAD, 16);
        if ((readback & 0x3F) != PHASE_AAD) {
            std::stringstream ss;
            ss << "GCM_INIT -> GCM_AAD rejected after init: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_phase_transition_gating", ss.str());
            return;
        }

        // GCM_SAVE from GCM_AAD is only allowed once a block has been absorbed.
        readback = write_gcm_phase(PHASE_SAVE, 16);
        if ((readback & 0x3F) != PHASE_AAD) {
            std::stringstream ss;
            ss << "GCM_SAVE allowed on the first block: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_phase_transition_gating", ss.str());
            return;
        }

        // GCM_RESTORE is not reachable from GCM_AAD at all.
        readback = write_gcm_phase(PHASE_RESTORE, 16);
        if ((readback & 0x3F) != PHASE_AAD) {
            std::stringstream ss;
            ss << "GCM_AAD -> GCM_RESTORE should be rejected: got 0x" << std::hex << readback;
            report_test_fail("test_gcm_phase_transition_gating", ss.str());
            return;
        }

        report_test_pass("test_gcm_phase_transition_gating");

    } catch (const std::exception& e) {
        report_test_fail("test_gcm_phase_transition_gating", e.what());
    }
}

// =============================================================================
// Test Case: authenticated encryption against GCM test case 4
// =============================================================================
void testbench::test_gcm_encrypt_nist_case4()
{
    report_test_start("test_gcm_encrypt_nist_case4");

    try {
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false);
        wait(10, SC_NS);

        uint32_t key_share0[8] = {0}, key_share1[8] = {0};
        for (int i = 0; i < 4; ++i) {
            key_share0[i] = (static_cast<uint32_t>(GCM_KEY[i * 4 + 0]) << 0)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 1]) << 8)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 2]) << 16) |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 3]) << 24);
        }
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t j0[4];
        for (int i = 0; i < 4; ++i) {
            j0[i] = (static_cast<uint32_t>(GCM_J0[i * 4 + 0]) << 0)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 1]) << 8)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 2]) << 16) |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 3]) << 24);
        }
        write_iv(j0);
        wait(10, SC_NS);

        write_gcm_phase(PHASE_INIT, 16);

        // The 20-byte AAD is one full block plus a 4-byte remainder.
        write_gcm_phase(PHASE_AAD, 16);
        uint8_t scratch[16];
        gcm_feed_block(GCM_AAD, scratch, false);

        uint8_t partial[16] = {0};
        memcpy(partial, GCM_AAD + 16, 4);
        write_gcm_phase(PHASE_AAD, 4);
        gcm_feed_block(partial, scratch, false);

        // The counter for the payload starts at inc32(J0), which the model
        // reaches by advancing the IV registers; software reloads it here.
        uint32_t ctr[4];
        for (int i = 0; i < 4; ++i) {
            ctr[i] = j0[i];
        }
        ctr[3] = (ctr[3] & 0x00FFFFFF) | (0x02u << 24);  // big-endian counter = 2
        write_iv(ctr);
        wait(10, SC_NS);

        // 60 bytes of payload: three full blocks and a 12-byte remainder.
        uint8_t ciphertext[64] = {0};
        write_gcm_phase(PHASE_TEXT, 16);
        for (int b = 0; b < 3; ++b) {
            gcm_feed_block(GCM_PLAINTEXT + b * 16, ciphertext + b * 16, true);
        }

        memset(partial, 0, sizeof(partial));
        memcpy(partial, GCM_PLAINTEXT + 48, 12);
        write_gcm_phase(PHASE_TEXT, 12);
        gcm_feed_block(partial, ciphertext + 48, true);

        if (memcmp(ciphertext, GCM_CIPHERTEXT, 60) != 0) {
            report_test_fail("test_gcm_encrypt_nist_case4",
                             "ciphertext mismatch: got " + hex(ciphertext, 60) +
                             ", expected " + hex(GCM_CIPHERTEXT, 60));
            return;
        }

        // The length block is supplied by software, matching aes_ghash.sv.
        uint8_t tag[16];
        write_gcm_phase(PHASE_TAG, 16);
        gcm_feed_block(GCM_LENGTH_BLOCK, tag, true);

        if (memcmp(tag, GCM_TAG, 16) != 0) {
            report_test_fail("test_gcm_encrypt_nist_case4",
                             "tag mismatch: got " + hex(tag, 16) +
                             ", expected " + hex(GCM_TAG, 16));
            return;
        }

        report_test_pass("test_gcm_encrypt_nist_case4");

    } catch (const std::exception& e) {
        report_test_fail("test_gcm_encrypt_nist_case4", e.what());
    }
}

// =============================================================================
// Test Case: authenticated decryption against GCM test case 4
//
// Decryption authenticates the ciphertext, which the hardware takes from
// DATA_IN rather than DATA_OUT, so the tag must come out identical.
// =============================================================================
void testbench::test_gcm_decrypt_nist_case4()
{
    report_test_start("test_gcm_decrypt_nist_case4");

    try {
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        configure_aes(AES_DEC, AES_MODE_GCM, AES_128, false);
        wait(10, SC_NS);

        uint32_t key_share0[8] = {0}, key_share1[8] = {0};
        for (int i = 0; i < 4; ++i) {
            key_share0[i] = (static_cast<uint32_t>(GCM_KEY[i * 4 + 0]) << 0)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 1]) << 8)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 2]) << 16) |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 3]) << 24);
        }
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t j0[4];
        for (int i = 0; i < 4; ++i) {
            j0[i] = (static_cast<uint32_t>(GCM_J0[i * 4 + 0]) << 0)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 1]) << 8)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 2]) << 16) |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 3]) << 24);
        }
        write_iv(j0);
        wait(10, SC_NS);

        write_gcm_phase(PHASE_INIT, 16);

        uint8_t scratch[16];
        write_gcm_phase(PHASE_AAD, 16);
        gcm_feed_block(GCM_AAD, scratch, false);

        uint8_t partial[16] = {0};
        memcpy(partial, GCM_AAD + 16, 4);
        write_gcm_phase(PHASE_AAD, 4);
        gcm_feed_block(partial, scratch, false);

        uint32_t ctr[4];
        for (int i = 0; i < 4; ++i) {
            ctr[i] = j0[i];
        }
        ctr[3] = (ctr[3] & 0x00FFFFFF) | (0x02u << 24);
        write_iv(ctr);
        wait(10, SC_NS);

        uint8_t plaintext[64] = {0};
        write_gcm_phase(PHASE_TEXT, 16);
        for (int b = 0; b < 3; ++b) {
            gcm_feed_block(GCM_CIPHERTEXT + b * 16, plaintext + b * 16, true);
        }

        memset(partial, 0, sizeof(partial));
        memcpy(partial, GCM_CIPHERTEXT + 48, 12);
        write_gcm_phase(PHASE_TEXT, 12);
        gcm_feed_block(partial, plaintext + 48, true);

        if (memcmp(plaintext, GCM_PLAINTEXT, 60) != 0) {
            report_test_fail("test_gcm_decrypt_nist_case4",
                             "plaintext mismatch: got " + hex(plaintext, 60) +
                             ", expected " + hex(GCM_PLAINTEXT, 60));
            return;
        }

        uint8_t tag[16];
        write_gcm_phase(PHASE_TAG, 16);
        gcm_feed_block(GCM_LENGTH_BLOCK, tag, true);

        if (memcmp(tag, GCM_TAG, 16) != 0) {
            report_test_fail("test_gcm_decrypt_nist_case4",
                             "tag mismatch: got " + hex(tag, 16) +
                             ", expected " + hex(GCM_TAG, 16));
            return;
        }

        report_test_pass("test_gcm_decrypt_nist_case4");

    } catch (const std::exception& e) {
        report_test_fail("test_gcm_decrypt_nist_case4", e.what());
    }
}

// =============================================================================
// Test Case: GCM_SAVE and GCM_RESTORE suspend and resume a message
//
// The saved value is the GHASH accumulator with S added, so it never leaves the
// block in the clear; GCM_RESTORE removes S again. Interrupting the message
// after the AAD and resuming must produce the same tag as running it straight
// through.
// =============================================================================
void testbench::test_gcm_save_restore()
{
    report_test_start("test_gcm_save_restore");

    try {
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false);
        wait(10, SC_NS);

        uint32_t key_share0[8] = {0}, key_share1[8] = {0};
        for (int i = 0; i < 4; ++i) {
            key_share0[i] = (static_cast<uint32_t>(GCM_KEY[i * 4 + 0]) << 0)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 1]) << 8)  |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 2]) << 16) |
                            (static_cast<uint32_t>(GCM_KEY[i * 4 + 3]) << 24);
        }
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t j0[4];
        for (int i = 0; i < 4; ++i) {
            j0[i] = (static_cast<uint32_t>(GCM_J0[i * 4 + 0]) << 0)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 1]) << 8)  |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 2]) << 16) |
                    (static_cast<uint32_t>(GCM_J0[i * 4 + 3]) << 24);
        }
        write_iv(j0);
        wait(10, SC_NS);

        write_gcm_phase(PHASE_INIT, 16);

        uint8_t scratch[16];
        write_gcm_phase(PHASE_AAD, 16);
        gcm_feed_block(GCM_AAD, scratch, false);

        uint8_t partial[16] = {0};
        memcpy(partial, GCM_AAD + 16, 4);
        write_gcm_phase(PHASE_AAD, 4);
        gcm_feed_block(partial, scratch, false);

        // Suspend: GCM_SAVE emits its block without consuming any input.
        write_gcm_phase(PHASE_SAVE, 16);
        wait_for_output_valid(2000);
        uint32_t saved_words[4];
        read_data_out(saved_words);
        wait(10, SC_NS);

        // Resume from GCM_INIT, which is the only exit from GCM_SAVE.
        write_gcm_phase(PHASE_INIT, 16);
        write_gcm_phase(PHASE_RESTORE, 16);

        uint8_t saved[16];
        for (int i = 0; i < 4; ++i) {
            saved[i * 4 + 0] = (saved_words[i] >> 0)  & 0xFF;
            saved[i * 4 + 1] = (saved_words[i] >> 8)  & 0xFF;
            saved[i * 4 + 2] = (saved_words[i] >> 16) & 0xFF;
            saved[i * 4 + 3] = (saved_words[i] >> 24) & 0xFF;
        }
        gcm_feed_block(saved, scratch, false);

        uint32_t ctr[4];
        for (int i = 0; i < 4; ++i) {
            ctr[i] = j0[i];
        }
        ctr[3] = (ctr[3] & 0x00FFFFFF) | (0x02u << 24);
        write_iv(ctr);
        wait(10, SC_NS);

        uint8_t ciphertext[64] = {0};
        write_gcm_phase(PHASE_TEXT, 16);
        for (int b = 0; b < 3; ++b) {
            gcm_feed_block(GCM_PLAINTEXT + b * 16, ciphertext + b * 16, true);
        }

        memset(partial, 0, sizeof(partial));
        memcpy(partial, GCM_PLAINTEXT + 48, 12);
        write_gcm_phase(PHASE_TEXT, 12);
        gcm_feed_block(partial, ciphertext + 48, true);

        if (memcmp(ciphertext, GCM_CIPHERTEXT, 60) != 0) {
            report_test_fail("test_gcm_save_restore",
                             "ciphertext mismatch after restore: got " + hex(ciphertext, 60));
            return;
        }

        uint8_t tag[16];
        write_gcm_phase(PHASE_TAG, 16);
        gcm_feed_block(GCM_LENGTH_BLOCK, tag, true);

        if (memcmp(tag, GCM_TAG, 16) != 0) {
            report_test_fail("test_gcm_save_restore",
                             "tag mismatch after restore: got " + hex(tag, 16) +
                             ", expected " + hex(GCM_TAG, 16));
            return;
        }

        report_test_pass("test_gcm_save_restore");

    } catch (const std::exception& e) {
        report_test_fail("test_gcm_save_restore", e.what());
    }
}
