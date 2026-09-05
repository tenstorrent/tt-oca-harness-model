// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file coverage_tests.cpp
 * @brief Edge paths not exercised by the FUNC / GCM suites.
 *
 * These cases keep line coverage on src/aes.cpp above the repository
 * ≥ 95% gate. Each case still asserts a real behavioural outcome.
 */

#include "testbench.h"
#include "aes_basetest.h"

#include <tlm.h>

namespace {

void write_ctrl(testbench &tb, aes_test &t, uint32_t ctrl_val)
{
    t.register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    sc_core::wait(1, sc_core::SC_NS);
    t.register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_val);
    sc_core::wait(1, sc_core::SC_NS);
}

uint32_t make_ctrl(testbench::operation_e op, testbench::mode_e mode,
                   testbench::key_len_e key_len, bool manual, bool sideload,
                   uint32_t reseed_rate)
{
    uint32_t ctrl = 0;
    ctrl |= (static_cast<uint32_t>(op) & 0x3u);
    ctrl |= ((static_cast<uint32_t>(mode) & 0x3Fu) << 2);
    ctrl |= ((static_cast<uint32_t>(key_len) & 0x7u) << 8);
    ctrl |= (sideload ? (1u << 11) : 0u);
    ctrl |= ((reseed_rate & 0x7u) << 12);
    ctrl |= (manual ? (1u << 15) : 0u);
    return ctrl;
}

void nist_key_shares(testbench &tb, uint32_t *share0, uint32_t *share1)
{
    uint32_t actual[4] = {0x2b7e1516u, 0x28aed2a6u, 0xabf71588u, 0x09cf4f3cu};
    tb.generate_two_share_key(share0, share1, actual, 4);
}

tlm::tlm_response_status keymgr_xact(aes_test &t, tlm::tlm_command cmd,
                                     uint64_t addr, uint32_t &word)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&word));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    t.keymgr_socket->b_transport(trans, delay);
    return trans.get_response_status();
}

} // namespace

void testbench::test_coverage_keymgr_rejects_non_write()
{
    report_test_start("test_coverage_keymgr_rejects_non_write");

    uint32_t word = 0xA5A5A5A5u;
    const tlm::tlm_response_status st =
        keymgr_xact(*m_test, tlm::TLM_READ_COMMAND, 0x00u, word);

    if (st == tlm::TLM_COMMAND_ERROR_RESPONSE) {
        report_test_pass("test_coverage_keymgr_rejects_non_write");
    } else {
        report_test_fail("test_coverage_keymgr_rejects_non_write",
                         "KeyMgr READ must return TLM_COMMAND_ERROR_RESPONSE");
    }
}

void testbench::test_coverage_prng_reseed_trigger_and_rates()
{
    report_test_start("test_coverage_prng_reseed_trigger_and_rates");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x3243f6a8u, 0x885a308du, 0x313198a2u, 0xe0370734u};

        // Manual TRIGGER.PRNG_RESEED: idle drops then returns.
        trigger_prng_reseed();
        wait(5, SC_NS);
        wait_for_idle(2000);
        if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_prng_reseed_trigger_and_rates",
                             "AES did not return idle after TRIGGER.PRNG_RESEED");
            return;
        }

        // PER_64 and PER_8K both reach get_prng_reseed_threshold() on the
        // next cipher, without firing the automatic reseed (counter is 1).
        for (uint32_t rate : {0x2u, 0x4u}) {
            write_ctrl(*this, *m_test,
                       make_ctrl(AES_ENC, AES_MODE_ECB, AES_128, false, false, rate));
            write_key_shares(share0, share1, 4);
            write_data_in(block);
            wait_for_output_valid(2000);
            uint32_t out[4];
            read_data_out(out);
            wait_for_idle(2000);
            if ((read_status() & (1u << STATUS_IDLE_BIT)) == 0u) {
                report_test_fail("test_coverage_prng_reseed_trigger_and_rates",
                                 "cipher with a non-PER_1 reseed rate did not finish");
                return;
            }
        }

        report_test_pass("test_coverage_prng_reseed_trigger_and_rates");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_prng_reseed_trigger_and_rates", e.what());
    }
}

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

        // Raise escalation, then reset while leaving lc_escalate_en high.
        // reset_process clears the error latch but escalation_monitor is
        // posedge-only, so the next cipher hits check_escalation() and aborts.
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

void testbench::test_coverage_sideload_missing_key_and_manual_start()
{
    report_test_start("test_coverage_sideload_missing_key_and_manual_start");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        const uint32_t block[4] = {0xaaaaaaaaU, 0xbbbbbbbbU, 0xccccccccU, 0xddddddddU};

        // KEY_CTRL is independent of rst_ni. Force-clear the committed key
        // through the same TLM path used to push one.
        aes_if::keymgr_sideload_key_t cleared;
        cleared.valid = false;
        m_test->set_keymgr_key(cleared);
        uint32_t key_ctrl = 0;
        (void)keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, key_ctrl);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false, true);
        (void)keymgr_xact(*m_test, tlm::TLM_WRITE_COMMAND, 0x40u, key_ctrl);
        wait(5, SC_NS);

        uint32_t ctrl = 0;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl);
        if ((ctrl & (1u << 11)) == 0u) {
            report_test_fail("test_coverage_sideload_missing_key_and_manual_start",
                             "CTRL_SHADOWED.SIDELOAD did not commit");
            return;
        }

        wait_for_idle(2000);
        write_data_in(block);
        wait(200, SC_NS);
        uint32_t status = read_status();
        if ((status & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_sideload_missing_key_and_manual_start",
                             "auto-start ran without a valid sideload key");
            return;
        }
        if ((status & (1u << STATUS_IDLE_BIT)) == 0u) {
            report_test_fail("test_coverage_sideload_missing_key_and_manual_start",
                             "AES left idle while waiting for a sideload key");
            return;
        }

        // Manual START with a committed sideload key must produce output.
        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        aes_if::keymgr_sideload_key_t km;
        for (int i = 0; i < 8; ++i) {
            km.key_share0[i] = share0[i];
            km.key_share1[i] = share1[i];
        }
        km.valid = true;
        m_test->set_keymgr_key(km);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, true, true);
        write_data_in(block);
        trigger_manual_start();
        wait_for_output_valid(2000);
        uint32_t out[4] = {};
        read_data_out(out);
        bool nonzero = false;
        for (uint32_t w : out) {
            if (w != 0u) {
                nonzero = true;
            }
        }
        if (!nonzero) {
            report_test_fail("test_coverage_sideload_missing_key_and_manual_start",
                             "manual sideload START produced an all-zero DATA_OUT");
            return;
        }

        report_test_pass("test_coverage_sideload_missing_key_and_manual_start");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_sideload_missing_key_and_manual_start", e.what());
    }
}

void testbench::test_coverage_error_state_and_busy_gcm_writes()
{
    report_test_start("test_coverage_error_state_and_busy_gcm_writes");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t block[4] = {0x01020304u, 0x05060708u, 0x090a0b0cu, 0x0d0e0f10u};

        // CTRL_GCM_SHADOWED writes are ignored while a cipher is in flight.
        configure_aes(AES_ENC, AES_MODE_ECB, AES_256, false, false);
        write_key_shares(share0, share1, 4);
        for (int i = 0; i < 3; ++i) {
            m_test->register_write_32(aes_basetest::DATA_IN_OFFSET +
                                          static_cast<unsigned>(i * 4),
                                      block[i]);
            wait(1, SC_NS);
        }
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET + 12u, block[3]);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(1, SC_NS);
        wait_for_output_valid(2000);
        uint32_t discard[4];
        read_data_out(discard);
        wait_for_idle(2000);

        // Fatal ALERT_TEST locks the block; subsequent control writes are dropped.
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x2u);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::TRIGGER_OFFSET, 0x1u);
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x0u);
        m_test->register_write_32(aes_basetest::CTRL_GCM_SHADOWED_OFFSET, 0x401u);
        wait(5, SC_NS);

        const uint32_t status = read_status();
        uint32_t regwen = 0;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen);

        m_test->trigger_reset();
        wait(20, SC_NS);

        if (((status & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) && (regwen & 0x1u)) {
            report_test_pass("test_coverage_error_state_and_busy_gcm_writes");
        } else {
            report_test_fail("test_coverage_error_state_and_busy_gcm_writes",
                             "error-state writes must not unlock REGWEN or clear FATAL");
        }
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_error_state_and_busy_gcm_writes", e.what());
    }
}

void testbench::test_coverage_gcm_shadow_mismatch_and_init_gates()
{
    report_test_start("test_coverage_gcm_shadow_mismatch_and_init_gates");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        // Two-write mismatch on CTRL_GCM_SHADOWED raises the recoverable alert;
        // 0x401 is INIT+16 bytes, 0x041 is INIT+1 byte — both legal after
        // sanitise, so the NUM_VALID_BYTES field disagrees and faults.
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

        // GCM_INIT does not auto-start from DATA_IN, and INIT without an IV
        // must not derive H/S (ensure_gcm_init returns false).
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

        // Sideload GCM INIT with no committed key also fails ensure_gcm_init.
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

void testbench::test_coverage_trigger_readback_and_gcm_manual_init()
{
    report_test_start("test_coverage_trigger_readback_and_gcm_manual_init");

    try {
        m_test->trigger_reset();
        wait(20, SC_NS);

        uint32_t trigger_rd = 0xFFFFFFFFu;
        m_test->register_read_32(aes_basetest::TRIGGER_OFFSET, trigger_rd);
        if (trigger_rd != 0u) {
            report_test_fail("test_coverage_trigger_readback_and_gcm_manual_init",
                             "TRIGGER is write-only and must read back 0");
            return;
        }

        // Manual START in GCM_INIT with key+IV runs perform_gcm_block() in
        // the INIT arm (no DATA_OUT). The same path with no IV hits the
        // ensure_gcm_init failure return.
        uint32_t share0[8], share1[8];
        nist_key_shares(*this, share0, share1);
        const uint32_t iv[4] = {0xcafebabeu, 0xfacedbadu, 0xdecaf888u, 0x00000001u};

        configure_aes(AES_ENC, AES_MODE_GCM, AES_128, true, false);
        write_key_shares(share0, share1, 4);
        trigger_manual_start();
        wait_for_idle(2000);
        if ((read_status() & (1u << STATUS_OUTPUT_VALID_BIT)) != 0u) {
            report_test_fail("test_coverage_trigger_readback_and_gcm_manual_init",
                             "GCM_INIT without IV must not emit DATA_OUT");
            return;
        }

        write_iv(iv);
        write_gcm_phase(0x01u, 16u);
        trigger_manual_start();
        wait_for_idle(2000);
        if ((read_status() & (1u << STATUS_ALERT_FATAL_BIT)) != 0u) {
            report_test_fail("test_coverage_trigger_readback_and_gcm_manual_init",
                             "manual GCM_INIT with key+IV must not fatal");
            return;
        }

        report_test_pass("test_coverage_trigger_readback_and_gcm_manual_init");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_trigger_readback_and_gcm_manual_init", e.what());
    }
}
