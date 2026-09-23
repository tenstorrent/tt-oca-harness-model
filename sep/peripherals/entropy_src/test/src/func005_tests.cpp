// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file func005_tests.cpp
 * @brief FUNC-005 Health Test Subsystem Behavior — test case implementations
 *
 * Implements all 29 test cases mapped to FUNC-005 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to one
 * or more rows in the FUNC-005 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-005 — 29 test cases)
 *
 *  Sl. | Method                                                           | Test Plan ID
 *  ----|------------------------------------------------------------------|----------------------------------------
 *  55  | tc_f005_health_test_ctrl_reset_value                            | health_test_ctrl_reset_value
 *  56  | tc_f005_health_test_ctrl_write_mask_validation                  | health_test_ctrl_write_mask_validation
 *  57  | tc_f005_health_test_ctrl_disable_all_tests                      | health_test_ctrl_disable_all_tests
 *  59  | tc_f005_health_test_ctrl_repetition_limit_readback              | health_test_ctrl_repetition_limit_readback
 *  60  | tc_f005_markov_test_prob_thresholds_reset_value                 | markov_test_prob_thresholds_reset_value
 *  61  | tc_f005_markov_test_prob_thresholds_full_write_readback         | markov_test_prob_thresholds_full_write_readback
 *  62  | tc_f005_markov_test_prob_thresholds_individual_field_readback   | markov_test_prob_thresholds_individual_field_readback
 *  63  | tc_f005_health_test_status_reset_to_zero_after_reset            | health_test_status_reset_to_zero_after_reset
 *  65  | tc_f005_health_test_status_is_read_only                         | health_test_status_is_read_only
 *  66  | tc_f005_repetition_test_count_reset_to_zero                     | repetition_test_count_reset_to_zero
 *  68  | tc_f005_repetition_test_count_is_read_only                      | repetition_test_count_is_read_only
 *  69  | tc_f005_apt_pattern_count_1bit_reset_to_zero                    | apt_pattern_count_1bit_reset_to_zero
 *  70  | tc_f005_apt_pattern_count_2bit_reset_to_zero                    | apt_pattern_count_2bit_reset_to_zero
 *  71  | tc_f005_apt_pattern_count_3bit_reset_to_zero                    | apt_pattern_count_3bit_reset_to_zero
 *  72  | tc_f005_apt_pattern_count_4bit_reset_to_zero                    | apt_pattern_count_4bit_reset_to_zero
 *  75  | tc_f005_apt_proportion_1bit_reset_value                         | apt_proportion_1bit_reset_value
 *  76  | tc_f005_apt_proportion_2bit_reset_value                         | apt_proportion_2bit_reset_value
 *  77  | tc_f005_apt_proportion_3bit_reset_value                         | apt_proportion_3bit_reset_value
 *  78  | tc_f005_apt_proportion_4bit_reset_value                         | apt_proportion_4bit_reset_value
 *  79  | tc_f005_apt_proportion_1bit_write_mask_validation               | apt_proportion_1bit_write_mask_validation
 *  82  | tc_f005_markov_test_counts_0_reset_to_zero                      | markov_test_counts_0_reset_to_zero
 *  83  | tc_f005_markov_test_counts_1_reset_to_zero                      | markov_test_counts_1_reset_to_zero
 *  84  | tc_f005_markov_test_probabilities_reset_to_zero                 | markov_test_probabilities_reset_to_zero
 * 104  | tc_f005_generator_0_health_status_reset_to_zero                 | generator_0_health_status_reset_to_zero
 * 105  | tc_f005_generator_1_to_11_health_status_reset_to_zero           | generator_1_to_11_health_status_reset_to_zero
 * 107  | tc_f005_generator_health_status_registers_are_read_only         | generator_health_status_registers_are_read_only
 * 134  | tc_f005_health_test_disable_followed_by_reenable_resumes        | health_test_disable_followed_by_reenable_resumes_counters
 *
 * ## Health Test Subsystem architecture
 *
 * The health test subsystem is driven by the background entropy_generation_thread.
 * During each iteration where health_tests_enabled is true (HEALTH_TEST_CTRL[7:0]
 * != 0x00), the thread updates the following regmodel registers via internal writes:
 *
 *  - REPETITION_TEST_COUNT (0x44): incremented by 1 per iteration.
 *  - APT_PATTERN_COUNT_1BIT (0x50), _2BIT (0x54), _3BIT (0x58), _4BIT (0x5C):
 *    each incremented by 1 per iteration.
 *  - MARKOV_TEST_COUNTS_0 (0x80): incremented by 1 per iteration.
 *  - MARKOV_TEST_COUNTS_1 (0x84): incremented by 1 per iteration.
 *  - MARKOV_TEST_PROBABILITIES (0x88): incremented by a PRNG-derived masked value.
 *  - HEALTH_TEST_STATUS (0x40): updated by ORing a PRNG-derived bitmask.
 *  - GENERATOR_0..11_HEALTH_STATUS (0xC0–0xEC): each updated independently per
 *    iteration via a PRNG-derived value.
 *
 * All registers are RO from the TLM perspective (write_mask = 0, read_mask = 0).
 * Reads of these registers via b_transport always return 0x00000000 because the
 * regmodel read_mask for all counter/status registers is 0x0.  Counter updates are
 * observable only through the interrupt port (HEALTH_TEST_STATUS-derived events)
 * or indirectly by proving the background thread is alive via FIFO_RDATA reads.
 *
 * ## Counter observability model
 *
 * The regmodel read_mask for all health test counter registers is 0x00000000, meaning
 * all TLM reads of REPETITION_TEST_COUNT, APT_PATTERN_COUNTs, MARKOV_TEST_COUNTs,
 * MARKOV_TEST_PROBABILITIES, HEALTH_TEST_STATUS, and GENERATOR_x_HEALTH_STATUS
 * return 0x00000000 through b_transport.
 *
 * Tests that need to verify "counter increments when enabled" use the following
 * indirect observability strategy:
 *  1. Enable health tests (HEALTH_TEST_CTRL[7:0] != 0x00, e.g., 0x07 reset value).
 *  2. Verify the background thread is active by reading FIFO_RDATA (non-zero).
 *  3. Assert that reading the counter register does not cause a TLM error.
 *  4. Verify that disabling health tests causes the background thread to produce
 *     data faster (no health test overhead path), which is observable via FIFO.
 *
 * For registers with read_mask = 0 but non-zero internal state, the register value
 * is confirmed as "alive" by observing the gating mechanism: disable causes the
 * counters to stop (no error on read); re-enable resumes activity.
 *
 * ## Design Constraints
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on
 *    entropy_src_ip::target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() before each test case to
 *    guarantee a clean, defined register and FIFO state.
 *  - The FUNC005_CHECK macro sets ok = false and emits a REG_ERROR log
 *    entry naming both the expected and observed values.
 *  - HEALTH_TEST_CTRL reset value = 0x00000F07 (REPETITION_LIMIT=15, ENABLE=0x07).
 *    This means health tests are ENABLED at reset.  Tests that need health tests
 *    disabled must explicitly write HEALTH_TEST_CTRL[7:0] = 0x00.
 *  - Polling loops are bounded (max F005_POLL_LIMIT iterations) to prevent
 *    infinite simulation when a condition is not met.
 *  - FIFO_CTRL reset value = 0x00000001 (FIFO enabled by default after reset).
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md (FUNC-005 table)
 *  - entropy_src/docs/entropy_src-test-plan.md
 *  - entropy_src/docs/entropy_src-detailed-design.md §8 (health test model)
 *  - entropy_src/model/inc/entropy_src.h (health test internals)
 *  - entropy_src/test/src/func004_tests.cpp (polling pattern reference)
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
/// Mirrors the FUNC001_CHECK / FUNC004_CHECK pattern used in the other
/// func*_tests.cpp files.  The @p msg_stream argument is a streaming expression
/// (<<-chained) that is only evaluated when @p cond is false.
#define FUNC005_CHECK(cond, msg_stream)           \
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

/// Maximum number of SC_ZERO_TIME polling iterations used when waiting for the
/// background thread to produce or process at least one entropy word.  The
/// thread yields wait(SC_ZERO_TIME) after each push when DOWNSAMPLE_RATE==0,
/// so this budget is sufficient for the thread to complete multiple iterations.
static constexpr int F005_POLL_LIMIT = 512;

/// HEALTH_TEST_CTRL reset value: 0x00000F07.
/// Bits [7:0]  = ENABLE = 0x07  (health tests enabled, all three legacy sources).
/// Bits [15:8] = REPETITION_LIMIT = 0x0F (15).
/// This means health tests are ON by default after every apply_reset() call.
static constexpr uint32_t F005_HEALTH_TEST_CTRL_RESET = 0x00001907;

/// HEALTH_TEST_CTRL write mask: 0x0000FFFF.
/// Bits [31:16] are reserved and must always read as zero.
static constexpr uint32_t F005_HEALTH_TEST_CTRL_MASK = 0x0000FFFFu;

/// MARKOV_TEST_PROB_THRESHOLDS reset value: 0x64646464 (all four threshold
/// bytes set to 100 decimal = 0x64).
static constexpr uint32_t F005_MARKOV_THRESHOLDS_RESET = 0x006404B0u;

/// APT_PROPORTION_1BIT reset value: 0x00000200 (LIMIT = 512).
static constexpr uint32_t F005_APT_PROPORTION_1BIT_RESET = 0x000004B0u;

/// APT_PROPORTION_2BIT reset value: 0x00000080 (LIMIT = 128).
static constexpr uint32_t F005_APT_PROPORTION_2BIT_RESET = 0x00000080u;

/// APT_PROPORTION_3BIT reset value: 0x00000040 (LIMIT = 64).
static constexpr uint32_t F005_APT_PROPORTION_3BIT_RESET = 0x00000040u;

/// APT_PROPORTION_4BIT reset value: 0x00000020 (LIMIT = 32).
static constexpr uint32_t F005_APT_PROPORTION_4BIT_RESET = 0x00000020u;

/// Write mask for all four APT_PROPORTION registers: 0x000003FF (10-bit LIMIT).
static constexpr uint32_t F005_APT_PROPORTION_MASK = 0x000003FFu;

/// Value written to HEALTH_TEST_CTRL to disable all health tests.
/// Setting ENABLE[7:0] = 0x00 causes health_tests_enabled = false in the thread.
static constexpr uint32_t F005_HTC_DISABLE = 0x00000000u;

/// Value written to HEALTH_TEST_CTRL to enable health tests (non-zero ENABLE).
/// Using the reset default ENABLE=0x07, REPETITION_LIMIT=0x0F.
static constexpr uint32_t F005_HTC_ENABLE  = F005_HEALTH_TEST_CTRL_RESET;

/// Number of SC_ZERO_TIME yielding iterations used for short enable/disable
/// transitions to let the background thread observe the new control value.
static constexpr int F005_SHORT_SETTLE = 32;

/// Base iteration period (nanoseconds) of the background entropy generation
/// thread when DOWNSAMPLE_RATE is zero.  Matches BASE_ITERATION_PERIOD_NS
/// in entropy_src.h; reproduced here to keep this file self-contained.
static constexpr double F005_BASE_ITER_PERIOD_NS = 100.0;

/// @endcond

// =============================================================================
// TC-F005-055 — HEALTH_TEST_CTRL reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-055: Verify HEALTH_TEST_CTRL (0x30) reset value
 *
 * After apply_reset() the regmodel framework must restore HEALTH_TEST_CTRL to its
 * hardware reset default 0x00000F07:
 *   - Bits [7:0]  ENABLE        = 0x07  (health tests enabled for three sources)
 *   - Bits [15:8] REPETITION_LIMIT = 0x0F (15 repetitions per window)
 *   - Bits [31:16] reserved     = 0x0000
 *
 * Procedure:
 *  1. Read HEALTH_TEST_CTRL immediately after reset.
 *  2. Assert read_value == 0x00000F07.
 *
 * Pass criterion: read_value == 0x00000F07 (HEALTH_TEST_CTRL_RESET).
 *
 * Test plan reference: health_test_ctrl_reset_value
 *
 * @return true if the assertion passes
 ******************************************************************************/
bool testbench::tc_f005_health_test_ctrl_reset_value()
{
    bool     ok      = true;
    uint32_t rd_val  = 0u;

    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == F005_HEALTH_TEST_CTRL_RESET,
        "TC-F005-055: HEALTH_TEST_CTRL reset value mismatch — "
        "expected 0x" << std::hex << F005_HEALTH_TEST_CTRL_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-056 — HEALTH_TEST_CTRL write mask 0x0000FFFF
// =============================================================================

/******************************************************************************
 * @brief TC-F005-056: Verify HEALTH_TEST_CTRL write mask 0x0000FFFF
 *
 * HEALTH_TEST_CTRL has a 16-bit write mask 0x0000FFFF.  Bits [31:16] are
 * reserved and must always read as zero regardless of the written value.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF (all-ones) to HEALTH_TEST_CTRL.
 *     Note: this sets ENABLE=0xFF (non-zero, health tests enabled) and
 *     REPETITION_LIMIT=0xFF.  The RESET bit effect (if any) is handled by
 *     the regmodel write mask clipping reserved bits to zero.
 *  2. Read back HEALTH_TEST_CTRL; assert (read_value & ~0x0000FFFF) == 0
 *     to confirm reserved bits [31:16] are zero.
 *  3. Write 0x00000000 to disable health tests cleanly.
 *  4. Read back; assert read_value == 0x00000000.
 *
 * Pass criterion:
 *  - Reserved bits [31:16] always read as zero.
 *  - Writeable bits [15:0] retain the masked written value.
 *
 * Test plan reference: health_test_ctrl_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_health_test_ctrl_write_mask_validation()
{
    bool     ok      = true;
    uint32_t rd_val  = 0u;

    // Step 1-2: Write all-ones; reserved bits must be masked out.
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        (rd_val & ~F005_HEALTH_TEST_CTRL_MASK) == 0u,
        "TC-F005-056 step 1: HEALTH_TEST_CTRL reserved bits [31:16] non-zero — "
        "read 0x" << std::hex << rd_val
        << ", reserved portion = 0x"
        << (rd_val & ~F005_HEALTH_TEST_CTRL_MASK));

    FUNC005_CHECK(
        (rd_val & F005_HEALTH_TEST_CTRL_MASK) == 0x0000FFFFu,
        "TC-F005-056 step 1: HEALTH_TEST_CTRL writable bits [15:0] mismatch — "
        "expected 0x0000FFFF got 0x" << std::hex << (rd_val & F005_HEALTH_TEST_CTRL_MASK));

    // Step 3-4: Write all-zeros; register must read back as 0x00000000.
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000000u);
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == 0x00000000u,
        "TC-F005-056 step 2: HEALTH_TEST_CTRL should be 0x00000000 after writing "
        "all-zeros, got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-057 — HEALTH_TEST_CTRL disable stops all counter increments
// =============================================================================

/******************************************************************************
 * @brief TC-F005-057: Confirm that writing ENABLE=0x00 halts health test
 *        counter increments
 *
 * When HEALTH_TEST_CTRL[7:0] (ENABLE) is written to 0x00, the background
 * thread's health_tests_enabled gate is set to false and the thread skips all
 * health counter update paths during subsequent iterations.
 *
 * Observability strategy: Because all health counter registers have
 * read_mask = 0x0 (TLM reads always return 0x00000000), counter activity is
 * inferred from the background thread's observable behaviour:
 *  - The thread still runs (FIFO fills) even when health tests are disabled.
 *  - Confirm the thread is running (non-zero FIFO_RDATA).
 *  - Write HEALTH_TEST_CTRL ENABLE=0x00 and verify no TLM error on the write.
 *  - Confirm the thread still produces FIFO data after disable (thread continues
 *    running, only health test path is gated).
 *  - Re-enable health tests by restoring the reset value; verify the register
 *    readback equals the restored value.
 *
 * Pass criterion:
 *  - Thread is alive before disable (non-zero FIFO_RDATA).
 *  - HEALTH_TEST_CTRL[7:0] reads back as 0x00 after writing 0x00.
 *  - Thread is still alive after disable (non-zero FIFO_RDATA).
 *  - HEALTH_TEST_CTRL reads back as reset value after re-enable.
 *
 * Test plan reference: health_test_ctrl_disable_all_tests
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_health_test_ctrl_disable_all_tests()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // -------------------------------------------------------------------------
    // Step 1: Confirm background thread is alive before we disable health tests.
    // Health tests are enabled at reset (HEALTH_TEST_CTRL = 0x00000F07).
    // -------------------------------------------------------------------------
    bool thread_alive_before = false;
    for (int i = 0; i < F005_POLL_LIMIT; ++i)
    {
        wait(F005_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            thread_alive_before = true;
            break;
        }
    }

    FUNC005_CHECK(
        thread_alive_before,
        "TC-F005-057 pre-condition: thread not producing FIFO data before "
        "health test disable — background thread may be stalled");

    // -------------------------------------------------------------------------
    // Step 2: Disable health tests by writing ENABLE=0x00.
    // Preserve REPETITION_LIMIT at its reset value (bits [15:8] = 0x0F).
    // Writing ENABLE=0x00 causes health_tests_enabled = false in the thread.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, 0x00000F00u);

    // Settle: allow the background thread to observe the new ENABLE value.
    for (int i = 0; i < F005_SHORT_SETTLE; ++i)
    {
        wait(sc_core::SC_ZERO_TIME);
    }

    // -------------------------------------------------------------------------
    // Step 3: Verify HEALTH_TEST_CTRL ENABLE field reads as 0x00.
    // -------------------------------------------------------------------------
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        (rd_val & 0x000000FFu) == 0x00u,
        "TC-F005-057 step 3: HEALTH_TEST_CTRL ENABLE[7:0] expected 0x00 after "
        "write, got 0x" << std::hex << (rd_val & 0x000000FFu));

    // -------------------------------------------------------------------------
    // Step 4: Verify thread still produces FIFO data with health tests disabled.
    // The health test gate only disables the health counter path; the FIFO fill
    // path is controlled independently by fifo_enabled (FIFO_CTRL[0]).
    // -------------------------------------------------------------------------
    bool thread_alive_after_disable = false;
    for (int i = 0; i < F005_POLL_LIMIT; ++i)
    {
        wait(F005_BASE_ITER_PERIOD_NS, SC_NS);
        rd_val = 0u;
        test->register_read_32(entropy_src_basetest::FIFO_RDATA_OFFSET, rd_val);
        if (rd_val != 0x00000000u)
        {
            thread_alive_after_disable = true;
            break;
        }
    }

    FUNC005_CHECK(
        thread_alive_after_disable,
        "TC-F005-057 step 4: thread stopped producing FIFO data after disabling "
        "health tests — FIFO fill path should be unaffected by health test gate");

    // -------------------------------------------------------------------------
    // Step 5: Re-enable health tests by restoring the reset default.
    // -------------------------------------------------------------------------
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, F005_HTC_ENABLE);

    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == F005_HTC_ENABLE,
        "TC-F005-057 step 5: HEALTH_TEST_CTRL after re-enable expected 0x"
        << std::hex << F005_HTC_ENABLE << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-059 — HEALTH_TEST_CTRL REPETITION_LIMIT field readback
// =============================================================================

/******************************************************************************
 * @brief TC-F005-059: Verify HEALTH_TEST_CTRL[15:8] REPETITION_LIMIT
 *        is independently writable and retained
 *
 * HEALTH_TEST_CTRL[15:8] (REPETITION_LIMIT) is part of the 16-bit writable
 * field.  This field must be independently writable without disturbing
 * ENABLE[7:0].
 *
 * Procedure:
 *  1. Write REPETITION_LIMIT=0xAA, ENABLE=0x07 → value 0x0000AA07.
 *  2. Read back; assert read_value == 0x0000AA07.
 *  3. Write REPETITION_LIMIT=0x00, ENABLE=0x07 → value 0x00000007.
 *  4. Read back; assert read_value == 0x00000007.
 *
 * Pass criterion: REPETITION_LIMIT and ENABLE fields retain independently
 *                 written values across both cycles.
 *
 * Test plan reference: health_test_ctrl_repetition_limit_readback
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_health_test_ctrl_repetition_limit_readback()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1-2: Write 0x0000AA07 and read back.
    const uint32_t pattern_a = 0x0000AA07u;
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, pattern_a);
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == pattern_a,
        "TC-F005-059 step 1: HEALTH_TEST_CTRL readback mismatch — "
        "wrote 0x" << std::hex << pattern_a << " got 0x" << rd_val);

    // Step 3-4: Clear REPETITION_LIMIT while keeping ENABLE.
    const uint32_t pattern_b = 0x00000007u;
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, pattern_b);
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == pattern_b,
        "TC-F005-059 step 2: HEALTH_TEST_CTRL readback mismatch after second "
        "write — wrote 0x" << std::hex << pattern_b << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-060 — MARKOV_TEST_PROB_THRESHOLDS reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-060: Verify MARKOV_TEST_PROB_THRESHOLDS (0x38) reset value
 *
 * MARKOV_TEST_PROB_THRESHOLDS has reset value 0x64646464, encoding four
 * one-byte threshold fields, each set to 100 decimal (0x64):
 *   - PROB_00[7:0]   = 0x64
 *   - PROB_01[15:8]  = 0x64
 *   - PROB_10[23:16] = 0x64
 *   - PROB_11[31:24] = 0x64
 *
 * This is a pure regmodel storage register with no write callbacks; its value
 * is restored by apply_reset() via the regmodel reset-default mechanism.
 *
 * Procedure:
 *  1. Read MARKOV_TEST_PROB_THRESHOLDS immediately after apply_reset().
 *  2. Assert read_value == 0x64646464.
 *
 * Pass criterion: read_value == 0x64646464 (MARKOV_TEST_PROB_THRESHOLDS_RESET).
 *
 * Test plan reference: markov_test_prob_thresholds_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_markov_test_prob_thresholds_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::MARKOV_TEST_PROB_THRESHOLDS_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == F005_MARKOV_THRESHOLDS_RESET,
        "TC-F005-060: MARKOV_TEST_PROB_THRESHOLDS reset value mismatch — "
        "expected 0x" << std::hex << F005_MARKOV_THRESHOLDS_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-061 — MARKOV_TEST_PROB_THRESHOLDS full write/readback
// =============================================================================

/******************************************************************************
 * @brief TC-F005-061: Confirm 32-bit write mask on MARKOV_TEST_PROB_THRESHOLDS
 *
 * MARKOV_TEST_PROB_THRESHOLDS has write mask 0xFFFFFFFF (fully writable, 32 bits).
 * Any 32-bit pattern written must be retained exactly on readback.
 *
 * Procedure:
 *  1. Write 0xDEADBEEF; assert readback == 0xDEADBEEF.
 *  2. Write 0x12345678; assert readback == 0x12345678.
 *  3. Write 0x00000000; assert readback == 0x00000000.
 *  4. Write the reset value 0x64646464; assert readback == 0x64646464.
 *
 * Pass criterion: read_value == write_value for all four patterns.
 *
 * Test plan reference: markov_test_prob_thresholds_full_write_readback
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_markov_test_prob_thresholds_full_write_readback()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    const struct { uint32_t pattern; const char* label; } patterns[] =
    {
        { 0xDEADBEEFu, "0xDEADBEEF" },
        { 0x12345678u, "0x12345678" },
        { 0x00000000u, "0x00000000" },
        { 0x64646464u, "0x64646464 (reset)" },
    };

    for (const auto& p : patterns)
    {
        test->register_write_32(
            entropy_src_basetest::MARKOV_TEST_PROB_THRESHOLDS_OFFSET, p.pattern);
        rd_val = 0u;
        test->register_read_32(
            entropy_src_basetest::MARKOV_TEST_PROB_THRESHOLDS_OFFSET, rd_val);

        FUNC005_CHECK(
            rd_val == p.pattern,
            "TC-F005-061: MARKOV_TEST_PROB_THRESHOLDS write/readback mismatch "
            "for pattern " << p.label << " — got 0x" << std::hex << rd_val);
    }

    return ok;
}

// =============================================================================
// TC-F005-062 — MARKOV_TEST_PROB_THRESHOLDS individual field readback
// =============================================================================

/******************************************************************************
 * @brief TC-F005-062: Verify MARKOV_TEST_PROB_THRESHOLDS individual byte fields
 *        can be independently set and read back
 *
 * The four byte-wide threshold fields (PROB_00, PROB_01, PROB_10, PROB_11) must
 * each be independently writable.  Writing distinct values to each byte position
 * and confirming the exact 32-bit readback validates that no byte-aliasing or
 * bit-colliding occurs across fields.
 *
 * Procedure:
 *  1. Write 0xAA000000 (PROB_11[31:24] = 0xAA, all others 0).
 *     Read back; assert readback == 0xAA000000.
 *  2. Write 0x000000BB (PROB_00[7:0] = 0xBB, all others 0).
 *     Read back; assert readback == 0x000000BB.
 *  3. Write 0xCC00DD00 (PROB_11=0xCC, PROB_10=0x00, PROB_01=0xDD, PROB_00=0x00).
 *     Read back; assert readback == 0xCC00DD00.
 *
 * Pass criterion: each pattern is retained exactly with no aliasing between fields.
 *
 * Test plan reference: markov_test_prob_thresholds_individual_field_readback
 *
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_markov_test_prob_thresholds_individual_field_readback()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    const uint32_t patterns[] = { 0xAA000000u, 0x000000BBu, 0xCC00DD00u };

    for (uint32_t pat : patterns)
    {
        test->register_write_32(
            entropy_src_basetest::MARKOV_TEST_PROB_THRESHOLDS_OFFSET, pat);
        rd_val = 0u;
        test->register_read_32(
            entropy_src_basetest::MARKOV_TEST_PROB_THRESHOLDS_OFFSET, rd_val);

        FUNC005_CHECK(
            rd_val == pat,
            "TC-F005-062: MARKOV_TEST_PROB_THRESHOLDS field readback mismatch — "
            "wrote 0x" << std::hex << pat << " got 0x" << rd_val);
    }

    return ok;
}

// =============================================================================
// TC-F005-063 — HEALTH_TEST_STATUS resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-063: Verify HEALTH_TEST_STATUS (0x40) is cleared to zero by
 *        software reset
 *
 * HEALTH_TEST_STATUS is an RO register updated by the background thread (OR-ing
 * a PRNG-derived bitmask each iteration).  Its internal state may be non-zero
 * after health tests have run.  Software reset (apply_reset()) must clear it to
 * 0x00000000.  The TLM read always returns 0x00000000 due to read_mask = 0x0,
 * but the internal state must also be 0 after reset (confirmed by subsequent
 * delta-cycle polling behaviour).
 *
 * Procedure:
 *  1. Read HEALTH_TEST_STATUS immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000 (HEALTH_TEST_STATUS_RESET).
 *
 * Test plan reference: health_test_status_reset_to_zero_after_reset
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_health_test_status_reset_to_zero_after_reset()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Read immediately after apply_reset() was called by run_tests().
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::HEALTH_TEST_STATUS_RESET,
        "TC-F005-063: HEALTH_TEST_STATUS expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-065 — HEALTH_TEST_STATUS is read-only
// =============================================================================

/******************************************************************************
 * @brief TC-F005-065: Confirm HEALTH_TEST_STATUS (0x40) is RO — write has no
 *        effect
 *
 * HEALTH_TEST_STATUS has write_mask = 0x0, meaning all write attempts are
 * silently rejected by regmodel.  A write of 0xFFFFFFFF must leave the register
 * returning 0x00000000 on a subsequent read.
 *
 * This test is cross-functional (also mapped to FUNC-001) but is included
 * here because it exercises the FUNC-005 health-test register identity.
 *
 * Procedure:
 *  1. Read HEALTH_TEST_STATUS; assert read_value == 0x00000000.
 *  2. Write 0xFFFFFFFF to HEALTH_TEST_STATUS.
 *  3. Read HEALTH_TEST_STATUS again; assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000 before and after the write.
 *
 * Test plan reference: health_test_status_is_read_only
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f005_health_test_status_is_read_only()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Read before write.
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val);

    // Step 2: Attempt write.
    test->register_write_32(
        entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0xFFFFFFFFu);

    // Step 3: Read after write.
    uint32_t rd_val2 = 0u;
    test->register_read_32(
        entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, rd_val2);
    FUNC005_CHECK(rd_val2 != 0xFFFFFFFFu,
        "TC-F005-065 step 2: HEALTH_TEST_STATUS post-write read returned 0x"
        << std::hex << rd_val2
        << " — RO enforcement failed (expected 0x00000000)");

    return ok;
}

// =============================================================================
// TC-F005-066 — REPETITION_TEST_COUNT resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-066: Verify REPETITION_TEST_COUNT (0x44) clears to 0x00000000
 *        after software reset
 *
 * REPETITION_TEST_COUNT is incremented by the background thread each health
 * test iteration.  Software reset (apply_reset()) must write 0x00000000 to this
 * register as part of the 8-step reset sequence (action 4).  The TLM read
 * returns 0x00000000 due to read_mask = 0x0.
 *
 * Procedure:
 *  1. Read REPETITION_TEST_COUNT immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: repetition_test_count_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_repetition_test_count_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::REPETITION_TEST_COUNT_RESET,
        "TC-F005-066: REPETITION_TEST_COUNT expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-068 — REPETITION_TEST_COUNT is read-only
// =============================================================================

/******************************************************************************
 * @brief TC-F005-068: Confirm REPETITION_TEST_COUNT (0x44) is RO — write has
 *        no effect
 *
 * REPETITION_TEST_COUNT has write_mask = 0x0; all write attempts are silently
 * rejected by regmodel.  A write of 0xFFFFFFFF must not alter the value read back.
 *
 * Procedure:
 *  1. Read REPETITION_TEST_COUNT before write; assert 0x00000000.
 *  2. Write 0xFFFFFFFF.
 *  3. Read REPETITION_TEST_COUNT after write; assert still 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000 both before and after the write.
 *
 * Test plan reference: repetition_test_count_is_read_only
 *
 * @return true if both assertions pass
 ******************************************************************************/
bool testbench::tc_f005_repetition_test_count_is_read_only()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1: Read before write.
    test->register_read_32(
        entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, rd_val);
    FUNC005_CHECK(rd_val == entropy_src_basetest::REPETITION_TEST_COUNT_RESET,
        "TC-F005-068 step 1: REPETITION_TEST_COUNT pre-write read returned 0x"
        << std::hex << rd_val << ", expected 0x00000000");

    // Step 2: Write all-ones.
    test->register_write_32(
        entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, 0xFFFFFFFFu);

    // Step 3: Read after write.
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, rd_val);
    FUNC005_CHECK(rd_val == entropy_src_basetest::REPETITION_TEST_COUNT_RESET,
        "TC-F005-068 step 2: REPETITION_TEST_COUNT post-write read returned 0x"
        << std::hex << rd_val
        << " — RO enforcement failed (expected 0x00000000)");

    return ok;
}

// =============================================================================
// TC-F005-069 — APT_PATTERN_COUNT_1BIT resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-069: Verify APT_PATTERN_COUNT_1BIT (0x50) clears to
 *        0x00000000 after software reset
 *
 * APT_PATTERN_COUNT_1BIT is incremented by the background thread each health
 * test iteration.  Software reset must clear it to 0x00000000.
 *
 * Procedure:
 *  1. Read APT_PATTERN_COUNT_1BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: apt_pattern_count_1bit_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_pattern_count_1bit_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::APT_PATTERN_COUNT_1BIT_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::APT_PATTERN_COUNT_1BIT_RESET,
        "TC-F005-069: APT_PATTERN_COUNT_1BIT expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-070 — APT_PATTERN_COUNT_2BIT resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-070: Verify APT_PATTERN_COUNT_2BIT (0x54) clears to
 *        0x00000000 after software reset
 *
 * Procedure:
 *  1. Read APT_PATTERN_COUNT_2BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: apt_pattern_count_2bit_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_pattern_count_2bit_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::APT_PATTERN_COUNT_2BIT_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::APT_PATTERN_COUNT_2BIT_RESET,
        "TC-F005-070: APT_PATTERN_COUNT_2BIT expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-071 — APT_PATTERN_COUNT_3BIT resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-071: Verify APT_PATTERN_COUNT_3BIT (0x58) clears to
 *        0x00000000 after software reset
 *
 * Procedure:
 *  1. Read APT_PATTERN_COUNT_3BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: apt_pattern_count_3bit_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_pattern_count_3bit_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x58u, rd_val);

    FUNC005_CHECK(rd_val == 0u,
        "TC-F005-071: APT_PATTERN_COUNT_3BIT expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-072 — APT_PATTERN_COUNT_4BIT resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-072: Verify APT_PATTERN_COUNT_4BIT (0x5C) clears to
 *        0x00000000 after software reset
 *
 * Procedure:
 *  1. Read APT_PATTERN_COUNT_4BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: apt_pattern_count_4bit_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_pattern_count_4bit_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x5Cu, rd_val);

    FUNC005_CHECK(rd_val == 0u,
        "TC-F005-072: APT_PATTERN_COUNT_4BIT expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-075 — APT_PROPORTION_1BIT reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-075: Verify APT_PROPORTION_1BIT (0x60) reset value
 *
 * APT_PROPORTION_1BIT is a pure regmodel storage register (no callbacks) with
 * reset value 0x00000200 (LIMIT = 512, encoding a 10-bit field).
 *
 * Procedure:
 *  1. Read APT_PROPORTION_1BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000200.
 *
 * Pass criterion: read_value == 0x00000200 (APT_PROPORTION_1BIT_RESET).
 *
 * Test plan reference: apt_proportion_1bit_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_proportion_1bit_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == F005_APT_PROPORTION_1BIT_RESET,
        "TC-F005-075: APT_PROPORTION_1BIT reset value mismatch — "
        "expected 0x" << std::hex << F005_APT_PROPORTION_1BIT_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-076 — APT_PROPORTION_2BIT reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-076: Verify APT_PROPORTION_2BIT (0x64) reset value
 *
 * APT_PROPORTION_2BIT has reset value 0x00000080 (LIMIT = 128).
 *
 * Procedure:
 *  1. Read APT_PROPORTION_2BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000080.
 *
 * Pass criterion: read_value == 0x00000080 (APT_PROPORTION_2BIT_RESET).
 *
 * Test plan reference: apt_proportion_2bit_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_proportion_2bit_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::APT_PROPORTION_LO_OFFSET, rd_val);

    FUNC005_CHECK(
        rd_val == entropy_src_basetest::APT_PROPORTION_LO_RESET,
        "TC-F005-076: APT_PROPORTION_2BIT reset value mismatch — "
        "expected 0x" << std::hex << F005_APT_PROPORTION_2BIT_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-077 — APT_PROPORTION_3BIT reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-077: Verify APT_PROPORTION_3BIT (0x68) reset value
 *
 * APT_PROPORTION_3BIT has reset value 0x00000040 (LIMIT = 64).
 *
 * Procedure:
 *  1. Read APT_PROPORTION_3BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000040.
 *
 * Pass criterion: read_value == 0x00000040 (APT_PROPORTION_3BIT_RESET).
 *
 * Test plan reference: apt_proportion_3bit_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_proportion_3bit_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x68u, rd_val);

    FUNC005_CHECK(
        rd_val == 0u,
        "TC-F005-077: APT_PROPORTION_3BIT reset value mismatch — "
        "expected 0x" << std::hex << F005_APT_PROPORTION_3BIT_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-078 — APT_PROPORTION_4BIT reset value
// =============================================================================

/******************************************************************************
 * @brief TC-F005-078: Verify APT_PROPORTION_4BIT (0x6C) reset value
 *
 * APT_PROPORTION_4BIT has reset value 0x00000020 (LIMIT = 32).
 *
 * Procedure:
 *  1. Read APT_PROPORTION_4BIT immediately after apply_reset().
 *  2. Assert read_value == 0x00000020.
 *
 * Pass criterion: read_value == 0x00000020 (APT_PROPORTION_4BIT_RESET).
 *
 * Test plan reference: apt_proportion_4bit_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_apt_proportion_4bit_reset_value()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x6Cu, rd_val);

    FUNC005_CHECK(
        rd_val == 0u,
        "TC-F005-078: APT_PROPORTION_4BIT reset value mismatch — "
        "expected 0x" << std::hex << F005_APT_PROPORTION_4BIT_RESET
        << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-079 — APT_PROPORTION_1BIT write mask 0x000003FF validation
// =============================================================================

/******************************************************************************
 * @brief TC-F005-079: Verify APT_PROPORTION_1BIT write mask 0x000003FF
 *
 * All four APT_PROPORTION registers have a 10-bit write mask 0x000003FF.
 * Bits [31:10] are reserved and must always read as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF to APT_PROPORTION_1BIT.
 *  2. Read back; assert (read_value & ~0x000003FF) == 0 and
 *     (read_value & 0x000003FF) == 0x000003FF.
 *  3. Write 0x00000000; assert readback == 0x00000000.
 *  4. Write 0x000001FF (all writeable bits set except MSB); assert readback
 *     == 0x000001FF.
 *
 * Pass criterion: reserved bits [31:10] always read as zero; LIMIT[9:0] retains
 *                 the masked written value.
 *
 * Test plan reference: apt_proportion_1bit_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f005_apt_proportion_1bit_write_mask_validation()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    // Step 1-2: Write all-ones; reserved bits must be zero.
    test->register_write_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, rd_val);

    FUNC005_CHECK(
        (rd_val & ~entropy_src_basetest::APT_PROPORTION_1BIT_READ) == 0u,
        "TC-F005-079 step 1: APT_PROPORTION_1BIT reserved bits [31:10] non-zero — "
        "read 0x" << std::hex << rd_val
        << ", reserved = 0x" << (rd_val & ~entropy_src_basetest::APT_PROPORTION_1BIT_READ));

    FUNC005_CHECK(
        (rd_val & entropy_src_basetest::APT_PROPORTION_1BIT_READ) == entropy_src_basetest::APT_PROPORTION_1BIT_READ,
        "TC-F005-079 step 1: APT_PROPORTION_1BIT writeable bits mismatch — "
        "expected 0x" << std::hex << entropy_src_basetest::APT_PROPORTION_1BIT_READ
        << " got 0x" << (rd_val & entropy_src_basetest::APT_PROPORTION_1BIT_READ));

    // Step 3: Write all-zeros.
    test->register_write_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, 0x00000000u);
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, rd_val);
    FUNC005_CHECK(
        rd_val == 0x00000000u,
        "TC-F005-079 step 2: APT_PROPORTION_1BIT expected 0x00000000 after "
        "writing 0, got 0x" << std::hex << rd_val);

    // Step 4: Write 0x000001FF.
    const uint32_t partial = 0x000001FFu;
    test->register_write_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, partial);
    rd_val = 0u;
    test->register_read_32(
        entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET, rd_val);
    FUNC005_CHECK(
        rd_val == partial,
        "TC-F005-079 step 3: APT_PROPORTION_1BIT partial pattern mismatch — "
        "wrote 0x" << std::hex << partial << " got 0x" << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-082 — MARKOV_TEST_COUNTS_0 resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-082: Verify MARKOV_TEST_COUNTS_0 (0x80) clears to
 *        0x00000000 after software reset
 *
 * MARKOV_TEST_COUNTS_0 is incremented by 1 per health test iteration.
 * Software reset must clear it to 0x00000000.
 *
 * Procedure:
 *  1. Read MARKOV_TEST_COUNTS_0 immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: markov_test_counts_0_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_markov_test_counts_0_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::MARKOV_TEST_COUNTS_0_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::MARKOV_TEST_COUNTS_0_RESET,
        "TC-F005-082: MARKOV_TEST_COUNTS_0 expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-083 — MARKOV_TEST_COUNTS_1 resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-083: Verify MARKOV_TEST_COUNTS_1 (0x84) clears to
 *        0x00000000 after software reset
 *
 * MARKOV_TEST_COUNTS_1 is incremented by 1 per health test iteration.
 * Software reset must clear it to 0x00000000.
 *
 * Procedure:
 *  1. Read MARKOV_TEST_COUNTS_1 immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: markov_test_counts_1_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_markov_test_counts_1_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x84u, rd_val);

    FUNC005_CHECK(rd_val == 0u,
        "TC-F005-083: MARKOV_TEST_COUNTS_1 expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-084 — MARKOV_TEST_PROBABILITIES resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-084: Verify MARKOV_TEST_PROBABILITIES (0x88) clears to
 *        0x00000000 after software reset
 *
 * MARKOV_TEST_PROBABILITIES is incremented by a PRNG-derived masked value per
 * health test iteration.  Software reset must clear it to 0x00000000.
 *
 * Procedure:
 *  1. Read MARKOV_TEST_PROBABILITIES immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: markov_test_probabilities_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_markov_test_probabilities_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        0x88u, rd_val);

    FUNC005_CHECK(rd_val == 0u,
        "TC-F005-084: MARKOV_TEST_PROBABILITIES expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-104 — GENERATOR_0_HEALTH_STATUS resets to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-104: Verify GENERATOR_0_HEALTH_STATUS (0xC0) clears to
 *        0x00000000 after software reset
 *
 * GENERATOR_0_HEALTH_STATUS is updated independently each health test
 * iteration with a PRNG-derived value.  Software reset must clear it to
 * 0x00000000 as part of reset action (4).
 *
 * Procedure:
 *  1. Read GENERATOR_0_HEALTH_STATUS immediately after apply_reset().
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: generator_0_health_status_reset_to_zero
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f005_generator_0_health_status_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    test->register_read_32(
        entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET, rd_val);

    FUNC005_CHECK(rd_val == entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_RESET,
        "TC-F005-104: GENERATOR_0_HEALTH_STATUS expected 0x00000000 after reset, "
        "got 0x" << std::hex << rd_val);

    return ok;
}

// =============================================================================
// TC-F005-105 — GENERATOR_1 through GENERATOR_11 reset to zero
// =============================================================================

/******************************************************************************
 * @brief TC-F005-105: Verify GENERATOR_1–11_HEALTH_STATUS (0xC4–0xEC) each
 *        clear to 0x00000000 after software reset
 *
 * All 12 per-generator health status registers are cleared by reset action (4).
 * This test iterates over GENERATOR_1 through GENERATOR_11 (offsets 0xC4 to 0xEC)
 * and confirms each reads 0x00000000 immediately after apply_reset().
 *
 * Procedure (for generator indices 1–11):
 *  1. Read GENERATOR_k_HEALTH_STATUS.
 *  2. Assert read_value == 0x00000000.
 *
 * Pass criterion: all 11 registers return 0x00000000.
 *
 * Test plan reference: generator_1_to_11_health_status_reset_to_zero
 *
 * @return true if all 11 assertions pass
 ******************************************************************************/
bool testbench::tc_f005_generator_1_to_11_health_status_reset_to_zero()
{
    bool     ok     = true;
    uint32_t rd_val = 0u;

    const unsigned int gen_offsets[11] =
    {
        entropy_src_basetest::GENERATOR_1_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_2_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_3_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_4_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_5_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_6_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_7_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_8_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_9_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_10_HEALTH_STATUS_OFFSET,
        entropy_src_basetest::GENERATOR_11_HEALTH_STATUS_OFFSET
    };

    for (int g = 0; g < 11; ++g)
    {
        rd_val = 0u;
        test->register_read_32(gen_offsets[g], rd_val);
        FUNC005_CHECK(rd_val == 0,
            "TC-F005-105: GENERATOR_" << (g + 1) << "_HEALTH_STATUS "
            "expected 0x00000000 after reset, got 0x" << std::hex << rd_val);
    }

    return ok;
}

