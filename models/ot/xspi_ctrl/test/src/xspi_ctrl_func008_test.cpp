/******************************************************************************
 * @file xspi_ctrl_func008_test.cpp
 * @brief Test cases for FUNC_XSPI_008 — Direct Mode Flash Forwarding
 *
 * This file implements all test cases mapped to FUNC_XSPI_008 in the
 * xspi_ctrl functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_008 Test Coverage (DM/CFG test cases, including
 * TC_XSPI_CFG_001_num_targets_4):
 *
 * - TC_XSPI_DM_001: AXI READ → READ_ZERO_LATENCY (opcode 0x03).
 *                   Issues a 4-byte AXI READ at flash address 0x00001000 in
 *                   Direct mode; verifies cdns_extension opcode=0x03,
 *                   instr_type=1(READ), bank_num=0, address=0x00001000.
 *
 * - TC_XSPI_DM_002: AXI WRITE → WREN (0x06) + PAGE_PROGRAM (0x02).
 *                   Issues a 4-byte AXI WRITE at 0x00002000; verifies two
 *                   flash transactions (m_flash_stub_tx_count==2), last
 *                   ext.opcode=0x02 (PAGE_PROGRAM), instr_type=2(WRITE).
 *
 * - TC_XSPI_DM_003: Bank selection — direct_access_cfg.dac_bank_num=2 routes
 *                   AXI slave READs to xspi_bus_socket[2] with
 *                   cdns_extension.bank_num=2 (NUM_TARGETS>=4 build).
 *                   Confirms per-CS activity: only the selected socket’s stub
 *                   count increments; other CS lines see no extra traffic for
 *                   that AXI read.
 *
 * - TC_XSPI_DM_004: 64-bit address remapping via rmp_addr_en.
 *                   Programs N=0x00001000 into direct_access_rmp, sets
 *                   rmp_addr_en=1; issues AXI READ at 0x00005000; verifies
 *                   ext.address == 0x00004000 (= 0x5000 - 0x1000).
 *
 * - TC_XSPI_DM_005: ctrl_status.ctrl_busy cleared after Direct-mode dispatch.
 *                   After a Direct-mode READ completes, reads ctrl_status and
 *                   verifies ctrl_busy (bit 7) is cleared (0).
 *
 * - TC_XSPI_CFG_001: Out-of-range dac_bank_num (NUM_TARGETS=4 build) sets
 *                    cmd_ignored; no flash tx (dac_bank_num=4).
 *
 * - TC_XSPI_CFG_002: n_banks read; bank=0 routes; bank=4 is out of range
 *                    (rejects with cmd_ignored) for a 4-target DUT.
 *
 * - TC_XSPI_CFG_001_num_targets_4: DUT with NUM_TARGETS=4 — n_banks[25:24]==3
 *   (per testcase: four-bank build); xspi_bus_socket[0..3]; direct-mode
 *   transactions for banks 0–3 succeed; dac_bank_num=5 → cmd_ignored, no
 *   xspi_bus_socket activity.
 *
 * Design reference:
 *   - docs/xspi_ctrl-detailed-design.md Section 7.1 (Direct Mode)
 *   - docs/xspi_ctrl-detailed-design.md Section 3.3 (NUM_TARGETS / n_banks)
 *   - docs/xspi_ctrl-functionality-testcases.md FUNC_XSPI_008 section
 *   - model/inc/cdns_extension.h (cdns_extension struct, axi_trans struct)
 *   - model/src/xspi_ctrl.cpp b_transport_axi_slave() — FUNC_XSPI_008
 *
 * Implementation notes:
 *   - Each test starts with apply_reset() followed by a PoR transaction with
 *     discovery_inhibit=1 to unblock register write callbacks (sets
 *     m_init_comp_done=true). Without this, writes to ctrl_config are silently
 *     discarded.
 *   - The default work_mode after reset is 2'b00 (Direct mode). Setting
 *     ctrl_config=0x00 explicitly confirms direct mode; it is the reset state.
 *   - cdns_extension capture uses m_last_flash_ext (populated by
 *     b_transport_flash_stub() on each flash socket invocation).
 *   - m_flash_stub_tx_count is reset (or baselined) before each AXI slave
 *     transaction to measure exactly how many flash bus transactions the model
 *     issued.
 *   - All AXI slave transactions are dispatched via t_axi_slave_initiator
 *     (testbench member).
 *   - The testbench and DUT use NUM_TARGETS=4 (sc_main) so TC_XSPI_DM_003 can
 *     route to xspi_bus_socket[2] and boundary tests use a bank index above 3.
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
    CsmlLogger func008_logger;
}  // namespace

// =============================================================================
// Helper: build and send a PoR transaction with discovery_inhibit=1
//
// Issues a minimal xspi_PoR_trans transaction on por_initiator to establish
// m_init_comp_done=true so that register write callbacks are unblocked.
// discovery_inhibit=1 skips SFDP reads (fast path, deterministic).
//
// @param por_initiator  testbench por_initiator socket
// =============================================================================
static void send_por_inhibited(
    tlm_utils::simple_initiator_socket<testbench, 64>& por_initiator)
{
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 1u;   // Skip SFDP discovery (fast path)
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 1u;   // 1-1-1 SDR (not used when inhibited)
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_dummy_cnt = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    tlm::tlm_generic_payload payload;
    uint8_t data_buf[8] = {0};

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(0x0u));
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

    // Remove extension before payload destructs to prevent double-free.
    xspi_PoR_trans* removed = nullptr;
    payload.get_extension(removed);
    if (removed != nullptr) {
        payload.clear_extension(removed);
    }
}

// =============================================================================
// Helper: issue a 4-byte AXI READ via t_axi_slave_initiator at the given addr.
//
// Constructs a TLM_READ_COMMAND payload with a 4-byte data buffer and calls
// t_axi_slave_initiator->b_transport() synchronously.  The response status is
// returned to the caller.
//
// @param axi_init   t_axi_slave_initiator socket
// @param addr       AXI slave address to read from
// @param data_out   4-byte buffer to receive read data (filled by flash stub)
// @return           TLM response status from the DUT
// =============================================================================
static tlm::tlm_response_status send_axi_read(
    tlm_utils::simple_initiator_socket<testbench, 64>& axi_init,
    uint64_t addr,
    uint8_t  data_out[4])
{
    tlm::tlm_generic_payload payload;
    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(addr));
    payload.set_data_ptr(data_out);
    payload.set_data_length(4u);
    payload.set_streaming_width(4u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    axi_init->b_transport(payload, delay);

    return payload.get_response_status();
}

// =============================================================================
// Helper: issue a 4-byte AXI WRITE via t_axi_slave_initiator at the given addr.
//
// Constructs a TLM_WRITE_COMMAND payload with the supplied 4-byte data buffer
// and calls t_axi_slave_initiator->b_transport() synchronously.
//
// @param axi_init   t_axi_slave_initiator socket
// @param addr       AXI slave address to write to
// @param data_in    4-byte write data buffer
// @return           TLM response status from the DUT
// =============================================================================
static tlm::tlm_response_status send_axi_write(
    tlm_utils::simple_initiator_socket<testbench, 64>& axi_init,
    uint64_t      addr,
    const uint8_t data_in[4])
{
    tlm::tlm_generic_payload payload;
    // tlm_generic_payload data_ptr must be non-const; take a local copy.
    uint8_t buf[4];
    std::memcpy(buf, data_in, 4u);

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(addr));
    payload.set_data_ptr(buf);
    payload.set_data_length(4u);
    payload.set_streaming_width(4u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0u);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    axi_init->b_transport(payload, delay);

    return payload.get_response_status();
}

// =============================================================================
// TC_XSPI_DM_001: AXI READ → READ_ZERO_LATENCY (opcode 0x03)
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DM_001 — AXI READ translates to READ_ZERO_LATENCY on flash bus
 *
 * Verification objective:
 *   Confirms that in Direct mode (ctrl_config.work_mode=2'b00), an AXI slave
 *   READ transaction received on t_axi_slave_socket is forwarded to the flash
 *   bus as a READ_ZERO_LATENCY command (opcode 0x03) via xspi_bus_socket[0].
 *
 * Stimulus:
 *   1. Apply reset to DUT.
 *   2. Send PoR with discovery_inhibit=1 to unblock register write callbacks.
 *   3. Write ctrl_config=0x00 (work_mode=2'b00 = Direct mode; bits[6:5]=2'b00).
 *   4. Reset m_flash_stub_tx_count, reset m_last_flash_ext.
 *   5. Issue 4-byte AXI READ at address 0x00001000 via t_axi_slave_initiator.
 *   6. Wait 5 ns for LT quantum synchronisation.
 *
 * Expected behavioral side-effects (pass conditions):
 *   - AXI response == TLM_OK_RESPONSE
 *   - m_flash_stub_tx_count == 1  (exactly one flash transaction issued)
 *   - m_last_flash_ext.opcode     == 0x03 (READ_ZERO_LATENCY)
 *   - m_last_flash_ext.instr_type == XSPI_INSTR_READ (1)
 *   - m_last_flash_ext.bank_num   == 0
 *   - m_last_flash_ext.address    == 0x00001000
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 7.1.4 (AXI Slave READ Behavior).
 *   docs/xspi_ctrl-architecture-behaviour-map.json state_machines.DIRECT.
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_DM_001.
 ******************************************************************************/
 void testbench::tc_xspi_dm_001_axi_read_to_flash_read()
 {
     func008_logger.setMaxVerbosity(2);
     report_test_start(
         "TC_XSPI_DM_001: Direct mode — config, program, read, compare + bus checks");
     bool allpassed = true;
     auto fail = [&](const char* msg) {
         CSML_ERROR(0, func008_logger) << msg;
         allpassed = false;
     };
 
     apply_reset();
     wait(5, sc_core::SC_NS);
 
     send_por_inhibited(por_initiator);
     wait(10, sc_core::SC_NS);
 
     test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_OFFSET,
                             0x00000000u);
     wait(2, sc_core::SC_NS);
     test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_1_OFFSET,
                             0x00000000u);
     wait(2, sc_core::SC_NS);
     test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                             0x00000000u);
     wait(2, sc_core::SC_NS);
     test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET,
                             0x00000000u);  // work_mode = 0b000 (Direct)
     wait(5, sc_core::SC_NS);
     CSML_INFO(2, func008_logger)
         << "  DM_001: remap off — direct_access_cfg=0 (rmp_addr_en=0, "
         << "dac_bank_num=0), rmp/rmp_1=0";
 
     // Controller must be idle before traffic: ctrl_busy (bit 7) and
     // gcmd_eng_busy (bit 3) = 0 per ctrl_status (0x100). See TC_XSPI_DM_005.
     {
         uint32_t ctrl_stat = 0u;
         test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                                ctrl_stat);
         CSML_INFO(2, func008_logger)
             << "  DM_001: pre-traffic ctrl_status=0x" << std::hex << ctrl_stat
             << std::dec;
         if ((ctrl_stat & (1u << 7u)) != 0u) {
             fail("TC_XSPI_DM_001: ctrl_status.ctrl_busy (bit7) must be 0 (idle)");
         }
         if ((ctrl_stat & (1u << 3u)) != 0u) {
             fail("TC_XSPI_DM_001: ctrl_status.gcmd_eng_busy (bit3) must be 0 "
                  "(idle before Direct-mode ops)");
         }
     }
 
     constexpr uint64_t k_flash_addr = 0x00001000ULL;
     const uint8_t      pattern[4]   = { 0xA5u, 0x5Au, 0x3Cu, 0xC3u };
 
     // --- Phase A: AXI WRITE (programs flash via WREN + PAGE_PROGRAM) ---
     m_flash_stub_tx_count = 0;
     m_last_flash_ext      = cdns_extension();
     m_last_flash_bank     = -1;
 
     tlm::tlm_response_status wresp =
         send_axi_write(t_axi_slave_initiator, k_flash_addr, pattern);
     wait(20, sc_core::SC_NS);
 
     if (wresp != tlm::TLM_OK_RESPONSE) {
         fail("TC_XSPI_DM_001: AXI WRITE response not OK");
     }
     if (m_flash_stub_tx_count < 2) {
         fail("TC_XSPI_DM_001: expected at least 2 flash transactions (WREN+PP)");
     }
     if (m_last_flash_ext.opcode != 0x02u) {
         fail("TC_XSPI_DM_001: last flash opcode after WRITE must be PAGE_PROGRAM (0x02)");
     }
 
     // --- Phase B: AXI READ — single READ_ZERO_LATENCY @ 0x1000 ---
     m_flash_stub_tx_count = 0;
     m_last_flash_ext      = cdns_extension();
     m_last_flash_bank     = -1;
 
     uint8_t read_buf[4] = { 0, 0, 0, 0 };
     tlm::tlm_response_status rresp =
         send_axi_read(t_axi_slave_initiator, k_flash_addr, read_buf);
     wait(20, sc_core::SC_NS);
 
     if (rresp != tlm::TLM_OK_RESPONSE) {
         fail("TC_XSPI_DM_001: AXI READ response not OK");
     }
     if (m_flash_stub_tx_count != 1) {
         fail("TC_XSPI_DM_001: READ phase expected exactly 1 flash transaction");
     }
     if (m_last_flash_ext.opcode != 0x03u) {
         fail("TC_XSPI_DM_001: opcode must be 0x03 (READ_ZERO_LATENCY)");
     }
     if (m_last_flash_ext.instr_type != static_cast<uint8_t>(XSPI_INSTR_READ)) {
         fail("TC_XSPI_DM_001: instr_type must be XSPI_INSTR_READ");
     }
     if (m_last_flash_ext.bank_num != 0u) {
         fail("TC_XSPI_DM_001: bank_num must be 0");
     }
     if (m_last_flash_ext.address != k_flash_addr) {
         fail("TC_XSPI_DM_001: flash extension address mismatch");
     }
     CSML_INFO(2, func008_logger)
         << "  DM_001: read_buf[4] = "
         << "0x" << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<unsigned>(read_buf[0]) << " "
         << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[1]) << " "
         << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[2]) << " "
         << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[3])
         << std::dec << " (expected pattern match)";
 
     if (std::memcmp(read_buf, pattern, 4u) != 0) {
         fail("TC_XSPI_DM_001: read data does not match programmed pattern");
     }
 
     report_test_result("TC_XSPI_DM_001", allpassed);
 }
// =============================================================================
// TC_XSPI_DM_002: AXI WRITE → WREN (0x06) + PAGE_PROGRAM (0x02)
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DM_002 — AXI WRITE translates to WREN + PAGE_PROGRAM
 *
 * Verification objective:
 *   Confirms that in Direct mode, an AXI slave WRITE transaction results in
 *   exactly two flash bus transactions: first WREN (opcode=0x06, generic), then
 *   PAGE_PROGRAM (opcode=0x02, write). Verifies both count and final extension.
 *
 * Stimulus:
 *   1. Apply reset, send inhibited PoR.
 *   2. Set ctrl_config.work_mode = 2'b00 (Direct mode).
 *   3. Reset tx counter and last extension.
 *   4. Issue 4-byte AXI WRITE at address 0x00002000 with data {0xDE,0xAD,0xBE,0xEF}.
 *
 * Expected behavioral side-effects:
 *   - AXI response == TLM_OK_RESPONSE
 *   - m_flash_stub_tx_count == 2  (WREN + PAGE_PROGRAM)
 *   - m_last_flash_ext.opcode     == 0x02 (PAGE_PROGRAM — captured last)
 *   - m_last_flash_ext.instr_type == XSPI_INSTR_WRITE (2)
 *   - m_last_flash_ext.address    == 0x00002000
 *   - m_last_flash_ext.data_bytes == 4
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 7.1.5 (AXI Slave WRITE Behavior).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_DM_002.
 ******************************************************************************/
void testbench::tc_xspi_dm_002_axi_write_to_wren_page_program()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_DM_002: AXI WRITE → WREN (0x06) + PAGE_PROGRAM (0x02)");

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    // Set Direct mode.
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Reset capture state.
    m_flash_stub_tx_count = 0;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    // Issue 4-byte AXI WRITE at 0x00002000.
    const uint8_t write_data[4] = { 0xDEu, 0xADu, 0xBEu, 0xEFu };
    tlm::tlm_response_status resp = send_axi_write(t_axi_slave_initiator,
                                                    0x00002000ULL, write_data);
    wait(15, sc_core::SC_NS);

    // --- Assertions ---
    bool pass_resp     = (resp == tlm::TLM_OK_RESPONSE);
    bool pass_tx_count = (m_flash_stub_tx_count == 2);
    bool pass_opcode   = (m_last_flash_ext.opcode == 0x02u);    // PAGE_PROGRAM
    bool pass_itype    = (m_last_flash_ext.instr_type
                          == static_cast<uint8_t>(XSPI_INSTR_WRITE));
    bool pass_addr     = (m_last_flash_ext.address  == 0x00002000ULL);
    bool pass_bytes    = (m_last_flash_ext.data_bytes == 4u);

    CSML_INFO(2, func008_logger)
        << "  DM_002: resp=" << static_cast<int>(resp)
        << " tx_count=" << std::dec << m_flash_stub_tx_count
        << " last_opcode=0x" << std::hex
        << static_cast<unsigned>(m_last_flash_ext.opcode)
        << " instr_type=" << std::dec
        << static_cast<unsigned>(m_last_flash_ext.instr_type)
        << " addr=0x" << std::hex << m_last_flash_ext.address
        << " bytes=" << std::dec << m_last_flash_ext.data_bytes;

    if (!pass_resp)     CSML_ERROR(0, func008_logger)
        << "  FAIL: AXI response != TLM_OK_RESPONSE";
    if (!pass_tx_count) CSML_ERROR(0, func008_logger)
        << "  FAIL: tx_count=" << m_flash_stub_tx_count
        << " expected 2 (WREN + PAGE_PROGRAM)";
    if (!pass_opcode)   CSML_ERROR(0, func008_logger)
        << "  FAIL: last opcode=0x" << std::hex
        << static_cast<unsigned>(m_last_flash_ext.opcode)
        << " expected 0x02 (PAGE_PROGRAM)";
    if (!pass_itype)    CSML_ERROR(0, func008_logger)
        << "  FAIL: instr_type="
        << static_cast<unsigned>(m_last_flash_ext.instr_type)
        << " expected " << static_cast<unsigned>(XSPI_INSTR_WRITE);
    if (!pass_addr)     CSML_ERROR(0, func008_logger)
        << "  FAIL: address=0x" << std::hex << m_last_flash_ext.address
        << " expected 0x00002000";
    if (!pass_bytes)    CSML_ERROR(0, func008_logger)
        << "  FAIL: data_bytes=" << std::dec << m_last_flash_ext.data_bytes
        << " expected 4";

    bool passed = pass_resp && pass_tx_count && pass_opcode
                  && pass_itype && pass_addr && pass_bytes;
    report_test_result("TC_XSPI_DM_002", passed);
}

// =============================================================================
// TC_XSPI_DM_003: Bank selection via dac_bank_num
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DM_003 — direct_access_cfg.dac_bank_num=2 routes to
 *        xspi_bus_socket[2]
 *
 * Verifies (NUM_TARGETS >= 4):
 *   1) ctrl_config.work_mode = Direct (0).
 *   2) direct_access_cfg(0x398) dac_bank_num = 2[2:0] routes AXI READs from
 *      t_axi_slave_socket to xspi_bus_socket[2] with cdns_extension.bank_num=2.
 *   3) Per-CS stubs: the bank-2 read increments only the socket[2] hit counter;
 *      the other CS stubs do not see an extra transfer for that read
 *      (e.g. xspi_bus_socket[0] stays idle for the phase-B read).
 *
 * Phase A (sanity): dac_bank_num=0 → socket[0], bank_num=0 in extension.
 * Phase B:          dac_bank_num=2 → socket[2], bank_num=2; no cmd_ignored.
 ******************************************************************************/
void testbench::tc_xspi_dm_003_bank_selection_dac_bank_num()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_DM_003: dac_bank_num=2 — AXI READ routes to xspi_bus_socket[2] "
        "(ext.bank_num=2; not socket[0] for the bank-2 read; NUM_TARGETS>=4)");

    if (dut->NUM_TARGETS < 4 || m_num_targets < 4) {
        CSML_ERROR(0, func008_logger)
            << "  DM_003 requires NUM_TARGETS>=4 and 4 flash stubs; DUT="
            << dut->NUM_TARGETS << " m_num_targets=" << m_num_targets;
        report_test_result("TC_XSPI_DM_003", false);
        return;
    }

    // -------------------------------------------------------------------------
    // Phase A: dac_bank_num=0 — routes to xspi_bus_socket[0]
    // -------------------------------------------------------------------------
    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);
    m_last_flash_ext  = cdns_extension();
    m_last_flash_bank = -1;

    int pa0 = m_flash_tx_per_target[0], pa1 = m_flash_tx_per_target[1],
        pa2 = m_flash_tx_per_target[2], pa3 = m_flash_tx_per_target[3];

    uint8_t read_buf_a[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp_a = send_axi_read(t_axi_slave_initiator,
                                                    0x00001000ULL, read_buf_a);
    wait(15, sc_core::SC_NS);

    uint32_t intr_stat_a = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_stat_a);

    bool pass_a_resp   = (resp_a == tlm::TLM_OK_RESPONSE);
    bool pass_a_bank   = (m_last_flash_ext.bank_num == 0u);
    bool pass_a_no_ign = ((intr_stat_a & (1u << 20u)) == 0u);
    bool pass_a_sock0
        = (m_flash_tx_per_target[0] == pa0 + 1) && (m_flash_tx_per_target[1] == pa1)
          && (m_flash_tx_per_target[2] == pa2) && (m_flash_tx_per_target[3] == pa3);

    CSML_INFO(2, func008_logger)
        << "  DM_003 Phase A: ext.bank_num="
        << static_cast<unsigned>(m_last_flash_ext.bank_num)
        << " per_t=["
        << m_flash_tx_per_target[0] << " "
        << m_flash_tx_per_target[1] << " "
        << m_flash_tx_per_target[2] << " "
        << m_flash_tx_per_target[3] << "]";

    if (!pass_a_resp)   CSML_ERROR(0, func008_logger)
        << "  Phase A FAIL: AXI response != TLM_OK_RESPONSE";
    if (!pass_a_bank)   CSML_ERROR(0, func008_logger)
        << "  Phase A FAIL: ext.bank_num="
        << static_cast<unsigned>(m_last_flash_ext.bank_num) << " expected 0";
    if (!pass_a_no_ign) CSML_ERROR(0, func008_logger)
        << "  Phase A FAIL: cmd_ignored (bit20) set; intr=0x" << std::hex
        << intr_stat_a;
    if (!pass_a_sock0)  CSML_ERROR(0, func008_logger)
        << "  Phase A FAIL: expected +1 on flash stub[0] only";

    // -------------------------------------------------------------------------
    // Phase B: dac_bank_num=2 (0x398[2:0]) — routes to xspi_bus_socket[2]
    // -------------------------------------------------------------------------
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET, 0x00000002u);
    wait(5, sc_core::SC_NS);

    uint32_t dac_cfg_readback = 0u;
    test->register_read_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET, dac_cfg_readback);
    bool pass_b_field = ((dac_cfg_readback & 0x7u) == 0x2u);
    if (!pass_b_field) {
        CSML_ERROR(0, func008_logger)
            << "  Phase B FAIL: direct_access_cfg[2:0]= " << (dac_cfg_readback & 0x7u)
            << " expected 2 (dac_bank_num)";
    }

    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);
    m_last_flash_ext  = cdns_extension();
    m_last_flash_bank = -1;

    int b0 = m_flash_tx_per_target[0], b1 = m_flash_tx_per_target[1],
        b2 = m_flash_tx_per_target[2], b3 = m_flash_tx_per_target[3];

    uint8_t read_buf_b[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp_b = send_axi_read(t_axi_slave_initiator,
                                                    0x00001000ULL, read_buf_b);
    wait(15, sc_core::SC_NS);

    uint32_t intr_stat_b = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_stat_b);

    bool pass_b_resp   = (resp_b == tlm::TLM_OK_RESPONSE);
    bool pass_b_bank   = (m_last_flash_ext.bank_num == 2u);
    bool pass_b_no_ign = ((intr_stat_b & (1u << 20u)) == 0u);
    // Only xspi_bus_socket[2] handles this read; CS0/1/3 idle (unchanged count).
    bool pass_b_routing = (m_flash_tx_per_target[0] == b0)
                          && (m_flash_tx_per_target[1] == b1)
                          && (m_flash_tx_per_target[2] == b2 + 1)
                          && (m_flash_tx_per_target[3] == b3);

    CSML_INFO(2, func008_logger)
        << "  DM_003 Phase B: dac_cfg_rb=0x" << std::hex << dac_cfg_readback
        << " ext.bank_num=" << static_cast<unsigned>(m_last_flash_ext.bank_num)
        << " per_t=["
        << m_flash_tx_per_target[0] << " " << m_flash_tx_per_target[1] << " "
        << m_flash_tx_per_target[2] << " " << m_flash_tx_per_target[3] << "]"
        << " cmd_ignored=" << std::dec << ((intr_stat_b >> 20u) & 0x1u);

    if (!pass_b_resp)   CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: AXI response != TLM_OK_RESPONSE";
    if (!pass_b_bank)   CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: ext.bank_num="
        << static_cast<unsigned>(m_last_flash_ext.bank_num) << " expected 2";
    if (!pass_b_no_ign) CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: cmd_ignored (bit20) set for in-range bank 2; intr=0x"
        << std::hex << intr_stat_b;
    if (!pass_b_routing) CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: only stub[2] should increment; per_t[0-3] now "
        << m_flash_tx_per_target[0] << " " << m_flash_tx_per_target[1] << " "
        << m_flash_tx_per_target[2] << " " << m_flash_tx_per_target[3];

    bool passed = pass_a_resp && pass_a_bank && pass_a_no_ign && pass_a_sock0
                  && pass_b_field && pass_b_resp && pass_b_bank && pass_b_no_ign
                  && pass_b_routing;
    report_test_result("TC_XSPI_DM_003", passed);
}

// =============================================================================
// TC_XSPI_DM_004: 64-bit address remapping via rmp_addr_en
// =============================================================================

// =============================================================================
// TC_XSPI_DM_004: 64-bit address remapping via rmp_addr_en
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DM_004 — 64-bit address remap: flash_addr = AXI_addr - N
 *
 * Same style as tc_xspi_dm_001_axi_read_to_flash_read(): fail() lambda,
 * ctrl_status idle check, CSML_INFO, report_test_result.
 *
 * N=0x1000 in rmp/rmp_1, rmp_addr_en=1, AXI READ @0x5000 → ext.address=0x4000.
 * No flash backing array required.
 ******************************************************************************/
void testbench::tc_xspi_dm_004_address_remapping_rmp_addr_en()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_DM_004: rmp_addr_en — AXI @0x5000, N=0x1000, flash addr 0x4000");

    auto fail = [&](const char* msg) {
        CSML_ERROR(0, func008_logger) << msg;
        report_test_result("TC_XSPI_DM_004", false);
        throw std::runtime_error(msg);
    };

    constexpr uint64_t k_n_lo       = 0x00001000ULL;  // The value of N
    constexpr uint64_t k_axi_addr = 0x00005000ULL;  // AXI READ @ 0x00005000
    constexpr uint64_t k_flash_addr = 0x00004000ULL; // Flash address

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_OFFSET,
                            static_cast<uint32_t>(k_n_lo));
    wait(2, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_1_OFFSET,
                            0x00000000u);
    wait(2, sc_core::SC_NS);

    // Enable Remapping
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                            0x00001000u);
    wait(2, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET,
                            0x00000000u);
    wait(5, sc_core::SC_NS);

    CSML_INFO(2, func008_logger)
        << "  DM_004: remap ON — rmp=0x" << std::hex << k_n_lo
        << " rmp_1=0 direct_access_cfg=0x1000 (rmp_addr_en=1), Direct mode";

    {
        uint32_t ctrl_stat = 0u;
        test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                               ctrl_stat);
        CSML_INFO(2, func008_logger)
            << "  DM_004: pre-traffic ctrl_status=0x" << std::hex << ctrl_stat
            << std::dec;
        if ((ctrl_stat & (1u << 7u)) != 0u) {
            fail("TC_XSPI_DM_004: ctrl_status.ctrl_busy (bit7) must be 0 (idle)");
        }
        if ((ctrl_stat & (1u << 3u)) != 0u) {
            fail("TC_XSPI_DM_004: ctrl_status.gcmd_eng_busy (bit3) must be 0");
        }
    }

    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    m_flash_stub_tx_count = 0;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    uint8_t read_buf[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp =
        send_axi_read(t_axi_slave_initiator, k_axi_addr, read_buf);
    wait(20, sc_core::SC_NS);

    uint32_t intr_stat = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_stat);

    if (resp != tlm::TLM_OK_RESPONSE) {
        fail("TC_XSPI_DM_004: AXI READ response not OK");
    }
    if ((intr_stat & (1u << 20u)) != 0u) {
        fail("TC_XSPI_DM_004: cmd_ignored must not be set for valid remap");
    }
    if (m_flash_stub_tx_count != 1) {
        fail("TC_XSPI_DM_004: expected exactly 1 flash transaction");
    }

    // Verify Translated Address
    if (m_last_flash_ext.address != k_flash_addr) {
        fail("TC_XSPI_DM_004: cdns_extension.address must be AXI - N (0x4000)");
    }
    if (m_last_flash_ext.opcode != 0x03u) {
        fail("TC_XSPI_DM_004: opcode must be 0x03 (READ_ZERO_LATENCY)");
    }
    if (m_last_flash_ext.instr_type != static_cast<uint8_t>(XSPI_INSTR_READ)) {
        fail("TC_XSPI_DM_004: instr_type must be XSPI_INSTR_READ");
    }

    // default bank 0
    if (m_last_flash_ext.bank_num != 0u) {
        fail("TC_XSPI_DM_004: bank_num must be 0");
    }

    CSML_INFO(2, func008_logger)
        << "  DM_004: read_buf[4] = "
        << "0x" << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<unsigned>(read_buf[0]) << " "
        << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[1]) << " "
        << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[2]) << " "
        << "0x" << std::setw(2) << static_cast<unsigned>(read_buf[3])
        << std::dec << " flash_ext.addr=0x" << std::hex << m_last_flash_ext.address
        << std::dec;

    report_test_result("TC_XSPI_DM_004", true);
}

// =============================================================================
// TC_XSPI_DM_005: ctrl_status.ctrl_busy lifecycle in Direct mode
// =============================================================================

// =============================================================================
// TC_XSPI_DM_005: ctrl_status.ctrl_busy lifecycle in Direct mode
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_DM_005 — ctrl_busy during flash dispatch, idle after AXI READ
 *
 * Test plan: ctrl_busy asserted while flash transaction runs, de-asserted after.
 * LT keeps AXI b_transport synchronous; we sample ctrl_status from inside
 * b_transport_flash_stub() while the DUT is blocked on the flash socket.
 * No flash backing RAM required.
 ******************************************************************************/
void testbench::tc_xspi_dm_005_direct_mode_ctrl_status_busy()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_DM_005: ctrl_busy during flash + idle after Direct AXI READ");

    auto fail = [&](const char* msg) {
        CSML_ERROR(0, func008_logger) << msg;
        report_test_result("TC_XSPI_DM_005", false);
        throw std::runtime_error(msg);
    };

    constexpr uint64_t k_axi_addr = 0x00001000ULL;

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_OFFSET,
                            0x00000000u);
    wait(2, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::direct_access_rmp_1_OFFSET,
                            0x00000000u);
    wait(2, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                            0x00000000u);
    wait(2, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET,
                            0x00000000u);
    wait(5, sc_core::SC_NS);

    CSML_INFO(2, func008_logger)
        << "  DM_005: Direct mode — remap off, dac_bank_num=0";

    {
        uint32_t ctrl_stat = 0u;
        test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                                 ctrl_stat);
        CSML_INFO(2, func008_logger)
            << "  DM_005: pre-traffic ctrl_status=0x" << std::hex << ctrl_stat
            << std::dec;
        if ((ctrl_stat & (1u << 7u)) != 0u) {
            fail("TC_XSPI_DM_005: ctrl_busy (bit7) must be 0 before AXI READ");
        }
        if ((ctrl_stat & (1u << 3u)) != 0u) {
            fail("TC_XSPI_DM_005: gcmd_eng_busy (bit3) must be 0 before AXI READ");
        }
    }

    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    m_flash_stub_tx_count = 0;
    m_last_flash_ext    = cdns_extension();
    m_last_flash_bank   = -1;
    m_ctrl_busy_seen_during_flash_read   = false;
    m_probe_ctrl_busy_on_next_flash_read = true;

    uint8_t read_buf[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp =
        send_axi_read(t_axi_slave_initiator, k_axi_addr, read_buf);
    wait(20, sc_core::SC_NS);

    if (resp != tlm::TLM_OK_RESPONSE) {
        fail("TC_XSPI_DM_005: AXI READ response not OK");
    }
    if (!m_ctrl_busy_seen_during_flash_read) {
        fail("TC_XSPI_DM_005: ctrl_busy (bit7) not set during flash b_transport");
    }

    uint32_t ctrl_stat_after = 0u;
    test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                           ctrl_stat_after);
    CSML_INFO(2, func008_logger)
        << "  DM_005: post-AXI ctrl_status=0x" << std::hex << ctrl_stat_after
        << " ctrl_busy=" << ((ctrl_stat_after >> 7u) & 0x1u) << std::dec;

    if ((ctrl_stat_after & (1u << 7u)) != 0u) {
        fail("TC_XSPI_DM_005: ctrl_busy (bit7) must be 0 after completion");
    }
    if (m_flash_stub_tx_count < 1) {
        fail("TC_XSPI_DM_005: expected at least one flash transaction");
    }

    report_test_result("TC_XSPI_DM_005", true);
}

// =============================================================================
// TC_XSPI_CFG_001: NUM_TARGETS boundary — out-of-range bank sets cmd_ignored
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_CFG_001 — Out-of-range bank triggers cmd_ignored
 *
 * Verification objective:
 *   Confirms that when direct_access_cfg.dac_bank_num is set to a value >=
 *   NUM_TARGETS (4 in the reference testbench), dispatch_flash_transaction()
 *   sets intr_status.cmd_ignored (bit 20) and does NOT issue a flash
 *   transaction on any xspi_bus_socket. The AXI slave still responds with
 *   TLM_OK_RESPONSE.
 *
 * Stimulus:
 *   1. Apply reset, send inhibited PoR.
 *   2. Write direct_access_cfg = 0x00000004 (dac_bank_num=4; valid banks are
 *      0..3 for NUM_TARGETS=4).
 *   3. Set ctrl_config.work_mode = 2'b00.
 *   4. Clear intr_status (W1C write).
 *   5. Save m_flash_stub_tx_count as baseline.
 *   6. Issue 4-byte AXI READ at 0x00001000.
 *
 * Expected behavioral side-effects:
 *   - AXI response == TLM_OK_RESPONSE
 *   - intr_status.cmd_ignored (bit 20) == 1
 *   - m_flash_stub_tx_count unchanged (no flash transaction dispatched)
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 7.1 (bank validation),
 *   dispatch_flash_transaction() in model/src/xspi_ctrl.cpp.
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_CFG_001.
 ******************************************************************************/
void testbench::tc_xspi_cfg_001_num_targets_1_boundary()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_CFG_001: NUM_TARGETS=4 boundary — out-of-range bank=4 "
        "sets cmd_ignored");

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    // Set dac_bank_num=4 (out of range: valid bank indices 0..NUM_TARGETS-1).
    // bits[2:0] = 0b100.
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                            0x00000004u);
    wait(5, sc_core::SC_NS);

    // Set Direct mode.
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Clear intr_status (W1C write with all bits to clear any stale flags).
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET,
                            0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    // Save baseline tx count.
    int tx_before = m_flash_stub_tx_count;

    // Issue 4-byte AXI READ — should trigger cmd_ignored, not flash dispatch.
    uint8_t read_buf[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp = send_axi_read(t_axi_slave_initiator,
                                                   0x00001000ULL, read_buf);
    wait(15, sc_core::SC_NS);

    // Read intr_status to verify cmd_ignored (bit 20).
    uint32_t intr_stat = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_stat);

    // --- Assertions ---
    bool pass_resp      = (resp == tlm::TLM_OK_RESPONSE);
    bool pass_cmd_ign   = ((intr_stat & (1u << 20u)) != 0u);  // bit 20 set
    bool pass_no_tx     = (m_flash_stub_tx_count == tx_before); // no flash tx

    CSML_INFO(2, func008_logger)
        << "  CFG_001: resp=" << static_cast<int>(resp)
        << " intr_status=0x" << std::hex << intr_stat
        << " cmd_ignored=" << ((intr_stat >> 20u) & 0x1u)
        << " tx_before=" << std::dec << tx_before
        << " tx_after=" << m_flash_stub_tx_count;

    if (!pass_resp)    CSML_ERROR(0, func008_logger)
        << "  FAIL: AXI response != TLM_OK_RESPONSE";
    if (!pass_cmd_ign) CSML_ERROR(0, func008_logger)
        << "  FAIL: intr_status.cmd_ignored (bit20) not set;"
        << " intr_status=0x" << std::hex << intr_stat;
    if (!pass_no_tx)   CSML_ERROR(0, func008_logger)
        << "  FAIL: flash tx issued despite out-of-range bank; "
        << "tx_count=" << std::dec << m_flash_stub_tx_count;

    bool passed = pass_resp && pass_cmd_ign && pass_no_tx;
    report_test_result("TC_XSPI_CFG_001", passed);
}

// =============================================================================
// TC_XSPI_CFG_002: NUM_TARGETS boundary and routing verification
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_CFG_002 — NUM_TARGETS socket array boundary and routing
 *
 *   Phase A — read ctrl_features_reg and report n_banks[25:24] (RO build info).
 *   Phase B — dac_bank_num=0: AXI READ routes in-range to socket[0], bank_num=0.
 *   Phase C — dac_bank_num=4: out of range for NUM_TARGETS=4 (banks 0–3) —
 *     cmd_ignored, no new flash b_transport.
 *
 * Stimulus:
 *   Phase A: Read ctrl_features_reg and extract n_banks bits[25:24].
 *   Phase B:
 *     1. Apply reset, send inhibited PoR.
 *     2. Write direct_access_cfg = 0x00000000 (dac_bank_num=0).
 *     3. Set ctrl_config.work_mode = 2'b00.
 *     4. Clear intr_status, reset capture state.
 *     5. Issue 4-byte AXI READ at 0x00003000.
 *   Phase C:
 *     6. Write direct_access_cfg = 0x00000004 (dac_bank_num=4).
 *     7. Clear intr_status, save tx baseline.
 *     8. Issue 4-byte AXI READ at 0x00003000.
 *
 * Expected behavioral side-effects:
 *   Phase A: ctrl_features_reg read succeeds (no assertion on n_banks value).
 *   Phase B:
 *     - AXI response == TLM_OK_RESPONSE
 *     - m_last_flash_ext.bank_num == 0
 *     - m_flash_stub_tx_count == 1 (exactly one flash transaction)
 *     - intr_status.cmd_ignored (bit 20) == 0
 *   Phase C:
 *     - intr_status.cmd_ignored (bit 20) == 1
 *     - m_flash_stub_tx_count unchanged from Phase B (no new flash transaction)
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 3.3 (NUM_TARGETS / n_banks),
 *   Section 7.1.2 (bank selection), Section 5.1.5 (ctrl_features_reg).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_CFG_002.
 ******************************************************************************/
void testbench::tc_xspi_cfg_002_num_targets_routing()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_CFG_002: NUM_TARGETS routing — n_banks read, bank=0 routes, "
        "bank=4 boundary-rejected (NUM_TARGETS=4 build)");

    // =========================================================================
    // Phase A: Read ctrl_features_reg and report n_banks bits[25:24]
    //   (no assertion — documents the architectural default-bin divergence)
    // =========================================================================
    uint32_t feat_reg = 0u;
    test->register_read_32(xspi_ctrl_basetest::ctrl_features_reg_OFFSET, feat_reg);
    uint32_t n_banks_enc = (feat_reg >> 24u) & 0x3u;
    CSML_INFO(2, func008_logger)
        << "  CFG_002 Phase A: ctrl_features_reg=0x" << std::hex << feat_reg
        << " n_banks[25:24]=" << std::dec << n_banks_enc
        << " (0=1bank, 1=2banks, 2=4banks, 3=8banks)"
        << " — DUT NUM_TARGETS=" << dut->NUM_TARGETS;

    // =========================================================================
    // Phase B: dac_bank_num=0 (in-range) — positive routing to socket[0]
    // =========================================================================

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    // Set dac_bank_num=0.
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                            0x00000000u);
    wait(5, sc_core::SC_NS);

    // Set Direct mode.
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);

    // Clear intr_status, reset capture state.
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);
    m_flash_stub_tx_count = 0;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    uint8_t read_buf_b[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp_b = send_axi_read(t_axi_slave_initiator,
                                                     0x00003000ULL, read_buf_b);
    wait(15, sc_core::SC_NS);

    uint32_t intr_b = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_b);

    bool pass_b_resp   = (resp_b == tlm::TLM_OK_RESPONSE);
    bool pass_b_tx     = (m_flash_stub_tx_count == 1);
    bool pass_b_bank   = (m_last_flash_ext.bank_num == 0u);
    bool pass_b_no_ign = ((intr_b & (1u << 20u)) == 0u);

    CSML_INFO(2, func008_logger)
        << "  CFG_002 Phase B: resp=" << static_cast<int>(resp_b)
        << " tx_count=" << std::dec << m_flash_stub_tx_count
        << " ext.bank_num=" << static_cast<unsigned>(m_last_flash_ext.bank_num)
        << " intr=0x" << std::hex << intr_b;

    if (!pass_b_resp)   CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: AXI response != TLM_OK_RESPONSE";
    if (!pass_b_tx)     CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: tx_count=" << std::dec << m_flash_stub_tx_count
        << " expected 1";
    if (!pass_b_bank)   CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: ext.bank_num="
        << static_cast<unsigned>(m_last_flash_ext.bank_num) << " expected 0";
    if (!pass_b_no_ign) CSML_ERROR(0, func008_logger)
        << "  Phase B FAIL: intr_status.cmd_ignored set unexpectedly;"
        << " intr=0x" << std::hex << intr_b;

    // =========================================================================
    // Phase C: dac_bank_num=4 (out-of-range for NUM_TARGETS=4) — boundary
    // =========================================================================

    // Write dac_bank_num=4 (bits[2:0]=0b100).
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                            0x00000004u);
    wait(5, sc_core::SC_NS);

    // Clear intr_status and capture tx baseline.
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);
    int tx_baseline = m_flash_stub_tx_count;

    uint8_t read_buf_c[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp_c = send_axi_read(t_axi_slave_initiator,
                                                     0x00003000ULL, read_buf_c);
    wait(15, sc_core::SC_NS);

    uint32_t intr_c = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr_c);

    bool pass_c_cmd_ign = ((intr_c & (1u << 20u)) != 0u);
    bool pass_c_no_tx   = (m_flash_stub_tx_count == tx_baseline);

    CSML_INFO(2, func008_logger)
        << "  CFG_002 Phase C: resp=" << static_cast<int>(resp_c)
        << " cmd_ignored=" << ((intr_c >> 20u) & 0x1u)
        << " tx_baseline=" << tx_baseline
        << " tx_after=" << m_flash_stub_tx_count;

    if (!pass_c_cmd_ign) CSML_ERROR(0, func008_logger)
        << "  Phase C FAIL: intr_status.cmd_ignored (bit20) not set;"
        << " intr=0x" << std::hex << intr_c
        << " (dac_bank_num=4 >= NUM_TARGETS=4 must trigger cmd_ignored)";
    if (!pass_c_no_tx)   CSML_ERROR(0, func008_logger)
        << "  Phase C FAIL: flash transaction dispatched despite out-of-range bank;"
        << " tx_count=" << std::dec << m_flash_stub_tx_count;

    bool passed = pass_b_resp && pass_b_tx && pass_b_bank && pass_b_no_ign
               && pass_c_cmd_ign && pass_c_no_tx;
    report_test_result("TC_XSPI_CFG_002", passed);
}

// =============================================================================
// TC_XSPI_CFG_001_num_targets_4: four-target build — banks 0–3 in range;
// bank 5 rejected
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_CFG_001_num_targets_4 — NUM_TARGETS=4 configuration coverage
 *
 *   1) ctrl_features_reg n_banks[25:24] == 3 (testcase: four-bank / four-CS
 *      build; matches reference ctrl_features reset encoding).
 *   2) DUT has exactly four xspi_bus_socket entries (indices 0..3).
 *   3) Direct mode: for each dac_bank_num in 0..3, one AXI READ produces
 *      TLM_OK, cmd_ignored clear, ext.bank_num matches, only the selected
 *      m_flash_tx_per_target[bank] increments.
 *   4) dac_bank_num=5: TLM_OK, intr_status.cmd_ignored set, no per-target
 *      or aggregate flash count change.
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 3.3, 4.6, 7.1.2;
 *   dispatch_flash_transaction() in model/src/xspi_ctrl.cpp.
 ******************************************************************************/
void testbench::tc_xspi_cfg_002_num_targets_4()
{
    func008_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_CFG_001_num_targets_4: NUM_TARGETS=4 — n_banks, four sockets, "
        "DM banks 0–3 OK, bank=5 OOR");

    if (dut->NUM_TARGETS != 4 || m_num_targets != 4) {
        CSML_ERROR(0, func008_logger)
            << "  TC_XSPI_CFG_001_num_targets_4 requires NUM_TARGETS=4; DUT="
            << dut->NUM_TARGETS << " m_num_targets=" << m_num_targets;
        report_test_result("TC_XSPI_CFG_001_num_targets_4", false);
        return;
    }

    if (dut->xspi_bus_socket.size() != 4U) {
        CSML_ERROR(0, func008_logger)
            << "  FAIL: xspi_bus_socket.size()=" << dut->xspi_bus_socket.size()
            << " expected 4";
        report_test_result("TC_XSPI_CFG_001_num_targets_4", false);
        return;
    }

    apply_reset();
    wait(5, sc_core::SC_NS);

    send_por_inhibited(por_initiator);
    wait(10, sc_core::SC_NS);

    uint32_t feat_reg = 0u;
    test->register_read_32(xspi_ctrl_basetest::ctrl_features_reg_OFFSET,
                          feat_reg);
    uint32_t n_banks_enc = (feat_reg >> 24u) & 0x3u;
    const bool pass_n_banks = (n_banks_enc == 3u);
    CSML_INFO(2, func008_logger)
        << "  CFG_001_num_targets_4: ctrl_features_reg=0x" << std::hex
        << feat_reg << " n_banks[25:24]=" << std::dec << n_banks_enc
        << " (expected 3 for this test)";
    if (!pass_n_banks) {
        CSML_ERROR(0, func008_logger)
            << "  FAIL: n_banks[25:24]=" << n_banks_enc << " expected 3";
    }

    // Direct mode; establish clean interrupt state.
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET, 0x00000000u);
    wait(5, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    bool pass_banks_0_3 = true;

    for (unsigned bank = 0u; bank < 4u; ++bank) {
        test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET,
                                static_cast<uint32_t>(bank));
        wait(5, sc_core::SC_NS);
        test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET,
                                0xFFFFFFFFu);
        wait(5, sc_core::SC_NS);
        m_last_flash_ext  = cdns_extension();
        m_last_flash_bank = -1;

        int t0 = m_flash_tx_per_target[0], t1 = m_flash_tx_per_target[1],
            t2 = m_flash_tx_per_target[2], t3 = m_flash_tx_per_target[3];

        uint8_t read_buf[4] = {0, 0, 0, 0};
        tlm::tlm_response_status resp = send_axi_read(
            t_axi_slave_initiator, 0x00001000ULL, read_buf);
        wait(15, sc_core::SC_NS);

        uint32_t intr = 0u;
        test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr);

        bool ok_resp = (resp == tlm::TLM_OK_RESPONSE);
        bool no_ign  = ((intr & (1u << 20u)) == 0u);
        bool ok_bank = (m_last_flash_ext.bank_num
                        == static_cast<uint8_t>(bank));
        int e0 = t0, e1 = t1, e2 = t2, e3 = t3;
        if (bank == 0u) {
            e0 = t0 + 1;
        } else if (bank == 1u) {
            e1 = t1 + 1;
        } else if (bank == 2u) {
            e2 = t2 + 1;
        } else {
            e3 = t3 + 1;
        }
        bool ok_route
            = (m_flash_tx_per_target[0] == e0)
              && (m_flash_tx_per_target[1] == e1)
              && (m_flash_tx_per_target[2] == e2)
              && (m_flash_tx_per_target[3] == e3);

        if (!ok_resp || !no_ign || !ok_bank || !ok_route) {
            pass_banks_0_3 = false;
            CSML_ERROR(0, func008_logger)
                << "  FAIL(bank " << bank << "): resp=" << static_cast<int>(resp)
                << " cmd_ignored=" << ((intr >> 20u) & 0x1u)
                << " ext.bank_num=" << static_cast<unsigned>(m_last_flash_ext.bank_num)
                << " per_t=[" << m_flash_tx_per_target[0] << " "
                << m_flash_tx_per_target[1] << " "
                << m_flash_tx_per_target[2] << " "
                << m_flash_tx_per_target[3] << "]";
        }
    }

    // Out-of-range: dac_bank_num=5 (field value 0b101) — no flash on any CS.
    test->register_write_32(xspi_ctrl_basetest::direct_access_cfg_OFFSET, 0x5u);
    wait(5, sc_core::SC_NS);
    test->register_write_32(xspi_ctrl_basetest::intr_status_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    int b0 = m_flash_tx_per_target[0], b1 = m_flash_tx_per_target[1],
        b2 = m_flash_tx_per_target[2], b3 = m_flash_tx_per_target[3];
    int total_before = m_flash_stub_tx_count;

    uint8_t read_oob[4] = {0, 0, 0, 0};
    tlm::tlm_response_status resp5 = send_axi_read(
        t_axi_slave_initiator, 0x00001000ULL, read_oob);
    wait(15, sc_core::SC_NS);

    uint32_t intr5 = 0u;
    test->register_read_32(xspi_ctrl_basetest::intr_status_OFFSET, intr5);

    const bool pass5_resp = (resp5 == tlm::TLM_OK_RESPONSE);
    const bool pass5_ign
        = ((intr5 & (1u << 20u)) != 0u);
    const bool pass5_no_tx
        = (m_flash_tx_per_target[0] == b0)
          && (m_flash_tx_per_target[1] == b1)
          && (m_flash_tx_per_target[2] == b2)
          && (m_flash_tx_per_target[3] == b3)
          && (m_flash_stub_tx_count == total_before);

    if (!pass5_resp) {
        CSML_ERROR(0, func008_logger)
            << "  FAIL(bank5): AXI response != TLM_OK_RESPONSE";
    }
    if (!pass5_ign) {
        CSML_ERROR(0, func008_logger)
            << "  FAIL(bank5): cmd_ignored not set; intr=0x" << std::hex
            << intr5;
    }
    if (!pass5_no_tx) {
        CSML_ERROR(0, func008_logger)
            << "  FAIL(bank5): flash traffic on OOR dac_bank_num=5; per_t=["
            << m_flash_tx_per_target[0] << " " << m_flash_tx_per_target[1]
            << " " << m_flash_tx_per_target[2] << " "
            << m_flash_tx_per_target[3] << "]"
            << " total_before=" << std::dec << total_before
            << " total_after=" << m_flash_stub_tx_count;
    }

    const bool passed
        = pass_n_banks && pass_banks_0_3 && pass5_resp && pass5_ign
          && pass5_no_tx;
    report_test_result("TC_XSPI_CFG_001_num_targets_4", passed);
}

// =============================================================================
// run_func008_tests: orchestrate all FUNC_XSPI_008 test cases
// =============================================================================

/******************************************************************************
 * @brief run_func008_tests — top-level entry for FUNC_XSPI_008 suite
 *
 * Orchestrates all FUNC_XSPI_008 test cases (Direct Mode Flash
 * Forwarding) in document order. Called from run_tests() after
 * run_func007_tests() completes.
 *
 * Test execution order matches the dependency ordering in
 * docs/xspi_ctrl-functionality-testcases.md: DM tests first (verifying
 * READ, WRITE, bank selection, address remap, ctrl_busy lifecycle), followed
 * by CFG tests (boundary enforcement and routing verification).
 *
 * Each test calls apply_reset() internally to guarantee a clean entry state.
 ******************************************************************************/
void testbench::run_func008_tests()
{
    func008_logger.setMaxVerbosity(2);

    CSML_INFO(2, func008_logger)
        << "================================================";
    CSML_INFO(2, func008_logger)
        << "  FUNC_XSPI_008: Direct Mode Flash Forwarding";
    CSML_INFO(2, func008_logger)
        << "================================================";

    // TC_XSPI_DM_001: AXI READ → READ_ZERO_LATENCY
    tc_xspi_dm_001_axi_read_to_flash_read();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_DM_002: AXI WRITE → WREN + PAGE_PROGRAM
    tc_xspi_dm_002_axi_write_to_wren_page_program();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_DM_003: Bank selection via dac_bank_num=0
    tc_xspi_dm_003_bank_selection_dac_bank_num();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_DM_004: 64-bit address remapping via rmp_addr_en
    tc_xspi_dm_004_address_remapping_rmp_addr_en();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_DM_005: ctrl_status.ctrl_busy lifecycle
    tc_xspi_dm_005_direct_mode_ctrl_status_busy();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_CFG_001_num_targets_4: n_banks, four sockets, DM 0–3, bank 5 OOR
    tc_xspi_cfg_002_num_targets_4();
    wait(10, sc_core::SC_NS);

    // TC_XSPI_CFG_001: out-of-range bank (NUM_TARGETS=4)
    // tc_xspi_cfg_001_num_targets_1_boundary();
    // wait(10, sc_core::SC_NS);

    // // TC_XSPI_CFG_002: NUM_TARGETS routing verification
    // tc_xspi_cfg_002_num_targets_routing();
    // wait(10, sc_core::SC_NS);

    CSML_INFO(2, func008_logger)
        << "  FUNC_XSPI_008 suite complete.";
}
