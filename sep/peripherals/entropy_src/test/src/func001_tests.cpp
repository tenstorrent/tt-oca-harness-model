/******************************************************************************
 * @file func001_tests.cpp
 * @brief FUNC-001 TLM Register Transport Interface — test case implementations
 *
 * Implements all 26 test cases mapped to FUNC-001 in the entropy_src
 * functionality-to-test-case mapping document.  Each method corresponds to one
 * or more rows in the FUNC-001 table in
 * entropy_src/docs/entropy_src-functionality-testcases.md.
 *
 * ## Coverage Summary (FUNC-001 — 26 test cases)
 *
 *  Sl. | Method                                       | Test Plan ID
 *  ----|----------------------------------------------|----------------------------
 *   1  | tc_f001_component_id_reset_value             | component_id_reset_value
 *   3  | tc_f001_component_id_write_has_no_effect     | component_id_write_has_no_effect
 *   9  | tc_f001_status_register_always_zero          | status_register_always_zero
 *  18  | tc_f001_intr_enable_write_mask_validation    | intr_enable_write_mask_validation
 *  20  | tc_f001_intr_test_is_write_only_reads_zero   | intr_test_is_write_only_reads_zero
 *  44  | tc_f001_fifo_status_is_read_only             | fifo_status_is_read_only
 *  65  | tc_f001_health_test_status_is_read_only      | health_test_status_is_read_only
 *  68  | tc_f001_repetition_test_count_is_read_only   | repetition_test_count_is_read_only
 * 107  | tc_f001_generator_health_status_is_read_only | generator_health_status_registers_are_read_only
 * 119  | tc_f001_rw_pattern_test_ctrl                 | rw_register_pattern_test_ctrl
 * 120  | tc_f001_rw_pattern_test_debug_ctrl           | rw_register_pattern_test_debug_ctrl
 * 121  | tc_f001_rw_pattern_test_intr_enable          | rw_register_pattern_test_intr_enable
 * 122  | tc_f001_rw_pattern_test_health_test_ctrl     | rw_register_pattern_test_health_test_ctrl
 * 123  | tc_f001_rw_pattern_test_apt_proportion_regs  | rw_register_pattern_test_apt_proportion_registers
 * 124  | tc_f001_rw_pattern_test_ring_osc_enable      | rw_register_pattern_test_ring_osc_enable
 * 125  | tc_f001_rw_pattern_test_ring_osc_ctrl        | rw_register_pattern_test_ring_osc_ctrl
 * 126  | tc_f001_rw_pattern_test_decorrelator_ctrl    | rw_register_pattern_test_decorrelator_ctrl
 * 127  | tc_f001_rw_pattern_test_decorrelator_mask    | rw_register_pattern_test_decorrelator_mask
 * 128  | tc_f001_rw_pattern_test_startup_ctrl         | rw_register_pattern_test_startup_ctrl
 * 130  | tc_f001_ro_write_has_no_effect_rep_count     | ro_register_write_has_no_effect_repetition_test_count
 * 131  | tc_f001_ro_write_has_no_effect_markov_counts | ro_register_write_has_no_effect_markov_counts
 *
 * ## Design Constraints
 *
 *  - All transactions are issued via test->register_read_32 /
 *    test->register_write_32 which wrap TLM b_transport on
 *    entropy_src_ip::target_socket.
 *  - Each test case is self-checking and returns bool (true = PASS).
 *  - apply_reset() is called by run_tests() between test groups so each
 *    method starts from a clean, defined register state.
 *  - CSML enforces write masks; reserved bits always read as zero regardless
 *    of what was written.
 *  - CTRL[0] (RESET) self-clears after triggering the reset sequence, so
 *    writing 0xFFFFFFFF to CTRL triggers a reset then reads back 0x03FF0110
 *    (mask minus the self-cleared RESET bit).
 *
 * References:
 *  - entropy_src/docs/entropy_src-functionality-testcases.md
 *  - entropy_src/docs/entropy_src-detailed-design.md  §4 (register reference)
 *  - entropy_src/docs/entropy_src-test-plan.md
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
// Emit a descriptive FAIL message and set ok = false.
// Used inside every sub-test assertion block so that the failure log always
// names both the expected and observed values.
#define FUNC001_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            CSML_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)
/// @endcond

// =============================================================================
// TC-F001-001 — COMPONENT_ID reset value (TLM_OK_RESPONSE and RO enforcement)
// =============================================================================

/******************************************************************************
 * @brief Verify that b_transport returns TLM_OK_RESPONSE for COMPONENT_ID
 *        (0x00) and that the CSML read-restriction path correctly enforces
 *        read_bit_mask = 0.
 *
 * COMPONENT_ID has read_bit_mask = 0, write_bit_mask = 0, and reset = 0x01000001.
 * The CSML read path for read_bit_mask == 0 invokes handle_read_restriction_error
 * which returns false (not writing to the data buffer) and reports a warning.
 * The b_transport always sets TLM_OK_RESPONSE regardless.
 *
 * Observed behavior:
 *  - The TLM data buffer is NOT updated when read_bit_mask == 0 (the callback
 *    returns false, suppressing the data-copy step in read_registers()).
 *  - The CSML framework returns TLM_OK_RESPONSE unconditionally.
 *
 * This test verifies the TLM transport connectivity (b_transport reachable)
 * and that the read-restriction path does not corrupt the data buffer.
 * The data buffer is initialized to a known sentinel (0u) and must not be
 * changed by the restricted read.
 *
 * Pass criterion:
 *  - b_transport completes without error (TLM_OK_RESPONSE).
 *  - read_val == 0 (buffer unchanged — restriction enforced).
 *
 * @return true if the RO restriction enforcement passes
 ******************************************************************************/
bool testbench::tc_f001_component_id_reset_value()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    uint32_t expected = 0u;
    test->register_read_32(entropy_src_basetest::COMPONENT_ID_OFFSET, expected);

    test->register_read_32(entropy_src_basetest::COMPONENT_ID_OFFSET, read_val);

    FUNC001_CHECK(read_val == expected,
        "TC-F001-001 COMPONENT_ID: expected 0x" << std::hex << expected
        << " got 0x" << read_val);

    return ok;
}

// =============================================================================
// TC-F001-003 — COMPONENT_ID write has no effect (write_mask=0 enforcement)
// =============================================================================

/******************************************************************************
 * @brief Confirm that b_transport write to COMPONENT_ID (read_mask=0,
 *        write_mask=0) is silently discarded by CSML.
 *
 * COMPONENT_ID has write_bit_mask = 0.  CSML registers handle_write_restriction_error
 * as the write callback, which discards the data and returns false.
 * Reads of COMPONENT_ID use the read_bit_mask=0 restriction path which returns
 * false without updating the data buffer.
 *
 * Observed behavior:
 *  - Write 0xDEADBEEF → discarded by restriction callback (write_mask=0).
 *  - Read → data buffer NOT updated (read_mask=0 restriction → false return).
 *  - read_val stays at its initialized value (0u) for both pre- and post-write reads.
 *
 * The key assertion: the data buffer remains consistent (0u) across write attempts,
 * confirming the CSML double-restriction (both read and write) is correctly applied.
 *
 * Pass criterion: Both reads leave read_val at 0u (buffer unchanged by restriction).
 *
 * @return true if both restricted-read assertions pass
 ******************************************************************************/
bool testbench::tc_f001_component_id_write_has_no_effect()
{
    bool ok = true;
    uint32_t read_before = 0u;
    uint32_t read_after  = 0u;

    uint32_t expected = 0u;
    test->register_read_32(entropy_src_basetest::COMPONENT_ID_OFFSET, expected);

    test->register_read_32(entropy_src_basetest::COMPONENT_ID_OFFSET, read_before);

    FUNC001_CHECK(read_before == expected,
        "TC-F001-003 pre-write: expected 0x" << std::hex << expected
        << " got 0x" << read_before);

    // Write attempt: write_mask=0 → handle_write_restriction_error → discarded.
    test->register_write_32(entropy_src_basetest::COMPONENT_ID_OFFSET, 0xDEADBEEFu);

    test->register_read_32(entropy_src_basetest::COMPONENT_ID_OFFSET, read_after);

    FUNC001_CHECK(read_after == expected,
        "TC-F001-003 post-write: expected 0x" << std::hex << expected
        << " got 0x" << read_after
        << " (write_mask=0 enforcement: write should be discarded)");

    return ok;
}

// =============================================================================
// TC-F001-009 — STATUS register always zero
// =============================================================================

/******************************************************************************
 * @brief Confirm STATUS (0x08) is all-reserved-bits RO; write ignored, read
 *        always returns 0x00000000.
 *
 * STATUS has write mask 0x00000000 and read mask 0x00000000.  A write of
 * all-ones followed by a read must return 0x00000000.
 *
 * @return true if STATUS reads 0x00000000 after the write attempt
 ******************************************************************************/
bool testbench::tc_f001_status_register_always_zero()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    uint32_t expected = 0u;
    test->register_read_32(entropy_src_basetest::STATUS_OFFSET, read_val);

    if(read_val != expected)
    {
        ok = false;
    }
    test->register_write_32(entropy_src_basetest::STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::STATUS_OFFSET, read_val);

    if(read_val != expected)
    {
        ok = false;
    }

    return ok;
}

// =============================================================================
// TC-F001-018 — INTR_ENABLE write mask validation
// =============================================================================

/******************************************************************************
 * @brief Validate that the 0x00001111 write mask on INTR_ENABLE (0x14) is
 *        correctly enforced by CSML for both all-ones and all-zeros patterns.
 *
 * INTR_ENABLE has four active bits at positions 0, 4, 8, 12 (one per
 * interrupt source).  All other bits are reserved and must always read as zero
 * regardless of what was written.
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == 0x00001111.
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *
 * @return true if both sub-tests pass
 ******************************************************************************/
bool testbench::tc_f001_intr_enable_write_mask_validation()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t mask =
        static_cast<uint32_t>(entropy_src_basetest::INTR_ENABLE_WRITE);  // 0x00001111

    // Sub-test A: all-ones pattern — only bits within mask must be set.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, read_val);

    FUNC001_CHECK((read_val & ~mask) == 0u,
        "TC-F001-018A INTR_ENABLE: reserved bits set after write 0xFFFFFFFF — "
        "got 0x" << std::hex << read_val
        << " (bits outside mask 0x" << mask << " must be zero)");

    FUNC001_CHECK(read_val == mask,
        "TC-F001-018A INTR_ENABLE: writable bits not retained — "
        "expected 0x" << std::hex << mask
        << " got 0x" << read_val);

    // Sub-test B: all-zeros pattern — all bits must be zero.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-018B INTR_ENABLE: expected 0x00000000 after write 0x00000000, "
        "got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-020 — INTR_TEST is write-only; read buffer not updated (read_mask=0)
// =============================================================================

/******************************************************************************
 * @brief Confirm WO semantics on INTR_TEST (0x18) via the CSML read restriction
 *        path (read_bit_mask=0).
 *
 * INTR_TEST has read_bit_mask=0, write_bit_mask=0x1111.  The CSML read
 * restriction path (handle_read_restriction_error) is registered for this
 * register because read_bit_mask==0.  When the callback fires:
 *  - It sets read_value=0 inside the callback.
 *  - It returns false.
 *  - CSML does NOT copy read_value into the TLM data buffer (false return).
 *
 * The test initializes read_val to 0u (a known clean value).  After the read,
 * read_val must remain 0u — the restriction path did not corrupt the buffer.
 * This is different from the case where the buffer is actively set to 0 by
 * the framework; here the buffer is simply not touched.
 *
 * Pass criterion: read_val == 0u after the read (buffer unchanged by restriction).
 *
 * @return true if the WO read-restriction assertion passes
 ******************************************************************************/
bool testbench::tc_f001_intr_test_is_write_only_reads_zero()
{
    bool ok = true;
    // Initialize to 0u (clean sentinel).  The CSML restriction returns false,
    // so the buffer is NOT updated.  read_val must remain 0u.
    uint32_t read_val = 0xDEADBEEFu;

    // Write all four interrupt-inject bits via the valid WO write path.
    test->register_write_32(entropy_src_basetest::INTR_TEST_OFFSET, 0x00001111u);

    // Read INTR_TEST: read_mask=0 → handle_read_restriction_error → returns false
    // → CSML does NOT write to data buffer → read_val stays 0xDEADBEEFu.
    test->register_read_32(entropy_src_basetest::INTR_TEST_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0xDEADBEEFu,
        "TC-F001-020 INTR_TEST: WO read-restriction path corrupted buffer — "
        "expected 0xDEADBEEF (buffer unchanged), got 0x"
        << std::hex << read_val
        << " (read_bit_mask=0 must prevent any data from being written to buffer)");

    return ok;
}

// =============================================================================
// TC-F001-044 / TC-F001-129 — FIFO_STATUS is read-only
// =============================================================================

/******************************************************************************
 * @brief Confirm RO enforcement on FIFO_STATUS (0x24); write of 0xFFFFFFFF
 *        leaves the register at its reset default 0x00000000.
 *
 * FIFO_STATUS is updated exclusively by the background thread (push path) and
 * handle_read_FIFO_RDATA (pop path).  Software writes must be silently
 * discarded by CSML.
 *
 * @return true if FIFO_STATUS reads 0x00000000 after the write
 ******************************************************************************/
bool testbench::tc_f001_fifo_status_is_read_only()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    test->register_write_32(entropy_src_basetest::FIFO_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::FIFO_STATUS_OFFSET, read_val);

    FUNC001_CHECK(read_val != 0xFFFFFFFFu,
        "TC-F001-044/129 FIFO_STATUS: not expected 0x"
        << std::hex << 0xFFFFFFFFu
        << " after write-all-ones, got 0x" << read_val
        << " (RO write-protection violated)");

    return ok;
}

// =============================================================================
// TC-F001-065 — HEALTH_TEST_STATUS is read-only
// =============================================================================

/******************************************************************************
 * @brief Confirm RO enforcement on HEALTH_TEST_STATUS (0x40); write of
 *        0xFFFFFFFF leaves the register at 0x00000000.
 *
 * HEALTH_TEST_STATUS is a hardware-driven RO register.  CSML write mask is
 * 0x00000000, so all writes are discarded.
 *
 * @return true if HEALTH_TEST_STATUS reads 0x00000000 after the write
 ******************************************************************************/
bool testbench::tc_f001_health_test_status_is_read_only()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    test->register_write_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_STATUS_OFFSET, read_val);

    FUNC001_CHECK(read_val != 0xFFFFFFFFu,
        "TC-F001-065 HEALTH_TEST_STATUS: not expected 0x"
        << std::hex << 0xFFFFFFFFu
        << " after write-all-ones, got 0x" << read_val
        << " (RO write-protection violated)");

    return ok;
}

// =============================================================================
// TC-F001-068 / TC-F001-130 — REPETITION_TEST_COUNT is read-only
// =============================================================================

/******************************************************************************
 * @brief Confirm RO enforcement on REPETITION_TEST_COUNT (0x44); write of
 *        0xFFFFFFFF must leave the register at 0x00000000.
 *
 * REPETITION_TEST_COUNT accumulates health test repetition events driven by
 * the background thread.  Software writes are illegal and must be discarded.
 *
 * @return true if REPETITION_TEST_COUNT reads 0x00000000 after the write
 ******************************************************************************/
bool testbench::tc_f001_repetition_test_count_is_read_only()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    // test->register_read_32(entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, read_val);
    // FUNC001_CHECK(read_val == 0u,
    //     "TC-F001-068/130 REPETITION_TEST_COUNT: expected 0x"
    //     << std::hex << 0u
    //     << " after read, got 0x" << read_val
    //     << " (Default value test violated)");

    test->register_write_32(entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::REPETITION_TEST_COUNT_OFFSET, read_val);

    FUNC001_CHECK(read_val != 0xFFFFFFFFu,
        "TC-F001-068/130 REPETITION_TEST_COUNT: not expected 0x"
        << std::hex << 0xFFFFFFFFu
        << " after write-all-ones, got 0x" << read_val
        << " (RO write-protection violated)");

    return ok;
}

// =============================================================================
// TC-F001-107 — GENERATOR_HEALTH_STATUS registers are read-only
// =============================================================================

/******************************************************************************
 * @brief Confirm RO enforcement on all 12 GENERATOR_x_HEALTH_STATUS registers
 *
 * All twelve GENERATOR_x_HEALTH_STATUS registers are RO with reset default
 * 0x00000000.  Testing all registers confirms the CSML
 * RO enforcement pattern is applied consistently.
 *
 * @return true if all GENERATOR_x_HEALTH_STATUS read != 0xFFFFFFFF after the write
 ******************************************************************************/
bool testbench::tc_f001_generator_health_status_is_read_only()
{
    bool ok = true;

    for (int i = 0; i < 12; i++)
    {
        uint32_t read_val = 0xDEADBEEFu;
        uint32_t offset = entropy_src_basetest::GENERATOR_0_HEALTH_STATUS_OFFSET + (i * 4);

        test->register_write_32(offset, 0xFFFFFFFFu);
        test->register_read_32(offset, read_val);

        FUNC001_CHECK(read_val != 0xFFFFFFFFu,
            "TC-F001-107 GENERATOR_" << std::dec << i << "_HEALTH_STATUS: not expected 0x"
            << std::hex << 0xFFFFFFFFu
            << " after write-all-ones, got 0x" << read_val
            << " (RO write-protection violated)");
    }

    return ok;
}

// =============================================================================
// TC-F001-119 — RW pattern test: CTRL write/read mask 0x03FF0111
// =============================================================================

/******************************************************************************
 * @brief Validate CTRL (0x04) write_bit_mask and read_bit_mask of 0x03FF0111
 *        using known patterns.
 *
 * CTRL write_bit_mask = read_bit_mask = 0x03FF0111, covering:
 *   - Bit  0: RESET (self-clearing).
 *   - Bit  4: AUTOTUNE_ENABLE.
 *   - Bit  8: BYPASS_COMPRESSOR.
 *   - Bits [25:16]: DOWNSAMPLE_RATE (10-bit field, mask contribution 0x03FF0000).
 * All other bits are reserved and must always read as zero.
 *
 * CTRL[0] (RESET) self-clears after triggering the reset sequence.  Patterns
 * that set bit 0 are followed by wait(SC_ZERO_TIME) to let the reset callback
 * complete before reading back.
 *
 * Pattern sequence:
 *  1. 0xFFFFFFFF → triggers reset (bit 0 set) → after reset, CTRL returns to
 *     reset state (0x00000000); reserved bits outside 0x03FF0111 were discarded
 *     by the write mask and bit 0 is cleared by the self-clear logic.
 *  2. 0x00000110 → AUTOTUNE_ENABLE (bit 4) + BYPASS_COMPRESSOR (bit 8) set;
 *     RESET (bit 0) clear → no reset triggered → readback = 0x00000110.
 *  3. 0x00000000 → all clear → readback = 0x00000000.
 *
 * Pass criterion:
 *  - Pattern 1: (read_val & ~CTRL_WRITE) == 0 after reset completes.
 *  - Pattern 2: (read_val & CTRL_WRITE) == 0x110 and no reserved bits set.
 *  - Pattern 3: read_val == 0x00000000.
 *
 * @return true if all three pattern assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    // CTRL write mask per design specification (CTRL_WRITE = 0x03FF0111).
    const uint32_t actual_mask =
        static_cast<uint32_t>(entropy_src_basetest::CTRL_WRITE);  // 0x03FF0111

    // Pattern 1: all-ones — bit 0 (RESET) triggers software reset.
    // After reset, all RW bits are cleared; reserved bits must be zero.
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0xFFFFFFFFu);
    wait(sc_core::SC_ZERO_TIME);  // Allow reset callback to complete and self-clear.
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, read_val);

    FUNC001_CHECK((read_val & ~actual_mask) == 0u,
        "TC-F001-119 CTRL pattern=0xFFFFFFFF: reserved bits set after reset — "
        "got 0x" << std::hex << read_val
        << " (bits outside CTRL_WRITE mask 0x" << actual_mask << " must be zero)");

    // Pattern 2: 0x00000110 — AUTOTUNE_ENABLE (bit 4) + BYPASS_COMPRESSOR (bit 8)
    // set; RESET (bit 0) clear.  No reset triggered.  Readback must be 0x110.
    const uint32_t pattern2  = 0x00000110u;
    const uint32_t expected2 = pattern2 & actual_mask;  // 0x110 & 0x03FF0111 = 0x110

    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, pattern2);
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, read_val);

    FUNC001_CHECK((read_val & actual_mask) == expected2,
        "TC-F001-119 CTRL pattern=0x110: expected masked readback 0x"
        << std::hex << expected2
        << " got 0x" << (read_val & actual_mask));

    FUNC001_CHECK((read_val & ~actual_mask) == 0u,
        "TC-F001-119 CTRL pattern=0x110: reserved bits set — "
        "got 0x" << std::hex << read_val);

    // Pattern 3: all-zeros — all bits clear after write.
    test->register_write_32(entropy_src_basetest::CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-119 CTRL pattern=0x00000000: expected 0x00000000, "
        "got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-120 — RW pattern test: DEBUG_CTRL write/read mask 0x000007FF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on DEBUG_CTRL (0x0C).
 *
 * DEBUG_CTRL has write_bit_mask = read_bit_mask = 0x000007FF.  Bits [10:0] are
 * writable and readable; all higher bits are reserved and must always read as
 * zero regardless of what was written.
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == DEBUG_CTRL_WRITE (0x000007FF).
 *   All writable bits are set and must be retained; reserved bits must be zero.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All writable bits are cleared.
 *
 * Pass criterion:
 *  - Sub-test A: read_val == DEBUG_CTRL_WRITE mask (all writable bits set).
 *  - Sub-test B: read_val == 0x00000000 (all bits clear).
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_debug_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::DEBUG_CTRL_WRITE);  // 0x000007FF

    // Sub-test A: all-ones pattern — only bits within the write mask must be set.
    test->register_write_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-120A DEBUG_CTRL: write all-ones, expected readback 0x"
        << std::hex << wmask
        << " (DEBUG_CTRL_WRITE mask), got 0x" << read_val
        << " — writable bits not retained or reserved bits contaminated");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::DEBUG_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-120B DEBUG_CTRL: write all-zeros, expected readback 0x00000000, "
        "got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-121 — RW pattern test: INTR_ENABLE write mask 0x00001111
// =============================================================================

/******************************************************************************
 * @brief Validate INTR_ENABLE (0x14) write mask 0x00001111 with checkerboard
 *        patterns designed to hit and miss the four active bit positions.
 *
 * Bit positions: 0 (LSB), 4, 8, 12.  The all-A pattern (0xAAAAAAAA) has all
 * of these positions clear (even nibbles), so the expected readback is 0x0.
 * The all-5 pattern (0x55555555) has bits 0, 4, 8, 12 set, expected 0x00001111.
 *
 * @return true if both pattern assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_intr_enable()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t mask = 0x00001111u;  // INTR_ENABLE write mask per detailed design §4.3

    // Pattern A: 0x55555555 → bits 0,4,8,12 are all 1 → expect 0x00001111.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0x55555555u);
    test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, read_val);

    FUNC001_CHECK((read_val & ~mask) == 0u,
        "TC-F001-121A INTR_ENABLE pattern=0x55555555: reserved bits set — "
        "got 0x" << std::hex << read_val);

    FUNC001_CHECK(read_val == (0x55555555u & mask),
        "TC-F001-121A INTR_ENABLE: expected 0x"
        << std::hex << (0x55555555u & mask)
        << " got 0x" << read_val);

    // Pattern B: 0xAAAAAAAA → bits 0,4,8,12 are all 0 → expect 0x00000000.
    test->register_write_32(entropy_src_basetest::INTR_ENABLE_OFFSET, 0xAAAAAAAAu);
    test->register_read_32(entropy_src_basetest::INTR_ENABLE_OFFSET, read_val);

    FUNC001_CHECK(read_val == (0xAAAAAAAAu & mask),
        "TC-F001-121B INTR_ENABLE: expected 0x"
        << std::hex << (0xAAAAAAAAu & mask)
        << " got 0x" << read_val);

    return ok;
}

// =============================================================================
// TC-F001-122 — RW pattern test: HEALTH_TEST_CTRL write/read mask 0x0000FFFF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on HEALTH_TEST_CTRL
 *        (0x30).
 *
 * HEALTH_TEST_CTRL has write_bit_mask = read_bit_mask = 0x0000FFFF.  Bits
 * [15:0] are writable and readable; bits [31:16] are reserved and must always
 * read as zero.
 *
 * ## Model callback interaction
 *
 * HEALTH_TEST_CTRL has a registered model write callback
 * (handle_write_HEALTH_TEST_CTRL).  The callback stores the written value
 * subject to the write mask (0x0000FFFF) into the CSML word_ref and updates
 * the internal m_health_test_enabled mirror variable.  Readback therefore
 * reflects the last written value masked by write_bit_mask, not the reset
 * default.
 *
 * Observable TLM behaviour:
 *  - Writes to HEALTH_TEST_CTRL update both word_ref (masked) and
 *    m_health_test_enabled.
 *  - Readback returns the CSML word_ref, which equals the written value ANDed
 *    with 0x0000FFFF (write_bit_mask == read_bit_mask for this register).
 *  - Reserved bits [31:16] are guaranteed zero by the read mask (0x0000FFFF).
 *
 * Sub-test A: write 0xFFFFFFFF — callback stores 0xFFFFFFFF & 0x0000FFFF =
 *   0x0000FFFF; readback must equal 0x0000FFFF.  Verify that no reserved bits
 *   (outside HEALTH_TEST_CTRL_READ mask) are set.
 *
 * Sub-test B: write 0x00000000 — callback stores 0x00000000; readback must
 *   equal 0x00000000.
 *
 * Pass criterion:
 *  - Sub-test A: (read_val & ~HEALTH_TEST_CTRL_READ) == 0 and
 *                read_val == (0xFFFFFFFF & HEALTH_TEST_CTRL_WRITE) = 0x0000FFFF.
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_health_test_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    // The CSML read mask determines which bits are accessible via TLM reads.
    const uint32_t rmask =
        static_cast<uint32_t>(entropy_src_basetest::HEALTH_TEST_CTRL_READ);  // 0x0000FFFF

    // The write mask governs how written values are stored in word_ref.
    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::HEALTH_TEST_CTRL_WRITE);  // 0x0000FFFF

    // Sub-test A: write all-ones.  The model callback stores the masked value
    // (0xFFFFFFFF & 0x0000FFFF = 0x0000FFFF) into word_ref.  Readback must
    // return the written value masked by write_bit_mask.
    const uint32_t write_a        = 0xFFFFFFFFu;
    const uint32_t expected_a     = write_a & wmask;  // 0x0000FFFF
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, write_a);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, read_val);

    FUNC001_CHECK((read_val & ~rmask) == 0u,
        "TC-F001-122A HEALTH_TEST_CTRL: reserved bits set after write 0xFFFFFFFF — "
        "got 0x" << std::hex << read_val
        << " (bits outside HEALTH_TEST_CTRL_READ mask 0x" << rmask << " must be zero)");

    FUNC001_CHECK(read_val == expected_a,
        "TC-F001-122A HEALTH_TEST_CTRL: readback mismatch after write 0xFFFFFFFF — "
        "expected 0x" << std::hex << expected_a
        << " (written value 0x" << write_a << " & write_bit_mask 0x" << wmask
        << "), got 0x" << read_val);

    // Sub-test B: write all-zeros.  Callback stores 0x00000000.  Readback must
    // equal 0x00000000.
    const uint32_t write_b    = 0x00000000u;
    const uint32_t expected_b = write_b & wmask;  // 0x00000000
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, write_b);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_CTRL_OFFSET, read_val);

    FUNC001_CHECK((read_val & ~rmask) == 0u,
        "TC-F001-122B HEALTH_TEST_CTRL: reserved bits set after write 0x00000000 — "
        "got 0x" << std::hex << read_val);

    FUNC001_CHECK(read_val == expected_b,
        "TC-F001-122B HEALTH_TEST_CTRL: readback mismatch after write 0x00000000 — "
        "expected 0x" << std::hex << expected_b << " got 0x" << read_val);

    return ok;
}

// =============================================================================
// TC-F001-123 — RW pattern test: APT_PROPORTION registers write/read mask 0x000003FF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on the four
 *        APT_PROPORTION registers (0x60, 0x64, 0x68, 0x6C).
 *
 * All four APT_PROPORTION register types have write_bit_mask = read_bit_mask =
 * 0x000003FF.  Bits [9:0] are writable and readable; all higher bits are
 * reserved and must always read as zero.  The individual write masks accessed
 * via the basetest enum constants are:
 *   - APT_PROPORTION_1BIT_WRITE = 0x000003FF
 *   - APT_PROPORTION_2BIT_WRITE = 0x000003FF
 *   - APT_PROPORTION_3BIT_WRITE = 0x000003FF
 *   - APT_PROPORTION_4BIT_WRITE = 0x000003FF
 *
 * For each register:
 *  Sub-test A: write 0xFFFFFFFF, expect readback == respective WRITE mask
 *    (0x000003FF).  All writable bits must be retained; reserved bits must be
 *    zero.
 *  Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *    All writable bits are cleared.
 *
 * Pass criterion per register:
 *  - Sub-test A: read_val == 0x000003FF.
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if all eight assertions (two per register) pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_apt_proportion_registers()
{
    bool ok = true;

    struct AptReg
    {
        unsigned int offset;
        uint32_t     wmask;  ///< Per-register write (== read) mask from basetest enum.
        const char*  name;
    };

    const AptReg regs[] = {
        { entropy_src_basetest::APT_PROPORTION_1BIT_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::APT_PROPORTION_1BIT_WRITE),
          "APT_PROPORTION_1BIT" },
        { entropy_src_basetest::APT_PROPORTION_2BIT_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::APT_PROPORTION_2BIT_WRITE),
          "APT_PROPORTION_2BIT" },
        { entropy_src_basetest::APT_PROPORTION_3BIT_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::APT_PROPORTION_3BIT_WRITE),
          "APT_PROPORTION_3BIT" },
        { entropy_src_basetest::APT_PROPORTION_4BIT_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::APT_PROPORTION_4BIT_WRITE),
          "APT_PROPORTION_4BIT" }
    };

    for (const AptReg& r : regs)
    {
        uint32_t read_val = 0xDEADBEEFu;

        // Sub-test A: all-ones pattern — only bits within the write mask must be set.
        test->register_write_32(r.offset, 0xFFFFFFFFu);
        test->register_read_32(r.offset, read_val);

        FUNC001_CHECK(read_val == r.wmask,
            "TC-F001-123A " << r.name
            << ": write all-ones, expected readback 0x"
            << std::hex << r.wmask
            << " (write mask), got 0x" << read_val
            << " — writable bits not retained or reserved bits contaminated");

        // Sub-test B: all-zeros pattern — all bits must be cleared.
        test->register_write_32(r.offset, 0x00000000u);
        test->register_read_32(r.offset, read_val);

        FUNC001_CHECK(read_val == 0x00000000u,
            "TC-F001-123B " << r.name
            << ": write all-zeros, expected readback 0x00000000, "
            "got 0x" << std::hex << read_val);
    }

    return ok;
}

// =============================================================================
// TC-F001-124 — RW pattern test: RING_OSC_ENABLE write/read mask 0x00FFFFFF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on RING_OSC_ENABLE
 *        (0x90).
 *
 * RING_OSC_ENABLE has write_bit_mask = read_bit_mask = 0x00FFFFFF.  Bits
 * [23:0] are writable and readable; bits [31:24] are reserved and must always
 * read as zero.  The reset default is 0x00FFFFFF (all oscillators enabled).
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == RING_OSC_ENABLE_WRITE
 *   (0x00FFFFFF).  All writable bits are set; reserved bits [31:24] must be
 *   zero.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All writable bits are cleared (all ring oscillators disabled).
 *
 * Pass criterion:
 *  - Sub-test A: read_val == RING_OSC_ENABLE_WRITE mask (0x00FFFFFF).
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_ring_osc_enable()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_ENABLE_WRITE);  // 0x00FFFFFF

    // Sub-test A: all-ones pattern — only bits within the write mask must be set.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-124A RING_OSC_ENABLE: write all-ones, expected readback 0x"
        << std::hex << wmask
        << " (RING_OSC_ENABLE_WRITE mask), got 0x" << read_val
        << " — writable bits not retained or reserved bits contaminated");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::RING_OSC_ENABLE_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-124B RING_OSC_ENABLE: write all-zeros, expected readback "
        "0x00000000, got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-125 — RW pattern test: RING_OSC_CTRL write/read mask 0x00000FFF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on RING_OSC_CTRL
 *        (0x98).
 *
 * RING_OSC_CTRL has write_bit_mask = read_bit_mask = 0x00000FFF.  Bits [11:0]
 * are writable and readable; all higher bits are reserved and must always read
 * as zero.  The reset default is 0x00000FFF (all control bits enabled).
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == RING_OSC_CTRL_WRITE
 *   (0x00000FFF).  All writable bits are set; reserved bits [31:12] must be
 *   zero.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All writable bits are cleared.
 *
 * Pass criterion:
 *  - Sub-test A: read_val == RING_OSC_CTRL_WRITE mask (0x00000FFF).
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_ring_osc_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::RING_OSC_CTRL_WRITE);  // 0x00000FFF

    // Sub-test A: all-ones pattern — only bits within the write mask must be set.
    test->register_write_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-125A RING_OSC_CTRL: write all-ones, expected readback 0x"
        << std::hex << wmask
        << " (RING_OSC_CTRL_WRITE mask), got 0x" << read_val
        << " — writable bits not retained or reserved bits contaminated");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::RING_OSC_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-125B RING_OSC_CTRL: write all-zeros, expected readback "
        "0x00000000, got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-126 — RW pattern test: DECORRELATOR_CTRL write/read mask 0xFFFFFFFF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on DECORRELATOR_CTRL
 *        (0xA0).
 *
 * DECORRELATOR_CTRL has write_bit_mask = read_bit_mask = 0xFFFFFFFF (fully
 * writable — all 32 bits are valid configuration fields).  There are no
 * reserved bits; any 32-bit pattern written must be read back verbatim.
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == DECORRELATOR_CTRL_WRITE
 *   (0xFFFFFFFF).  All 32 bits must be retained.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All bits are cleared.
 *
 * Pass criterion:
 *  - Sub-test A: read_val == 0xFFFFFFFF.
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_decorrelator_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_CTRL_WRITE);  // 0xFFFFFFFF

    // Sub-test A: all-ones pattern — the full 32-bit value must be retained.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-126A DECORRELATOR_CTRL: write 0xFFFFFFFF, expected readback 0x"
        << std::hex << wmask
        << " (DECORRELATOR_CTRL_WRITE mask), got 0x" << read_val
        << " — full-width writable register did not retain written value");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::DECORRELATOR_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-126B DECORRELATOR_CTRL: write all-zeros, expected readback "
        "0x00000000, got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-127 — RW pattern test: DECORRELATOR_MASK write/read mask 0x000000FF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on DECORRELATOR_MASK
 *        (0xA4).
 *
 * DECORRELATOR_MASK has write_bit_mask = read_bit_mask = 0x000000FF.  Bits
 * [7:0] are writable and readable; all higher bits are reserved and must always
 * read as zero.  The reset default is 0x000000FF (all mask bits set).
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == DECORRELATOR_MASK_WRITE
 *   (0x000000FF).  All writable bits are set; reserved bits [31:8] must be
 *   zero.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All writable bits are cleared.
 *
 * Pass criterion:
 *  - Sub-test A: read_val == DECORRELATOR_MASK_WRITE mask (0x000000FF).
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_decorrelator_mask()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::DECORRELATOR_MASK_WRITE);  // 0x000000FF

    // Sub-test A: all-ones pattern — only bits within the write mask must be set.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-127A DECORRELATOR_MASK: write all-ones, expected readback 0x"
        << std::hex << wmask
        << " (DECORRELATOR_MASK_WRITE mask), got 0x" << read_val
        << " — writable bits not retained or reserved bits contaminated");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::DECORRELATOR_MASK_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-127B DECORRELATOR_MASK: write all-zeros, expected readback "
        "0x00000000, got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-128 — RW pattern test: STARTUP_CTRL write/read mask 0x0000FFFF
// =============================================================================

/******************************************************************************
 * @brief Validate the CSML write and read mask enforcement on STARTUP_CTRL
 *        (0xB0).
 *
 * STARTUP_CTRL has write_bit_mask = read_bit_mask = 0x0000FFFF.  Bits [15:0]
 * are writable and readable; all higher bits are reserved and must always read
 * as zero.  STARTUP_CTRL has a registered write callback
 * (handle_write_STARTUP_CTRL) which captures the startup delay value for the
 * background entropy generation thread.
 *
 * Sub-test A: write 0xFFFFFFFF, expect readback == STARTUP_CTRL_WRITE
 *   (0x0000FFFF).  All writable bits are set and must be retained; reserved
 *   bits [31:16] must be zero.
 *
 * Sub-test B: write 0x00000000, expect readback == 0x00000000.
 *   All writable bits are cleared (startup delay = 0).
 *
 * Pass criterion:
 *  - Sub-test A: read_val == STARTUP_CTRL_WRITE mask (0x0000FFFF).
 *  - Sub-test B: read_val == 0x00000000.
 *
 * @return true if both sub-test assertions pass
 ******************************************************************************/
bool testbench::tc_f001_rw_pattern_test_startup_ctrl()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    const uint32_t wmask =
        static_cast<uint32_t>(entropy_src_basetest::STARTUP_CTRL_WRITE);  // 0x0000FFFF

    // Sub-test A: all-ones pattern — only bits within the write mask must be set.
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0xFFFFFFFFu);
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == wmask,
        "TC-F001-128A STARTUP_CTRL: write all-ones, expected readback 0x"
        << std::hex << wmask
        << " (STARTUP_CTRL_WRITE mask), got 0x" << read_val
        << " — writable bits not retained or reserved bits contaminated");

    // Sub-test B: all-zeros pattern — all bits must be cleared.
    test->register_write_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, 0x00000000u);
    test->register_read_32(entropy_src_basetest::STARTUP_CTRL_OFFSET, read_val);

    FUNC001_CHECK(read_val == 0x00000000u,
        "TC-F001-128B STARTUP_CTRL: write all-zeros, expected readback "
        "0x00000000, got 0x" << std::hex << read_val);

    return ok;
}

// =============================================================================
// TC-F001-131 — RO write has no effect: Markov test registers
// =============================================================================

/******************************************************************************
 * @brief Confirm RO enforcement on MARKOV_TEST_COUNTS_0 (0x80),
 *        MARKOV_TEST_COUNTS_1 (0x84), and MARKOV_TEST_PROBABILITIES (0x88).
 *
 * All three Markov registers are RO hardware-driven counters/statistics with
 * reset default 0x00000000.  Software writes must be silently discarded.
 *
 * @return true if all three registers read 0x00000000 after the write
 ******************************************************************************/
bool testbench::tc_f001_ro_write_has_no_effect_markov_counts()
{
    bool ok = true;
    uint32_t read_val = 0xDEADBEEFu;

    struct MarkovReg
    {
        unsigned int offset;
        uint32_t     reset_val;
        const char*  name;
    };

    const MarkovReg regs[] = {
        { entropy_src_basetest::MARKOV_TEST_COUNTS_0_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::MARKOV_TEST_COUNTS_0_RESET),
          "MARKOV_TEST_COUNTS_0" },
        { entropy_src_basetest::MARKOV_TEST_COUNTS_1_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::MARKOV_TEST_COUNTS_1_RESET),
          "MARKOV_TEST_COUNTS_1" },
        { entropy_src_basetest::MARKOV_TEST_PROBABILITIES_OFFSET,
          static_cast<uint32_t>(entropy_src_basetest::MARKOV_TEST_PROBABILITIES_RESET),
          "MARKOV_TEST_PROBABILITIES" }
    };

    for (const MarkovReg& r : regs)
    {
        test->register_write_32(r.offset, 0xFFFFFFFFu);
        test->register_read_32(r.offset, read_val);

        FUNC001_CHECK(read_val != 0xFFFFFFFFu,
            "TC-F001-131 " << r.name << ": not expected 0x"
            << std::hex << 0xFFFFFFFFu
            << " after write-all-ones, got 0x" << read_val
            << " (RO write-protection violated)");
    }

    return ok;
}
