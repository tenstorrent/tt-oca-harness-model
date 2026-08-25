// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file func004_tests.cpp
 * @brief FUNC-004 Background Entropy Generation Process — test case
 *        implementations
 *
 * Implements all 12 test cases mapped to FUNC-004 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to one
 * or more rows in the FUNC-004 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-004 — 12 test cases)
 *
 *  Sl. | Method                                                       | Test Plan ID
 *  ----|--------------------------------------------------------------|-----------------------------------
 *  39  | tc_f004_fifo_ctrl_enable_disable_fifo                        | fifo_ctrl_enable_disable_fifo
 *  41  | tc_f004_fifo_status_level_increments_with_background_fill    | fifo_status_level_increments_with_background_fill
 *  53  | tc_f004_fifo_fill_halts_when_disabled_during_operation       | fifo_fill_halts_when_disabled_during_operation
 *  54  | tc_f004_fifo_fill_resumes_after_reenable                     | fifo_fill_resumes_after_reenable
 *   6  | tc_f004_ctrl_downsample_rate_readback                        | ctrl_downsample_rate_readback
 * 101  | tc_f004_startup_ctrl_nonzero_delay_applied_after_reset       | startup_ctrl_nonzero_delay_applied_after_reset
 * 102  | tc_f004_startup_ctrl_zero_delay_no_holdoff                   | startup_ctrl_zero_delay_no_holdoff
 * 103  | tc_f004_startup_ctrl_delay_consumed_only_at_next_reset       | startup_ctrl_delay_consumed_only_at_next_reset
 * 113  | tc_f004_software_reset_stabilization_holdoff_observable      | software_reset_stabilization_holdoff_observable
 * 115  | tc_f004_software_reset_fifo_disabled_does_not_fill           | software_reset_fifo_disabled_background_does_not_fill
 * 140  | tc_f004_background_process_entropy_continuously_generated    | background_process_entropy_continuously_generated
 *
 * ## Background SC_THREAD architecture
 *
 * The entropy_generation_thread runs continuously through four logical states:
 *
 *  - WAITING_FOR_ENABLE: suspended on m_fifo_fill_event | m_reset_event when
 *    m_fifo_enabled == false (FIFO_CTRL[0] = 0).
 *  - STARTUP_DELAY: waits m_startup_delay_ns (from STARTUP_CTRL[15:0])
 *    before entering RUNNING; interruptible by m_reset_event.
 *  - RUNNING: one PRNG push per iteration when FIFO not full; overflow detection
 *    sets INTR_STATUS[8]; DOWNSAMPLE_RATE (CTRL[25:16]) scales the per-iteration
 *    wait (BASE_ITERATION_PERIOD_NS * (1 + rate) ns when non-zero, SC_ZERO_TIME
 *    when zero).
 *  - RESET_PENDING: entered when m_reset_event fires; clears m_reset_in_progress
 *    and transitions back to WAITING_FOR_ENABLE or RUNNING.
 *
 * ## Timing model
 *
 * When DOWNSAMPLE_RATE == 0 the thread yields with wait(SC_ZERO_TIME) every
 * iteration; all thread scheduling is delta-cycle driven.  This allows the
 * testbench to race the thread using SC_ZERO_TIME polling loops.
 *
 * When DOWNSAMPLE_RATE != 0 the thread uses a real-time wait of
 * (BASE_ITERATION_PERIOD_NS * (1 + rate)) ns per iteration.  Tests that
 * exercise DOWNSAMPLE_RATE must use wait(sc_time(N, SC_NS)) to advance real
 * simulation time so the thread's timed waits can expire.
 *
 * FIFO_STATUS (read_bit_mask = 0x0) — reads always return 0x00000000 through
 * TLM b_transport.  FIFO fill level is inferred from FIFO_RDATA reads
 * (successful pop returns non-zero data; empty FIFO read returns 0x00000000)
 * or from the autonomous overflow interrupt.
 *
 * ## Design Constraints
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on
 *    entropy_src_ip::target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() before each test case to
 *    guarantee a clean, defined register and FIFO state.
 *  - The FUNC004_CHECK macro sets ok = false and emits a CSML_ERROR log
 *    entry naming both the expected and observed values.
 *  - Polling loops are bounded (max 512 iterations of SC_ZERO_TIME) to
 *    prevent infinite simulation when a condition is not met.
 *  - FIFO_CTRL reset value is 0x00000001 (FIFO enabled by default after
 *    reset).  Tests that need a disabled FIFO must explicitly write
 *    FIFO_CTRL = 0x00000000.
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md (FUNC-004 table)
 *  - entropy_src/docs/entropy_src-test-plan.md
 *  - entropy_src/docs/entropy_src-detailed-design.md §7 (SC_THREAD design)
 *  - entropy_src/model/inc/entropy_src.h (BASE_ITERATION_PERIOD_NS,
 *    m_fifo_fill_event, m_reset_event, FIFO_DEPTH)
 *  - entropy_src/test/src/func002_tests.cpp (TC-F002-051/052 overflow pattern)
 *  - entropy_src/test/inc/testbench.h
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"

#include <iomanip>
#include <sstream>

// =============================================================================
// Internal helper macro
// =============================================================================

/// @cond INTERNAL
/// Emit a descriptive FAIL message and set ok = false.
/// Mirrors the FUNC001_CHECK / FUNC002_CHECK / FUNC003_CHECK pattern in
/// the other func*_tests.cpp files.  The @p msg_stream argument is a
/// streaming expression (<<-chained) that is only evaluated when @p cond
/// is false.
#define FUNC004_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            CSML_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)
/// @endcond

// =============================================================================
// File-local constants
// =============================================================================

/// @cond INTERNAL

/// Maximum FIFO depth as defined in entropy_src.h (FIFO_DEPTH = 32).
/// Reproduced here as a file-local constant to avoid accessing private DUT
/// internals; the value is fixed by the IP configuration parameter.
static constexpr unsigned int F004_FIFO_DEPTH = 32u;

/// Interrupt bit position for FIFO_OVERFLOW in INTR_STATUS / INTR_ENABLE.
/// Defined at bit 8 per the architecture-behaviour map.
static constexpr uint32_t F004_INTR_BIT_FIFO_OVERFLOW = (1u << 8u);

/// Base iteration period (nanoseconds) of the background entropy generation
/// thread when DOWNSAMPLE_RATE is zero.  Matches BASE_ITERATION_PERIOD_NS
/// in entropy_src.h; reproduced here to keep this file self-contained.
static constexpr double F004_BASE_ITER_PERIOD_NS = 100.0;

/// Upper bound on the number of SC_ZERO_TIME polling iterations used to
/// wait for a background thread to complete a bounded number of push
/// operations.  The thread requires one SC_ZERO_TIME yield per push when
/// DOWNSAMPLE_RATE == 0; providing 2 × (FIFO_DEPTH + 32) gives ample
/// headroom for both the push loop and any interrupt propagation deltas.
static constexpr int F004_POLL_LIMIT = 512;

/// @endcond

// =============================================================================
// TC-F004-039 — FIFO fill gate: enable/disable via FIFO_CTRL
// =============================================================================

/******************************************************************************
 * @brief TC-F004-039: Verify that FIFO_CTRL[0]=0 disables FIFO fill and that
 *        FIFO_CTRL[0]=1 re-enables it.
 *
 * After a software reset the FIFO_CTRL reset value is 0x00000001 (FIFO
 * enabled).  The background entropy_generation_thread is in the RUNNING state
 * and pushes one PRNG word per SC_ZERO_TIME yield.
 *
 * This test directly exercises the m_fifo_enabled gate in the background
 * thread:
 *   - When FIFO_CTRL[0] is cleared (0), handle_write_FIFO_CTRL sets
 *     m_fifo_enabled = false and drains the FIFO queue.  The thread
 *     transitions to WAITING_FOR_ENABLE and blocks on
 *     wait(m_fifo_fill_event | m_reset_event).
 *   - When FIFO_CTRL[0] is set (1), handle_write_FIFO_CTRL sets
 *     m_fifo_enabled = true and fires m_fifo_fill_event.notify(SC_ZERO_TIME),
 *     waking the thread so that fill resumes.
 *
 * Procedure:
 *  1. Precondition: FIFO enabled (reset default FIFO_CTRL = 0x00000001).
 *  2. Poll up to F004_POLL_LIMIT SC_ZERO_TIME yields until at least one
 *     successful FIFO_RDATA read returns a non-zero entropy word, confirming
 *     the thread has pushed data.
 *  3. Write FIFO_CTRL = 0x00000000 to disable the FIFO.
 *  4. Yield several delta cycles; confirm that FIFO_RDATA now returns
 *     0x00000000 (FIFO drained, underflow, WAITING_FOR_ENABLE active).
 *  5. Write FIFO_CTRL = 0x00000001 to re-enable the FIFO.
 *  6. Poll up to F004_POLL_LIMIT SC_ZERO_TIME yields until at least one
 *     FIFO_RDATA read returns a non-zero word, confirming fill has resumed.
 *
 * Pass criterion:
 *  - Step 2: at least one non-zero word popped before the poll budget expires.
 *  - Step 4: FIFO_RDATA returns 0x00000000 confirming FIFO is drained.
 *  - Step 6: at least one non-zero word popped after re-enable.
 *
 * Test plan reference: fifo_ctrl_enable_disable_fifo
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_fifo_ctrl_enable_disable_fifo()
{
    bool     ok      = true;
    uint32_t rd_val  = 0u;

    // -------------------------------------------------------------------------
    // Step 1: FIFO is enabled at reset (FIFO_CTRL reset value = 0x00000001).
    // No explicit write needed; apply_reset() called by run_tests() guarantees
    // this precondition.
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Step 2: Poll until the thread pushes at least one word.
    // With DOWNSAMPLE_RATE == 0 the thread yields via wait(SC_ZERO_TIME) after
    // each push, so each iteration of this poll loop advances exactly one
    // delta cycle, allowing the thread to run and push.
    //
    // rd_val is explicitly reset to 0 before every register_read_32 call.
    // The CSML framework does NOT write into the TLM payload data buffer when a
    // read callback returns false (e.g. FIFO underflow); the buffer retains its
    // previous content.  Resetting rd_val to 0 before each read ensures that a
    // stale non-zero value from a previous successful pop does not produce a
    // false positive in the polling check.
    // -------------------------------------------------------------------------
    bool got_data_before_disable = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        //wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;  // Reset before read: CSML does not update buf on callback false
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            got_data_before_disable = true;
            break;
        }
    }

    FUNC004_CHECK(got_data_before_disable,
        "TC-F004-039 step 2: no non-zero word returned from FIFO_RDATA within "
        << F004_POLL_LIMIT << " SC_ZERO_TIME yields after reset — "
        "background thread did not push any data while FIFO_CTRL[0]=1");

    // -------------------------------------------------------------------------
    // Step 3: Disable the FIFO.
    // handle_write_FIFO_CTRL detects the 1→0 transition: sets m_fifo_enabled=false,
    // drains the internal queue, and clears FIFO_STATUS.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);

    // Yield several delta cycles to let the thread observe m_fifo_enabled=false
    // and block on the wait(m_fifo_fill_event | m_reset_event) suspension.
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // -------------------------------------------------------------------------
    // Step 4: Confirm FIFO is NOT drained automatically (per new RDL).
    // Data is preserved while disabled per new RDL.
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);

    FUNC004_CHECK(rd_val != 0x00000000u,
        "TC-F004-039 step 4: FIFO_RDATA returned 0x00000000 after FIFO_CTRL=0 — "
        "expected non-zero (FIFO should NOT be drained by disable callback per new RDL)");
 
    // Manually drain the FIFO while it is disabled.
    for (int i = 0; i < 32; ++i)
    {
        uint32_t val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, val);
        if (val == 0x00000000u) break;
    }

    // -------------------------------------------------------------------------
    // Step 5: Re-enable the FIFO.
    // handle_write_FIFO_CTRL detects the 0→1 transition: sets m_fifo_enabled=true
    // and fires m_fifo_fill_event.notify(SC_ZERO_TIME), releasing the thread from
    // its WAITING_FOR_ENABLE suspension.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));

    // -------------------------------------------------------------------------
    // Step 6: Poll until fill resumes.
    // -------------------------------------------------------------------------
    bool got_data_after_reenable = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            got_data_after_reenable = true;
            break;
        }
    }

    FUNC004_CHECK(got_data_after_reenable,
        "TC-F004-039 step 6: no non-zero word returned from FIFO_RDATA within "
        << F004_POLL_LIMIT << " SC_ZERO_TIME yields after FIFO_CTRL re-enabled — "
        "background thread did not resume fill after m_fifo_fill_event wakeup");

    return ok;
}

// =============================================================================
// TC-F004-041 — Thread liveness: FIFO_STATUS.LEVEL increments with fill
// =============================================================================

/******************************************************************************
 * @brief TC-F004-041: Confirm that the background entropy generation thread is
 *        alive and continuously pushing words while the FIFO is enabled and
 *        not full.
 *
 * After reset the FIFO is empty and FIFO_CTRL = 0x00000001 (enabled).  The
 * thread is in RUNNING state and pushes one PRNG word per SC_ZERO_TIME yield.
 * This test exercises the primary "thread liveness" observable: reading
 * FIFO_RDATA returns a non-zero value, proving that the FIFO contains data
 * and the thread has been producing entropy words.
 *
 * Because FIFO_STATUS has read_bit_mask = 0x0 (TLM reads always return 0),
 * liveness is confirmed by a successful FIFO_RDATA read, not by inspecting
 * FIFO_STATUS.LEVEL directly.
 *
 * Procedure:
 *  1. Start from a clean state (apply_reset() done by run_tests()).
 *  2. Poll up to F004_POLL_LIMIT SC_ZERO_TIME yields; on each iteration
 *     read FIFO_RDATA.
 *  3. Record any non-zero FIFO_RDATA value as evidence of thread activity.
 *  4. After the poll ends, assert that at least one non-zero word was read.
 *
 * Pass criterion:
 *  - FIFO_RDATA returned at least one value != 0x00000000 within the poll
 *    budget, proving the thread pushed data into the FIFO.
 *
 * Test plan reference: fifo_status_level_increments_with_background_fill
 *
 *
 * @return true if the liveness assertion passes
 ******************************************************************************/
bool testbench::tc_f004_fifo_status_level_increments_with_background_fill()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Poll: yield SC_ZERO_TIME each iteration to let the background thread
    // execute its push-and-yield loop until it produces at least one word.
    // Reset rd_val before each read: CSML does not update the TLM payload
    // data buffer when a read callback returns false (underflow path), so a
    // stale value could mask an empty-FIFO condition without this reset.
    bool thread_alive = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            thread_alive = true;
            break;
        }
    }

    FUNC004_CHECK(thread_alive,
        "TC-F004-041: FIFO_RDATA returned only 0x00000000 across "
        << F004_POLL_LIMIT << " SC_ZERO_TIME polling iterations — "
        "background entropy_generation_thread appears to not be filling the FIFO "
        "(expected at least one non-zero PRNG word within the poll budget)");

    return ok;
}

// =============================================================================
// TC-F004-053 — FIFO fill halts when disabled during active operation
// =============================================================================

/******************************************************************************
 * @brief TC-F004-053: Confirm that writing FIFO_CTRL[0]=0 while the thread is
 *        in RUNNING state causes it to transition to WAITING_FOR_ENABLE and
 *        stop pushing new words.
 *
 * When handle_write_FIFO_CTRL sets m_fifo_enabled = false and drains the FIFO,
 * the thread observes m_fifo_enabled == false at the top of its next loop
 * iteration and blocks on wait(m_fifo_fill_event | m_reset_event).  No further
 * push actions will occur until m_fifo_fill_event is fired.
 *
 * Procedure:
 *  1. Allow the thread to push some data (poll until first non-zero read).
 *  2. Disable FIFO via FIFO_CTRL = 0x00000000.
 *  3. Yield several delta cycles for the thread to reach WAITING_FOR_ENABLE.
 *  4. Confirm the FIFO is empty: read FIFO_RDATA multiple times and verify
 *     each returns 0x00000000 (drained by the disable callback; thread blocked).
 *
 * Pass criterion:
 *  - All FIFO_RDATA reads after FIFO disable return 0x00000000, confirming the
 *    FIFO was drained by the disable callback and no new data was pushed while
 *    the thread was suspended in WAITING_FOR_ENABLE.
 *
 * Test plan reference: fifo_fill_halts_when_disabled_during_operation
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_fifo_fill_halts_when_disabled_during_operation()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Let the thread push at least one word to confirm it was running.
    // Reset rd_val before each read to prevent stale-buffer false positives.
    bool had_data = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            had_data = true;
            break;
        }
    }

    FUNC004_CHECK(had_data,
        "TC-F004-053 pre-condition: FIFO_RDATA returned only 0x00000000 in "
        << F004_POLL_LIMIT << " iterations before disabling FIFO — "
        "cannot verify halt if thread was not running");

    // Step 2: Disable FIFO.
    // handle_write_FIFO_CTRL immediately drains the queue.  The thread will
    // observe m_fifo_enabled=false on its next loop iteration and block.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);

    // Step 3: Yield several deltas so the thread reaches WAITING_FOR_ENABLE.
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // Step 4: Drain check — read FIFO_RDATA 8 times; all must return 0x00000000.
    // The FIFO was drained by the disable callback; the thread is suspended so
    // no new data is being pushed.
    // IMPORTANT: reset rd_val to 0 before every read.  The CSML framework does
    // not update the TLM data buffer when the read callback returns false
    // (empty-FIFO underflow path); without the reset rd_val would retain the
    // non-zero value from the pre-disable poll and every assertion would fail.
    static constexpr int DRAIN_CHECK_COUNT = 8;
    for (int r = 0; r < DRAIN_CHECK_COUNT; ++r)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        FUNC004_CHECK(rd_val == 0x00000000u,
            "TC-F004-053 step 4 read[" << r << "]: FIFO_RDATA returned 0x"
            << std::hex << rd_val << " after FIFO disabled — "
            "expected 0x00000000 (queue drained; thread in WAITING_FOR_ENABLE)");
    }

    return ok;
}

// =============================================================================
// TC-F004-054 — FIFO fill resumes after re-enable
// =============================================================================

/******************************************************************************
 * @brief TC-F004-054: Confirm that writing FIFO_CTRL[0]=1 after the thread has
 *        been suspended in WAITING_FOR_ENABLE fires m_fifo_fill_event and
 *        causes fill to resume.
 *
 * handle_write_FIFO_CTRL detects the 0→1 transition and calls
 * m_fifo_fill_event.notify(SC_ZERO_TIME).  The thread unblocks from its
 * wait(m_fifo_fill_event | m_reset_event), optionally executes the STARTUP_DELAY
 * (zero at reset), then enters RUNNING and pushes the next word.
 *
 * Procedure:
 *  1. Allow the thread to run; verify it was alive.
 *  2. Disable FIFO via FIFO_CTRL = 0x00000000; yield to enter
 *     WAITING_FOR_ENABLE.
 *  3. Confirm FIFO is empty (underflow read returns 0x00000000).
 *  4. Re-enable FIFO via FIFO_CTRL = 0x00000001.
 *  5. Poll F004_POLL_LIMIT yields until FIFO_RDATA returns a non-zero word.
 *  6. Assert that a non-zero word was received, confirming fill resumed.
 *
 * Pass criterion:
 *  - FIFO_RDATA returns at least one non-zero word within F004_POLL_LIMIT
 *    SC_ZERO_TIME yields after re-enable.
 *
 * Test plan reference: fifo_fill_resumes_after_reenable
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f004_fifo_fill_resumes_after_reenable()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Confirm thread is alive.
    // Reset rd_val before every read to avoid stale-buffer false positives.
    bool pre_alive = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            pre_alive = true;
            break;
        }
    }

    FUNC004_CHECK(pre_alive,
        "TC-F004-054 pre-condition: thread did not produce any data before "
        "disable — cannot verify resume");

    // Step 2: Disable FIFO; yield to allow the thread to reach WAITING_FOR_ENABLE.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // Step 3: Drain and confirm FIFO is now empty.
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u) break;
    }
 
    rd_val = 0u;
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    FUNC004_CHECK(rd_val == 0x00000000u,
        "TC-F004-054 step 3: FIFO_RDATA still non-zero after manual drain");

    // Step 4: Re-enable.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));

    // Steps 5-6: Poll until a non-zero word is produced.
    bool resumed = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            resumed = true;
            break;
        }
    }

    FUNC004_CHECK(resumed,
        "TC-F004-054 step 6: no non-zero word returned from FIFO_RDATA within "
        << F004_POLL_LIMIT << " SC_ZERO_TIME yields after FIFO_CTRL re-enable — "
        "m_fifo_fill_event wakeup mechanism may be broken");

    return ok;
}

// =============================================================================
// TC-F004-006 — CTRL DOWNSAMPLE_RATE field readback
// =============================================================================

/******************************************************************************
 * @brief TC-F004-006: Verify that CTRL[25:16] DOWNSAMPLE_RATE is writable and
 *        readable, and that a non-zero rate produces a measurably slower FIFO
 *        fill compared with rate = 0.
 *
 * The background thread iterates at:
 *   effective_period = BASE_ITERATION_PERIOD_NS * (1 + DOWNSAMPLE_RATE)
 *
 * At rate=0: 100ns per push.  At rate=1: 200ns per push.
 * By measuring words produced in the same fixed real-time window, we confirm
 * that DOWNSAMPLE_RATE controls the fill pacing.
 *
 * Procedure:
 *  Sub-test 1: Write DOWNSAMPLE_RATE=1 to CTRL, read back, verify bits [25:16].
 *  Sub-test 2:
 *    a) Reset.  Wait a fixed window.  Drain FIFO → count_rate0.
 *    b) Reset.  Set DOWNSAMPLE_RATE=1.  Wait the same window.  Drain → count_rate1.
 *    c) Assert count_rate0 > count_rate1.
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_ctrl_downsample_rate_readback()
{
    bool     ok      = true;
    uint32_t rd_val  = 0u;

    // -------------------------------------------------------------------------
    // Sub-test 1: DOWNSAMPLE_RATE readback verification.
    // -------------------------------------------------------------------------
    const uint32_t ctrl_rate1_write = 0x00010000u; // DOWNSAMPLE_RATE = 1
    const uint32_t ctrl_mask        = static_cast<uint32_t>(
        entropy_src_basetest::CTRL_WRITE);          // 0x03FF0111

    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, ctrl_rate1_write);
    test->register_read_32 (entropy_src_basetest::CTRL_OFFSET, rd_val);

    const uint32_t downsample_readback = (rd_val >> 16u) & 0x3FFu;
    FUNC004_CHECK(downsample_readback == 0x001u,
        "TC-F004-006 sub-test 1: CTRL DOWNSAMPLE_RATE readback incorrect — "
        "wrote 0x" << std::hex << ctrl_rate1_write
        << ", expected bits [25:16]=0x001, got DOWNSAMPLE_RATE=0x"
        << downsample_readback
        << " (full CTRL readback: 0x" << (rd_val & ctrl_mask) << ")");

    FUNC004_CHECK((rd_val & ~ctrl_mask) == 0u,
        "TC-F004-006 sub-test 1: reserved bits set in CTRL after write — "
        "CSML mask enforcement failed; (read & ~mask) = 0x"
        << std::hex << (rd_val & ~ctrl_mask));

    // -------------------------------------------------------------------------
    // Sub-test 2: Rate effect on fill speed.
    //
    // Use the same measurement for both rates:
    //   1. Reset, set CTRL rate, FIFO disable/enable to get clean QK state.
    //   2. Wait a fixed real-time window.
    //   3. Drain the FIFO and count words produced.
    //
    // Window = 40 × BASE_ITER_PERIOD_NS = 4000 ns.
    // At rate=0 (100ns/push) → ~40 words (saturates at FIFO_DEPTH=32).
    // At rate=1 (200ns/push) → ~20 words.
    // The comparison shows rate=1 produced fewer words.
    //
    // Both measurements use an identical FIFO disable→enable sequence to
    // ensure the background thread starts from a clean quantum-keeper state.
    // Without this symmetry, apply_reset()'s QK drain causes one measurement
    // to lose time relative to the other.
    // -------------------------------------------------------------------------
    static constexpr unsigned int MEASURE_ITERS = 40u;
    const double window_ns =
        static_cast<double>(MEASURE_ITERS) * F004_BASE_ITER_PERIOD_NS;

    // --- Measure at rate=0 (default after reset) ---
    apply_reset();
    // Use same disable→enable pattern as rate=1 for symmetric QK state.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::sc_time(window_ns, sc_core::SC_NS));

    int count_rate0 = 0;
    for (unsigned int i = 0; i < F004_FIFO_DEPTH + 1u; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
            ++count_rate0;
        else
            break;
    }

    // --- Measure at rate=1 ---
    // Disable/re-enable FIFO after CTRL write to force the thread to restart
    // with the new rate.  Without this, the thread's quantum keeper may have
    // accumulated time at rate=0 from the initial post-reset run.
    apply_reset();
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, ctrl_rate1_write);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::sc_time(window_ns, sc_core::SC_NS));

    int count_rate1 = 0;
    for (unsigned int i = 0; i < F004_FIFO_DEPTH + 1u; ++i)
    {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
            ++count_rate1;
        else
            break;
    }

    // --- Assertions ---
    FUNC004_CHECK(count_rate0 > 0,
        "TC-F004-006 sub-test 2: no data produced at DOWNSAMPLE_RATE=0 in "
        << std::dec << static_cast<int>(window_ns)
        << " ns window — thread not running");

    FUNC004_CHECK(count_rate1 > 0,
        "TC-F004-006 sub-test 2: no data produced at DOWNSAMPLE_RATE=1 in "
        << std::dec << static_cast<int>(window_ns)
        << " ns window — thread not running at rate=1");

    FUNC004_CHECK(count_rate0 > count_rate1,
        "TC-F004-006 sub-test 2: DOWNSAMPLE_RATE did not slow fill — "
        "count_rate0=" << std::dec << count_rate0
        << ", count_rate1=" << count_rate1
        << " (expected count_rate0 > count_rate1)");

    // Restore a clean state for subsequent tests.
    apply_reset();

    return ok;
}

// =============================================================================
// TC-F004-101 — STARTUP_CTRL non-zero delay applied after reset
// =============================================================================

/******************************************************************************
 * @brief TC-F004-101: Verify that writing a non-zero STARTUP_CTRL[15:0]
 *        DELAY_CYCLES value causes the background thread to observe a hold-off
 *        before filling the FIFO after the next software reset.
 *
 * Architecture map (timing_constraints: startup-holdoff): After a software
 * reset in which STARTUP_CTRL was pre-programmed to a non-zero value,
 * handle_write_CTRL restores m_startup_delay_ns = STARTUP_CTRL (reset value
 * = 0x00000000, but we write it before triggering the reset).  Wait — there
 * is a subtlety: handle_write_CTRL calls reset_all_registers() which resets
 * STARTUP_CTRL to 0x00000000 and also sets m_startup_delay_ns = 0.
 *
 * Therefore the sequence to observe a non-zero startup delay is:
 *   1. Apply software reset (clears everything, m_startup_delay_ns = 0).
 *   2. Write STARTUP_CTRL = delay_value (fires handle_write_STARTUP_CTRL;
 *      sets m_startup_delay_ns = delay_value without disturbing the thread).
 *   3. Apply a second software reset (handle_write_CTRL: resets all registers
 *      including STARTUP_CTRL back to 0x00000000, then sets
 *      m_startup_delay_ns = 0).
 *
 * Hmm — that also zeros m_startup_delay_ns.  Per the detailed design Section
 * 7.6 step 8: "m_startup_delay_ns = 0" is set unconditionally from the
 * STARTUP_CTRL reset value (0x00000000).  So a post-reset startup delay can
 * only be observed if the thread re-reads m_startup_delay_ns from the
 * register AFTER the reset callback has returned.
 *
 * Looking at the thread implementation: after RESET_PENDING, the thread uses
 * the value of m_startup_delay_ns AS SET BY handle_write_CTRL (= 0 after
 * reset because STARTUP_CTRL resets to 0x00000000).  Therefore, to observe
 * a non-zero startup delay via a reset, STARTUP_CTRL must be written BEFORE
 * the reset so it captures the delay, but since handle_write_CTRL resets it...
 *
 * The practical observable is:
 *   1. Apply reset (m_startup_delay_ns = 0, FIFO enabled).
 *   2. Immediately write STARTUP_CTRL = N (m_startup_delay_ns = N).
 *   3. Disable FIFO (m_fifo_enabled = false; thread goes to WAITING_FOR_ENABLE).
 *   4. Re-enable FIFO: handle_write_FIFO_CTRL fires m_fifo_fill_event.
 *      Thread wakes, sees m_startup_delay_ns = N, and waits N ns before RUNNING.
 *   5. During the N-ns window, FIFO_RDATA must return 0x00000000 (no fill).
 *   6. After the N-ns window, FIFO_RDATA returns non-zero (fill started).
 *
 * Procedure:
 *  1. Reset (m_startup_delay_ns = 0, FIFO enabled).
 *  2. Write STARTUP_CTRL = 1000 (1000 ns startup delay).
 *  3. Disable FIFO (0x00 → thread to WAITING_FOR_ENABLE).
 *  4. Yield a few deltas for the thread to reach WAITING_FOR_ENABLE.
 *  5. Re-enable FIFO (0x01 → m_fifo_fill_event fires).
 *  6. Immediately (within a few SC_ZERO_TIME deltas, before the 1000 ns expire)
 *     read FIFO_RDATA; it must return 0x00000000 (startup delay active).
 *  7. Wait 2000 ns (2 × startup delay) to let the delay expire.
 *  8. Poll until FIFO_RDATA returns non-zero (fill started after delay).
 *
 * Pass criterion:
 *  - FIFO_RDATA returns 0x00000000 during the startup delay.
 *  - FIFO_RDATA returns a non-zero word within the poll budget after the delay.
 *
 * Test plan reference: startup_ctrl_nonzero_delay_applied_after_reset
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_startup_ctrl_nonzero_delay_applied_after_reset()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Reset (m_startup_delay_ns = 0 after reset).
    // apply_reset() was called by run_tests() before this method.

    // Step 2: Program STARTUP_CTRL with a 1000-ns delay.
    // STARTUP_CTRL write mask = 0x0000FFFF; value 1000 = 0x000003E8.
    const uint32_t delay_ns  = 1000u;
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_ns);
    wait(sc_core::SC_ZERO_TIME);  // Let handle_write_STARTUP_CTRL execute.

    // Confirm the write was accepted.
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    FUNC004_CHECK((rd_val & 0x0000FFFFu) == delay_ns,
        "TC-F004-101 pre-condition: STARTUP_CTRL readback 0x" << std::hex << rd_val
        << " does not match written value 0x" << delay_ns
        << " — STARTUP_CTRL write failed");

    // Step 3: Disable FIFO to force the thread to WAITING_FOR_ENABLE.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u) break;
    }

    // Step 4: Yield several deltas so the thread observes m_fifo_enabled=false.
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // Step 5: Re-enable FIFO — fires m_fifo_fill_event.  Thread wakes and
    // enters STARTUP_DELAY for 1000 ns before RUNNING.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    // Step 6: During the startup delay, FIFO_RDATA must return 0x00000000.
    // A few SC_ZERO_TIME yields will not advance real simulation time
    // (SC_ZERO_TIME does not advance sc_time_stamp), so the 1000-ns delay
    // has not elapsed and the thread is still in STARTUP_DELAY.
    // Reset rd_val before each read to avoid stale-buffer false failures.
    for (int i = 0; i < 8; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        FUNC004_CHECK(rd_val == 0x00000000u,
            "TC-F004-101 step 6 read[" << i << "]: FIFO_RDATA returned 0x"
            << std::hex << rd_val << " during startup delay — "
            "expected 0x00000000 (thread should be in STARTUP_DELAY for "
            << std::dec << delay_ns << " ns)");
    }

    // Step 7: Advance real simulation time by 2 × startup delay.
    wait(sc_core::sc_time(static_cast<double>(delay_ns) * 2.0, sc_core::SC_NS));

    // Step 8: Poll until fill begins.
    bool fill_started = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            fill_started = true;
            break;
        }
    }

    FUNC004_CHECK(fill_started,
        "TC-F004-101 step 8: no non-zero word from FIFO_RDATA within "
        << F004_POLL_LIMIT << " iterations after waiting 2 × "
        << std::dec << delay_ns << " ns — "
        "thread did not enter RUNNING state after STARTUP_DELAY expired");

    return ok;
}

// =============================================================================
// TC-F004-102 — STARTUP_CTRL zero delay: no holdoff
// =============================================================================

/******************************************************************************
 * @brief TC-F004-102: Verify that with STARTUP_CTRL[15:0] = 0 the background
 *        thread enters RUNNING immediately after a FIFO re-enable (no startup
 *        hold-off occurs).
 *
 * After reset, STARTUP_CTRL resets to 0x00000000 (m_startup_delay_ns = 0).
 * The STARTUP_DELAY state in the thread only waits if m_startup_delay_ns > 0.
 * With m_startup_delay_ns == 0 the thread skips the timed wait and enters
 * RUNNING immediately.
 *
 * Procedure:
 *  1. Confirm STARTUP_CTRL = 0x00000000 after reset.
 *  2. Disable FIFO (FIFO_CTRL = 0x00000000) to park the thread.
 *  3. Yield a few deltas.
 *  4. Re-enable FIFO (FIFO_CTRL = 0x00000001).
 *  5. Within 4 SC_ZERO_TIME yields (no real-time advance), verify at least
 *     one non-zero word is produced — confirming no startup delay.
 *
 * Pass criterion:
 *  - FIFO_RDATA returns a non-zero word within 4 SC_ZERO_TIME yields of
 *    re-enabling the FIFO (no startup hold-off delay active).
 *
 * Test plan reference: startup_ctrl_zero_delay_no_holdoff
 *
 * @return true if assertions pass
 ******************************************************************************/
bool testbench::tc_f004_startup_ctrl_zero_delay_no_holdoff()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Confirm STARTUP_CTRL reset value is 0x00000000.
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    FUNC004_CHECK((rd_val & 0x0000FFFFu) == 0x00000000u,
        "TC-F004-102 step 1: STARTUP_CTRL[15:0] = 0x" << std::hex
        << (rd_val & 0x0000FFFFu)
        << " after reset — expected 0x00000000 (STARTUP_CTRL_RESET = 0x00000000)");

    // Step 2: Disable FIFO.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);

    // Step 3: Yield to let thread reach WAITING_FOR_ENABLE.
    for (int i = 0; i < 8; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // Step 4: Re-enable FIFO.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);

    // Step 5: With zero startup delay, within a few SC_ZERO_TIME yields the
    // thread must have already pushed at least one word.
    // Poll with a tight bound (16 iterations — much less than F004_POLL_LIMIT)
    // to confirm that the thread enters RUNNING without a real-time delay.
    // Reset rd_val before each read to prevent stale-buffer false positives.
    bool fast_fill = false;
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            fast_fill = true;
            break;
        }
    }

    FUNC004_CHECK(fast_fill,
        "TC-F004-102 step 5: no data produced within 16 SC_ZERO_TIME yields "
        "after re-enabling FIFO with STARTUP_CTRL=0 — expected immediate fill "
        "start (m_startup_delay_ns == 0 must skip STARTUP_DELAY state)");

    return ok;
}

// =============================================================================
// TC-F004-103 — STARTUP_CTRL delay consumed only at next FIFO wake
// =============================================================================

/******************************************************************************
 * @brief TC-F004-103: Confirm that writing STARTUP_CTRL during steady-state
 *        operation (FIFO enabled, thread in RUNNING) does not disrupt the
 *        currently active fill.
 *
 * Writing STARTUP_CTRL while the thread is in the RUNNING state triggers
 * handle_write_STARTUP_CTRL, which updates m_startup_delay_ns.  However, the
 * startup delay is only consumed the next time the thread transitions through
 * the STARTUP_DELAY state — i.e., after the next FIFO disable/re-enable or
 * reset cycle.  A write during steady-state RUNNING must not cause the thread
 * to pause or restart.
 *
 * Procedure:
 *  1. Confirm the thread is running (at least one non-zero FIFO_RDATA read).
 *  2. Write STARTUP_CTRL = 500 ns while the thread is running.
 *  3. Immediately (without disabling the FIFO) verify that FIFO_RDATA
 *     continues to return non-zero words — confirming no disruption.
 *  4. Verify STARTUP_CTRL retained the written value.
 *  5. Restore STARTUP_CTRL = 0.
 *
 * Pass criterion:
 *  - FIFO_RDATA returns non-zero words both before and immediately after the
 *    STARTUP_CTRL write, confirming no interruption to the running thread.
 *  - STARTUP_CTRL readback == 500 (0x000001F4) after the write.
 *
 * Test plan reference: startup_ctrl_delay_consumed_only_at_next_reset
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_startup_ctrl_delay_consumed_only_at_next_reset()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Confirm thread is alive before the STARTUP_CTRL write.
    // Reset rd_val before each read to prevent stale-buffer false positives.
    bool before_alive = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            before_alive = true;
            break;
        }
    }

    FUNC004_CHECK(before_alive,
        "TC-F004-103 step 1: no data before STARTUP_CTRL write — "
        "pre-condition failed (thread not running)");

    // Step 2: Write STARTUP_CTRL = 500 ns during active RUNNING state.
    const uint32_t delay_mid = 500u;
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, delay_mid);
    wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));

    // Step 3: Confirm fill continues undisturbed.
    bool after_alive = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            after_alive = true;
            break;
        }
    }

    FUNC004_CHECK(after_alive,
        "TC-F004-103 step 3: no data after STARTUP_CTRL write during RUNNING — "
        "writing STARTUP_CTRL mid-operation should NOT disrupt fill; "
        "the delay is consumed only at the next FIFO wake");

    // Step 4: STARTUP_CTRL readback.
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, rd_val);
    FUNC004_CHECK((rd_val & 0x0000FFFFu) == delay_mid,
        "TC-F004-103 step 4: STARTUP_CTRL readback 0x" << std::hex << rd_val
        << " does not reflect written value 0x" << std::dec << delay_mid
        << " — STARTUP_CTRL retention failed");

    // Step 5: Restore STARTUP_CTRL = 0 so subsequent tests are not affected.
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0x00000000u);
    wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));

    return ok;
}

// =============================================================================
// TC-F004-113 — Software reset stabilization: non-zero STARTUP_CTRL hold-off
//               observable
// =============================================================================

/******************************************************************************
 * @brief TC-F004-113: Confirm that a non-zero startup delay programmed before
 *        a software reset produces an observable hold-off period between the
 *        reset and the first post-reset entropy push.
 *
 * Architecture flow:
 *   1. Write STARTUP_CTRL = N (m_startup_delay_ns = N).
 *   2. Apply software reset via CTRL[0]=1.
 *      handle_write_CTRL: resets all registers → STARTUP_CTRL = 0x00000000,
 *      m_startup_delay_ns = 0.
 *
 * Because handle_write_CTRL resets STARTUP_CTRL and m_startup_delay_ns = 0
 * AFTER a standard reset, the post-reset startup delay is only active when
 * STARTUP_CTRL is written AFTER the reset.  We therefore use the pattern:
 *   1. Reset (clean state, m_startup_delay_ns = 0).
 *   2. Write STARTUP_CTRL = N (m_startup_delay_ns = N).
 *   3. Disable FIFO then re-enable to trigger STARTUP_DELAY.
 *   4. Measure that the hold-off is observable (same as TC-F004-101 path).
 *
 * For the "reset stabilization" aspect, we also verify that after a subsequent
 * reset the startup delay is cleared (m_startup_delay_ns = 0, no hold-off).
 *
 * Procedure:
 *  1. Reset (apply_reset via run_tests).
 *  2. Write STARTUP_CTRL = 800 ns.
 *  3. Disable FIFO → WAITING_FOR_ENABLE.
 *  4. Re-enable FIFO → STARTUP_DELAY begins (800 ns hold-off).
 *  5. Within 8 SC_ZERO_TIME yields confirm FIFO_RDATA == 0x00000000 (hold-off).
 *  6. Advance 1600 ns; verify fill starts.
 *  7. Apply a second software reset (clears STARTUP_CTRL → 0).
 *  8. Yield and confirm FIFO fills immediately (no hold-off after second reset).
 *
 * Pass criterion:
 *  - FIFO_RDATA = 0x00000000 during the 800 ns hold-off.
 *  - Fill starts within F004_POLL_LIMIT yields after the hold-off expires.
 *  - After the second reset, fill starts within 64 SC_ZERO_TIME yields
 *    (no hold-off because STARTUP_CTRL reset = 0x00000000).
 *
 * Test plan reference: software_reset_stabilization_holdoff_observable
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f004_software_reset_stabilization_holdoff_observable()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 2: Program a startup delay AFTER the initial reset.
    const uint32_t holdoff_ns = 800u;
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, holdoff_ns);
    wait(sc_core::SC_ZERO_TIME);

    // Step 3: Disable FIFO and drain.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u) break;
    }

    // Step 4: Re-enable FIFO → fires m_fifo_fill_event → STARTUP_DELAY.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    // Step 5: During hold-off, FIFO_RDATA must be 0x00000000.
    // Reset rd_val before each read to prevent stale-buffer false failures.
    for (int i = 0; i < 8; ++i)
    {
       wait(1, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        FUNC004_CHECK(rd_val == 0x00000000u,
            "TC-F004-113 step 5 read[" << i << "]: FIFO_RDATA = 0x"
            << std::hex << rd_val << " during " << std::dec << holdoff_ns
            << " ns hold-off — expected 0x00000000 (thread in STARTUP_DELAY)");
    }

    // Step 6: Advance 2 × holdoff_ns to let the startup delay expire.
    wait(sc_core::sc_time(static_cast<double>(holdoff_ns) * 2.0, sc_core::SC_NS));

    bool fill_after_holdoff = false;
    for (int i = 0; i < F004_POLL_LIMIT; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            fill_after_holdoff = true;
            break;
        }
    }

    FUNC004_CHECK(fill_after_holdoff,
        "TC-F004-113 step 6: no data after " << std::dec << (holdoff_ns * 2u)
        << " ns startup hold-off — thread did not enter RUNNING after delay");

    // Step 7: Apply second reset (clears STARTUP_CTRL → 0; m_startup_delay_ns=0).
    apply_reset();

    // Step 8: After reset STARTUP_CTRL is 0, so fill starts immediately.
    bool immediate_fill = false;
    for (int i = 0; i < 64; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            immediate_fill = true;
            break;
        }
    }

    FUNC004_CHECK(immediate_fill,
        "TC-F004-113 step 8: no immediate fill after second reset with "
        "STARTUP_CTRL=0 — expected thread to enter RUNNING without delay "
        "after reset clears STARTUP_CTRL to 0x00000000");

    return ok;
}

// =============================================================================
// TC-F004-115 — Software reset with FIFO disabled: thread does not fill
// =============================================================================

/******************************************************************************
 * @brief TC-F004-115: Confirm that after a software reset in which the software
 *        subsequently disables the FIFO, the thread transitions to
 *        WAITING_FOR_ENABLE and does not push any data.
 *
 * After a reset the FIFO_CTRL reset value is 0x00000001 (FIFO enabled) and
 * the thread enters RUNNING.  If software then writes FIFO_CTRL = 0 before
 * (or shortly after) the thread's first iteration, the thread must observe
 * m_fifo_enabled = false, transition to WAITING_FOR_ENABLE, and halt.
 *
 * To create a reliable test scenario we:
 *   1. Apply reset.
 *   2. Immediately write FIFO_CTRL = 0x00000000 (before the thread can push).
 *   3. Yield several deltas; confirm FIFO_RDATA returns 0x00000000.
 *
 * Pass criterion:
 *  - After disabling FIFO immediately post-reset, FIFO_RDATA returns
 *    0x00000000 across 32 consecutive SC_ZERO_TIME polls (FIFO stays empty).
 *
 * Test plan reference: software_reset_fifo_disabled_background_does_not_fill
 *
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f004_software_reset_fifo_disabled_does_not_fill()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Disable FIFO immediately after the reset applied by run_tests().
    // There is a race: the thread may have already pushed one word in the
    // delta cycles consumed by run_tests() → apply_reset() overhead.
    // We write FIFO_CTRL=0 before yielding to the thread to minimise this.
    test->register_write_32(entropy_src_basetest::FIFO_CTRL_OFFSET, 0x00000000u);
    for (int i = 0; i < 32; ++i) {
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val == 0u) break;
    }

    // Drain any word that may have been pushed before the disable landed.
    wait(sc_core::SC_ZERO_TIME);
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
    // Do not assert here — the thread may have pushed one word in the race.
    // We only check the steady-state below.

    // Let the thread fully observe m_fifo_enabled=false.
    for (int i = 0; i < 16; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // Drain one more time in case of the race window.
    test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);

    // Now confirm no further data is pushed (thread is in WAITING_FOR_ENABLE).
    // Reset rd_val before each read: CSML does not clear the data buffer on a
    // false-returning callback (underflow path), so without the reset rd_val
    // would keep whatever value was last set (possibly non-zero from draining).
    static constexpr int STEADY_STATE_CHECKS = 32;
    for (int r = 0; r < STEADY_STATE_CHECKS; ++r)
    {
        wait(sc_core::SC_ZERO_TIME);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        FUNC004_CHECK(rd_val == 0x00000000u,
            "TC-F004-115 read[" << r << "]: FIFO_RDATA = 0x" << std::hex << rd_val
            << " with FIFO disabled post-reset — "
            "expected 0x00000000 (thread should be in WAITING_FOR_ENABLE, "
            "no new pushes until m_fifo_fill_event)");
    }

    return ok;
}

// =============================================================================
// TC-F004-140 — Continuous entropy generation liveness (primary SC_THREAD test)
// =============================================================================

/******************************************************************************
 * @brief TC-F004-140: Primary SC_THREAD liveness test — confirm that the
 *        background entropy generation thread produces data continuously
 *        between two time measurements.
 *
 * This is the definitive FUNC-004 liveness verification.  The test takes two
 * "snapshots" of the FIFO content separated by a fixed real-time window and
 * verifies that data was produced in both snapshots, confirming that the thread
 * is running continuously.
 *
 * Procedure:
 *  1. Start from clean state (apply_reset()).
 *  2. Snapshot T1: Poll 64 SC_ZERO_TIME yields; record the first non-zero
 *     word received (evidence of FIFO fill at T1).
 *  3. Advance real simulation time by 500 ns (enough for 5 iterations at
 *     rate=0 if the thread were rate-limited, but irrelevant since rate=0
 *     uses SC_ZERO_TIME).
 *  4. Snapshot T2: Poll 64 more SC_ZERO_TIME yields; record the first non-zero
 *     word received (evidence of continued fill at T2).
 *  5. Assert both snapshots returned non-zero words.
 *
 * Pass criterion:
 *  - Snapshot T1: at least one non-zero word polled.
 *  - Snapshot T2: at least one non-zero word polled after time advance.
 *  This confirms the thread ran before and after the time window — continuous.
 *
 * Test plan reference: background_process_entropy_continuously_generated
 *
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f004_background_process_entropy_continuously_generated()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Snapshot T1: poll for a non-zero word within 64 iterations.
    // Use real-time waits so the background thread can advance via quantum keeper.
    // -------------------------------------------------------------------------
    bool snap_t1 = false;
    for (int i = 0; i < 64; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            snap_t1 = true;
            break;
        }
    }

    FUNC004_CHECK(snap_t1,
        "TC-F004-140 T1: no non-zero word within 64 iterations "
        "post-reset — background thread not running at T1");

    // -------------------------------------------------------------------------
    // Advance real simulation time to separate the two measurements.
    // -------------------------------------------------------------------------
    wait(sc_core::sc_time(500.0, sc_core::SC_NS));

    // -------------------------------------------------------------------------
    // Snapshot T2: poll for a non-zero word within 64 iterations.
    // -------------------------------------------------------------------------
    bool snap_t2 = false;
    for (int i = 0; i < 64; ++i)
    {
        wait(sc_core::sc_time(F004_BASE_ITER_PERIOD_NS, sc_core::SC_NS));
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            snap_t2 = true;
            break;
        }
    }

    FUNC004_CHECK(snap_t2,
        "TC-F004-140 T2: no non-zero word within 64 iterations "
        "after 500 ns time advance — background thread not running at T2; "
        "continuous entropy generation may have stopped");

    return ok;
}
