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

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
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

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u)
            break;
    }

    for (int i = 0; i < 32; ++i)
        wait(sc_core::SC_ZERO_TIME);

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

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// Coverage: software reset during post-reenable STARTUP_DELAY (~998-1001)
bool testbench::tc_cov_sw_reset_during_reenable_startup_delay()
{
    const uint32_t delay_ns = 2000u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i)
        wait(sc_core::SC_ZERO_TIME);

    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    // Reset while thread is still in STARTUP_DELAY (no timed wait elapsed).
    apply_reset();

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return true;
}

// Coverage: hardware reset mid-fill re-derive path (~922-946)
bool testbench::tc_cov_hw_reset_rederive_state()
{
    bool ok = true;
    uint32_t rd_val = 0u;

    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 300u);
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

    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    COV_CHECK(rd_val == entropy_src_basetest::STARTUP_CTRL_RESET,
        "TC-COV-004: STARTUP_CTRL not cleared after hw reset");

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

    return ok;
}
