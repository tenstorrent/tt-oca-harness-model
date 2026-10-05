// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "aes_basetest.h"

#include <cstring>

// Quality coverage cases with independent oracles (no unconditional pass).

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
    uint32_t data[4] = {0xAABBCCDDu, 0x11223344u, 0x55667788u, 0x99AABBCCu};
    tb->write_data_in(data);
}

void snapshot_writable(testbench* tb, uint32_t* ctrl, uint32_t* gcm, uint32_t* regwen,
                       uint32_t* iv0, uint32_t* status, bool* fatal, bool* recov)
{
    tb->m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, *ctrl);
    tb->m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, *gcm);
    tb->m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, *regwen);
    tb->m_test->register_read_32(aes_basetest::IV_OFFSET, *iv0);
    *status = tb->read_status();
    *fatal = tb->alert_fatal_signal.read();
    *recov = tb->alert_recov_signal.read();
}

} // namespace

void testbench::test_coverage_prng_reseed_busy_interval()
{
    report_test_start("test_coverage_prng_reseed_busy_interval");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);
        wait_for_idle(1000);

        // Model reseed delay: 10 cycles @ 100 MHz = 100 ns.
        uint32_t trigger_rd = 0xFFFFFFFFu;
        m_test->register_read_32(aes_basetest::TRIGGER_OFFSET, trigger_rd);
        if (trigger_rd != 0u) {
            report_test_fail("test_coverage_prng_reseed_busy_interval",
                             "TRIGGER is write-only and must read back 0");
            return;
        }

        if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_prng_reseed_busy_interval",
                             "precondition: AES not idle before reseed");
            return;
        }

        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x8u);
        // Immediately after the write callback, enter_busy has cleared IDLE.
        const uint32_t busy_status = read_status();
        if ((busy_status & (1u << STATUS_IDLE_BIT)) != 0u) {
            report_test_fail("test_coverage_prng_reseed_busy_interval",
                             "TRIGGER.PRNG_RESEED did not assert busy (IDLE stayed 1)");
            return;
        }

        wait(150, SC_NS); // > 100 ns reseed delay
        const uint32_t done_status = read_status();
        if ((done_status & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_prng_reseed_busy_interval",
                             "reseed did not return to IDLE after expected delay");
            return;
        }
        if ((done_status & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_prng_reseed_busy_interval",
                             "reseed raised fatal alert");
            return;
        }

        // Configuring PER_64 / PER_8K must keep the cipher path live (one block
        // each). Full 64/8192-block threshold walks are out of runtime budget.
        write_ctrl(this, make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, false, false, 0x2));
        write_dummy_key(this);
        uint32_t block[4] = {0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC};
        write_data_in(block);
        wait_for_output_valid(2000);
        uint32_t out[4];
        read_data_out(out);
        wait_for_idle(2000);

        write_ctrl(this, make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, false, false, 0x4));
        write_dummy_key(this);
        write_data_in(block);
        wait_for_output_valid(2000);
        read_data_out(out);
        wait_for_idle(2000);

        report_test_pass("test_coverage_prng_reseed_busy_interval");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_prng_reseed_busy_interval", e.what());
    }
}

void testbench::test_coverage_error_state_writes_rejected()
{
    report_test_start("test_coverage_error_state_writes_rejected");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        // Program a known CTRL / GCM / IV baseline before locking.
        write_ctrl(this, make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, true, false, 0x1));
        write_dummy_iv(this);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);

        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x2);
        wait(10, SC_NS);

        if (!alert_fatal_signal.read()) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "ALERT_TEST fatal did not fire");
            return;
        }

        uint32_t ctrl0 = 0, gcm0 = 0, regwen0 = 0, iv0 = 0, status0 = 0;
        bool fatal0 = false, recov0 = false;
        snapshot_writable(this, &ctrl0, &gcm0, &regwen0, &iv0, &status0, &fatal0, &recov0);

        // These writes must be dropped while locked in ERROR.
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x081u);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x0);
        wait(1, SC_NS);
        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0xFu);
        wait(5, SC_NS);
        m_test->register_write_32(aes_basetest::IV_OFFSET, 0xDEADBEEFu);
        wait(1, SC_NS);

        uint32_t ctrl1 = 0, gcm1 = 0, regwen1 = 0, iv1 = 0, status1 = 0;
        bool fatal1 = false, recov1 = false;
        snapshot_writable(this, &ctrl1, &gcm1, &regwen1, &iv1, &status1, &fatal1, &recov1);

        if (ctrl1 != ctrl0 || gcm1 != gcm0 || regwen1 != regwen0 || iv1 != iv0) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "ERROR-state write mutated a writable register");
            return;
        }
        if (!fatal1 || ((status1 & (1u << STATUS_ALERT_FATAL_BIT)) == 0u)) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "fatal alert cleared without reset");
            return;
        }
        if ((status1 & (1u << STATUS_OUTPUT_VALID_BIT)) !=
            (status0 & (1u << STATUS_OUTPUT_VALID_BIT))) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "ERROR-state write changed OUTPUT_VALID");
            return;
        }

        // Only reset recovers.
        m_test->trigger_reset();
        wait(20, SC_NS);
        if (alert_fatal_signal.read() ||
            (read_status() & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_error_state_writes_rejected",
                             "reset did not clear fatal ERROR state");
            return;
        }

        report_test_pass("test_coverage_error_state_writes_rejected");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_error_state_writes_rejected", e.what());
    }
}

void testbench::test_coverage_gcm_aes192_aes256_init()
{
    report_test_start("test_coverage_gcm_aes192_aes256_init");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        const testbench::key_len_e lens[] = {AES_192, AES_256};
        for (auto kl : lens) {
            configure_aes(AES_ENC, AES_MODE_GCM, kl, false);
            write_dummy_key(this);
            write_dummy_iv(this);
            write_gcm_phase(0x01, 16);
            wait_for_idle(2000);

            uint32_t gcm = 0;
            m_test->register_read_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, gcm);
            if ((gcm & 0x3Fu) != 0x01u) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_INIT phase did not commit");
                return;
            }
            const uint32_t st = read_status();
            if ((st & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_INIT raised fatal");
                return;
            }
            if ((st & (1u << STATUS_IDLE_BIT)) == 0u) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_INIT left AES non-idle");
                return;
            }
            if ((st & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_INIT must not emit DATA_OUT");
                return;
            }

            // Advance to AAD then SAVE and require a 16-byte export (H/S path live).
            write_gcm_phase(0x04, 16);
            uint8_t scratch[16] = {0};
            uint8_t aad[16] = {0x01, 0x02, 0x03, 0x04};
            gcm_feed_block(aad, scratch, false);
            write_gcm_phase(0x10, 16);
            wait(10, SC_NS);
            if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) == 0u) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_SAVE after AAD produced no output");
                return;
            }
            uint32_t saved[4] = {};
            read_data_out(saved);
            bool nonzero = false;
            for (uint32_t w : saved) {
                if (w != 0u) nonzero = true;
            }
            // GHASH(S) XOR is nonzero for this AAD/key/IV combination.
            if (!nonzero) {
                report_test_fail("test_coverage_gcm_aes192_aes256_init",
                                 "GCM_SAVE export was all-zero (H/S init likely skipped)");
                return;
            }
            wait_for_idle(2000);
        }

        report_test_pass("test_coverage_gcm_aes192_aes256_init");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_gcm_aes192_aes256_init", e.what());
    }
}

void testbench::test_coverage_gcm_output_valid_iv_guard()
{
    report_test_start("test_coverage_gcm_output_valid_iv_guard");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, false);
        write_dummy_key(this);
        write_dummy_iv(this);

        // DATA_IN complete while still in GCM_INIT must not auto-start.
        write_dummy_data(this);
        wait(20, SC_NS);
        if (read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) {
            report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                             "GCM_INIT auto-started from DATA_IN");
            return;
        }

        write_gcm_phase(0x01, 16);
        write_gcm_phase(0x04, 16);
        uint8_t scratch[16] = {0};
        uint8_t aad[16] = {0x01, 0x02, 0x03, 0x04};
        gcm_feed_block(aad, scratch, false);
        write_gcm_phase(0x10, 16);
        wait(10, SC_NS);

        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) == 0u) {
            report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                             "precondition: GCM_SAVE did not set OUTPUT_VALID");
            return;
        }

        uint32_t iv_before[4] = {};
        for (int i = 0; i < 4; ++i) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + static_cast<unsigned>(i * 4),
                                     iv_before[i]);
        }
        uint32_t out_before_status = read_status();

        const uint32_t new_iv[4] = {0xA5A5A5A5u, 0x5A5A5A5Au, 0x11223344u, 0x55667788u};
        write_iv(new_iv);
        wait(10, SC_NS);

        const uint32_t st = read_status();
        if ((st & (1u << STATUS_OUTPUT_VALID_BIT)) == 0u) {
            report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                             "IV write while OUTPUT_VALID cleared OUTPUT_VALID");
            return;
        }
        if ((st & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                             "IV write while OUTPUT_VALID started a new operation");
            return;
        }
        if ((out_before_status & (1u << STATUS_OUTPUT_VALID_BIT)) == 0u) {
            report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                             "precondition OUTPUT_VALID lost before IV write");
            return;
        }

        // Model accepts IV writes when idle even with OUTPUT_VALID; auto-start
        // must still refuse. Assert the documented IV update.
        for (int i = 0; i < 4; ++i) {
            uint32_t iv_after = 0;
            m_test->register_read_32(aes_basetest::IV_OFFSET + static_cast<unsigned>(i * 4),
                                     iv_after);
            if (iv_after != new_iv[i]) {
                report_test_fail("test_coverage_gcm_output_valid_iv_guard",
                                 "IV write while OUTPUT_VALID was ignored unexpectedly");
                return;
            }
        }

        report_test_pass("test_coverage_gcm_output_valid_iv_guard");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_gcm_output_valid_iv_guard", e.what());
    }
}

void testbench::test_coverage_gcm_manual_init_no_iv()
{
    report_test_start("test_coverage_gcm_manual_init_no_iv");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        uint32_t actual[4] = {0x2b7e1516u, 0x28aed2a6u, 0xabf71588u, 0x09cf4f3cu};
        generate_two_share_key(share0, share1, actual, 4);

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, true, false);
        write_key_shares(share0, share1, 4);
        write_gcm_phase(0x01u, 16u);
        trigger_manual_start();
        wait_for_idle(2000);
        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_manual_init_no_iv",
                             "GCM_INIT without IV must not emit DATA_OUT");
            return;
        }
        if ((read_status() & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_manual_init_no_iv",
                             "missing IV during GCM_INIT must not fatal");
            return;
        }

        const uint32_t iv[4] = {0xcafebabeu, 0xfacedbadu, 0xdecaf888u, 0x00000001u};
        write_iv(iv);
        write_gcm_phase(0x01u, 16u);
        trigger_manual_start();
        wait_for_idle(2000);
        if ((read_status() & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_manual_init_no_iv",
                             "manual GCM_INIT with key+IV must not fatal");
            return;
        }
        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_gcm_manual_init_no_iv",
                             "GCM_INIT must not emit DATA_OUT");
            return;
        }

        report_test_pass("test_coverage_gcm_manual_init_no_iv");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_gcm_manual_init_no_iv", e.what());
    }
}

void testbench::test_coverage_aes_none_rejects_cipher()
{
    report_test_start("test_coverage_aes_none_rejects_cipher");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        // Illegal MODE sanitises to AES_NONE (0x3F). Manual START must not
        // produce ciphertext (get_openssl_cipher returns nullptr → fatal).
        const uint32_t ctrl_none = make_ctrl(AES_ENC, AES_MODE_NONE, AES_128, true, false, 0x1);
        write_ctrl(this, ctrl_none);
        write_dummy_key(this);
        uint32_t block[4] = {0x01020304u, 0x05060708u, 0x090a0b0cu, 0x0d0e0f10u};
        write_data_in(block);
        trigger_manual_start();
        wait(50, SC_NS);

        // TRIGGER.START ignores AES_NONE entirely (no spawn).
        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_aes_none_rejects_cipher",
                             "AES_NONE produced OUTPUT_VALID");
            return;
        }
        if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_aes_none_rejects_cipher",
                             "AES_NONE START left AES busy");
            return;
        }

        report_test_pass("test_coverage_aes_none_rejects_cipher");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_aes_none_rejects_cipher", e.what());
    }
}
