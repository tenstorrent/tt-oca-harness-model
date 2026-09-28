// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "csrng_basetest.h"
#include <array>

namespace {

constexpr uint32_t CTRL_ALL_ENABLE = 0x6666u;  // ENABLE/SW_APP/READ_INT/FIPS_FORCE = 0x6

} // namespace

void testbench::test_coverage_invalid_instance_and_helpers()
{
    report_test_start("test_coverage_invalid_instance_and_helpers");

    try {
        apply_reset();
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, CTRL_ALL_ENABLE);
        wait(5, SC_NS);

        // Invalid instance: generate / instantiate / reseed / update / uninstantiate.
        if (m_crng->generate_random_blocks(-1, 1) ||
            m_crng->generate_random_blocks(3, 1)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "invalid generate_random_blocks succeeded");
            return;
        }
        if (m_crng->generate_random_blocks(0, 1)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "generate on uninstantiated instance succeeded");
            return;
        }

        if (!m_crng->reseed_required(-1)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "reseed_required(-1) should be true");
            return;
        }
        (void)m_crng->reseed_required(0);
        m_crng->reset_reseed_counter(-1);
        m_crng->reset_reseed_counter(0);

        if (m_crng->validate_instance_number(-1) ||
            m_crng->validate_instance_number(3)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "validate_instance_number accepted a bad id");
            return;
        }

        std::array<uint32_t, 12> extra{};
        extra[0] = 0xA5A5A5A5u;
        if (m_crng->cmd_instantiate(-1, 0x9, 0, extra) ||
            m_crng->cmd_generate(-1, 1) ||
            m_crng->cmd_reseed(-1, 0x9, 0, extra) ||
            m_crng->cmd_update(-1, 1, extra)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "invalid-instance command succeeded");
            return;
        }

        (void)m_crng->cmd_uninstantiate(-1);
        m_crng->uninstantiate_instance(-1);

        // Instantiate SW instance, then UPDATE with clen=0 (must fail).
        if (!m_crng->cmd_instantiate(0, 0x9, 0, extra)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "deterministic instantiate failed");
            return;
        }
        if (m_crng->cmd_update(0, 0, extra)) {
            report_test_fail("test_coverage_invalid_instance_and_helpers",
                             "UPDATE with clen=0 should fail");
            return;
        }

        (void)m_crng->cmd_uninstantiate(0);
        report_test_pass("test_coverage_invalid_instance_and_helpers");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_invalid_instance_and_helpers", e.what());
    }
}

void testbench::test_coverage_fsm_states_and_genbits_repeat()
{
    report_test_start("test_coverage_fsm_states_and_genbits_repeat");

    try {
        apply_reset();
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, CTRL_ALL_ENABLE);
        wait(5, SC_NS);

        // Each poked state must publish its own MAIN_SM_STATE encoding.
        // Checking only "not zero" after the loop accepts a model that writes
        // 0xFF for every state, because ERROR is last. COMMAND_DISPATCH is
        // omitted: that arm calls process_command() and would not stay put.
        using St = csrng_model::CommandFSMState;
        const struct { St state; uint32_t encoding; } steps[] = {
            { St::IDLE,              csrng_model::MAIN_SM_IDLE },
            { St::ENTROPY_REQUEST,   csrng_model::MAIN_SM_ENTROPY_REQUEST },
            { St::CTR_DRBG_GENERATE, csrng_model::MAIN_SM_CTR_DRBG_GEN },
            { St::STATE_UPDATE,      csrng_model::MAIN_SM_STATE_UPDATE },
            { St::ERROR,             csrng_model::MAIN_SM_ERROR },
        };
        for (const auto& step : steps) {
            m_crng->m_cmd_fsm_state = step.state;
            m_crng->cmd_fsm_event.notify(SC_ZERO_TIME);
            wait(2, SC_NS);
            uint32_t sm = 0;
            m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, sm);
            if (sm != step.encoding) {
                report_test_fail("test_coverage_fsm_states_and_genbits_repeat",
                                 "MAIN_SM_STATE encoding did not follow the poked FSM state");
                return;
            }
        }

        // Pulse interrupt / alert update threads a second time.
        m_crng->intr_update_event.notify(SC_ZERO_TIME);
        m_crng->alert_update_event.notify(SC_ZERO_TIME);
        wait(2, SC_NS);

        // 64-bit GENBITS repetition: same 64-bit pair as the previous one.
        m_crng->m_genbits_valid = true;
        m_crng->m_genbits_fips = true;
        m_crng->m_genbits_read_index = 0;
        m_crng->m_genbits_buffer = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
        m_crng->m_previous_genbits_64 =
            (static_cast<uint64_t>(0x11111111u) << 32) | 0x22222222u;

        // The two reads are one 64-bit pair, planted equal to m_previous_genbits_64.
        // A denied read returns 0, and a check that only looked at the alert bit
        // would then pass for the wrong reason, so the words themselves are part
        // of the oracle. RECOV_ALERT_STS.CS_BUS_CMP_ALERT is bit 12
        // (csrng_register.h); there is no other legal bit for this condition.
        uint32_t word = 0;
        m_test->register_read_32(csrng_basetest::GENBITS_OFFSET, word);
        wait(1, SC_NS);
        if (word != 0x11111111u) {
            report_test_fail("test_coverage_fsm_states_and_genbits_repeat",
                             "first GENBITS word was not the planted pair");
            return;
        }
        m_test->register_read_32(csrng_basetest::GENBITS_OFFSET, word);
        wait(1, SC_NS);
        if (word != 0x22222222u) {
            report_test_fail("test_coverage_fsm_states_and_genbits_repeat",
                             "second GENBITS word was not the planted pair");
            return;
        }

        uint32_t recov = 0;
        m_test->register_read_32(csrng_basetest::RECOV_ALERT_STS_OFFSET, recov);
        if ((recov & (1u << 12)) == 0) {
            report_test_fail("test_coverage_fsm_states_and_genbits_repeat",
                             "CS_BUS_CMP_ALERT (bit 12) not set after a repeated 64-bit GENBITS pair");
            return;
        }

        report_test_pass("test_coverage_fsm_states_and_genbits_repeat");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_fsm_states_and_genbits_repeat", e.what());
    }
}

void testbench::test_coverage_int_state_and_regwen_denies()
{
    report_test_start("test_coverage_int_state_and_regwen_denies");

    try {
        apply_reset();
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, CTRL_ALL_ENABLE);
        wait(5, SC_NS);

        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, 0x7);
        wait(1, SC_NS);

        // Instance 3 is out of range (value & 0x3 == 3).
        m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, 0x3);
        wait(1, SC_NS);
        uint32_t val = 0xFFFFFFFFu;
        m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, val);
        wait(1, SC_NS);
        if (val != 0) {
            report_test_fail("test_coverage_int_state_and_regwen_denies",
                             "INT_STATE_VAL for instance 3 was not 0");
            return;
        }

        // Register access offers the target 10 ns and waits whatever comes back.
        // The register file ignores the delay argument, so the wait is exactly
        // that 10 ns. Zeroing it or adding to it both move this timestamp.
        {
            const sc_core::sc_time t0 = sc_core::sc_time_stamp();
            uint32_t ignored = 0;
            m_test->register_read_32(csrng_basetest::CTRL_OFFSET, ignored);
            if (sc_core::sc_time_stamp() - t0 != sc_core::sc_time(10, sc_core::SC_NS)) {
                report_test_fail("test_coverage_int_state_and_regwen_denies",
                                 "register read did not wait exactly the 10 ns it offered");
                return;
            }
        }

        // Deterministic INSTANTIATE so the 14-word window is not all zeros.
        // A fresh instance reads as zero, and "the 15th word equals the first"
        // is then true whether or not the index wraps.
        m_test->register_write_32(csrng_basetest::CMD_REQ_OFFSET,
                                  (1u) | (0u << 4) | (0x9u << 8) | (0u << 12));
        wait(50, SC_US);
        uint32_t sts = 0;
        m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, sts);
        if (((sts >> 3) & 0x7u) != 0u) {
            report_test_fail("test_coverage_int_state_and_regwen_denies",
                             "deterministic INSTANTIATE did not return CMD_STS success");
            return;
        }

        // 14 words: reseed counter, V[4], Key[8], status. The 15th read wraps
        // to word 0. Word 13 is the status byte, which is 1 after instantiate,
        // while word 0 (the counter) is 0, so a stuck index fails one of these.
        m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, 0x0);
        wait(1, SC_NS);
        uint32_t window[15] = {};
        for (int i = 0; i < 15; ++i) {
            m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, window[i]);
            wait(1, SC_NS);
        }
        if (window[14] != window[0] || window[13] == window[0]) {
            report_test_fail("test_coverage_int_state_and_regwen_denies",
                             "INT_STATE_VAL did not walk 14 words and wrap to the counter");
            return;
        }

        uint32_t interval_before = 0, fips_before = 0, enable_before = 0;
        m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, interval_before);
        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, fips_before);
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, enable_before);

        // Lock REGWEN and attempt writes that must be dropped.
        m_test->register_write_32(csrng_basetest::REGWEN_OFFSET, 0);
        wait(1, SC_NS);
        m_test->register_write_32(csrng_basetest::RESEED_INTERVAL_OFFSET, 0x1234);
        wait(1, SC_NS);
        m_test->register_write_32(csrng_basetest::FIPS_FORCE_OFFSET, 0x7);
        wait(1, SC_NS);
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, 0x9999u);
        wait(1, SC_NS);

        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, 0);
        wait(1, SC_NS);
        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, 0x0);
        wait(1, SC_NS);

        uint32_t interval_after = 0, fips_after = 0, ctrl_after = 0, enable_after = 0;
        m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, interval_after);
        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, fips_after);
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, ctrl_after);
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, enable_after);
        if (interval_after != interval_before || fips_after != fips_before ||
            ctrl_after != CTRL_ALL_ENABLE || enable_after != enable_before) {
            report_test_fail("test_coverage_int_state_and_regwen_denies",
                             "a locked write changed RESEED_INTERVAL, FIPS_FORCE, CTRL, or INT_STATE_READ_ENABLE");
            return;
        }

        // Extra reads of debug/status windows.
        m_test->register_read_32(csrng_basetest::ERR_CODE_OFFSET, val);
        m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, val);
        m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, val);
        m_test->register_read_32(csrng_basetest::GENBITS_VLD_OFFSET, val);
        m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, val);

        report_test_pass("test_coverage_int_state_and_regwen_denies");
    } catch (const std::exception& e) {
        report_test_fail("test_coverage_int_state_and_regwen_denies", e.what());
    }
}
