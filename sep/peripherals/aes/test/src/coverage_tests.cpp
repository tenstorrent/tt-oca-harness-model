// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file coverage_tests.cpp
 * @brief Protocol / KeyMgr / TLM matrix cases with independent oracles.
 */

#include "testbench.h"
#include "aes_basetest.h"

#include <tlm.h>
#include <cstring>
#include <cmath>
#include <vector>

namespace {

void nist_key_shares(testbench &tb, uint32_t *share0, uint32_t *share1)
{
    uint32_t actual[4] = {0x2b7e1516u, 0x28aed2a6u, 0xabf71588u, 0x09cf4f3cu};
    tb.generate_two_share_key(share0, share1, actual, 4);
}

tlm::tlm_response_status keymgr_xact(aes_test &t, tlm::tlm_command cmd,
                                     uint64_t addr, unsigned char *data,
                                     unsigned int len, unsigned int streaming = 0)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(streaming == 0 ? (len == 0 ? 1u : len) : streaming);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    t.keymgr_socket->b_transport(trans, delay);
    return trans.get_response_status();
}

tlm::tlm_response_status keymgr_word(aes_test &t, tlm::tlm_command cmd,
                                     uint64_t addr, uint32_t &word)
{
    return keymgr_xact(t, cmd, addr, reinterpret_cast<unsigned char *>(&word), 4);
}

} // namespace

void testbench::test_coverage_escalation_aborts_in_flight_cipher()
{
    report_test_start("test_coverage_escalation_aborts_in_flight_cipher");

    try {
        m_test->trigger_reset();
        lc_escalate_signal.write(false);
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};

        m_test->lc_escalate_en_o.write(true);
        wait(10, SC_NS);
        m_test->trigger_reset();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, false);
        write_key_shares(share0, share1, 4);
        write_data_in(block);
        wait(150, SC_NS);

        const uint32_t status = read_status();
        const bool fatal = (status & (1u << STATUS_ALERT_FATAL_BIT)) != 0u;
        const bool output_valid = (status & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u;

        lc_escalate_signal.write(false);
        wait(20, SC_NS);
        m_test->trigger_reset();
        wait(20, SC_NS);

        if (fatal && !output_valid) {
            report_test_pass("test_coverage_escalation_aborts_in_flight_cipher");
        } else {
            report_test_fail("test_coverage_escalation_aborts_in_flight_cipher",
                             "in-flight escalation must fatal-alert and drop the cipher");
        }
    } catch (const std::exception &e) {
        lc_escalate_signal.write(false);
        report_test_fail("test_coverage_escalation_aborts_in_flight_cipher", e.what());
    }
}

void testbench::test_coverage_sideload_manual_openssl()
{
    report_test_start("test_coverage_sideload_manual_openssl");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        const uint32_t block[4] = {0xaaaaaaaaU, 0xbbbbbbbbU, 0xccccccccU, 0xddddddddU};
        const uint8_t plaintext[16] = {
            0xaa, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xbb, 0xbb,
            0xcc, 0xcc, 0xcc, 0xcc, 0xdd, 0xdd, 0xdd, 0xdd};
        const uint8_t key_bytes[16] = {
            0x16, 0x15, 0x7e, 0x2b, 0xa6, 0xd2, 0xae, 0x28,
            0x88, 0x15, 0xf7, 0xab, 0x3c, 0x4f, 0xcf, 0x09};

        // KEY_CTRL=0 invalidates; shares are not shredded by this model.
        aes_if::keymgr_sideload_key_t cleared;
        cleared.valid = false;
        m_test->set_keymgr_key(cleared);
        uint32_t key_ctrl = 0;
        if (keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, key_ctrl) !=
            tlm::TLM_OK_RESPONSE) {
            report_test_fail("test_coverage_sideload_manual_openssl",
                             "KEY_CTRL=0 write failed");
            return;
        }

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, true);
        wait_for_idle(2000);
        write_data_in(block);
        wait(200, SC_NS);
        uint32_t status = read_status();
        if ((status & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_sideload_manual_openssl",
                             "auto-start ran without a valid sideload key");
            return;
        }

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        aes_if::keymgr_sideload_key_t km;
        for (int i = 0; i < 8; ++i) {
            km.key_share0[i] = share0[i];
            km.key_share1[i] = share1[i];
        }
        km.valid = true;
        m_test->set_keymgr_key(km);

        // Prove exact share bytes were accepted: encrypt with the known key.
        uint8_t expected[32] = {};
        size_t expected_len = 0;
        if (!openssl_encrypt(plaintext, 16, key_bytes, 16, nullptr,
                             AES_MODE_ECB, expected, expected_len) ||
            expected_len < 16) {
            report_test_fail("test_coverage_sideload_manual_openssl",
                             "OpenSSL reference encrypt failed");
            return;
        }

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_data_in(block);
        trigger_manual_start();
        wait_for_output_valid(2000);
        uint32_t out[4] = {};
        read_data_out(out);

        uint8_t got[16];
        for (int i = 0; i < 4; ++i) {
            got[i * 4 + 0] = (out[i] >> 0) & 0xFF;
            got[i * 4 + 1] = (out[i] >> 8) & 0xFF;
            got[i * 4 + 2] = (out[i] >> 16) & 0xFF;
            got[i * 4 + 3] = (out[i] >> 24) & 0xFF;
        }
        if (std::memcmp(got, expected, 16) != 0) {
            report_test_fail("test_coverage_sideload_manual_openssl",
                             "sideload ciphertext mismatch vs OpenSSL (wrong key shares)");
            return;
        }

        report_test_pass("test_coverage_sideload_manual_openssl");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_sideload_manual_openssl", e.what());
    }
}

void testbench::test_coverage_busy_gcm_write_ignored()
{
    report_test_start("test_coverage_busy_gcm_write_ignored");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x01020304u, 0x05060708u, 0x090a0b0cu, 0x0d0e0f10u};

        // Commit a known GCM value while idle.
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        uint32_t gcm_before = 0;
        m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, gcm_before);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_256, false, false);
        write_key_shares(share0, share1, 4);
        for (int i = 0; i < 3; ++i) {
            m_test->register_write_32(aes_basetest::DATA_IN_OFFSET +
                                          static_cast<unsigned>(i * 4),
                                      block[i]);
            wait(1, SC_NS);
        }
        // Completing DATA_IN starts the cipher; GCM write must be ignored while busy.
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 12u, block[3]);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x081u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x081u);
        wait(1, SC_NS);

        uint32_t gcm_during = 0;
        m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, gcm_during);
        if (gcm_during != gcm_before) {
            report_test_fail("test_coverage_busy_gcm_write_ignored",
                             "CTRL_GCM_SHADOWED changed during busy ECB");
            return;
        }

        wait_for_output_valid(2000);
        uint32_t discard[4];
        read_data_out(discard);
        wait_for_idle(2000);

        uint32_t gcm_after = 0;
        m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, gcm_after);
        if (gcm_after != gcm_before) {
            report_test_fail("test_coverage_busy_gcm_write_ignored",
                             "CTRL_GCM_SHADOWED changed after busy window");
            return;
        }

        report_test_pass("test_coverage_busy_gcm_write_ignored");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_busy_gcm_write_ignored", e.what());
    }
}

void testbench::test_coverage_gcm_shadow_mismatch_and_init_gates()
{
    report_test_start("test_coverage_gcm_shadow_mismatch_and_init_gates");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x041u);
        wait(5, SC_NS);
        uint32_t status = read_status();
        if ((status & (1u << STATUS_ALERT_RECOV_BIT)) == 0u) {
            report_test_fail("test_coverage_gcm_shadow_mismatch_and_init_gates",
                             "GCM shadow mismatch did not raise recoverable alert");
            return;
        }

        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(5, SC_NS);
        status = read_status();
        if ((status & (1u << STATUS_ALERT_RECOV_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_shadow_mismatch_and_init_gates",
                             "matching GCM shadow write did not clear recoverable alert");
            return;
        }

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x0u, 0x0u, 0x0u, 0x0u};

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false, false);
        write_key_shares(share0, share1, 4);
        write_gcm_phase(0x01u, 16u);
        write_data_in(block);
        wait(50, SC_NS);
        status = read_status();
        if ((status & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_shadow_mismatch_and_init_gates",
                             "GCM_INIT auto-started from DATA_IN");
            return;
        }

        m_test->invalidate_keymgr_key();
        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false, true);
        write_gcm_phase(0x01u, 16u);
        if ((read_status() & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_shadow_mismatch_and_init_gates",
                             "missing sideload key during GCM_INIT must not fatal");
            return;
        }

        report_test_pass("test_coverage_gcm_shadow_mismatch_and_init_gates");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_gcm_shadow_mismatch_and_init_gates", e.what());
    }
}

void testbench::test_coverage_register_tlm_matrix()
{
    report_test_start("test_coverage_register_tlm_matrix");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        // Seed STATUS-adjacent readable state.
        uint32_t status0 = read_status();
        uint32_t ctrl0 = 0;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl0);

        uint32_t scratch = 0xA5A5A5A5u;
        auto expect = [&](tlm::tlm_response_status got,
                          tlm::tlm_response_status want,
                          const char *label) -> bool {
            if (got != want) {
                report_test_fail("test_coverage_register_tlm_matrix", label);
                return false;
            }
            return true;
        };

        // IGNORE → COMMAND_ERROR (reg_file::validate).
        if (!expect(m_test->register_b_transport(tlm::TLM_IGNORE_COMMAND,
                                                 aes_basetest::STATUS_OFFSET,
                                                 reinterpret_cast<unsigned char *>(&scratch),
                                                 4, 4),
                    tlm::TLM_COMMAND_ERROR_RESPONSE,
                    "IGNORE must return TLM_COMMAND_ERROR_RESPONSE")) {
            return;
        }

        // Null data pointer.
        if (!expect(m_test->register_b_transport(tlm::TLM_READ_COMMAND,
                                                 aes_basetest::STATUS_OFFSET,
                                                 nullptr, 4, 4),
                    tlm::TLM_GENERIC_ERROR_RESPONSE,
                    "null data_ptr must return TLM_GENERIC_ERROR_RESPONSE")) {
            return;
        }

        // Zero length.
        if (!expect(m_test->register_b_transport(tlm::TLM_READ_COMMAND,
                                                 aes_basetest::STATUS_OFFSET,
                                                 reinterpret_cast<unsigned char *>(&scratch),
                                                 0, 1),
                    tlm::TLM_BURST_ERROR_RESPONSE,
                    "zero length must return TLM_BURST_ERROR_RESPONSE")) {
            return;
        }

        // Streaming width < len.
        if (!expect(m_test->register_b_transport(tlm::TLM_READ_COMMAND,
                                                 aes_basetest::STATUS_OFFSET,
                                                 reinterpret_cast<unsigned char *>(&scratch),
                                                 4, 2),
                    tlm::TLM_BURST_ERROR_RESPONSE,
                    "streaming_width < len must return TLM_BURST_ERROR_RESPONSE")) {
            return;
        }

        // OOB / hole beyond the map (memory size 0x8C → last legal 0x88).
        uint32_t hole = 0xFFFFFFFFu;
        if (!expect(m_test->register_b_transport(tlm::TLM_WRITE_COMMAND, 0x90u,
                                                 reinterpret_cast<unsigned char *>(&hole),
                                                 4, 4),
                    tlm::TLM_OK_RESPONSE,
                    "OOB write is reserved-location OK per reg_file")) {
            return;
        }
        uint32_t ctrl1 = 0;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl1);
        if (ctrl1 != ctrl0 || read_status() != status0) {
            report_test_fail("test_coverage_register_tlm_matrix",
                             "OOB write mutated in-window state");
            return;
        }

        // Unaligned write into CTRL_SHADOWED must not commit a new control value
        // via a full-word side effect; reg_file splits by byte. Snapshot + refuse
        // mutation of the architectural CTRL after a rejected malformed xact is
        // already covered above; here assert a 1-byte write OK + STATUS unchanged
        // for a RO register (STATUS write is dropped by callback absence / WO mask).
        uint8_t one = 0x5Au;
        if (!expect(m_test->register_b_transport(tlm::TLM_WRITE_COMMAND,
                                                 aes_basetest::STATUS_OFFSET,
                                                 &one, 1, 1),
                    tlm::TLM_OK_RESPONSE,
                    "1-byte STATUS write is serviced as reserved/no-callback OK")) {
            return;
        }
        if (read_status() != status0) {
            report_test_fail("test_coverage_register_tlm_matrix",
                             "STATUS write mutated STATUS");
            return;
        }

        report_test_pass("test_coverage_register_tlm_matrix");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_register_tlm_matrix", e.what());
    }
}

void testbench::test_coverage_keymgr_tlm_matrix()
{
    report_test_start("test_coverage_keymgr_tlm_matrix");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t word = 0xA5A5A5A5u;
        if (keymgr_word(*m_test, tlm::TLM_READ_COMMAND, 0x00u, word) !=
            tlm::TLM_COMMAND_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr READ must return TLM_COMMAND_ERROR_RESPONSE");
            return;
        }

        if (keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x00u, nullptr, 4) !=
            tlm::TLM_GENERIC_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr null data_ptr must return GENERIC_ERROR");
            return;
        }

        uint32_t sink = 0;
        if (keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x00u,
                        reinterpret_cast<unsigned char *>(&sink), 0) !=
            tlm::TLM_BURST_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr len=0 must return BURST_ERROR");
            return;
        }

        if (keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x00u,
                        reinterpret_cast<unsigned char *>(&sink), 3) !=
            tlm::TLM_BURST_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr len!=4 must return BURST_ERROR");
            return;
        }

        if (keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x01u,
                        reinterpret_cast<unsigned char *>(&sink), 4) !=
            tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr unaligned address must return ADDRESS_ERROR");
            return;
        }

        if (keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x44u,
                        reinterpret_cast<unsigned char *>(&sink), 4) !=
            tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KeyMgr unknown address must return ADDRESS_ERROR");
            return;
        }

        // Valid push: exact share words + KEY_CTRL=1, then KEY_CTRL=0 clears valid
        // (shares retained — encrypt after re-commit without re-push still works).
        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        for (int i = 0; i < 8; ++i) {
            if (keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND,
                            static_cast<uint64_t>(i * 4), share0[i]) !=
                tlm::TLM_OK_RESPONSE) {
                report_test_fail("test_coverage_keymgr_tlm_matrix",
                                 "KEY_SHARE0 push failed");
                return;
            }
            if (keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND,
                            0x20u + static_cast<uint64_t>(i * 4), share1[i]) !=
                tlm::TLM_OK_RESPONSE) {
                report_test_fail("test_coverage_keymgr_tlm_matrix",
                                 "KEY_SHARE1 push failed");
                return;
            }
        }
        uint32_t ctrl = 1;
        if (keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, ctrl) !=
            tlm::TLM_OK_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix", "KEY_CTRL=1 failed");
            return;
        }

        const uint32_t block[4] = {0x00112233u, 0x44556677u, 0x8899aabbu, 0xccddeeffu};
        uint8_t plaintext[16];
        for (int i = 0; i < 4; ++i) {
            plaintext[i * 4 + 0] = (block[i] >> 0) & 0xFF;
            plaintext[i * 4 + 1] = (block[i] >> 8) & 0xFF;
            plaintext[i * 4 + 2] = (block[i] >> 16) & 0xFF;
            plaintext[i * 4 + 3] = (block[i] >> 24) & 0xFF;
        }
        const uint8_t key_bytes[16] = {
            0x16, 0x15, 0x7e, 0x2b, 0xa6, 0xd2, 0xae, 0x28,
            0x88, 0x15, 0xf7, 0xab, 0x3c, 0x4f, 0xcf, 0x09};
        uint8_t expected[32] = {};
        size_t expected_len = 0;
        if (!openssl_encrypt(plaintext, 16, key_bytes, 16, nullptr,
                             AES_MODE_ECB, expected, expected_len)) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "OpenSSL reference failed");
            return;
        }

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_data_in(block);
        trigger_manual_start();
        wait_for_output_valid(2000);
        uint32_t out[4] = {};
        read_data_out(out);
        uint8_t got[16];
        for (int i = 0; i < 4; ++i) {
            got[i * 4 + 0] = (out[i] >> 0) & 0xFF;
            got[i * 4 + 1] = (out[i] >> 8) & 0xFF;
            got[i * 4 + 2] = (out[i] >> 16) & 0xFF;
            got[i * 4 + 3] = (out[i] >> 24) & 0xFF;
        }
        if (std::memcmp(got, expected, 16) != 0) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "pushed share bytes do not match OpenSSL ciphertext");
            return;
        }

        ctrl = 0;
        if (keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, ctrl) !=
            tlm::TLM_OK_RESPONSE) {
            report_test_fail("test_coverage_keymgr_tlm_matrix", "KEY_CTRL=0 failed");
            return;
        }
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_data_in(block);
        trigger_manual_start();
        wait(100, SC_NS);
        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "KEY_CTRL=0 must invalidate sideload (no ciphertext)");
            return;
        }

        // Re-commit without re-pushing shares — proves shares were not shredded.
        ctrl = 1;
        keymgr_word(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, ctrl);
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_data_in(block);
        trigger_manual_start();
        wait_for_output_valid(2000);
        read_data_out(out);
        for (int i = 0; i < 4; ++i) {
            got[i * 4 + 0] = (out[i] >> 0) & 0xFF;
            got[i * 4 + 1] = (out[i] >> 8) & 0xFF;
            got[i * 4 + 2] = (out[i] >> 16) & 0xFF;
            got[i * 4 + 3] = (out[i] >> 24) & 0xFF;
        }
        if (std::memcmp(got, expected, 16) != 0) {
            report_test_fail("test_coverage_keymgr_tlm_matrix",
                             "re-commit without re-push lost key shares");
            return;
        }

        report_test_pass("test_coverage_keymgr_tlm_matrix");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_keymgr_tlm_matrix", e.what());
    }
}

void testbench::test_coverage_byte_enable_data_in()
{
    report_test_start("test_coverage_byte_enable_data_in");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, false);
        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        write_key_shares(share0, share1, 4);

        // Full-word seed, then a partial BE write that must preserve upper bytes.
        const uint32_t seed = 0x11223344u;
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET, seed);
        wait(1, SC_NS);

        const unsigned char be[4] = {0xFF, 0x00, 0x00, 0x00}; // lane 0 only
        const uint32_t partial = 0xAABBAABBu; // only low byte should apply → 0x112233BB
        if (m_test->register_write_32_with_be(aes_basetest::DATA_IN_OFFSET, partial, be) !=
            tlm::TLM_OK_RESPONSE) {
            report_test_fail("test_coverage_byte_enable_data_in",
                             "partial BE write failed");
            return;
        }
        wait(1, SC_NS);

        // Complete the remaining DATA_IN words and encrypt; compare to OpenSSL
        // with the merged first word 0x112233BB.
        const uint32_t w1 = 0x55667788u;
        const uint32_t w2 = 0x99AABBCCu;
        const uint32_t w3 = 0xDDEEF00Du;
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 4, w1);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 8, w2);
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 12, w3);
        wait(1, SC_NS);

        const uint32_t merged0 = 0x112233BBu;
        uint8_t plaintext[16] = {
            static_cast<uint8_t>((merged0 >> 0) & 0xFF),
            static_cast<uint8_t>((merged0 >> 8) & 0xFF),
            static_cast<uint8_t>((merged0 >> 16) & 0xFF),
            static_cast<uint8_t>((merged0 >> 24) & 0xFF),
            static_cast<uint8_t>((w1 >> 0) & 0xFF),
            static_cast<uint8_t>((w1 >> 8) & 0xFF),
            static_cast<uint8_t>((w1 >> 16) & 0xFF),
            static_cast<uint8_t>((w1 >> 24) & 0xFF),
            static_cast<uint8_t>((w2 >> 0) & 0xFF),
            static_cast<uint8_t>((w2 >> 8) & 0xFF),
            static_cast<uint8_t>((w2 >> 16) & 0xFF),
            static_cast<uint8_t>((w2 >> 24) & 0xFF),
            static_cast<uint8_t>((w3 >> 0) & 0xFF),
            static_cast<uint8_t>((w3 >> 8) & 0xFF),
            static_cast<uint8_t>((w3 >> 16) & 0xFF),
            static_cast<uint8_t>((w3 >> 24) & 0xFF),
        };
        const uint8_t key_bytes[16] = {
            0x16, 0x15, 0x7e, 0x2b, 0xa6, 0xd2, 0xae, 0x28,
            0x88, 0x15, 0xf7, 0xab, 0x3c, 0x4f, 0xcf, 0x09};
        uint8_t expected[32] = {};
        size_t expected_len = 0;
        if (!openssl_encrypt(plaintext, 16, key_bytes, 16, nullptr,
                             AES_MODE_ECB, expected, expected_len)) {
            report_test_fail("test_coverage_byte_enable_data_in",
                             "OpenSSL reference failed");
            return;
        }

        trigger_manual_start();
        wait_for_output_valid(2000);
        uint32_t out[4] = {};
        read_data_out(out);
        uint8_t got[16];
        for (int i = 0; i < 4; ++i) {
            got[i * 4 + 0] = (out[i] >> 0) & 0xFF;
            got[i * 4 + 1] = (out[i] >> 8) & 0xFF;
            got[i * 4 + 2] = (out[i] >> 16) & 0xFF;
            got[i * 4 + 3] = (out[i] >> 24) & 0xFF;
        }
        if (std::memcmp(got, expected, 16) != 0) {
            report_test_fail("test_coverage_byte_enable_data_in",
                             "partial BE cleared untouched DATA_IN bytes");
            return;
        }

        // TRIGGER: disabled lanes must not assert START.
        const unsigned char be_hi[4] = {0x00, 0x00, 0x00, 0xFF};
        const uint32_t trig = 0x00000001u; // START in lane 0 — disabled by be_hi
        m_test->register_write_32_with_be(aes_basetest::TRIGGER_OFFSET, trig, be_hi);
        wait(20, SC_NS);
        if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_byte_enable_data_in",
                             "disabled TRIGGER.START lane started a cipher");
            return;
        }

        report_test_pass("test_coverage_byte_enable_data_in");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_byte_enable_data_in", e.what());
    }
}

void testbench::test_coverage_transport_dbg_and_dmi()
{
    report_test_start("test_coverage_transport_dbg_and_dmi");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t status = 0;
        const unsigned n = m_test->register_transport_dbg(
            tlm::TLM_READ_COMMAND, aes_basetest::STATUS_OFFSET,
            reinterpret_cast<unsigned char *>(&status), 4);
        if (n != 4u) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "transport_dbg STATUS read byte count != 4");
            return;
        }
        if (status != read_status()) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "transport_dbg STATUS disagrees with b_transport");
            return;
        }

        // Debug write of TRIGGER runs the same side-effect path as b_transport
        // (inherited Memory::transport_dbg → write_registers).
        const uint32_t idle_before = read_status() & (1u << STATUS_IDLE_BIT);
        uint32_t trig = 0x8u; // PRNG_RESEED
        const unsigned wn = m_test->register_transport_dbg(
            tlm::TLM_WRITE_COMMAND, aes_basetest::TRIGGER_OFFSET,
            reinterpret_cast<unsigned char *>(&trig), 4);
        if (wn != 4u) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "transport_dbg TRIGGER write byte count != 4");
            return;
        }
        wait(150, SC_NS);
        if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "debug PRNG_RESEED did not complete");
            return;
        }
        if (idle_before == 0u) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "precondition idle was already clear");
            return;
        }

        // DMI is not registered on the Memory socket → must refuse.
        if (m_test->register_get_direct_mem_ptr(tlm::TLM_READ_COMMAND,
                                                aes_basetest::STATUS_OFFSET)) {
            report_test_fail("test_coverage_transport_dbg_and_dmi",
                             "get_direct_mem_ptr must return false");
            return;
        }

        report_test_pass("test_coverage_transport_dbg_and_dmi");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_transport_dbg_and_dmi", e.what());
    }
}

void testbench::test_coverage_clk_i_no_effect_on_latency()
{
    report_test_start("test_coverage_clk_i_no_effect_on_latency");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x00112233u, 0x44556677u, 0x8899aabbu, 0xccddeeffu};

        auto run_once = [&](bool toggle_clk) -> double {
            configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, false);
            write_key_shares(share0, share1, 4);
            write_data_in(block);
            const sc_time t0 = sc_time_stamp();
            trigger_manual_start();
            if (toggle_clk) {
                for (int i = 0; i < 20; ++i) {
                    m_test->clk_o.write(true);
                    wait(1, SC_NS);
                    m_test->clk_o.write(false);
                    wait(1, SC_NS);
                }
            }
            wait_for_output_valid(2000);
            const double ns = (sc_time_stamp() - t0).to_seconds() * 1e9;
            uint32_t out[4];
            read_data_out(out);
            wait_for_idle(2000);
            return ns;
        };

        const double lat_quiet = run_once(false);
        m_test->trigger_reset();
        wait(20, SC_NS);
        const double lat_toggle = run_once(true);

        // Cipher delay is hard-coded from m_clk_freq_hz (100 MHz), not clk_i.
        // Absolute latency is covered by FUNC-009; here only the clk_i delta matters.
        if (std::abs(lat_quiet - lat_toggle) > 50.0) {
            report_test_fail("test_coverage_clk_i_no_effect_on_latency",
                             "toggling clk_i changed cipher latency");
            return;
        }

        report_test_pass("test_coverage_clk_i_no_effect_on_latency");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_clk_i_no_effect_on_latency", e.what());
    }
}
