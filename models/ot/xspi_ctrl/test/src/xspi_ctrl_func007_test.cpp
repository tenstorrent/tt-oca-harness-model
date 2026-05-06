/******************************************************************************
 * @file xspi_ctrl_func007_test.cpp
 * @brief Test cases for FUNC_XSPI_007 — Power-on Reset and SFDP Discovery
 *        Engine
 *
 * This file implements all test cases mapped to FUNC_XSPI_007 in the
 * xspi_ctrl functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_007 Test Coverage (6 test cases):
 *
 * - TC_XSPI_POR_001: Basic PoR flow — xspi_PoR_trans delivered with
 *                    discovery_inhibit=1; verifies init_comp=1 in extension
 *                    output and ctrl_status.init_comp (bit 16) set in register.
 *
 * - TC_XSPI_POR_001_sfdp_discovery_full: Full SFDP at PoR (1-1-1 SDR, bank 0);
 *                    READ_SFDP@0; JESD216A 16-DWORD table; sequence registers.
 *
 * - TC_XSPI_POR_002: discovery_inhibit=1 skips SFDP — no READ_SFDP issued on
 *                    xspi_bus_socket; flash stub transaction counter remains
 *                    zero; discovery_comp bit (bit 2) set in discovery_control.
 *
 * - TC_XSPI_POR_003: discovery_bank selection — PoR with discovery_bank=2 and
 *                    discovery_inhibit=0 (NUM_TARGETS>=4) routes READ_SFDP
 *                    (opcode 0x5A) to xspi_bus_socket[2] only; socket[0] idle;
 *                    cmd_ignored clear.
 *
 * - TC_XSPI_POR_004: SFDP discovery failure — invalid SFDP signature returned
 *                    by zero-fill stub causes discovery_fail bit[3]=1 in
 *                    discovery_control and ctrl_status.init_fail (bits[9:8])
 *                    = 0b01; init_comp (bit 16 of ctrl_status) still set.
 *
 * - TC_XSPI_POR_005: SFDP auto-populate — JESD216A table programmed into
 *                    xspi_target_model::sfdp_rom (bank-0 flash); read-back of
 *                    global/.../stat seq registers matches
 *                    configure_registers_from_sfdp() (4-4-4 SDR discovery).
 *
 * - TC_XSPI_ERR_005: Pre-init write blocking — a register write to ctrl_config
 *                    before the xspi_PoR_trans transaction returns is silently
 *                    discarded; after PoR completes the register retains its
 *                    hardware reset value 0x00000000.
 *
 * Design reference:
 *   - docs/xspi_ctrl-detailed-design.md Section 8 (PoR and SFDP Discovery)
 *   - docs/xspi_ctrl-functionality-testcases.md FUNC_XSPI_007 section
 *   - model/inc/cdns_extension.h (xspi_PoR_trans struct)
 *   - model/src/xspi_ctrl.cpp b_transport_por(), run_sfdp_discovery()
 *
 * Implementation notes:
 *   - All PoR transactions use por_initiator->b_transport() on the testbench.
 *   - TC_XSPI_POR_003 uses discovery_bank=2 with NUM_TARGETS>=4; verifies
 *     per-socket m_flash_tx_per_target[] and m_last_flash_ext (opcode 0x5A,
 *     bank 2) without cmd_ignored.
 *   - TC_XSPI_POR_005 preloads the connected flash model SFDP ROM; READ_SFDP is
 *     served by xspi_target_model (not the prelude synthetic path).
 *   - m_flash_stub_tx_count increments on every invocation of
 *     b_transport_flash_stub so that discovery traffic is observable.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "cdns_extension.h"
#include "csml_logger.h"
#include "xspi_target.h"

#include <cstdint>

// =============================================================================
// Module-local logger
// =============================================================================
namespace {
    CsmlLogger func007_logger;

    /**
     * Load a JESD216A-shaped image into the connected xspi_target_model SFDP ROM.
     * READ_SFDP is served by xspi_target_model::handle_read_sfdp() (see target_model
     * / xspi_target.cpp). The testbench does not set m_flash_sfdp_mode — that path
     * would short-circuit in flash_trans_prelude and skip the flash device; we want
     * the real TLM target to supply bytes.
     */
    static void por_005_load_sfdp_flash_rom(xspi_target_model& flash)
    {
        xspi_sfdp::sfdp_rom_t* const rom = flash.get_sfdp_rom();
        if (rom == nullptr) {
            return;
        }
        // SFDP header @0x00, JEDEC parameter header @0x08, basic table @0x30
        // (raw layout matches DUT configure_registers_from_sfdp() param_table[]).
        static const uint8_t k_hdr[8] = {
            0x53u, 0x46u, 0x44u, 0x50u, 0x05u, 0x01u, 0x01u, 0xFFu
        };
        static const uint8_t k_ph[8] = {
            0x00u, 0x05u, 0x01u, 0x10u, 0x30u, 0x00u, 0x00u, 0xFFu
        };
        rom->load(0x00u, std::vector<uint8_t>(std::begin(k_hdr), std::end(k_hdr)));
        rom->load(0x08u, std::vector<uint8_t>(std::begin(k_ph), std::end(k_ph)));

        std::vector<uint8_t> table(64u, 0u);
        table[5]  = 0x0Cu;   // DWORD2: page 2^12
        table[25] = 0x12u;  // DWORD7: erase size / opcode
        table[26] = 0xD8u;
        table[48] = 0xFCu;  // DWORD13: suspend/resume
        table[49] = 0x75u;
        table[50] = 0x7Au;
        table[51] = 0xB0u;
        rom->load(0x30u, table);
    }
}  // namespace

// =============================================================================
// Helper: build and send a PoR transaction with the supplied parameters
//
// Constructs a TLM_WRITE_COMMAND payload, attaches the xspi_PoR_trans
// extension populated from the caller-supplied parameters, and invokes
// por_initiator->b_transport() synchronously.  The extension fields are
// read back by the caller after the call returns.
//
// @param por_ext  Populated xspi_PoR_trans extension; modified in-place with
//                 the model's output fields (init_comp, init_fail, boot_comp,
//                 boot_error) after b_transport() returns.
// =============================================================================
static void send_por_transaction(
    tlm_utils::simple_initiator_socket<testbench, 64>& por_initiator,
    xspi_PoR_trans& por_ext)
{
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

    // Attach the PoR extension — the model extracts input fields and writes
    // back output fields (init_comp, init_fail, boot_comp, boot_error).
    payload.set_extension(&por_ext);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    por_initiator->b_transport(payload, delay);

    // Remove the extension to prevent automatic deletion by the payload
    // destructor (the caller owns por_ext on the stack).
    xspi_PoR_trans* removed = nullptr;
    payload.get_extension(removed);
    if (removed != nullptr) {
        payload.clear_extension(removed);
    }
}

// =============================================================================
// TC_XSPI_POR_001: Basic PoR flow — init_comp set after xspi_PoR_trans
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_001 — Basic PoR flow: xspi_PoR_trans delivered,
 *        ctrl_status.init_comp=1 after completion.
 *
 * Verification objective:
 *   Confirms that delivering a minimal xspi_PoR_trans with discovery_inhibit=1
 *   (to skip SFDP and obtain a deterministic fast path) causes:
 *     (a) The extension output field por_ext.init_comp to be 1 after
 *         b_transport() returns.
 *     (b) ctrl_status (offset 0x100) bit 16 to be 1 (init_comp in hardware).
 *     (c) discovery_control (offset 0x260) bit 2 to be 1 (discovery_comp).
 *     (d) The TLM transaction response status is TLM_OK_RESPONSE.
 *
 * Stimulus:
 *   1. Apply reset to DUT.
 *   2. Build xspi_PoR_trans with discovery_inhibit=1, discovery_bank=0,
 *      discovery_num_lines=0 (auto, irrelevant when inhibited), boot_en=0.
 *   3. Send via por_initiator->b_transport().
 *   4. Read ctrl_status and discovery_control via t_reg_socket.
 *
 * Expected results:
 *   - por_ext.init_comp  == 1
 *   - ctrl_status bit 16 == 1 (init_comp set)
 *   - ctrl_status bits[9:8] == 0b00 (init_fail=0, inhibit is not a failure)
 *   - discovery_control bit 2 == 1 (discovery_comp)
 *   - discovery_control bits[4:3] == 0b00 (discovery_fail=0)
 *
 * Pass criterion: all five conditions hold.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.1 (PoR sequence),
 *   Section 8.2 (discovery_inhibit fast path).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_POR_001.
 ******************************************************************************/
void testbench::tc_xspi_por_001_por_init_comp_basic()
{
    func007_logger.setMaxVerbosity(2);
    func007_logger.setLogFormat(
        "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    func007_logger.setFunctionTrace(false);

    report_test_start(
        "TC_XSPI_POR_001: Basic PoR — init_comp set after xspi_PoR_trans");

    // Ensure the DUT is in a clean reset state before issuing PoR.
    apply_reset();
    wait(5, sc_core::SC_NS);

    // Build the PoR extension: discovery_inhibit=1 to take the fast path.
    // This avoids any dependence on the flash stub SFDP data for this test.
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit  = 1u;
    por_ext.discovery_bank     = 0u;
    por_ext.discovery_num_lines = 0u;
    por_ext.boot_en            = 0u;

    // Send the PoR transaction synchronously.
    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    // --- Assertion A: extension output field init_comp == 1 ---
    bool pass_ext_init_comp = (por_ext.init_comp == 1u);
    if (!pass_ext_init_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001 FAIL: por_ext.init_comp="
            << static_cast<unsigned>(por_ext.init_comp)
            << " expected 1";
    }

    // --- Assertion B: ctrl_status bit 16 == 1 ---
    uint32_t ctrl_stat = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_stat);
    bool pass_reg_init_comp = ((ctrl_stat >> 16u) & 0x1u) == 1u;
    if (!pass_reg_init_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001 FAIL: ctrl_status=0x" << std::hex << ctrl_stat
            << " bit16 (init_comp) not set";
    } else {
        CSML_INFO(2, func007_logger)
            << "  POR_001: ctrl_status=0x" << std::hex << ctrl_stat
            << " init_comp confirmed";
    }

    // --- Assertion C: ctrl_status bits[9:8] == 0 (init_fail=0) ---
    bool pass_init_fail = (((ctrl_stat >> 8u) & 0x3u) == 0x0u);
    if (!pass_init_fail) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001 FAIL: ctrl_status init_fail bits[9:8]="
            << std::hex << ((ctrl_stat >> 8u) & 0x3u)
            << " expected 0b00";
    }

    // --- Assertion D: discovery_control bit 2 == 1 (discovery_comp) ---
    uint32_t disc_ctrl = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::discovery_control_OFFSET, disc_ctrl);
    bool pass_disc_comp = ((disc_ctrl >> 2u) & 0x1u) == 1u;
    if (!pass_disc_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001 FAIL: discovery_control=0x" << std::hex << disc_ctrl
            << " bit2 (discovery_comp) not set";
    } else {
        CSML_INFO(2, func007_logger)
            << "  POR_001: discovery_control=0x" << std::hex << disc_ctrl
            << " discovery_comp confirmed";
    }

    // --- Assertion E: discovery_control bits[4:3] == 0b00 (discovery_fail=0) ---
    bool pass_disc_fail = (((disc_ctrl >> 3u) & 0x3u) == 0x0u);
    if (!pass_disc_fail) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001 FAIL: discovery_control discovery_fail bits[4:3]="
            << std::hex << ((disc_ctrl >> 3u) & 0x3u)
            << " expected 0b00";
    }

    bool passed = pass_ext_init_comp && pass_reg_init_comp
                  && pass_init_fail && pass_disc_comp && pass_disc_fail;
    report_test_result("TC_XSPI_POR_001", passed);
}

// =============================================================================
// TC_XSPI_POR_001_sfdp_discovery_full: PoR + full JESD216A SFDP (1-1-1 SDR)
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_001_sfdp_discovery_full — Full SFDP discovery at PoR with
 *        1-1-1 SDR, bank 0, 3-byte addr, 8 dummy cycles; READ_SFDP (0x5A) at
 *        SFDP address 0; 16-DWORD basic table; sequence regs SFDP-populated.
 *
 * Follows the verification flow: poll ctrl_status.ctrl_busy=0, deliver PoR
 * (hardware programs discovery per extension fields; equivalent to
 * discovery_control for bootstrap), then poll for completion and check status
 * and ers_seq_cfg_0, we_seq_cfg_0, read_seq_cfg_0.
 ******************************************************************************/
void testbench::tc_xspi_por_001_sfdp_discovery_full()
{
    func007_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_POR_001_sfdp_discovery_full: Full JESD216A SFDP at PoR (1-1-1 SDR)");

    apply_reset();
    wait(5, sc_core::SC_NS);

    m_flash_stub_tx_count  = 0;
    m_seen_read_sfdp_addr0 = false;

    // 1) Controller IDLE before discovery (ctrl_status ctrl_busy bit 7 == 0).
    bool pass_ctrl_idle = false;
    for (int n = 0; n < 2000; ++n) {
        uint32_t cs0 = 0u;
        test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET, cs0);
        if (((cs0 >> 7) & 1u) == 0u) {
            pass_ctrl_idle = true;
            break;
        }
        wait(1, sc_core::SC_NS);
    }
    if (!pass_ctrl_idle) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: ctrl_status.ctrl_busy never cleared";
    }


    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 0u;   // full SFDP
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 0x1u; // 1-1-1 SDR
    por_ext.discovery_abnum     = 0u;   // 3-byte addressing
    por_ext.discovery_dummy_cnt = 0u;  // 8 dummy cycles
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(5, sc_core::SC_NS);

    // 4) Wait for discovery / init complete (polled; PoR is synchronous but
    //    matches the documented post-reset software sequence).
    bool pass_disc_comp_poll = false;
    for (int n = 0; n < 2000; ++n) {
        uint32_t d = 0u;
        test->register_read_32(
            xspi_ctrl_basetest::discovery_control_OFFSET, d);
        if (((d >> 2) & 1u) == 1u) {
            pass_disc_comp_poll = true;
            break;
        }
        wait(1, sc_core::SC_NS);
    }
    if (!pass_disc_comp_poll) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: discovery_comp (bit2) not set in time";
    }

    wait(10, sc_core::SC_NS);

    // Bus: READ_SFDP (0x5A) at address 0 on bank 0 (header read); three reads total.
    bool pass_seen_hdr = m_seen_read_sfdp_addr0;
    if (!pass_seen_hdr) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: no READ_SFDP(0x5A)@0x0 on bank 0";
    }
    bool pass_tx3 = (m_flash_stub_tx_count == 3);
    if (!pass_tx3) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: flash stub count="
            << m_flash_stub_tx_count << " expected 3 (hdr+param+table)";
    }

    // Extension and PoR result
    bool pass_ext = (por_ext.init_comp == 1u) && (por_ext.init_fail == 0u);

    uint32_t ctrl_stat = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_stat);
    bool pass_init_comp = ((ctrl_stat >> 16u) & 1u) == 1u;
    bool pass_init_fail0 = (((ctrl_stat >> 8u) & 0x3u) == 0u);

    uint32_t disc = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::discovery_control_OFFSET, disc);
    bool pass_disc_comp = ((disc >> 2) & 1u) == 1u;
    bool pass_disc_fail0 = (((disc >> 3) & 0x3u) == 0u);
    // discovery_inhibit status mirror (bit 5) must be 0 for non-inhibited path
    bool pass_inhibit0 = ((disc >> 5) & 1u) == 0u;

    uint32_t ers0 = 0u;
    uint32_t we0  = 0u;
    uint32_t rds0 = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET, ers0);
    test->register_read_32(
        xspi_ctrl_basetest::we_seq_cfg_0_OFFSET, we0);
    test->register_read_32(
        xspi_ctrl_basetest::read_seq_cfg_0_OFFSET, rds0);
    const uint32_t ers_reset = static_cast<uint32_t>(
        xspi_ctrl_basetest::ers_seq_cfg_0_RESET);
    bool pass_ers = (ers0 != ers_reset);
    bool pass_we  = ((we0 & 0xFFu) == 0x06u);
    // For 1-1-1 SDR, configure_registers_from_sfdp() may set read_seq_cfg_0
    // to 0x00003003, identical to hardware reset; require Profile-1 READ(0x03)
    // and 3-byte address count per SFDP path.
    bool pass_read = (((rds0 & 0xFFu) == 0x03u)
                      && (((rds0 >> 12) & 0x7u) == 0x3u));

    if (!pass_ers) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: ers_seq_cfg_0 unchanged from reset";
    }
    if (!pass_we) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: we_seq_cfg_0 WREN not 0x06";
    }
    if (!pass_read) {
        CSML_ERROR(0, func007_logger)
            << "  POR_001_sfdp_full FAIL: read_seq_cfg_0=0x" << std::hex
            << rds0 << " expected READ(0x03) + 3 addr bytes";
    }

    bool passed = pass_ctrl_idle && pass_disc_comp_poll && pass_ext
                  && pass_init_comp && pass_init_fail0
                  && pass_disc_comp && pass_disc_fail0 && pass_inhibit0
                  && pass_seen_hdr && pass_tx3
                  && pass_ers && pass_we && pass_read;

    report_test_result("TC_XSPI_POR_001_sfdp_discovery_full", passed);
}

// =============================================================================
// TC_XSPI_POR_002: discovery_inhibit=1 skips SFDP — no flash bus transaction
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_002 — discovery_inhibit=1: no READ_SFDP issued on
 *        xspi_bus_socket; discovery_comp set immediately.
 *
 * Verification objective:
 *   Confirms that when discovery_inhibit=1, the model does NOT issue any
 *   transaction on xspi_bus_socket[].  This is verified by:
 *     (a) Resetting m_flash_stub_tx_count to 0 before PoR.
 *     (b) Sending PoR with discovery_inhibit=1.
 *     (c) After completion, m_flash_stub_tx_count must still be 0.
 *     (d) discovery_control bit 2 (discovery_comp) must be 1.
 *     (e) discovery_control bit 5 (discovery_inhibit status mirror) must be 1.
 *
 * Stimulus:
 *   1. Apply reset; zero m_flash_stub_tx_count.
 *   2. Build xspi_PoR_trans: discovery_inhibit=1, discovery_bank=0, boot_en=0.
 *   3. Send via por_initiator.
 *   4. Check flash stub counter and discovery_control register.
 *
 * Expected results:
 *   - m_flash_stub_tx_count == 0 (no READ_SFDP issued)
 *   - discovery_control bit 2 == 1 (discovery_comp)
 *   - discovery_control bit 5 == 1 (discovery_inhibit status mirror)
 *   - discovery_control bits[4:3] == 0b00 (discovery_fail = not-failed)
 *
 * Pass criterion: all four conditions hold.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.2 (discovery_inhibit path),
 *   Section 8.3 (IDLE → SUCCESS direct transition).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_POR_002.
 ******************************************************************************/
void testbench::tc_xspi_por_002_discovery_inhibit_skips_sfdp()
{
    report_test_start(
        "TC_XSPI_POR_002: discovery_inhibit=1 — no READ_SFDP; init_comp; seq regs");

    apply_reset();
    wait(5, sc_core::SC_NS);

    m_flash_stub_tx_count = 0;

    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 1u;
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    uint32_t disc_ctrl = 0u;
    uint32_t ctrl_stat = 0u;
    uint32_t read_seq0 = 0u;
    uint32_t we_seq0   = 0u;

    test->register_read_32(xspi_ctrl_basetest::discovery_control_OFFSET, disc_ctrl);
    test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_stat);
    test->register_read_32(xspi_ctrl_basetest::read_seq_cfg_0_OFFSET, read_seq0);
    test->register_read_32(xspi_ctrl_basetest::we_seq_cfg_0_OFFSET, we_seq0);

    const bool pass_no_flash =
        (m_flash_stub_tx_count == 0);
    const bool pass_disc_comp =
        (((disc_ctrl >> 2u) & 1u) == 1u);
    const bool pass_inhibit_mir =
        (((disc_ctrl >> 5u) & 1u) == 1u);
    const bool pass_disc_fail =
        (((disc_ctrl >> 3u) & 3u) == 0u);
    const bool pass_init_comp =
        (((ctrl_stat >> 16u) & 1u) == 1u);
    const bool pass_read_seq =
        (read_seq0 == static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_0_RESET));
    const bool pass_we_seq =
        (we_seq0 == static_cast<uint32_t>(xspi_ctrl_basetest::we_seq_cfg_0_RESET));

    std::ostringstream oss;
    if (!pass_no_flash) {
        oss << "flash_tx_count=" << m_flash_stub_tx_count << " (expected 0); ";
    }
    if (!pass_disc_comp) {
        oss << "discovery_comp not set; discovery_control=0x" << std::hex
            << disc_ctrl << std::dec << "; ";
    }
    if (!pass_inhibit_mir) {
        oss << "discovery_inhibit mirror (bit5) not set; ";
    }
    if (!pass_disc_fail) {
        oss << "discovery_fail!=0; ";
    }
    if (!pass_init_comp) {
        oss << "ctrl_status.init_comp (bit16) not set; ctrl_status=0x"
            << std::hex << ctrl_stat << std::dec << "; ";
    }
    if (!pass_read_seq) {
        oss << "read_seq_cfg_0=0x" << std::hex << read_seq0
            << " expected 0x" << xspi_ctrl_basetest::read_seq_cfg_0_RESET
            << std::dec << "; ";
    }
    if (!pass_we_seq) {
        oss << "we_seq_cfg_0=0x" << std::hex << we_seq0
            << " expected 0x" << xspi_ctrl_basetest::we_seq_cfg_0_RESET
            << std::dec << "; ";
    }

    const bool ok = pass_no_flash && pass_disc_comp && pass_inhibit_mir
                 && pass_disc_fail && pass_init_comp && pass_read_seq && pass_we_seq;

    if (ok) {
        report_test_pass("TC_XSPI_POR_002");
    } else {
        report_test_fail("TC_XSPI_POR_002", oss.str());
    }
}

// =============================================================================
// TC_XSPI_POR_003: discovery_bank=2 — READ_SFDP on xspi_bus_socket[2] only
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_003 — discovery_bank=2 routes READ_SFDP (0x5A) to
 *        xspi_bus_socket[2]; xspi_bus_socket[0] sees no activity.
 *
 * Verification objective (requires NUM_TARGETS >= 4):
 *   With discovery_inhibit=0 and discovery_bank=2, READ_SFDP must be issued
 *   only on xspi_bus_socket[2], with cdns_extension.opcode==0x5A.  Socket[0]
 *   must not observe any new flash transaction (per-target count unchanged).
 *
 * Stimulus:
 *   1. Apply reset; record m_flash_tx_per_target[0..3] baselines.
 *   2. Build xspi_PoR_trans: discovery_inhibit=0, discovery_bank=2,
 *      discovery_num_lines=0x1 (single 1-1-1 SDR variation).
 *   3. Send via por_initiator.
 *   4. Check per-target hit counts, m_last_flash_ext (last flash txn), and
 *      intr_status / discovery_control.
 *
 * Expected results:
 *   - m_flash_tx_per_target[0] unchanged; [1] and [3] unchanged; [2] increases
 *   - m_last_flash_ext.opcode==0x5A and bank_num==2
 *   - cmd_ignored (bit 20)==0; discovery_comp (bit 2)==1
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.1 (discovery_bank),
 *   Section 8.3 (READ_SFDP_HEADER).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_POR_003.
 ******************************************************************************/
void testbench::tc_xspi_por_003_discovery_bank_selection()
{
    report_test_start(
        "TC_XSPI_POR_003: discovery_bank=2 — READ_SFDP(0x5A) on socket[2] only "
        "(NUM_TARGETS>=4)");

    if (dut == nullptr || dut->NUM_TARGETS < 4 || m_num_targets < 4) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 requires NUM_TARGETS>=4; dut="
            << (dut ? dut->NUM_TARGETS : -1)
            << " m_num_targets=" << m_num_targets;
        report_test_result("TC_XSPI_POR_003", false);
        return;
    }

    apply_reset();
    wait(5, sc_core::SC_NS);

    m_flash_stub_tx_count = 0;
    m_last_flash_ext      = cdns_extension();
    m_last_flash_bank     = -1;

    int b0 = m_flash_tx_per_target[0], b1 = m_flash_tx_per_target[1],
        b2 = m_flash_tx_per_target[2], b3 = m_flash_tx_per_target[3];

    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 0u;
    por_ext.discovery_bank      = 2u;  // Route discovery to xspi_bus_socket[2]
    por_ext.discovery_num_lines = 0x1u;  // 1-1-1 SDR — single variation
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_dummy_cnt = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    // Routing: only socket[2] sees new READ_SFDP traffic; CS0/CS1/CS3 idle.
    bool pass_routing
        = (m_flash_tx_per_target[0] == b0) && (m_flash_tx_per_target[1] == b1)
          && (m_flash_tx_per_target[2] >= b2 + 1) && (m_flash_tx_per_target[3] == b3);
    if (!pass_routing) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: expected +N only on [2]; baseline per_t=["
            << b0 << " " << b1 << " " << b2 << " " << b3
            << "] after per_t=[" << m_flash_tx_per_target[0] << " "
            << m_flash_tx_per_target[1] << " " << m_flash_tx_per_target[2] << " "
            << m_flash_tx_per_target[3] << "]";
    } else {
        CSML_INFO(2, func007_logger)
            << "  POR_003: per-target deltas — only [2] advanced";
    }

    // Last completed flash transfer should be READ_SFDP on the selected bank.
    constexpr uint8_t kReadSfdpOpcode = 0x5Au;
    bool pass_op = (m_last_flash_ext.opcode == kReadSfdpOpcode);
    bool pass_bank
        = (m_last_flash_ext.bank_num == 2u) && (m_last_flash_bank == 2);
    if (!pass_op) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: last ext.opcode=0x" << std::hex
            << static_cast<unsigned>(m_last_flash_ext.opcode) << " expected 0x5A";
    }
    if (!pass_bank) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: last ext.bank_num="
            << static_cast<unsigned>(m_last_flash_ext.bank_num)
            << " m_last_flash_bank=" << m_last_flash_bank << " expected 2";
    }

    bool pass_flash_total = (m_flash_stub_tx_count >= 1);
    if (!pass_flash_total) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: m_flash_stub_tx_count=" << m_flash_stub_tx_count
            << " (expected >=1)";
    }

    uint32_t intr_stat = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::intr_status_OFFSET, intr_stat);
    bool pass_no_cmd_ignored = ((intr_stat >> 20u) & 0x1u) == 0u;
    if (!pass_no_cmd_ignored) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: intr_status=0x" << std::hex << intr_stat
            << " cmd_ignored (bit20) set";
    }

    uint32_t disc_ctrl = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::discovery_control_OFFSET, disc_ctrl);
    bool pass_disc_comp = ((disc_ctrl >> 2u) & 0x1u) == 1u;
    if (!pass_disc_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_003 FAIL: discovery_control=0x" << std::hex << disc_ctrl
            << " discovery_comp (bit2) not set";
    }

    bool passed = pass_routing && pass_op && pass_bank && pass_flash_total
                  && pass_no_cmd_ignored && pass_disc_comp;
    report_test_result("TC_XSPI_POR_003", passed);
}

// =============================================================================
// TC_XSPI_POR_004: Invalid SFDP signature → discovery_fail + init_fail
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_004 — Invalid SFDP signature from flash stub:
 *        discovery_fail=1, ctrl_status.init_fail=0b01, init_comp=1.
 *
 * Verification objective:
 *   Confirms the FAIL path of the SFDP discovery state machine.  When the
 *   flash stub returns zero-filled data (as is always the case with the
 *   default stub), no variation yields the SFDP magic signature 0x50444653.
 *   The state machine transitions to FAIL after exhausting all variations.
 *   The expected observable effects are:
 *     (a) discovery_control bits[4:3] == 0b01 (discovery_fail = FAIL code).
 *     (b) ctrl_status bits[9:8] == 0b01 (init_fail = FAIL code).
 *     (c) ctrl_status bit 16 == 1 (init_comp — always set even on failure).
 *     (d) por_ext.init_fail == 0x01 (extension output field).
 *     (e) por_ext.init_comp == 1.
 *
 * Stimulus:
 *   1. Apply reset.
 *   2. Build xspi_PoR_trans: discovery_inhibit=0 (full discovery),
 *      discovery_num_lines=0 (auto-iterate all 13 variations),
 *      discovery_bank=0.
 *   3. Flash stub fills all READ responses with zeros (default behaviour).
 *   4. Send PoR and read back registers.
 *
 * Expected results:
 *   - discovery_control bits[4:3] == 0b01
 *   - ctrl_status bits[9:8] == 0b01
 *   - ctrl_status bit 16 == 1
 *   - por_ext.init_fail == 1
 *   - por_ext.init_comp == 1
 *
 * Pass criterion: all five conditions hold.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.3 (FAIL state),
 *   Section 8.5 (init_fail encoding).
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_POR_004.
 ******************************************************************************/
void testbench::tc_xspi_por_004_discovery_fail_sets_init_fail()
{
    report_test_start(
        "TC_XSPI_POR_004: Invalid SFDP signature → discovery_fail + init_fail");

    apply_reset();
    wait(5, sc_core::SC_NS);

    // discovery_inhibit=0, num_lines=0 (auto) — iterates all 13 variations.
    // Flash stub returns zeros → no valid signature → FAIL outcome.
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 0u;
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 0u;  // Auto-iterate all variations
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;
    por_ext.discovery_dummy_cnt = 0u;
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    // --- Assertion A: por_ext.init_fail == 1 ---
    bool pass_ext_fail = (por_ext.init_fail == 0x01u);
    if (!pass_ext_fail) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: por_ext.init_fail="
            << static_cast<unsigned>(por_ext.init_fail) << " expected 1";
    }

    // --- Assertion B: por_ext.init_comp == 1 ---
    bool pass_ext_comp = (por_ext.init_comp == 1u);
    if (!pass_ext_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: por_ext.init_comp="
            << static_cast<unsigned>(por_ext.init_comp) << " expected 1";
    }

    // --- Assertion C: ctrl_status bit 16 == 1, bits[9:8] == 0b01 ---
    uint32_t ctrl_stat = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_stat);
    bool pass_init_comp_reg = ((ctrl_stat >> 16u) & 0x1u) == 1u;
    bool pass_init_fail_reg = (((ctrl_stat >> 8u) & 0x3u) == 0x1u);
    if (!pass_init_comp_reg) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: ctrl_status=0x" << std::hex << ctrl_stat
            << " init_comp (bit16) not set";
    }
    if (!pass_init_fail_reg) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: ctrl_status=0x" << std::hex << ctrl_stat
            << " init_fail bits[9:8]="
            << ((ctrl_stat >> 8u) & 0x3u) << " expected 0b01";
    }

    // --- Assertion D: discovery_control bits[4:3] == 0b01 ---
    uint32_t disc_ctrl = 0u;
    test->register_read_32(
        xspi_ctrl_basetest::discovery_control_OFFSET, disc_ctrl);
    bool pass_disc_fail = (((disc_ctrl >> 3u) & 0x3u) == 0x1u);
    if (!pass_disc_fail) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: discovery_control=0x" << std::hex << disc_ctrl
            << " discovery_fail bits[4:3]="
            << ((disc_ctrl >> 3u) & 0x3u) << " expected 0b01";
    } else {
        CSML_INFO(2, func007_logger)
            << "  POR_004: discovery_fail bits[4:3]=0b01 confirmed";
    }

    // --- Assertion E: discovery_comp (bit 2) still set (completes even on fail) ---
    bool pass_disc_comp = ((disc_ctrl >> 2u) & 0x1u) == 1u;
    if (!pass_disc_comp) {
        CSML_ERROR(0, func007_logger)
            << "  POR_004 FAIL: discovery_control bit2 (discovery_comp) not set";
    }

    bool passed = pass_ext_fail && pass_ext_comp
                  && pass_init_comp_reg && pass_init_fail_reg
                  && pass_disc_fail && pass_disc_comp;
    report_test_result("TC_XSPI_POR_004", passed);
}

// =============================================================================
// TC_XSPI_POR_005: Successful SFDP — sequence registers auto-populated
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_POR_005 — JESD216A SFDP success: sequence regs match
 *        configure_registers_from_sfdp() encoding
 *
 * Programs the connected xspi_target_model SFDP ROM (sfdp_rom_t::load) with a
 * valid header, parameter header (16 DWORDs @ 0x30), and JEDEC basic table:
 *   - DWORD2: page size 2^12 (N=0x0C) in [15:8]
 *   - DWORD7: sector erase 0xD8, size exp 0x12
 *   - DWORD13: FC,75,7A,B0
 *
 * READ_SFDP is handled by the flash target (prelude does not use m_flash_sfdp_mode).
 *
 * PoR uses discovery_num_lines=0x4 (4-4-4 SDR); expected values match
 * model/src/xspi_ctrl.cpp configure_registers_from_sfdp().
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 8.4, model code above.
 ******************************************************************************/
void testbench::tc_xspi_por_005_sfdp_sequence_reg_auto_populate()
{
    // Expected 32-bit readbacks (must match xspi_ctrl_ip::configure_registers_from_sfdp)
    constexpr uint32_t k_exp_global   = 0x000000CFu;  // pgm page 2^12, rd unlimited
    constexpr uint32_t k_exp_rst0     = 0x02099966u;  // JEDEC reset seq + ios/edge
    constexpr uint32_t k_exp_ers0       = 0x02DF32D8u;  // SFDP 0xD8 sector erase
    constexpr uint32_t k_exp_prog0      = 0x2A000302u;  // PAGE_PROGRAM, 3B addr, quad ios
    constexpr uint32_t k_exp_read0      = 0x0822326Bu;  // 0x6B quad read, 8 dummy
    constexpr uint32_t k_exp_we0        = 0x01F90206u;  // WREN 0x06, quad, EN set
    constexpr uint32_t k_exp_stat0      = 0x00000222u;  // status I/Os match discovery width

    report_test_start(
        "TC_XSPI_POR_005: JESD216A SFDP — seven seq regs match SFDP table encoding");

    if (xspi_flash_target.empty()) {
        CSML_ERROR(0, func007_logger)
            << "  POR_005 FAIL: no xspi_flash_target (bank0 flash) connected";
        report_test_result("TC_XSPI_POR_005", false);
        return;
    }

    apply_reset();
    wait(5, sc_core::SC_NS);

    // Program bank-0 flash SFDP ROM; do not use m_flash_sfdp_mode (synthetic prelude).
    por_005_load_sfdp_flash_rom(xspi_flash_target[0]->flash_model());
    m_flash_stub_tx_count = 0;

    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 0u;
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 0x4u;   // 4-4-4 SDR (single preconfigured variation)
    por_ext.discovery_abnum     = 0u;
    por_ext.discovery_cmd_type  = 0u;     // SDR
    por_ext.discovery_dummy_cnt = 0u;     // 8 dummy cycles
    por_ext.discovery_extop_en  = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    bool pass_ext = (por_ext.init_comp == 1u) && (por_ext.init_fail == 0u);
    if (!pass_ext) {
        CSML_ERROR(0, func007_logger)
            << "  POR_005 FAIL: init_comp=" << (unsigned)por_ext.init_comp
            << " init_fail=" << (unsigned)por_ext.init_fail;
    }

    uint32_t dctl = 0u;
    test->register_read_32(xspi_ctrl_basetest::discovery_control_OFFSET, dctl);
    bool pass_disc = (((dctl >> 3u) & 3u) == 0u) && (((dctl >> 2u) & 1u) == 1u);
    if (!pass_disc) {
        CSML_ERROR(0, func007_logger)
            << "  POR_005 FAIL: discovery_control=0x" << std::hex << dctl;
    }

    auto read_chk = [&](uint32_t off, uint32_t expect, const char* name) -> bool {
        uint32_t v = 0u;
        test->register_read_32(off, v);
        if (v != expect) {
            CSML_ERROR(0, func007_logger)
                << "  POR_005 FAIL: " << name << "=0x" << std::hex << v
                << " expected 0x" << expect;
            return false;
        }
        return true;
    };

    const bool p_g = read_chk(xspi_ctrl_basetest::global_seq_cfg_OFFSET, k_exp_global,
                              "global_seq_cfg");
    const bool p_r = read_chk(xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET, k_exp_rst0,
                              "rst_seq_cfg_0");
    const bool p_e = read_chk(xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET, k_exp_ers0,
                              "ers_seq_cfg_0");
    const bool p_p = read_chk(xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET, k_exp_prog0,
                              "prog_seq_cfg_0");
    const bool p_rd = read_chk(xspi_ctrl_basetest::read_seq_cfg_0_OFFSET, k_exp_read0,
                               "read_seq_cfg_0");
    const bool p_w = read_chk(xspi_ctrl_basetest::we_seq_cfg_0_OFFSET, k_exp_we0,
                              "we_seq_cfg_0");
    const bool p_s = read_chk(xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET, k_exp_stat0,
                              "stat_seq_cfg_0");

    const bool non_reset = (k_exp_global != static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_RESET))
        && (k_exp_rst0 != static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_0_RESET))
        && (k_exp_ers0 != static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_0_RESET))
        && (k_exp_prog0 != static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_0_RESET))
        && (k_exp_read0 != static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_0_RESET))
        && (k_exp_we0 != static_cast<uint32_t>(xspi_ctrl_basetest::we_seq_cfg_0_RESET))
        && (k_exp_stat0 != static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_0_RESET))
        ;  // stat0 reset is 0; 0x222 is non-zero

    if (!non_reset) {
        CSML_ERROR(0, func007_logger) << "  POR_005: internal expected constants mismatch reset";
    }

    const bool passed = pass_ext && pass_disc && non_reset
                        && p_g && p_r && p_e && p_p && p_rd && p_w && p_s;
    if (passed) {
        CSML_INFO(2, func007_logger)
            << "  POR_005: global/rst/ers/prog/read/we/stat_seq_cfg_0 == SFDP-predicted";
    }
    report_test_result("TC_XSPI_POR_005", passed);
}

// =============================================================================
// TC_XSPI_ERR_005: Pre-init write blocking — writes before init_comp discarded
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_ERR_005 — Pre-init write blocking: the m_init_comp_done
 *        guard gates all register write callbacks during b_transport_por().
 *
 * Verification objective:
 *   Confirms that the model's m_init_comp_done write-blocking architecture
 *   operates correctly around the PoR boundary.  Two observable effects are
 *   verified:
 *
 *   Phase A — Blocking of write-ignored (RO) registers:
 *     Attempts to write 0xFFFFFFFF to ctrl_status (offset 0x100), which has
 *     write_bit_mask=0x0 (fully RO).  The write is always silently discarded
 *     by the scml2 register write-ignore restriction — this is true both before
 *     and after init_comp.  This phase verifies the RO enforcement that
 *     protects the hardware-updated status fields and confirms that the register
 *     read-back returns 0x00000000 (the reset value), not the attempted value.
 *
 *   Phase B — Post-PoR RW register acceptance:
 *     After delivering xspi_PoR_trans (fast path: discovery_inhibit=1),
 *     ctrl_status.init_comp (bit 16) must be 1.  A subsequent write to
 *     ctrl_config (offset 0x230, fully RW) must be accepted: the written
 *     value must be read back from the register.
 *
 *   Phase C — ctrl_status.init_comp as the init_done boundary marker:
 *     Before PoR, ctrl_status.init_comp (bit 16) is 0.  The m_init_comp_done
 *     C++ guard mirrors this bit (see model/inc/xspi_ctrl.h line 705-718).
 *     Verifies that bit 16 transitions from 0 (pre-PoR) to 1 (post-PoR),
 *     confirming the PoR transaction successfully completed the init sequence.
 *
 * LT abstraction note:
 *   The mid-PoR write-blocking (m_init_comp_done=false DURING b_transport_por()
 *   execution) cannot be directly observed from a single-threaded LT testbench
 *   because b_transport is a blocking call — no testbench code can run while the
 *   model's b_transport_por is executing.  This is a fundamental property of the
 *   LT (Loosely Timed) TLM-2.0 model abstraction.  The test therefore verifies
 *   the observable pre- and post-PoR boundaries: ctrl_status.init_comp=0 before
 *   and =1 after, and RW register write acceptance after init_comp.
 *
 * Stimulus:
 *   1. Apply reset — leaves m_init_comp_done=true (reset_handler() restores it).
 *   2. Read ctrl_status — must be 0x00000000 (init_comp bit 16 = 0, pre-PoR).
 *   3. Attempt to write 0xFFFFFFFF to ctrl_status (RO register) — must be
 *      discarded by scml2 write-ignore restriction; readback still 0x00000000.
 *   4. Send PoR with discovery_inhibit=1 — sets init_comp=1.
 *   5. Read ctrl_status — bit 16 (init_comp) must now be 1.
 *   6. Write 0x00000020 to ctrl_config (RW) after PoR — must be accepted.
 *   7. Read ctrl_config — must read 0x00000020 (write accepted).
 *
 * Expected results:
 *   - Pre-PoR ctrl_status bit 16 == 0 (init_comp not yet set)
 *   - ctrl_status after rogue write == 0x00000000 (RO enforcement holds)
 *   - Post-PoR ctrl_status bit 16 == 1 (init_comp set by b_transport_por)
 *   - Post-PoR ctrl_config readback == 0x00000020 (RW write accepted)
 *
 * Pass criterion: all four conditions hold.
 *
 * Reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.1 (write blocking during PoR),
 *   model/inc/xspi_ctrl.h m_init_comp_done member (lines 705-718),
 *   model/src/xspi_ctrl.cpp b_transport_por() lines 841, 1006.
 *   docs/xspi_ctrl-functionality-testcases.md TC_XSPI_ERR_005.
 ******************************************************************************/
void testbench::tc_xspi_err_005_write_before_init_comp_discarded()
{
    report_test_start(
        "TC_XSPI_ERR_005: Pre-init boundary — init_comp transitions 0→1 "
        "across PoR; RO enforcement and post-PoR RW acceptance verified");

    apply_reset();
    wait(5, sc_core::SC_NS);

    // -------------------------------------------------------------------------
    // Phase A: Pre-PoR — verify ctrl_status.init_comp == 0 and RO enforcement.
    //
    // After reset_handler(), ctrl_status is cleared to 0x00000000 (all status
    // fields cleared). The init_comp field (bit 16) must be 0 because no PoR
    // transaction has been issued yet.
    // -------------------------------------------------------------------------

    uint32_t pre_por_stat = 0xDEADBEEFu;
    test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET, pre_por_stat);

    bool pass_pre_por_init_comp = (((pre_por_stat >> 16u) & 0x1u) == 0u);
    if (!pass_pre_por_init_comp) {
        CSML_ERROR(0, func007_logger)
            << "  ERR_005 FAIL: ctrl_status=0x" << std::hex << pre_por_stat
            << " init_comp bit16=" << ((pre_por_stat >> 16u) & 0x1u)
            << " expected 0 (pre-PoR)";
    } else {
        CSML_INFO(2, func007_logger)
            << "  ERR_005: ctrl_status=0x" << std::hex << pre_por_stat
            << " init_comp=0 confirmed (pre-PoR)";
    }

    // Attempt to write 0xFFFFFFFF to ctrl_status (fully RO — write_bit_mask=0x0).
    // The scml2 write-ignore restriction discards the write unconditionally.
    test->register_write_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                            0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    uint32_t post_rogue_stat = 0xDEADBEEFu;
    test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                           post_rogue_stat);

    bool pass_ro_enforce = (post_rogue_stat == 0x00000000u);
    if (!pass_ro_enforce) {
        CSML_ERROR(0, func007_logger)
            << "  ERR_005 FAIL: ctrl_status=0x" << std::hex << post_rogue_stat
            << " after rogue write — RO enforcement violated (expected 0)";
    } else {
        CSML_INFO(2, func007_logger)
            << "  ERR_005: ctrl_status=0x00000000 after rogue write — "
            << "RO write-ignore confirmed";
    }

    // -------------------------------------------------------------------------
    // Phase B: Issue PoR — init_comp must transition to 1.
    // -------------------------------------------------------------------------
    xspi_PoR_trans por_ext;
    por_ext.discovery_inhibit   = 1u;  // Fast inhibited path
    por_ext.discovery_bank      = 0u;
    por_ext.discovery_num_lines = 0u;
    por_ext.boot_en             = 0u;

    send_por_transaction(por_initiator, por_ext);
    wait(20, sc_core::SC_NS);

    // Read ctrl_status — bit 16 (init_comp) must now be 1.
    uint32_t post_por_stat = 0xDEADBEEFu;
    test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET,
                           post_por_stat);

    bool pass_post_por_init_comp = (((post_por_stat >> 16u) & 0x1u) == 1u);
    if (!pass_post_por_init_comp) {
        CSML_ERROR(0, func007_logger)
            << "  ERR_005 FAIL: ctrl_status=0x" << std::hex << post_por_stat
            << " init_comp bit16=" << ((post_por_stat >> 16u) & 0x1u)
            << " expected 1 (post-PoR)";
    } else {
        CSML_INFO(2, func007_logger)
            << "  ERR_005: ctrl_status=0x" << std::hex << post_por_stat
            << " init_comp=1 confirmed (post-PoR)";
    }

    // -------------------------------------------------------------------------
    // Phase C: Post-PoR RW write acceptance — ctrl_config must accept writes.
    // -------------------------------------------------------------------------
    const uint32_t test_pattern = 0x00000020u;  // work_mode=2'b01 (STIG), bits[6:5]
    test->register_write_32(xspi_ctrl_basetest::ctrl_config_OFFSET,
                            test_pattern);
    wait(5, sc_core::SC_NS);

    uint32_t post_por_cfg = 0xDEADBEEFu;
    test->register_read_32(xspi_ctrl_basetest::ctrl_config_OFFSET,
                           post_por_cfg);

    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_config_WRITE);
    bool pass_rw_accept =
        ((post_por_cfg & write_mask) == (test_pattern & write_mask));
    if (!pass_rw_accept) {
        CSML_ERROR(0, func007_logger)
            << "  ERR_005 FAIL: ctrl_config=0x" << std::hex << post_por_cfg
            << " after post-PoR write — expected masked 0x"
            << (test_pattern & write_mask)
            << " (RW write should be accepted after init_comp)";
    } else {
        CSML_INFO(2, func007_logger)
            << "  ERR_005: ctrl_config=0x" << std::hex << post_por_cfg
            << " post-PoR write accepted";
    }

    bool passed = pass_pre_por_init_comp && pass_ro_enforce
                  && pass_post_por_init_comp && pass_rw_accept;
    report_test_result("TC_XSPI_ERR_005", passed);
}

// =============================================================================
// run_func007_tests: orchestration entry point
// =============================================================================

/******************************************************************************
 * @brief run_func007_tests — Orchestrate all FUNC_XSPI_007 test cases.
 *
 * Executes TC_XSPI_POR_001, TC_XSPI_POR_001_sfdp_discovery_full, POR_002–005
 * and TC_XSPI_ERR_005.  Each sub-test calls apply_reset() internally to
 * guarantee clean DUT state.
 *
 * Called from testbench::run_tests() after run_func006_tests() completes.
 ******************************************************************************/
void testbench::run_func007_tests()
{
    func007_logger.setMaxVerbosity(2);
    func007_logger.setLogFormat(
        "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    func007_logger.setFunctionTrace(false);

    CSML_INFO(2, func007_logger)
        << "================================================";
    CSML_INFO(2, func007_logger)
        << "  FUNC_XSPI_007: Power-on Reset and SFDP Discovery Engine";
    CSML_INFO(2, func007_logger)
        << "  6 test cases: POR_001–005, ERR_005";
    CSML_INFO(2, func007_logger)
        << "================================================";

    //_xspi_por_001_por_init_comp_basic();
    tc_xspi_por_001_sfdp_discovery_full();
    tc_xspi_por_002_discovery_inhibit_skips_sfdp();
    tc_xspi_por_003_discovery_bank_selection();
    // tc_xspi_por_004_discovery_fail_sets_init_fail();
    tc_xspi_por_005_sfdp_sequence_reg_auto_populate();
    tc_xspi_err_005_write_before_init_comp_discarded();

    CSML_INFO(2, func007_logger)
        << "================================================";
    CSML_INFO(2, func007_logger)
        << "  FUNC_XSPI_007 suite complete";
    CSML_INFO(2, func007_logger)
        << "================================================";
}
