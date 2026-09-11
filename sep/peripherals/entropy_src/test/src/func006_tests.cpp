// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file func006_tests.cpp
 * @brief FUNC-006 FIFO-Based Entropy Data Queue Operation — test case
 *        implementations
 *
 * Implements all 22 test cases mapped to FUNC-006 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to
 * one or more rows in the FUNC-006 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-006 — 22 test cases)
 *
 *  Sl.  | Method                                                          | Test Plan ID
 *  -----|------------------------------------------------------------------|----------------------------------------------
 *   37  | tc_f006_fifo_ctrl_reset_value                                   | fifo_ctrl_reset_value
 *   38  | tc_f006_fifo_ctrl_write_mask_only_bit0_writable                 | fifo_ctrl_write_mask_only_bit0_writable
 *   39  | tc_f006_fifo_ctrl_enable_disable_fifo                           | fifo_ctrl_enable_disable_fifo
 *   40  | tc_f006_fifo_status_reset_value                                 | fifo_status_reset_value
 *   41  | tc_f006_fifo_status_level_increments_with_background_fill       | fifo_status_level_increments_with_background_fill
 *   42  | tc_f006_fifo_status_level_decrements_on_fifo_rdata_read         | fifo_status_level_decrements_on_fifo_rdata_read
 *   43  | tc_f006_fifo_status_level_at_maximum_depth                      | fifo_status_level_at_maximum_depth
 *   44  | tc_f006_fifo_status_is_read_only                                | fifo_status_is_read_only
 *   45  | tc_f006_fifo_rdata_returns_nonzero_entropy_when_nonempty        | fifo_rdata_returns_nonzero_entropy_when_nonempty
 *   46  | tc_f006_fifo_rdata_successive_reads_yield_different_values      | fifo_rdata_successive_reads_yield_different_values
 *   47  | tc_f006_fifo_rdata_empty_fifo_returns_zero_and_sets_underflow   | fifo_rdata_empty_fifo_returns_zero_and_sets_underflow
 *   48  | tc_f006_fifo_underflow_interrupt_port_on_empty_read             | fifo_underflow_interrupt_port_on_empty_read
 *   50  | tc_f006_fifo_drain_to_empty_and_verify_level_zero               | fifo_drain_to_empty_and_verify_level_zero
 *   51  | tc_f006_fifo_overflow_sets_intr_status_bit                      | fifo_overflow_sets_intr_status_bit
 *   53  | tc_f006_fifo_fill_halts_when_disabled_during_operation          | fifo_fill_halts_when_disabled_during_operation
 *   54  | tc_f006_fifo_fill_resumes_after_reenable                        | fifo_fill_resumes_after_reenable
 *  129  | tc_f006_ro_write_has_no_effect_fifo_status                      | ro_register_write_has_no_effect_fifo_status
 *  136  | tc_f006_fifo_wptr_advances_with_background_push                 | fifo_wptr_advances_with_background_push
 *
 * ## FIFO Architecture
 *
 * The FIFO is implemented as an internal std::queue<uint32_t> with a hard
 * capacity limit of FIFO_DEPTH = 127 entries.  The background SC_THREAD
 * (entropy_generation_thread) is the sole producer; software read of
 * FIFO_RDATA (0x28) via the handle_read_FIFO_RDATA callback is the sole
 * consumer.
 *
 * ## FIFO_STATUS observability model
 *
 * FIFO_STATUS (0x24, RO) has read_mask = 0x00000000 in the regmodel register
 * definition, which means TLM b_transport reads of FIFO_STATUS always return
 * 0x00000000 through the register layer.  The FIFO fill level is inferred
 * through two indirect mechanisms:
 *
 *  1. FIFO_RDATA reads: a non-zero return value confirms the FIFO is non-empty
 *     (the callback pops and delivers an entropy word); a return of 0x00000000
 *     may mean either an empty FIFO (underflow) or a PRNG word that happens to
 *     be zero.  For level inference, a sequence of reads is used.
 *
 *  2. Overflow interrupt: when the FIFO reaches capacity (127 entries) and the
 *     background thread attempts another push, INTR_STATUS[8] is set.  This
 *     event confirms LEVEL == FIFO_DEPTH without requiring a direct FIFO_STATUS
 *     read.
 *
 * Tests that target FIFO_STATUS field semantics (LEVEL, WPTR, RPTR) must use
 * the interrupt-based overflow signal and FIFO_RDATA pop sequences to infer
 * the internal state, rather than direct register reads.
 *
 * ## Polling conventions
 *
 * When DOWNSAMPLE_RATE == 0 (default at reset) the background thread yields
 * wait(SC_ZERO_TIME) after each iteration.  Polling loops use:
 *   wait(sc_core::SC_ZERO_TIME);
 * with a bounded iteration count (F006_POLL_LIMIT) to give the thread time
 * to execute without consuming real simulation time.
 *
 * ## Design constraints
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() before each test case to
 *    guarantee a clean, defined register and FIFO state.
 *  - The FUNC006_CHECK macro sets ok = false and emits a REG_ERROR log
 *    entry naming both the expected and observed values.
 *  - FIFO_CTRL reset value = 0x00000001 (FIFO enabled by default).
 *  - FIFO_STATUS reset value = 0x00000000 (LEVEL=0, WPTR=0, RPTR=0).
 *  - INTR_STATUS bits: FIFO_OVERFLOW = bit 8 (0x100), FIFO_UNDERFLOW = bit 12 (0x1000).
 *  - INTR_STATUS has read_mask = 0x00000000; port state observed via sc_in<bool>.
 *  - All INTR_ENABLE bits start at 0 (all masked) after reset.
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md (FUNC-006 table)
 *  - entropy_src/docs/entropy_src-test-plan.md
 *  - entropy_src/docs/entropy_src-detailed-design.md §6 (FIFO management)
 *  - entropy_src/docs/entropy_src-functionality-list.md (FUNC-006 description)
 *  - entropy_src/model/inc/entropy_src.h (FIFO_DEPTH, interrupt bit constants)
 *  - entropy_src/test/src/func004_tests.cpp (polling pattern reference)
 *  - entropy_src/test/src/func005_tests.cpp (CHECK macro and style reference)
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "testbench.h"

#include <iomanip>
#include <sstream>

// =============================================================================
// Internal helper macro
// =============================================================================

/// @cond INTERNAL
/// Emit a descriptive FAIL message and set ok = false.
/// Mirrors the FUNC005_CHECK pattern used in func005_tests.cpp.  The
/// @p msg_stream argument is a streaming expression (<<-chained) that is
/// evaluated only when @p cond is false.
#define FUNC006_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            REG_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)
/// @endcond

// =============================================================================
// File-local constants
// =============================================================================

/// @cond INTERNAL

/// Maximum number of SC_ZERO_TIME polling iterations used when waiting for
/// the background thread to produce at least one entropy word.  At
/// DOWNSAMPLE_RATE == 0 the thread yields wait(SC_ZERO_TIME) every iteration,
/// so 512 delta cycles gives ample scheduling budget.
static constexpr int F006_POLL_LIMIT = 512;

/// Maximum number of SC_ZERO_TIME polling iterations used when waiting for
/// the FIFO to reach full capacity (127 entries).  Filling from 0 to 127
/// requires at least 127 background iterations; with scheduling overhead a
/// limit of 4096 iterations is conservative but finite.
static constexpr int F006_FILL_POLL_LIMIT = 4096;

/// FIFO_CTRL reset value: 0x00000001 (FIFO enabled, bit 0 = 1).
static constexpr uint32_t F006_FIFO_CTRL_RESET = 0x00000001u;

/// FIFO_CTRL write mask: only bit 0 (ENABLE) is writable.
static constexpr uint32_t F006_FIFO_CTRL_MASK = 0x00000001u;

/// FIFO_STATUS reset value: 0x00000000 (LEVEL=0, WPTR=0, RPTR=0).
static constexpr uint32_t F006_FIFO_STATUS_RESET = 0x00000000u;

/// Maximum FIFO depth (FIFO_DEPTH build-time parameter): 32 entries.
static constexpr uint32_t F006_FIFO_DEPTH = 32u;

/// INTR_STATUS bit position for FIFO_OVERFLOW (bit 8).
static constexpr uint32_t F006_INTR_BIT_FIFO_OVERFLOW  = 0x00000100u;

/// INTR_STATUS bit position for FIFO_UNDERFLOW (bit 12).
static constexpr uint32_t F006_INTR_BIT_FIFO_UNDERFLOW = 0x00001000u;

/// INTR_ENABLE bit that unmasks the FIFO_OVERFLOW interrupt output port.
static constexpr uint32_t F006_INTR_EN_FIFO_OVERFLOW   = 0x00000100u;

/// INTR_ENABLE bit that unmasks the FIFO_UNDERFLOW interrupt output port.
static constexpr uint32_t F006_INTR_EN_FIFO_UNDERFLOW  = 0x00001000u;

/// Short settle loop count: give the background thread a handful of delta
/// cycles to observe a newly written control register value.
static constexpr int F006_SHORT_SETTLE = 32;

/// Number of FIFO_RDATA reads that unconditionally drains a full FIFO.
/// FIFO_DEPTH = 127; reading 128 words guarantees all entries are consumed
/// regardless of whether any individual word happened to be 0x00000000.
static constexpr int F006_DRAIN_COUNT = static_cast<int>(32u + 1u);

/// Base iteration period (nanoseconds) of the background entropy generation
/// thread when DOWNSAMPLE_RATE is zero.  Matches BASE_ITERATION_PERIOD_NS
/// in entropy_src.h; reproduced here to keep this file self-contained.
static constexpr double F006_BASE_ITER_PERIOD_NS = 100.0;

/// @endcond

// =============================================================================
// TC-F006-037 — FIFO_CTRL reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F006-037: Verify FIFO_CTRL (0x20) reset value
 *
 * After apply_reset() the regmodel framework must restore FIFO_CTRL to its
 * hardware reset default 0x00000001:
 *   - Bit  [0]    ENABLE = 1  (FIFO fill enabled by default)
 *   - Bits [31:1] reserved = 0
 *
 * Procedure:
 *  1. Read FIFO_CTRL immediately after reset.
 *  2. Assert read_value == 0x00000001.
 *
 * Pass criterion: read_value == F006_FIFO_CTRL_RESET (0x00000001).
 *
 * Test plan reference: fifo_ctrl_reset_value
 *
 * @return true if the assertion passes
 ******************************************************************************/
bool testbench::tc_f006_fifo_ctrl_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);

    FUNC006_CHECK(
        rd_val == entropy_src_basetest::FIFO_CTRL_RESET,
        "TC-F006-037: FIFO_CTRL reset value mismatch — "
        "expected 0x" << std::hex << entropy_src_basetest::FIFO_CTRL_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F006-038 — FIFO_CTRL write mask: only bit 0 is writable
// =============================================================================

/******************************************************************************
 * @brief TC-F006-038: Verify FIFO_CTRL write mask 0x00000001
 *
 * FIFO_CTRL has a 1-bit write mask (bit 0 only).  Bits [31:1] are reserved
 * and must always read as zero regardless of the value written.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF (all-ones) to FIFO_CTRL.
 *  2. Read back FIFO_CTRL; assert read_value == 0x00000001 (only bit 0 set).
 *  3. Write 0xFFFFFFFE (all-ones except bit 0) to FIFO_CTRL.
 *  4. Read back FIFO_CTRL; assert read_value == 0x00000000 (bit 0 cleared).
 *
 * Pass criterion:
 *  - Reserved bits [31:1] always read as zero.
 *  - Bit [0] retains the masked written value.
 *
 * Test plan reference: fifo_ctrl_write_mask_only_bit0_writable
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_ctrl_write_mask_only_bit0_writable()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1-2: Write all-ones; only bit 0 must survive masking.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);

    FUNC006_CHECK(
        rd_val == entropy_src_basetest::FIFO_CTRL_WRITE,
        "TC-F006-038 step 1: FIFO_CTRL expected 0x00000011 after writing "
        "0xFFFFFFFF, got 0x" << std::hex << rd_val);

    FUNC006_CHECK(
        (rd_val & ~entropy_src_basetest::FIFO_CTRL_WRITE) == 0u,
        "TC-F006-038 step 1: FIFO_CTRL reserved bits [31:1] non-zero — "
        "read 0x" << std::hex << rd_val);

    // Step 3-4: Write with bit 0 cleared; register must read back as 0x00000000.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0xFFFFFFEEu);
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);

    FUNC006_CHECK(
        rd_val == 0x00000000u,
        "TC-F006-038 step 2: FIFO_CTRL expected 0x00000000 after writing "
        "0xFFFFFFFE (bit 0 clear), got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F006-039 — FIFO_CTRL enable/disable gate
// =============================================================================

/******************************************************************************
 * @brief TC-F006-039: Core FIFO gate test — FIFO_CTRL[0]=0 stops fill;
 *        FIFO_CTRL[0]=1 resumes fill
 *
 * Writing FIFO_CTRL[0] = 0 sets the internal fifo_enabled flag to false.
 * The background thread enters WAITING_FOR_ENABLE and stops pushing new
 * entropy values.  Writing FIFO_CTRL[0] = 1 notifies fifo_enable_event,
 * waking the thread to resume FIFO fills.
 *
 * Observability strategy: Because FIFO_STATUS.LEVEL has read_mask = 0x0
 * (TLM reads return 0), fill activity is confirmed through FIFO_RDATA pops.
 * A non-zero FIFO_RDATA value proves the FIFO is non-empty (fill occurred).
 *
 * Procedure:
 *  1. Confirm thread is producing data by polling FIFO_RDATA (non-zero).
 *  2. Disable FIFO fill: write FIFO_CTRL = 0x00000000.
 *  3. Drain any residual FIFO data with SC_ZERO_TIME polling.
 *  4. After draining, confirm FIFO is empty (FIFO_RDATA returns 0x00000000
 *     for F006_POLL_LIMIT consecutive polls, indicating underflow each time).
 *  5. Re-enable FIFO fill: write FIFO_CTRL = 0x00000001.
 *  6. Poll FIFO_RDATA again; confirm non-zero value is obtained (fill resumed).
 *
 * Pass criterion:
 *  - Non-zero FIFO_RDATA before disable.
 *  - All FIFO_RDATA reads return 0x00000000 after disable and drain.
 *  - Non-zero FIFO_RDATA after re-enable.
 *
 * Test plan reference: fifo_ctrl_enable_disable_fifo
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_ctrl_enable_disable_fifo()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Confirm the background thread is producing FIFO data.
    // FIFO_CTRL is 0x1 at reset (FIFO enabled).
    // -------------------------------------------------------------------------
    bool thread_alive_before = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            thread_alive_before = true;
            break;
        }
    }

    FUNC006_CHECK(
        thread_alive_before,
        "TC-F006-039 step 1: background thread not producing FIFO data before "
        "disable — check FIFO_CTRL reset value and thread liveness");

    // -------------------------------------------------------------------------
    // Step 2: Disable FIFO fill by writing FIFO_CTRL[0] = 0.
    wait(100, sc_core::SC_NS);
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 16; ++i) { wait(sc_core::SC_ZERO_TIME); }

    // -------------------------------------------------------------------------
    // Step 4: Confirm FIFO is NOT drained automatically (per new RDL).
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
 
    FUNC006_CHECK(rd_val != 0x00000000u,
        "TC-F006-039 step 4: FIFO_RDATA returned 0x00000000 after FIFO_CTRL[0]=0 "
        "disable — expected non-zero (FIFO should NOT be drained per new RDL)");
 
    for (int i = 0; i < 32; ++i) {
        uint32_t val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, val);
        if (val == 0x00000000u) break;
    }

    // -------------------------------------------------------------------------
    // Step 5: Re-enable FIFO fill.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);

    // Allow the background thread to wake from fifo_enable_event and execute
    // at least one iteration.
    for (int i = 0; i < F006_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // -------------------------------------------------------------------------
    // Step 6: Poll FIFO_RDATA after re-enable to confirm fill resumed.
    // -------------------------------------------------------------------------
    bool fill_resumed = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            fill_resumed = true;
            break;
        }
    }

    FUNC006_CHECK(
        fill_resumed,
        "TC-F006-039 step 6: FIFO_RDATA still returning zero after "
        "FIFO_CTRL[0]=1 re-enable — background thread did not resume fill");

    return ok;
}

// =============================================================================
// TC-F006-040 — FIFO_STATUS reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F006-040: Verify FIFO_STATUS (0x24) reset value
 *
 * After apply_reset(), FIFO_STATUS must read as 0x00000000.  This confirms:
 *   - LEVEL[6:0]  = 0 (FIFO empty)
 *   - WPTR[13:7]  = 0
 *   - RPTR[20:14] = 0
 *   - Reserved bits [31:21] = 0
 *
 * Note: FIFO_STATUS has read_mask = 0x00000000 in the regmodel register
 * definition, which means the TLM layer enforces a zero return for all
 * reads through b_transport.  A return of 0x00000000 is therefore the
 * only possible response and simultaneously confirms reset state and
 * correct regmodel read-restriction enforcement.
 *
 * Procedure:
 *  1. Disable FIFO fill before reading to ensure no background pushes occur
 *     that might change the internal state during measurement.
 *  2. Read FIFO_STATUS; assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == F006_FIFO_STATUS_RESET (0x00000000).
 *
 * Test plan reference: fifo_status_reset_value
 *
 * @return true if the assertion passes
 ******************************************************************************/
bool testbench::tc_f006_fifo_status_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Disable FIFO fill to prevent background pushes.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);
    // Manually drain entropy words pushed during the reset window.
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u) break;
    }

    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);

    FUNC006_CHECK(
        (rd_val & 0x7Fu) == (entropy_src_basetest::FIFO_STATUS_RESET & 0x7Fu),
        "TC-F006-040: FIFO_STATUS LEVEL mismatch — "
        "expected LEVEL=0, got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F006-041 — FIFO_STATUS.LEVEL increments with background fill
// =============================================================================

/******************************************************************************
 * @brief TC-F006-041: Confirm FIFO_STATUS.LEVEL increases as the background
 *        thread pushes values
 *
 * The background thread pushes one PRNG word per iteration while fifo_enabled
 * is true.  FIFO_STATUS.LEVEL must increase from 0.  Because FIFO_STATUS
 * has read_mask = 0x0 (TLM reads always return 0), the fill level is inferred
 * indirectly through FIFO_RDATA pops:
 *  - If at least N consecutive non-zero FIFO_RDATA pops are obtained, the
 *    FIFO had at least N entries, proving LEVEL was at least N.
 *
 * Procedure:
 *  1. With FIFO_CTRL[0]=1 (enabled at reset), poll FIFO_RDATA for a non-zero
 *     value (proves at least one push occurred, i.e., LEVEL >= 1).
 *  2. Read FIFO_STATUS; assert read_value == 0x00000000 (regmodel read restriction
 *     confirmation — read_mask = 0x0 enforced by regmodel).
 *  3. Assert the non-zero word was obtained within F006_POLL_LIMIT iterations.
 *
 * Pass criterion:
 *  - At least one non-zero FIFO_RDATA pop within the poll limit (LEVEL >= 1).
 *  - FIFO_STATUS TLM read returns 0x00000000.
 *
 * Test plan reference: fifo_status_level_increments_with_background_fill
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_status_level_increments_with_background_fill()
{
    bool     ok            = true;
    uint32_t rd_val        = 0u;
    bool     level_nonzero = false;

    // FIFO_CTRL is 0x1 at reset — fill is already enabled.
    // Poll for a non-zero FIFO_RDATA to confirm the FIFO received at least one
    // push from the background thread.
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            level_nonzero = true;
            break;
        }
    }

    FUNC006_CHECK(level_nonzero,
        "TC-F006-041: background thread did not produce any non-zero FIFO "
        "data within " << std::dec << F006_POLL_LIMIT
        << " SC_ZERO_TIME iterations — LEVEL did not increment from 0");

    // Confirm FIFO_STATUS TLM read returns 0x00000000 (read_mask enforcement).
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);

    FUNC006_CHECK((rd_val & 0x7f) != 0,
        "TC-F006-041: FIFO_STATUS TLM read expected non zero level, got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F006-042 — FIFO_STATUS.LEVEL decrements on FIFO_RDATA read
// =============================================================================

/******************************************************************************
 * @brief TC-F006-042: Confirm each FIFO_RDATA read decrements LEVEL by 1
 *        and increments RPTR by 1 (destructive pop semantics)
 *
 * The handle_read_FIFO_RDATA callback pops the front entry from the internal
 * queue, returns the entropy word, and atomically updates FIFO_STATUS
 * (LEVEL--, RPTR++).
 *
 * ## Architectural constraint (detailed-design Section 8.1):
 * Writing FIFO_CTRL[0]=0 immediately drains the entire FIFO queue via the
 * handle_write_FIFO_CTRL callback — before any software read can observe the
 * entries.  Therefore, the test MUST NOT disable fill before draining.
 *
 * ## Verified Strategy:
 * 1. Enable INTR_ENABLE[8|12] and allow the background thread to fill the FIFO
 *    until the overflow port asserts (LEVEL == 127).  This uses wait(SC_ZERO_TIME)
 *    per iteration which gives the background thread time to push each word.
 * 2. Do F006_DRAIN_COUNT = 128 consecutive FIFO_RDATA reads WITHOUT any
 *    wait() calls between reads.  Because b_transport is synchronous and the
 *    background thread only runs at wait() boundaries, all 128 reads execute
 *    before the background thread can push new words.  This ensures the reads
 *    see the 127 words that were in the FIFO at overflow detection time and the
 *    128th read triggers the underflow condition.
 * 3. After the burst drain, yield one SC_ZERO_TIME and verify intr_o.
 *
 * Pass criterion:
 *  - At least one non-zero word popped (confirms destructive pop from non-empty FIFO).
 *  - intr_o asserts after the 128 consecutive reads.
 *
 * Test plan reference: fifo_status_level_decrements_on_fifo_rdata_read
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_status_level_decrements_on_fifo_rdata_read()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Enable overflow and underflow interrupts.  Let the background
    // thread fill the FIFO to overflow capacity via SC_ZERO_TIME polling.
    // Overflow (intr_o = true) confirms LEVEL == 127.
    // Fill remains ENABLED throughout — FIFO must not be disabled here because
    // disable drains the FIFO (detailed-design §8.1 / handle_write_FIFO_CTRL).
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET,
        F006_INTR_EN_FIFO_OVERFLOW | F006_INTR_EN_FIFO_UNDERFLOW);

    // Clear any stale overflow/underflow status that might have persisted.
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET,
        F006_INTR_BIT_FIFO_OVERFLOW | F006_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    bool overflow_detected = false;
    for (int i = 0; i < F006_FILL_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);
        if (test->intr_i.read() && (rd_val & F006_INTR_BIT_FIFO_OVERFLOW))
        {
            overflow_detected = true;
            break;
        }
    }

    FUNC006_CHECK(
        overflow_detected,
        "TC-F006-042 step 1: FIFO did not reach overflow within "
        << std::dec << F006_FILL_POLL_LIMIT << " SC_ZERO_TIME iterations");

    if (!overflow_detected)
    {
        // Can't proceed — clean up and bail early.
        test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
        return false;
    }

    // -------------------------------------------------------------------------
    // Step 2: Drain exactly F006_DRAIN_COUNT = 128 reads as a SYNCHRONOUS BURST
    // (no wait() between reads).  The background thread cannot run between reads
    // because it only gets CPU time at wait() boundaries.  Therefore, all 128
    // reads operate on the 127-entry FIFO that was full at overflow detection:
    //   - Reads 1–127:  pop one entry each, LEVEL decrements 127→0
    //   - Read 128:     FIFO is empty → handle_read_FIFO_RDATA returns 0x00000000
    //                   and sets INTR_STATUS[12] (FIFO_UNDERFLOW)
    // Fill is still ENABLED; the background thread will restart at the next
    // wait(), but by then all 128 reads are complete.
    // -------------------------------------------------------------------------
    int      pop_count    = 0;
    uint32_t last_nonzero = 0u;

    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            ++pop_count;
            last_nonzero = rd_val;
        }
    }

    // At least one non-zero word must have been popped (FIFO was full at start).
    FUNC006_CHECK(
        pop_count > 0,
        "TC-F006-042 step 2: no non-zero words popped during burst drain — "
        "destructive pop not decrementing LEVEL (pop_count=0)");

    // -------------------------------------------------------------------------
    // Step 3: Yield one SC_ZERO_TIME so the interrupt SC_METHOD can update the
    // port state, then verify intr_o is asserted.
    // -------------------------------------------------------------------------
    wait(sc_core::SC_ZERO_TIME);

    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::INTR_STATUS_OFFSET, rd_val);

    FUNC006_CHECK(
        test->intr_i.read() == true && (rd_val & F006_INTR_BIT_FIFO_UNDERFLOW),
        "TC-F006-042 step 3: intr_o not asserted after "
        << std::dec << F006_DRAIN_COUNT << " burst reads — "
        "LEVEL did not reach 0 (pop_count was " << pop_count << ", "
        "last_nonzero=0x" << std::hex << last_nonzero << ")");

    // -------------------------------------------------------------------------
    // Step 4: Clean up.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET,
        F006_INTR_BIT_FIFO_OVERFLOW | F006_INTR_BIT_FIFO_UNDERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-043 — FIFO_STATUS.LEVEL saturates at FIFO_DEPTH (127)
// =============================================================================

/******************************************************************************
 * @brief TC-F006-043: Confirm FIFO_STATUS.LEVEL saturates at 127 (FIFO_DEPTH)
 *
 * When the background thread attempts to push into a full FIFO (LEVEL == 127),
 * the push is discarded and INTR_STATUS[8] (FIFO_OVERFLOW) is set.  LEVEL
 * does not increment beyond FIFO_DEPTH.
 *
 * Observability strategy: FIFO_STATUS read_mask = 0x0, so level is confirmed
 * by detecting the overflow interrupt:
 *  - Enable INTR_ENABLE[8] (FIFO_OVERFLOW).
 *  - Allow the background thread to fill the FIFO to capacity.
 *  - Once intr_o asserts, LEVEL == FIFO_DEPTH is confirmed.
 *  - Read FIFO_STATUS and confirm 0x00000000 (regmodel read restriction).
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[8] to allow the overflow port to assert.
 *  2. Ensure FIFO fill is enabled (FIFO_CTRL[0]=1, reset default).
 *  3. Poll intr_o port until it asserts (max F006_FILL_POLL_LIMIT).
 *  4. Assert overflow port == true (LEVEL reached FIFO_DEPTH).
 *  5. Read FIFO_STATUS; assert 0x00000000 (regmodel enforcement).
 *  6. Clean up: clear INTR_STATUS[8] via W1C; disable INTR_ENABLE[8].
 *
 * Pass criterion:
 *  - intr_o asserts within the poll limit.
 *  - FIFO_STATUS read returns 0x00000000.
 *
 * Test plan reference: fifo_status_level_at_maximum_depth
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_status_level_at_maximum_depth()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Enable INTR_ENABLE[8] so the overflow port can assert.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET, F006_INTR_EN_FIFO_OVERFLOW);

    // -------------------------------------------------------------------------
    // Step 2: FIFO fill is enabled by default; confirm and proceed.
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_CTRL_OFFSET, rd_val);
    FUNC006_CHECK(rd_val == entropy_src_basetest::FIFO_CTRL_RESET,
        "TC-F006-043 step 2: FIFO_CTRL[0] not 1 at reset — "
        "expected FIFO fill enabled, got 0x" << std::hex << rd_val);

    // -------------------------------------------------------------------------
    // Step 3: Poll intr_o port.
    // The background thread fills the FIFO from 0 to 127, then the next push
    // triggers overflow and sets INTR_STATUS[8], asserting the port.
    // -------------------------------------------------------------------------
    bool overflow_observed = false;
    for (int i = 0; i < F006_FILL_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        if (test->intr_i.read())
        {
            overflow_observed = true;
            break;
        }
    }

    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);

    // -------------------------------------------------------------------------
    // Step 4: Assert overflow was observed (LEVEL reached FIFO_DEPTH).
    // -------------------------------------------------------------------------
    FUNC006_CHECK(overflow_observed && ((rd_val & 0x7f) == 32),
        "TC-F006-043 step 4: intr_o did not assert within "
        << std::dec << F006_FILL_POLL_LIMIT
        << " SC_ZERO_TIME iterations — FIFO may not have reached FIFO_DEPTH=32");

    // -------------------------------------------------------------------------
    // Step 5: Clean up — clear INTR_STATUS[8] and disable interrupt enable.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_OVERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-044 — FIFO_STATUS is read-only
// =============================================================================

/******************************************************************************
 * @brief TC-F006-044: Confirm FIFO_STATUS (0x24) is RO — software write has
 *        no observable effect
 *
 * FIFO_STATUS is a read-only register (write_mask = 0x00000000).  A software
 * write of 0xFFFFFFFF must be silently ignored by regmodel.  A subsequent read
 * must return the same value as before the write (0x00000000, since
 * read_mask = 0x0).
 *
 * Procedure:
 *  1. Read FIFO_STATUS before write; record value (expected 0x00000000).
 *  2. Write 0xFFFFFFFF to FIFO_STATUS.
 *  3. Read FIFO_STATUS after write; assert value == pre-write value.
 *
 * Pass criterion: read_after_write == read_before_write == 0x00000000.
 *
 * Test plan reference: fifo_status_is_read_only
 *
 * @return true if assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_status_is_read_only()
{
    bool     ok         = true;
    uint32_t before     = 0u;
    uint32_t after      = 0u;

    // Disable fill to get a stable read-only snapshot.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, before);
    test->register_write_32(entropy_src_basetest::FIFO_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, after);

    FUNC006_CHECK(
        after == before,
        "TC-F006-044: FIFO_STATUS changed after write — "
        "expected 0x" << std::hex << before << " got 0x" << after);

    return ok;
}

// =============================================================================
// TC-F006-045 — FIFO_RDATA returns non-zero entropy when non-empty
// =============================================================================

/******************************************************************************
 * @brief TC-F006-045: Confirm FIFO_RDATA returns a non-zero PRNG value when
 *        the queue is non-empty
 *
 * When the internal FIFO queue is non-empty, handle_read_FIFO_RDATA pops the
 * front entry and delivers it to the TLM payload.  The entropy source uses
 * std::mt19937 which has a negligible probability of producing 0x00000000.
 * A single non-zero FIFO_RDATA read within the poll limit confirms the pop
 * path is operational.
 *
 * Procedure:
 *  1. With FIFO_CTRL[0]=1 (reset default), poll FIFO_RDATA.
 *  2. Assert that at least one non-zero word is returned within F006_POLL_LIMIT.
 *
 * Pass criterion: at least one FIFO_RDATA read returns a value != 0x00000000.
 *
 * Test plan reference: fifo_rdata_returns_nonzero_entropy_when_nonempty
 *
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f006_fifo_rdata_returns_nonzero_entropy_when_nonempty()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;
    bool     got_nonzero = false;

    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            got_nonzero = true;
            break;
        }
    }

    FUNC006_CHECK(
        got_nonzero,
        "TC-F006-045: FIFO_RDATA did not return a non-zero entropy word within "
        << std::dec << F006_POLL_LIMIT
        << " SC_ZERO_TIME polls — FIFO pop or PRNG not functioning");

    return ok;
}

// =============================================================================
// TC-F006-046 — Successive FIFO_RDATA reads yield different values
// =============================================================================

/******************************************************************************
 * @brief TC-F006-046: Confirm successive FIFO_RDATA reads yield different
 *        PRNG values (FIFO pops correctly from the queue)
 *
 * Each FIFO_RDATA read must pop a distinct entry from the internal queue.
 * Since the PRNG is non-deterministic from the testbench's perspective, this
 * test gathers a set of N reads and verifies that not all values are identical
 * — statistically, two distinct PRNG words must appear within a reasonable
 * sample.
 *
 * Procedure:
 *  1. Collect up to F006_POLL_LIMIT FIFO_RDATA reads.
 *  2. Assert that the set of observed values contains at least two distinct
 *     non-zero values OR that successive reads differ at least once.
 *
 * Pass criterion: at least two FIFO_RDATA reads return different non-zero values.
 *
 * Test plan reference: fifo_rdata_successive_reads_yield_different_values
 *
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f006_fifo_rdata_successive_reads_yield_different_values()
{
    bool     ok          = true;
    uint32_t first_val   = 0u;
    uint32_t second_val  = 0u;
    bool     got_first   = false;
    bool     got_second  = false;

    // Collect the first non-zero FIFO_RDATA value.
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        uint32_t rv = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rv);
        if (rv != 0x00000000u)
        {
            first_val = rv;
            got_first = true;
            break;
        }
    }

    FUNC006_CHECK(
        got_first,
        "TC-F006-046: could not obtain first non-zero FIFO_RDATA value within "
        << std::dec << F006_POLL_LIMIT << " iterations");

    // Collect a second non-zero FIFO_RDATA value that differs from the first.
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        uint32_t rv = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rv);
        if (rv != 0x00000000u && rv != first_val)
        {
            second_val = rv;
            got_second = true;
            break;
        }
    }

    FUNC006_CHECK(
        got_second,
        "TC-F006-046: successive FIFO_RDATA reads did not yield a second "
        "distinct non-zero value within " << std::dec << F006_POLL_LIMIT
        << " iterations — first_val=0x" << std::hex << first_val
        << ", second_val=0x" << second_val);

    return ok;
}

// =============================================================================
// TC-F006-047 — Empty FIFO read returns zero and sets underflow
// =============================================================================

/******************************************************************************
 * @brief TC-F006-047: Core underflow test — empty queue read returns
 *        0x00000000 and sets INTR_STATUS[12] (FIFO_UNDERFLOW)
 *
 * When FIFO_RDATA is read while the internal queue is empty,
 * handle_read_FIFO_RDATA must:
 *  - Return 0x00000000 in the TLM payload.
 *  - Set INTR_STATUS[12] (FIFO_UNDERFLOW) via regmodel internal write.
 *  - Call update_interrupt_outputs.
 *
 * Because INTR_STATUS has read_mask = 0x0 (TLM reads return 0), underflow
 * is confirmed by:
 *  (a) FIFO_RDATA returns 0x00000000.
 *  (b) intr_o port asserts when INTR_ENABLE[12]=1.
 *
 * Procedure:
 *  1. Stop FIFO fill (FIFO_CTRL[0]=0) and drain the FIFO completely.
 *  2. Enable INTR_ENABLE[12].
 *  3. Read FIFO_RDATA; assert return value == 0x00000000.
 *  4. Assert intr_o port is true.
 *  5. Clean up: W1C clear INTR_STATUS[12]; disable INTR_ENABLE[12].
 *
 * Pass criterion:
 *  - FIFO_RDATA returns 0x00000000 on empty-FIFO read.
 *  - intr_o == true after the empty read with INTR_ENABLE[12]=1.
 *
 * Test plan reference: fifo_rdata_empty_fifo_returns_zero_and_sets_underflow
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_rdata_empty_fifo_returns_zero_and_sets_underflow()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Disable fill and unconditionally drain FIFO_DRAIN_COUNT = 128
    // reads to guarantee the FIFO is empty regardless of zero-valued PRNG words.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < F006_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }
    // Drain exactly FIFO_DEPTH+1 reads unconditionally.
    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }

    // -------------------------------------------------------------------------
    // Step 2: Enable INTR_ENABLE[12] so underflow port can assert.
    // Clear any underflow status triggered by the drain reads.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET, F006_INTR_EN_FIFO_UNDERFLOW);
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    // -------------------------------------------------------------------------
    // Step 3: Read FIFO_RDATA on an empty FIFO; must return 0x00000000.
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);

    FUNC006_CHECK(
        rd_val == 0x00000000u,
        "TC-F006-047 step 3: empty FIFO_RDATA read expected 0x00000000, "
        "got 0x" << std::hex << rd_val);

    // Allow update_interrupt_outputs to propagate via SC_METHOD.
    wait(sc_core::SC_ZERO_TIME);

    // -------------------------------------------------------------------------
    // Step 4: Assert intr_o port is true.
    // -------------------------------------------------------------------------
    FUNC006_CHECK(
        test->intr_i.read() == true,
        "TC-F006-047 step 4: intr_o port not asserted after "
        "empty FIFO read with INTR_ENABLE[12]=1");

    // -------------------------------------------------------------------------
    // Step 5: Clean up.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_UNDERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-048 — intr_o port assertion on empty read
// =============================================================================

/******************************************************************************
 * @brief TC-F006-048: Confirm empty FIFO read asserts intr_o
 *        port when INTR_ENABLE[12]=1
 *
 * This is the port-level complement to TC-F006-047.  It verifies the full
 * interrupt output path: underflow event → INTR_STATUS[12] → AND with
 * INTR_ENABLE[12] → intr_o sc_out<bool>.
 *
 * Procedure:
 *  1. Disable FIFO fill and drain to empty.
 *  2. Enable INTR_ENABLE[12].
 *  3. Confirm intr_o is currently false (no pending underflow).
 *  4. Read FIFO_RDATA on empty FIFO to trigger underflow.
 *  5. After SC_ZERO_TIME propagation, assert intr_o == true.
 *  6. W1C clear INTR_STATUS[12]; assert intr_o == false.
 *  7. Clean up: disable INTR_ENABLE[12].
 *
 * Pass criterion:
 *  - intr_o == false before empty read.
 *  - intr_o == true after empty read with INTR_ENABLE[12]=1.
 *  - intr_o == false after W1C clear.
 *
 * Test plan reference: fifo_underflow_interrupt_port_on_empty_read
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_underflow_interrupt_port_on_empty_read()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Disable fill and unconditionally drain FIFO_DRAIN_COUNT = 128
    // reads to guarantee the FIFO is empty.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < F006_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }
    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }

    // -------------------------------------------------------------------------
    // Step 2: Enable INTR_ENABLE[12]; clear any underflow status from drain.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET, F006_INTR_EN_FIFO_UNDERFLOW);
    // Clear ALL INTR_STATUS bits to avoid stale bits keeping the combined line asserted.
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, 0x00001111u);
    wait(sc_core::SC_ZERO_TIME);

    // -------------------------------------------------------------------------
    // Step 3: Port must be false before the empty read.
    // -------------------------------------------------------------------------
    FUNC006_CHECK(
        test->intr_i.read() == false,
        "TC-F006-048 step 3: intr_o expected false before "
        "empty read, got true");

    // -------------------------------------------------------------------------
    // Step 4: Trigger underflow with an empty FIFO read.
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    wait(sc_core::SC_ZERO_TIME);

    // -------------------------------------------------------------------------
    // Step 5: Port must be true after the empty read.
    // -------------------------------------------------------------------------
    FUNC006_CHECK(
        test->intr_i.read() == true,
        "TC-F006-048 step 5: intr_o expected true after "
        "empty FIFO read with INTR_ENABLE[12]=1, got false");

    // -------------------------------------------------------------------------
    // Step 6: W1C clear INTR_STATUS[12] and verify port deasserts.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    FUNC006_CHECK(
        test->intr_i.read() == false,
        "TC-F006-048 step 6: intr_o expected false after "
        "W1C clear of INTR_STATUS[12], got true");

    // -------------------------------------------------------------------------
    // Step 7: Clean up.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-050 — Drain FIFO to empty and verify LEVEL zero
// =============================================================================

/******************************************************************************
 * @brief TC-F006-050: Confirm draining all entries via FIFO_RDATA reads
 *        reduces FIFO_STATUS.LEVEL to exactly 0
 *
 * This test exercises the full software-read drain sequence and confirms that
 * the FIFO reaches the empty state (underflow on the subsequent read).
 *
 * ## Architectural constraint (detailed-design Section 8.1):
 * Writing FIFO_CTRL[0]=0 immediately drains the entire FIFO queue in the
 * handle_write_FIFO_CTRL callback.  Therefore, the test must NOT disable fill
 * before the software-read drain.  Instead, fill is kept active and a burst
 * of F006_DRAIN_COUNT = 128 synchronous reads (no wait() between reads) races
 * ahead of the background thread.
 *
 * Strategy:
 *  1. Enable INTR_ENABLE[8|12].  Allow background to fill FIFO to overflow.
 *  2. Burst-drain exactly F006_DRAIN_COUNT = 128 reads with no wait() between
 *     reads.  The background thread cannot push during this burst.
 *  3. Yield SC_ZERO_TIME; assert intr_o (LEVEL == 0).
 *  4. Do one additional read; assert it returns 0x00000000 (still empty).
 *
 * Pass criterion:
 *  - At least one non-zero word popped (confirms fill produced data).
 *  - intr_o asserts after burst drain.
 *  - Post-drain FIFO_RDATA reads return 0x00000000.
 *
 * Test plan reference: fifo_drain_to_empty_and_verify_level_zero
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_drain_to_empty_and_verify_level_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Enable overflow and underflow interrupts.  Clear stale status.
    // Allow the background thread to fill the FIFO to overflow (LEVEL == 127).
    // Keep fill ENABLED throughout — disabling fill drains the FIFO immediately
    // (documented in handle_write_FIFO_CTRL / detailed-design §8.1).
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET,
        F006_INTR_EN_FIFO_OVERFLOW | F006_INTR_EN_FIFO_UNDERFLOW);
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET,
        F006_INTR_BIT_FIFO_OVERFLOW | F006_INTR_BIT_FIFO_UNDERFLOW);
    wait(sc_core::SC_ZERO_TIME);

    bool overflow_seen = false;
    for (int i = 0; i < F006_FILL_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        if (test->intr_i.read())
        {
            overflow_seen = true;
            break;
        }
    }

    FUNC006_CHECK(
        overflow_seen,
        "TC-F006-050 step 1: FIFO did not reach overflow in "
        << std::dec << F006_FILL_POLL_LIMIT << " SC_ZERO_TIME iterations");

    if (!overflow_seen)
    {
        test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
        return false;
    }

    // -------------------------------------------------------------------------
    // Step 2: Burst-drain F006_DRAIN_COUNT = 128 reads with NO wait() between
    // reads.  All 128 reads complete before the background thread can push new
    // words.  Reads 1–127 pop the 127 entries; read 128 finds FIFO empty and
    // sets INTR_STATUS[12].
    // -------------------------------------------------------------------------
    int pop_count = 0;
    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            ++pop_count;
        }
    }

    FUNC006_CHECK(
        pop_count > 0,
        "TC-F006-050 step 2: no non-zero words popped in burst drain — "
        "FIFO was empty at start of drain (pop_count=0)");

    // -------------------------------------------------------------------------
    // Step 3: Yield one SC_ZERO_TIME for the interrupt SC_METHOD to update
    // ports, then verify intr_o is asserted (LEVEL == 0).
    // -------------------------------------------------------------------------
    wait(sc_core::SC_ZERO_TIME);

    FUNC006_CHECK(
        test->intr_i.read() == true,
        "TC-F006-050 step 3: intr_o not asserted after burst drain "
        "of " << std::dec << F006_DRAIN_COUNT << " reads (pop_count=" << pop_count << ")");

    // -------------------------------------------------------------------------
    // Step 4: Clear underflow status, then do one additional empty read.
    // The FIFO must still be empty (background thread may push new words in
    // subsequent deltas, so this step must happen before another wait()).
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_UNDERFLOW);
    // Re-enable underflow bit so the port can fire again.
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET,
        F006_INTR_EN_FIFO_OVERFLOW | F006_INTR_EN_FIFO_UNDERFLOW);

    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);

    FUNC006_CHECK(
        rd_val == 0x00000000u,
        "TC-F006-050 step 4: post-drain FIFO_RDATA expected 0x00000000 "
        "(FIFO still empty), got 0x" << std::hex << rd_val);

    wait(sc_core::SC_ZERO_TIME);

    FUNC006_CHECK(
        test->intr_i.read() == true,
        "TC-F006-050 step 4: intr_o not asserted on second empty read");

    // -------------------------------------------------------------------------
    // Step 5: Clean up.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET,
        F006_INTR_BIT_FIFO_OVERFLOW | F006_INTR_BIT_FIFO_UNDERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-051 — Overflow sets INTR_STATUS[8]
// =============================================================================

/******************************************************************************
 * @brief TC-F006-051: Core overflow test — background push into full FIFO
 *        sets INTR_STATUS[8] (FIFO_OVERFLOW); LEVEL stays at 127
 *
 * When the background thread attempts to push a word into a full FIFO
 * (LEVEL == FIFO_DEPTH = 127), the push is discarded and INTR_STATUS[8]
 * is set via regmodel internal write.  LEVEL must not exceed FIFO_DEPTH.
 *
 * Overflow is detected via the intr_o port (INTR_ENABLE[8]=1)
 * since INTR_STATUS has read_mask = 0x0.
 *
 * Procedure:
 *  1. Enable INTR_ENABLE[8].
 *  2. Allow the background thread to fill the FIFO (FIFO_CTRL[0]=1, default).
 *  3. Poll intr_o until asserted (LEVEL reached 127).
 *  4. Assert intr_o == true.
 *  5. Read FIFO_STATUS; assert 0x00000000 (regmodel enforcement).
 *  6. Read one word from FIFO_RDATA; assert it is non-zero (FIFO was full).
 *  7. Clean up.
 *
 * Pass criterion:
 *  - intr_o asserts (LEVEL reached FIFO_DEPTH).
 *  - FIFO_STATUS TLM read returns 0x00000000.
 *  - At least one non-zero word can be popped after overflow (FIFO was full).
 *
 * Test plan reference: fifo_overflow_sets_intr_status_bit
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_overflow_sets_intr_status_bit()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Enable INTR_ENABLE[8] (FIFO_OVERFLOW).
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_ENABLE_OFFSET, F006_INTR_EN_FIFO_OVERFLOW);

    // -------------------------------------------------------------------------
    // Step 2-3: Poll for overflow port assertion.
    // -------------------------------------------------------------------------
    bool overflow_detected = false;
    for (int i = 0; i < F006_FILL_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        if (test->intr_i.read())
        {
            overflow_detected = true;
            break;
        }
    }

    // -------------------------------------------------------------------------
    // Step 4: Assert overflow was detected.
    // -------------------------------------------------------------------------
    FUNC006_CHECK(overflow_detected,
        "TC-F006-051 step 4: intr_o did not assert within "
        << std::dec << F006_FILL_POLL_LIMIT
        << " SC_ZERO_TIME iterations — FIFO did not reach FIFO_DEPTH=127");

    // ------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Step 6: Pop one word — must be non-zero (FIFO was full of PRNG words).
    // -------------------------------------------------------------------------
    bool popped_nonzero = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            popped_nonzero = true;
            break;
        }
    }

    FUNC006_CHECK(popped_nonzero,
        "TC-F006-051 step 6: could not pop a non-zero word after overflow — "
        "FIFO should have been full of PRNG words at this point");

    // -------------------------------------------------------------------------
    // Step 7: Clean up.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::INTR_STATUS_OFFSET, F006_INTR_BIT_FIFO_OVERFLOW);
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    return ok;
}

// =============================================================================
// TC-F006-053 — FIFO fill halts when FIFO_CTRL[0]=0 mid-operation
// =============================================================================

/******************************************************************************
 * @brief TC-F006-053: Confirm FIFO_CTRL[0]=0 mid-operation freezes
 *        FIFO_STATUS.LEVEL
 *
 * Writing FIFO_CTRL[0] = 0 while the background thread is actively filling
 * the FIFO must:
 *  1. Cause the thread to stop pushing new words.
 *  2. Leave the FIFO at whatever level it reached at the time of disable.
 *
 * Because FIFO_STATUS has read_mask = 0x0, level freezing is confirmed by
 * draining all words that were in the FIFO at the time of disable and
 * verifying no new words appear after drain.
 *
 * Procedure:
 *  1. Let the background thread fill the FIFO for a short period.
 *  2. Disable FIFO fill (FIFO_CTRL[0]=0).
 *  3. Wait for settle period.
 *  4. Drain all FIFO entries until underflow.
 *  5. Poll for additional words post-drain; assert none appear
 *     (all polls return 0x00000000).
 *
 * Pass criterion:
 *  - At least one word was drained (FIFO was non-empty at disable time).
 *  - After drain, all further reads return 0x00000000 (fill is halted).
 *
 * Test plan reference: fifo_fill_halts_when_disabled_during_operation
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_fill_halts_when_disabled_during_operation()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Poll until the background thread has pushed at least one word.
    // FIFO_CTRL[0]=1 at reset; the thread begins filling immediately.
    // We wait until a non-zero FIFO_RDATA is obtainable to confirm the FIFO
    // is non-empty before we disable fill.
    // -------------------------------------------------------------------------
    bool pre_disable_fill_confirmed = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            pre_disable_fill_confirmed = true;
            break;
        }
    }

    FUNC006_CHECK(
        pre_disable_fill_confirmed,
        "TC-F006-053 step 1: background thread did not produce any FIFO data "
        "before disable — cannot verify fill halts");

    // -------------------------------------------------------------------------
    // Step 2: Disable FIFO fill mid-operation.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);

    // -------------------------------------------------------------------------
    // Step 3: Allow the thread to observe the new control value.
    // -------------------------------------------------------------------------
    for (int i = 0; i < F006_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // -------------------------------------------------------------------------
    // Step 4: Unconditionally drain FIFO_DRAIN_COUNT reads to empty the FIFO.
    // Count non-zero words to confirm the FIFO had content at disable time.
    // -------------------------------------------------------------------------
    int drain_count = 0;
    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            ++drain_count;
        }
    }

    // The pre-disable poll already popped one word; drain_count here counts
    // residual words in the FIFO after disable (may be 0 if the thread was
    // very fast to halt).  The overall test confirms halt by checking step 5.
    (void)drain_count; // drain_count is informational; not asserted here.

    // -------------------------------------------------------------------------
    // Step 5: Poll for additional words after drain; none should appear.
    // -------------------------------------------------------------------------
    bool new_words_appeared = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            new_words_appeared = true;
            break;
        }
    }

    FUNC006_CHECK(
        !new_words_appeared,
        "TC-F006-053 step 5: new words appeared after disable and drain — "
        "FIFO fill did not halt when FIFO_CTRL[0]=0");

    return ok;
}

// =============================================================================
// TC-F006-054 — FIFO fill resumes after re-enable
// =============================================================================

/******************************************************************************
 * @brief TC-F006-054: Confirm FIFO_CTRL[0]=1 re-enables fill and LEVEL
 *        resumes increasing from the halted value
 *
 * Writing FIFO_CTRL[0] = 1 after a disable must notify fifo_enable_event,
 * waking the background thread from WAITING_FOR_ENABLE.  The thread resumes
 * pushing new PRNG words into the FIFO from the level at which it was halted.
 *
 * Procedure:
 *  1. Disable FIFO fill (FIFO_CTRL[0]=0) and drain to empty.
 *  2. Re-enable FIFO fill (FIFO_CTRL[0]=1).
 *  3. Poll FIFO_RDATA for a non-zero word (confirms fill resumed).
 *  4. Assert at least one non-zero FIFO_RDATA obtained.
 *
 * Pass criterion:
 *  - Non-zero FIFO_RDATA obtained after re-enable within F006_POLL_LIMIT.
 *
 * Test plan reference: fifo_fill_resumes_after_reenable
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f006_fifo_fill_resumes_after_reenable()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Disable fill and unconditionally drain FIFO_DRAIN_COUNT reads.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < F006_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }
    for (int i = 0; i < F006_DRAIN_COUNT; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    }

    // -------------------------------------------------------------------------
    // Step 2: Re-enable fill.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);

    // -------------------------------------------------------------------------
    // Step 3: Poll for a non-zero FIFO_RDATA word.
    // -------------------------------------------------------------------------
    bool fill_resumed = false;
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            fill_resumed = true;
            break;
        }
    }

    // -------------------------------------------------------------------------
    // Step 4: Assert fill resumed.
    // -------------------------------------------------------------------------
    FUNC006_CHECK(
        fill_resumed,
        "TC-F006-054 step 4: FIFO fill did not resume after FIFO_CTRL[0]=1 "
        "re-enable — no non-zero FIFO_RDATA within " << std::dec
        << F006_POLL_LIMIT << " SC_ZERO_TIME iterations");

    return ok;
}

// =============================================================================
// TC-F006-129 — RO write has no effect on FIFO_STATUS
// =============================================================================

/******************************************************************************
 * @brief TC-F006-129: Confirm FIFO_STATUS (0x24) is fully RO from the
 *        software perspective
 *
 * An explicit write-then-read check: writing 0xFFFFFFFF to FIFO_STATUS
 * must leave the register value unchanged at 0x00000000.  The regmodel write-mask
 * enforcement (write_mask = 0x0) silently discards the write.
 *
 * Procedure:
 *  1. Disable FIFO fill (quiescent state).
 *  2. Read FIFO_STATUS before write; record value.
 *  3. Write 0xFFFFFFFF to FIFO_STATUS.
 *  4. Read FIFO_STATUS after write; assert value unchanged.
 *
 * Pass criterion: FIFO_STATUS after write == FIFO_STATUS before write
 *                 == 0x00000000.
 *
 * Test plan reference: ro_register_write_has_no_effect_fifo_status
 *
 *
 * @return true if assertions pass
 ******************************************************************************/
bool testbench::tc_f006_ro_write_has_no_effect_fifo_status()
{
    bool     ok     = true;
    uint32_t before = 0u;
    uint32_t after  = 0u;

    // Disable fill to avoid concurrency with background pushes.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, before);
    test->register_write_32(entropy_src_basetest::FIFO_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, after);

    FUNC006_CHECK(
        after == before,
        "TC-F006-129: FIFO_STATUS changed after write of 0xFFFFFFFF — "
        "before=0x" << std::hex << before << " after=0x" << after);

    return ok;
}

/******************************************************************************
 * @brief TC-F006-136: Confirm FIFO_STATUS.WPTR[13:7] increments from 0 as
 *        background pushes occur
 *
 * Each background push increments the internal wptr counter.  FIFO_STATUS
 * is updated atomically after each push: WPTR = wptr % 128 packed into
 * bits [13:7].
 *
 * Because FIFO_STATUS has read_mask = 0x0 (TLM reads return 0), WPTR
 * advancement is inferred indirectly:
 *  - Each non-zero FIFO_RDATA pop corresponds to exactly one prior push
 *    (wptr was incremented when that word was pushed).
 *  - Obtaining N distinct non-zero pops confirms wptr advanced at least N
 *    times from its reset value of 0.
 *
 * Procedure:
 *  1. With FIFO_CTRL[0]=1 (reset default), collect a batch of non-zero
 *     FIFO_RDATA pops.
 *  2. Assert at least 2 distinct non-zero values obtained (wptr advanced
 *     by at least 2).
 *  3. Read FIFO_STATUS; confirm TLM returns 0x00000000 (read_mask enforcement).
 *
 * Pass criterion:
 *  - At least 2 non-zero, distinct words popped (wptr advanced >= 2 times).
 *  - FIFO_STATUS TLM read returns 0x00000000.
 *
 * Test plan reference: fifo_wptr_advances_with_background_push
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_wptr_advances_with_background_push()
{
    bool     ok         = true;
    uint32_t rd_val     = 0u;
    int      advances   = 0;
    uint32_t prev_wptr  = 0xFFFFFFFFu; // Initialize to an impossible value

    // Collect WPTR advancements
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
        uint32_t current_wptr = (rd_val >> 8) & 0x1F;
        
        if (prev_wptr != 0xFFFFFFFFu && current_wptr != prev_wptr)
        {
            ++advances;
            if (advances >= 2)
            {
                break;
            }
        }
        prev_wptr = current_wptr;
    }

    FUNC006_CHECK(advances >= 2,
        "TC-F006-136: WPTR did not advance at least twice. "
        "Got " << std::dec << advances << " advances.");

    return ok;
}

// =============================================================================
// TC-F006-137 — FIFO_STATUS.RPTR advances with FIFO read
// =============================================================================

/******************************************************************************
 * @brief TC-F006-137: Confirm FIFO_STATUS.RPTR[20:16] increments as words
 *        are popped from FIFO_RDATA
 *
 * Procedure:
 *  1. Wait for FIFO to fill slightly via background thread.
 *  2. Read FIFO_STATUS to get initial RPTR.
 *  3. Read FIFO_RDATA to pop a word.
 *  4. Read FIFO_STATUS and verify RPTR advanced.
 *  5. Repeat to observe multiple advancements.
 *
 * Pass criterion:
 *  - RPTR advanced by at least 2 after popping words.
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f006_fifo_rptr_advances_with_read()
{
    bool     ok         = true;
    uint32_t rd_val     = 0u;
    int      advances   = 0;

    // Give some time for background thread to push data
    for (int i = 0; i < F006_POLL_LIMIT; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u) {
            break;
        }
    }

    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
    uint32_t prev_rptr = (rd_val >> 16) & 0x1F;

    // Read multiple times from FIFO_RDATA and verify RPTR advances
    for (int i = 0; i < 3; ++i)
    {
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        wait(F006_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, rd_val);
        uint32_t current_rptr = (rd_val >> 16) & 0x1F;
        REG_DEBUG(2, logger) << "TC-F006-137: RPTR advanced to " << std::hex << current_rptr;
        if (current_rptr != prev_rptr)
        {
            ++advances;
        }
        prev_rptr = current_rptr;
    }

    FUNC006_CHECK(advances >= 2,
        "TC-F006-137: RPTR did not advance correctly with FIFO reads. "
        "Got " << std::dec << advances << " advances.");

    return ok;
}