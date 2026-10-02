// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/******************************************************************************
 * @file test_coverage.cpp
 * @brief Frontdoor coverage / quality scenarios for entropy_src
 ******************************************************************************/

#include "testbench.h"

#include <array>
#include <cstring>

#define COV_CHECK(cond, msg_stream)             \
    do {                                        \
        if (!(cond)) {                          \
            REG_ERROR(0, logger) << msg_stream; \
            ok = false;                         \
        }                                       \
    } while (false)

namespace {

uint32_t fifo_level(entropy_src_test* test)
{
    uint32_t st = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, st);
    return st & 0x7Fu;
}

bool wait_fifo_level_ge(entropy_src_test* test, uint32_t min_level, int budget)
{
    for (int i = 0; i < budget; ++i) {
        wait(sc_core::sc_time(100.0, sc_core::SC_NS));
        if (fifo_level(test) >= min_level)
            return true;
    }
    return false;
}

}  // namespace

// Boot rst_ni held low at elaboration; program STARTUP_CTRL, then release.
bool testbench::tc_cov_boot_rst_n_with_startup_delay()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t delay_ns = 500u;

    for (int i = 0; i < 8; ++i)
        wait(sc_core::SC_ZERO_TIME);

    // rst_ni still low: program STARTUP_CTRL through MMIO, then release.
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == delay_ns,
        "TC-COV-001: STARTUP_CTRL not programmed before rst_ni release");

    const sc_core::sc_time t0 = sc_core::sc_time_stamp();
    sig_rst_n.write(true);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);

    // During hold-off, LEVEL must stay 0.
    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 0.5, sc_core::SC_NS));
    COV_CHECK(fifo_level(test) == 0u,
        "TC-COV-001: FIFO filled before STARTUP_CTRL deadline");

    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 2.0, sc_core::SC_NS));
    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-001: FIFO did not fill after boot STARTUP_CTRL delay");
    COV_CHECK(sc_core::sc_time_stamp() >= t0 + sc_core::sc_time(delay_ns, sc_core::SC_NS),
        "TC-COV-001: fill observed before programmed delay elapsed");

    return ok;
}

bool testbench::tc_cov_fifo_reenable_startup_delay()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t delay_ns = 1200u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == delay_ns, "TC-COV-002: STARTUP_CTRL write failed");

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 64; ++i) {
        if (fifo_level(test) == 0u)
            break;
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }
    wait(sc_core::sc_time(200.0, sc_core::SC_US));
    COV_CHECK(fifo_level(test) == 0u, "TC-COV-002: FIFO not empty before re-enable");

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    for (int i = 0; i < 8; ++i) {
        wait(sc_core::SC_ZERO_TIME);
        COV_CHECK(fifo_level(test) == 0u,
            "TC-COV-002: LEVEL rose during post-reenable STARTUP_DELAY");
    }

    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 2.0, sc_core::SC_NS));
    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-002: FIFO did not fill after re-enable STARTUP_DELAY");

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0u);
    return ok;
}

// rst_ni during post-reenable STARTUP_DELAY: FIFO/regs/IRQ clean, then restart.
bool testbench::tc_cov_sw_reset_during_reenable_startup_delay()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t delay_ns = 2000u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x11111111u);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::sc_time(200.0, sc_core::SC_US));

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    apply_hw_reset();

    // Sample FIFO empty while rst_ni is held (apply_hw_reset already released).
    // Re-assert briefly to observe drained state under reset.
    sig_rst_n.write(false);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x7Fu) == 0u,
        "TC-COV-003: FIFO_STATUS.LEVEL not 0 while rst_ni held");
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-003: STARTUP_CTRL not cleared by rst_ni");
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-003: INTR_STATUS not cleared by rst_ni");
    COV_CHECK(!sig_intr.read(),
        "TC-COV-003: irq_o still asserted while rst_ni held");
    sig_rst_n.write(true);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);

    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-003: generation did not restart after rst_ni release");
    return ok;
}

bool testbench::tc_cov_hw_reset_rederive_state()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 300u);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-004: no fill before hw reset");

    apply_hw_reset();

    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::CTRL_RESET,
        "TC-COV-004: CTRL not at reset after hw reset");
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-004: STARTUP_CTRL not cleared after hw reset");
    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-004: generation did not restart after hw reset");
    return ok;
}

bool testbench::tc_cov_new_rdl_register_access()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    test->register_write_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x0000FFFFu,
        "TC-COV-005: HEALTH_TEST_WINDOW_SIZE mask failed");

    test->register_write_32(entropy_src_basetest::HT_WATERMARK_NUM_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::HT_WATERMARK_NUM_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x0000000Fu,
        "TC-COV-005: HT_WATERMARK_NUM mask failed");

    test->register_write_32(entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, 0x0000001Fu);
    test->register_read_32(entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x0000001Fu,
        "TC-COV-005: GENERATOR_0_SAMPLE_CLK_CONFIG RW failed");

    test->register_write_32(entropy_src_basetest::SHA256_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::SHA256_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::SHA256_STATUS_RESET,
        "TC-COV-005: SHA256_STATUS RO enforcement failed");

    // Health-test enable is a no-op for the generation thread: counters stay 0.
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000101u);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000101u,
        "TC-COV-005: HEALTH_TEST_CTRL enable write failed");
    wait(sc_core::sc_time(5.0, sc_core::SC_US));
    test->register_read_32(entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-005: health stub advanced REPETITION_TEST_COUNT");
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) == 0u,
        "TC-COV-005: health stub raised HEALTH_TEST_FAILED");
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0u);

    // HEALTH_TEST_STATUS W1C register storage via MMIO only (no HW seed).
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0xFFu);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-005: HEALTH_TEST_STATUS W1C/storage allowed software set");

    return ok;
}

bool testbench::tc_cov_fips_lock_w1s()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    // Literal writable masks (test-local oracles, not DUT members).
    struct LockedReg {
        unsigned offset;
        uint32_t wmask;
        uint32_t probe;
        const char* name;
    };
    const LockedReg locked[] = {
        {entropy_src_basetest::CTRL_OFFSET, 0x13FF0112u, 0x00000110u, "CTRL"},
        {entropy_src_basetest::ALERT_THRESHOLD_OFFSET, 0xFFFFFFFFu, 0x10u, "ALERT_THRESHOLD"},
        {entropy_src_basetest::MIN_ENTROPY_H_OFFSET, 0xFFFFFFFFu, 0x20u, "MIN_ENTROPY_H"},
        {entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, 0xFFFFu, 0x2u, "HEALTH_TEST_WINDOW_SIZE"},
        {entropy_src_basetest::APT_PROPORTION_LO_OFFSET, 0xFFFFu, 0x100u, "APT_PROPORTION_LO"},
        {entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, 0x1Fu, 0x1Fu, "GEN0_CLK"},
        {entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0xFFFu, 0x1u, "RING_OSC_ENABLE"},
        {entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0xFFFFu, 0x1u, "HEALTH_TEST_CTRL"},
    };

    for (const auto& r : locked) {
        test->register_write_32(r.offset, r.probe);
        test->register_read_32(r.offset, rd_val);
        COV_CHECK((rd_val & r.wmask) == (r.probe & r.wmask),
            "TC-COV-006: pre-lock write failed for " << r.name);
    }

    // FIFO_CTRL: ENABLE stays writable; churn bit 4 locks under FIPS.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x11u);
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x11u, "TC-COV-006: FIFO_CTRL pre-lock write failed");

    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0x1u);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x1u, "TC-COV-006: FIPS_LOCK did not stick");
    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0x0u);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x1u, "TC-COV-006: FIPS_LOCK cleared by write of 0");

    for (const auto& r : locked) {
        uint32_t before = 0u;
        test->register_read_32(r.offset, before);
        const uint32_t attempt = before ^ (r.wmask & 0xFFu ? (r.wmask & 0xFFu) : 0x1u);
        test->register_write_32(r.offset, attempt);
        test->register_read_32(r.offset, rd_val);
        COV_CHECK(rd_val == before,
            "TC-COV-006: " << r.name << " changed while FIPS_LOCK set");
    }

    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    const uint32_t fifo_before = rd_val;
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, fifo_before ^ 0x11u);
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    const uint32_t fifo_expected =
        (fifo_before & ~0x1u) | ((fifo_before ^ 0x11u) & 0x1u);
    COV_CHECK(rd_val == fifo_expected,
        "TC-COV-006: FIFO_CTRL lock policy wrong (ENABLE writable, churn frozen)");

    apply_reset();
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u, "TC-COV-006: FIPS_LOCK not cleared by reset");

    test->register_write_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, 0x3u);
    test->register_read_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x3u,
        "TC-COV-006: ALERT_THRESHOLD still locked after reset unlock");

    return ok;
}

bool testbench::tc_cov_boot_phase_done_gate()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t BOOT_PHASE_DONE = (1u << 12);
    const uint32_t IDLE            = (1u << 9);

    // Truth table: BOOT_PHASE_DONE requires MODULE_ENABLE and nonzero RING_OSC.
    struct Row { uint32_t ctrl; uint32_t osc; bool done; bool expect_idle; const char* tag; };
    const Row rows[] = {
        {0x10000000u, 0x0u, false, true,  "mod_off_gen_off"},
        {0x10000000u, 0x1u, false, true,  "mod_off_gen_on"},
        {0x10000002u, 0x0u, false, false, "mod_on_gen_off"},
        {0x10000002u, 0x1u, true,  false, "mod_on_gen_on"},
        {0x10000002u, 0x0u, false, false, "mod_on_gen_off_clears"},
        {0x10000002u, 0xFu, true,  false, "mod_on_multi_gen"},
        {0x10000000u, 0xFu, false, true,  "mod_off_clears"},
    };

    for (const auto& row : rows) {
        test->register_write_32(entropy_src_basetest::CTRL_OFFSET, row.ctrl);
        test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, row.osc);
        wait(sc_core::SC_ZERO_TIME);
        test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
        COV_CHECK(((rd_val & BOOT_PHASE_DONE) != 0u) == row.done,
            "TC-COV-007: BOOT_PHASE_DONE mismatch " << row.tag
            << " ctrl=0x" << std::hex << row.ctrl << " osc=0x" << row.osc
            << " status=0x" << rd_val);
        if (row.expect_idle) {
            COV_CHECK((rd_val & IDLE) != 0u,
                "TC-COV-007: IDLE expected for " << row.tag);
        }
    }

    // Leave DONE asserted for the RO write check.
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0x10000002u);
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x1u);
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & BOOT_PHASE_DONE) != 0u,
        "TC-COV-007: MAIN_SM_STATUS RO write disturbed BOOT_PHASE_DONE");

    return ok;
}

bool testbench::tc_cov_irq_overflow_underflow()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t INTR_OVERFLOW  = (1u << 8);
    const uint32_t INTR_UNDERFLOW = (1u << 12);
    const uint32_t INTR_ALL       = 0x11111111u;

    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, INTR_ALL);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET, INTR_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    COV_CHECK(sig_intr.read(), "TC-COV-008: irq_o stayed low after inject");

    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, INTR_ALL);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x1u);
    bool overflowed = false;
    bool saw_wrap = false;
    uint8_t prev_wptr = 0xFF;
    for (int i = 0; i < 800; ++i) {
        wait(sc_core::sc_time(100.0, sc_core::SC_NS));
        test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
        const uint8_t wptr = static_cast<uint8_t>((rd_val >> 8) & 0x3Fu);
        if (prev_wptr != 0xFF && wptr < prev_wptr)
            saw_wrap = true;
        prev_wptr = wptr;
        test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
        if ((rd_val & INTR_OVERFLOW) != 0u) {
            overflowed = true;
            break;
        }
    }
    COV_CHECK(overflowed, "TC-COV-008: FIFO never overflowed");
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x7Fu) == 64u,
        "TC-COV-008: FIFO_STATUS.LEVEL not 64 after overflow");
    // Pointer wrap is observable once WPTR has advanced through 64 pushes.
    COV_CHECK(saw_wrap || ((rd_val >> 8) & 0x3Fu) == 0u,
        "TC-COV-008: WPTR did not wrap at 64 entries");

    for (unsigned i = 0; i < 64u; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }
    COV_CHECK(fifo_level(test) == 0u, "TC-COV-008: LEVEL not 0 after drain");
    rd_val = 0xFFFFFFFFu;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u, "TC-COV-008: underflow pop did not return 0");
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & INTR_UNDERFLOW) != 0u,
        "TC-COV-008: FIFO_UNDERFLOW not asserted");

    return ok;
}

bool testbench::tc_cov_reset_while_fifo_disabled()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 800u);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 16; ++i)
        wait(sc_core::SC_ZERO_TIME);

    apply_hw_reset();

    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) != 0u,
        "TC-COV-009: FIFO_CTRL.ENABLE not restored after hw reset");
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
    // Immediately after release LEVEL may still be 0 briefly.
    COV_CHECK(wait_fifo_level_ge(test, 1u, 256),
        "TC-COV-009: fill did not restart after reset from disabled");

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    apply_reset();
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) == 0u,
        "TC-COV-009: CTRL RSVD0 not zero after rst_ni");

    return ok;
}

// Compact raw-payload TLM matrix against regmodel::Memory contract.
bool testbench::tc_cov_verbose_callbacks_and_recovery()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    uint32_t neighbor_before = 0u;
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, neighbor_before);

    auto raw = [&](tlm::tlm_command cmd, sc_dt::uint64 addr,
                   unsigned char* data, unsigned len,
                   unsigned streaming, unsigned char* be, unsigned be_len,
                   sc_core::sc_time delay_in)
        -> std::pair<tlm::tlm_response_status, sc_core::sc_time> {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = delay_in;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(data);
        gp.set_data_length(len);
        gp.set_streaming_width(streaming);
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        test->initiator_socket->b_transport(gp, delay);
        return {gp.get_response_status(), delay};
    };

    // IGNORE → COMMAND_ERROR; neighbor unchanged.
    {
        uint32_t sink = 0xA5A5A5A5u;
        auto [st, d] = raw(tlm::TLM_IGNORE_COMMAND, entropy_src_basetest::CTRL_OFFSET,
                           reinterpret_cast<unsigned char*>(&sink), 4, 4,
                           nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_COMMAND_ERROR_RESPONSE,
            "TC-TLM: IGNORE expected COMMAND_ERROR");
        test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
        COV_CHECK(rd_val == neighbor_before,
            "TC-TLM: IGNORE mutated CTRL");
    }

    // Null data pointer → GENERIC_ERROR
    {
        auto [st, d] = raw(tlm::TLM_READ_COMMAND, entropy_src_basetest::CTRL_OFFSET,
                           nullptr, 4, 4, nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_GENERIC_ERROR_RESPONSE,
            "TC-TLM: null ptr expected GENERIC_ERROR");
    }

    // Length 0 → BURST_ERROR
    {
        uint32_t sink = 0;
        auto [st, d] = raw(tlm::TLM_READ_COMMAND, entropy_src_basetest::CTRL_OFFSET,
                           reinterpret_cast<unsigned char*>(&sink), 0, 0,
                           nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_BURST_ERROR_RESPONSE,
            "TC-TLM: len0 expected BURST_ERROR");
    }

    // Lengths 1/2/3/5/8 — Memory services in-window accesses with OK.
    {
        std::array<unsigned, 5> lens = {1, 2, 3, 5, 8};
        for (unsigned len : lens) {
            std::array<unsigned char, 8> buf{};
            auto [st, d] = raw(tlm::TLM_READ_COMMAND, entropy_src_basetest::CTRL_OFFSET,
                               buf.data(), len, len, nullptr, 0, sc_core::SC_ZERO_TIME);
            COV_CHECK(st == tlm::TLM_OK_RESPONSE,
                "TC-TLM: len " << len << " expected OK, got " << static_cast<int>(st));
        }
    }

    // Hole offset: OK + read zero; must not corrupt neighbor INTR_ENABLE.
    {
        uint32_t probe_before = 0u;
        test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x1111u);
        test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, probe_before);

        uint32_t hole = 0xDEADBEEFu;
        auto [st, d] = raw(tlm::TLM_WRITE_COMMAND, 0x58u,
                           reinterpret_cast<unsigned char*>(&hole), 4, 4,
                           nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_OK_RESPONSE, "TC-TLM: hole write expected OK");
        hole = 0xFFFFFFFFu;
        auto [st2, d2] = raw(tlm::TLM_READ_COMMAND, 0x58u,
                             reinterpret_cast<unsigned char*>(&hole), 4, 4,
                             nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st2 == tlm::TLM_OK_RESPONSE && hole == 0u,
            "TC-TLM: hole read expected 0");
        test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, rd_val);
        COV_CHECK(rd_val == probe_before,
            "TC-TLM: hole write corrupted neighboring INTR_ENABLE");
    }

    // Unaligned address inside a mapped register: OK (byte-lane path).
    {
        uint8_t b = 0;
        auto [st, d] = raw(tlm::TLM_READ_COMMAND,
                           entropy_src_basetest::CTRL_OFFSET + 1u, &b, 1, 1,
                           nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_OK_RESPONSE, "TC-TLM: unaligned byte read expected OK");
    }

    // streaming_width < len → BURST_ERROR; no mutate
    {
        uint32_t before = 0, data = 0x12345678u;
        test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, before);
        auto [st, d] = raw(tlm::TLM_WRITE_COMMAND,
                           entropy_src_basetest::DEBUG_CTRL_OFFSET,
                           reinterpret_cast<unsigned char*>(&data), 4, 2,
                           nullptr, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_BURST_ERROR_RESPONSE,
            "TC-TLM: streaming_width<len expected BURST_ERROR");
        test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, rd_val);
        COV_CHECK(rd_val == before, "TC-TLM: bad streaming mutated DEBUG_CTRL");
    }

    // byte-enable length 0 with non-null ptr → BYTE_ENABLE_ERROR
    {
        uint32_t data = 0x1u;
        unsigned char be = 0xF;
        auto [st, d] = raw(tlm::TLM_WRITE_COMMAND,
                           entropy_src_basetest::DEBUG_CTRL_OFFSET,
                           reinterpret_cast<unsigned char*>(&data), 4, 4,
                           &be, 0, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
            "TC-TLM: be_len=0 expected BYTE_ENABLE_ERROR");
    }

    // Valid byte enables (low 16 bits of DEBUG_CTRL)
    {
        uint32_t data = 0x000001FFu;
        unsigned char be[4] = {0xFF, 0xFF, 0x00, 0x00};
        auto [st, d] = raw(tlm::TLM_WRITE_COMMAND,
                           entropy_src_basetest::DEBUG_CTRL_OFFSET,
                           reinterpret_cast<unsigned char*>(&data), 4, 4,
                           be, 4, sc_core::SC_ZERO_TIME);
        COV_CHECK(st == tlm::TLM_OK_RESPONSE, "TC-TLM: byte-enable write expected OK");
        test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, rd_val);
        COV_CHECK((rd_val & 0x7FFu) == 0x1FFu,
            "TC-TLM: byte-enable write did not land");
    }

    // Nonzero incoming delay: Memory does not consume it (contract).
    {
        uint32_t data = 0;
        sc_core::sc_time din(25.0, sc_core::SC_NS);
        auto [st, dout] = raw(tlm::TLM_READ_COMMAND,
                              entropy_src_basetest::COMPONENT_ID_OFFSET,
                              reinterpret_cast<unsigned char*>(&data), 4, 4,
                              nullptr, 0, din);
        COV_CHECK(st == tlm::TLM_OK_RESPONSE, "TC-TLM: delay read expected OK");
        COV_CHECK(dout == din, "TC-TLM: delay was modified by b_transport");
    }

    // transport_dbg: same storage path; returns byte count.
    {
        uint32_t data = 0;
        tlm::tlm_generic_payload gp;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(entropy_src_basetest::COMPONENT_ID_OFFSET);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        const unsigned n = test->initiator_socket->transport_dbg(gp);
        COV_CHECK(n == 4u && gp.is_response_ok(),
            "TC-TLM: transport_dbg read failed");
        COV_CHECK(data == entropy_src_basetest::COMPONENT_ID_RESET,
            "TC-TLM: transport_dbg COMPONENT_ID mismatch");
    }

    // DMI: Memory does not register get_direct_mem_ptr → must refuse.
    {
        tlm::tlm_generic_payload gp;
        tlm::tlm_dmi dmi;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(entropy_src_basetest::CTRL_OFFSET);
        gp.set_data_length(4);
        const bool granted = test->initiator_socket->get_direct_mem_ptr(gp, dmi);
        COV_CHECK(!granted, "TC-TLM: DMI unexpectedly granted");
    }

    // Interrupt enable masking while status pending (ES-F-05 style).
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, 0x11111111u);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0u);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    COV_CHECK(!sig_intr.read(), "TC-TLM/IRQ: irq high with enable=0");
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET, 0x100u);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    COV_CHECK(!sig_intr.read(), "TC-TLM/IRQ: irq high after inject with enable=0");
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x100u);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    COV_CHECK(sig_intr.read(), "TC-TLM/IRQ: irq did not follow enable of pending");

    return ok;
}

// Export-path repetition check (get_seed_384) with REPETITION_LIMIT=1.
// Generation-thread health remains a no-op; this path evaluates the 48-byte
// seed after a destructive FIFO read — assert fail counters/IRQ, not RAND data.
bool testbench::tc_cov_export_repetition_fail()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    // ENABLE=0x01, REPETITION_LIMIT=1 → any consecutive equal bytes fail.
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000101u);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000101u,
        "TC-COV-011: HEALTH_TEST_CTRL program failed");

    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, 0x1u);  // W1C clear
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x1u);

    bool saw_fail = false;
    uint32_t fails_after = 0u;
    uint32_t intr_after = 0u;

    for (int attempt = 0; attempt < 80 && !saw_fail; ++attempt) {
        test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x1u);
        COV_CHECK(wait_fifo_level_ge(test, 12u, 800),
            "TC-COV-011: FIFO did not reach 12 words");
        if (!ok)
            return false;
        // Freeze generation so LEVEL is stable across the export pop.
        test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x0u);
        wait(sc_core::sc_time(50.0, sc_core::SC_US));

        uint8_t seed[48] = {};
        bool fips = true;
        const bool accepted = dut->entropy_export->get_seed_384(seed, fips);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);

        test->register_read_32(entropy_src_basetest::REPCNT_TOTAL_FAILS_OFFSET, fails_after);
        test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, intr_after);

        if (!accepted) {
            saw_fail = true;
            COV_CHECK(fails_after >= 1u,
                "TC-COV-011: get_seed rejected but REPCNT_TOTAL_FAILS still 0");
            COV_CHECK((intr_after & 0x1u) != 0u,
                "TC-COV-011: get_seed rejected but HEALTH_TEST_FAILED not set");
            COV_CHECK(!fips,
                "TC-COV-011: fips_compliant true on rejected seed");
            COV_CHECK(sig_intr.read(),
                "TC-COV-011: irq_o low after health-test fail with enable=1");
        } else {
            // Pass path still records max-run into REPETITION_TEST_COUNT.
            uint32_t rep = 0u;
            test->register_read_32(
                entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, rep);
            COV_CHECK(rep >= 1u && rep <= 48u,
                "TC-COV-011: REPETITION_TEST_COUNT out of range after pass");
            COV_CHECK(fips, "TC-COV-011: fips_compliant false on accepted seed");
        }
    }

    COV_CHECK(saw_fail,
        "TC-COV-011: never observed export repetition fail in 80 draws "
        "(REPETITION_LIMIT=1)");
    return ok;
}
