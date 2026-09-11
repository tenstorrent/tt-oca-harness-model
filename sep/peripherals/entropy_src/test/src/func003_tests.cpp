// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file func003_tests.cpp
 * @brief FUNC-003 Peripheral Configuration Register Retention — test case
 *        implementations
 *
 * Implements all 19 test cases mapped to FUNC-003 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to one
 * or more rows in the FUNC-003 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-003 — 19 test cases)
 *
 *  Sl. | Method                                            | Test Plan ID
 *  ----|---------------------------------------------------|-----------------------------------
 *  10  | tc_f003_debug_ctrl_reset_value                    | debug_ctrl_reset_value
 *  11  | tc_f003_debug_ctrl_write_mask_select_signal       | debug_ctrl_write_mask_select_signal
 *  86  | tc_f003_ring_osc_enable_reset_value               | ring_osc_enable_reset_value
 *  87  | tc_f003_ring_osc_enable_write_mask_validation     | ring_osc_enable_write_mask_validation
 *  88  | tc_f003_ring_osc_enable_partial_disable_readback  | ring_osc_enable_partial_disable_readback
 *  89  | tc_f003_ring_osc_tune_reset_value                 | ring_osc_tune_reset_value
 *  90  | tc_f003_ring_osc_tune_write_mask_validation       | ring_osc_tune_write_mask_validation
 *  91  | tc_f003_ring_osc_tune_write_readback_retained     | ring_osc_tune_write_readback_retained
 *  92  | tc_f003_ring_osc_ctrl_reset_value                 | ring_osc_ctrl_reset_value
 *  93  | tc_f003_ring_osc_ctrl_write_mask_validation       | ring_osc_ctrl_write_mask_validation
 *  94  | tc_f003_decorrelator_ctrl_reset_value             | decorrelator_ctrl_reset_value
 *  95  | tc_f003_decorrelator_ctrl_full_write_readback     | decorrelator_ctrl_full_write_readback
 *  96  | tc_f003_decorrelator_mask_reset_value             | decorrelator_mask_reset_value
 *  97  | tc_f003_decorrelator_mask_write_mask_validation   | decorrelator_mask_write_mask_validation
 * 137  | tc_f003_config_regs_reset_restores_defaults       | config_regs_reset_restores_defaults
 *
 * ## Registers under test
 *
 *  Register           | Offset                      | Write Mask                      | Reset Value
 *  -------------------|-----------------------------|----------------------------------|------------------
 *  DEBUG_CTRL         | DEBUG_CTRL_OFFSET    (0x0C) | DEBUG_CTRL_WRITE    = 0x000007FF | DEBUG_CTRL_RESET    = 0x00000000
 *  RING_OSC_ENABLE    | RING_OSC_ENABLE_OFFSET(0x90)| RING_OSC_ENABLE_WRITE= 0x00FFFFFF| RING_OSC_ENABLE_RESET= 0x00FFFFFF
 *  RING_OSC_TUNE      | RING_OSC_TUNE_OFFSET  (0x94)| RING_OSC_TUNE_WRITE = 0x00FFFFFF | RING_OSC_TUNE_RESET  = 0x00000000
 *  RING_OSC_CTRL      | RING_OSC_CTRL_OFFSET  (0x98)| RING_OSC_CTRL_WRITE = 0x00000FFF | RING_OSC_CTRL_RESET  = 0x00000FFF
 *  DECORRELATOR_CTRL  | DECORRELATOR_CTRL_OFFSET(0xA0)| DECORRELATOR_CTRL_WRITE=0xFFFFFFFF | DECORRELATOR_CTRL_RESET=0x0003F000
 *  DECORRELATOR_MASK  | DECORRELATOR_MASK_OFFSET(0xA4)| DECORRELATOR_MASK_WRITE=0x000000FF | DECORRELATOR_MASK_RESET=0x000000FF
 *
 * ## Architectural facts
 *
 *  - All six registers are pure regmodel storage with no write or read callbacks.
 *    There are no side-effects on INTR_STATUS, interrupt ports, or FIFO state.
 *  - regmodel enforces write masks automatically: bits outside the mask are
 *    discarded on write and always read as zero.
 *  - No wait(SC_ZERO_TIME) is required: these registers have no delta-cycle
 *    side effects on sc_out<bool> ports.
 *  - apply_reset() (via CTRL[0]=1) triggers the entropy_src reset callback
 *    which restores every register to its documented hardware reset default,
 *    including all six FUNC-003 configuration registers.
 *
 * ## Design Constraints
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on
 *    entropy_src_ip::target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() before each test group.
 *  - The FUNC003_CHECK macro sets ok = false and emits a REG_ERROR log entry
 *    naming both the expected and observed values.
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md
 *  - entropy_src/docs/entropy_src-detailed-design.md §4 (register reference)
 *  - entropy_src/docs/entropy_src-test-plan.md
 *  - entropy_src/test/inc/testbench.h
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
/// Used inside every sub-test assertion block so that the failure log always
/// names both the expected and observed values.
#define FUNC003_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            REG_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)
/// @endcond

// =============================================================================
// TC-F003-010 — DEBUG_CTRL reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that DEBUG_CTRL (0x0C) reads 0x00000000 after apply_reset().
 *
 * DEBUG_CTRL has write_bit_mask = 0x000007FF and reset value 0x00000000.
 * A software reset via CTRL[0]=1 must restore DEBUG_CTRL to its reset default.
 *
 * Procedure:
 *  1. apply_reset() is called by run_tests() before this method.
 *  2. Read DEBUG_CTRL and assert the value equals DEBUG_CTRL_RESET (0x00000000).
 *
 * Pass criterion: read_value == 0x00000000.
 *
 * Test plan reference: debug_ctrl_reset_value
 *
 * @return true if the assertion passes
 ******************************************************************************/
bool testbench::tc_f003_debug_ctrl_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::DEBUG_CTRL_RESET);  // 0x00000000

    test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-010 DEBUG_CTRL reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-011 — DEBUG_CTRL write mask SELECT_SIGNAL (bits [7:0] and [10:8])
// =============================================================================

/******************************************************************************
 * @brief Verify that DEBUG_CTRL (0x0C) write mask 0x000007FF rejects bits
 *        outside [10:0] and retains bits inside the mask.
 *
 * Writing 0xFFFFFFFF must return 0x000007FF (only the eleven writable bits
 * are retained).  All bits at positions [31:11] must read as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF to DEBUG_CTRL.
 *  2. Read back DEBUG_CTRL; assert readback == 0x000007FF.
 *  3. Write 0x00000000 to DEBUG_CTRL; assert readback == 0x00000000.
 *
 * Pass criterion:
 *  - Write 0xFFFFFFFF: readback == 0x000007FF (write mask enforced).
 *  - Write 0x00000000: readback == 0x00000000 (all bits cleared).
 *
 * Test plan reference: debug_ctrl_write_mask_select_signal
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_debug_ctrl_write_mask_select_signal()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::DEBUG_CTRL_WRITE);  // 0x000007FF

    // Sub-test A: all-ones pattern — expect write mask applied.
    test->register_write_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32 (entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == wmask,
        "TC-F003-011 DEBUG_CTRL write-all-ones: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << wmask
        << " (write mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — reserved bits [31:11] must read zero");

    // Sub-test B: all-zeros pattern — expect 0x00000000.
    test->register_write_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-011 DEBUG_CTRL write-all-zeros: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-086 — RING_OSC_ENABLE reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_ENABLE (0x90) reads 0x00FFFFFF after reset.
 *
 * The hardware reset default enables all 12 ring oscillators and their
 * associated sample clocks (bits [23:0] all set).  A software reset must
 * restore this default.
 *
 * Pass criterion: read_value == 0x00FFFFFF (RING_OSC_ENABLE_RESET).
 *
 * Test plan reference: ring_osc_enable_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_enable_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_ENABLE_RESET);  // 0x00FFFFFF

    test->register_read_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-086 RING_OSC_ENABLE reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — all 24 RO enable bits must be set at reset");

    return ok;
}

// =============================================================================
// TC-F003-087 — RING_OSC_ENABLE write mask 0x00FFFFFF
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_ENABLE (0x90) write mask 0x00FFFFFF restricts
 *        writes to bits [23:0] with bits [31:24] always reading as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF; assert readback == 0x00FFFFFF (mask applied).
 *  2. Write 0x00000000; assert readback == 0x00000000 (all enable bits cleared).
 *
 * Pass criterion:
 *  - Write 0xFFFFFFFF: readback == 0x00FFFFFF.
 *  - Write 0x00000000: readback == 0x00000000.
 *
 * Test plan reference: ring_osc_enable_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_enable_write_mask_validation()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_ENABLE_WRITE);  // 0x00FFFFFF

    // Sub-test A: all-ones — expect write mask applied.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0xFFFFFFFFu);
    test->register_read_32 (entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC003_CHECK(read_val == wmask,
        "TC-F003-087 RING_OSC_ENABLE write-all-ones: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << wmask
        << " (write mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — bits [31:24] must read zero");

    // Sub-test B: all-zeros — expect 0x00000000.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-087 RING_OSC_ENABLE write-all-zeros: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-088 — RING_OSC_ENABLE partial-disable readback
// =============================================================================

/******************************************************************************
 * @brief Verify that individual ring oscillator enable bits within
 *        RING_OSC_ENABLE can be selectively cleared and confirmed.
 *
 * Procedure:
 *  1. Write 0x00000000 to RING_OSC_ENABLE (disable all ROs).
 *  2. Read back; assert readback == 0x00000000.
 *  3. Write 0x00000FFF to RING_OSC_ENABLE (enable only lower 12 ROs).
 *  4. Read back; assert readback == 0x00000FFF.
 *
 * Pass criterion:
 *  - Step 2: readback == 0x00000000.
 *  - Step 4: readback == 0x00000FFF (partial enable pattern retained).
 *
 * Test plan reference: ring_osc_enable_partial_disable_readback
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_enable_partial_disable_readback()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    // Sub-test A: disable all ring oscillators.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-088 RING_OSC_ENABLE disable-all: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — all RO enable bits must be clearable");

    // Sub-test B: enable only lower 12 ring oscillators.
    const uint32_t partial = 0x00000FFFu;
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, partial);
    test->register_read_32 (entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC003_CHECK(read_val == partial,
        "TC-F003-088 RING_OSC_ENABLE partial-enable (0x00000FFF): expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << partial
        << ", got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — lower 12 bits must be independently settable");

    return ok;
}

// =============================================================================
// TC-F003-089 — RING_OSC_TUNE reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_TUNE (0x94) reads 0x00000000 after apply_reset().
 *
 * Pass criterion: read_value == 0x00000000 (RING_OSC_TUNE_RESET).
 *
 * Test plan reference: ring_osc_tune_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_tune_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_TUNE_RESET);  // 0x00000000

    test->register_read_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-089 RING_OSC_TUNE reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-090 — RING_OSC_TUNE write mask 0x00FFFFFF
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_TUNE (0x94) write mask 0x00FFFFFF restricts
 *        writes to bits [23:0] with bits [31:24] always reading as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF; assert readback == 0x00FFFFFF.
 *  2. Write 0x00000000; assert readback == 0x00000000.
 *
 * Pass criterion: (read_value & ~0x00FFFFFF) == 0 for all patterns.
 *
 * Test plan reference: ring_osc_tune_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_tune_write_mask_validation()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_TUNE_WRITE);  // 0x00FFFFFF

    // Sub-test A: all-ones pattern.
    test->register_write_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, 0xFFFFFFFFu);
    test->register_read_32 (entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);

    FUNC003_CHECK(read_val == wmask,
        "TC-F003-090 RING_OSC_TUNE write-all-ones: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << wmask
        << " (write mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — bits [31:24] must read zero");

    // Sub-test B: all-zeros pattern.
    test->register_write_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-090 RING_OSC_TUNE write-all-zeros: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-091 — RING_OSC_TUNE write/readback retention
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_TUNE retains written values across consecutive
 *        write-read cycles (pure regmodel storage — no callbacks erase the value).
 *
 * Procedure:
 *  1. Write 0x00AABBCC; read back and assert readback == 0x00AABBCC.
 *  2. Write 0x00555AAA; read back and assert readback == 0x00555AAA.
 *
 * Pass criterion: last-written value is always retained by subsequent reads.
 *
 * Test plan reference: ring_osc_tune_write_readback_retained
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_tune_write_readback_retained()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_TUNE_WRITE);  // 0x00FFFFFF

    // Cycle 1: distinctive pattern within write mask.
    const uint32_t pattern1 = 0x00AABBCCu & wmask;  // 0x00AABBCC (within mask)
    test->register_write_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, pattern1);
    test->register_read_32 (entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);

    FUNC003_CHECK(read_val == pattern1,
        "TC-F003-091 RING_OSC_TUNE cycle-1: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << pattern1
        << " retained, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    // Cycle 2: overwrite with a second pattern to confirm value replacement.
    const uint32_t pattern2 = 0x00555AAAu & wmask;  // 0x00555AAA (within mask)
    test->register_write_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, pattern2);
    test->register_read_32 (entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);

    FUNC003_CHECK(read_val == pattern2,
        "TC-F003-091 RING_OSC_TUNE cycle-2: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << pattern2
        << " retained after overwrite, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-092 — RING_OSC_CTRL reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_CTRL (0x98) reads 0x00000FFF after apply_reset().
 *
 * The hardware reset default configures all 12 ring oscillators to use the
 * internal sample clock (bits [11:0] all set).
 *
 * Pass criterion: read_value == 0x00000FFF (RING_OSC_CTRL_RESET).
 *
 * Test plan reference: ring_osc_ctrl_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_ctrl_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_CTRL_RESET);  // 0x00000FFF

    test->register_read_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-092 RING_OSC_CTRL reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — all 12 sample-clock-select bits must be set at reset");

    return ok;
}

// =============================================================================
// TC-F003-093 — RING_OSC_CTRL write mask 0x00000FFF
// =============================================================================

/******************************************************************************
 * @brief Verify that RING_OSC_CTRL (0x98) write mask 0x00000FFF restricts
 *        writes to bits [11:0] with bits [31:12] always reading as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF; assert readback == 0x00000FFF.
 *  2. Write 0x00000000; assert readback == 0x00000000.
 *
 * Pass criterion: (read_value & ~0x00000FFF) == 0 for all patterns.
 *
 * Test plan reference: ring_osc_ctrl_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_ring_osc_ctrl_write_mask_validation()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_CTRL_WRITE);  // 0x00000FFF

    // Sub-test A: all-ones pattern.
    test->register_write_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32 (entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == wmask,
        "TC-F003-093 RING_OSC_CTRL write-all-ones: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << wmask
        << " (write mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — bits [31:12] must read zero");

    // Sub-test B: all-zeros pattern.
    test->register_write_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-093 RING_OSC_CTRL write-all-zeros: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-094 — DECORRELATOR_CTRL reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that DECORRELATOR_CTRL (0xA0) reads 0x0003F000 after reset.
 *
 * The hardware reset default sets SAMPLE_CLK_DIV = 63 (bits [17:12] = 0x3F)
 * and BYPASS = 0 (bit [18] = 0).  The full value is 0x0003F000.
 *
 * Pass criterion: read_value == 0x0003F000 (DECORRELATOR_CTRL_RESET).
 *
 * Test plan reference: decorrelator_ctrl_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f003_decorrelator_ctrl_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_CTRL_RESET);  // 0x0003F000

    test->register_read_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-094 DECORRELATOR_CTRL reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — SAMPLE_CLK_DIV=63 (bits[17:12]) must be set at reset");

    return ok;
}

// =============================================================================
// TC-F003-095 — DECORRELATOR_CTRL full write/readback (mask 0xFFFFFFFF)
// =============================================================================

/******************************************************************************
 * @brief Verify that DECORRELATOR_CTRL (0xA0) is fully writable (write mask
 *        0xFFFFFFFF) and retains all 32 written bits.
 *
 * DECORRELATOR_CTRL is the only one of the six FUNC-003 registers with a full
 * 32-bit write mask.  All 32 bits must be independently writable and readable.
 *
 * Procedure:
 *  1. Write 0xDEADBEEF; assert readback == 0xDEADBEEF.
 *  2. Write checkerboard 0x55AA55AA; assert readback == 0x55AA55AA.
 *
 * Pass criterion: read_value == write_value for all patterns (no masking loss).
 *
 * Test plan reference: decorrelator_ctrl_full_write_readback
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_decorrelator_ctrl_full_write_readback()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    // Cycle 1: distinctive sentinel value.
    const uint32_t pattern1 = 0xDEADBEEFu;
    test->register_write_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, pattern1);
    test->register_read_32 (entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == pattern1,
        "TC-F003-095 DECORRELATOR_CTRL 0xDEADBEEF: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << pattern1
        << " retained (full 32-bit mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    // Cycle 2: 32-bit checkerboard.
    const uint32_t pattern2 = 0x55AA55AAu;
    test->register_write_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, pattern2);
    test->register_read_32 (entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);

    FUNC003_CHECK(read_val == pattern2,
        "TC-F003-095 DECORRELATOR_CTRL checkerboard 0x55AA55AA: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << pattern2
        << " retained, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-096 — DECORRELATOR_MASK reset value
// =============================================================================

/******************************************************************************
 * @brief Verify that DECORRELATOR_MASK (0xA4) reads 0x000000FF after reset.
 *
 * The hardware reset default enables all eight entropy byte lanes (bits [7:0]
 * all set).  A software reset must restore this default.
 *
 * Pass criterion: read_value == 0x000000FF (DECORRELATOR_MASK_RESET).
 *
 * Test plan reference: decorrelator_mask_reset_value
 *
 * @return true if assertion passes
 ******************************************************************************/
bool testbench::tc_f003_decorrelator_mask_reset_value()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t expected =
        static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_MASK_RESET);  // 0x000000FF

    test->register_read_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);

    FUNC003_CHECK(read_val == expected,
        "TC-F003-096 DECORRELATOR_MASK reset: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << expected
        << " after reset, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — all 8 byte-lane mask bits must be enabled at reset");

    return ok;
}

// =============================================================================
// TC-F003-097 — DECORRELATOR_MASK write mask 0x000000FF
// =============================================================================

/******************************************************************************
 * @brief Verify that DECORRELATOR_MASK (0xA4) write mask 0x000000FF restricts
 *        writes to bits [7:0] with bits [31:8] always reading as zero.
 *
 * Procedure:
 *  1. Write 0xFFFFFFFF; assert readback == 0x000000FF.
 *  2. Write 0x00000000; assert readback == 0x00000000.
 *
 * Pass criterion: (read_value & ~0x000000FF) == 0 for all patterns.
 *
 * Test plan reference: decorrelator_mask_write_mask_validation
 *
 * @return true if all assertions pass
 ******************************************************************************/
bool testbench::tc_f003_decorrelator_mask_write_mask_validation()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_MASK_WRITE);  // 0x000000FF

    // Sub-test A: all-ones pattern.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, 0xFFFFFFFFu);
    test->register_read_32 (entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);

    FUNC003_CHECK(read_val == wmask,
        "TC-F003-097 DECORRELATOR_MASK write-all-ones: expected 0x"
        << std::hex << std::setw(8) << std::setfill('0') << wmask
        << " (write mask), got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val
        << " — bits [31:8] must read zero");

    // Sub-test B: all-zeros pattern.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, 0x00000000u);
    test->register_read_32 (entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);

    FUNC003_CHECK(read_val == 0x00000000u,
        "TC-F003-097 DECORRELATOR_MASK write-all-zeros: expected 0x00000000, got 0x"
        << std::hex << std::setw(8) << std::setfill('0') << read_val);

    return ok;
}

// =============================================================================
// TC-F003-137 — Software reset restores all six register defaults
// =============================================================================

/******************************************************************************
 * @brief Verify that triggering a software reset via CTRL[0]=1 restores all
 *        six configuration registers to their documented hardware reset defaults
 *        even after arbitrary non-default values have been written.
 *
 * This test is the primary end-to-end retention test: it confirms that the
 * reset sequence (handle_write_CTRL callback) correctly reloads every FUNC-003
 * register and that regmodel's post-reset readback matches the documented default.
 *
 * Procedure:
 *  1. Write a non-default pattern to each register.
 *  2. Trigger apply_reset() (writes CTRL[0]=1 then waits one delta cycle).
 *  3. Read each register and assert the value equals its documented reset
 *     default.
 *
 * Expected defaults after reset:
 *  Register           | Reset Value
 *  -------------------|-----------
 *  DEBUG_CTRL         | 0x00000000
 *  RING_OSC_ENABLE    | 0x00FFFFFF
 *  RING_OSC_TUNE      | 0x00000000
 *  RING_OSC_CTRL      | 0x00000FFF
 *  DECORRELATOR_CTRL  | 0x0003F000
 *  DECORRELATOR_MASK  | 0x000000FF
 *
 * Pass criterion: read_value == reset_value for all six registers after reset.
 *
 * Test plan reference: config_regs_reset_restores_defaults
 *
 * @return true if all six assertions pass
 ******************************************************************************/
bool testbench::tc_f003_config_regs_reset_restores_defaults()
{
    bool     ok       = true;
    uint32_t read_val = 0xDEADBEEFu;

    // -------------------------------------------------------------------------
    // Step 1: Write non-default patterns to all six registers.
    // Use patterns that differ from every register's reset value so that a
    // failed reset is immediately detectable on readback.
    // -------------------------------------------------------------------------
    test->register_write_32(entropy_src_basetest::DEBUG_CTRL_OFFSET,        0x000005A5u);
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET,   0x00000000u);
    test->register_write_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET,     0x00DEADC0u);
    test->register_write_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET,     0x00000000u);
    test->register_write_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, 0xBEEFF00Du);
    test->register_write_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, 0x00000000u);

    // -------------------------------------------------------------------------
    // Step 2: Apply software reset via CTRL[0]=1 (self-clearing).
    //         apply_reset() issues the write and waits one delta cycle.
    // -------------------------------------------------------------------------
    apply_reset();

    // -------------------------------------------------------------------------
    // Step 3: Verify each register has been restored to its reset default.
    // -------------------------------------------------------------------------

    // DEBUG_CTRL — reset default 0x00000000
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::DEBUG_CTRL_RESET);
        test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 DEBUG_CTRL after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must restore DEBUG_CTRL to 0x00000000");
    }

    // RING_OSC_ENABLE — reset default 0x00FFFFFF
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::RING_OSC_ENABLE_RESET);
        test->register_read_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 RING_OSC_ENABLE after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must restore all 24 RO-enable bits");
    }

    // RING_OSC_TUNE — reset default 0x00000000
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::RING_OSC_TUNE_RESET);
        test->register_read_32(entropy_src_basetest::RING_OSC_TUNE_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 RING_OSC_TUNE after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must clear RING_OSC_TUNE");
    }

    // RING_OSC_CTRL — reset default 0x00000FFF
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::RING_OSC_CTRL_RESET);
        test->register_read_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 RING_OSC_CTRL after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must restore all 12 sample-clock-select bits");
    }

    // DECORRELATOR_CTRL — reset default 0x0003F000
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_CTRL_RESET);
        test->register_read_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 DECORRELATOR_CTRL after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must restore SAMPLE_CLK_DIV=63 (bits[17:12])");
    }

    // DECORRELATOR_MASK — reset default 0x000000FF
    {
        const uint32_t expected =
            static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_MASK_RESET);
        test->register_read_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);
        FUNC003_CHECK(read_val == expected,
            "TC-F003-137 DECORRELATOR_MASK after reset: expected 0x"
            << std::hex << std::setw(8) << std::setfill('0') << expected
            << ", got 0x"
            << std::hex << std::setw(8) << std::setfill('0') << read_val
            << " — reset must restore all 8 byte-lane mask bits");
    }

    return ok;
}
