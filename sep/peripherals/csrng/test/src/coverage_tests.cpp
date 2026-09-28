// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file coverage_tests.cpp
 * @brief Edge paths not exercised by the FUNC suites.
 *
 * These cases keep line coverage on src/csrng.cpp above the repository
 * ≥ 95% gate. Each case still asserts a real behavioural outcome.
 */

#include "testbench.h"
#include "csrng_basetest.h"

#include <array>

namespace {

uint32_t cmd_header(uint8_t acmd, uint8_t clen, uint8_t flag0, uint16_t glen)
{
    return (acmd & 0xFu) | ((clen & 0xFu) << 4) | ((flag0 & 0xFu) << 8) |
           ((glen & 0xFFFu) << 12);
}

bool cmd_ready(csrng_test &t)
{
    uint32_t sts = 0;
    t.register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, sts);
    return (sts & 0x2u) != 0;
}

void wait_ready(csrng_test &t)
{
    for (int i = 0; i < 200 && !cmd_ready(t); ++i) {
        sc_core::wait(10, sc_core::SC_US);
    }
}

} // namespace

void testbench::test_coverage_invalid_instance_and_seed_life()
{
    report_test_start("test_coverage_invalid_instance_and_seed_life");

    try {
        apply_reset();

        // Seed-life helpers: out-of-range instance is treated as "must reseed",
        // and a valid instance with a zero counter does not yet need a reseed.
        if (!m_crng->reseed_required(-1) || !m_crng->reseed_required(5)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "reseed_required must be true for an invalid instance");
            return;
        }
        if (m_crng->reseed_required(0)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "fresh instance 0 should not require a reseed");
            return;
        }

        m_crng->reset_reseed_counter(-1);
        m_crng->reset_reseed_counter(0);
        uint32_t ctr = 0;
        m_test->register_read_32(csrng_basetest::RESEED_COUNTER_0_OFFSET, ctr);
        if (ctr != 0u) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "reset_reseed_counter(0) left a non-zero counter");
            return;
        }

        const std::array<uint32_t, 12> empty{};
        if (m_crng->generate_random_blocks(-1, 1u)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "generate_random_blocks(-1) must fail");
            return;
        }
        if (m_crng->generate_random_blocks(0, 1u)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "GENERATE on an uninstantiated instance must fail");
            return;
        }
        if (m_crng->cmd_instantiate(-1, 0x9u, 0u, empty)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "INSTANTIATE(-1) must fail");
            return;
        }
        if (m_crng->cmd_generate(-1, 1u)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "GENERATE(-1) must fail");
            return;
        }
        if (m_crng->cmd_reseed(-1, 0x9u, 0u, empty)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "RESEED(-1) must fail");
            return;
        }
        if (m_crng->cmd_update(-1, 1u, empty)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "UPDATE(-1) must fail");
            return;
        }
        if (!m_crng->cmd_uninstantiate(-1)) {
            report_test_fail("test_coverage_invalid_instance_and_seed_life",
                             "UNINSTANTIATE always succeeds, even for -1");
            return;
        }
        m_crng->uninstantiate_instance(-1);

        report_test_pass("test_coverage_invalid_instance_and_seed_life");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_invalid_instance_and_seed_life", e.what());
    }
}

void testbench::test_coverage_update_requires_additional_data()
{
    report_test_start("test_coverage_update_requires_additional_data");

    try {
        apply_reset();
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, 0x6666u);
        wait(20, SC_US);
        wait_ready(*m_test);

        m_test->register_write_32(csrng_basetest::CMD_REQ_OFFSET,
                                  cmd_header(1u, 0u, 0x9u, 0u));
        wait(50, SC_US);
        wait_ready(*m_test);

        const std::array<uint32_t, 12> empty{};
        if (m_crng->cmd_update(0, 0u, empty)) {
            report_test_fail("test_coverage_update_requires_additional_data",
                             "UPDATE with clen=0 must be rejected");
            return;
        }

        m_test->register_write_32(csrng_basetest::CMD_REQ_OFFSET,
                                  cmd_header(4u, 0u, 0u, 0u));
        wait(50, SC_US);

        uint32_t sts = 0;
        m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, sts);
        const uint32_t cmd_sts = (sts >> 3) & 0x7u;
        if (cmd_sts != 0x3u) {
            report_test_fail("test_coverage_update_requires_additional_data",
                             "CMD_STS must report INVALID_CMD_SEQ for UPDATE clen=0");
            return;
        }

        report_test_pass("test_coverage_update_requires_additional_data");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_update_requires_additional_data", e.what());
    }
}

void testbench::test_coverage_int_state_num_out_of_range()
{
    report_test_start("test_coverage_int_state_num_out_of_range");

    try {
        apply_reset();
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, 0x6666u);
        wait(10, SC_US);
        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, 0xFu);
        wait(1, SC_US);
        m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, 0x3u);
        wait(1, SC_US);

        uint32_t val = 0xFFFFFFFFu;
        m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, val);
        if (val != 0u) {
            report_test_fail("test_coverage_int_state_num_out_of_range",
                             "INT_STATE_VAL must read 0 for instance 3");
            return;
        }

        report_test_pass("test_coverage_int_state_num_out_of_range");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_int_state_num_out_of_range", e.what());
    }
}

void testbench::test_coverage_reseed_interval_locked_and_fsm_states()
{
    report_test_start("test_coverage_reseed_interval_locked_and_fsm_states");

    try {
        apply_reset();

        m_test->register_write_32(csrng_basetest::RESEED_INTERVAL_OFFSET, 0xA5A5A5A5u);
        wait(1, SC_US);
        m_test->register_write_32(csrng_basetest::REGWEN_OFFSET, 0x0u);
        wait(1, SC_US);
        m_test->register_write_32(csrng_basetest::RESEED_INTERVAL_OFFSET, 0x5A5A5A5Au);
        wait(1, SC_US);

        uint32_t interval = 0;
        m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, interval);
        if (interval != 0xA5A5A5A5u) {
            report_test_fail("test_coverage_reseed_interval_locked_and_fsm_states",
                             "RESEED_INTERVAL write must be ignored while REGWEN is locked");
            return;
        }

        // Same encodings as the other FSM poke, checked per state. A final
        // "not zero" read only ever sees ERROR (0xFF).
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
            m_crng->cmd_fsm_event.notify(sc_core::SC_ZERO_TIME);
            wait(1, SC_NS);
            uint32_t sm = 0;
            m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, sm);
            if (sm != step.encoding) {
                report_test_fail("test_coverage_reseed_interval_locked_and_fsm_states",
                                 "MAIN_SM_STATE encoding did not follow the poked FSM state");
                return;
            }
        }

        report_test_pass("test_coverage_reseed_interval_locked_and_fsm_states");
    } catch (const std::exception &e) {
        report_test_fail("test_coverage_reseed_interval_locked_and_fsm_states", e.what());
    }
}
