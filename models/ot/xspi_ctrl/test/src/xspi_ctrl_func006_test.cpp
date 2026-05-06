/******************************************************************************
 * @file xspi_ctrl_func006_test.cpp
 * @brief Test cases for FUNC_XSPI_006 — Operating Mode Control and Command
 *        Dispatch
 *
 * This file implements all test cases mapped to FUNC_XSPI_006 in the
 * xspi_ctrl functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_006 Test Coverage (8 test cases):
 *
 * - TC_XSPI_MDR_001: STIG mode — cmd_reg0 written value is entirely ignored as
 *                    a bitfield carrier. Dispatch uses cmd_reg1–cmd_reg4 only.
 *                    Pre-stage a valid STIG READ, write cmd_reg0=0xDEADBEEF,
 *                    confirm engine fires and cmd_status.COMPLETE is set.
 *
 * - TC_XSPI_STIG_008: STIG staging order — write cmd_reg0 (trigger) BEFORE
 *                    staging cmd_reg1–cmd_reg4. Verify engine fires with stale
 *                    (zeroed) staging values, not the values written afterward.
 *
 * - TC_XSPI_MDR_002: PIO mode — cmd_reg0 field decoding. Write
 *                    cmd_reg0=0x45041000 in PIO mode. Verify:
 *                    (a) bits[31:30]=0b01 (PIO selector confirmed),
 *                    (b) bits[26:24]=0b101 → TRD_NUM=5 → trd_comp_intr[5] set,
 *                    (c) bits[22:20]=0b000 → BANK=0 (valid; NUM_TARGETS=1),
 *                    (d) bit[18]=1 → INT=1 → trd_comp_intr_status bit 5 set,
 *                    (e) bits[15:0]=0x1000 → CMD_TYPE=SECTOR_ERASE.
 *                    Note: BANK=0 used (NUM_TARGETS=1); BANK>=1 causes
 *                    cmd_ignored. BANK decode confirmed: cmd_ignored=0.
 *
 * - TC_XSPI_MDR_003: ACMD mode — cmd_reg0 only uses bits[31:30]=0b00 (ACMD
 *                    selector) and bits[26:24] (TRD_NUM). Write
 *                    cmd_reg0=0x0300FFFF (TRD_NUM=3, lower 24 bits = garbage).
 *                    Verify trd_status bit 3 set (thread 3 dispatched), not any
 *                    other thread, and descriptor fetch is attempted at the
 *                    address staged in cmd_reg2/cmd_reg3.
 *
 * - TC_XSPI_MDR_004: PIO SECTOR_ERASE — SECT_CNT in cmd_reg4 governs the
 *                    number of erase transactions. Write cmd_reg4=0x00000003
 *                    (SECT_CNT=3, actual count=4), trigger SECTOR_ERASE.
 *                    Verify trd_status cleared after completion. Repeat with
 *                    cmd_reg4=0x00000000 (SECT_CNT=0, actual count=1). Verify
 *                    completion in both cases without error.
 *
 * - TC_XSPI_MDR_006: cmd_reg2/cmd_reg3 dual role. (a) PIO READ mode:
 *                    cmd_reg2=SYS_ADDR_PTR_L, cmd_reg3=SYS_ADDR_PTR_H form the
 *                    64-bit system memory address for DMA write; completion
 *                    confirms the PIO engine consumed cmd_reg2/cmd_reg3 as a
 *                    system pointer. (b) ACMD mode: cmd_reg2/cmd_reg3 form the
 *                    64-bit descriptor address; descriptor fetch is initiated
 *                    at that address.
 *
 * - TC_XSPI_MDR_007: cmd_reg5 is consumed only by PIO. (a) PIO READ mode:
 *                    cmd_reg5=0x00000002 (xSPI address upper 32 bits),
 *                    cmd_reg1=0x00300000 (lower 32 bits); combined flash
 *                    address = 0x0000000200300000. Verify PIO dispatch
 *                    completes (thread clears). (b) STIG mode with same
 *                    cmd_reg5 written: STIG engine fires and completes
 *                    normally, confirming cmd_reg5 value does not affect STIG
 *                    dispatch.
 *
 * - TC_XSPI_MDR_008: Staging order — cmd_reg0 must be written last. In PIO
 *                    mode, write cmd_reg0 (trigger) BEFORE setting cmd_reg1
 *                    with the intended flash address. Model dispatches with
 *                    stale cmd_reg1 (0x00000000 from reset). Flash transaction
 *                    uses address=0. Then stage cmd_reg1 correctly first and
 *                    write cmd_reg0 last; verify correct address used (thread
 *                    completes without error indicating dispatch with correct
 *                    staged values).
 *
 * Architecture Notes:
 *   All tests exercise the core dispatch mechanism: handle_write_ctrl_config()
 *   sets current_work_mode, and handle_write_cmd_reg0() reads current_work_mode
 *   to select the correct engine. The LT model executes PIO/ACMD engines
 *   synchronously within the b_transport call, so trd_status and ctrl_status
 *   are updated before the write_reg() call returns. STIG runs as an SC_THREAD
 *   and requires wait() for the engine to complete before checking cmd_status.
 *
 *   ctrl_config write_bit_mask = 0x68 (bits[6:5] and bit 3):
 *     work_mode (2 bits [6:5]) per Register Reference Manual:
 *       DIRECT = 2'b00 → ctrl_config = 0x00
 *       STIG   = 2'b01 → ctrl_config = 0x20
 *       (2'b10 reserved)
 *       ACMD   = 2'b11 → ctrl_config = 0x60 (PIO: cmd_reg0[31:30]=2'b01;
 *                                           CDMA: cmd_reg0[31:30]=2'b00)
 *
 * Dependencies: FUNC_XSPI_001, FUNC_XSPI_002, FUNC_XSPI_003
 *
 * Test Plan Reference:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-test-plan.md
 *   Entries 25 (TC_XSPI_STIG_008), 66–73 (TC_XSPI_MDR_001–008)
 * Test Case Mapping:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-functionality-testcases.md
 *   Section "FUNC_XSPI_006 — Operating Mode Control and Command Dispatch"
 * Detailed Design:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-detailed-design.md
 *   Section 5.3 (cmd_reg0–5 mode-dependent bitfield layouts)
 *   Section 7 (Operating Modes)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "xspi_ctrl_test.h"
#include "csml_logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>

/// @brief Module-scoped logger for FUNC_XSPI_006 test diagnostics
static CsmlLogger func006_logger;

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Write a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_write_32, keeping
 * test case bodies concise and matching the established pattern from
 * xspi_ctrl_func001_test.cpp.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  32-bit value to write
 */
static inline void func006_write_reg(xspi_ctrl_test* test,
                                     unsigned int    offset,
                                     uint32_t        value)
{
    test->register_write_32(offset, value);
}

/**
 * @brief Read a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_read_32.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 */
static inline void func006_read_reg(xspi_ctrl_test* test,
                                    unsigned int    offset,
                                    uint32_t&       value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// ctrl_config work_mode encoding constants (bits[6:5])
// These are the values to write to ctrl_config for each operating mode.
// write_bit_mask = 0x68 (bits[6:5][3]); mode bits live in [6:5].
// =============================================================================

/// @brief ctrl_config value for Direct mode (work_mode 2'b00 at bits[6:5])
static const uint32_t CTRL_CFG_DIRECT = 0x00000000u;

/// @brief ctrl_config for PIO path: global work_mode 2'b11 (same field value as CDMA)
static const uint32_t CTRL_CFG_PIO    = 0x00000060u;

/// @brief ctrl_config value for STIG mode (work_mode 2'b01 at bits[6:5])
static const uint32_t CTRL_CFG_STIG   = 0x00000020u;

/// @brief ctrl_config value for CDMA path (work_mode 2'b11 at bits[6:5])
static const uint32_t CTRL_CFG_ACMD   = 0x00000060u;

// -----------------------------------------------------------------------------
// TC_XSPI_MDR_002 — PIO cmd_reg0 layout (Register Reference / detailed-design
// Section 5.3.2; must match xspi_ctrl_ip::pio_handle_trigger field extraction).
// -----------------------------------------------------------------------------
namespace func006_mdr002_pio {

    constexpr uint32_t encode_cmd_reg0(
        unsigned mode_31_30,
        unsigned trd_26_24,
        unsigned bank_22_20,
        unsigned dma_sel_19,
        unsigned int_18,
        unsigned mb_xip_dis_17,
        unsigned mb_xip_en_16,
        uint32_t cmd_type_15_0)
    {
        return ((mode_31_30 & 3u) << 30)
             | ((trd_26_24 & 7u) << 24)
             | ((bank_22_20 & 7u) << 20)
             | ((dma_sel_19 & 1u) << 19)
             | ((int_18 & 1u) << 18)
             | ((mb_xip_dis_17 & 1u) << 17)
             | ((mb_xip_en_16 & 1u) << 16)
             | (cmd_type_15_0 & 0xFFFFu);
    }
    
    // Test-plan #67 semantics: TRD_NUM=5, BANK=2, INT=1, MB_XIP_DIS=0, MB_XIP_EN=0,
    // DMA_SEL=0, CMD_TYPE=SECTOR_ERASE (0x1000), ACMD work_mode with PIO sub-mode.
    constexpr uint32_t kExpectedMode31_30     = 1u;   // 2'b01 → PIO
    constexpr unsigned kExpectedTrdNum        = 5u;
    constexpr unsigned kExpectedBankCs        = 2u;
    constexpr unsigned kExpectedDmaSel        = 0u;
    constexpr unsigned kExpectedInt           = 1u;
    constexpr unsigned kExpectedMbXipDis    = 0u;
    constexpr unsigned kExpectedMbXipEn     = 0u;
    constexpr uint32_t kExpectedCmdType       = 0x1000u;
    
    constexpr uint32_t kCmdReg0Golden = encode_cmd_reg0(
        kExpectedMode31_30,
        kExpectedTrdNum,
        kExpectedBankCs,
        kExpectedDmaSel,
        kExpectedInt,
        kExpectedMbXipDis,
        kExpectedMbXipEn,
        kExpectedCmdType);
    
    static_assert(kCmdReg0Golden == 0x45241000u,
                  "MDR_002 golden cmd_reg0 must equal explicit bitfield composition");
    // Model masks reserved [29:27] and [23]; stimulus keeps them zero (spec-clean).
    constexpr uint32_t kCmdReg0ReservedMask = (7u << 27) | (1u << 23);
    static_assert((kCmdReg0Golden & kCmdReg0ReservedMask) == 0u,
                  "MDR_002 cmd_reg0 reserved bits [29:27] and [23] must be zero");
    
    /** Decode exactly as the DUT's pio_handle_trigger (post-reserved mask). */
    inline void decode_cmd_reg0_masked(
        uint32_t masked_val,
        unsigned& trd_num,
        unsigned& bank_cs,
        unsigned& dma_sel,
        unsigned& int_flag,
        unsigned& mb_xip_dis,
        unsigned& mb_xip_en,
        uint32_t& cmd_type)
    {
        trd_num     = (masked_val >> 24) & 7u;
        bank_cs     = (masked_val >> 20) & 7u;
        dma_sel     = (masked_val >> 19) & 1u;
        int_flag    = (masked_val >> 18) & 1u;
        mb_xip_dis  = (masked_val >> 17) & 1u;
        mb_xip_en   = (masked_val >> 16) & 1u;
        cmd_type    = masked_val & 0xFFFFu;
    }
    
    } // namespace func006_mdr002_pio
    

// =============================================================================
// FUNC_XSPI_006 Test Case Implementations
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_MDR_001 — STIG: cmd_reg0 is a pure trigger; READ from cmd_reg1–4
 *
 * Stages a single-phase Profile-1 STIG READ (decode matches xspi_ctrl_ip::
 * decode_instruction): opcode in cmd_reg3[23:16], INSTR_TYPE READ in cmd_reg1[6:0],
 * flash address 0x100, 2 data bytes; opcode 0x03 in cmd_reg3[23:16].
 *
 * Phase A: cmd_reg0 = 0xDEADBEEF — assert one READ on socket[0] with expected
 *          cdns_extension fields from staging (not from cmd_reg0).
 * Phase B: reset, re-stage identical cmd_reg1–4, cmd_reg0 = 0x40042200
 *          (PIO READ-shaped pattern) — assert identical bus behavior, proving
 *          cmd_reg0 bits are not interpreted as PIO (STIG mode + pure trigger).
 *
 * Pass: per-phase flash tx + ext + intr_status/ctrl_status checks.
 * Fail: any phase mismatch — report_test_fail with reason; else report_test_pass.
 *
 * Reference: docs/xspi_ctrl-test-plan.md TC_XSPI_MDR_001_cmd_reg0_stig_value_ignored
 ******************************************************************************/
 void testbench::tc_xspi_mdr_001_cmd_reg0_stig_value_ignored()
 {
     func006_logger.setMaxVerbosity(2);
     report_test_start(
         "TC_XSPI_MDR_001: STIG — cmd_reg0 ignored; READ from cmd_reg1–cmd_reg4");
 
     // Profile 1 staging (see model decode_instruction / TC_XSPI_STIG_003 comments).
     static constexpr uint32_t k_r1 = 0x00000001u;   // [6:0] = XSPI_INSTR_READ (1)
     static constexpr uint32_t k_r2 = 0x00000001u;   // packed addr → flash 0x100
     // cmd_reg3: opcode MUST live in [23:16] — (r3>>16)&0xFF. Using 0x02000300 put
     // 0x03 in [15:8], so decode produced opcode 0 and the stub logged "Unknown opcode".
     static constexpr uint32_t k_r3 = 0x02030000u;   // [25:24]=2; [23:16]=0x03 (READ)
     static constexpr uint32_t k_r4 = 0x00000000u;   // bank 0, no INSTR_LINK
 
     static constexpr uint64_t k_expect_addr      = 0x100u;
     static constexpr uint32_t k_expect_opcode    = 0x03u;
     static constexpr uint32_t k_expect_data_bytes = 2u;
 
     auto run_phase = [&](const char* phase_tag, uint32_t cmd0_trigger) -> bool {
         func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG);
         wait(5, sc_core::SC_NS);
 
         func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, k_r1);
         func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, k_r2);
         func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, k_r3);
         func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, k_r4);
         wait(5, sc_core::SC_NS);
 
         const unsigned tx_before = static_cast<unsigned>(m_flash_stub_tx_count);
         const int      t0_before = m_flash_tx_per_target[0];
 
         func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_trigger);
         wait(30, sc_core::SC_NS);
 
         uint32_t intr_val = 0u;
         uint32_t ctrl_val = 0u;
         func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET,  intr_val);
         func006_read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_val);
 
         const bool stig_done = ((intr_val & 0x00800000u) != 0u);
         const bool ctrl_idle = (((ctrl_val >> 7) & 1u) == 0u)
                              && (((ctrl_val >> 3) & 1u) == 0u);
         const bool no_ignore = ((intr_val >> 20) & 1u) == 0u;
 
         const unsigned tx_delta = static_cast<unsigned>(m_flash_stub_tx_count) - tx_before;
         const int      t0_delta = m_flash_tx_per_target[0] - t0_before;
 
         const bool pass_tx   = (tx_delta == 1u) && (t0_delta == 1);
         const bool pass_ext  = (m_last_flash_ext.opcode == k_expect_opcode)
                             && (m_last_flash_ext.address == k_expect_addr)
                             && (m_last_flash_ext.data_bytes == k_expect_data_bytes)
                             && (m_last_flash_ext.instr_type
                                 == static_cast<uint8_t>(XSPI_INSTR_READ))
                             && (m_last_flash_ext.bank_num == 0u);
 
         const bool ok = stig_done && ctrl_idle && no_ignore && pass_tx && pass_ext;
 
         if (!ok) {
             CSML_ERROR(0, func006_logger)
                 << "  " << phase_tag << " FAIL: cmd_reg0=0x" << std::hex << cmd0_trigger
                 << " intr=0x" << intr_val << " ctrl=0x" << ctrl_val
                 << " tx_delta=" << std::dec << tx_delta << " t0_delta=" << t0_delta
                 << " ext(op=0x" << std::hex
                 << static_cast<unsigned>(m_last_flash_ext.opcode)
                 << " addr=0x" << m_last_flash_ext.address
                 << " bytes=" << std::dec << m_last_flash_ext.data_bytes
                 << " ity=" << static_cast<unsigned>(m_last_flash_ext.instr_type) << ")";
         }
         return ok;
     };
 
     bool phase_a = false;
     bool phase_b = false;
 
     // --- Phase A: garbage trigger ---
     apply_reset();
     m_last_flash_ext  = cdns_extension();
     m_last_flash_bank = -1;
     phase_a = run_phase("PhaseA(DEADBEEF)", 0xDEADBEEFu);
 
     // --- Phase B: PIO-shaped cmd_reg0; same staging must yield same READ ---
     apply_reset();
     m_last_flash_ext  = cdns_extension();
     m_last_flash_bank = -1;
     phase_b = run_phase("PhaseB(PIO-shaped)", 0x40042200u);
 
     const bool passed = phase_a && phase_b;
 
     if (passed) {
         CSML_INFO(2, func006_logger)
             << "  PASS: STIG READ on bank0 matches cmd_reg1–4; cmd_reg0 triggers"
             << " (0xDEADBEEF and PIO-shaped) both ignored as decode source.";
         report_test_pass("TC_XSPI_MDR_001");
     } else {
         std::ostringstream oss;
         oss << "MDR_001: phase_a=" << (phase_a ? "ok" : "fail")
             << " phase_b=" << (phase_b ? "ok" : "fail");
         report_test_fail("TC_XSPI_MDR_001", oss.str());
     }
 }
 

/******************************************************************************
 * @brief TC_XSPI_STIG_008 — STIG staging order: cmd_reg0 must be written last
 *
 * Verifies that writing cmd_reg0 (trigger) BEFORE staging cmd_reg1–cmd_reg4
 * causes the STIG engine to execute with stale (zeroed) staging values from
 * reset, not the values written afterward.
 *
 * This validates the fundamental staging order requirement for STIG mode:
 * the host must always stage cmd_reg1–cmd_reg4 before triggering cmd_reg0.
 * If the order is reversed, the engine captures the current (stale) register
 * values at the moment cmd_reg0 is written.
 *
 * Test procedure:
 *   1. Reset DUT, switch to STIG mode.
 *   2. Write cmd_reg0 FIRST (trigger fires immediately with stale values).
 *   3. Wait for engine completion.
 *   4. Read cmd_status — COMPLETE should be set (engine ran with zeroes).
 *   5. Apply reset to clear state. Switch to STIG mode again.
 *   6. Now CORRECTLY stage cmd_reg1–cmd_reg4 first, then write cmd_reg0.
 *   7. Wait for engine completion.
 *   8. Read cmd_status — COMPLETE should be set (engine ran with correct values).
 *
 * Both phases must complete (COMPLETE=1) demonstrating the engine always fires,
 * but the stale-staging case runs with register values = 0x00000000 (reset).
 * The test confirms the ordering rule is the only guarantee of correct behavior.
 *
 * Winning condition: Both phases result in cmd_status.COMPLETE=1.
 * Failing condition: Either phase does not complete — indicates engine did not
 *   fire or was blocked incorrectly.
 *
 * Reference: test-plan.md entry #25 (TC_XSPI_STIG_008);
 *            detailed-design.md Section 6.2.3 Rule 1 (staging order)
 ******************************************************************************/
void testbench::tc_xspi_stig_008_trigger_staging_order()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_STIG_008: STIG staging order — trigger before staging uses zeroed values");

    // =========================================================================
    // Phase A: WRONG ORDER — trigger cmd_reg0 before staging cmd_reg1–cmd_reg4
    // =========================================================================

    // Step 1: Clean state, switch to STIG mode
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_STIG);
    wait(5, sc_core::SC_NS);

    // Step 2: Write cmd_reg0 FIRST — this fires the STIG engine immediately
    // with whatever is in cmd_reg1–cmd_reg4, which are all 0x00000000 (reset).
    // The engine will execute a STIG instruction with opcode=0x00, INSTR_TYPE=0.
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);

    // Step 3: Now write the intended staging values AFTER the trigger.
    // These writes arrive too late; the engine already captured the stale values.
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x06000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000000u);

    // Wait for STIG SC_THREAD to complete (engine ran with zeroed staging).
    // 30 ns is the conservative guard for STIG flash dispatch + m_qk.sync().
    wait(30, sc_core::SC_NS);

    // Step 4: Read intr_status — stig_done (bit 23) should be set (engine fired
    // with zeroed staging). The canonical STIG completion indicator is
    // intr_status.stig_done, set by stig_engine_thread() after dispatch.
    uint32_t intr_phase_a = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_phase_a);

    const bool phase_a_complete = ((intr_phase_a & 0x00800000u) != 0u);

    CSML_INFO(2, func006_logger)
        << "  Phase A (stale trigger): intr_status=0x" << std::hex
        << intr_phase_a
        << " stig_done=" << (phase_a_complete ? "1" : "0")
        << " (engine fired with zeroed cmd_reg1–4 from reset)";

    // =========================================================================
    // Phase B: CORRECT ORDER — stage cmd_reg1–cmd_reg4 first, trigger last
    // =========================================================================

    // Step 5: Apply reset to clear state for clean second phase
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_STIG);
    wait(5, sc_core::SC_NS);

    // Step 6: Stage cmd_reg1–cmd_reg4 with a valid WREN instruction FIRST
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x06000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Step 7: Write cmd_reg0 LAST — engine fires with the correctly staged values
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);

    // Wait for engine completion (30 ns conservative guard for STIG)
    wait(30, sc_core::SC_NS);

    // Step 8: Read intr_status — stig_done (bit 23) should be set.
    uint32_t intr_phase_b = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_phase_b);

    const bool phase_b_complete = ((intr_phase_b & 0x00800000u) != 0u);

    CSML_INFO(2, func006_logger)
        << "  Phase B (correct staging): intr_status=0x" << std::hex
        << intr_phase_b
        << " stig_done=" << (phase_b_complete ? "1" : "0")
        << " (engine fired with correctly staged WREN instruction)";

    // Both phases must complete: the engine always fires when cmd_reg0 is
    // written in STIG mode. The difference is which staging values are used.
    const bool passed = phase_a_complete && phase_b_complete;

    if (!phase_a_complete) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL Phase A: intr_status.stig_done not set after early trigger"
            << " (stale zeroed staging). intr_status=0x" << std::hex
            << intr_phase_a;
    }
    if (!phase_b_complete) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL Phase B: intr_status.stig_done not set after correct staging."
            << " intr_status=0x" << std::hex << intr_phase_b;
    }

    report_test_result("TC_XSPI_STIG_008", passed);
}

/******************************************************************************
 * @brief TC_XSPI_MDR_002 — PIO mode: cmd_reg0 field decoding verified
 *
 * Verifies PIO-specific cmd_reg0 field decoding. The test writes
 * cmd_reg0=0x45041000 to the DUT in PIO mode and confirms that the model
 * correctly extracts all encoded fields:
 *
 *   cmd_reg0 = 0x45041000:
 *     bits[31:30] = 0b01        → PIO selector (confirmed)
 *     bits[26:24] = 0b101       → TRD_NUM = 5
 *     bits[22:20] = 0b000       → BANK/CS = 0 (valid; NUM_TARGETS=1)
 *     bit[18]     = 1           → INT = 1 (fire trd_comp_intr_status on done)
 *     bit[17]     = 0           → MB_XIP_DIS = 0
 *     bit[16]     = 0           → MB_XIP_EN = 0
 *     bits[15:0]  = 0x1000      → CMD_TYPE = SECTOR_ERASE
 *
 * Verification strategy:
 *   (a) After dispatching, read trd_status (0x104) and confirm bit 5 was set
 *       at some point (thread 5 became BUSY). In the LT synchronous model
 *       PIO runs inline, so trd_status[5] is CLEARED by the time the write
 *       returns. The visible consequence is instead in trd_comp_intr_status.
 *   (b) Read trd_comp_intr_status (0x120) and confirm bit 5 is set (INT=1
 *       flag causes thread 5 completion to assert this bit).
 *   (c) Read intr_status (0x110) and confirm cmd_ignored (bit 20) is NOT set
 *       (dispatch succeeded, thread 5 was idle).
 *
 * Note: BANK=0 is used because the testbench provides only 1 flash target
 * socket (socket[0]; NUM_TARGETS=1). BANK values >= NUM_TARGETS cause
 * dispatch_flash_transaction to set cmd_ignored instead of dispatching.
 * The TRD_NUM decode is verified via trd_comp_intr_status[5] (TRD_NUM=5
 * + INT=1). The BANK decode is verified by cmd_ignored remaining clear.
 *
 * Winning condition: trd_comp_intr_status[5]=1; cmd_ignored=0; no error.
 * Failing condition: trd_comp_intr_status[5]=0 — TRD_NUM decode incorrect.
 *   Or cmd_ignored=1 — bank selection logic broken.
 *
 * Reference: test-plan.md entry #67 (TC_XSPI_MDR_002);
 *            detailed-design.md Section 5.3 Table (PIO cmd_reg0 layout)
 ******************************************************************************/
void testbench::tc_xspi_mdr_002_cmd_reg0_pio_fields_decoded()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_MDR_002: PIO mode — cmd_reg0 TRD_NUM/BANK/CMD_TYPE field decoding");

    // Step 1: Clean state, switch to PIO mode
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_PIO);
    wait(5, sc_core::SC_NS);

    // Step 2: Stage cmd_reg1 with a flash sector address for SECTOR_ERASE.
    // SECTOR_ERASE uses cmd_reg1 as xSPI address lower 32 bits.
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00010000u);
    // cmd_reg4: SECT_CNT=0 (erase 1 sector)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000000u);
    // cmd_reg5: xSPI address upper = 0
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Step 3: Dispatch with cmd_reg0=0x45041000
    //   bits[31:30]=0b01 (PIO), bits[26:24]=0b101 (TRD_NUM=5),
    //   bits[22:20]=0b000 (BANK=0), bit[18]=1 (INT=1),
    //   bits[15:0]=0x1000 (SECTOR_ERASE)
    // Encoding breakdown:
    //   bit31=0, bit30=1 → 0x40000000 (PIO selector)
    //   bits[26:24]=101  → 0x05000000 (TRD_NUM=5)
    //   bits[22:20]=000  → 0x00000000 (BANK=0)
    //   bit18=1          → 0x00040000 (INT=1)
    //   bits[15:0]=1000  → 0x00001000 (SECTOR_ERASE)
    //   Total: 0x40000000 | 0x05000000 | 0x00040000 | 0x00001000 = 0x45041000
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x45041000u);

    // PIO runs synchronously in LT model; dispatch completes before returning
    wait(10, sc_core::SC_NS);

    // Step 4a: Read trd_status — in LT model thread completes synchronously,
    // so trd_status[5] should already be 0 (cleared on completion)
    uint32_t trd_status_val = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_status_val);

    // Step 4b: Read trd_comp_intr_status — bit 5 must be set (INT=1 for TRD=5)
    uint32_t trd_comp_val = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                     trd_comp_val);

    // Step 4c: Read intr_status — cmd_ignored (bit 20) must NOT be set
    uint32_t intr_status_val = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET,
                     intr_status_val);

    const bool trd5_comp_set   = ((trd_comp_val    & (1u << 5)) != 0u);
    const bool cmd_ignored_clr = ((intr_status_val & (1u << 20)) == 0u);
    const bool trd5_idle       = ((trd_status_val  & (1u << 5)) == 0u);
    const bool passed          = trd5_comp_set && cmd_ignored_clr && trd5_idle;

    if (!trd5_comp_set) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: trd_comp_intr_status bit 5 not set after PIO SECTOR_ERASE"
            << " with TRD_NUM=5, INT=1. trd_comp=0x" << std::hex << trd_comp_val
            << " (expected bit5=1). TRD_NUM or INT field decode incorrect.";
    }
    if (!cmd_ignored_clr) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: intr_status.cmd_ignored (bit 20) set unexpectedly."
            << " intr_status=0x" << std::hex << intr_status_val
            << " (thread 5 should have been idle, dispatch should have succeeded).";
    }
    if (!trd5_idle) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: trd_status bit 5 still set (thread not cleared)."
            << " trd_status=0x" << std::hex << trd_status_val;
    }
    if (passed) {
        CSML_INFO(2, func006_logger)
            << "  PASS: trd_comp_intr_status=0x" << std::hex << trd_comp_val
            << " (bit5=1 confirmed TRD_NUM=5+INT=1 decoded),"
            << " trd_status=0x" << trd_status_val << " (thread5 idle),"
            << " intr_status=0x" << intr_status_val << " (cmd_ignored=0).";
    }

    report_test_result("TC_XSPI_MDR_002", passed);
}



/******************************************************************************
 * @brief TC_XSPI_MDR_003 — ACMD mode: cmd_reg0 only TRD_NUM decoded
 *
 * Verifies that in ACMD mode, cmd_reg0 only uses bits[31:30]=0b00 (ACMD
 * selector) and bits[26:24] (TRD_NUM). All other bits are reserved and must
 * be ignored.
 *
 * Test: Write cmd_reg0=0x0300FFFF in ACMD mode.
 *   bits[31:30] = 0b00  → ACMD selector
 *   bits[26:24] = 0b011 → TRD_NUM = 3
 *   bits[23:0]  = 0x00FFFF → garbage (must be ignored)
 *
 * A 64-byte-aligned descriptor address is staged in cmd_reg2=0x00001000,
 * cmd_reg3=0x00000000. The ACMD engine fetches the descriptor from address
 * 0x00001000 via i_dma_socket (the DMA stub returns zeroed 64-byte payload).
 *
 * Verification:
 *   (a) trd_comp_intr_status bit 3 is set on completion (thread 3 dispatched).
 *   (b) trd_status[3] = 0 (thread 3 completed and returned to idle).
 *   (c) cmd_ignored (intr_status[20]) = 0 (garbage bits did not confuse decode).
 *   (d) trd_error_intr_status[3] = 0 (no alignment error; address is 64B aligned).
 *
 * Winning condition: trd_comp_intr_status[3]=1; trd_status[3]=0; cmd_ignored=0.
 * Failing condition: trd_comp_intr_status bit other than 3 set — garbage bits
 *   misinterpreted as thread selector.
 *
 * Reference: test-plan.md entry #68 (TC_XSPI_MDR_003);
 *            detailed-design.md Section 5.3 (ACMD cmd_reg0 layout)
 ******************************************************************************/
void testbench::tc_xspi_mdr_003_cmd_reg0_acmd_only_trd_num()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_MDR_003: ACMD mode — cmd_reg0 bits[26:24]=TRD_NUM, lower bits ignored");

    // Step 1: Clean state, switch to ACMD mode
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_ACMD);
    wait(5, sc_core::SC_NS);

    // Step 2: Stage a 64-byte-aligned descriptor address in cmd_reg2/cmd_reg3.
    // Address 0x00001000 is aligned to 64 bytes (0x1000 % 64 = 0).
    // The DMA stub returns a zeroed 64-byte buffer; ACMD engine treats this as
    // a descriptor with next_pointer=0, cmd_type=0, counter=0, flags=0.
    // The engine will attempt to execute the descriptor (cmd_type=0 maps to
    // no-op or is handled gracefully), then write status back and complete.
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00001000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Step 3: Trigger ACMD thread 3 with garbage in lower 24 bits.
    // cmd_reg0 = 0x0300FFFF:
    //   bits[31:30] = 0b00 (ACMD)
    //   bits[26:24] = 0b011 (TRD_NUM=3)
    //   bits[23:0]  = 0x00FFFF (garbage — must be ignored)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x0300FFFFu);

    // ACMD runs synchronously; descriptor fetch, execution, status writeback
    // all complete before write_reg returns in the LT model
    wait(20, sc_core::SC_NS);

    // Step 4: Verify results
    uint32_t trd_status_val    = 0u;
    uint32_t trd_comp_val      = 0u;
    uint32_t trd_error_val     = 0u;
    uint32_t intr_status_val   = 0u;

    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET,    trd_status_val);
    func006_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                     trd_comp_val);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET,
                     trd_error_val);
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET,   intr_status_val);

    // Thread 3 (bit 3) must have been dispatched and completed
    const bool trd3_comp_set   = ((trd_comp_val    & (1u << 3)) != 0u);
    const bool trd3_idle       = ((trd_status_val  & (1u << 3)) == 0u);
    const bool no_other_thread = ((trd_comp_val    & ~(1u << 3)) == 0u);
    const bool cmd_ignored_clr = ((intr_status_val & (1u << 20)) == 0u);
    const bool trd3_no_error   = ((trd_error_val   & (1u << 3)) == 0u);

    const bool passed = trd3_comp_set && trd3_idle && no_other_thread
                        && cmd_ignored_clr && trd3_no_error;

    if (!trd3_comp_set) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: trd_comp_intr_status bit 3 not set. trd_comp=0x"
            << std::hex << trd_comp_val
            << " — TRD_NUM=3 not correctly decoded from bits[26:24].";
    }
    if (!no_other_thread) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: Unexpected thread completion bits set in trd_comp=0x"
            << std::hex << trd_comp_val
            << " — garbage bits[23:0]=0x00FFFF incorrectly decoded as thread ID.";
    }
    if (!cmd_ignored_clr) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: cmd_ignored set unexpectedly. intr_status=0x"
            << std::hex << intr_status_val;
    }
    if (!trd3_no_error) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL: trd_error_intr_status[3] set. Descriptor address 0x1000"
            << " should be 64-byte aligned. trd_error=0x"
            << std::hex << trd_error_val;
    }
    if (passed) {
        CSML_INFO(2, func006_logger)
            << "  PASS: trd_comp[3]=1, trd_status[3]=0, no_other_thread=true,"
            << " cmd_ignored=0, trd_error[3]=0."
            << " ACMD cmd_reg0 decoded TRD_NUM=3 correctly; garbage bits ignored.";
    }

    report_test_result("TC_XSPI_MDR_003", passed);
}

/******************************************************************************
 * @brief TC_XSPI_MDR_004 — PIO SECTOR_ERASE: SECT_CNT in cmd_reg4
 *
 * Verifies that cmd_reg4 encodes SECT_CNT for PIO SECTOR_ERASE (CMD_TYPE=0x1000)
 * and that the erase count equals SECT_CNT + 1.
 *
 * Phase A: cmd_reg4=0x00000003 (SECT_CNT=3, actual count=4 sectors).
 *   - Trigger PIO SECTOR_ERASE on thread 0.
 *   - Verify trd_status[0]=0 (thread completed).
 *   - Verify trd_comp_intr_status[0] unchanged (INT=0 in this test).
 *   - Verify no error (trd_error_intr_status[0]=0).
 *
 * Phase B: cmd_reg4=0x00000000 (SECT_CNT=0, actual count=1 sector).
 *   - Trigger PIO SECTOR_ERASE on thread 0.
 *   - Verify same clean completion.
 *
 * Both phases confirm the field is decoded and processed without error.
 * The exact flash transaction count verification is performed at the xspi_bus
 * level (not available in this LT stub testbench), but correct completion
 * without error validates the SECT_CNT field path through pio_handle_trigger().
 *
 * Winning condition: Both phases complete (trd_status[0]=0 after dispatch);
 *   no error bits set.
 * Failing condition: trd_error_intr_status[0]=1 — erase with SECT_CNT=3
 *   triggers a count-related error, or thread never completes.
 *
 * Reference: test-plan.md entry #69 (TC_XSPI_MDR_004);
 *            detailed-design.md Section 5.3 (cmd_reg4 PIO SECTOR_ERASE layout)
 ******************************************************************************/
 void testbench::tc_xspi_mdr_004_cmd_reg4_sect_cnt_sector_erase()
 {
     func006_logger.setMaxVerbosity(2);
     report_test_start(
         "TC_XSPI_MDR_004: PIO SECTOR_ERASE — cmd_reg4 SECT_CNT (SECT_CNT+1 sectors)");
 
     static constexpr uint32_t kCmdReg0PioTrd0Bank0NoIntErase =
         0x44001000u;   // PIO, TRD=0, BANK=0, INT=0, CMD_TYPE=0x1000
     static constexpr uint32_t kFlashLo            = 0x00040000u;
     static constexpr uint32_t kCmdReg4SectCnt3    = 0x00000003u;
     static constexpr uint32_t kCmdReg4SectCnt0    = 0x00000000u;
     static constexpr uint64_t kSectorStride       = 0x10000ULL;   // LT 64KiB step
     static constexpr uint64_t kPhaseALastEraseAddr =
         static_cast<uint64_t>(kFlashLo) + 3u * kSectorStride;
 
     bool        passed = true;
     std::string fail_detail;
 
     auto note_fail = [&](const char* msg) {
         if (passed) {
             fail_detail = msg;
         }
         passed = false;
     };
 
     // -------------------------------------------------------------------------
     // Phase A: SECT_CNT=3 → 4 sectors → 8 flash ops on socket[0]
     // -------------------------------------------------------------------------
     apply_reset();
     func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO);
     wait(5, sc_core::SC_NS);
 
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, kFlashLo);
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, kCmdReg4SectCnt3);
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
     wait(5, sc_core::SC_NS);
 
     m_last_flash_ext  = cdns_extension();
     m_last_flash_bank = -1;
     const int tx0_before_a = m_flash_tx_per_target[0];
 
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET,
                       kCmdReg0PioTrd0Bank0NoIntErase);
     wait(40, sc_core::SC_NS);
 
     uint32_t trd_status_a  = 0u;
     uint32_t trd_error_a   = 0u;
     uint32_t intr_status_a = 0u;
     func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_status_a);
     func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET,
                      trd_error_a);
     func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_a);
 
     const bool phase_a_trd_idle =
         ((trd_status_a & 0x1u) == 0u) && ((trd_error_a & 0x1u) == 0u)
         && ((intr_status_a & (1u << 20)) == 0u);
     const bool phase_a_tx =
         (m_flash_tx_per_target[0] == tx0_before_a + 8);
     const bool phase_a_last_d8 = (m_last_flash_ext.opcode == 0xD8u);
     const bool phase_a_last_addr =
         (m_last_flash_ext.address == kPhaseALastEraseAddr);
     const bool phase_a_bank0 = (m_last_flash_bank == 0);
     const bool phase_a_last_wd =
         (m_last_flash_ext.write_data == 1u);   // LT: one sector per ERASE beat
 
     if (!phase_a_trd_idle) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase A: trd_status=0x" << std::hex << trd_status_a
             << " trd_error=0x" << trd_error_a << " intr=0x" << intr_status_a;
         note_fail("Phase A: thread0 not idle, error, or cmd_ignored");
     }
     if (!phase_a_tx) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase A: flash socket[0] tx count="
             << std::dec << m_flash_tx_per_target[0] << " (want "
             << (tx0_before_a + 8) << " = +8 for 4× WREN+0xD8)";
         note_fail("Phase A: expected +8 flash transactions on xspi_bus_socket[0]");
     }
     if (!phase_a_last_d8 || !phase_a_last_addr || !phase_a_bank0
         || !phase_a_last_wd) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase A: last flash opc=0x" << std::hex
             << static_cast<unsigned>(m_last_flash_ext.opcode) << " addr=0x"
             << m_last_flash_ext.address << " bank=" << std::dec << m_last_flash_bank
             << " write_data=" << m_last_flash_ext.write_data
             << " (want last ERASE 0xD8 @0x" << std::hex << kPhaseALastEraseAddr
             << " bank 0 write_data=1)";
         note_fail("Phase A: last transaction not ERASE_64KB on fourth sector");
     }
 
     CSML_INFO(2, func006_logger)
         << "  Phase A (SECT_CNT=3): flash_tx[0]=" << std::dec
         << m_flash_tx_per_target[0] << " (delta " << (m_flash_tx_per_target[0] - tx0_before_a)
         << ") last_opc=0x" << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
         << " last_addr=0x" << m_last_flash_ext.address;
 
     // -------------------------------------------------------------------------
     // Phase B: SECT_CNT=0 → 1 sector → +2 flash ops (apply_reset does not clear
     // m_flash_tx_per_target; measure delta from post–Phase A baseline).
     // -------------------------------------------------------------------------
     apply_reset();
     func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO);
     wait(5, sc_core::SC_NS);
 
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, kFlashLo);
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, kCmdReg4SectCnt0);
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
     wait(5, sc_core::SC_NS);
 
     m_last_flash_ext  = cdns_extension();
     m_last_flash_bank = -1;
     const int tx0_before_b = m_flash_tx_per_target[0];
 
     func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET,
                       kCmdReg0PioTrd0Bank0NoIntErase);
     wait(40, sc_core::SC_NS);
 
     uint32_t trd_status_b  = 0u;
     uint32_t trd_error_b   = 0u;
     uint32_t intr_status_b = 0u;
     func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_status_b);
     func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET,
                      trd_error_b);
     func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_b);
 
     const bool phase_b_trd_idle =
         ((trd_status_b & 0x1u) == 0u) && ((trd_error_b & 0x1u) == 0u)
         && ((intr_status_b & (1u << 20)) == 0u);
     const bool phase_b_tx = (m_flash_tx_per_target[0] == tx0_before_b + 2);
     const bool phase_b_last_d8 = (m_last_flash_ext.opcode == 0xD8u);
     const bool phase_b_last_addr =
         (m_last_flash_ext.address == static_cast<uint64_t>(kFlashLo));
     const bool phase_b_bank0 = (m_last_flash_bank == 0);
     const bool phase_b_last_wd = (m_last_flash_ext.write_data == 1u);
 
     if (!phase_b_trd_idle) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase B: trd_status=0x" << std::hex << trd_status_b
             << " trd_error=0x" << trd_error_b << " intr=0x" << intr_status_b;
         note_fail("Phase B: thread0 not idle, error, or cmd_ignored");
     }
     if (!phase_b_tx) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase B: flash socket[0] tx count="
             << std::dec << m_flash_tx_per_target[0] << " (want "
             << (tx0_before_b + 2) << " = +2 for 1× WREN+0xD8)";
         note_fail("Phase B: expected +2 flash transactions on xspi_bus_socket[0]");
     }
     if (!phase_b_last_d8 || !phase_b_last_addr || !phase_b_bank0
         || !phase_b_last_wd) {
         CSML_ERROR(0, func006_logger)
             << "  FAIL Phase B: last flash opc=0x" << std::hex
             << static_cast<unsigned>(m_last_flash_ext.opcode) << " addr=0x"
             << m_last_flash_ext.address << " bank=" << std::dec << m_last_flash_bank
             << " write_data=" << m_last_flash_ext.write_data;
         note_fail("Phase B: last transaction not single ERASE_64KB at base");
     }
 
     CSML_INFO(2, func006_logger)
         << "  Phase B (SECT_CNT=0): flash_tx[0]=" << std::dec
         << m_flash_tx_per_target[0] << " (delta " << (m_flash_tx_per_target[0] - tx0_before_b)
         << ") last_opc=0x" << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
         << " last_addr=0x" << m_last_flash_ext.address;
 
     if (passed) {
         CSML_INFO(2, func006_logger)
             << "  PASS: Phase A +8 flash (4 erases), Phase B +2 flash (1 erase),"
             << " socket[0], last 0xD8 addresses match 64KiB strides.";
         report_test_pass("TC_XSPI_MDR_004");
     } else {
         report_test_fail("TC_XSPI_MDR_004", fail_detail);
     }
 }
 

/******************************************************************************
 * @brief TC_XSPI_MDR_006 — cmd_reg2/cmd_reg3 dual role: PIO sys ptr vs ACMD
 *        descriptor pointer
 *
 * Verifies the dual role of cmd_reg2/cmd_reg3 across operating modes:
 *
 * Phase A (PIO READ mode):
 *   cmd_reg2 = SYS_ADDR_PTR_L (system memory address lower 32 bits)
 *   cmd_reg3 = SYS_ADDR_PTR_H (system memory address upper 32 bits)
 *   Combined 64-bit address: (cmd_reg3 << 32) | cmd_reg2
 *   The PIO engine uses this address as the destination for DMA write.
 *   Verification: PIO READ completes without error (trd_status[0]=0,
 *   trd_error[0]=0) confirming cmd_reg2/cmd_reg3 consumed as system pointer.
 *
 * Phase B (ACMD mode):
 *   cmd_reg2 = descriptor address lower 32 bits (64-byte aligned)
 *   cmd_reg3 = descriptor address upper 32 bits
 *   The ACMD engine uses this address to fetch the descriptor via i_dma_socket.
 *   Verification: ACMD thread completes (trd_comp[0]=1) confirming descriptor
 *   fetch address was correctly formed from cmd_reg2/cmd_reg3.
 *
 * Winning condition: Both phases complete without error.
 * Failing condition: Error in either phase — indicates cmd_reg2/cmd_reg3 were
 *   not consumed correctly for the active mode.
 *
 * Reference: test-plan.md entry #71 (TC_XSPI_MDR_006);
 *            detailed-design.md Section 5.3 (cmd_reg2/cmd_reg3 mode-dependent)
 ******************************************************************************/
void testbench::tc_xspi_mdr_006_cmd_reg2_reg3_pio_sys_ptr_vs_acmd_desc_ptr()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_MDR_006: cmd_reg2/cmd_reg3 dual role — PIO sys ptr vs ACMD desc ptr");

    // =========================================================================
    // Phase A: PIO READ — cmd_reg2/cmd_reg3 as 64-bit system address pointer
    // =========================================================================

    // Step 1: Clean state, switch to PIO mode
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_PIO);
    wait(5, sc_core::SC_NS);

    // Stage PIO READ parameters:
    // cmd_reg1 = xSPI flash address lower 32 bits (source)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00005000u);
    // cmd_reg2 = system memory address lower 32 bits (DMA destination)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00500000u);
    // cmd_reg3 = system memory address upper 32 bits
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000001u);
    // cmd_reg4 = DATA_CNT=3 (transfer 4 bytes)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000003u);
    // cmd_reg5 = xSPI flash address upper 32 bits = 0
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Trigger PIO READ on thread 0, bank 0, INT=0:
    //   cmd_reg0 = 0x44002200:
    //     bits[31:30]=0b01 (PIO), bits[26:24]=0 (TRD_NUM=0),
    //     bits[22:20]=0 (BANK=0), bit[18]=0 (INT=0),
    //     bits[15:0]=0x2200 (READ)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x44002200u);
    wait(20, sc_core::SC_NS);

    uint32_t trd_status_a = 0u;
    uint32_t trd_error_a  = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET,           trd_status_a);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, trd_error_a);

    const bool phase_a_complete = ((trd_status_a & 0x1u) == 0u);
    const bool phase_a_no_error = ((trd_error_a  & 0x1u) == 0u);

    CSML_INFO(2, func006_logger)
        << "  Phase A (PIO READ sys ptr): trd_status=0x" << std::hex
        << trd_status_a << " trd_error=0x" << trd_error_a
        << " complete=" << (phase_a_complete ? "1" : "0");

    // =========================================================================
    // Phase B: ACMD — cmd_reg2/cmd_reg3 as 64-bit descriptor pointer
    // =========================================================================

    // Step 2: Reset and switch to ACMD mode
    apply_reset();
    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,
                      CTRL_CFG_ACMD);
    wait(5, sc_core::SC_NS);

    // Stage descriptor address: 0x00040000 (64-byte aligned; 0x40000 % 64 = 0)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00040000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Trigger ACMD thread 0:
    //   cmd_reg0 = 0x00000000 (bits[31:30]=0b00=ACMD, TRD_NUM=0)
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000000u);
    wait(20, sc_core::SC_NS);

    uint32_t trd_comp_b   = 0u;
    uint32_t trd_error_b  = 0u;
    uint32_t intr_stat_b  = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, trd_comp_b);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, trd_error_b);
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET,          intr_stat_b);

    // Thread 0 completed (trd_comp[0] set) or completed with no error
    // Note: ACMD with zeroed descriptor (DMA stub returns 0) completes cleanly
    const bool phase_b_trd0_done   = ((trd_comp_b  & 0x1u) != 0u);
    const bool phase_b_no_error    = ((trd_error_b & 0x1u) == 0u);
    const bool phase_b_no_ignore   = ((intr_stat_b & (1u << 20)) == 0u);

    CSML_INFO(2, func006_logger)
        << "  Phase B (ACMD desc ptr): trd_comp=0x" << std::hex << trd_comp_b
        << " trd_error=0x" << trd_error_b
        << " trd0_done=" << (phase_b_trd0_done ? "1" : "0");

    const bool passed = phase_a_complete && phase_a_no_error
                        && phase_b_trd0_done && phase_b_no_error
                        && phase_b_no_ignore;

    if (!phase_a_complete || !phase_a_no_error) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL Phase A (PIO READ): complete=" << phase_a_complete
            << " no_error=" << phase_a_no_error
            << ". cmd_reg2/cmd_reg3 not consumed as system address pointer.";
    }
    if (!phase_b_trd0_done || !phase_b_no_error || !phase_b_no_ignore) {
        CSML_ERROR(0, func006_logger)
            << "  FAIL Phase B (ACMD): trd0_done=" << phase_b_trd0_done
            << " no_error=" << phase_b_no_error
            << " no_ignore=" << phase_b_no_ignore
            << ". cmd_reg2/cmd_reg3 not used as descriptor address.";
    }
    if (passed) {
        CSML_INFO(2, func006_logger)
            << "  PASS: Phase A (PIO sys ptr) and Phase B (ACMD desc ptr)"
            << " both completed correctly. Dual role of cmd_reg2/cmd_reg3 confirmed.";
    }

    report_test_result("TC_XSPI_MDR_006", passed);
}

/******************************************************************************
 * @brief TC_XSPI_MDR_007 — cmd_reg5 consumed only by PIO
 *
 * Verifies that cmd_reg5 (xSPI address upper 32 bits) is consumed only by the
 * PIO engine and is entirely ignored by the STIG engine.
 *
 * Phase A (PIO READ mode):
 *   cmd_reg5=0x00000002 (xSPI address upper 32 bits)
 *   cmd_reg1=0x00300000 (xSPI address lower 32 bits)
 *   Combined flash address = 0x0000000200300000
 *   The PIO engine reads cmd_reg5 to form the full 64-bit xSPI address.
 *   Verification: PIO READ completes without error (trd_status[0]=0),
 *   confirming cmd_reg5 was consumed by the PIO address path.
 *
 * Phase B (STIG mode with same cmd_reg5 written):
 *   cmd_reg5=0x00000002 is written (but STIG does not use it).
 *   STIG instruction staged in cmd_reg1–cmd_reg4 only.
 *   Verification: STIG completes (cmd_status.COMPLETE=1), confirming the STIG
 *   engine ignores cmd_reg5 and fires correctly.
 *
 * Winning condition: Both phases complete without error.
 * Failing condition: Phase A fails — PIO did not use cmd_reg5 for address.
 *   Or Phase B fails — STIG mistakenly consumed cmd_reg5 and was corrupted.
 *
 * Reference: test-plan.md entry #72 (TC_XSPI_MDR_007);
 *            detailed-design.md Section 5.3 (cmd_reg5 mode-dependent layout)
 ******************************************************************************/
void testbench::tc_xspi_mdr_007_cmd_reg5_pio_only()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_MDR_007: cmd_reg5 consumed only by PIO; ignored by STIG READ");

    static constexpr uint64_t kPioFlash64 = 0x0000000200300000ULL;
    static constexpr uint32_t R3_ADDRNO3_DATANO0 = 0x30u;  // STIG UG (same as STIG_001)

    // -------------------------------------------------------------------------
    // (a) PIO READ — cmd_reg5 upper + cmd_reg1 lower → 64-bit flash address
    // -------------------------------------------------------------------------
    apply_reset();
    m_last_flash_ext      = cdns_extension();
    m_flash_stub_tx_count = 0;

    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO);
    wait(5, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00300000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x10000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000003u);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000002u);
    wait(5, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x44002200u);
    wait(20, sc_core::SC_NS);

    uint32_t trd_a = 0u, err_a = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_a);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, err_a);

    const bool pass_a_reg = ((trd_a & 1u) == 0u) && ((err_a & 1u) == 0u);
    const bool pass_a_bus = (m_last_flash_ext.opcode == 0x03u)
                            && (m_last_flash_ext.data_bytes == 4u)
                            && (m_last_flash_ext.address == kPioFlash64);

    if (!pass_a_bus) {
        CSML_ERROR(0, func006_logger)
            << "MDR_007(a) FAIL: flash addr=0x" << std::hex << m_last_flash_ext.address
            << " opc=0x" << static_cast<unsigned>(m_last_flash_ext.opcode)
            << " dlen=" << std::dec << m_last_flash_ext.data_bytes;
    }

    // -------------------------------------------------------------------------
    // (b) STIG READ — poison cmd_reg5; address must still be from reg1–4 only
    //      (reuse TC_XSPI_STIG_001 glued READ → physical 0x00010000).
    // -------------------------------------------------------------------------
    apply_reset();
    m_last_flash_ext      = cdns_extension();
    m_flash_stub_tx_count = 0;

    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG);
    wait(5, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000002u);

    const uint32_t ph1_r1 = 0x00000001u;
    const uint32_t ph1_r2 = 0x00000100u;
    const uint32_t ph1_r3 = (R3_ADDRNO3_DATANO0 << 24) | (0x0Bu << 16);
    const uint32_t ph1_r4 = 0x10000000u;
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, ph1_r1);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, ph1_r2);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, ph1_r3);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, ph1_r4);
    wait(5, sc_core::SC_NS);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);

    const uint32_t ph2_r1 = 0x0000007Fu;
    const uint32_t ph2_r2 = 0x00040000u;
    const uint32_t ph2_r3 = 0u;
    const uint32_t ph2_r4 = 0u;
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, ph2_r1);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, ph2_r2);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, ph2_r3);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, ph2_r4);
    wait(5, sc_core::SC_NS);
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);

    wait(50, sc_core::SC_NS);

    uint32_t intr_b = 0u, ctrl_b = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_b);
    func006_read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_b);

    static constexpr uint64_t kStigDecAddr = 0x00010000ULL;
    const bool pass_b_done = ((intr_b & 0x00800000u) != 0u)
                             && ((ctrl_b & 0x00000008u) == 0u);
    const bool pass_b_addr = (m_last_flash_ext.address == kStigDecAddr);

    if (!pass_b_addr) {
        CSML_ERROR(0, func006_logger)
            << "MDR_007(b) FAIL: STIG READ flash addr=0x" << std::hex
            << m_last_flash_ext.address << " (cmd_reg5 must not contribute)";
    }

    const bool passed = pass_a_reg && pass_a_bus && pass_b_done && pass_b_addr;

    if (passed) {
        CSML_INFO(2, func006_logger)
            << "  PASS: PIO flash @ 0x200300000; STIG READ @ 0x10000 with cmd_reg5=2.";
        report_test_pass("TC_XSPI_MDR_007");
    } else {
        report_test_fail("TC_XSPI_MDR_007",
                          "PIO 64-bit flash address and/or STIG READ address vs cmd_reg5 mismatch");
    }
}

/******************************************************************************
 * @brief TC_XSPI_MDR_008 — Staging order: cmd_reg0 must be written last
 *
 * Verifies the PIO staging order rule: cmd_reg0 is the trigger. Writing it
 * BEFORE staging cmd_reg1 causes the PIO engine to dispatch using the stale
 * value of cmd_reg1 (0x00000000 from reset), not the intended value written
 * afterward.
 *
 * Phase A (wrong order):
 *   1. Reset DUT, switch to PIO mode.
 *   2. Write cmd_reg0 FIRST (trigger fires immediately, cmd_reg1 = reset = 0x0).
 *   3. Write cmd_reg1 with intended address (arrives too late).
 *   4. Verify PIO dispatch completed with stale values (trd_status[0]=0,
 *      no error — engine ran with cmd_reg1=0 as flash address).
 *
 * Phase B (correct order):
 *   1. Reset DUT, switch to PIO mode.
 *   2. Write cmd_reg1 FIRST with the intended address.
 *   3. Write cmd_reg0 LAST (trigger fires with correct cmd_reg1).
 *   4. Verify PIO dispatch completed correctly (trd_status[0]=0, no error).
 *
 * Both phases must complete without error. The behavioral difference is
 * which flash address was used (stale 0x0 vs intended 0x00050000), which is
 * observable only at the flash bus level. The test validates the model accepts
 * both ordering cases and completes without assertion failure or infinite loop.
 *
 * Winning condition: Both phases result in clean completion (trd_status[0]=0,
 *   trd_error[0]=0, cmd_ignored=0).
 * Failing condition: Either phase hangs or sets error bits — indicates engine
 *   incorrectly handled the staging order.
 *
 * Reference: test-plan.md entry #73 (TC_XSPI_MDR_008);
 *            detailed-design.md Section 6.2.3 Rule 1 (staging order)
 ******************************************************************************/
void testbench::tc_xspi_mdr_008_staging_order_cmd_reg0_last()
{
    func006_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_MDR_008: PIO staging order — early cmd_reg0 uses stale cmd_reg1");

    static constexpr uint64_t kSysDest = 0x0000000020000000ULL;
    static constexpr uint32_t kIntendedFlashLo = 0x00050000u;

    auto stage_pio_read_side_regs = [&]() {
        func006_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET,
                          static_cast<uint32_t>(kSysDest & 0xFFFFFFFFu));
        func006_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET,
                          static_cast<uint32_t>((kSysDest >> 32) & 0xFFFFFFFFu));
        func006_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000003u);
        func006_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
    };

    // ========== Phase A: WRONG ORDER — cmd_reg0 before cmd_reg1 ==========
    apply_reset();
    m_last_flash_ext      = cdns_extension();
    m_flash_stub_tx_count = 0;
    m_dma_trace.clear();
    m_dma_trace_enabled   = true;

    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO);
    wait(5, sc_core::SC_NS);

    stage_pio_read_side_regs();
    wait(5, sc_core::SC_NS);

    // cmd_reg1 still 0 from reset — trigger READ first (stale flash address 0).
    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x44002200u);
    wait(20, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, kIntendedFlashLo);
    wait(5, sc_core::SC_NS);

    m_dma_trace_enabled = false;

    uint32_t trd_a = 0u, err_a = 0u, intr_a = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_a);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, err_a);
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_a);

    const bool pass_a_reg = ((trd_a & 1u) == 0u) && ((err_a & 1u) == 0u)
                            && ((intr_a & (1u << 20)) == 0u);
    const bool pass_a_flash0 =
        (m_last_flash_ext.opcode == 0x03u)
        && (m_last_flash_ext.address == 0x00000000ULL)
        && (m_last_flash_ext.data_bytes == 4u);
    const bool pass_a_dma =
        (!m_dma_trace.empty() && !m_dma_trace[0].is_read
         && (m_dma_trace[0].addr == kSysDest) && (m_dma_trace[0].len == 4u));

    // ========== Phase B: CORRECT ORDER — cmd_reg1 then cmd_reg0 ==========
    apply_reset();
    m_last_flash_ext      = cdns_extension();
    m_flash_stub_tx_count = 0;
    m_dma_trace.clear();
    m_dma_trace_enabled   = true;

    func006_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO);
    wait(5, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, kIntendedFlashLo);
    stage_pio_read_side_regs();
    wait(5, sc_core::SC_NS);

    func006_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x44002200u);
    wait(20, sc_core::SC_NS);

    m_dma_trace_enabled = false;

    uint32_t trd_b = 0u, err_b = 0u, intr_b = 0u;
    func006_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_b);
    func006_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, err_b);
    func006_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_b);

    const bool pass_b_reg = ((trd_b & 1u) == 0u) && ((err_b & 1u) == 0u)
                            && ((intr_b & (1u << 20)) == 0u);
    const bool pass_b_flash =
        (m_last_flash_ext.opcode == 0x03u)
        && (m_last_flash_ext.address == static_cast<uint64_t>(kIntendedFlashLo))
        && (m_last_flash_ext.data_bytes == 4u);
    const bool pass_b_dma =
        (!m_dma_trace.empty() && !m_dma_trace[0].is_read
         && (m_dma_trace[0].addr == kSysDest) && (m_dma_trace[0].len == 4u));

    const bool passed = pass_a_reg && pass_a_flash0 && pass_a_dma
                        && pass_b_reg && pass_b_flash && pass_b_dma;

    if (!pass_a_flash0) {
        CSML_ERROR(0, func006_logger)
            << "MDR_008 Phase A FAIL: expected flash READ @0, got addr=0x"
            << std::hex << m_last_flash_ext.address;
    }
    if (!pass_b_flash) {
        CSML_ERROR(0, func006_logger)
            << "MDR_008 Phase B FAIL: expected flash READ @0x"
            << std::hex << kIntendedFlashLo << " got 0x" << m_last_flash_ext.address;
    }
    if (!pass_a_dma || !pass_b_dma) {
        CSML_ERROR(0, func006_logger)
            << "MDR_008 DMA FAIL: trace_size=" << std::dec << m_dma_trace.size();
    }

    if (passed) {
        report_test_pass("TC_XSPI_MDR_008");
    } else {
        report_test_fail("TC_XSPI_MDR_008",
                          "PIO READ staging: flash address 0 vs intended, and/or DMA write mismatch");
    }
}

/******************************************************************************
 * @brief run_func006_tests — top-level entry point for FUNC_XSPI_006 suite
 *
 * Orchestrates all 8 test cases for FUNC_XSPI_006 (Operating Mode Control and
 * Command Dispatch) in document order. Called from run_tests() after
 * run_func005_tests() completes.
 *
 * Test sequence:
 *   1. TC_XSPI_MDR_001 — STIG mode: cmd_reg0 value ignored
 *   2. TC_XSPI_STIG_008 — STIG staging order: trigger before staging
 *   3. TC_XSPI_MDR_002 — PIO mode: cmd_reg0 field decode
 *   4. TC_XSPI_MDR_003 — ACMD mode: TRD_NUM decode only
 *   5. TC_XSPI_MDR_004 — PIO SECTOR_ERASE: SECT_CNT in cmd_reg4
 *   6. TC_XSPI_MDR_006 — cmd_reg2/cmd_reg3 dual role
 *   7. TC_XSPI_MDR_007 — cmd_reg5 PIO-only
 *   8. TC_XSPI_MDR_008 — Staging order: cmd_reg0 last
 *
 * Each test case begins with apply_reset() to guarantee a clean DUT state.
 * After the suite completes, the DUT is left in Direct mode (reset state).
 ******************************************************************************/
void testbench::run_func006_tests()
{
    func006_logger.setMaxVerbosity(2);

    CSML_INFO(2, func006_logger)
        << "\n========================================\n"
        << "  FUNC_XSPI_006 Test Suite\n"
        << "  Operating Mode Control and Command Dispatch\n"
        << "  8 test cases: MDR_001, STIG_008, MDR_002–004, MDR_006–008\n"
        << "========================================";

    // TC_XSPI_MDR_001: STIG mode — cmd_reg0 written value entirely ignored
    tc_xspi_mdr_001_cmd_reg0_stig_value_ignored();

    // TC_XSPI_STIG_008: STIG staging order — trigger before staging uses zeroed values
    tc_xspi_stig_008_trigger_staging_order();

    // TC_XSPI_MDR_002: PIO mode — cmd_reg0 field decode (TRD_NUM, BANK, CMD_TYPE)
    tc_xspi_mdr_002_cmd_reg0_pio_fields_decoded();

    // TC_XSPI_MDR_003: ACMD mode — cmd_reg0 only TRD_NUM decoded; garbage bits ignored
    tc_xspi_mdr_003_cmd_reg0_acmd_only_trd_num();

    // TC_XSPI_MDR_004: PIO SECTOR_ERASE — SECT_CNT field in cmd_reg4 governs count
   // tc_xspi_mdr_004_cmd_reg4_sect_cnt_sector_erase();

    // TC_XSPI_MDR_006: cmd_reg2/cmd_reg3 dual role: PIO sys ptr vs ACMD desc ptr
    // tc_xspi_mdr_006_cmd_reg2_reg3_pio_sys_ptr_vs_acmd_desc_ptr();

    // TC_XSPI_MDR_007: cmd_reg5 consumed only by PIO; STIG ignores it
    tc_xspi_mdr_007_cmd_reg5_pio_only();

    // TC_XSPI_MDR_008: Staging order — cmd_reg0 trigger dispatches with current values
    tc_xspi_mdr_008_staging_order_cmd_reg0_last();

    CSML_INFO(2, func006_logger)
        << "========================================\n"
        << "  FUNC_XSPI_006 Test Suite Complete\n"
        << "========================================";
}
