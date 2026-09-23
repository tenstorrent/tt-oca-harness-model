// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/******************************************************************************
 * @file test_coverage.cpp
 * @brief Targeted coverage tests for entropy_src.cpp thread paths
 ******************************************************************************/

#include "testbench.h"

#define COV_CHECK(cond, msg_stream)           \
    do {                                      \
        if (!(cond)) {                        \
            REG_ERROR(0, logger) << msg_stream; \
            ok = false;                       \
        }                                     \
    } while (false)

// Coverage: entropy_generation_thread boot rst_n gate + initial STARTUP_DELAY
// (entropy_src.cpp ~890-910).  Requires testbench to hold rst_ni low until
// this test runs (see testbench constructor).
bool testbench::tc_cov_boot_rst_n_with_startup_delay()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    const uint32_t delay_ns = 500u;

    for (int i = 0; i < 8; ++i)
        wait(sc_core::SC_ZERO_TIME);

    dut->m_startup_delay_ns = delay_ns;
    wait(sc_core::SC_ZERO_TIME);

    sig_rst_n.write(true);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);

    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 2.0, sc_core::SC_NS));

    bool fill_started = false;
    for (int i = 0; i < 256; ++i) {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0u) {
            fill_started = true;
            break;
        }
    }

    COV_CHECK(fill_started,
        "TC-COV-001: FIFO did not fill after boot rst_n release + STARTUP_DELAY");

    return ok;
}

// Coverage: STARTUP_DELAY after FIFO re-enable (~987-1007)
bool testbench::tc_cov_fifo_reenable_startup_delay()
{
    bool ok = true;
    uint32_t rd_val = 0u;
    const uint32_t delay_ns = 1200u;

    dut->m_startup_delay_ns = delay_ns;
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u)
            break;
    }

    // Finish any leftover quantum so the thread parks in WAITING_FOR_ENABLE.
    wait(sc_core::sc_time(200.0, sc_core::SC_US));

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    for (int i = 0; i < 8; ++i) {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        COV_CHECK(rd_val == 0u,
            "TC-COV-002: FIFO_RDATA should be 0 during post-reenable STARTUP_DELAY");
    }

    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 2.0, sc_core::SC_NS));

    bool fill_started = false;
    for (int i = 0; i < 256; ++i) {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0u) {
            fill_started = true;
            break;
        }
    }

    COV_CHECK(fill_started,
        "TC-COV-002: FIFO did not fill after re-enable STARTUP_DELAY expired");

    dut->m_startup_delay_ns = 0x00000000u;
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// Coverage: software reset during post-reenable STARTUP_DELAY (~998-1001)
bool testbench::tc_cov_sw_reset_during_reenable_startup_delay()
{
    const uint32_t delay_ns = 2000u;

    dut->m_startup_delay_ns = delay_ns;
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::sc_time(200.0, sc_core::SC_US));

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    // Interrupt the post-reenable STARTUP_DELAY the way CTRL.RESET used to:
    // set the software-reset flag and wake the quantum wait.
    dut->m_reset_in_progress = true;
    dut->m_reset_complete_event.notify(sc_core::sc_time(1.0, sc_core::SC_NS));
    dut->m_reset_event.notify(sc_core::SC_ZERO_TIME);
    wait(sc_core::sc_time(10.0, sc_core::SC_NS));

    dut->m_startup_delay_ns = 0x00000000u;
    wait(sc_core::SC_ZERO_TIME);

    return true;
}

// Coverage: hardware reset mid-fill re-derive path (~922-946)
bool testbench::tc_cov_hw_reset_rederive_state()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    dut->m_startup_delay_ns = 300u;
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    for (int i = 0; i < 64; ++i) {
        wait(sc_core::SC_ZERO_TIME);
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0u)
            break;
    }

    apply_hw_reset();

    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::CTRL_RESET,
        "TC-COV-004: CTRL not at reset after hw reset re-derive");

    COV_CHECK(dut->m_startup_delay_ns == 0u,
        "TC-COV-004: startup delay not cleared after hw reset");

    return ok;
}

// Coverage: exercise new RDL register read/write paths via TLM
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

    return ok;
}

// Coverage: FIPS_LOCK (0x154) is write-one-to-set, not plain RW.
//
// The lock is one-way in hardware: firmware applies it after the startup health
// test passes and reads it back to confirm it stuck. A later write of 0 -- a
// full-word rewrite of the register file, say -- must not drop it. Only a reset
// clears it.
bool testbench::tc_cov_fips_lock_w1s()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::FIPS_LOCK_RESET,
        "TC-COV-006: FIPS_LOCK not clear after reset");

    test->register_read_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::ALERT_THRESHOLD_RESET,
        "TC-COV-006: ALERT_THRESHOLD not at reset");
    test->register_write_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, 0x00000010u);
    test->register_read_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000010u,
        "TC-COV-006: ALERT_THRESHOLD write ignored before lock");

    test->register_write_32(entropy_src_basetest::MIN_ENTROPY_H_OFFSET, 0x00000020u);
    test->register_read_32(entropy_src_basetest::MIN_ENTROPY_H_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000020u,
        "TC-COV-006: MIN_ENTROPY_H write ignored before lock");

    test->register_write_32(entropy_src_basetest::BIW_OBS_CTRL_OFFSET, 0x1u);
    test->register_read_32(entropy_src_basetest::BIW_OBS_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x1u, "TC-COV-006: BIW_OBS_CTRL write failed");
    test->register_write_32(entropy_src_basetest::NOISE_OBS_CTRL_OFFSET, 0x000000F3u);
    test->register_read_32(entropy_src_basetest::NOISE_OBS_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x000000F1u, "TC-COV-006: NOISE_OBS_CTRL FLUSH not self-clearing on read");
    test->register_read_32(entropy_src_basetest::BIW_OBS_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u, "TC-COV-006: BIW_OBS_STATUS not empty");
    test->register_read_32(entropy_src_basetest::NOISE_OBS_RDATA_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u, "TC-COV-006: NOISE_OBS_RDATA not empty");

    // Writing 0 to an already-clear lock leaves it clear.
    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000000u,
        "TC-COV-006: FIPS_LOCK set by a write of 0");

    // Writing 1 sets it, and the read-back firmware relies on sees it.
    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0x00000001u);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000001u,
        "TC-COV-006: FIPS_LOCK did not stick when written to 1");

    // W1S: a subsequent write of 0 must NOT clear it.
    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000001u,
        "TC-COV-006: FIPS_LOCK cleared by a write of 0 (W1S violated)");

    // Reserved bits [31:1] stay masked off, and the lock survives the attempt.
    test->register_write_32(entropy_src_basetest::FIPS_LOCK_OFFSET, 0xFFFFFFFEu);
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000001u,
        "TC-COV-006: FIPS_LOCK reserved bits not masked");

    const uint32_t ctrl_before = entropy_src_basetest::CTRL_RESET;
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == ctrl_before,
        "TC-COV-006: CTRL changed while FIPS_LOCK set");
    test->register_write_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, 0x00000001u);
    test->register_read_32(entropy_src_basetest::ALERT_THRESHOLD_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000010u,
        "TC-COV-006: ALERT_THRESHOLD changed while FIPS_LOCK set");
    test->register_write_32(entropy_src_basetest::MIN_ENTROPY_H_OFFSET, 0x00000001u);
    test->register_read_32(entropy_src_basetest::MIN_ENTROPY_H_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x00000020u,
        "TC-COV-006: MIN_ENTROPY_H changed while FIPS_LOCK set");

    test->register_read_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, rd_val);
    const uint32_t window_before = rd_val;
    const uint32_t window_attempt = (window_before == 0x1u) ? 0x2u : 0x1u;
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, window_attempt);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, rd_val);
    COV_CHECK(rd_val == window_before,
        "TC-COV-006: HEALTH_TEST_WINDOW_SIZE changed while FIPS_LOCK set");

    test->register_read_32(entropy_src_basetest::APT_PROPORTION_LO_OFFSET, rd_val);
    const uint32_t lo_before = rd_val;
    const uint32_t lo_attempt = (lo_before == 0x350u) ? 0x100u : 0x350u;
    test->register_write_32(entropy_src_basetest::APT_PROPORTION_LO_OFFSET, lo_attempt);
    test->register_read_32(entropy_src_basetest::APT_PROPORTION_LO_OFFSET, rd_val);
    COV_CHECK(rd_val == lo_before,
        "TC-COV-006: APT_PROPORTION_LO changed while FIPS_LOCK set");

    test->register_read_32(entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, rd_val);
    const uint32_t clk_before = rd_val;
    const uint32_t clk_attempt = (clk_before == 0u) ? 0x1Fu : 0u;
    test->register_write_32(entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, clk_attempt);
    test->register_read_32(entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET, rd_val);
    COV_CHECK(rd_val == clk_before,
        "TC-COV-006: GENERATOR_0_SAMPLE_CLK_CONFIG changed while FIPS_LOCK set");

    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    const uint32_t fifo_before = rd_val;
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, fifo_before ^ 0x11u);
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    const uint32_t fifo_expected =
        (fifo_before & ~0x1u) | ((fifo_before ^ 0x11u) & 0x1u);
    COV_CHECK(rd_val == fifo_expected,
        "TC-COV-006: FIFO_CTRL lock did not keep ENABLE writable and churn frozen");

    // Health status is woclr[7:0]: a 1 clears, a 0 leaves the bit, and software cannot set bits.
    dut->HEALTH_TEST_STATUS = 0x21u;
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0x01u);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x20u,
        "TC-COV-006: HEALTH_TEST_STATUS W1C did not clear only bit 0");
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0x00u);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0x20u,
        "TC-COV-006: HEALTH_TEST_STATUS write of 0 cleared a sticky bit");
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0xFFu);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-006: HEALTH_TEST_STATUS W1C left bits set");

    dut->GENERATOR_0_HEALTH_STATUS = 0x08u;
    test->register_write_32(entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET, 0xFFu);
    test->register_read_32(entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-006: GENERATOR_0_HEALTH_STATUS is not W1C");
    test->register_write_32(entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET, 0xFFu);
    test->register_read_32(entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-006: software set GENERATOR_0_HEALTH_STATUS");

    // Only a reset returns it to 0.
    apply_reset();
    test->register_read_32(entropy_src_basetest::FIPS_LOCK_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::FIPS_LOCK_RESET,
        "TC-COV-006: FIPS_LOCK not cleared by reset");

    return ok;
}

// Coverage: MAIN_SM_STATUS.BOOT_PHASE_DONE (0xB4 bit 12) tracks the
// RING_OSC_ENABLE write, and MAIN_SM_STATUS itself is read-only.
//
// The gate is on the generators alone -- see handle_write_RING_OSC_ENABLE --
// so enabling a single generator is enough to assert it.
bool testbench::tc_cov_boot_phase_done_gate()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    const uint32_t BOOT_PHASE_DONE = (1u << 12);
    const uint32_t IDLE            = (1u << 9);

    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::MAIN_SM_STATUS_RESET,
        "TC-COV-007: MAIN_SM_STATUS not at reset default");

    // Step 1 of the firmware sequence: configure with the generators off.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & BOOT_PHASE_DONE) == 0u,
        "TC-COV-007: BOOT_PHASE_DONE asserted with all generators off");

    // Step 2: enable one generator -- the startup window is modelled as passed.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & BOOT_PHASE_DONE) != 0u,
        "TC-COV-007: BOOT_PHASE_DONE not asserted after generator enable");
    COV_CHECK((rd_val & IDLE) == 0u,
        "TC-COV-007: IDLE still set after generator enable");

    // MAIN_SM_STATUS is RO: a write must not disturb it.
    test->register_write_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & BOOT_PHASE_DONE) != 0u && (rd_val & ~BOOT_PHASE_DONE) == 0u,
        "TC-COV-007: MAIN_SM_STATUS RO enforcement failed");

    // Clearing MODULE_ENABLE holds the main SM idle and drops BOOT_PHASE_DONE.
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0x10000000u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::MAIN_SM_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & BOOT_PHASE_DONE) == 0u,
        "TC-COV-007: BOOT_PHASE_DONE stuck after MODULE_ENABLE clear");
    COV_CHECK((rd_val & IDLE) != 0u,
        "TC-COV-007: IDLE not set after MODULE_ENABLE clear");

    return ok;
}

// Coverage: irq_o assertion, FIFO overflow once-full, and FIFO underflow.
bool testbench::tc_cov_irq_overflow_underflow()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    const uint32_t INTR_OVERFLOW  = (1u << 8);
    const uint32_t INTR_UNDERFLOW = (1u << 12);
    const uint32_t INTR_NEW = (1u << 16) | (1u << 20) | (1u << 24) | (1u << 28);
    const uint32_t INTR_ALL       = 0x11111111u;

    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, INTR_ALL);
    test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, rd_val);
    COV_CHECK(rd_val == INTR_ALL,
        "TC-COV-008: INTR_ENABLE did not accept the eight RDL sources");

    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET,
                            INTR_UNDERFLOW | INTR_NEW);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);

    COV_CHECK(sig_intr.read(),
        "TC-COV-008: irq_o stayed low after INTR_TEST + INTR_ENABLE");

    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & INTR_UNDERFLOW) != 0u,
        "TC-COV-008: INTR_TEST did not set FIFO_UNDERFLOW");
    COV_CHECK((rd_val & INTR_NEW) == INTR_NEW,
        "TC-COV-008: INTR_TEST did not set persistent/autotune/observer bits");

    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, INTR_ALL);
    wait(sc_core::SC_ZERO_TIME);
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u,
        "TC-COV-008: INTR_STATUS W1C did not clear injected bits");

    // Let the background thread fill to FIFO_DEPTH so overflow is asserted.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    bool overflowed = false;
    bool saw_wide_wptr = false;
    for (int i = 0; i < 512; ++i) {
        wait(sc_core::sc_time(100.0, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
        if (((rd_val >> 8) & 0x3Fu) >= 32u)
            saw_wide_wptr = true;
        test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
        if ((rd_val & INTR_OVERFLOW) != 0u) {
            overflowed = true;
            break;
        }
    }
    COV_CHECK(overflowed, "TC-COV-008: FIFO never overflowed");
    COV_CHECK(saw_wide_wptr,
        "TC-COV-008: WPTR stayed inside 5 bits; RDL WPTR is [13:8]");

    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x7Fu) == 64u,
        "TC-COV-008: FIFO_STATUS.LEVEL not 64 after overflow");

    // Drain every word, then one extra read for underflow.
    for (unsigned i = 0; i < 64u; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }
    rd_val = 0xFFFFFFFFu;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    COV_CHECK(rd_val == 0u, "TC-COV-008: underflow pop did not return 0");
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
    COV_CHECK((rd_val & INTR_UNDERFLOW) != 0u,
        "TC-COV-008: FIFO_UNDERFLOW not asserted after empty pop");

    return ok;
}

// Coverage: hw/sw reset while the generation thread is in WAITING_FOR_ENABLE.
bool testbench::tc_cov_reset_while_fifo_disabled()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    dut->m_startup_delay_ns = 800u;
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 16; ++i)
        wait(sc_core::SC_ZERO_TIME);

    apply_hw_reset();

    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) != 0u,
        "TC-COV-009: FIFO_CTRL.ENABLE not restored after hw reset");

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    dut->m_startup_delay_ns = 600u;
    for (int i = 0; i < 16; ++i)
        wait(sc_core::SC_ZERO_TIME);

    apply_reset();

    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) == 0u,
        "TC-COV-009: CTRL bit 0 (RSVD0) not zero after rst_ni from IDLE");

    return ok;
}

// Coverage: re-hit CSML_INFO callback bodies and handle_reset_recovery drain
// + post-reset STARTUP_DELAY (the SW-reset callback empties the FIFO first,
// so recovery's drain/delay arms need a direct poke).
bool testbench::tc_cov_verbose_callbacks_and_recovery()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    dut->logger.setMaxVerbosity(3);

    // CTRL no-reset path (downsample / bypass only).
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0x00010100u);
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, rd_val);
    COV_CHECK((rd_val & 0x1u) == 0u,
        "TC-COV-010: CTRL no-reset write set RESET");

    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000001u);
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000000u);
    dut->m_startup_delay_ns = 250u;
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x11111111u);
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_write_32(entropy_src_basetest::INTR_STATUS_OFFSET, 0x00000001u);

    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000003u);
    wait(sc_core::SC_ZERO_TIME);
    // Second enable write: BOOT_PHASE_DONE already set, skip the assert arm.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000003u);
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    // Wait for at least one entropy word so FIFO_RDATA pop logs fire.
    bool got_word = false;
    for (int i = 0; i < 128; ++i) {
        wait(sc_core::sc_time(100.0, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0u) {
            got_word = true;
            break;
        }
    }
    COV_CHECK(got_word, "TC-COV-010: FIFO never produced a word");

    // RUNNING-path RESET_PENDING (top-of-loop / post-pacing checks).
    dut->m_reset_in_progress = true;
    dut->m_reset_complete_event.notify(sc_core::sc_time(150.0, sc_core::SC_NS));
    wait(sc_core::sc_time(200.0, sc_core::SC_NS));

    // Park the generation thread so it cannot refill during recovery.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::sc_time(200.0, sc_core::SC_US));

    // In-thread RESET_PENDING: wake WAITING_FOR_ENABLE via m_reset_event.
    dut->m_reset_in_progress = true;
    dut->m_reset_complete_event.notify(sc_core::sc_time(1.0, sc_core::SC_NS));
    dut->m_reset_event.notify(sc_core::SC_ZERO_TIME);
    wait(sc_core::sc_time(10.0, sc_core::SC_NS));

    // qk_sync_interruptible early-return when the keeper has no local time.
    dut->m_qk.reset();
    dut->qk_sync_interruptible();

    // Direct recovery helper: leftover FIFO words + non-zero startup delay.
    dut->m_fifo.push(0xA5A5A5A5u);
    dut->m_fifo.push(0x5A5A5A5Au);
    dut->m_startup_delay_ns = 2000u;
    dut->m_reset_in_progress = true;
    sc_core::sc_spawn([&]() {
        wait(sc_core::SC_ZERO_TIME);
        dut->m_reset_complete_event.notify(sc_core::SC_ZERO_TIME);
    });
    dut->handle_reset_recovery();
    COV_CHECK(dut->m_fifo.empty(),
        "TC-COV-010: handle_reset_recovery left leftover FIFO words");
    COV_CHECK(!dut->m_reset_in_progress,
        "TC-COV-010: handle_reset_recovery left m_reset_in_progress set");

    return ok;
}
