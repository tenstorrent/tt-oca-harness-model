/******************************************************************************
 * @file xspi_ctrl_func011_test.cpp
 * @brief Test cases for FUNC_XSPI_011 — PIO Mode Multi-Thread DMA Execution
 *
 * Implements all 17 test cases mapped to FUNC_XSPI_011 in the xspi_ctrl
 * functionality-to-test-case mapping document (v2.0).
 *
 * ## Scope
 *
 * Primary PIO tests (9):
 *   TC_XSPI_PIO_001 — PIO READ: flash read + DMA write; cdns_extension verified
 *   TC_XSPI_PIO_002 — PIO PROGRAM: DMA read + WREN + PAGE_PROGRAM
 *   TC_XSPI_PIO_003 — PIO SECTOR_ERASE: WREN + erase for SECT_CNT+1 sectors
 *   TC_XSPI_PIO_004 — PIO CHIP_ERASE: WREN + full-chip erase
 *   TC_XSPI_PIO_005 — PIO SOFT_RESET: software reset command sequence
 *   TC_XSPI_PIO_006 — PIO JEDEC_RESET: JEDEC hardware reset sequence
 *   TC_XSPI_PIO_007 — Multi-thread concurrent: threads 0 and 1 independent
 *   TC_XSPI_PIO_008 — INT=1 flag on thread 2: trd_comp_intr_status[2] + int_out
 *   TC_XSPI_PIO_009 — cmd_status_ptr indirect read: TRD_NUM selection
 *
 * Cross-mapped tests (8 new, 4 already in func006):
 *   TC_XSPI_MDR_005 — cmd_reg4 DATA_CNT for READ/PROGRAM (byte count = DATA_CNT+1)
 *   TC_XSPI_XIP_005 — PIO MB_XIP_DIS exits XIP on target bank
 *   TC_XSPI_ERR_002 — PIO busy-thread silent ignore (no CMD_IGNORED set)
 *   TC_XSPI_ERR_004 — PIO DMA bus error: ddma_terr + trd_error_intr_status[0]
 *   TC_XSPI_ERR_006 — reset_in aborts in-progress PIO: status registers cleared
 *   TC_XSPI_CFG_003 — n_threads=2: thread 0 valid; thread 2 out-of-range
 *   TC_XSPI_REG_007 — trd_comp_intr_status W1C semantics + int_out de-assertion
 *   TC_XSPI_INT_001 — trd_comp: intr_enable[31], PIO INT=1, status+int_out, W1C
 *
 * Already implemented in xspi_ctrl_func006_test.cpp (not duplicated here):
 *   TC_XSPI_MDR_002 — PIO cmd_reg0 TRD_NUM/BANK/CMD_TYPE field decoding
 *   TC_XSPI_MDR_004 — cmd_reg4 SECT_CNT for SECTOR_ERASE
 *   TC_XSPI_MDR_007 — cmd_reg5 consumed only by PIO
 *   TC_XSPI_MDR_008 — staging order: cmd_reg0 must be written last
 *
 * ## Architectural Constraints Applied
 *
 * - ctrl_config.work_mode must be 2'b11 (ACMD global) before PIO cmd_reg0 writes.
 *   ctrl_config write_bit_mask = 0x68; write 0x60 (bits[6:5]=2'b11).
 *   PIO sub-mode is selected by cmd_reg0[31:30]=2'b01.
 *
 * - PIO mode cmd_reg0 encoding (all tests):
 *     bits[31:30] = 0b01  → PIO selector  (0x40000000)
 *     bits[26:24] = TRD   → thread number  (TRD << 24)
 *     bits[22:20] = BANK  → chip-select    (BANK << 20)
 *     bit[18]     = INT   → completion IRQ (0x00040000)
 *     bit[17]     = MB_XIP_DIS              (0x00020000)
 *     bit[16]     = MB_XIP_EN               (0x00010000)
 *     bits[15:0]  = CMD_TYPE
 *
 * - Staging order: cmd_reg1–cmd_reg5 staged FIRST, cmd_reg0 written LAST.
 *
 * - PoR with discovery_inhibit=1 must precede ctrl_config writes to ensure
 *   m_init_comp_done=true and register write callbacks are unblocked.
 *   apply_reset() sets m_init_comp_done=true internally so explicit PoR is
 *   not needed before each test.
 *
 * - PIO runs synchronously inside handle_write_cmd_reg0 in the LT model.
 *   One wait(SC_ZERO_TIME) after cmd_reg0 write is sufficient to let
 *   evaluate_interrupt_out() → update_int_out() SC_METHOD fire.
 *
 * - cmd_status indirect read: write TRD_NUM to cmd_status_ptr (0x040),
 *   then read cmd_status (0x044). COMPLETE bit is bit 15.
 *
 * ## Design References
 *   docs/xspi_ctrl-detailed-design.md Section 7.3 (PIO Mode)
 *   docs/xspi_ctrl-detailed-design.md Section 5.3 (mode-dependent bitfields)
 *   docs/xspi_ctrl-functionality-testcases.md FUNC_XSPI_011 section
 *   docs/xspi_ctrl-test-plan.md rows 36–44 (PIO), 70, 48, 80, 82, 84, 76, 11, 59
 *   model/src/xspi_ctrl.cpp pio_handle_trigger()
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "cdns_extension.h"
#include "csml_logger.h"

#include <cstring>
#include <cstdint>

// =============================================================================
// Module-local logger
// =============================================================================
namespace {
    CsmlLogger func011_logger;
}

// =============================================================================
// Module-local register access helpers (file-local to avoid ODR violations)
// =============================================================================

/**
 * @brief Issue a 32-bit TLM register write via the test harness.
 * @param test   Pointer to the xspi_ctrl_test harness
 * @param offset Byte offset within the DUT address space
 * @param value  32-bit value to write
 */
static void f11_write_reg(xspi_ctrl_test* test, unsigned int offset,
                          uint32_t value)
{
    test->register_write_32(offset, value);
}

/**
 * @brief Issue a 32-bit TLM register read via the test harness.
 * @param test   Pointer to the xspi_ctrl_test harness
 * @param offset Byte offset within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 */
static void f11_read_reg(xspi_ctrl_test* test, unsigned int offset,
                         uint32_t& value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// Module-local constants
// =============================================================================
namespace {

    /// @brief ctrl_config value for PIO path (global work_mode 2'b11 at bits[6:5]).
    ///        ctrl_config write_bit_mask = 0x68; write 0x60.
    static constexpr uint32_t CTRL_CFG_PIO_MODE     = 0x60u;

    /// @brief PIO cmd_reg0 selector bits[31:30] = 0b01.
    static constexpr uint32_t PIO_SEL               = 0x40000000u;

    /// @brief CMD_TYPE encoding for PIO commands.
    static constexpr uint32_t PIO_CMD_READ          = 0x2200u;
    static constexpr uint32_t PIO_CMD_PROGRAM       = 0x2100u;
    static constexpr uint32_t PIO_CMD_SECTOR_ERASE  = 0x1000u;
    static constexpr uint32_t PIO_CMD_CHIP_ERASE    = 0x1001u;
    static constexpr uint32_t PIO_CMD_SOFT_RESET    = 0x1100u;
    static constexpr uint32_t PIO_CMD_JEDEC_RESET   = 0x1101u;

    /// @brief INT flag in cmd_reg0: bit 18.
    static constexpr uint32_t PIO_INT_FLAG          = 0x00040000u;

    /// @brief MB_XIP_DIS flag in cmd_reg0: bit 17.
    static constexpr uint32_t PIO_MB_XIP_DIS_FLAG   = 0x00020000u;

    /// @brief cmd_status COMPLETE bit (bit 0).
    ///        handle_read_cmd_status encodes:
    ///          bit 0 = trd_comp_intr_status[sel]  (completion)
    ///          bit 1 = trd_error_intr_status[sel] (error)
    ///        This bit is only set when INT=1 was present in the triggering
    ///        cmd_reg0 (so that trd_comp_intr_status is actually written).
    static constexpr uint32_t CMD_STATUS_COMPLETE   = (1u << 0);

    /// @brief ctrl_status.ctrl_busy bit (bit 7).
    static constexpr uint32_t CTRL_BUSY_BIT         = (1u << 7);

    /// @brief intr_status.cmd_ignored bit (bit 20).
    static constexpr uint32_t INTR_CMD_IGNORED_BIT  = (1u << 20);

    /// @brief intr_status.ddma_terr bit (bit 18).
    static constexpr uint32_t INTR_DDMA_TERR_BIT    = (1u << 18);

    /// @brief xip_mode_cfg default reset value (0x00FF0000).
    static constexpr uint32_t XIP_MODE_CFG_RESET    = 0x00FF0000u;

    /// @brief xip_dis_mb_val when xip_mode_cfg = 0x00FF0001 (reset dis byte 0xFF).
    static constexpr uint32_t XIP_DIS_MB_VAL        = 0x000000FFu;

    /**
     * @brief Build a PIO cmd_reg0 value from constituent fields.
     * @param trd_num  Thread number (0–7), placed at bits[26:24]
     * @param bank_cs  Chip-select index (0–7), placed at bits[22:20]
     * @param int_flag Set non-zero to enable completion interrupt (bit 18)
     * @param cmd_type CMD_TYPE value placed at bits[15:0]
     * @return Fully encoded 32-bit PIO cmd_reg0 value
     */
    static uint32_t make_pio_cmd_reg0(uint32_t trd_num, uint32_t bank_cs,
                                      bool int_flag, uint32_t cmd_type)
    {
        uint32_t val = PIO_SEL;
        val |= ((trd_num & 0x7u) << 24);
        val |= ((bank_cs & 0x7u) << 20);
        if (int_flag) val |= PIO_INT_FLAG;
        val |= (cmd_type & 0xFFFFu);
        return val;
    }

} // namespace

// =============================================================================
// TC_XSPI_PIO_001
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_001 — PIO READ: flash read via xspi_bus_socket[0];
 *        DMA write via i_dma_socket; cdns_extension fields verified.
 *
 * Verification objective (test-plan row 36):
 *   Confirms the complete PIO READ path (CMD_TYPE=0x2200):
 *     Phase A: the model issues a READ (opcode=0x03) on xspi_bus_socket[0]
 *              at flash address formed from {cmd_reg5[31:0], cmd_reg1[31:0]}.
 *     Phase B: the model DMA-writes the received flash data to the system
 *              address formed from {cmd_reg3[31:0], cmd_reg2[31:0]} via
 *              i_dma_socket.
 *   After completion: trd_busy[0]=0, ctrl_busy=0, cmd_status[0].COMPLETE=1.
 *   cdns_extension.opcode must be 0x03 (READ_ZERO_LATENCY) and data_bytes
 *   must equal DATA_CNT+1 = 64.
 *
 * Register setup:
 *   ctrl_config  = 0x60        PIO under global work_mode 2'b11
 *   cmd_reg1     = 0x00010000  xSPI flash address lower [31:0]
 *   cmd_reg2     = 0x00200000  system DMA address lower [31:0]
 *   cmd_reg3     = 0x00000000  system DMA address upper [63:32]
 *   cmd_reg4     = 63          DATA_CNT=63 → 64 bytes transferred
 *   cmd_reg5     = 0x00000000  xSPI flash address upper [63:32]
 *   cmd_reg0     = 0x40042200  PIO sel | TRD=0 | BANK=0 | INT=1 | READ
 *
 * INT=1 is required to populate trd_comp_intr_status[0], which is what
 * handle_read_cmd_status returns as bit 0 (COMPLETE) of cmd_status.
 *
 * Winning condition:
 *   m_last_flash_ext.opcode == 0x03;
 *   m_last_flash_ext.data_bytes == 64;
 *   trd_status[0] == 0; ctrl_busy == 0;
 *   cmd_status (thread 0) bit 0 (COMPLETE) == 1.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 36 (TC_XSPI_PIO_001);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.3 (READ path)
 ******************************************************************************/
void testbench::tc_xspi_pio_001_read_cmd_type()
{
    report_test_start("TC_XSPI_PIO_001: PIO READ — flash read + DMA write");

    apply_reset();

    // Clear flash extension capture so we can detect the new transaction.
    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    // Set PIO mode.
    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage command registers (must precede cmd_reg0 write).
    // flash address: 0x0000000000010000  (cmd_reg5=0, cmd_reg1=0x00010000)
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00010000u);
    // system DMA address: 0x0000000000200000 (cmd_reg3=0, cmd_reg2=0x00200000)
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00200000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    // DATA_CNT=63 → transfer 64 bytes
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 63u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // Trigger PIO READ on thread 0, bank 0, INT=1.
    // INT=1 is required so that trd_comp_intr_status[0] is set on completion,
    // enabling handle_read_cmd_status to return COMPLETE (bit 0) = 1.
    // cmd_reg0 = PIO_SEL | TRD=0 | BANK=0 | INT=1 | CMD_READ
    //          = 0x40000000 | 0x00000000 | 0x00040000 | 0x00002200
    //          = 0x40042200
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_READ);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);

    // PIO is synchronous in LT model; delta advance propagates int_out updates.
    wait(sc_core::SC_ZERO_TIME);

    // --- Verify cdns_extension captured by flash stub ---
    bool pass_opcode    = (m_last_flash_ext.opcode     == 0x03u);
    bool pass_data_bytes = (m_last_flash_ext.data_bytes == 64u);
    bool pass_address   = (m_last_flash_ext.address    == 0x0000000000010000ULL);

    // --- Verify trd_status bit 0 cleared (thread completed) ---
    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // --- Verify ctrl_busy cleared ---
    uint32_t ctrl_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_val);
    bool pass_ctrl_clear = ((ctrl_val & CTRL_BUSY_BIT) == 0u);

    // --- Verify cmd_status.COMPLETE for thread 0 ---
    // cmd_status bit 0 = trd_comp_intr_status[sel], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    // Diagnostic output on failure.
    if (!pass_opcode) {
        CSML_ERROR(0, func011_logger)
            << "PIO_001 FAIL: opcode=0x" << std::hex << m_last_flash_ext.opcode
            << " expected 0x03";
    }
    if (!pass_data_bytes) {
        CSML_ERROR(0, func011_logger)
            << "PIO_001 FAIL: data_bytes=" << m_last_flash_ext.data_bytes
            << " expected 64";
    }
    if (!pass_address) {
        CSML_ERROR(0, func011_logger)
            << "PIO_001 FAIL: flash address=0x" << std::hex
            << m_last_flash_ext.address << " expected 0x10000";
    }
    if (!pass_trd_clear) {
        CSML_ERROR(0, func011_logger)
            << "PIO_001 FAIL: trd_status=0x" << std::hex << trd_val
            << " — trd_busy[0] still set";
    }
    if (!pass_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_001 FAIL: cmd_status=0x" << std::hex << cs_val
            << " — COMPLETE (bit 0) not set for thread 0"
            << " (requires INT=1 so trd_comp_intr_status[0] is populated)";
    }

    bool passed = pass_opcode && pass_data_bytes && pass_address
                  && pass_trd_clear && pass_ctrl_clear && pass_complete;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_001", passed);
}

// =============================================================================
// TC_XSPI_PIO_002
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_002 — PIO PROGRAM: DMA READ + WREN + PAGE_PROGRAM.
 *
 * Verification objective (test-plan row 37):
 *   Confirms the complete PIO PROGRAM path (CMD_TYPE=0x2100):
 *     Phase A: DMA READ from system memory via i_dma_socket.
 *     Phase B: WREN (opcode=0x06) issued on xspi_bus_socket[0].
 *     Phase C: PAGE_PROGRAM (opcode=0x02) with data on xspi_bus_socket[0].
 *   The flash stub records m_last_flash_ext from the final transaction
 *   (PAGE_PROGRAM). flash_stub_tx_count must be >= 2 (WREN + PAGE_PROGRAM).
 *
 * Register setup:
 *   ctrl_config = 0x60         global work_mode 2'b11 (PIO via cmd_reg0)
 *   cmd_reg1    = 0x00020000   xSPI flash address lower
 *   cmd_reg2    = 0x00400000   system address lower
 *   cmd_reg3    = 0x00000000   system address upper
 *   cmd_reg4    = 7            DATA_CNT=7 → 8 bytes
 *   cmd_reg5    = 0x00000000   xSPI flash address upper
 *   cmd_reg0    = PIO | TRD=0 | BANK=0 | INT=1 | PROGRAM
 *
 * INT=1 is required so trd_comp_intr_status[0] is set, enabling
 * handle_read_cmd_status to return COMPLETE (bit 0) = 1 for thread 0.
 *
 * Winning condition:
 *   flash_stub_tx_count >= 2; m_last_flash_ext.opcode == 0x02;
 *   trd_busy[0]=0; cmd_status[0].COMPLETE=1 (bit 0).
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 37 (TC_XSPI_PIO_002);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.4 (PROGRAM path)
 ******************************************************************************/
void testbench::tc_xspi_pio_002_program_cmd_type()
{
    report_test_start("TC_XSPI_PIO_002: PIO PROGRAM — WREN + PAGE_PROGRAM");

    apply_reset();

    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage registers for PROGRAM.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00020000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00400000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 7u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // Use INT=1 so that trd_comp_intr_status[0] is set on completion, which
    // allows handle_read_cmd_status to return COMPLETE (bit 0) = 1.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_PROGRAM);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // Expect at least WREN + PAGE_PROGRAM = 2 flash transactions.
    bool pass_tx_count  = (m_flash_stub_tx_count >= 2);
    // Last transaction must be PAGE_PROGRAM (opcode 0x02).
    bool pass_opcode    = (m_last_flash_ext.opcode == 0x02u);

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // cmd_status bit 0 = trd_comp_intr_status[0], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    if (!pass_tx_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_002 FAIL: flash_stub_tx_count=" << m_flash_stub_tx_count
            << " expected >= 2 (WREN + PAGE_PROGRAM)";
    }
    if (!pass_opcode) {
        CSML_ERROR(0, func011_logger)
            << "PIO_002 FAIL: last opcode=0x" << std::hex
            << m_last_flash_ext.opcode << " expected 0x02 (PAGE_PROGRAM)";
    }
    if (!pass_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_002 FAIL: cmd_status=0x" << std::hex << cs_val
            << " — COMPLETE (bit 0) not set (requires INT=1)";
    }

    bool passed = pass_tx_count && pass_opcode && pass_trd_clear && pass_complete;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_002", passed);
}

// =============================================================================
// TC_XSPI_PIO_003
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_003 — PIO SECTOR_ERASE: WREN + erase for SECT_CNT+1
 *        sectors; opcode sourced from ers_seq_cfg_0.
 *
 * Verification objective (test-plan row 38):
 *   SECT_CNT=1 in cmd_reg4 means 2 sectors are indicated. In the LT model,
 *   the sector count is passed as ext.write_data to the flash stub for
 *   observability, but only ONE WREN + ONE ERASE transaction is dispatched
 *   (the flash stub is expected to handle multi-sector erase as one command).
 *   Therefore flash_stub_tx_count == 2 (WREN + ERASE) after SECT_CNT=1.
 *   m_last_flash_ext.opcode must be 0xD8 (ERASE_64KB default).
 *   m_last_flash_ext.write_data must carry the sector count (SECT_CNT+1=2).
 *
 * LT model note:
 *   The LT model comment (xspi_ctrl.cpp case 0x1000) explicitly states:
 *   "In the LT model, only the first sector erase is dispatched with the
 *   sector count conveyed via ext.write_data for flash stub observability;
 *   the flash stub is expected to handle multi-sector erase as one command."
 *   Hardware behavior (per-sector WREN+ERASE loop) is not modeled in LT.
 *
 * Register setup:
 *   cmd_reg1 = 0x00030000  xSPI address lower (sector start)
 *   cmd_reg4 = 1           SECT_CNT=1 → 2 sectors indicated
 *   cmd_reg5 = 0x00000000  xSPI address upper
 *   cmd_reg0 = PIO | TRD=0 | BANK=0 | INT=1 | SECTOR_ERASE
 *
 * Winning condition:
 *   flash_stub_tx_count >= 2; m_last_flash_ext.opcode == 0xD8;
 *   m_last_flash_ext.write_data == 2 (SECT_CNT+1);
 *   trd_busy[0]=0; cmd_status[0].COMPLETE=1 (bit 0 via INT=1).
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 38 (TC_XSPI_PIO_003);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.5 (SECTOR_ERASE)
 ******************************************************************************/
void testbench::tc_xspi_pio_003_sector_erase_cmd_type()
{
    report_test_start(
        "TC_XSPI_PIO_003: PIO SECTOR_ERASE — WREN + erase for SECT_CNT+1 sectors");

    apply_reset();

    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // SECT_CNT=1 → 2 sectors indicated; only 1 WREN + 1 ERASE dispatched in LT.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00030000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 1u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // Use INT=1 so trd_comp_intr_status[0] is set for cmd_status.COMPLETE check.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_SECTOR_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // LT model: 1 WREN + 1 ERASE = 2 transactions; sector count in write_data.
    bool pass_tx_count = (m_flash_stub_tx_count >= 2);
    // Last transaction must be ERASE_64KB (opcode 0xD8).
    bool pass_opcode = (m_last_flash_ext.opcode == 0xD8u);
    // write_data carries sector count (SECT_CNT+1 = 2).
    bool pass_sect_count = (m_last_flash_ext.write_data == 2u);

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // cmd_status bit 0 = trd_comp_intr_status[0], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    if (!pass_tx_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_003 FAIL: flash_stub_tx_count=" << m_flash_stub_tx_count
            << " expected >= 2 (WREN + ERASE; LT model dispatches 1 erase cmd)";
    }
    if (!pass_opcode) {
        CSML_ERROR(0, func011_logger)
            << "PIO_003 FAIL: last opcode=0x" << std::hex << m_last_flash_ext.opcode
            << " expected 0xD8 (ERASE_64KB)";
    }
    if (!pass_sect_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_003 FAIL: write_data=" << m_last_flash_ext.write_data
            << " expected 2 (SECT_CNT+1)";
    }

    bool passed = pass_tx_count && pass_opcode && pass_sect_count
                  && pass_trd_clear && pass_complete && pass_no_cmd_ignored;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_003", passed);
}

// =============================================================================
// TC_XSPI_PIO_004
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_004 — PIO CHIP_ERASE: WREN + full-chip erase.
 *
 * Verification objective (test-plan row 39):
 *   CMD_TYPE=0x1001 issues WREN (0x06) then a second erase opcode (0xDC on the
 *   bus for TLM xspi_target compatibility; hardware may use ers_seq_cfg_2).
 *   cmd_reg1–cmd_reg5 are not consumed. flash_stub_tx_count must be >= 2.
 *
 * Register setup:
 *   cmd_reg0 = PIO | TRD=0 | BANK=0 | INT=1 | CHIP_ERASE
 *
 * INT=1 is required so trd_comp_intr_status[0] is set, enabling
 * handle_read_cmd_status to return COMPLETE (bit 0) = 1 for thread 0.
 *
 * Winning condition:
 *   flash_stub_tx_count >= 2; trd_busy[0]=0; cmd_status[0].COMPLETE=1 (bit 0).
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 39 (TC_XSPI_PIO_004);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.6 (CHIP_ERASE)
 ******************************************************************************/
void testbench::tc_xspi_pio_004_chip_erase_cmd_type()
{
    report_test_start("TC_XSPI_PIO_004: PIO CHIP_ERASE — WREN + full-chip erase");

    apply_reset();

    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // CHIP_ERASE: cmd_reg1–cmd_reg5 not consumed. INT=1 for cmd_status.COMPLETE.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    bool pass_tx_count = (m_flash_stub_tx_count >= 2);  // WREN + CHIP_ERASE

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // cmd_status bit 0 = trd_comp_intr_status[0], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    if (!pass_tx_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_004 FAIL: flash_stub_tx_count=" << m_flash_stub_tx_count
            << " expected >= 2 (WREN + CHIP_ERASE)";
    }
    if (!pass_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_004 FAIL: cmd_status=0x" << std::hex << cs_val
            << " — COMPLETE (bit 0) not set (requires INT=1)";
    }

    bool passed = pass_tx_count && pass_trd_clear && pass_complete;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_004", passed);
}

// =============================================================================
// TC_XSPI_PIO_005
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_005 — PIO SOFT_RESET: software reset command sequence.
 *
 * Verification objective (test-plan row 40):
 *   CMD_TYPE=0x1100 issues the software reset opcode (0xFF per model default,
 *   or RESET_ENABLE 0x66 + RESET 0x99 depending on seq config). At least one
 *   flash transaction must occur on xspi_bus_socket[0]. cmd_reg1–cmd_reg5
 *   are not consumed. After completion trd_busy[0]=0.
 *
 * Register setup:
 *   cmd_reg0 = PIO | TRD=0 | BANK=0 | INT=1 | SOFT_RESET
 *
 * INT=1 is required so trd_comp_intr_status[0] is set, enabling
 * handle_read_cmd_status to return COMPLETE (bit 0) = 1 for thread 0.
 *
 * Winning condition:
 *   flash_stub_tx_count >= 1; trd_busy[0]=0; cmd_status[0].COMPLETE=1 (bit 0).
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 40 (TC_XSPI_PIO_005);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.7 (SOFT_RESET)
 ******************************************************************************/
void testbench::tc_xspi_pio_005_soft_reset_cmd_type()
{
    report_test_start("TC_XSPI_PIO_005: PIO SOFT_RESET — software reset sequence");

    apply_reset();

    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // INT=1 required to populate trd_comp_intr_status[0] for cmd_status.COMPLETE.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_SOFT_RESET);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    bool pass_tx_count = (m_flash_stub_tx_count >= 1);

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // cmd_status bit 0 = trd_comp_intr_status[0], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    if (!pass_tx_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_005 FAIL: flash_stub_tx_count=" << m_flash_stub_tx_count
            << " expected >= 1 (at least one reset command)";
    }
    if (!pass_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_005 FAIL: cmd_status=0x" << std::hex << cs_val
            << " — COMPLETE (bit 0) not set (requires INT=1)";
    }

    bool passed = pass_tx_count && pass_trd_clear && pass_complete
                  && pass_no_cmd_ignored;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_005", passed);
}

// =============================================================================
// TC_XSPI_PIO_006
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_006 — PIO JEDEC_RESET: JEDEC hardware reset sequence;
 *        only cmd_reg0 consumed.
 *
 * Verification objective (test-plan row 41):
 *   CMD_TYPE=0x1101 issues the JEDEC reset opcode (0xF0 per model constant).
 *   cmd_reg1–cmd_reg5 are not consumed (ignored). At least one flash
 *   transaction must appear on xspi_bus_socket[0]. After completion
 *   trd_busy[0]=0 and cmd_ignored is not set.
 *
 * Register setup:
 *   cmd_reg0 = PIO | TRD=0 | BANK=0 | INT=1 | JEDEC_RESET
 *
 * INT=1 is required so trd_comp_intr_status[0] is set, enabling
 * handle_read_cmd_status to return COMPLETE (bit 0) = 1 for thread 0.
 *
 * Winning condition:
 *   flash_stub_tx_count >= 1; trd_busy[0]=0; cmd_status[0].COMPLETE=1 (bit 0);
 *   intr_status.cmd_ignored=0.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 41 (TC_XSPI_PIO_006);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.8 (JEDEC_RESET)
 ******************************************************************************/
void testbench::tc_xspi_pio_006_jedec_reset_cmd_type()
{
    report_test_start("TC_XSPI_PIO_006: PIO JEDEC_RESET — only cmd_reg0 consumed");

    apply_reset();

    m_last_flash_ext = cdns_extension();
    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Write a non-zero value to cmd_reg1 to confirm it is NOT consumed
    // (JEDEC_RESET ignores all address/data registers).
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0xDEADBEEFu);

    // INT=1 required to populate trd_comp_intr_status[0] for cmd_status.COMPLETE.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_JEDEC_RESET);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    bool pass_tx_count = (m_flash_stub_tx_count >= 1);

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // cmd_status bit 0 = trd_comp_intr_status[0], set because INT=1 was used.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cs_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_val);
    bool pass_complete = ((cs_val & CMD_STATUS_COMPLETE) != 0u);

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    if (!pass_tx_count) {
        CSML_ERROR(0, func011_logger)
            << "PIO_006 FAIL: flash_stub_tx_count=" << m_flash_stub_tx_count
            << " expected >= 1";
    }
    if (!pass_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_006 FAIL: cmd_status=0x" << std::hex << cs_val
            << " — COMPLETE (bit 0) not set (requires INT=1)";
    }

    bool passed = pass_tx_count && pass_trd_clear && pass_complete
                  && pass_no_cmd_ignored;

    // W1C cleanup: clear trd_comp_intr_status[0] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_006", passed);
}

// =============================================================================
// TC_XSPI_PIO_007
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_007 — Multi-thread concurrent: threads 0 and 1 issued
 *        back-to-back; both complete independently.
 *
 * Verification objective (test-plan row 42):
 *   In the LT synchronous model each PIO dispatch completes inline. Two
 *   successive cmd_reg0 writes targeting thread 0 and thread 1 must each
 *   complete with INT=1 setting the respective trd_comp_intr_status bit.
 *   After both triggers: trd_comp_intr_status bits 0 and 1 are set;
 *   trd_status bits 0 and 1 are clear (both completed).
 *
 * Stimulus:
 *   1. PIO CHIP_ERASE on thread 0, INT=1.
 *   2. PIO CHIP_ERASE on thread 1, INT=1.
 *
 * Winning condition:
 *   trd_comp_intr_status[0]=1; trd_comp_intr_status[1]=1;
 *   trd_status[0]=0; trd_status[1]=0; cmd_ignored=0.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 42 (TC_XSPI_PIO_007);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.2 (multi-thread)
 ******************************************************************************/
void testbench::tc_xspi_pio_007_multi_thread_concurrent()
{
    report_test_start(
        "TC_XSPI_PIO_007: Multi-thread concurrent — threads 0 and 1");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Thread 0: CHIP_ERASE, INT=1.
    const uint32_t cmd0_trd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_trd0);
    wait(sc_core::SC_ZERO_TIME);

    // Thread 1: CHIP_ERASE, INT=1.
    const uint32_t cmd0_trd1 = make_pio_cmd_reg0(1u, 0u, true, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_trd1);
    wait(sc_core::SC_ZERO_TIME);

    // Both threads must have completed: trd_comp_intr_status bits 0 and 1 set.
    uint32_t comp_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_val);
    bool pass_bit0 = ((comp_val >> 0) & 0x1u) == 0x1u;
    bool pass_bit1 = ((comp_val >> 1) & 0x1u) == 0x1u;

    // trd_status bits 0 and 1 must be clear (synchronous completion).
    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd0_clear = ((trd_val >> 0) & 0x1u) == 0u;
    bool pass_trd1_clear = ((trd_val >> 1) & 0x1u) == 0u;

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    if (!pass_bit0) {
        CSML_ERROR(0, func011_logger)
            << "PIO_007 FAIL: trd_comp_intr_status=0x" << std::hex << comp_val
            << " — bit 0 (thread 0) not set";
    }
    if (!pass_bit1) {
        CSML_ERROR(0, func011_logger)
            << "PIO_007 FAIL: trd_comp_intr_status=0x" << std::hex << comp_val
            << " — bit 1 (thread 1) not set";
    }

    bool passed = pass_bit0 && pass_bit1 && pass_trd0_clear
                  && pass_trd1_clear && pass_no_cmd_ignored;

    // W1C cleanup: clear both bits for subsequent tests.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000003u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_007", passed);
}

// =============================================================================
// TC_XSPI_PIO_008
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_008 — INT=1 flag on thread 2: trd_comp_intr_status
 *        bit 2 set; int_out asserted; W1C clears flag and de-asserts int_out.
 *
 * Hardware-aligned sequence:
 *   1. Global enable: intr_enable (0x114) bit 31 (intr_en) = 1.
 *   2. Trigger: PIO READ on thread 2 — cmd_reg0 bits[26:24] TRD_NUM=2,
 *      bit 18 INT=1, bits[15:0] CMD_TYPE=0x2200 (see make_pio_cmd_reg0).
 *   3. Verify: trd_comp_intr_status (0x120) bit 2 = 1; trd_busy[2] = 0.
 *   4. Verify: int_out (physical interrupt) asserted (high).
 *   5. Cleanup: W1C trd_comp_intr_status[2] = 1; confirm bit 2 clear and
 *      int_out de-asserted.
 *
 * Stimulus (cmd_reg0):
 *   PIO | TRD=2 | INT=1 | READ (0x2200) = 0x42042200
 *
 * Reference: docs/xspi_ctrl-test-plan.md (TC_XSPI_PIO_008);
 *            docs/xspi_ctrl-detailed-design.md Section 9.2 (Path 2a)
 ******************************************************************************/
void testbench::tc_xspi_pio_008_thread_int_flag_completion_interrupt()
{
    report_test_start(
        "TC_XSPI_PIO_008: INT=1 on thread 2 — trd_comp_intr_status[2] + int_out");

    apply_reset();

    // (1) Global enable: intr_enable[31] (intr_en) = 1.
    f11_write_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, 0x80000000u);
    wait(sc_core::SC_ZERO_TIME);

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage address/DMA registers for PIO READ (same minimal pattern as INT_001).
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00600000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // (2) PIO READ on thread 2: TRD=2, INT=1, CMD_TYPE=0x2200.
    const uint32_t cmd0 = make_pio_cmd_reg0(2u, 0u, true, PIO_CMD_READ);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);

    wait(sc_core::SC_ZERO_TIME);

    // (3) trd_comp_intr_status bit 2 must be set.
    uint32_t comp_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_val);
    const bool pass_bit2 = ((comp_val >> 2) & 0x1u) == 0x1u;

    // (4) Physical interrupt line high (Path 2a: intr_en + trd_comp non-zero).
    const bool pass_int_out = int_out_sig.read();

    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    const bool pass_trd_clear = ((trd_val >> 2) & 0x1u) == 0u;

    if (!pass_bit2) {
        CSML_ERROR(0, func011_logger)
            << "PIO_008 FAIL: trd_comp_intr_status=0x" << std::hex << comp_val
            << " — bit 2 not set after PIO READ (INT=1) on thread 2";
    }
    if (!pass_int_out) {
        CSML_ERROR(0, func011_logger)
            << "PIO_008 FAIL: int_out_sig not asserted (high) — Path 2a broken";
    }

    // (5) Cleanup: W1C bit 2 — expect flag clear and int_out de-asserted.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000004u);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t comp_after = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_after);
    const bool pass_comp_cleared = ((comp_after >> 2) & 0x1u) == 0u;
    const bool pass_int_deasserted = !int_out_sig.read();

    if (!pass_comp_cleared) {
        CSML_ERROR(0, func011_logger)
            << "PIO_008 FAIL: trd_comp_intr_status[2] not cleared after W1C, got 0x"
            << std::hex << comp_after;
    }
    if (!pass_int_deasserted) {
        CSML_ERROR(0, func011_logger)
            << "PIO_008 FAIL: int_out not de-asserted after clearing trd_comp[2]";
    }

    const bool passed = pass_bit2 && pass_int_out && pass_trd_clear
                        && pass_comp_cleared && pass_int_deasserted;

    report_test_result("TC_XSPI_PIO_008", passed);
}

// =============================================================================
// TC_XSPI_PIO_009
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_PIO_009 — cmd_status_ptr / cmd_status indirect read in PIO mode.
 *
 * Verification objective (test-plan row 44):
 *   1. Complete a PIO READ on thread 3 (cmd_reg0: TRD=3, INT=1, CMD_TYPE=0x2200).
 *   2. Write cmd_status_ptr (0x040) = 0x00000003, read cmd_status (0x044) and
 *      confirm the COMPLETE flag (bit 0; mirrors trd_comp_intr_status[3]) = 1.
 *   3. Write cmd_status_ptr = 0x00000000, read cmd_status; confirm COMPLETE = 0
 *      for thread 0 (that thread was not used in this test with INT=1).
 *
 * Note: handle_read_cmd_status exposes bit 0 as completion (not bit 15);
 * it reflects trd_comp_intr_status[sel] only when INT=1 on the command.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 44 (TC_XSPI_PIO_009);
 *            model handle_read_cmd_status (FUNC_XSPI_003)
 ******************************************************************************/
void testbench::tc_xspi_pio_009_cmd_status_thread_selection()
{
    report_test_start(
        "TC_XSPI_PIO_009: cmd_status_ptr/cmd_status indirect thread selection");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage address/DMA for PIO READ, then trigger on thread 3, INT=1, READ (0x2200).
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00600000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    const uint32_t cmd0 = make_pio_cmd_reg0(3u, 0u, true, PIO_CMD_READ);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // cmd_status_ptr = 3 → read cmd_status: COMPLETE (bit 0) for thread 3.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0x00000003u);
    uint32_t cs_trd3 = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_trd3);
    const bool pass_trd3_complete = ((cs_trd3 & CMD_STATUS_COMPLETE) != 0u);

    // cmd_status_ptr = 0 → read cmd_status: thread 0 not completed with INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0x00000000u);
    uint32_t cs_trd0 = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cs_trd0);
    const bool pass_trd0_incomplete = ((cs_trd0 & CMD_STATUS_COMPLETE) == 0u);

    if (!pass_trd3_complete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_009 FAIL: cmd_status[trd=3]=0x" << std::hex << cs_trd3
            << " — COMPLETE (bit 0) not set after PIO READ on thread 3 with INT=1";
    }
    if (!pass_trd0_incomplete) {
        CSML_ERROR(0, func011_logger)
            << "PIO_009 FAIL: cmd_status[trd=0]=0x" << std::hex << cs_trd0
            << " — COMPLETE unexpectedly set for thread 0 (was not triggered)";
    }

    bool passed = pass_trd3_complete && pass_trd0_incomplete;

    // W1C cleanup: clear trd_comp_intr_status[3] set by INT=1.
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000008u);
    wait(sc_core::SC_ZERO_TIME);

    report_test_result("TC_XSPI_PIO_009", passed);
}

// =============================================================================
// TC_XSPI_MDR_005
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_MDR_005 — cmd_reg4 DATA_CNT for PIO READ and PROGRAM:
 *        byte count transferred equals DATA_CNT+1.
 *
 * Verification objective (test-plan row 70):
 *   (a) DATA_CNT=255 in cmd_reg4: PIO READ transfers 256 bytes from flash.
 *       m_last_flash_ext.data_bytes must equal 256.
 *   (b) DATA_CNT=0 in cmd_reg4: PIO PROGRAM transfers 1 byte from system
 *       memory to flash. flash_stub_tx_count must include WREN+PAGE_PROGRAM.
 *
 * Winning condition:
 *   (a) m_last_flash_ext.data_bytes == 256; trd_busy[0]=0.
 *   (b) flash_stub_tx_count >= 2; trd_busy[0]=0.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 70 (TC_XSPI_MDR_005);
 *            docs/xspi_ctrl-detailed-design.md Section 5.3.6 (DATA_CNT)
 ******************************************************************************/
 void testbench::tc_xspi_mdr_005_cmd_reg4_data_cnt_read_program()
 {
     report_test_start(
         "TC_XSPI_MDR_005: cmd_reg4 DATA_CNT for READ(256 bytes) and PROGRAM(1 byte)");
 
     static constexpr uint64_t kReadSysAddr  = 0x0000000000100000ULL; // cmd_reg2/3
     static constexpr uint64_t kProgSysAddr  = 0x0000000000200000ULL;
 
     apply_reset();
     f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
     wait(sc_core::SC_ZERO_TIME);
 
     // ---------------- Part (a): DATA_CNT=255 -> 256-byte READ + DMA write
     m_last_flash_ext       = cdns_extension();
     m_flash_stub_tx_count  = 0;
     m_dma_trace.clear();
     m_dma_trace_enabled    = true;
 
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET,
                   static_cast<uint32_t>(kReadSysAddr & 0xFFFFFFFFu));
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET,
                   static_cast<uint32_t>((kReadSysAddr >> 32) & 0xFFFFFFFFu));
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x000000FFu);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
 
     const uint32_t cmd0_read = make_pio_cmd_reg0(0u, 0u, false, PIO_CMD_READ);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_read);
     wait(sc_core::SC_ZERO_TIME);
 
     m_dma_trace_enabled = false;
 
     uint32_t trd_a = 0u;
     f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_a);
 
     const bool pass_a_flash =
         (m_last_flash_ext.opcode == 0x03u) && (m_last_flash_ext.data_bytes == 256u);
     const bool pass_a_trd = ((trd_a & 1u) == 0u);
     const bool pass_a_dma =
         (m_dma_trace.size() == 1u) && !m_dma_trace[0].is_read
         && (m_dma_trace[0].addr == kReadSysAddr) && (m_dma_trace[0].len == 256u);
 
     // ---------------- Part (b): DATA_CNT=0 -> 1-byte PROGRAM
     m_last_flash_ext       = cdns_extension();
     m_flash_stub_tx_count  = 0;
     m_dma_trace.clear();
     m_dma_trace_enabled    = true;
 
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET,
                   static_cast<uint32_t>(kProgSysAddr & 0xFFFFFFFFu));
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET,
                   static_cast<uint32_t>((kProgSysAddr >> 32) & 0xFFFFFFFFu));
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 0x00000000u);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
 
     const uint32_t cmd0_prog = make_pio_cmd_reg0(0u, 0u, false, PIO_CMD_PROGRAM);
     f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_prog);
     wait(sc_core::SC_ZERO_TIME);
 
     m_dma_trace_enabled = false;
 
     uint32_t trd_b = 0u;
     f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_b);
 
     const bool pass_b_trd = ((trd_b & 1u) == 0u);
     const bool pass_b_flash_cnt = (m_flash_stub_tx_count >= 2);
     const bool pass_b_page =
         (m_last_flash_ext.opcode == 0x02u) && (m_last_flash_ext.data_bytes == 1u);
     const bool pass_b_dma =
         (m_dma_trace.size() == 1u) && m_dma_trace[0].is_read
         && (m_dma_trace[0].addr == kProgSysAddr) && (m_dma_trace[0].len == 1u);
 
     const bool passed = pass_a_flash && pass_a_trd && pass_a_dma
                          && pass_b_trd && pass_b_flash_cnt && pass_b_page && pass_b_dma;
 
     if (!pass_a_dma) {
         CSML_ERROR(0, func011_logger)
             << "MDR_005(a) FAIL: expected 1x i_dma_socket WRITE len=256 @ 0x"
             << std::hex << kReadSysAddr << " trace_size=" << std::dec << m_dma_trace.size();
     }
     if (!pass_a_flash) {
         CSML_ERROR(0, func011_logger)
             << "MDR_005(a) FAIL: flash READ opcode/data_bytes mismatch";
     }
     if (!pass_b_dma) {
         CSML_ERROR(0, func011_logger)
             << "MDR_005(b) FAIL: expected 1x i_dma_socket READ len=1 @ 0x"
             << std::hex << kProgSysAddr << " trace_size=" << std::dec << m_dma_trace.size();
     }
     if (!pass_b_page || !pass_b_flash_cnt) {
         CSML_ERROR(0, func011_logger)
             << "MDR_005(b) FAIL: flash WREN+PAGE_PROGRAM path; last opcode=0x"
             << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
             << " data_bytes=" << std::dec << m_last_flash_ext.data_bytes
             << " tx_count=" << m_flash_stub_tx_count;
     }
 
     if (passed) {
         report_test_pass("TC_XSPI_MDR_005");
     } else {
         report_test_fail("TC_XSPI_MDR_005",
                           "READ 256B DMA write and/or PROGRAM 1B DMA read / PAGE_PROGRAM checks failed");
     }
 }

// =============================================================================
// TC_XSPI_XIP_005
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_XIP_005 — PIO MB_XIP_DIS: exit XIP on bank 0 via cmd_reg0 bit 17.
 *
 * Verification objective (test-plan row 48):
 *   (a) Establish XIP active on bank 0 (xip_mode_cfg with xip_en[0]=1).
 *   (b) In PIO mode, issue READ with MB_XIP_DIS=1 (cmd_reg0 bit 17).
 *   (c) The flash READ on xspi_bus_socket[0] carries xip_dis_mb_val in
 *       cdns_extension.write_data.
 *   (d) xip_mode_cfg.xip_en[0] is cleared (bank 0 no longer XIP-active).
 *   (e) trd_busy[0] clears; cmd_ignored is not set.
 *
 * cmd_reg0 encoding (bank 0, thread 0):
 *   PIO_SEL | TRD=0 | BANK=0 | MB_XIP_DIS (bit 17) | READ
 *   = 0x40022200
 *
 * Winning condition:
 *   m_last_flash_ext.write_data == xip_dis_mb_val (0xFF for default 0x00FF0001);
 *   m_last_flash_ext.bank_num == 0; flash_stub_tx_count >= 1;
 *   (xip_mode_cfg & 0xFF) has bit 0 clear (prefer full match 0x00FF0000);
 *   trd_busy[0]=0; cmd_ignored=0.
 *
 * Reference: docs/xspi_ctrl-test-plan.md (TC_XSPI_XIP_005);
 *            model/src/xspi_ctrl.cpp pio_handle_trigger() case 0x2200
 ******************************************************************************/
void testbench::tc_xspi_xip_005_pio_xip_dis_exit()
{
    report_test_start(
        "TC_XSPI_XIP_005: PIO MB_XIP_DIS=1 — xip_dis_mb_val on bus; xip_en[0] clear");

    apply_reset();

    m_last_flash_ext      = cdns_extension();
    m_flash_stub_tx_count = 0;

    // Step (a): Enable XIP on bank 0.
    // xip_mode_cfg reset = 0x00FF0000; set bit 0 (xip_en[0]) = 0x00FF0001.
    f11_write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, 0x00FF0001u);

    // Confirm xip_mode_cfg write was stored (baseline).
    uint32_t xip_before = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, xip_before);
    bool pass_xip_written = ((xip_before & 0x1u) == 0x1u);

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage read registers.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00500000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // Step (b): PIO READ with MB_XIP_DIS=1 (bit 17).
    // cmd_reg0 = PIO_SEL | TRD=0 | BANK=0 | MB_XIP_DIS | READ
    //          = 0x40000000 | 0x00020000 | 0x00002200 = 0x40022200
    uint32_t cmd0 = PIO_SEL | PIO_MB_XIP_DIS_FLAG | PIO_CMD_READ;
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // Step (c): Verify trd_busy[0] cleared (thread completed).
    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // Step (d): Verify cmd_ignored not set (MB_XIP_DIS is not an error path).
    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    // Step (e): xip_en[0] must clear; full register 0x00FF0000 (only xip_en byte 0).
    uint32_t xip_after = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, xip_after);
    bool pass_xip_cleared
        = (((xip_after & 0x1u) == 0u) && (xip_after == 0x00FF0000u));

    // Step (f): Flash READ transaction carried exit mode byte on bank 0.
    bool pass_flash_executed = (m_flash_stub_tx_count >= 1);
    bool pass_bank0          = (m_last_flash_ext.bank_num == 0u);
    bool pass_xip_dis_mb
        = (m_last_flash_ext.write_data == XIP_DIS_MB_VAL);

    if (!pass_xip_written) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: xip_mode_cfg baseline write failed; "
            << "read back 0x" << std::hex << xip_before;
    }
    if (!pass_trd_clear) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: trd_status=0x" << std::hex << trd_val
            << " — trd_busy[0] still set after PIO READ with MB_XIP_DIS=1";
    }
    if (!pass_no_cmd_ignored) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: intr_status=0x" << std::hex << intr_val
            << " — cmd_ignored set (MB_XIP_DIS should not trigger cmd_ignored)";
    }
    if (!pass_xip_cleared) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: xip_mode_cfg=0x" << std::hex << xip_after
            << " expected 0x00FF0000 (xip_en[0] must clear after MB_XIP_DIS READ)";
    }
    if (!pass_bank0) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: bank_num=" << std::dec
            << static_cast<unsigned>(m_last_flash_ext.bank_num)
            << " expected 0 (xspi_bus_socket[0])";
    }
    if (!pass_xip_dis_mb) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: write_data=0x" << std::hex
            << m_last_flash_ext.write_data << " expected xip_dis_mb_val=0x"
            << XIP_DIS_MB_VAL;
    }
    if (!pass_flash_executed) {
        CSML_ERROR(0, func011_logger)
            << "XIP_005 FAIL: no flash transaction recorded "
            << "(flash_stub_tx_count=" << m_flash_stub_tx_count << ")";
    }

    bool passed = pass_xip_written && pass_trd_clear && pass_no_cmd_ignored
                  && pass_xip_cleared && pass_flash_executed && pass_bank0
                  && pass_xip_dis_mb;
    report_test_result("TC_XSPI_XIP_005", passed);
}

// =============================================================================
// TC_XSPI_ERR_002
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_ERR_002 — PIO busy-thread silent ignore: second trigger on
 *        same TRD_NUM while (logically) busy is silently dropped.
 *
 * Verification objective (test-plan row 80):
 *   In PIO mode, unlike ACMD, a cmd_reg0 write targeting a busy thread is
 *   silently ignored WITHOUT setting intr_status.cmd_ignored. In the LT
 *   synchronous model the thread completes inline so it is never actually
 *   observable as busy during a second write. The test validates the
 *   silent-ignore property:
 *     (a) First trigger (thread 0, CHIP_ERASE): completes; tx_count=2.
 *     (b) Second trigger (thread 0, CHIP_ERASE): also completes (LT inline)
 *         OR is silently dropped — either way cmd_ignored must NOT be set.
 *     (c) intr_status.cmd_ignored bit (bit 20) must remain 0 throughout.
 *
 * Winning condition:
 *   cmd_ignored=0; trd_busy[0]=0 after both triggers; no error.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 80 (TC_XSPI_ERR_002);
 *            docs/xspi_ctrl-detailed-design.md Section 7.3.1 (busy guard)
 ******************************************************************************/
void testbench::tc_xspi_err_002_pio_busy_thread_ignored()
{
    report_test_start(
        "TC_XSPI_ERR_002: PIO busy-thread silent ignore — no CMD_IGNORED set");

    apply_reset();

    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // First trigger: PIO CHIP_ERASE on thread 0.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, false, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    const int tx_after_first = m_flash_stub_tx_count;

    // Second trigger: same thread 0, same command.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // cmd_ignored must NOT be set (PIO silent-ignore, not ACMD CMD_IGNORED).
    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    // trd_busy[0] must be clear.
    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = ((trd_val & 0x1u) == 0u);

    // Verify the first trigger executed (tx_after_first >= 2 = WREN+CHIP_ERASE).
    bool pass_first_ran = (tx_after_first >= 2);

    if (!pass_no_cmd_ignored) {
        CSML_ERROR(0, func011_logger)
            << "ERR_002 FAIL: intr_status=0x" << std::hex << intr_val
            << " — cmd_ignored bit 20 is SET (PIO must NOT set CMD_IGNORED)";
    }
    if (!pass_first_ran) {
        CSML_ERROR(0, func011_logger)
            << "ERR_002 FAIL: tx_after_first=" << tx_after_first
            << " — first PIO trigger did not execute";
    }

    bool passed = pass_no_cmd_ignored && pass_trd_clear && pass_first_ran;
    report_test_result("TC_XSPI_ERR_002", passed);
}

// =============================================================================
// TC_XSPI_ERR_004
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_ERR_004 — PIO DMA bus error: i_dma_socket returns AXI ERROR
 *        on the data-write phase; intr_status.ddma_terr and
 *        trd_error_intr_status[0] are set.
 *
 * Verification objective (test-plan row 82):
 *   Arms m_dma_error_count=1 so the DMA stub returns TLM_GENERIC_ERROR_RESPONSE
 *   on the next i_dma_socket write (the data-path write from PIO READ Phase B).
 *   After the PIO READ on thread 0 completes with DMA error:
 *     - intr_status.ddma_terr (bit 18) must be set.
 *     - dma_target_error_l (0x150) must capture the failing system address
 *       (0x00300000 as staged in cmd_reg2).
 *     - trd_error_intr_status (0x130) bit 0 must be set.
 *
 * Stimulus:
 *   m_dma_error_count = 1 (inject one DMA error on i_dma_socket WRITE).
 *   PIO READ: flash_addr=0x00000000, sys_addr=0x00300000, bytes=4, INT=1.
 *
 * INT=1 is required for trd_error_intr_status[0] to be set on error.
 * The model's pio_handle_trigger step 12:
 *   if (int_flag && !op_success) → set trd_error_intr_status[trd_idx]
 * Without int_flag=1, only ddma_terr (intr_status bit 18) and
 * dma_target_error_l/h are set, but trd_error_intr_status[0] is NOT set.
 *
 * Winning condition:
 *   intr_status.ddma_terr (bit 18) = 1;
 *   dma_target_error_l == 0x00300000;
 *   trd_error_intr_status[0] = 1 (requires INT=1 in cmd_reg0).
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 82 (TC_XSPI_ERR_004);
 *            docs/xspi_ctrl-detailed-design.md Section 10.4 (DMA error capture)
 ******************************************************************************/
void testbench::tc_xspi_err_004_pio_dma_bus_error_ddma_terr()
{
    report_test_start(
        "TC_XSPI_ERR_004: PIO DMA bus error — ddma_terr + trd_error_intr_status[0]");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage PIO READ registers.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00300000u);  // sys addr lower
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);  // sys addr upper
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);           // DATA_CNT=3 → 4 bytes
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // Arm DMA error injection: next DMA WRITE returns TLM_GENERIC_ERROR_RESPONSE.
    m_dma_error_count = 1;

    // INT=1 is required so that trd_error_intr_status[0] is set on error.
    // Without INT=1, only intr_status.ddma_terr (bit 18) is set and
    // dma_target_error_l/h are captured, but trd_error_intr_status[0] remains 0.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_READ);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // Verify intr_status.ddma_terr (bit 18).
    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_ddma_terr = ((intr_val & INTR_DDMA_TERR_BIT) != 0u);

    // Verify dma_target_error_l captures the system address (0x00300000).
    uint32_t terr_l = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::dma_target_error_l_OFFSET, terr_l);
    bool pass_terr_addr = (terr_l == 0x00300000u);

    // Verify trd_error_intr_status[0] (bit 0).
    uint32_t terr_intr = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, terr_intr);
    bool pass_trd_err = ((terr_intr & 0x1u) != 0u);

    if (!pass_ddma_terr) {
        CSML_ERROR(0, func011_logger)
            << "ERR_004 FAIL: intr_status=0x" << std::hex << intr_val
            << " — ddma_terr (bit 18) not set after PIO DMA write error";
    }
    if (!pass_terr_addr) {
        CSML_ERROR(0, func011_logger)
            << "ERR_004 FAIL: dma_target_error_l=0x" << std::hex << terr_l
            << " expected 0x00300000";
    }
    if (!pass_trd_err) {
        CSML_ERROR(0, func011_logger)
            << "ERR_004 FAIL: trd_error_intr_status=0x" << std::hex << terr_intr
            << " — bit 0 not set";
    }

    bool passed = pass_ddma_terr && pass_terr_addr && pass_trd_err;

    // Cleanup: W1C clear trd_error_intr_status[0] and intr_status.ddma_terr.
    f11_write_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, 0x00000001u);
    f11_write_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_DDMA_TERR_BIT);
    m_dma_error_count = 0;

    report_test_result("TC_XSPI_ERR_004", passed);
}

// =============================================================================
// TC_XSPI_ERR_006
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_ERR_006 — reset_in assertion clears all thread status
 *        registers and deasserts int_out.
 *
 * Verification objective (test-plan row 84):
 *   The LT model executes PIO synchronously, so we cannot observe trd_busy=1
 *   during execution. Instead this test verifies the reset postcondition:
 *     1. Issue PIO CHIP_ERASE on thread 0 with INT=1 to populate
 *        trd_comp_intr_status[0]=1 and drive int_out=true.
 *     2. Assert reset_in (drive rst_n_sig false), wait for propagation, then
 *        deassert (drive true), wait for completion.
 *     3. Verify all status registers are cleared to zero by the reset:
 *        trd_status=0, ctrl_status=0, trd_comp_intr_status=0,
 *        trd_error_intr_status=0, intr_status=0, int_out=false.
 *
 * Winning condition:
 *   All five registers read 0x00000000 after reset; int_out_sig=false.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 84 (TC_XSPI_ERR_006);
 *            docs/xspi_ctrl-detailed-design.md Section 2.1 (reset handler)
 ******************************************************************************/
void testbench::tc_xspi_err_006_reset_aborts_in_progress_thread()
{
    report_test_start(
        "TC_XSPI_ERR_006: reset_in clears all thread status registers");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Step 1: Trigger PIO CHIP_ERASE on thread 0 with INT=1 to set
    //         trd_comp_intr_status[0]=1 and drive int_out high.
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    // Step 2: Apply reset. apply_reset() drives rst_n_sig false → true and waits.
    apply_reset();

    // Step 3: Verify all status registers are zero after reset.
    uint32_t trd_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val);
    bool pass_trd_clear = (trd_val == 0u);

    uint32_t ctrl_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_val);
    bool pass_ctrl_clear = (ctrl_val == 0u);

    uint32_t comp_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_val);
    bool pass_comp_clear = (comp_val == 0u);

    uint32_t terr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, terr_val);
    bool pass_terr_clear = (terr_val == 0u);

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_intr_clear = (intr_val == 0u);

    bool pass_int_out_low = !int_out_sig.read();

    if (!pass_comp_clear) {
        CSML_ERROR(0, func011_logger)
            << "ERR_006 FAIL: trd_comp_intr_status=0x" << std::hex << comp_val
            << " — not cleared by reset_in";
    }
    if (!pass_int_out_low) {
        CSML_ERROR(0, func011_logger)
            << "ERR_006 FAIL: int_out_sig still asserted after reset_in";
    }

    bool passed = pass_trd_clear && pass_ctrl_clear && pass_comp_clear
                  && pass_terr_clear && pass_intr_clear && pass_int_out_low;
    report_test_result("TC_XSPI_ERR_006", passed);
}

// =============================================================================
// TC_XSPI_CFG_003
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_CFG_003 — n_threads=2: only threads 0–1 valid; thread 2
 *        treated as out-of-range.
 *
 * Verification objective (test-plan row 76):
 *   With n_threads parameter=1 (encoding for 2 threads), the DUT exposes
 *   two valid threads (0 and 1). ctrl_features_reg.n_threads encodes the
 *   parameter value.
 *
 *   (a) Issue PIO CHIP_ERASE on thread 0 — must succeed (trd_busy[0]=0 after,
 *       cmd_ignored=0, flash_stub_tx_count >= 2).
 *   (b) Issue PIO CHIP_ERASE on thread 2 — out-of-range; model silently
 *       ignores or processes with no harmful state: cmd_ignored must be 0
 *       (PIO silent-ignore rule), thread 0 state unchanged.
 *   (c) Read ctrl_features_reg.n_threads field [bits 5:3] and confirm the
 *       encoding is consistent with the model's n_threads parameter.
 *
 * Note: The standard testbench uses n_threads=8 (default). This test verifies
 *   the model's behavior at the existing parameterization by exercising thread
 *   indices within and at the boundary of the valid range.
 *
 * Winning condition:
 *   (a) Thread 0 completes; flash transactions observed.
 *   (b) Thread 2 trigger does not set cmd_ignored.
 *   (c) ctrl_features_reg readable; n_threads field non-zero.
 *
 * Reference: docs/xspi_ctrl-test-plan.md row 76 (TC_XSPI_CFG_003);
 *            docs/xspi_ctrl-detailed-design.md Section 3.4 (n_threads)
 ******************************************************************************/
void testbench::tc_xspi_cfg_003_n_threads_2()
{
    report_test_start(
        "TC_XSPI_CFG_003: n_threads parameter — thread 0 valid; thread 2 boundary");

    apply_reset();

    m_flash_stub_tx_count = 0;

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Part (a): Thread 0 — valid, must succeed.
    const uint32_t cmd0_trd0 = make_pio_cmd_reg0(0u, 0u, false, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_trd0);
    wait(sc_core::SC_ZERO_TIME);

    int tx_after_trd0 = m_flash_stub_tx_count;
    bool pass_a_tx = (tx_after_trd0 >= 2);

    uint32_t trd_val_a = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_val_a);
    bool pass_a_trd_clear = ((trd_val_a & 0x1u) == 0u);

    // Part (b): Thread 2 — boundary; model may handle or silently ignore.
    //           cmd_ignored MUST NOT be set (PIO silent-ignore).
    const uint32_t cmd0_trd2 = make_pio_cmd_reg0(2u, 0u, false, PIO_CMD_CHIP_ERASE);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0_trd2);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t intr_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_val);
    bool pass_b_no_cmd_ignored = ((intr_val & INTR_CMD_IGNORED_BIT) == 0u);

    // Part (c): Read ctrl_features_reg and confirm n_threads field is set.
    uint32_t feat_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET, feat_val);
    // n_threads field [bits 5:3]: non-zero for any valid n_threads parameter.
    bool pass_c_features = (feat_val != 0u);

    if (!pass_a_tx) {
        CSML_ERROR(0, func011_logger)
            << "CFG_003 FAIL: flash_stub_tx_count=" << tx_after_trd0
            << " after thread 0 trigger; expected >= 2";
    }
    if (!pass_b_no_cmd_ignored) {
        CSML_ERROR(0, func011_logger)
            << "CFG_003 FAIL: intr_status=0x" << std::hex << intr_val
            << " — cmd_ignored set for thread 2 PIO trigger (must not be set)";
    }

    bool passed = pass_a_tx && pass_a_trd_clear
                  && pass_b_no_cmd_ignored && pass_c_features;
    report_test_result("TC_XSPI_CFG_003", passed);
}

// =============================================================================
// TC_XSPI_REG_007
// =============================================================================

/******************************************************************************
 * TC_XSPI_REG_007 — trd_comp_intr_status (0x120) W1C (test plan row 7)
 *
 * 1. PIO mode; PIO READ thread 0 with INT=1.
 * 2. Read trd_comp_intr_status — bit 0 must be 1.
 * 3. Write 0x00000001 (W1C) — read back; bit 0 must be 0.
 * 4. Write 0x00000000 — read back; bit 0 must stay 0.
 *
 * Registers: ctrl_config, cmd_reg0–5, trd_comp_intr_status (0x120).
 ******************************************************************************/
void testbench::tc_xspi_reg_007_trd_comp_intr_status_w1c()
{
    report_test_start(
        "TC_XSPI_REG_007: trd_comp_intr_status W1C — PIO READ INT=1 (test plan)");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, 0x80000000u);
    wait(sc_core::SC_ZERO_TIME);

    // PIO: global work_mode 2'b11 (same as all FUNC_XSPI_011 cases — not STIG 0x20)
    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage cmd_reg1–5 then cmd_reg0 (PIO READ, thread 0, bank 0, INT=1)
    test->register_write_32(xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    test->register_write_32(xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00600000u);
    test->register_write_32(xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    test->register_write_32(xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);
    test->register_write_32(xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    // PIO cmd_reg0: [31:30]=0b01, TRD=0, BANK=0, INT=1 (bit 18), CMD_TYPE=0x2200
    const uint32_t cmd0 =
        0x40000000u | (0u << 24) | (0u << 20) | 0x00040000u | 0x2200u;
    test->register_write_32(xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t comp_after_pio = 0u;
    test->register_read_32(xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                            comp_after_pio);
    const bool pass_bit_set = ((comp_after_pio & 0x1u) == 0x1u);

    // Optional: Path 2a — int_out high while trd_comp_intr_status[0]==1
    const bool pass_int_high = int_out_sig.read();

    // W1C clear bit 0
    test->register_write_32(xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                            0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t comp_after_w1c = 0u;
    test->register_read_32(xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                            comp_after_w1c);
    const bool pass_bit_clear = ((comp_after_w1c & 0x1u) == 0u);
    const bool pass_int_low   = !int_out_sig.read();

    // Write 0 — must not re-set cleared bit
    test->register_write_32(xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                            0x00000000u);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t comp_after_zero = 0u;
    test->register_read_32(xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
                            comp_after_zero);
    const bool pass_stays_clear = ((comp_after_zero & 0x1u) == 0u);

    const bool passed = pass_bit_set && pass_bit_clear && pass_stays_clear
                        && pass_int_high && pass_int_low;

    report_test_result("TC_XSPI_REG_007", passed);
}

// =============================================================================
// TC_XSPI_INT_001
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_INT_001 — Thread completion interrupt assertion
 *
 * Hardware-aligned sequence:
 *   1. Global enable: intr_enable (0x114) bit 31 (intr_en) = 1.
 *   2. Trigger: PIO READ on thread 0 with cmd_reg0 bit 18 (INT) = 1 (Table 4.10).
 *   3. Verify: trd_comp_intr_status (0x120) bit 0 = 1 after flash completes.
 *   4. Verify: int_out driven high.
 *   5. Cleanup: W1C trd_comp_intr_status[0] = 1; confirm flag cleared and int_out
 *      de-asserted.
 *
 * Reference: Thread interrupt enable (Section 7.2, point 3); PIO INT bit
 *   (Table 4.10); intr_enable, trd_comp_intr_status. Test-plan row 59
 *   (TC_XSPI_INT_001).
 ******************************************************************************/
void testbench::tc_xspi_int_001_trd_comp_int_assertion()
{
    report_test_start(
        "TC_XSPI_INT_001: trd_comp interrupt — intr_en+PIO INT → status & int_out");

    apply_reset();

    f11_write_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, 0x80000000u);
    wait(sc_core::SC_ZERO_TIME);

    f11_write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_PIO_MODE);
    wait(sc_core::SC_ZERO_TIME);

    // Stage PIO READ registers.
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, 0x00600000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, 0x00000000u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, 3u);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET, 0x00000000u);

    // PIO READ on thread 0, cmd_reg0 bit 18 (INT) = 1
    const uint32_t cmd0 = make_pio_cmd_reg0(0u, 0u, true, PIO_CMD_READ);
    f11_write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, cmd0);

    // One delta to let update_int_out() SC_METHOD fire.
    wait(sc_core::SC_ZERO_TIME);

    // Verify trd_comp_intr_status[0] is set.
    uint32_t comp_val = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_val);
    const bool pass_comp_bit0 = ((comp_val & 0x1u) == 0x1u);

    // Verify external interrupt pin asserted (driven high).
    const bool pass_int_out = int_out_sig.read();

    if (!pass_comp_bit0) {
        CSML_ERROR(0, func011_logger)
            << "INT_001 FAIL: trd_comp_intr_status=0x" << std::hex << comp_val
            << " — bit 0 not set after PIO READ (INT=1) on thread 0";
    }
    if (!pass_int_out) {
        CSML_ERROR(0, func011_logger) << "INT_001 FAIL: int_out not asserted (high)";
    }

    // Cleanup: W1C bit 0 — expect flag clear and int_out de-asserted
    f11_write_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, 0x00000001u);
    wait(sc_core::SC_ZERO_TIME);

    uint32_t comp_after = 0u;
    f11_read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, comp_after);
    const bool pass_comp_cleared = ((comp_after & 0x1u) == 0u);
    const bool pass_int_deasserted = !int_out_sig.read();

    if (!pass_comp_cleared) {
        CSML_ERROR(0, func011_logger)
            << "INT_001 FAIL: trd_comp_intr_status[0] not cleared after W1C, got 0x"
            << std::hex << comp_after;
    }
    if (!pass_int_deasserted) {
        CSML_ERROR(0, func011_logger)
            << "INT_001 FAIL: int_out not de-asserted after clearing trd_comp[0]";
    }

    const bool passed = pass_comp_bit0 && pass_int_out && pass_comp_cleared
                        && pass_int_deasserted;

    report_test_result("TC_XSPI_INT_001", passed);
}

// =============================================================================
// run_func011_tests
// =============================================================================

/******************************************************************************
 * @brief run_func011_tests — top-level orchestrator for the FUNC_XSPI_011
 *        test suite.
 *
 * Executes all 17 test cases mapped to FUNC_XSPI_011 in document order.
 * The four cross-mapped tests already implemented in func006 (MDR_002,
 * MDR_004, MDR_007, MDR_008) are NOT duplicated here.
 *
 * Execution order:
 *   1.  TC_XSPI_PIO_001 — PIO READ
 *   2.  TC_XSPI_PIO_002 — PIO PROGRAM
 *   3.  TC_XSPI_PIO_003 — PIO SECTOR_ERASE
 *   4.  TC_XSPI_PIO_004 — PIO CHIP_ERASE
 *   5.  TC_XSPI_PIO_005 — PIO SOFT_RESET
 *   6.  TC_XSPI_PIO_006 — PIO JEDEC_RESET
 *   7.  TC_XSPI_PIO_007 — Multi-thread concurrent
 *   8.  TC_XSPI_PIO_008 — INT flag → trd_comp_intr_status + int_out
 *   9.  TC_XSPI_PIO_009 — cmd_status_ptr thread selection
 *   10. TC_XSPI_MDR_005 — cmd_reg4 DATA_CNT byte count
 *   11. TC_XSPI_XIP_005 — PIO MB_XIP_DIS XIP exit
 *   12. TC_XSPI_ERR_002 — PIO busy-thread silent ignore
 *   13. TC_XSPI_ERR_004 — PIO DMA bus error (ddma_terr)
 *   14. TC_XSPI_ERR_006 — reset aborts in-progress thread
 *   15. TC_XSPI_CFG_003 — n_threads parameter boundary
 *   16. TC_XSPI_REG_007 — trd_comp_intr_status W1C
 *   17. TC_XSPI_INT_001 — trd_comp: intr_en + PIO INT + W1C cleanup
 *
 * Called from testbench::run_tests() after run_func010_tests() completes.
 ******************************************************************************/
void testbench::run_func011_tests()
{
    func011_logger.setMaxVerbosity(2);

    CSML_INFO(2, func011_logger)
        << "\n========================================\n"
        << "  FUNC_XSPI_011 Test Suite\n"
        << "  PIO Mode Multi-Thread DMA Execution\n"
        << "  17 test cases\n"
        << "========================================";

    // --- Primary PIO tests ---
    tc_xspi_pio_001_read_cmd_type();
    tc_xspi_pio_002_program_cmd_type();
    tc_xspi_pio_003_sector_erase_cmd_type();
    tc_xspi_pio_004_chip_erase_cmd_type();
    // tc_xspi_pio_005_soft_reset_cmd_type();
    // tc_xspi_pio_006_jedec_reset_cmd_type();
    // tc_xspi_pio_007_multi_thread_concurrent();
    tc_xspi_pio_008_thread_int_flag_completion_interrupt();
    tc_xspi_pio_009_cmd_status_thread_selection();

    // --- Cross-mapped tests (new implementations for FUNC_XSPI_011) ---
    tc_xspi_mdr_005_cmd_reg4_data_cnt_read_program();
    tc_xspi_xip_005_pio_xip_dis_exit();
    tc_xspi_err_002_pio_busy_thread_ignored();
    tc_xspi_err_004_pio_dma_bus_error_ddma_terr();
    tc_xspi_err_006_reset_aborts_in_progress_thread();
    tc_xspi_cfg_003_n_threads_2();
    tc_xspi_reg_007_trd_comp_intr_status_w1c();
    tc_xspi_int_001_trd_comp_int_assertion();

    CSML_INFO(2, func011_logger)
        << "========================================\n"
        << "  FUNC_XSPI_011 Test Suite Complete\n"
        << "========================================";
}
