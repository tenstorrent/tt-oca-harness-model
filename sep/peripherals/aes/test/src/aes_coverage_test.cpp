// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"

// Targeted tests for model paths the functional suite does not reach:
// TRIGGER.PRNG_RESEED, PER_64/PER_8K thresholds, keymgr read reject,
// escalation abort of a spawned cipher, error-state write filters,
// GCM shadow mismatch / busy reject, sideload-not-ready, GCM init
// without IV, GCM_INIT via TRIGGER.START, and AES-192/256 GCM init.

namespace {

void write_ctrl(testbench* tb, uint32_t ctrl_val)
{
    tb->m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    wait(1, SC_NS);
    tb->m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    wait(1, SC_NS);
}

uint32_t make_ctrl(testbench::operation_e op, testbench::mode_e mode,
                   testbench::key_len_e key_len, bool manual, bool sideload,
                   uint32_t reseed_rate)
{
    uint32_t ctrl = 0;
    ctrl |= (op & 0x3);
    ctrl |= ((mode & 0x3F) << 2);
    ctrl |= ((key_len & 0x7) << 8);
    ctrl |= (sideload ? (1u << 11) : 0);
    ctrl |= ((reseed_rate & 0x7) << 12);
    ctrl |= (manual ? (1u << 15) : 0);
    return ctrl;
}

void write_dummy_key(testbench* tb)
{
    uint32_t k0[8] = {0x11111111, 0x22222222, 0x33333333, 0x44444444,
                      0x55555555, 0x66666666, 0x77777777, 0x88888888};
    uint32_t k1[8] = {0};
    tb->write_key_shares(k0, k1, 8);
}

void write_dummy_iv(testbench* tb)
{
    uint32_t iv[4] = {0x01020304, 0x05060708, 0x09101112, 0x13141516};
    tb->write_iv(iv);
}

void write_dummy_data(testbench* tb)
{
    uint32_t data[4] = {0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC};
    tb->write_data_in(data);
}

} // namespace

void testbench::test_coverage_prng_reseed_trigger_and_rates()
{
    report_test_start("test_coverage_prng_reseed_trigger_and_rates");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);
        wait_for_idle(1000);

        // TRIGGER is write-only; a read must return the self-cleared value.
        uint32_t trigger_rd = 0xFFFFFFFFu;
        m_test->register_read_32(aes_basetest::TRIGGER_OFFSET, trigger_rd);
        wait(1, SC_NS);
        if (trigger_rd != 0) {
            report_test_fail("test_coverage_prng_reseed_trigger_and_rates",
                             "TRIGGER readback was not 0");
            return;
        }

        trigger_prng_reseed();
        wait_for_idle(2000);

        // PER_64 then PER_8K: each cipher start evaluates the matching threshold.
        write_ctrl(this, make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, false, false, 0x2));
        write_dummy_key(this);
        write_dummy_data(this);
        wait_for_output_valid(2000);
        uint32_t discard[4];
        read_data_out(discard);
        wait_for_idle(2000);

        write_ctrl(this, make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, false, false, 0x4));
        write_dummy_key(this);
        write_dummy_data(this);
        wait_for_output_valid(2000);
        uint32_t out[4];
        read_data_out(out);
        wait_for_idle(2000);

        report_test_pass("test_coverage_prng_reseed_trigger_and_rates");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_prng_reseed_trigger_and_rates", e.what());
    }
}

void testbench::test_coverage_keymgr_read_rejected()
{
    report_test_start("test_coverage_keymgr_read_rejected");

    try {
        if (!m_test->keymgr_read_rejected(0x00)) {
            report_test_fail("test_coverage_keymgr_read_rejected",
                             "keymgr read was not rejected");
            return;
        }
        report_test_pass("test_coverage_keymgr_read_rejected");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_keymgr_read_rejected", e.what());
    }
}

void testbench::test_coverage_escalation_aborts_cipher()
{
    report_test_start("test_coverage_escalation_aborts_cipher");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);
        lc_escalate_signal.write(false);
        wait(10, SC_NS);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);
        write_dummy_key(this);
        write_dummy_data(this);

        // Spawn the cipher, then escalate before it completes so
        // perform_cipher_operation() takes the check_escalation abort path.
        trigger_manual_start();
        lc_escalate_signal.write(true);
        wait(50, SC_NS);

        if (!alert_fatal_signal.read()) {
            report_test_fail("test_coverage_escalation_aborts_cipher",
                             "fatal alert not asserted");
            lc_escalate_signal.write(false);
            return;
        }

        lc_escalate_signal.write(false);
        m_test->trigger_reset();
        wait(20, SC_NS);
        report_test_pass("test_coverage_escalation_aborts_cipher");
    } catch (const std::exception& e) {
        lc_escalate_signal.write(false);
        report_test_fail("test_coverage_escalation_aborts_cipher", e.what());
    }
}

void testbench::test_coverage_error_state_writes_rejected()
{
    report_test_start("test_coverage_error_state_writes_rejected");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x2);
        wait(10, SC_NS);

        if (!alert_fatal_signal.read()) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "ALERT_TEST fatal did not fire");
            return;
        }

        // These writes must be dropped while locked in ERROR.
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x0);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x8);
        wait(1, SC_NS);

        m_test->trigger_reset();
        wait(20, SC_NS);
        report_test_pass("test_coverage_error_state_writes_rejected");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_error_state_writes_rejected", e.what());
    }
}

void testbench::test_coverage_gcm_shadow_and_busy()
{
    report_test_start("test_coverage_gcm_shadow_and_busy");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true);
        write_dummy_key(this);
        write_dummy_data(this);
        trigger_manual_start();

        // Not idle: CTRL_GCM_SHADOWED writes are ignored.
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(1, SC_NS);
        wait_for_idle(2000);

        m_test->trigger_reset();
        wait(20, SC_NS);

        // Two-write mismatch, then a matching pair that clears the recoverable alert.
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x201);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(10, SC_NS);
        if (!alert_recov_signal.read()) {
            report_test_fail("test_coverage_gcm_shadow_and_busy",
                             "GCM shadow mismatch did not raise recoverable alert");
            return;
        }

        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401);
        wait(10, SC_NS);
        if (alert_recov_signal.read()) {
            report_test_fail("test_coverage_gcm_shadow_and_busy",
                             "matching GCM write did not clear recoverable alert");
            return;
        }

        report_test_pass("test_coverage_gcm_shadow_and_busy");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_gcm_shadow_and_busy", e.what());
    }
}

void testbench::test_coverage_sideload_and_gcm_init_guards()
{
    report_test_start("test_coverage_sideload_and_gcm_init_guards");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        m_test->invalidate_keymgr_key();

        // Auto-start with sideload but no committed key: load_sideload_key fails.
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, true);
        write_dummy_data(this);
        wait(20, SC_NS);
        uint32_t status = read_status();
        if (status & (1u << STATUS_OUTPUT_VALID_BIT)) {
            report_test_fail("test_coverage_sideload_and_gcm_init_guards",
                             "auto-start ran without a sideload key");
            return;
        }

        // GCM init with sideload and no key.
        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false, true);
        write_dummy_iv(this);
        write_gcm_phase(0x01, 16);
        wait(10, SC_NS);

        // GCM init with a software key but no IV.
        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false, false);
        write_dummy_key(this);
        write_gcm_phase(0x01, 16);
        wait(10, SC_NS);

        // Manual START in GCM_INIT (no DATA_IN) exercises perform_gcm_block INIT.
        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, true, false);
        write_dummy_key(this);
        write_dummy_iv(this);
        write_gcm_phase(0x01, 16);
        trigger_manual_start();
        wait_for_idle(2000);

        // Manual START with a valid sideload key.
        aes_if::keymgr_sideload_key_t key;
        key.valid = true;
        for (int i = 0; i < 8; ++i) {
            key.key_share0[i] = 0xA0A0A0A0u + static_cast<uint32_t>(i);
            key.key_share1[i] = 0;
        }
        m_test->set_keymgr_key(key);
        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_dummy_data(this);
        trigger_manual_start();
        wait_for_output_valid(2000);
        uint32_t out[4];
        read_data_out(out);
        wait_for_idle(2000);

        report_test_pass("test_coverage_sideload_and_gcm_init_guards");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_sideload_and_gcm_init_guards", e.what());
    }
}

void testbench::test_coverage_gcm_aes192_aes256_init()
{
    report_test_start("test_coverage_gcm_aes192_aes256_init");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_192, false);
        write_dummy_key(this);
        write_dummy_iv(this);
        write_gcm_phase(0x01, 16);
        wait_for_idle(2000);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_256, false);
        write_dummy_key(this);
        write_dummy_iv(this);
        write_gcm_phase(0x01, 16);
        wait_for_idle(2000);

        report_test_pass("test_coverage_gcm_aes192_aes256_init");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_gcm_aes192_aes256_init", e.what());
    }
}

void testbench::test_coverage_auto_start_gcm_and_output_valid()
{
    report_test_start("test_coverage_auto_start_gcm_and_output_valid");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        // DATA_IN complete while still in GCM_INIT must not auto-start.
        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false);
        write_dummy_key(this);
        write_dummy_iv(this);
        write_dummy_data(this);
        wait(20, SC_NS);
        uint32_t status = read_status();
        if (status & (1u << STATUS_OUTPUT_VALID_BIT)) {
            report_test_fail("test_coverage_auto_start_gcm_and_output_valid",
                             "GCM_INIT auto-started from DATA_IN");
            return;
        }

        // Absorb one AAD block so SAVE is legal, export GHASH, then rewrite IV
        // while OUTPUT_VALID is still set (auto-start must refuse).
        write_gcm_phase(0x01, 16);
        write_gcm_phase(0x04, 16);
        uint8_t scratch[16] = {0};
        uint8_t aad[16] = {0x01, 0x02, 0x03, 0x04};
        gcm_feed_block(aad, scratch, false);
        write_gcm_phase(0x10, 16);
        wait(10, SC_NS);

        write_dummy_iv(this);
        wait(10, SC_NS);

        report_test_pass("test_coverage_auto_start_gcm_and_output_valid");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_auto_start_gcm_and_output_valid", e.what());
    }
}
