/******************************************************************************
 * @file xspi_ctrl_func009_test.cpp
 * @brief FUNC_XSPI_009 — STIG mode (test-plan rows 18, 19, 20, 21, 23)
 *
 * All STIG 128b programming follows the Cadence UG Variant 1 (Profile 1) layout:
 *   - INSTR_TYPE[6:0] in cmd_reg1; CMD[87:80] in cmd_reg3[23:16] (not cmd_reg1[31:24]).
 *   - Address bytes: ADDR0 in cmd_reg1[31:24] … per Table 4.23; not ACMD DATA_CNT.
 *   - INSTR_LINK = cmd_reg4[28]. Glued data: INSTR=127, byte count in bits[79:48]
 *     (reg2/3), DIR at cmd_reg4[4] (Table 4.27). Two cmd_reg0 writes: FIFO push,
 *     then execute; completion via intr_status.stig_done[23] (or internal COMPLETE).
 *
 * TC_001/002: 4B READ/WRITE using two-instruction gluing. TC_003: WREN, opcode
 * in cmd_reg3. TC_004: RDSR 0x05, INSTR_LINK + 1B glued read. TC_006: same chain
 * with 1B read (link discipline). stig_write_read_test: glued
 * program/read; host supplies/consumes payload via t_axi to SDMA window 0xA0…
 * Helpers: stig_write_cmd_regs, stig_trigger_cmd0, R3_ADDRNO3_DATANO0.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "cdns_extension.h"
#include "csml_logger.h"
#include <cstdint>
#include <cstring>

// =============================================================================
// Module-local logger
// =============================================================================
namespace {
    CsmlLogger func009_logger;
}  // namespace

// =============================================================================
// Module-local constants
// =============================================================================
namespace {
    /// @brief Value to write to ctrl_config for STIG mode.
    ///        ctrl_config bits[6:5] = work_mode; 2'b01 → STIG mode.
    ///        ctrl_config write mask = 0x68 (bits 6,5,3).
    ///        Bit 5 = 1, bit 6 = 0 → work_mode = 2'b01 = STIG.
    ///        Write value: (1u << 5) = 0x20.
    static constexpr uint32_t CTRL_CFG_STIG_MODE    = 0x20u;

    /// @brief ctrl_status.gcmd_eng_mc_busy bit (bit 4).
    ///        Set during INSTR_LINK first phase; cleared on second phase.
    static constexpr uint32_t CTRL_STATUS_MC_BUSY    = (1u << 4);

    /// @brief intr_status.stig_done bit (bit 23).
    ///        Set by stig_finish() on every completed STIG operation (single-
    ///        phase) or on the first-phase interim notification (INSTR_LINK).
    ///        This is the observable completion indicator for STIG mode.
    ///
    ///        cmd_status read (0x044) merges the per-thread trd comp/err bits
    ///        (see handle_read_cmd_status) with the backing register's
    ///        cmd_status.COMPLETE (bit 15) set by stig_finish(). Either
    ///        intr_status.stig_done (bit 23) or cmd_status[15] may be used
    ///        to detect STIG completion; STIG tests in this file check both
    ///        where the test plan requires cmd_status.
    static constexpr uint32_t INTR_STATUS_STIG_DONE  = (1u << 23);

    /// @brief cmd_status.COMPLETE (bit 15) — set by stig_finish(), merged on read
    static constexpr uint32_t CMD_STATUS_COMPLETE    = (1u << 15);

    /// @brief Wait time after cmd_reg0 trigger to allow STIG SC_THREAD execution.
    ///        Single-phase READ/CONTROL: 1 flash b_transport (5 ns) + overhead.
    static constexpr int STIG_SETTLE_NS = 30;

    /// STIG INSTR_TYPE values for erase suspend / erase resume (see execute_stig()).
    static constexpr uint8_t kStigEraseSuspendInstr = 39u;
    static constexpr uint8_t kStigEraseResumeInstr  = 40u;

}  // namespace

// =============================================================================
// File-scope helper functions
// =============================================================================

/******************************************************************************
 * @brief Issue a PoR transaction with discovery_inhibit=1 to unblock callbacks.
 *
 * The xspi_ctrl model requires an xspi_PoR_trans transaction before register
 * write callbacks are active (m_init_comp_done gate). Setting discovery_inhibit=1
 * bypasses the SFDP READ_SFDP flash bus cycle, making this fast and deterministic
 * for test setup.
 *
 * @param por_initiator  testbench PoR initiator socket
 ******************************************************************************/
static void func009_send_por_inhibited(
    tlm_utils::simple_initiator_socket<testbench, 64>& por_initiator)
{
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 1u;   // Skip SFDP discovery
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 1u;
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_dummy_cnt = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    tlm::tlm_generic_payload payload;
    uint8_t data_buf[8] = {0};
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0u));
    payload.set_data_ptr(data_buf);
    payload.set_data_length(8u);
    payload.set_streaming_width(8u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    payload.set_extension(&por_ext);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    por_initiator->b_transport(payload, delay);

    // Remove the stack-allocated extension before the payload destructs.
    xspi_PoR_trans* removed = nullptr;
    payload.get_extension(removed);
    if (removed != nullptr) {
        payload.clear_extension(removed);
    }
}

/******************************************************************************
 * @brief PoR with SFDP discovery enabled (discovery_inhibit=0).
 *        DUT issues READ_SFDP; configure_registers_from_sfdp() fills stat_seq_cfg_*.
 ******************************************************************************/
static void func009_send_por_discovery_sfdp(
    tlm_utils::simple_initiator_socket<testbench, 64>& por_initiator)
{
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 0u;
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 1u;
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_dummy_cnt = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    tlm::tlm_generic_payload payload;
    uint8_t data_buf[8] = {0};
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0u));
    payload.set_data_ptr(data_buf);
    payload.set_data_length(8u);
    payload.set_streaming_width(8u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    payload.set_extension(&por_ext);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    por_initiator->b_transport(payload, delay);

    xspi_PoR_trans* removed = nullptr;
    payload.get_extension(removed);
    if (removed != nullptr) {
        payload.clear_extension(removed);
    }
}

/// DUT `k_stig_sdma_axi_base` — STIG merged write/read SDMA data window.
static constexpr uint64_t k_func009_sdma_axi_base = 0xA0000000uLL;

static tlm::tlm_response_status func009_sdma_axi_write(
    tlm_utils::simple_initiator_socket<testbench, 64>& axi,
    uint64_t                                           addr,
    const uint8_t*                                     data,
    uint32_t                                            n)
{
    if (n == 0u) {
        return tlm::TLM_OK_RESPONSE;
    }
    std::vector<uint8_t> buf(data, data + n);
    tlm::tlm_generic_payload pl;
    pl.set_command(tlm::TLM_WRITE_COMMAND);
    pl.set_address(static_cast<sc_dt::uint64>(addr));
    pl.set_data_ptr(buf.data());
    pl.set_data_length(n);
    pl.set_streaming_width(n);
    pl.set_byte_enable_ptr(nullptr);
    pl.set_byte_enable_length(0u);
    pl.set_dmi_allowed(false);
    pl.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    axi->b_transport(pl, d);
    return pl.get_response_status();
}

static tlm::tlm_response_status func009_sdma_axi_read(
    tlm_utils::simple_initiator_socket<testbench, 64>& axi,
    uint64_t                                         addr,
    uint8_t*                                         out,
    uint32_t                                         n)
{
    if (n == 0u) {
        return tlm::TLM_OK_RESPONSE;
    }
    tlm::tlm_generic_payload pl;
    pl.set_command(tlm::TLM_READ_COMMAND);
    pl.set_address(static_cast<sc_dt::uint64>(addr));
    pl.set_data_ptr(out);
    pl.set_data_length(n);
    pl.set_streaming_width(n);
    pl.set_byte_enable_ptr(nullptr);
    pl.set_byte_enable_length(0u);
    pl.set_dmi_allowed(false);
    pl.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    axi->b_transport(pl, d);
    return pl.get_response_status();
}

/******************************************************************************
 * @brief Write a 32-bit value to a DUT register.
 *
 * @param test    xspi_ctrl_test harness instance
 * @param offset  Register byte offset in DUT address space
 * @param value   32-bit value to write
 ******************************************************************************/
static void stig_wreg(xspi_ctrl_test* test,
                      unsigned int     offset,
                      uint32_t         value)
{
    test->register_write_32(offset, value);
}

/******************************************************************************
 * @brief Read a 32-bit value from a DUT register.
 *
 * @param test    xspi_ctrl_test harness instance
 * @param offset  Register byte offset in DUT address space
 * @param value   Reference to receive the 32-bit read value
 ******************************************************************************/
static void stig_rreg(xspi_ctrl_test* test,
                      unsigned int     offset,
                      uint32_t&        value)
{
    test->register_read_32(offset, value);
}

/******************************************************************************
 * @brief Block until the DUT asserts `intr_status.sdma_trigg[21]` (SDMA handshaking).
 *        The STIG thread must run into `stig_sdma_begin_host_write` / publish before
 *        an AXI transfer; ordering vs. the test process is not guaranteed.
 ******************************************************************************/
static void func009_wait_sdma_trigg(xspi_ctrl_test* t)
{
    // Merged path issues WREN before sdma_trigg; flash dispatch can be >> a few
    // nanoseconds, so 1ps×N was too small and AXI was offered before `await` was set.
    for (int n = 0; n < 5000; ++n) {
        uint32_t is = 0u;
        stig_rreg(t, xspi_ctrl_basetest::intr_status_OFFSET, is);
        if ((is & (1u << 21)) != 0u) {   // same as DUT: intr_status.sdma_trigg
            return;
        }
        wait(1, sc_core::SC_NS);
    }
    CSML_ERROR(0, func009_logger) << "func009: timeout waiting for intr_status.sdma_trigg[21]";
}

/******************************************************************************
 * @brief Stage all four STIG instruction registers and fire the trigger.
 *
 * Implements the mandatory staging order (Section 5.3.8, Rule 1):
 *   1. cmd_reg1 written (opcode, IOS, lower INSTR_TYPE context)
 *   2. cmd_reg2 written (address upper bits)
 *   3. cmd_reg3 written (DATA_CNT, address lower bits)
 *   4. cmd_reg4 written (INSTR_LINK, write data, authoritative INSTR_TYPE)
 *   5. cmd_reg0 written LAST — fires cmd_trigger_event in the DUT
 *
 * @param test       xspi_ctrl_test harness instance
 * @param reg1_val   Value for cmd_reg1 (STIG instr bits[31:0])
 * @param reg2_val   Value for cmd_reg2 (STIG instr bits[63:32])
 * @param reg3_val   Value for cmd_reg3 (STIG instr bits[95:64])
 * @param reg4_val   Value for cmd_reg4 (STIG instr bits[127:96])
 ******************************************************************************/
static void stig_stage_and_trigger(xspi_ctrl_test* test,
                                   uint32_t reg1_val,
                                   uint32_t reg2_val,
                                   uint32_t reg3_val,
                                   uint32_t reg4_val)
{
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, reg1_val);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, reg2_val);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, reg3_val);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, reg4_val);
    // cmd_reg0 written LAST — any value fires the STIG engine (value is ignored).
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);
}

/******************************************************************************
 * @brief Program cmd_reg1–4 only (no cmd_reg0) — first half of a FIFO entry.
 ******************************************************************************/
static void stig_write_cmd_regs(xspi_ctrl_test* test,
                                 uint32_t         r1,
                                 uint32_t         r2,
                                 uint32_t         r3,
                                 uint32_t         r4)
{
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET, r1);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET, r2);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET, r3);
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET, r4);
}

/******************************************************************************
 * @brief Write cmd_reg0 to push a staged 128b instruction to the command FIFO
 *        or to execute a glued phase (per UG: always last).
 ******************************************************************************/
static void stig_trigger_cmd0(xspi_ctrl_test* test)
{
    stig_wreg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET, 0x00000001u);
}

/// High byte of cmd_reg3 (bits [95:88] of 128b): bit95=0, [94:92] ADDR_NO (0-6),
/// [25:24] in word = DATA/MODE[1:0] — 0b0_011_00_00 = 0x30 for ADDR_NO=3, zero data in command.
static constexpr uint32_t R3_ADDRNO3_DATANO0 = 0x30u;

// =============================================================================
// TC_XSPI_STIG_001 — STIG READ handler encoding
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_001 — STIG READ 4B via Profile 1 instruction gluing
 *
 * UG Table 4.23/4.28: READ uses INSTR_TYPE=1 and must glue for >2 data bytes.
 * 128b layout: INSTR[6:0] in cmd_reg1; CMD[87:80] in cmd_reg3[23:16]; INSTR_LINK
 * at cmd_reg4[28]; second instruction INSTR=127, DATA[79:48]=4, DIR=0
 * (Table 4.27, bit 100 → cmd_reg4[4]).
 * Address 0x00010000: ADDR0..2 via UG (ADDR2 in cmd_reg2[15:8]=0x01).
 ******************************************************************************/
void testbench::tc_xspi_stig_001_read_handler()
{
    report_test_start("TC_XSPI_STIG_001: STIG READ Handler (Glued 4-Byte Read)");

    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    m_flash_stub_tx_count = 0u;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    // Instruction 1 — command phase: READ(1), READ_FAST(0x0B), 3 address bytes, 0
    // inline data, INSTR_LINK=1. cmd_reg3 = R3 high | CMD(0x0B) at [23:16].
    const uint32_t ph1_r1 = 0x00000001u;  // [6:0]=READ, [31:24] ADDR0=0
    const uint32_t ph1_r2 = 0x00000100u;  // addr bytes → physical 0x00010000
    const uint32_t ph1_r3 = (R3_ADDRNO3_DATANO0 << 24) | (0x0Bu << 16);
    const uint32_t ph1_r4 = 0x10000000u;  // INSTR_LINK

    stig_write_cmd_regs(test, ph1_r1, ph1_r2, ph1_r3, ph1_r4);
    stig_trigger_cmd0(test);

    // Instruction 2 — glued: INSTR=127, nbytes=4 at [79:48], DIR=0 (read)
    const uint32_t ph2_r1 = 0x0000007Fu;   // 127
    const uint32_t ph2_r2 = 0x00040000u;   // low 16 of 32b count: 4
    const uint32_t ph2_r3 = 0u;
    const uint32_t ph2_r4 = 0u;            // DIR: reg4[4]=0

    stig_write_cmd_regs(test, ph2_r1, ph2_r2, ph2_r3, ph2_r4);
    stig_trigger_cmd0(test);

    wait(50, sc_core::SC_NS);

    uint32_t intr_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);

    // One merged bus read; DUT encodes as GLUED(127) on the extension
    const bool pass_tx   = (m_flash_stub_tx_count == 1u);
    const bool pass_opc  = (m_last_flash_ext.opcode == 0x0Bu);
    const bool pass_addr = (m_last_flash_ext.address == 0x00010000ULL);
    const bool pass_dlen = (m_last_flash_ext.data_bytes == 4u);
    const bool pass_ity  = (m_last_flash_ext.instr_type
                            == static_cast<uint8_t>(XSPI_INSTR_GLUED));
    const bool pass_bank = (m_last_flash_ext.bank_num == 0u);
    const bool pass_done = ((intr_status_val & INTR_STATUS_STIG_DONE) != 0u);

    if (!pass_tx) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_001 [A] expected 1 flash tx, got " << m_flash_stub_tx_count;
    }
    if (!pass_opc) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [B] opcode";
    }
    if (!pass_addr) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [C] address";
    }
    if (!pass_dlen) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [D] data len";
    }
    if (!pass_ity) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [E] expect GLUED=127 on bus";
    }
    if (!pass_bank) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [F] bank";
    }
    if (!pass_done) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_001 [G] stig_done";
    }

    report_test_result("TC_XSPI_STIG_001", pass_tx && pass_opc && pass_addr && pass_dlen
                    && pass_ity && pass_bank && pass_done);
}

// =============================================================================
// TC_XSPI_STIG_002 — 4B program via gluing (INSTR=0 + 0x7F, DIR=1)
// =============================================================================
void testbench::tc_xspi_stig_002_write_handler()
{
    report_test_start("TC_XSPI_STIG_002: STIG WRITE (Glued 4-Byte Program)");

    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    m_flash_stub_tx_count = 0u;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    const uint32_t ph1_r1 = 0x00000000u;  // INSTR_TYPE=0, generic WRITE / program
    const uint32_t ph1_r2 = 0x00000200u;  // 0x00020000
    const uint32_t ph1_r3 = (R3_ADDRNO3_DATANO0 << 24) | (0x02u << 16);
    const uint32_t ph1_r4 = 0x10000000u;

    stig_write_cmd_regs(test, ph1_r1, ph1_r2, ph1_r3, ph1_r4);
    stig_trigger_cmd0(test);

    const uint32_t ph2_r1 = 0x0000007Fu;
    const uint32_t ph2_r2 = 0x00040000u;
    const uint32_t ph2_r3 = 0u;
    const uint32_t ph2_r4 = 0x10u;  // DIR=1 (host→flash)

    stig_write_cmd_regs(test, ph2_r1, ph2_r2, ph2_r3, ph2_r4);
    stig_trigger_cmd0(test);
    func009_wait_sdma_trigg(test);

    {
        const uint8_t w4[4] = {0xABu, 0xCDu, 0x12u, 0x34u};
        const tlm::tlm_response_status axs = func009_sdma_axi_write(
            t_axi_slave_initiator, k_func009_sdma_axi_base, w4, 4u);
        if (axs != tlm::TLM_OK_RESPONSE) {
            CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_002: SDMA AXI write (4B) status " << (int)axs;
        }
    }

    wait(50, sc_core::SC_NS);

    uint32_t intr_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);

    const bool pass_tx  = (m_flash_stub_tx_count == 2u);
    const bool pass_opc = (m_last_flash_ext.opcode == 0x02u);
    const bool pass_ity = (m_last_flash_ext.instr_type
                           == static_cast<uint8_t>(XSPI_INSTR_WRITE));
    const bool pass_d   = ((intr_status_val & INTR_STATUS_STIG_DONE) != 0u);

    if (!pass_tx)  { CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_002 [A]"; }
    if (!pass_opc) { CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_002 [B]"; }
    if (!pass_ity) { CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_002 [C]"; }
    if (!pass_d)   { CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_002 [D]"; }

    report_test_result("TC_XSPI_STIG_002", pass_tx && pass_opc && pass_ity && pass_d);
}

// =============================================================================
// TC_XSPI_STIG_003 — STIG control-command handler (WREN, no data)
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_003 — STIG control-command handler (WREN, no data)
 *
 * ## Specification Source
 *
 * Test-plan row 20:
 *   "Program cmd_reg1–cmd_reg4 with a control instruction encoding WREN
 *   (opcode=0x06), trigger via cmd_reg0. Confirm xspi_bus_socket[0] receives
 *   exactly one transaction with opcode=0x06 and no address or data payload.
 *   Confirm cmd_status.COMPLETE is set."
 *
 * Functionality-testcases.md:
 *   "STIG control handler for WREN: single opcode=0x06, no address or data;
 *   cdns_extension encoding verified"
 *
 * ## Dispatch Path Analysis
 *
 * With instr_type=XSPI_INSTR_GENERIC (0) in cmd_reg4[6:0], the execute_stig()
 * switch dispatches to the default case → handle_stig_control_command().
 * Within that handler, opcode=0x06 matches the WREN canonical path:
 *   is_read_cmd = false; data_bytes = 0; TLM_WRITE_COMMAND issued.
 * This produces exactly one flash transaction with opcode=0x06 and data_bytes=0.
 *
 * ## Stimulus
 *
 * 1. apply_reset() + PoR(inhibit).
 * 2. ctrl_config = 0x20 (STIG mode).
 * 3. Baseline flash stub counters.
 * 4. Stage STIG WREN control instruction:
 *      opcode=0x06, instr_type=XSPI_INSTR_GENERIC(0), DATA_CNT=0 (no data).
 *    Register values:
 *      cmd_reg1 = 0x06000000  opcode=0x06 in [31:24], INSTR_TYPE=0 in [6:0]
 *      cmd_reg2 = 0x00000000  no address bits needed
 *      cmd_reg3 = 0x00000000  DATA_CNT=0 (GENERIC type → data_bytes_count=0)
 *      cmd_reg4 = 0x00000000  INSTR_LINK=0, INSTR_TYPE=0 (XSPI_INSTR_GENERIC)
 * 5. Write cmd_reg0 to trigger.
 * 6. wait(20 ns).
 *
 * ## Pass Conditions
 *
 *   - m_flash_stub_tx_count == 1; target CS0 (xspi_bus_socket[0])
 *   - m_last_flash_ext.opcode == 0x06, address == 0, data_bytes == 0
 *   - intr_status.stig_done (bit 23) == 1
 *   - cmd_status.COMPLETE (bit 15) == 1 (after indirect read: cmd_status_ptr, cmd_status)
 ******************************************************************************/
void testbench::tc_xspi_stig_003_control_wren()
{
    report_test_start("TC_XSPI_STIG_003: STIG Control Command Handler (WREN)");

    // -------------------------------------------------------------------------
    // Step 1: Apply reset and send PoR.
    // -------------------------------------------------------------------------
    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    // -------------------------------------------------------------------------
    // Step 2: Set STIG mode.
    // -------------------------------------------------------------------------
    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    // -------------------------------------------------------------------------
    // Step 3: Baseline flash stub state.
    // -------------------------------------------------------------------------
    m_flash_stub_tx_count = 0u;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    // -------------------------------------------------------------------------
    // Step 4: Program cmd_reg1–cmd_reg4: control WREN, then trigger via cmd_reg0.
    // Cadence UG Profile 1 (this file, decode_instruction): INSTR[6:0] in
    // cmd_reg1, CMD[87:80] in cmd_reg3[23:16], not cmd_reg1[31:24].
    //   cmd_reg1: INSTR_TYPE=0 (GENERIC) → default → handle_stig_control_command
    //   cmd_reg2/cmd_reg3[7:0]: address bytes all 0 (no address phase)
    //   cmd_reg3: CMD=0x06 (WREN) in [23:16]; [25:24] data/mode=0
    //   cmd_reg4: INSTR_LINK=0, no glued second phase
    // -------------------------------------------------------------------------
    const uint32_t cmd_r1 = 0x00000000u;
    const uint32_t cmd_r2 = 0x00000000u;
    const uint32_t cmd_r3 = 0x00060000u;  // (0x06 << 16): WREN in CMD field
    const uint32_t cmd_r4 = 0x00000000u;

    stig_stage_and_trigger(test, cmd_r1, cmd_r2, cmd_r3, cmd_r4);

    // -------------------------------------------------------------------------
    // Step 5: Wait for STIG engine to complete the single control dispatch.
    // -------------------------------------------------------------------------
    wait(STIG_SETTLE_NS, sc_core::SC_NS);

    // -------------------------------------------------------------------------
    // Step 6: Read completion: intr_status + cmd_status (indirect, thread 0).
    // -------------------------------------------------------------------------
    uint32_t intr_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);

    stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cmd_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_status_val);

    // -------------------------------------------------------------------------
    // Step 7: Assertions — one bus transaction on CS0, WREN, no address/data;
    //           STIG done + cmd_status COMPLETE.
    // -------------------------------------------------------------------------
    const bool pass_tx = (m_flash_stub_tx_count == 1u);
    if (!pass_tx) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [A]: FAIL — expected 1 flash tx on xspi_bus_socket[0],"
            << " got " << m_flash_stub_tx_count;
    }

    const bool pass_cs0 = (m_last_flash_bank == 0);
    if (!pass_cs0) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [A2]: FAIL — expected CS0, bank="
            << m_last_flash_bank;
    }

    const bool pass_opcode = (m_last_flash_ext.opcode == 0x06u);
    if (!pass_opcode) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [B]: FAIL — opcode mismatch:"
            << " expected=0x06(WREN)"
            << " got=0x" << std::hex
            << static_cast<unsigned>(m_last_flash_ext.opcode);
    }

    // No address byte phase (physical address 0) and no data payload
    const bool pass_addr  = (m_last_flash_ext.address == 0u);
    if (!pass_addr) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [B2]: FAIL — address expected 0, got 0x"
            << std::hex << m_last_flash_ext.address;
    }

    const bool pass_bytes = (m_last_flash_ext.data_bytes == 0u);
    if (!pass_bytes) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [C]: FAIL — data_bytes mismatch:"
            << " expected=0 (command-only)"
            << " got=" << m_last_flash_ext.data_bytes;
    }

    const bool pass_done = ((intr_status_val & INTR_STATUS_STIG_DONE) != 0u);
    if (!pass_done) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [D]: FAIL — intr_status.stig_done not set"
            << " (intr_status=0x" << std::hex << intr_status_val << ")";
    }

    const bool pass_cstat = ((cmd_status_val & CMD_STATUS_COMPLETE) != 0u);
    if (!pass_cstat) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_003 [E]: FAIL — cmd_status.COMPLETE (bit 15) not set"
            << " (cmd_status=0x" << std::hex << cmd_status_val << ")";
    }

    const bool passed = pass_tx && pass_cs0 && pass_opcode && pass_addr
                        && pass_bytes && pass_done && pass_cstat;

    report_test_result("TC_XSPI_STIG_003", passed);
}

// =============================================================================
// TC_XSPI_STIG_004 — STIG READ_STATUS_REG (RDSR) via INSTR_LINK + glued data
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_004 — STIG control path for RDSR (opcode 0x05)
 *
 * Test plan / functionality case: READ_STATUS_REG on xspi_bus_socket[0], status
 * byte from flash stub, completion visible in cmd_status.
 *
 * Cadence UG Profile 1: INSTR_TYPE[6:0] in cmd_reg1; CMD[87:80] in cmd_reg3[23:16];
 * data length for a read is specified with a second INSTR_LINK phase (INSTR=127)
 * (same gluing pattern as TC_XSPI_STIG_001). INSTR_TYPE=1 (READ) in phase 1.
 *
 * DUT path: INSTR_LINK phase-2 calls handle_stig_merged_read() → one TLM_READ
 * with opcode 0x05, data_bytes=1; data lands in the STIG SDMA buffer. The LT
 * model does not map read data into cmd_status[31:16] (no DATA_FROM_DEV /
 * STATUS_SOURCE); host observes the byte via the STIG AXI window (0xA0…), and
 * completion via cmd_status.COMPLETE (bit 15) + intr_status.stig_done.
 *
 * A single-phase **WRDI (0x04)** is issued first so the persistent flash
 * `xspi_target` model (not reset with the DUT) is not left in WEL=1 from a
 * prior WREN; SR1[1] clear yields RDSR = 0x00 for the default stub.
 *
 * Flash stub: xspi_target returns `status_register` for READ_STATUS_REG
 * (handle_control_command, READ_STATUS_REG case).
 ******************************************************************************/
void testbench::tc_xspi_stig_004_control_rdsr()
{
    report_test_start("TC_XSPI_STIG_004: STIG READ_STATUS_REG (RDSR, Glued 1-Byte Read)");

    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    // xspi_target state survives across tests: clear WEL (SR1[1]) so RDSR=0x00
    {
        const uint32_t wrdi_r1 = 0u;
        const uint32_t wrdi_r2 = 0u;
        const uint32_t wrdi_r3 = 0x00040000u;  // CMD=WRDI(0x04) in [23:16]
        const uint32_t wrdi_r4 = 0u;
        stig_stage_and_trigger(test, wrdi_r1, wrdi_r2, wrdi_r3, wrdi_r4);
        wait(STIG_SETTLE_NS, sc_core::SC_NS);
    }

    m_flash_stub_tx_count = 0u;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    // Phase 1 — READ, CMD=0x05 (RDSR), no address, INSTR_LINK=1 (arm chain).
    const uint32_t ph1_r1 = 0x00000001u;  // XSPI_INSTR_READ(1) in [6:0]
    const uint32_t ph1_r2 = 0x00000000u;
    const uint32_t ph1_r3 = 0x00050000u;  // CMD=0x05; [25:24] data/mode=0
    const uint32_t ph1_r4 = 0x10000000u;  // INSTR_LINK=1

    stig_write_cmd_regs(test, ph1_r1, ph1_r2, ph1_r3, ph1_r4);
    stig_trigger_cmd0(test);

    // Phase 2 — Glued data: INSTR=127, 1 byte, DIR=0 (flash → host)
    const uint32_t ph2_r1 = 0x0000007Fu;
    const uint32_t ph2_r2 = 0x00010000u;  // low 16 of 32b count: 1
    const uint32_t ph2_r3 = 0u;
    const uint32_t ph2_r4 = 0u;  // [4]=0 read

    stig_write_cmd_regs(test, ph2_r1, ph2_r2, ph2_r3, ph2_r4);
    stig_trigger_cmd0(test);

    func009_wait_sdma_trigg(test);

    wait(50, sc_core::SC_NS);

    uint8_t      status_byte = 0xFFu;
    const tlm::tlm_response_status axs = func009_sdma_axi_read(
        t_axi_slave_initiator, k_func009_sdma_axi_base, &status_byte, 1u);
    if (axs != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_004: SDMA AXI read (1B) status "
                                        << (int)axs;
    }

    uint32_t intr_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);

    stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
    uint32_t cmd_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_status_val);

    const bool pass_tx = (m_flash_stub_tx_count == 1u);
    if (!pass_tx) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [A]: expected 1 flash tx, got " << m_flash_stub_tx_count;
    }

    const bool pass_cs0 = (m_last_flash_bank == 0);
    if (!pass_cs0) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [A2]: expected CS0, bank=" << m_last_flash_bank;
    }

    const bool pass_opc = (m_last_flash_ext.opcode == 0x05u);
    if (!pass_opc) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [B]: opcode expected 0x05, got 0x" << std::hex
            << static_cast<unsigned>(m_last_flash_ext.opcode);
    }

    const bool pass_dlen = (m_last_flash_ext.data_bytes == 1u);
    if (!pass_dlen) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [C]: data_bytes expected 1, got "
            << m_last_flash_ext.data_bytes;
    }

    const bool pass_ity = (m_last_flash_ext.instr_type
                           == static_cast<uint8_t>(XSPI_INSTR_GLUED));
    if (!pass_ity) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_004 [D]: expect GLUED(127) on bus";
    }

    const bool pass_addr = (m_last_flash_ext.address == 0u);
    if (!pass_addr) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [D2]: address expected 0, got 0x" << std::hex
            << m_last_flash_ext.address;
    }

    const bool pass_done = ((intr_status_val & INTR_STATUS_STIG_DONE) != 0u);
    if (!pass_done) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [E]: stig_done not set, intr_status=0x" << std::hex
            << intr_status_val;
    }

    const bool pass_cstat = ((cmd_status_val & CMD_STATUS_COMPLETE) != 0u);
    if (!pass_cstat) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [F]: cmd_status.COMPLETE (15) not set, cmd_status=0x"
            << std::hex << cmd_status_val;
    }

    // Default flash model SR1 after target reset: 0x00 (ready, WEL=0)
    const bool pass_data = (status_byte == 0x00u) && (axs == tlm::TLM_OK_RESPONSE);
    if (!pass_data) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_004 [G]: status byte expected 0x00, got 0x" << std::hex
            << static_cast<unsigned>(status_byte);
    }

    const bool passed = pass_tx && pass_cs0 && pass_opc && pass_dlen && pass_ity
                        && pass_addr && pass_done && pass_cstat && pass_data;

    report_test_result("TC_XSPI_STIG_004", passed);
}

// =============================================================================
// TC_XSPI_STIG_006 — INSTR_LINK two-phase chaining
// =============================================================================

// =============================================================================
// TC_XSPI_STIG_006 — INSTR_LINK two-phase chaining
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_006 — INSTR_LINK two-phase chaining
 *
 * ## Specification Source
 *
 * Test-plan row 23:
 *   "Program cmd_reg1–cmd_reg4 with the first phase of a chained instruction
 *   with INSTR_LINK=1 (bit 28 of cmd_reg4=1, INSTR_TYPE=encoded chain variant),
 *   write cmd_reg0 to trigger phase 1. Verify ctrl_status.gcmd_eng_mc_busy=1
 *   (waiting for second write). Then program cmd_reg1–cmd_reg4 with the second
 *   phase and write cmd_reg0 again. Verify the combined flash transaction
 *   appears on xspi_bus_socket[0] and cmd_status.COMPLETE is set after the
 *   second trigger."
 *
 * Functionality-testcases.md:
 *   "INSTR_LINK two-phase chain: first trigger buffers and asserts
 *   gcmd_eng_mc_busy; second trigger fires combined transaction;
 *   cdns_extension.instr_link field verified"
 *
 * ## INSTR_LINK State Machine (Section 5.3.8 Rule 6, model source)
 *
 * stig_engine_thread() INSTR_LINK first-phase path (inst.instr_link=1):
 *   stig_instr_link_pending = true;
 *   ctrl_status |= (1u << 4);    // gcmd_eng_mc_busy
 *   intr_status |= (1u << 23);   // stig_done (first-phase signal)
 *   evaluate_interrupt_out();
 *   continue;   // loop back — stig_finish() NOT called; COMPLETE stays 0
 *
 * stig_engine_thread() INSTR_LINK second-phase path (stig_instr_link_pending=true):
 *   inst.instr_type = XSPI_INSTR_GLUED (127); // forced regardless of cmd_reg4[6:0]
 *   stig_instr_link_pending = false;
 *   ctrl_status &= ~(1u << 4);   // clear gcmd_eng_mc_busy
 *   execute_stig(inst, &first_phase);  // dispatch Glued transaction
 *   stig_finish();   // sets COMPLETE=1, fires stig_done=1
 *
 * ## Stimulus
 *
 * Phase 1 (INSTR_LINK=1, arm the chain):
 * 1. apply_reset() + PoR(inhibit) — clean state; cmd_status=0, intr_status=0.
 * 2. ctrl_config = 0x20 (STIG mode).
 * 3. Clear intr_status.stig_done (W1C write 1 to bit 23).
 * 4. Stage first-phase instruction (INSTR_LINK=1), UG Profile 1 READ @0x10000
 *    (same command-phase layout as TC_XSPI_STIG_001):
 *      cmd_reg1..4 with INSTR_LINK=1 (bit 28 of cmd_reg4)
 * 5. Write cmd_reg0 trigger.
 * 6. wait(20 ns).
 * 7. READ ctrl_status, cmd_status, intr_status → phase-1 assertions.
 *
 * Phase 2 (INSTR_TYPE=GLUED, complete the chain):
 * 8. Stage second-phase glued data instruction (Table 4.27; 1 B read):
 *      cmd_reg1 = 0x7F, cmd_reg2 = 0x00010000 (nbytes low), cmd_reg3/4 per model
 * 9. Write cmd_reg0 trigger.
 * 10. wait(20 ns).
 * 11. READ ctrl_status, cmd_status, intr_status → phase-2 assertions.
 *
 * ## Pass Conditions
 *
 * After phase 1:
 *   - ctrl_status bit 4 (gcmd_eng_mc_busy) == 1
 *   - intr_status bit 23 (stig_done) == 1 (first-phase fires stig_done)
 *
 * After phase 2:
 *   - Exactly one new flash b_transport on xspi_bus_socket[0] (harness
 *     m_flash_tx_per_target[0]); combined GLUED read matches opcode/address/len.
 *   - ctrl_status bit 4 (gcmd_eng_mc_busy) == 0 (chain consumed)
 *   - intr_status bit 23 (stig_done) == 1 (second stig_done from stig_finish)
 *   - cmd_status read (0x044) has COMPLETE (bit 15) set (merged in handle_read_cmd_status).
 *
 * After phase 1:
 *   - cmd_status COMPLETE (bit 15) must still be 0 (stig_finish not called).
 *
 * Note on INSTR_LINK guard fix: the original handle_write_cmd_reg0() busy guard
 * blocked the second-phase trigger because gcmd_eng_busy (bit 3) remains set
 * during the INSTR_LINK pending state.  The guard was updated to exempt the
 * second trigger when stig_instr_link_pending=true and gcmd_eng_mc_busy (bit 4)
 * is set, allowing the chain to complete as specified in Section 5.3.8 Rule 6.
 ******************************************************************************/
 void testbench::tc_xspi_stig_006_instr_link_two_phase_chain()
 {
     report_test_start("TC_XSPI_STIG_006: INSTR_LINK Two-Phase Chain");
 
     static constexpr uint32_t kCmdStatusComplete = (1u << 15);
 
     // -------------------------------------------------------------------------
     // Step 1: Apply reset and send PoR for a clean DUT state.
     //         After reset: cmd_status=0 (COMPLETE=0), ctrl_status=0,
     //         intr_status=0, stig_instr_link_pending=false.
     // -------------------------------------------------------------------------
     apply_reset();
     func009_send_por_inhibited(por_initiator);
     wait(5, sc_core::SC_NS);
 
     // -------------------------------------------------------------------------
     // Step 2: Set STIG mode.
     // -------------------------------------------------------------------------
     stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
     wait(2, sc_core::SC_NS);
 
     // -------------------------------------------------------------------------
     // Step 3: Baseline flash socket[0] (xspi_flash_target path) and clear stig_done.
     //         intr_status is W1C; write 1 to bit 23 to clear any residual.
     // -------------------------------------------------------------------------
     m_last_flash_ext       = cdns_extension();
     m_last_flash_bank      = -1;
     const int flash_tx0_base = m_flash_tx_per_target[0];
 
     // W1C clear stig_done (bit 23) in intr_status.
     // intr_status write mask = 0x1FF7F000; bit 23 is within the mask.
     stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
     wait(2, sc_core::SC_NS);
 
     // =========================================================================
     // Phase 1 — First trigger, INSTR_LINK=1 (arm the chain)
     // =========================================================================
 
     // -------------------------------------------------------------------------
     // Step 4: Stage the first-phase instruction with INSTR_LINK=1.
     //
     //   Phase1: READ(0x0B) command @ 0x00010000, 3 address bytes, INSTR_LINK=1
     // -------------------------------------------------------------------------
     const uint32_t ph1_r1 = 0x00000001u;
     const uint32_t ph1_r2 = 0x00000100u;
     const uint32_t ph1_r3 = (R3_ADDRNO3_DATANO0 << 24) | (0x0Bu << 16);
     const uint32_t ph1_r4 = 0x10000000u;  // INSTR_LINK=1 (bit 28)
 
     stig_stage_and_trigger(test, ph1_r1, ph1_r2, ph1_r3, ph1_r4);
 
     // -------------------------------------------------------------------------
     // Step 5: Wait for the STIG thread to process the first-phase trigger.
     //         The thread sets gcmd_eng_mc_busy, fires stig_done, then loops
     //         back to wait(cmd_trigger_event) WITHOUT calling stig_finish().
     // -------------------------------------------------------------------------
     wait(STIG_SETTLE_NS, sc_core::SC_NS);
 
     // -------------------------------------------------------------------------
     // Step 6: Read phase-1 status (no flash dispatch yet — only arm chain).
     // -------------------------------------------------------------------------
     uint32_t ctrl_status_p1 = 0u;
     uint32_t intr_status_p1 = 0u;
     uint32_t cmd_status_p1  = 0u;
 
     stig_rreg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_status_p1);
     stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_p1);
     stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
     stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_status_p1);
 
     // -------------------------------------------------------------------------
     // Phase-1 assertions.
     // -------------------------------------------------------------------------
 
     // [A] gcmd_eng_mc_busy (bit 4) = 1 — chain is armed, waiting for 2nd trigger.
     bool pass_mc_p1 = ((ctrl_status_p1 & CTRL_STATUS_MC_BUSY) != 0u);
     if (!pass_mc_p1) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph1[A]: FAIL — gcmd_eng_mc_busy not set"
             << " (ctrl_status=0x" << std::hex << ctrl_status_p1 << ")";
     }
 
     // [B] stig_done (bit 23) = 1 — first-phase fires this interrupt.
     bool pass_sd_p1 = ((intr_status_p1 & INTR_STATUS_STIG_DONE) != 0u);
     if (!pass_sd_p1) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph1[B]: FAIL — stig_done not set after"
             << " INSTR_LINK first phase"
             << " (intr_status=0x" << std::hex << intr_status_p1 << ")";
     }
 
     // [B2] cmd_status.COMPLETE (bit 15) = 0 — stig_finish() not run after phase 1.
     const bool pass_no_flash_p1 =
         (m_flash_tx_per_target[0] == flash_tx0_base);
     if (!pass_no_flash_p1) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph1[B2]: FAIL — flash tx before phase-2"
             << " (tx0=" << std::dec << m_flash_tx_per_target[0]
             << " want " << flash_tx0_base << ")";
     }
 
     bool pass_complete_p1_clear = ((cmd_status_p1 & kCmdStatusComplete) == 0u);
     if (!pass_complete_p1_clear) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph1[B3]: FAIL — cmd_status COMPLETE set too early"
             << " (cmd_status=0x" << std::hex << cmd_status_p1 << ")";
     }
 
     // =========================================================================
     // Phase 2 — Second trigger, INSTR_TYPE=GLUED (0x7F) to complete the chain
     // =========================================================================
 
     // -------------------------------------------------------------------------
     // Step 7: Stage the second-phase (Glued Data Instruction).
     //
     //   Per Section 5.3.8 Rule 6 and model implementation:
     //   The stig_engine_thread forces instr_type to XSPI_INSTR_GLUED (127)
     //   when stig_instr_link_pending is true, regardless of the written
     //   cmd_reg4[6:0] value. However, we still encode 0x7F in cmd_reg4[6:0]
     //   to construct the correct Glued Data Instruction per spec table 4.29.
     //
     //   Phase2: INSTR=127, DATA=1 byte, DIR=read, INSTR_LINK=0
     // -------------------------------------------------------------------------
     const uint32_t ph2_r1 = 0x0000007Fu;
     const uint32_t ph2_r2 = 0x00010000u;  // 32b count=1
     const uint32_t ph2_r3 = 0u;
     const uint32_t ph2_r4 = 0u;           // reg4[4] DIR=0
 
     stig_stage_and_trigger(test, ph2_r1, ph2_r2, ph2_r3, ph2_r4);
 
     // -------------------------------------------------------------------------
     // Step 8: Wait for STIG engine to dispatch the Glued transaction and complete.
     // -------------------------------------------------------------------------
     wait(STIG_SETTLE_NS, sc_core::SC_NS);
 
     // -------------------------------------------------------------------------
     // Step 9: Read phase-2 status.
     // -------------------------------------------------------------------------
     uint32_t ctrl_status_p2 = 0u;
     uint32_t intr_status_p2 = 0u;
     uint32_t cmd_status_p2  = 0u;
 
     stig_rreg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_status_p2);
     stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_p2);
     stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
     stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_status_p2);
 
     // -------------------------------------------------------------------------
     // Phase-2 assertions.
     // -------------------------------------------------------------------------
 
     // [C] Exactly one combined flash transaction on socket[0] after phase 2.
     const bool pass_tx_p2 =
         (m_flash_tx_per_target[0] == flash_tx0_base + 1);
     if (!pass_tx_p2) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph2[C]: FAIL — flash tx count"
             << " (tx0=" << std::dec << m_flash_tx_per_target[0]
             << " want " << (flash_tx0_base + 1) << ")";
     }
 
     // [C2] Combined GLUED read: same command/addr as phase-1 glue (STIG_001 pattern).
     const bool pass_flash_ext =
         (m_last_flash_ext.opcode == 0x0Bu)
         && (m_last_flash_ext.address == 0x00010000ULL)
         && (m_last_flash_ext.data_bytes == 1u)
         && (m_last_flash_ext.bank_num == 0u)
         && (m_last_flash_ext.instr_type
             == static_cast<uint8_t>(XSPI_INSTR_GLUED));
     if (!pass_flash_ext) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph2[C2]: FAIL — flash ext opc=0x"
             << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
             << " addr=0x" << m_last_flash_ext.address
             << " bytes=" << std::dec << m_last_flash_ext.data_bytes
             << " ity=" << static_cast<unsigned>(m_last_flash_ext.instr_type);
     }
 
     // [D] gcmd_eng_mc_busy (bit 4) = 0 — chain has been consumed.
     bool pass_mc_p2 = ((ctrl_status_p2 & CTRL_STATUS_MC_BUSY) == 0u);
     if (!pass_mc_p2) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph2[D]: FAIL — gcmd_eng_mc_busy still set"
             << " after second trigger"
             << " (ctrl_status=0x" << std::hex << ctrl_status_p2 << ")";
     }
 
     // [E] stig_done (bit 23) = 1 — second stig_done from stig_finish().
     bool pass_sd_p2 = ((intr_status_p2 & INTR_STATUS_STIG_DONE) != 0u);
     if (!pass_sd_p2) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph2[E]: FAIL — stig_done not set after"
             << " INSTR_LINK second phase"
             << " (intr_status=0x" << std::hex << intr_status_p2 << ")";
     }
 
     // [F] cmd_status.COMPLETE (bit 15) = 1 after second trigger (stig_finish).
     const bool pass_complete_p2 = ((cmd_status_p2 & kCmdStatusComplete) != 0u);
     if (!pass_complete_p2) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_006 Ph2[F]: FAIL — cmd_status COMPLETE not set"
             << " (cmd_status=0x" << std::hex << cmd_status_p2 << ")";
     }
 
     const bool passed = pass_mc_p1 && pass_sd_p1 && pass_no_flash_p1 && pass_complete_p1_clear
                         && pass_tx_p2 && pass_flash_ext && pass_mc_p2 && pass_sd_p2
                         && pass_complete_p2;
 
     if (passed) {
         report_test_pass("TC_XSPI_STIG_006");
    } else {
        report_test_fail(
            "TC_XSPI_STIG_006",
            "INSTR_LINK chain: phase1 mc_busy+stig_done, no flash/COMPLETE; phase2 one "
            "GLUED flash on CS0, mc_busy clear, stig_done, cmd_status[15]=1");
    }
}

/******************************************************************************
 * @brief TC_XSPI_STIG_007 — After STIG completes: ctrl_status idle + cmd_status.COMPLETE
 *
 * Issues a single-phase STIG READ (1 B, opcode 0x0B @ flash address 0). Polls
 * ctrl_status(0x100) until gcmd_eng_busy (bit 3) clears, then reads cmd_status(0x044)
 * and requires hardware COMPLETE (bit 15) as merged by handle_read_cmd_status().
 * Confirms intr_status.stig_done and one flash transaction on xspi_bus_socket[0]
 * via the inbuilt flash target (no SFDP stub).
 *
 * Mismatch note: FUNC011 PIO tests treat cmd_status "COMPLETE" as indirect bit 0
 * (thread completion); STIG hardware uses register bit 15, now exposed on read.
 ******************************************************************************/
 void testbench::tc_xspi_stig_007_completion_sets_cmd_status()
 {
     report_test_start(
         "TC_XSPI_STIG_007: STIG completion — gcmd_eng_busy de-asserts + cmd_status.COMPLETE");
 
     static constexpr uint32_t kCtrlGcmdBusy      = (1u << 3);
     static constexpr uint32_t kCtrlCtrlBusy      = (1u << 7);
     static constexpr uint32_t kCmdStatusComplete = (1u << 15);
 
     apply_reset();
     func009_send_por_inhibited(por_initiator);
     wait(5, sc_core::SC_NS);
 
     stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
     wait(2, sc_core::SC_NS);
 
     m_last_flash_ext  = cdns_extension();
     m_last_flash_bank = -1;
     const int flash_tx0_before = m_flash_tx_per_target[0];
 
     // UG Profile 1: single-phase READ, 1 data byte, FAST_READ 0x0B, address 0.
     const uint32_t r1 = 0x00000001u;                       // INSTR_TYPE = READ (1)
     const uint32_t r2 = 0x00000000u;
     const uint32_t r3 = (1u << 24) | (0x0Bu << 16);        // DATA[1:0]=1 B, CMD[87:80]=0x0B
     const uint32_t r4 = 0x00000000u;                      // bank 0, INSTR_LINK=0
 
     stig_write_cmd_regs(test, r1, r2, r3, r4);
     stig_trigger_cmd0(test);
 
     bool       saw_gcmd_idle = false;
     uint32_t   ctrl          = 0u;
     for (int n = 0; n < 500; ++n) {
         stig_rreg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl);
         if ((ctrl & kCtrlGcmdBusy) == 0u) {
             saw_gcmd_idle = true;
             break;
         }
         wait(1, sc_core::SC_NS);
     }
 
     uint32_t intr = 0u;
     stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr);
 
     stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
     uint32_t cstat = 0u;
     stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cstat);
 
     const bool pass_ctrl_idle = saw_gcmd_idle && ((ctrl & kCtrlCtrlBusy) == 0u);
     const bool pass_done      = ((intr & INTR_STATUS_STIG_DONE) != 0u);
     const bool pass_complete  = ((cstat & kCmdStatusComplete) != 0u);
     const bool pass_flash =
         (m_flash_tx_per_target[0] == flash_tx0_before + 1)
         && (m_last_flash_ext.opcode == 0x0Bu)
         && (m_last_flash_ext.data_bytes == 1u)
         && (m_last_flash_ext.bank_num == 0u);
 
     if (!pass_ctrl_idle) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_007: ctrl_status gcmd_eng_busy poll failed; ctrl=0x"
             << std::hex << ctrl;
     }
     if (!pass_done) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_007: intr_status.stig_done not set; intr=0x" << std::hex << intr;
     }
     if (!pass_complete) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_007: cmd_status COMPLETE (bit15) not set; cmd_status=0x"
             << std::hex << cstat;
     }
     if (!pass_flash) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_007: flash socket[0] mismatch opc=0x"
             << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
             << " bytes=" << std::dec << m_last_flash_ext.data_bytes
             << " tx0=" << m_flash_tx_per_target[0];
     }
 
     const bool passed = pass_ctrl_idle && pass_done && pass_complete && pass_flash;
     if (passed) {
         report_test_pass("TC_XSPI_STIG_007");
     } else {
         report_test_fail(
             "TC_XSPI_STIG_007",
             "Expected STIG READ completion: ctrl_status.gcmd_eng_busy=0, cmd_status[15]=1, "
             "intr_status.stig_done, and one 0x0B READ on flash target 0");
     }
 }


// =============================================================================
// stig_write_read_test — STIG glued program / read + data integrity (func009)
// =============================================================================
//
// Requested API names (this file):
//   build_stig_cmd     → stig_cmd_build_wren_ug, stig_cmd_build_addr_common,
//                        stig_glue_set_data_n_79_48 (UG Profile 1, not ACMD DATA_CNT)
//   trigger_stig       → stig_trigger_cmd0 (file), alias trigger_stig() below
//   wait_for_complete  → stig_wait_stig_done_settle (polls intr_status.stig_done[23];
//                        cmd_status.COMPLETE is not observable; see file header)
//   handle_sdma_write / read → t_axi to `k_func009_sdma_axi_base` (0xA000_0000).
//
namespace {
/// Fills host program buffer (same 0xABCD1234… layout as the former LT fixed pattern).
static uint8_t lte_merged_pgm_byte(uint32_t n_bytes, uint32_t i)
{
    const uint32_t k_pgm = 0xABCD1234u;
    if (i >= n_bytes) {
        return 0u;
    }
    const uint32_t shift = 8u * (n_bytes - 1u - i);
    return static_cast<uint8_t>((k_pgm >> shift) & 0xFFu);
}

/// UG Table 4.27: 32b count in bits[79:48] → r2[31:16] low, r3[15:0] high.
static void stig_glue_set_data_n_79_48(uint32_t nbytes, uint32_t& r2, uint32_t& r3)
{
    r2 = (nbytes & 0xFFFFu) << 16;
    r3 = (nbytes >> 16) & 0xFFFFu;
}

static void stig_cmd_build_wren_ug(uint32_t& r1, uint32_t& r2, uint32_t& r3, uint32_t& r4)
{
    r1 = 0x00000000u;
    r2 = 0u;
    r3 = 0x00060000u;  // CMD(0x06) in [23:16]
    r4 = 0u;
}

static void stig_cmd_build_addr_common(uint32_t phys_addr, uint32_t& r1, uint32_t& r2, uint32_t& r3,
                                      uint32_t& r4,
                                      uint8_t  cmd_opcode,
                                      uint8_t  instr7,
                                      bool     instr_link)
{
    const uint8_t a0  = static_cast<uint8_t>(phys_addr & 0xFFu);
    const uint8_t a1  = static_cast<uint8_t>((phys_addr >> 8) & 0xFFu);
    const uint8_t a2  = static_cast<uint8_t>((phys_addr >> 16) & 0xFFu);
    const uint8_t a3a = static_cast<uint8_t>((phys_addr >> 24) & 0xFFu);
    const uint8_t a4a = 0u;
    const uint8_t a5a = 0u;  // 32-bit phys in test
    r1  = (static_cast<uint32_t>(instr7) & 0x7Fu) | (static_cast<uint32_t>(a0) << 24);
    r2  = static_cast<uint32_t>(a1) | (static_cast<uint32_t>(a2) << 8)
         | (static_cast<uint32_t>(a3a) << 16) | (static_cast<uint32_t>(a4a) << 24);
    r3  = (static_cast<uint32_t>(R3_ADDRNO3_DATANO0) << 24) | (static_cast<uint32_t>(cmd_opcode) << 16)
         | (static_cast<uint32_t>(a5a));
    r4 = (instr_link ? 1u : 0u) << 28;
}

static void stig_wait_stig_done_settle(xspi_ctrl_test* test, int settle_ns, const char* /*tag*/)
{
    wait(settle_ns, sc_core::SC_NS);
    uint32_t v = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, v);
    for (int k = 0; k < 20 && (v & INTR_STATUS_STIG_DONE) == 0u; ++k) {
        wait(5, sc_core::SC_NS);
        stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, v);
    }
}

static void trigger_stig(xspi_ctrl_test* t) { stig_trigger_cmd0(t); }

static void wait_for_stig_done_or_settle(xspi_ctrl_test* test, int primary_wait_ns = 0)
{
    stig_wait_stig_done_settle(
        test, primary_wait_ns > 0 ? primary_wait_ns : STIG_SETTLE_NS, "wait_for");
}

}  // namespace

/******************************************************************************
 * stig_write_read_test — WREN, glued page program, glued fast read, SDMA compare.
 * DATA length for glue from bits[79:48] (not ACMD DATA_CNT). INSTR 0/1/127 as per
 * UG. Integrity: program bytes on AXI write to SDMA window, read back on AXI.
 ******************************************************************************/
void testbench::stig_write_read_test()
{
    report_test_start("stig_write_read_test: STIG program/read integrity (glued, SDMA)");

    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    const uint32_t phys  = 0x00020000u;
    const uint32_t nbyte = 32u;
    uint8_t        wbuf[32] = {};
    uint8_t        rbuf[32] = {};
    for (uint32_t i = 0; i < nbyte; ++i) {
        wbuf[i] = lte_merged_pgm_byte(nbyte, i);
    }

    CSML_INFO(2, func009_logger) << "stig_write_read_test: phys_addr=0x" << std::hex << phys
                                  << " glue_bytes=" << std::dec << nbyte;

    stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    wait(2, sc_core::SC_NS);

    {
        uint32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0;
        stig_cmd_build_wren_ug(r1, r2, r3, r4);
        stig_stage_and_trigger(test, r1, r2, r3, r4);
    }
    wait_for_stig_done_or_settle(test, STIG_SETTLE_NS);
    uint32_t in0    = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, in0);
    const bool wren_ok = (in0 & INTR_STATUS_STIG_DONE) != 0u;
    if (wren_ok) {
        stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    }
    wait(2, sc_core::SC_NS);

    {
        uint32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0;
        stig_cmd_build_addr_common(phys, r1, r2, r3, r4, 0x02u, 0u, true);
        stig_write_cmd_regs(test, r1, r2, r3, r4);
        trigger_stig(test);
    }
    stig_wait_stig_done_settle(test, STIG_SETTLE_NS, "pp_ph1");
    stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    wait(2, sc_core::SC_NS);

    {
        uint32_t g2 = 0, g3 = 0;
        stig_glue_set_data_n_79_48(nbyte, g2, g3);
        const uint32_t p2_r1 = 0x0000007Fu;
        const uint32_t p2_r4 = 0x10u;  // DIR=1 write
        stig_write_cmd_regs(test, p2_r1, g2, g3, p2_r4);
        stig_trigger_cmd0(test);
    }
    func009_wait_sdma_trigg(test);
    {
        const tlm::tlm_response_status sdx = func009_sdma_axi_write(
            t_axi_slave_initiator, k_func009_sdma_axi_base, wbuf, nbyte);
        if (sdx != tlm::TLM_OK_RESPONSE) {
            CSML_ERROR(0, func009_logger) << "stig_write_read: SDMA program AXI write status "
                                          << static_cast<int>(sdx);
        }
    }
    stig_wait_stig_done_settle(test, 80, "pp_done");
    in0     = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, in0);
    const bool pp_ok = (in0 & INTR_STATUS_STIG_DONE) != 0u;
    if (pp_ok) {
        stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    }
    wait(2, sc_core::SC_NS);

    {
        uint32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0;
        stig_cmd_build_addr_common(phys, r1, r2, r3, r4, 0x0Bu, 1u, true);
        stig_write_cmd_regs(test, r1, r2, r3, r4);
        stig_trigger_cmd0(test);
    }
    stig_wait_stig_done_settle(test, STIG_SETTLE_NS, "read_ph1");
    stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    wait(2, sc_core::SC_NS);

    {
        uint32_t g2 = 0, g3 = 0;
        stig_glue_set_data_n_79_48(nbyte, g2, g3);
        const uint32_t p2_r1 = 0x0000007Fu;
        stig_write_cmd_regs(test, p2_r1, g2, g3, 0u);
        stig_trigger_cmd0(test);
    }
    stig_wait_stig_done_settle(test, 80, "read_done");
    func009_wait_sdma_trigg(test);
    {
        const tlm::tlm_response_status sdr = func009_sdma_axi_read(
            t_axi_slave_initiator, k_func009_sdma_axi_base, rbuf, nbyte);
        if (sdr != tlm::TLM_OK_RESPONSE) {
            CSML_ERROR(0, func009_logger) << "stig_write_read: SDMA read AXI read status "
                                          << static_cast<int>(sdr);
        }
    }
    in0 = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, in0);
    const bool read_done = (in0 & INTR_STATUS_STIG_DONE) != 0u;

    bool        cmp_ok  = true;
    std::size_t mis_idx = 0u;
    for (uint32_t i = 0; cmp_ok && i < nbyte; ++i) {
        if (rbuf[i] != wbuf[i]) {
            cmp_ok  = false;
            mis_idx = i;
        }
    }

    if (!cmp_ok) {
        const uint8_t eexp = wbuf[mis_idx];
        const uint8_t aact = rbuf[mis_idx];
        CSML_ERROR(0, func009_logger) << "stig_write_read_test: SDMA mismatch at offset " << std::dec
                                      << mis_idx << " exp=0x" << std::hex
                                      << static_cast<unsigned>(eexp) << " got=0x"
                                      << static_cast<unsigned>(aact);
    } else {
        CSML_INFO(1, func009_logger) << "stig_write_read_test: " << std::dec << nbyte
                                       << " B AXI readback match.";
    }

    const bool pass    = wren_ok && pp_ok && read_done && cmp_ok;
    if (!wren_ok)  { CSML_ERROR(0, func009_logger) << "stig_write_read_test: WREN stig_done"; }
    if (!pp_ok)    { CSML_ERROR(0, func009_logger) << "stig_write_read_test: program stig_done"; }
    if (!read_done) { CSML_ERROR(0, func009_logger) << "stig_write_read_test: read stig_done"; }

    report_test_result("stig_write_read_test", pass);
}


// =============================================================================
// TC_XSPI_STIG_005 — STIG READ_SFDP handler (opcode 0x5A @ address 0, 16 B)
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_005 — STIG READ_SFDP handler
 *
 * Stages a single-phase READ_SFDP per UG Profile 1: INSTR_TYPE = XSPI_INSTR_SFDP (96)
 * in cmd_reg1[6:0], CMD = 0x5A in cmd_reg3[23:16], flash address 0 from ADDR fields,
 * bank 0 in cmd_reg4[14:12]. decode_instruction() forces data_bytes_count = 16 for
 * SFDP. Confirms one flash read on xspi_bus_socket[0] (inbuilt target), opcode 0x5A,
 * address 0, 16 bytes, instr_type SFDP on the extension, cmd_status[15] COMPLETE,
 * and intr_status.stig_done.
 *
 * Mismatch note: Some RTL docs place READ_SFDP opcode in cmd_reg1[31:24]; this DUT
 * decodes CMD from cmd_reg3[23:16] like other STIG profiles (see decode_instruction()).
 ******************************************************************************/
 void testbench::tc_xspi_stig_005_read_sfdp_handler()
 {
     report_test_start("TC_XSPI_STIG_005: STIG READ_SFDP handler (0x5A @0, 16 B)");
 
     static constexpr uint32_t kCmdStatusComplete = (1u << 15);
 
     apply_reset();
     func009_send_por_inhibited(por_initiator);
     wait(5, sc_core::SC_NS);
 
     stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
     wait(2, sc_core::SC_NS);
 
     m_last_flash_ext       = cdns_extension();
     m_last_flash_bank      = -1;
     const int flash_tx0_before = m_flash_tx_per_target[0];
 
     // cmd_reg1[6:0] = INSTR_TYPE 96 (READ_SFDP). Address 0: ADDR0..5 = 0.
     const uint32_t r1 = static_cast<uint32_t>(XSPI_INSTR_SFDP) & 0x7Fu;  // 0x60
     const uint32_t r2 = 0x00000000u;
     const uint32_t r3 = (0x5Au << 16);  // CMD[87:80] = READ_SFDP; low address bytes 0
     const uint32_t r4 = 0x00000000u;    // bank 0, INSTR_LINK=0
 
     stig_write_cmd_regs(test, r1, r2, r3, r4);
     stig_trigger_cmd0(test);
     wait(STIG_SETTLE_NS, sc_core::SC_NS);
 
     uint32_t intr = 0u;
     stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr);
 
     stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
     uint32_t cstat = 0u;
     stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cstat);
 
     const bool pass_tx   = (m_flash_tx_per_target[0] == flash_tx0_before + 1);
     const bool pass_opc  = (m_last_flash_ext.opcode == 0x5Au);
     const bool pass_addr = (m_last_flash_ext.address == 0x00000000ULL);
     const bool pass_len  = (m_last_flash_ext.data_bytes == 16u);
     const bool pass_bank = (m_last_flash_ext.bank_num == 0u);
     const bool pass_ity =
         (m_last_flash_ext.instr_type == static_cast<uint8_t>(XSPI_INSTR_SFDP));
     const bool pass_done = ((intr & INTR_STATUS_STIG_DONE) != 0u);
     const bool pass_cmp  = ((cstat & kCmdStatusComplete) != 0u);
 
     if (!pass_tx) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_005: expected 1 flash tx on target 0, tx0="
             << m_flash_tx_per_target[0];
     }
     if (!pass_opc || !pass_addr || !pass_len || !pass_bank || !pass_ity) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_005: flash ext opc=0x" << std::hex
             << static_cast<unsigned>(m_last_flash_ext.opcode)
             << " addr=0x" << m_last_flash_ext.address
             << " bytes=" << std::dec << m_last_flash_ext.data_bytes
             << " bank=" << m_last_flash_ext.bank_num
             << " ity=" << static_cast<unsigned>(m_last_flash_ext.instr_type);
     }
     if (!pass_done) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_005: stig_done not set; intr=0x" << std::hex << intr;
     }
     if (!pass_cmp) {
         CSML_ERROR(0, func009_logger)
             << "TC_XSPI_STIG_005: cmd_status COMPLETE (15) not set; cstat=0x"
             << std::hex << cstat;
     }
 
     const bool passed = pass_tx && pass_opc && pass_addr && pass_len && pass_bank
                         && pass_ity && pass_done && pass_cmp;
     if (passed) {
         report_test_pass("TC_XSPI_STIG_005");
     } else {
         report_test_fail(
             "TC_XSPI_STIG_005",
             "READ_SFDP STIG: flash 0x5A @0 len 16 on CS0, instr_type SFDP, stig_done, cmd_status[15]");
     }
 }

 
 void testbench::tc_xspi_stig_009_suspend_resume_handler()
{
    report_test_start(
        "TC_XSPI_STIG_009: STIG suspend/resume — opcodes from SFDP DWORD 13 path");

    // SFDP DWORD 13 (param_table[12]) supplies erase suspend/resume into
    // stat_seq_cfg_8. PoR discovery issues READ_SFDP on xspi_bus_socket[0] →
    // flash_trans_prelude → xspi_flash_target[0] SFDP ROM (no m_flash_sfdp_mode stub).
    //
    // DWORD 13 opcodes are programmed via jedec_basic_table_t + update_sfdp_rom().
    // Erase-type-1 for configure_registers_from_sfdp is read from param_table[6]
    // with opcode in bits[23:16] and size in bits[15:8] (see xspi_ctrl.cpp); that
    // packing differs from dword_8_t LE layout, so we patch ROM dword index 6 only.

    static constexpr uint32_t kEraseParamDword6Le = 0x00D81200u;  // size 2^0x12, opcode 0xD8
    static constexpr uint32_t kSfdpBasicTableBase = 0x80u;
    static constexpr uint32_t kExpectedStat8 =
        (static_cast<uint32_t>(0xB0u) << 24)
        | (static_cast<uint32_t>(0x7Au) << 16);

    apply_reset();
    wait(5, sc_core::SC_NS);

    if (xspi_flash_target.empty()) {
        report_test_fail("TC_XSPI_STIG_009",
                         "no xspi_flash_target[0] — cannot run SFDP-on-socket path");
        return;
    }

    xspi_target_model& flash0 = xspi_flash_target[0]->flash_model();
    xspi_sfdp::jedec_basic_table_t* bt   = flash0.get_basic_table();
    bt->set_suspend_resume_opcodes(0xB0u, 0x7Au, 0x75u, 0xFCu);
    flash0.update_sfdp_rom();

    {
        std::vector<uint8_t> le6(4u);
        le6[0] = static_cast<uint8_t>(kEraseParamDword6Le & 0xFFu);
        le6[1] = static_cast<uint8_t>((kEraseParamDword6Le >> 8) & 0xFFu);
        le6[2] = static_cast<uint8_t>((kEraseParamDword6Le >> 16) & 0xFFu);
        le6[3] = static_cast<uint8_t>((kEraseParamDword6Le >> 24) & 0xFFu);
        flash0.get_sfdp_rom()->load(kSfdpBasicTableBase + 6u * 4u, le6);
    }

    func009_send_por_discovery_sfdp(por_initiator);
    wait(20, sc_core::SC_NS);

    uint32_t stat8 = 0u;
    stig_rreg(test, xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET, stat8);
    const uint8_t ers_susp = static_cast<uint8_t>((stat8 >> 24) & 0xFFu);
    const uint8_t ers_res  = static_cast<uint8_t>((stat8 >> 16) & 0xFFu);
    const bool pass_cfg = (stat8 == kExpectedStat8)
                          && (ers_susp == 0xB0u) && (ers_res == 0x7Au);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    auto run_suspend_resume = [&](uint8_t instr, uint8_t expected_opc,
                                  const char* tag) -> bool {
        m_last_flash_ext      = cdns_extension();
        m_flash_stub_tx_count = 0u;
        m_last_flash_bank     = -1;

        const int flash0_before = m_flash_tx_per_target[0];

        const uint32_t r1 = static_cast<uint32_t>(instr);
        const uint32_t r2 = 0u;
        const uint32_t r3 = static_cast<uint32_t>(expected_opc) << 16;
        const uint32_t r4 = 0u;
        stig_stage_and_trigger(test, r1, r2, r3, r4);
        wait(STIG_SETTLE_NS, sc_core::SC_NS);

        uint32_t intr = 0u;
        stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr);

        // cmd_status TLM read is volatile-indirect (PIO/ACMD thread projection);
        // STIG still sets hardware cmd_status.COMPLETE in stig_finish() — the
        // observable LT contract is intr_status.stig_done (bit 23).
        uint32_t cmd_stat = 0u;
        stig_wreg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0u);
        stig_rreg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_stat);

        const bool ok_socket0 = (m_flash_tx_per_target[0] == flash0_before + 1);
        const bool ok_flash = (m_last_flash_ext.opcode == expected_opc)
                              && (m_last_flash_ext.data_bytes == 0u)
                              && (m_last_flash_bank == 0);
        const bool ok_done  = ((intr & INTR_STATUS_STIG_DONE) != 0u);
        if (!ok_socket0 || !ok_flash || !ok_done) {
            CSML_ERROR(0, func009_logger)
                << "STIG_009 " << tag << " FAIL flash0_tx=" << std::dec
                << m_flash_tx_per_target[0] << " (want " << (flash0_before + 1)
                << ") opc=0x" << std::hex << static_cast<unsigned>(m_last_flash_ext.opcode)
                << " intr=0x" << intr << " cmd_status_volatile=0x" << cmd_stat;
        }
        return ok_socket0 && ok_flash && ok_done;
    };

    const bool pass_susp = run_suspend_resume(kStigEraseSuspendInstr, ers_susp, "SUSPEND");

    // W1C stig_done so the RESUME phase proves a fresh completion edge.
    stig_wreg(test, xspi_ctrl_basetest::intr_status_OFFSET, INTR_STATUS_STIG_DONE);
    wait(2, sc_core::SC_NS);

    const bool pass_res  = run_suspend_resume(kStigEraseResumeInstr, ers_res, "RESUME");

    const bool passed = pass_cfg && pass_susp && pass_res;

    if (passed) {
        report_test_pass("TC_XSPI_STIG_009");
    } else {
        report_test_fail("TC_XSPI_STIG_009",
                          "SFDP DWORD13→stat_seq_cfg_8 or suspend/resume STIG dispatch failed");
    }
}


// =============================================================================
// TC_XSPI_STIG_010 — Single-phase inline WRITE → handle_stig_write()
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_STIG_010 — STIG WRITE (Profile 1) without INSTR_LINK
 *
 * Stages cmd_reg1–4 so decode_instruction() yields instr_type=XSPI_INSTR_WRITE (2),
 * triggering execute_stig() → handle_stig_write() (WREN then PAGE_PROGRAM with
 * inline payload from cmd_reg1[15:8] and [23:16]). This path is distinct from
 * TC_XSPI_STIG_002, which uses INSTR_LINK + handle_stig_merged_write().
 *
 * Encoding: flash address 0x3000; PAGE_PROGRAM opcode 0x02 in cmd_reg3[23:16];
 * DATA[1:0] at cmd_reg3[25:24] = 2 bytes; inline data 0xAB, 0xCD in cmd_reg1.
 ******************************************************************************/
void testbench::tc_xspi_stig_010_single_phase_inline_write()
{
    report_test_start(
        "TC_XSPI_STIG_010: STIG single-phase inline WRITE (handle_stig_write)");

    apply_reset();
    func009_send_por_inhibited(por_initiator);
    wait(5, sc_core::SC_NS);

    stig_wreg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, CTRL_CFG_STIG_MODE);
    wait(2, sc_core::SC_NS);

    m_flash_stub_tx_count = 0u;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    // INSTR_TYPE=WRITE(2); ADDR0=0; inline DATA0/DATA1 in [15:8],[23:16]
    const uint32_t r1 = 0x00CDAB02u;
    // ADDR1=0x30, ADDR2..4=0 → physical 0x3000
    const uint32_t r2 = 0x00000030u;
    // ADDR_NO=3 pattern (0x30) with [25:24]=2 data bytes; CMD=0x02 at [23:16]
    const uint32_t r3 = ((R3_ADDRNO3_DATANO0 | 0x02u) << 24) | (0x02u << 16);
    const uint32_t r4 = 0x00000000u;  // bank 0, INSTR_LINK=0

    stig_write_cmd_regs(test, r1, r2, r3, r4);
    stig_trigger_cmd0(test);

    wait(50, sc_core::SC_NS);

    uint32_t intr_status_val = 0u;
    stig_rreg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);

    const bool pass_tx  = (m_flash_stub_tx_count == 2u);
    const bool pass_opc = (m_last_flash_ext.opcode == 0x02u);
    const bool pass_ity = (m_last_flash_ext.instr_type
                           == static_cast<uint8_t>(XSPI_INSTR_WRITE));
    const bool pass_addr = (m_last_flash_ext.address == 0x00003000ULL);
    const bool pass_dlen = (m_last_flash_ext.data_bytes == 2u);
    const bool pass_done = ((intr_status_val & INTR_STATUS_STIG_DONE) != 0u);

    if (!pass_tx) {
        CSML_ERROR(0, func009_logger)
            << "TC_XSPI_STIG_010 [A] expected 2 flash tx (WREN+PP), got "
            << m_flash_stub_tx_count;
    }
    if (!pass_opc) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_010 [B] last opcode";
    }
    if (!pass_ity) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_010 [C] instr_type";
    }
    if (!pass_addr) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_010 [D] address";
    }
    if (!pass_dlen) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_010 [E] data_bytes";
    }
    if (!pass_done) {
        CSML_ERROR(0, func009_logger) << "TC_XSPI_STIG_010 [F] stig_done";
    }

    report_test_result("TC_XSPI_STIG_010",
                       pass_tx && pass_opc && pass_ity && pass_addr && pass_dlen
                           && pass_done);
}


// =============================================================================
// run_func009_tests — top-level entry point for FUNC_XSPI_009 suite
// =============================================================================

/******************************************************************************
 * @brief run_func009_tests — FUNC_XSPI_009: STIG Mode Command Execution
 *
 * TC_001, 002, 003, 004, 006 plus stig_write_read_test (glued program/read integrity).
 ******************************************************************************/
void testbench::run_func009_tests()
{
    CSML_INFO(1, logger) << "\n"
        << "================================================\n"
        << "  FUNC_XSPI_009: STIG Mode Command Execution\n"
        << "  (001, 002, 003, 004, 006, 005, 007, 009, stig_write_read_test)\n"
        << "================================================";

    tc_xspi_stig_001_read_handler();
    tc_xspi_stig_002_write_handler();
    tc_xspi_stig_003_control_wren();
    tc_xspi_stig_004_control_rdsr();
    tc_xspi_stig_006_instr_link_two_phase_chain();
    stig_write_read_test();
    tc_xspi_stig_005_read_sfdp_handler();
    tc_xspi_stig_007_completion_sets_cmd_status();
    tc_xspi_stig_009_suspend_resume_handler();
    tc_xspi_stig_010_single_phase_inline_write();
    CSML_INFO(1, logger)
        << "================================================\n"
        << "  FUNC_XSPI_009 suite complete (9 test cases)\n"
        << "================================================";
}
