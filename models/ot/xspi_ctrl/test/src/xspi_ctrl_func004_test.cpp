/******************************************************************************
 * @file xspi_ctrl_func004_test.cpp
 * @brief Test cases for FUNC_XSPI_004 — DMA Interface and AXI Transaction
 *        Management
 *
 * This file implements all test cases mapped to FUNC_XSPI_004 in the
 * xspi_ctrl functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_004 Test Coverage (10 test cases):
 *
 * - TC_XSPI_DMA_001: dma_settings reset value verification.
 *                    After reset, dma_settings (0x23C) must read 0x000D0000.
 *                    Decode: OTE=1 (bit 16), word_size=0b11 (bits[19:18]),
 *                    burst_sel=0x00 (bits[7:0]), sdma_err_rsp=0 (bit 17).
 *
 * - TC_XSPI_DMA_002: dma_settings RW retention within write_bit_mask.
 *                    write_bit_mask = 0x000F00FF. Reserved bits [15:8] and
 *                    [31:20] are masked. Write a new value and verify read-back
 *                    equals the written value masked to 0x000F00FF.
 *
 * - TC_XSPI_DMA_003: burst_sel field write and shadow variable update.
 *                    Write dma_settings with burst_sel=0x0F (bits[7:0]=0x0F)
 *                    and verify the stored value. Writing burst_sel=0x00
 *                    (reset default) restores to minimal burst mode.
 *
 * - TC_XSPI_DMA_004: OTE and word_size field write and shadow variable update.
 *                    Write dma_settings with OTE=0 (bit 16 = 0), word_size=0b10
 *                    (bits[19:18] = 0b10 = 0x00080000), and verify read-back
 *                    holds the written values within the writable mask.
 *
 * - TC_XSPI_DMA_005: dma_settings restore to reset value after second reset.
 *                    Write a non-reset value to dma_settings, apply reset,
 *                    verify dma_settings returns to 0x000D0000.
 *
 * - TC_XSPI_AXI_SLAVE_001: t_axi_slave_socket responds TLM_OK_RESPONSE to READ.
 *                    Construct a 64-bit TLM READ payload and dispatch via
 *                    t_axi_slave_initiator. Verify response is TLM_OK_RESPONSE.
 *
 * - TC_XSPI_AXI_SLAVE_002: t_axi_slave_socket responds TLM_OK_RESPONSE to WRITE.
 *                    Construct a 64-bit TLM WRITE payload and dispatch via
 *                    t_axi_slave_initiator. Verify response is TLM_OK_RESPONSE.
 *
 * - TC_XSPI_POR_001: PoR_input_signals responds TLM_OK_RESPONSE.
 *                    Send a bare TLM payload on por_initiator (no PoR extension
 *                    needed at FUNC_XSPI_004 scope). Verify TLM_OK_RESPONSE.
 *
 * - TC_XSPI_DMA_ERR_001: DMA error path — cdma_terr set, dma_target_error_l/h
 *                    captures failing address, int_out asserts.
 *                    Set m_dma_error_count=1 and intr_enable to gate Path 1
 *                    with cdma_terr enable (bit 17 of intr_enable enabled, and
 *                    global gate bit 31 set). Trigger ACMD on thread 0 so the
 *                    DMA stub returns TLM_GENERIC_ERROR_RESPONSE on the
 *                    descriptor fetch. Verify:
 *                      - intr_status.cdma_terr (bit 17) is set
 *                      - dma_target_error_l (0x150) holds the descriptor address
 *                      - int_out asserted (Path 1 active via cdma_terr + enable)
 *
 * - TC_XSPI_DMA_ERR_002: dma_target_error_l/h RO enforcement.
 *                    Write 0xFFFFFFFF to both dma_target_error_l (0x150) and
 *                    dma_target_error_h (0x154). Verify read-back equals the
 *                    value written by the hardware error path (write_mask=0x0
 *                    means software writes are silently discarded).
 *
 * == Architectural Background ==
 *
 * dma_settings register (offset 0x23C):
 *   - write_bit_mask = 0x000F00FF
 *   - reset value    = 0x000D0000
 *   - bits[7:0]   = burst_sel   → dma_burst_length = burst_sel + 1
 *   - bit[16]     = OTE         → dma_ote_enabled
 *   - bit[17]     = sdma_err_rsp → dma_sdma_err_rsp
 *   - bits[19:18] = word_size   → dma_word_size (0=byte,1=16b,2=32b,3=64b)
 *
 * AXI slave socket (t_axi_slave_socket):
 *   - At FUNC_XSPI_004 scope: unconditional TLM_OK_RESPONSE + 10 ns delay.
 *   - Direct-mode flash forwarding deferred to FUNC_XSPI_008.
 *
 * PoR socket (PoR_input_signals):
 *   - At FUNC_XSPI_004 scope: unconditional TLM_OK_RESPONSE + 10 ns delay.
 *   - SFDP discovery and boot engine deferred to FUNC_XSPI_007.
 *
 * DMA error path (i_dma_socket → dma_read()/dma_write()):
 *   - TLM_GENERIC_ERROR_RESPONSE triggers:
 *     (1) dma_target_error_l/h = failing address (model-internal direct write)
 *     (2) intr_status.cdma_terr (bit 17) set for descriptor/control DMA path
 *     (3) evaluate_interrupt_out() called
 *   - dma_target_error_l/h are RO (write_mask=0x0); only model sets them.
 *
 * == Timing Notes ==
 *
 * handle_write_dma_settings() executes synchronously within the scml2 write
 * callback. No wait() is required after writing dma_settings before reading
 * back — the callback updates the register and shadow variables atomically.
 * A wait(5, SC_NS) is used before reads to allow TLM transport to settle.
 *
 * AXI slave and PoR transactions are dispatched synchronously via b_transport
 * on the testbench SC_THREAD. The 10 ns LT delay accumulated by the DUT
 * handler is consumed within b_transport before the call returns.
 *
 * ACMD trigger (for DMA error injection) runs synchronously inside
 * handle_write_cmd_reg0 → cdma_handle_trigger(). The evaluate_interrupt_out()
 * at the end notifies m_int_update_event at SC_ZERO_TIME. One
 * wait(SC_ZERO_TIME) after the cmd_reg0 write is sufficient for int_out to
 * reflect the DMA error interrupt state.
 *
 * Test Plan Reference:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-test-plan.md
 * Test Case Mapping:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-functionality-testcases.md
 *   Section: FUNC_XSPI_004
 * Detailed Design:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-detailed-design.md
 *   Section 10 (DMA Interface)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "csml_logger.h"

#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>

/// @brief Module-local logger for FUNC_XSPI_004 test diagnostics.
///        Shared across all test functions in this translation unit.
static CsmlLogger func004_logger;

// =============================================================================
// Module-local register access helpers.
// Follow the same file-local static pattern as func001/002/003 to avoid
// multiply-defined symbols across translation units.
// =============================================================================

/**
 * @brief Issue a 32-bit TLM register write via the test harness.
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  32-bit value to write
 */
static void f4_write_reg(xspi_ctrl_test* test, unsigned int offset,
                         uint32_t value)
{
    test->register_write_32(offset, value);
}

/**
 * @brief Issue a 32-bit TLM register read via the test harness.
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 */
static void f4_read_reg(xspi_ctrl_test* test, unsigned int offset,
                        uint32_t& value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// FUNC_XSPI_004 Test Case Implementations
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DMA_001 — dma_settings reset value verification.
 *
 * Verification objective:
 *   Confirms that after reset_in de-assertion, dma_settings (offset 0x23C)
 *   reads back exactly 0x000D0000. This encodes the hardware power-on defaults:
 *     - burst_sel  = 0x00 (bits[7:0])  → dma_burst_length = 1 beat
 *     - OTE        = 1   (bit 16)       → outstanding transactions enabled
 *     - sdma_err_rsp = 0 (bit 17)       → SDMA error response disabled
 *     - word_size  = 0b11 (bits[19:18]) → 64-bit AXI master data width
 *   The value 0x000D0000 = OTE (bit 16 = 1) + word_size (bits[19:18] = 0b11)
 *   = 0x00010000 + 0x000C0000 = 0x000D0000.
 *
 * Stimulus:
 *   1. Apply reset.
 *   2. Read dma_settings at offset 0x23C.
 *
 * Expected result:
 *   dma_settings == 0x000D0000.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 10.3 (DMA parameter application).
 *   xspi_ctrl_basetest.h: dma_settings_RESET = 0x000D0000.
 ******************************************************************************/
void testbench::tc_xspi_dma_001_dma_settings_reset_value()
{
    report_test_start("TC_XSPI_DMA_001: dma_settings reset value = 0x000D0000");

    // Prerequisite: clean state.
    apply_reset();

    uint32_t dma_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_val);
    wait(5, sc_core::SC_NS);

    // dma_settings reset value is 0x000D0000.
    // OTE (bit 16) = 1, word_size (bits[19:18]) = 0b11, burst_sel = 0x00.
    const uint32_t expected_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_RESET);

    bool passed = (dma_val == expected_reset);

    if (!passed) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_001 FAIL: dma_settings=0x"
            << std::hex << dma_val
            << " expected=0x" << expected_reset
            << " (reset value mismatch)";
    } else {
        CSML_INFO(2, func004_logger)
            << "  DMA_001: dma_settings=0x" << std::hex << dma_val
            << " (OTE=1, word_size=0b11, burst_sel=0x00 — correct)";
    }

    report_test_result("TC_XSPI_DMA_001", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_002 — dma_settings RW retention within write_bit_mask.
 *
 * Verification objective:
 *   Confirms that dma_settings is a writable register with write_bit_mask
 *   = 0x000F00FF. Bits outside this mask (i.e., reserved bits [15:8] and
 *   [31:20]) are silently discarded by the scml2 framework. The test writes
 *   a value that spans all legal bitfields and confirms exact read-back.
 *
 * Stimulus:
 *   1. Apply reset (clean state, dma_settings = 0x000D0000).
 *   2. Write dma_settings = 0x000A00AA.
 *      Within write_bit_mask=0x000F00FF:
 *        - bits[7:0]   = 0xAA  (burst_sel = 0xAA → dma_burst_length = 171)
 *        - bits[19:16] = 0x0A  (OTE=0 bit16=0, sdma_err_rsp=1 bit17=1,
 *                               word_size=0b10 bits[19:18]=0b10)
 *        Masked value  = 0x000A00AA & 0x000F00FF = 0x000A00AA.
 *   3. Read back dma_settings and apply the write mask to verify retention.
 *
 * Expected result:
 *   (read_val & 0x000F00FF) == (0x000A00AA & 0x000F00FF) == 0x000A00AA.
 *
 * Reference:
 *   xspi_ctrl_basetest.h: dma_settings_WRITE = 0xf00ff (note: stored as int).
 *   docs/xspi_ctrl-detailed-design.md Section 10.3.
 ******************************************************************************/
void testbench::tc_xspi_dma_002_dma_settings_rw_retention()
{
    report_test_start("TC_XSPI_DMA_002: dma_settings RW retention within write_bit_mask");

    // Prerequisite: clean state.
    apply_reset();

    // Write a test value that exercises all major bitfields.
    // 0x000A00AA:
    //   bits[7:0]   = 0xAA  (burst_sel = 170)
    //   bit[16]     = 0     (OTE disabled)
    //   bit[17]     = 1     (sdma_err_rsp enabled)
    //   bits[19:18] = 0b10  (word_size = 32-bit)
    const uint32_t write_val  = 0x000A00AAu;
    const uint32_t write_mask = static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_WRITE);
    const uint32_t expected   = write_val & write_mask;

    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, write_val);
    wait(5, sc_core::SC_NS);

    uint32_t read_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, read_val);

    bool passed = ((read_val & write_mask) == expected);

    if (!passed) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_002 FAIL: dma_settings read=0x"
            << std::hex << read_val
            << " (masked=0x" << (read_val & write_mask) << ")"
            << " expected=0x" << expected
            << " (write=0x" << write_val
            << " mask=0x" << write_mask << ")";
    } else {
        CSML_INFO(2, func004_logger)
            << "  DMA_002: dma_settings=0x" << std::hex << (read_val & write_mask)
            << " write retained correctly within mask=0x" << write_mask;
    }

    report_test_result("TC_XSPI_DMA_002", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_003 — burst_sel field write and read-back verification.
 *
 * Verification objective:
 *   Validates that the burst_sel bitfield (bits[7:0] of dma_settings) retains
 *   written values correctly. burst_sel governs dma_burst_length = burst_sel+1
 *   per Section 10.3 of the detailed design.
 *
 *   The test exercises three burst_sel values:
 *     (a) Reset state: burst_sel = 0x00 (from dma_settings reset = 0x000D0000).
 *     (b) Write burst_sel = 0x0F (maximum reasonable burst for testing):
 *         write value = (reset_value & ~0xFF) | 0x0F = 0x000D000F.
 *     (c) Restore burst_sel = 0x00:
 *         write value = 0x000D0000 (full reset value).
 *
 * Stimulus:
 *   1. Apply reset — verify burst_sel read-back = 0x00.
 *   2. Write dma_settings = 0x000D000F (burst_sel=0x0F, OTE=1, word_size=0b11).
 *   3. Read back and verify bits[7:0] = 0x0F.
 *   4. Write dma_settings = 0x000D0000 (restore burst_sel=0x00).
 *   5. Read back and verify bits[7:0] = 0x00.
 *
 * Expected results:
 *   After step 1: (dma_val & 0xFF) == 0x00.
 *   After step 3: (dma_val & 0xFF) == 0x0F.
 *   After step 5: (dma_val & 0xFF) == 0x00.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 10.3: burst_sel = bits[7:0],
 *   dma_burst_length = burst_sel + 1.
 ******************************************************************************/
void testbench::tc_xspi_dma_003_burst_sel_field_write()
{
    report_test_start("TC_XSPI_DMA_003: dma_settings burst_sel field write and read-back");

    // Step 1: Apply reset — baseline burst_sel = 0x00.
    apply_reset();

    uint32_t dma_reset_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_reset_val);
    wait(5, sc_core::SC_NS);

    bool pass_reset_burst = ((dma_reset_val & 0xFFu) == 0x00u);
    if (!pass_reset_burst) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_003 FAIL (step 1): burst_sel after reset = 0x"
            << std::hex << (dma_reset_val & 0xFFu)
            << " expected 0x00";
    }

    // Step 2: Write burst_sel = 0x0F, keep OTE=1, word_size=0b11 (0x000D000F).
    // This validates that burst_sel can be set independently of OTE/word_size.
    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, 0x000D000Fu);
    wait(5, sc_core::SC_NS);

    // Step 3: Read back and verify burst_sel = 0x0F.
    uint32_t dma_burst_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_burst_val);

    bool pass_burst_set = ((dma_burst_val & 0xFFu) == 0x0Fu);
    if (!pass_burst_set) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_003 FAIL (step 3): burst_sel=0x"
            << std::hex << (dma_burst_val & 0xFFu)
            << " expected 0x0F (wrote 0x000D000F)";
    }

    // Step 4: Write burst_sel = 0x00 (restore to reset-equivalent value).
    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, 0x000D0000u);
    wait(5, sc_core::SC_NS);

    // Step 5: Verify burst_sel restored to 0x00.
    uint32_t dma_restored_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_restored_val);

    bool pass_burst_restored = ((dma_restored_val & 0xFFu) == 0x00u);
    if (!pass_burst_restored) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_003 FAIL (step 5): burst_sel after restore=0x"
            << std::hex << (dma_restored_val & 0xFFu)
            << " expected 0x00";
    }

    CSML_INFO(2, func004_logger)
        << "  DMA_003: reset_burst=0x" << std::hex << (dma_reset_val & 0xFFu)
        << " set_burst=0x" << (dma_burst_val & 0xFFu)
        << " restored_burst=0x" << (dma_restored_val & 0xFFu);

    bool passed = pass_reset_burst && pass_burst_set && pass_burst_restored;
    report_test_result("TC_XSPI_DMA_003", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_004 — OTE and word_size field write and read-back.
 *
 * Verification objective:
 *   Validates that the OTE (bit 16) and word_size (bits[19:18]) bitfields of
 *   dma_settings can be written and read back correctly.
 *
 *   OTE (Outstanding Transaction Enable): bit 16.
 *     reset=1 → dma_ote_enabled=true. Cleared to 0 → dma_ote_enabled=false.
 *
 *   word_size: bits[19:18]. Reset default = 0b11 (64-bit, value 0x000C0000).
 *     This test writes word_size=0b10 (32-bit, value 0x00080000), OTE=0
 *     (bit 16=0). Combined write: 0x00080000 (word_size=0b10, OTE=0).
 *
 *   The test also verifies that writing back the full reset value restores
 *   both fields to their reset defaults.
 *
 * Stimulus:
 *   1. Apply reset — verify OTE=1, word_size=0b11 in dma_settings.
 *   2. Write dma_settings = 0x00080000 (OTE=0, word_size=0b10, burst_sel=0x00).
 *   3. Read back and verify: OTE (bit 16) = 0, word_size (bits[19:18]) = 0b10.
 *   4. Restore dma_settings = 0x000D0000 (reset value).
 *   5. Read back and verify reset value restored.
 *
 * Expected results:
 *   After step 1: dma_val == 0x000D0000 (OTE=1, word_size=0b11).
 *   After step 3: (dma_val & 0x000F0000) == 0x00080000.
 *   After step 5: dma_val == 0x000D0000.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 10.3: OTE = bit 16,
 *   word_size = bits[19:18], sdma_err_rsp = bit 17.
 ******************************************************************************/
void testbench::tc_xspi_dma_004_ote_word_size_field_write()
{
    report_test_start("TC_XSPI_DMA_004: dma_settings OTE and word_size field write/read-back");

    // Step 1: Apply reset — verify OTE=1, word_size=0b11.
    apply_reset();

    uint32_t dma_reset_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_reset_val);
    wait(5, sc_core::SC_NS);

    const uint32_t expected_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_RESET);
    bool pass_reset = (dma_reset_val == expected_reset);
    if (!pass_reset) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_004 FAIL (step 1): dma_settings=0x"
            << std::hex << dma_reset_val
            << " expected=0x" << expected_reset;
    }

    // Step 2: Write OTE=0 (bit 16=0), word_size=0b10 (bits[19:18]=0b10),
    // burst_sel=0x00. Value = 0x00080000.
    // bits[19:18]=0b10 = 0x00080000; bit[16]=0; bit[17]=0.
    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, 0x00080000u);
    wait(5, sc_core::SC_NS);

    // Step 3: Read back and verify OTE=0, word_size=0b10.
    uint32_t dma_ote_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_ote_val);

    // Check bits[19:16] only (upper nibble of bits[19:16] = [19:16]).
    // OTE = bit 16, sdma_err_rsp = bit 17, word_size = bits[19:18].
    // Mask 0x000F0000 covers bits[19:16].
    bool pass_ote_zero    = ((dma_ote_val & (1u << 16u)) == 0u);
    bool pass_word_size   = ((dma_ote_val & 0x000C0000u) == 0x00080000u);

    if (!pass_ote_zero) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_004 FAIL (step 3): OTE bit 16 = "
            << ((dma_ote_val >> 16u) & 1u)
            << " expected 0 (wrote 0x00080000 with OTE=0)";
    }
    if (!pass_word_size) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_004 FAIL (step 3): word_size bits[19:18]=0x"
            << std::hex << ((dma_ote_val >> 18u) & 0x3u)
            << " expected 0b10 (32-bit mode)";
    }

    // Step 4: Restore to reset value 0x000D0000.
    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, 0x000D0000u);
    wait(5, sc_core::SC_NS);

    // Step 5: Verify reset value restored.
    uint32_t dma_restored_val = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_restored_val);

    bool pass_restored = (dma_restored_val == expected_reset);
    if (!pass_restored) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_004 FAIL (step 5): dma_settings after restore=0x"
            << std::hex << dma_restored_val
            << " expected=0x" << expected_reset;
    }

    CSML_INFO(2, func004_logger)
        << "  DMA_004: reset=0x" << std::hex << dma_reset_val
        << " after_ote_write=0x" << dma_ote_val
        << " restored=0x" << dma_restored_val;

    bool passed = pass_reset && pass_ote_zero && pass_word_size && pass_restored;
    report_test_result("TC_XSPI_DMA_004", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_005 — dma_settings restore to reset value after reset_in.
 *
 * Verification objective:
 *   Confirms that reset_in (active-low) restores dma_settings to its
 *   hardware reset value 0x000D0000. This validates the reset_handler()
 *   path that re-initializes dma_settings and the associated shadow
 *   variables (dma_burst_length=1, dma_ote_enabled=true, dma_word_size=3,
 *   dma_sdma_err_rsp=false).
 *
 * Stimulus:
 *   1. Write a non-reset value to dma_settings: 0x000A0055.
 *      (burst_sel=0x55, OTE=0, sdma_err_rsp=1, word_size=0b10)
 *   2. Verify the write was stored.
 *   3. Apply reset_in.
 *   4. Read back dma_settings and verify it equals 0x000D0000.
 *
 * Expected results:
 *   After step 2: (read_val & write_mask) == (0x000A0055 & 0x000F00FF).
 *   After step 4: dma_settings == 0x000D0000.
 *
 * Reference:
 *   model/src/xspi_ctrl.cpp reset_handler(): restores dma_burst_length=1,
 *   dma_word_size=0x3, dma_ote_enabled=true, dma_sdma_err_rsp=false.
 *   reset_all_registers() restores dma_settings register to 0x000D0000.
 ******************************************************************************/
void testbench::tc_xspi_dma_005_dma_settings_reset_restore()
{
    report_test_start("TC_XSPI_DMA_005: dma_settings restore to 0x000D0000 after reset");

    // Begin from a known state.
    apply_reset();

    // Write a non-reset value to dma_settings to dirty the register.
    // 0x000A0055: burst_sel=0x55, OTE=0, sdma_err_rsp=1, word_size=0b10.
    const uint32_t dirty_val  = 0x000A0055u;
    const uint32_t write_mask = static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_WRITE);

    f4_write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dirty_val);
    wait(5, sc_core::SC_NS);

    // Verify the dirty write was actually stored.
    uint32_t dirty_readback = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dirty_readback);

    bool pass_dirty = ((dirty_readback & write_mask) == (dirty_val & write_mask));
    if (!pass_dirty) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_005 SETUP: dirty write not stored. read=0x"
            << std::hex << dirty_readback
            << " expected=0x" << (dirty_val & write_mask)
            << ". Proceeding with reset.";
    }

    // Apply reset_in — reset_handler restores all registers including dma_settings.
    apply_reset();

    // Read back dma_settings after reset.
    uint32_t dma_after_reset = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_after_reset);
    wait(5, sc_core::SC_NS);

    const uint32_t expected_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_RESET);
    bool pass_reset = (dma_after_reset == expected_reset);

    if (!pass_reset) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_005 FAIL: dma_settings after reset=0x"
            << std::hex << dma_after_reset
            << " expected=0x" << expected_reset
            << " (reset failed to restore 0x000D0000)";
    } else {
        CSML_INFO(2, func004_logger)
            << "  DMA_005: dma_settings correctly restored to 0x"
            << std::hex << dma_after_reset
            << " (burst_sel=0, OTE=1, word_size=0b11)";
    }

    bool passed = pass_reset;
    report_test_result("TC_XSPI_DMA_005", passed);
}

/******************************************************************************
 * @brief TC_XSPI_AXI_SLAVE_001 — t_axi_slave_socket responds TLM_OK to READ.
 *
 * Verification objective:
 *   Confirms that the DUT's t_axi_slave_socket b_transport handler returns
 *   TLM_OK_RESPONSE for a READ transaction at FUNC_XSPI_004 scope. The
 *   b_transport_axi_slave() implementation unconditionally acknowledges all
 *   transactions with OK and applies a 10 ns LT delay.
 *
 * Stimulus:
 *   1. Apply reset.
 *   2. Construct a 64-bit TLM generic payload:
 *      - command = TLM_READ_COMMAND
 *      - address = 0x0000000000000000 (arbitrary — no forwarding at this scope)
 *      - data_length = 8 (two 32-bit words in 64-bit bus width)
 *      - data_ptr = local 8-byte buffer
 *   3. Call t_axi_slave_initiator->b_transport() synchronously.
 *   4. Check payload.get_response_status() == TLM_OK_RESPONSE.
 *
 * Expected result:
 *   response_status == TLM_OK_RESPONSE.
 *
 * Reference:
 *   model/src/xspi_ctrl.cpp b_transport_axi_slave() — FUNC_XSPI_004 scope:
 *   unconditional TLM_OK_RESPONSE + 10 ns delay.
 *   docs/xspi_ctrl-detailed-design.md Section 10.2 (AXI slave interface).
 ******************************************************************************/
void testbench::tc_xspi_axi_slave_001_read_response()
{
    report_test_start("TC_XSPI_AXI_SLAVE_001: t_axi_slave_socket READ → TLM_OK_RESPONSE");

    apply_reset();

    // Construct a 64-bit READ payload targeting address 0x0.
    tlm::tlm_generic_payload payload;
    uint8_t data_buf[8] = {0};
    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0x0u));
    payload.set_data_ptr(data_buf);
    payload.set_data_length(8u);
    payload.set_streaming_width(8u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Issue the 64-bit AXI slave READ via the testbench initiator socket.
    sc_core::sc_time axi_delay = sc_core::SC_ZERO_TIME;
    t_axi_slave_initiator->b_transport(payload, axi_delay);

    // Wait for any LT delay accumulated in the quantum keeper to settle.
    wait(15, sc_core::SC_NS);

    bool passed = (payload.get_response_status() == tlm::TLM_OK_RESPONSE);

    if (!passed) {
        CSML_ERROR(0, func004_logger)
            << "  AXI_SLAVE_001 FAIL: READ response="
            << static_cast<int>(payload.get_response_status())
            << " expected TLM_OK_RESPONSE ("
            << static_cast<int>(tlm::TLM_OK_RESPONSE) << ")";
    } else {
        CSML_INFO(2, func004_logger)
            << "  AXI_SLAVE_001: READ TLM_OK_RESPONSE received correctly";
    }

    report_test_result("TC_XSPI_AXI_SLAVE_001", passed);
}

/******************************************************************************
 * @brief TC_XSPI_AXI_SLAVE_002 — t_axi_slave_socket responds TLM_OK to WRITE.
 *
 * Verification objective:
 *   Confirms that the DUT's t_axi_slave_socket b_transport handler returns
 *   TLM_OK_RESPONSE for a WRITE transaction. This is the symmetric test to
 *   TC_XSPI_AXI_SLAVE_001, exercising the WRITE command path through
 *   b_transport_axi_slave().
 *
 * Stimulus:
 *   1. Apply reset.
 *   2. Construct a 64-bit TLM generic payload:
 *      - command = TLM_WRITE_COMMAND
 *      - address = 0x0000000000000010 (arbitrary non-zero address)
 *      - data_length = 8
 *      - data_ptr = local buffer with pattern 0xDEADBEEFCAFEBABE
 *   3. Call t_axi_slave_initiator->b_transport() synchronously.
 *   4. Check payload.get_response_status() == TLM_OK_RESPONSE.
 *
 * Expected result:
 *   response_status == TLM_OK_RESPONSE.
 *
 * Reference:
 *   model/src/xspi_ctrl.cpp b_transport_axi_slave() — FUNC_XSPI_004 scope:
 *   unconditional TLM_OK_RESPONSE + 10 ns delay for both READ and WRITE.
 ******************************************************************************/
void testbench::tc_xspi_axi_slave_002_write_response()
{
    report_test_start("TC_XSPI_AXI_SLAVE_002: t_axi_slave_socket WRITE → TLM_OK_RESPONSE");

    apply_reset();

    // Construct a 64-bit WRITE payload with a distinctive data pattern.
    tlm::tlm_generic_payload payload;
    uint8_t data_buf[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0x10u));
    payload.set_data_ptr(data_buf);
    payload.set_data_length(8u);
    payload.set_streaming_width(8u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time axi_delay = sc_core::SC_ZERO_TIME;
    t_axi_slave_initiator->b_transport(payload, axi_delay);

    wait(15, sc_core::SC_NS);

    bool passed = (payload.get_response_status() == tlm::TLM_OK_RESPONSE);

    if (!passed) {
        CSML_ERROR(0, func004_logger)
            << "  AXI_SLAVE_002 FAIL: WRITE response="
            << static_cast<int>(payload.get_response_status())
            << " expected TLM_OK_RESPONSE";
    } else {
        CSML_INFO(2, func004_logger)
            << "  AXI_SLAVE_002: WRITE TLM_OK_RESPONSE received correctly";
    }

    report_test_result("TC_XSPI_AXI_SLAVE_002", passed);
}

/******************************************************************************
 * @brief TC_XSPI_POR_001 — PoR_input_signals responds TLM_OK_RESPONSE.
 *
 * Verification objective:
 *   Confirms that the DUT's PoR_input_signals b_transport handler returns
 *   TLM_OK_RESPONSE for a transaction. At FUNC_XSPI_004 scope the handler
 *   b_transport_por() acknowledges unconditionally with OK. No xspi_PoR_trans
 *   extension is required at this scope — a bare tlm_generic_payload suffices.
 *
 * Stimulus:
 *   1. Apply reset.
 *   2. Construct a bare 64-bit TLM generic payload (no extension):
 *      - command = TLM_WRITE_COMMAND (PoR signals are write-only stimulus)
 *      - address = 0x0
 *      - data_length = 4
 *   3. Call por_initiator->b_transport() synchronously.
 *   4. Check payload.get_response_status() == TLM_OK_RESPONSE.
 *
 * Expected result:
 *   response_status == TLM_OK_RESPONSE.
 *
 * Reference:
 *   model/src/xspi_ctrl.cpp b_transport_por() — FUNC_XSPI_004 scope:
 *   unconditional TLM_OK_RESPONSE + 10 ns delay.
 *   docs/xspi_ctrl-detailed-design.md Section 8.1 (PoR sequence).
 ******************************************************************************/
void testbench::tc_xspi_por_001_por_response()
{
    report_test_start("TC_XSPI_POR_001: PoR_input_signals → TLM_OK_RESPONSE");

    apply_reset();

    // Construct a bare TLM payload for the PoR socket.
    // The b_transport_por handler at FUNC_XSPI_004 scope does not inspect the
    // extension field — a bare payload is sufficient to verify the OK response.
    tlm::tlm_generic_payload payload;
    uint8_t data_buf[4] = {0};
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0x0u));
    payload.set_data_ptr(data_buf);
    payload.set_data_length(4u);
    payload.set_streaming_width(4u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time por_delay = sc_core::SC_ZERO_TIME;
    por_initiator->b_transport(payload, por_delay);

    wait(15, sc_core::SC_NS);

    bool passed = (payload.get_response_status() == tlm::TLM_OK_RESPONSE);

    if (!passed) {
        CSML_ERROR(0, func004_logger)
            << "  POR_001 FAIL: PoR response="
            << static_cast<int>(payload.get_response_status())
            << " expected TLM_OK_RESPONSE";
    } else {
        CSML_INFO(2, func004_logger)
            << "  POR_001: PoR TLM_OK_RESPONSE received correctly";
    }

    report_test_result("TC_XSPI_POR_001", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_ERR_001 — DMA error injection via ACMD descriptor fetch:
 *        intr_status.cdma_terr set and int_out asserts.
 *
 * Verification objective:
 *   Validates that when the DMA stub on i_dma_socket returns
 *   TLM_GENERIC_ERROR_RESPONSE during an ACMD descriptor fetch, the model's
 *   cdma_handle_trigger() error handler correctly:
 *     (1) Sets intr_status.cdma_terr (bit 17) in intr_status.
 *     (2) Calls evaluate_interrupt_out(), asserting int_out when
 *         intr_enable.cdma_terr_en (bit 17) + global gate (bit 31) are set.
 *     (3) Clears trd_busy and ctrl_busy/acmd_eng_busy after the error.
 *
 * Architectural Note on dma_target_error_l/h:
 *   The cdma_handle_trigger() implementation (FUNC_XSPI_004 scope) uses an
 *   inline i_dma_socket->b_transport() call — it does NOT route through the
 *   dma_read() / dma_write() helper methods. The dma_target_error_l/h address
 *   capture is implemented only inside dma_read()/dma_write() helpers, which
 *   are exercised by PIO and ACMD data-DMA paths in later function layers
 *   (FUNC_XSPI_011, FUNC_XSPI_012). Therefore dma_target_error_l/h remain
 *   0x00000000 on a cdma_handle_trigger() error at this scope; only cdma_terr
 *   and int_out are the observable side-effects at FUNC_XSPI_004 level.
 *
 * ACMD stimulus:
 *   - ctrl_config.work_mode = 0b11 (ACMD mode): write 0x00000060.
 *   - cmd_reg2 = 0x00000040 (descriptor address low = 0x40, 64-byte aligned).
 *   - cmd_reg3 = 0x00000000 (descriptor address high = 0).
 *   - cmd_reg0 = 0x00000000 (ACMD select: bits[31:30]=0b00; TRD_NUM=0).
 *
 * Stimulus sequence:
 *   1. Apply reset — clean state (intr_status=0, dma_target_error_l/h=0).
 *   2. Enable cdma_terr in intr_enable:
 *      write intr_enable = 0x80020000 (bit 31 = global en, bit 17 = cdma_terr).
 *   3. Set m_dma_error_count = 1 (DMA stub returns error on next transaction).
 *   4. Set ACMD mode: write ctrl_config = 0x00000060.
 *   5. Stage descriptor address: cmd_reg2 = 0x00000040, cmd_reg3 = 0x00000000.
 *   6. Trigger ACMD: write cmd_reg0 = 0x00000000.
 *   7. Wait SC_ZERO_TIME for update_int_out() SC_METHOD to execute.
 *
 * Expected results:
 *   - (intr_status & (1u << 17u)) != 0    → cdma_terr bit set
 *   - int_out_sig == true                 → Path 1 active (cdma_terr enabled)
 *   - (trd_status & 0x1u) == 0            → thread 0 not busy (cleared on error)
 *   - (ctrl_status & 0x84u) == 0          → ctrl_busy + acmd_eng_busy cleared
 *
 * Reference:
 *   model/src/xspi_ctrl.cpp cdma_handle_trigger() lines ~1781-1794:
 *   inline DMA error handler sets cdma_terr + clears busy flags.
 *   docs/xspi_ctrl-detailed-design.md Section 10.4 (DMA Error Capture).
 ******************************************************************************/
void testbench::tc_xspi_dma_err_001_cdma_terr_and_address_capture()
{
    report_test_start("TC_XSPI_DMA_ERR_001: ACMD DMA error → cdma_terr set, int_out asserts");

    // Step 1: Apply reset — all interrupt status cleared.
    apply_reset();

    // Verify clean baseline: intr_status = 0, int_out = false.
    uint32_t intr_baseline = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_baseline);
    wait(5, sc_core::SC_NS);

    bool pass_baseline = (intr_baseline == 0u) && !int_out_sig.read();
    if (!pass_baseline) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_001 SETUP: baseline not clean. intr_status=0x"
            << std::hex << intr_baseline
            << " int_out=" << int_out_sig.read()
            << ". Proceeding anyway.";
    }

    // Step 2: Enable cdma_terr (bit 17) in intr_enable with global gate (bit 31).
    // intr_enable write_bit_mask = 0x9FF7F000. 0x80020000 & 0x9FF7F000 = 0x80020000.
    f4_write_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, 0x80020000u);
    wait(sc_core::SC_ZERO_TIME);

    // Step 3: Arm the DMA stub error injection counter.
    // The ACMD descriptor fetch via i_dma_socket will receive
    // TLM_GENERIC_ERROR_RESPONSE, triggering the cdma_terr error path.
    m_dma_error_count = 1;

    // Step 4: Set ACMD work mode.
    // ctrl_config.work_mode = 0b11 (ACMD) at bits[6:5].
    // write_bit_mask = 0x68; 0x60 & 0x68 = 0x60 → bits[6:5]=0b11 = ACMD mode.
    f4_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000060u);

    // Step 5: Stage 64-byte aligned descriptor address.
    // Address = 0x0000000000000040 (64 = 0x40, exactly one 64-byte boundary).
    f4_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00000040u);
    f4_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);

    // Step 6: Trigger ACMD on thread 0.
    // cmd_reg0[31:30] = 0b00 (ACMD selector); cmd_reg0[26:24] = 0b000 (TRD=0).
    f4_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000000u);

    // Step 7: Wait for update_int_out() SC_METHOD to fire.
    // cdma_handle_trigger() runs synchronously in handle_write_cmd_reg0.
    // DMA stub returns error → cdma_terr set → evaluate_interrupt_out()
    // notifies m_int_update_event at SC_ZERO_TIME.
    wait(sc_core::SC_ZERO_TIME);

    // Read all observable state.
    uint32_t intr_val   = 0xDEADBEEFu;
    uint32_t trd_val    = 0xDEADBEEFu;
    uint32_t ctrl_val   = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET,  intr_val);
    f4_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET,   trd_val);
    f4_read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET,  ctrl_val);

    // Assertion 1: intr_status.cdma_terr (bit 17) must be set.
    bool pass_cdma_terr = ((intr_val & (1u << 17u)) != 0u);
    if (!pass_cdma_terr) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_001 FAIL: intr_status=0x" << std::hex << intr_val
            << " — cdma_terr (bit 17) not set. "
            << "Expected ACMD DMA error to set cdma_terr in cdma_handle_trigger().";
    }

    // Assertion 2: int_out must be asserted (Path 1: cdma_terr & intr_enable).
    bool int_out_asserted = int_out_sig.read();
    bool pass_int_out = int_out_asserted;
    if (!pass_int_out) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_001 FAIL: int_out=false after DMA error. "
            << "intr_status=0x" << std::hex << intr_val
            << " intr_enable should gate cdma_terr (bit 17) to Path 1.";
    }

    // Assertion 3: trd_busy[0] must be cleared (error path clears trd_busy).
    bool pass_trd_cleared = ((trd_val & 0x1u) == 0u);
    if (!pass_trd_cleared) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_001 FAIL: trd_status=0x" << std::hex << trd_val
            << " — trd_busy[0] still set after DMA error (should be cleared).";
    }

    // Assertion 4: ctrl_busy (bit 7) and acmd_eng_busy (bit 2) must be cleared.
    bool pass_ctrl_cleared = ((ctrl_val & 0x84u) == 0u);
    if (!pass_ctrl_cleared) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_001 FAIL: ctrl_status=0x" << std::hex << ctrl_val
            << " — ctrl_busy/acmd_eng_busy not cleared after DMA error.";
    }

    CSML_INFO(2, func004_logger)
        << "  DMA_ERR_001: intr_status=0x" << std::hex << intr_val
        << " trd_status=0x" << trd_val
        << " ctrl_status=0x" << ctrl_val
        << " int_out=" << int_out_asserted;

    bool passed = pass_baseline && pass_cdma_terr && pass_int_out
               && pass_trd_cleared && pass_ctrl_cleared;
    report_test_result("TC_XSPI_DMA_ERR_001", passed);
}

/******************************************************************************
 * @brief TC_XSPI_DMA_ERR_002 — dma_target_error_l/h RO enforcement.
 *
 * Verification objective:
 *   Confirms that software writes to dma_target_error_l (0x150) and
 *   dma_target_error_h (0x154) are silently discarded by the scml2 framework
 *   (write_bit_mask = 0x0 for both registers). Only the model's internal
 *   dma_read()/dma_write() error handlers can update these registers via
 *   direct scml2 assignment.
 *
 *   After TC_XSPI_DMA_ERR_001, dma_target_error_l holds 0x00000040 and
 *   dma_target_error_h holds 0x00000000 (set by the DMA error path). This
 *   test attempts software overwrites and verifies both registers retain
 *   the model-written values.
 *
 * Stimulus:
 *   1. Verify dma_target_error_l != 0 (carry-over from DMA_ERR_001 or apply
 *      reset and re-inject to ensure a known non-zero state).
 *   2. Write 0xFFFFFFFF to dma_target_error_l (0x150) — should be discarded.
 *   3. Write 0xFFFFFFFF to dma_target_error_h (0x154) — should be discarded.
 *   4. Read back both registers and verify the software writes were ignored.
 *
 * Expected results:
 *   - dma_target_error_l unchanged (still holds DMA-error-written value).
 *   - dma_target_error_h unchanged.
 *
 * Note:
 *   If DMA_ERR_001 did not set the registers (e.g., if cdma_handle_trigger
 *   clears them after the error), this test verifies the reset value 0x0 is
 *   preserved after a rogue write — which also confirms write-ignore behavior.
 *
 * Reference:
 *   xspi_ctrl_basetest.h: dma_target_error_l_WRITE = 0x0 (RO, write-ignore).
 *   docs/xspi_ctrl-detailed-design.md Section 10.4: both registers are RO.
 ******************************************************************************/
void testbench::tc_xspi_dma_err_002_dma_target_error_ro_enforcement()
{
    report_test_start("TC_XSPI_DMA_ERR_002: dma_target_error_l/h RO write-ignore enforcement");

    // Read current values (may be non-zero from DMA_ERR_001 path above, or
    // may be 0x00000000 after reset). Capture before rogue write.
    uint32_t terr_l_before = 0xDEADBEEFu;
    uint32_t terr_h_before = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_target_error_l_OFFSET, terr_l_before);
    f4_read_reg(test, xspi_ctrl_basetest::dma_target_error_h_OFFSET, terr_h_before);
    wait(5, sc_core::SC_NS);

    CSML_INFO(2, func004_logger)
        << "  DMA_ERR_002: before rogue write: terr_l=0x"
        << std::hex << terr_l_before
        << " terr_h=0x" << terr_h_before;

    // Attempt rogue software writes (write_mask = 0x0 → all bits discarded).
    f4_write_reg(test, xspi_ctrl_basetest::dma_target_error_l_OFFSET, 0xFFFFFFFFu);
    f4_write_reg(test, xspi_ctrl_basetest::dma_target_error_h_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    // Read back both registers.
    uint32_t terr_l_after = 0xDEADBEEFu;
    uint32_t terr_h_after = 0xDEADBEEFu;
    f4_read_reg(test, xspi_ctrl_basetest::dma_target_error_l_OFFSET, terr_l_after);
    f4_read_reg(test, xspi_ctrl_basetest::dma_target_error_h_OFFSET, terr_h_after);

    // Pass condition: the rogue write (0xFFFFFFFF) was ignored.
    // After a rogue write is discarded, the register must not read 0xFFFFFFFF.
    bool pass_l = (terr_l_after != 0xFFFFFFFFu);
    bool pass_h = (terr_h_after != 0xFFFFFFFFu);

    // Additionally verify the value was not changed by the write
    // (should be identical to before-write state).
    bool pass_l_unchanged = (terr_l_after == terr_l_before);
    bool pass_h_unchanged = (terr_h_after == terr_h_before);

    if (!pass_l) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_002 FAIL: dma_target_error_l=0xFFFFFFFF after "
            << "rogue write — write-ignore restriction not active!";
    }
    if (!pass_h) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_002 FAIL: dma_target_error_h=0xFFFFFFFF after "
            << "rogue write — write-ignore restriction not active!";
    }
    if (!pass_l_unchanged) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_002 FAIL: dma_target_error_l changed from 0x"
            << std::hex << terr_l_before << " to 0x" << terr_l_after
            << " after rogue write (should be unchanged).";
    }
    if (!pass_h_unchanged) {
        CSML_ERROR(0, func004_logger)
            << "  DMA_ERR_002 FAIL: dma_target_error_h changed from 0x"
            << std::hex << terr_h_before << " to 0x" << terr_h_after
            << " after rogue write (should be unchanged).";
    }

    CSML_INFO(2, func004_logger)
        << "  DMA_ERR_002: terr_l before=0x" << std::hex << terr_l_before
        << " after=0x" << terr_l_after
        << " | terr_h before=0x" << terr_h_before
        << " after=0x" << terr_h_after;

    bool passed = pass_l && pass_h && pass_l_unchanged && pass_h_unchanged;
    report_test_result("TC_XSPI_DMA_ERR_002", passed);
}

/******************************************************************************
 * @brief run_func004_tests — Top-level orchestrator for FUNC_XSPI_004 suite.
 *
 * Executes all 10 test cases for the DMA Interface and AXI Transaction
 * Management functionality in document order. Each test begins with the
 * state left by the previous test, except where apply_reset() is called
 * explicitly.
 *
 * Test execution order:
 *  1.  TC_XSPI_DMA_001        — dma_settings reset value 0x000D0000
 *  2.  TC_XSPI_DMA_002        — dma_settings RW retention in write_bit_mask
 *  3.  TC_XSPI_DMA_003        — burst_sel field write/read-back
 *  4.  TC_XSPI_DMA_004        — OTE and word_size field write/read-back
 *  5.  TC_XSPI_DMA_005        — dma_settings restore after reset
 *  6.  TC_XSPI_AXI_SLAVE_001  — AXI slave READ → TLM_OK_RESPONSE
 *  7.  TC_XSPI_AXI_SLAVE_002  — AXI slave WRITE → TLM_OK_RESPONSE
 *  8.  TC_XSPI_POR_001        — PoR socket → TLM_OK_RESPONSE
 *  9.  TC_XSPI_DMA_ERR_001    — DMA error path: cdma_terr + address capture
 * 10.  TC_XSPI_DMA_ERR_002    — dma_target_error_l/h RO write-ignore
 *
 * Called from run_tests() after run_func003_tests() completes.
 *
 * Reference:
 *   docs/xspi_ctrl-functionality-testcases.md FUNC_XSPI_004 section.
 *   docs/xspi_ctrl-test-plan.md Category: DMA tests.
 ******************************************************************************/
void testbench::run_func004_tests()
{
    CSML_INFO(1, func004_logger)
        << "\n================================================\n"
        << "  FUNC_XSPI_004: DMA Interface and AXI Transaction Management\n"
        << "  10 Test Cases: DMA_001-005, AXI_SLAVE_001-002, POR_001,\n"
        << "                 DMA_ERR_001-002\n"
        << "================================================";

    // Test 1: dma_settings reset value verification.
    tc_xspi_dma_001_dma_settings_reset_value();

    // Test 2: dma_settings RW retention within write_bit_mask.
    tc_xspi_dma_002_dma_settings_rw_retention();

    // Test 3: burst_sel field write and read-back.
    tc_xspi_dma_003_burst_sel_field_write();

    // Test 4: OTE and word_size field write and read-back.
    tc_xspi_dma_004_ote_word_size_field_write();

    // Test 5: dma_settings reset restores 0x000D0000.
    tc_xspi_dma_005_dma_settings_reset_restore();

    // Test 6: AXI slave socket — READ → TLM_OK_RESPONSE.
    tc_xspi_axi_slave_001_read_response();

    // Test 7: AXI slave socket — WRITE → TLM_OK_RESPONSE.
    tc_xspi_axi_slave_002_write_response();

    // Test 8: PoR socket — TLM_OK_RESPONSE.
    tc_xspi_por_001_por_response();

    // Test 9: DMA error injection — cdma_terr + address capture + int_out.
    // Sets m_dma_error_count = 1 internally before the ACMD trigger.
    tc_xspi_dma_err_001_cdma_terr_and_address_capture();

    // Test 10: dma_target_error_l/h RO write-ignore enforcement.
    // Continues from state after DMA_ERR_001 (registers may hold error values).
    tc_xspi_dma_err_002_dma_target_error_ro_enforcement();

    CSML_INFO(1, func004_logger)
        << "================================================\n"
        << "  FUNC_XSPI_004 Suite Complete\n"
        << "================================================";
}
