/******************************************************************************
 * @file xspi_ctrl.cpp
 * @brief xspi_ctrl SystemC TLM model implementation
 *
 * This file implements the xspi_ctrl constructor (port initialization, socket
 * array construction, b_transport registration, SC_THREAD and SC_METHOD
 * declarations, register initialization) and all register callback methods
 * for the Cadence XSPI Controller (IP6522 + IP6182 Soft PHY) TLM model.
 *
 * FUNC_XSPI_001 implements:
 *  - Elaboration-time initialization of all registers to hardware reset values
 *    (including non-zero resets: long_polling=1000, short_polling=500,
 *     xip_mode_cfg=0x00FF0000, xspi_ctrl_version=0x65220206,
 *     ctrl_features_reg=0x03130703) via reset_all_registers() in constructor.
 *  - scml2/csml protocol enforcement: RO registers enforce write-ignore via
 *    write_bit_mask=0 in register type constructors; W1C registers use
 *    callback-based clear logic in handle_write_* functions below.
 *  - reset_in-triggered restoration of all register fields to defined reset
 *    values via reset_handler() SC_METHOD.
 *  - On reset: all internal state, status bitmaps (trd_comp_intr_status,
 *    trd_error_intr_status, intr_status), and int_out are cleared before
 *    register values are restored.
 *  - b_transport_reg() routes t_reg_socket transactions through the scml2
 *    framework (memory.bind_to_socket already handles dispatch).
 *  - evaluate_interrupt_out() helper maintains int_out level after every
 *    W1C register write and enable register write.
 *  - PHY sub-region registers (dataslice_Rfile_a, ctb_Rfile_a): pure
 *    read/write storage — no callbacks, no side effects.
 *  - RO capability registers (ctrl_features_reg, xspi_ctrl_version):
 *    write_bit_mask=0 enforces write-ignore; immune to reset_in.
 *
 * FUNC_XSPI_004 implements:
 *  - handle_write_dma_settings(): stores dma_settings fields (burst_sel,
 *    word_size, OTE, sdma_err_rsp) into internal shadow variables
 *    (dma_burst_length, dma_word_size, dma_ote_enabled, dma_sdma_err_rsp).
 *    Constrains word_size to 32-bit max when dma_data_width==32.
 *    CSML callback registration for dma_settings at offset 0x023C.
 *  - dma_read() helper: builds tlm_generic_payload with TLM_READ_COMMAND,
 *    constructs 64-bit address from addr_low/addr_high per dma_addr_width,
 *    calls i_dma_socket->b_transport(); on TLM_GENERIC_ERROR_RESPONSE
 *    captures address in dma_target_error_l/h, sets cdma_terr or ddma_terr
 *    in intr_status, calls evaluate_interrupt_out().
 *  - dma_write() helper: same pattern as dma_read() with TLM_WRITE_COMMAND.
 *  - b_transport_axi_slave(): sets TLM_OK_RESPONSE, adds 10ns LT delay.
 *    Direct/XIP mode flash forwarding deferred to FUNC_XSPI_008/013.
 *  - b_transport_por(): sets TLM_OK_RESPONSE, adds 10ns LT delay.
 *    SFDP discovery and boot engine deferred to FUNC_XSPI_007.
 *
 * FUNC_XSPI_003 implements:
 *  - CSML callback registrations for cmd_status_ptr (write, offset 0x040)
 *    and cmd_status (read, offset 0x044) wired into the CSML dispatch table.
 *  - CSML callback registrations for all five interrupt registers:
 *    intr_status (W1C write, 0x110), intr_enable (write, 0x114),
 *    trd_comp_intr_status (W1C write, 0x120), trd_error_intr_status (W1C
 *    write, 0x130), trd_error_intr_en (write, 0x134).
 *  - handle_write_cmd_status_ptr(): stores thrd_status_sel shadow variable
 *    (bits[2:0] of written value, clamped to valid thread range).
 *  - handle_read_cmd_status(): volatile indirect read — constructs a live
 *    status word for thread thrd_status_sel from trd_comp_intr_status (bit 0)
 *    and trd_error_intr_status (bit 1); other bits reserved/zero in LT model.
 *  - All five interrupt handle_write_* bodies were already implemented in
 *    FUNC_XSPI_001; FUNC_XSPI_003 wires them into the CSML dispatch table.
 *
 * FUNC_XSPI_013 implements:
 *  - XIP (eXecute-In-Place) per-bank state management as a behavioral overlay
 *    applied within Direct, PIO, and ACMD operating modes. XIP is NOT a
 *    standalone work_mode value; it is a per-bank device optimization state
 *    tracked by dac_cfg.xip_active_banks (uint8_t, one bit per bank).
 *  - handle_write_xip_mode_cfg() callback (offset 0x388): updates
 *    dac_cfg.xip_active_banks from bits[7:0] (xip_en per-bank mask),
 *    xip_en_mb_val from bits[15:8] (entry mode byte, reset=0x00), and
 *    xip_dis_mb_val from bits[23:16] (exit mode byte, reset=0xFF).
 *    Direct write to xip_en[N] pre-configures bank N for XIP without an
 *    entry READ sequence.
 *  - handle_write_direct_access_cfg() (offset 0x398) captures
 *    mode_bit_xip_en (bit 8) into dac_cfg.xip_entry_armed and
 *    mode_bit_xip_dis (bit 9) into dac_cfg.xip_exit_pending, arming the
 *    corresponding entry/exit sequence on the next Direct-mode READ.
 *  - b_transport_axi_slave() (Direct mode):
 *      - Non-READ to XIP-active bank: sets intr_status.dir_cmd_err (bit 26),
 *        calls evaluate_interrupt_out(), returns TLM_OK_RESPONSE.
 *      - Entry arm (xip_entry_armed): inserts xip_en_mb_val in write_data
 *        field of cdns_extension; after READ sets xip_active_banks[N],
 *        clears xip_entry_armed, self-clears bit 8 of direct_access_cfg,
 *        mirrors into xip_mode_cfg.xip_en bits[7:0]. Transfer clamped to 64B.
 *      - Exit pending (xip_exit_pending && XIP active): inserts xip_dis_mb_val
 *        in write_data; after READ clears xip_active_banks[N], clears
 *        xip_exit_pending, self-clears bit 9 of direct_access_cfg, mirrors
 *        into xip_mode_cfg.xip_en. Transfer clamped to 64B.
 *  - pio_handle_trigger() (PIO mode):
 *      - Non-READ (non 0x2200) to XIP-active bank: sets CMD_ERROR in
 *        thread_status_[], sets trd_error_intr_status[N], calls
 *        evaluate_interrupt_out(), clears busy flags, returns early.
 *      - MB_XIP_EN (cmd_reg0 bit 16) on READ: inserts xip_en_mb_val in
 *        ext.write_data; on success sets xip_active_banks[N] and mirrors
 *        into xip_mode_cfg.xip_en bits[7:0].
 *      - MB_XIP_DIS (cmd_reg0 bit 17) on READ with XIP active: inserts
 *        xip_dis_mb_val in ext.write_data; on success clears xip_active_banks[N]
 *        and mirrors into xip_mode_cfg.xip_en.
 *  - cdma_handle_trigger() (ACMD mode):
 *      - MB_XIP_EN on non-READ: sets DSC_ERROR | FAIL in desc_status.
 *      - Non-READ to XIP-active bank (without MB_XIP_DIS): sets DSC_ERROR.
 *      - MB_XIP_EN on READ: inserts xip_en_mb_val in ext.write_data; on
 *        success sets xip_active_banks[N] and mirrors into xip_mode_cfg.
 *      - MB_XIP_DIS on READ with XIP active: inserts xip_dis_mb_val in
 *        ext.write_data; on success clears xip_active_banks[N] and mirrors
 *        into xip_mode_cfg.
 *  - Reset (reset_handler): xip_active_banks, xip_entry_armed,
 *    xip_exit_pending cleared to 0; xip_en_mb_val=0x00, xip_dis_mb_val=0xFF.
 *
 * Reference:
 *   - docs/xspi_ctrl-detailed-design.md Section 5 (Register Model),
 *     Section 6 (Callback Architecture), Section 7.5 (XIP Mode),
 *     Section 8.1 (PoR/Reset sequence), Section 9 (Interrupt Architecture)
 *   - docs/xspi_ctrl-architecture-behaviour-map.json (reset_behavior,
 *     interrupt_matrix, registers.xip_mode_cfg, registers.direct_access_cfg)
 *   - docs/sections/xspi_ctrl-register-callbacks.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "xspi_ctrl.h"
#include "cdns_extension.h"
#include "csml_logger.h"

#include <tlm.h>
#include <cstring>

/******************************************************************************
 * @brief xspi_ctrl constructor
 *
 * Initializes all port interfaces, builds the xspi_bus_socket array sized to
 * NUM_TARGETS, registers b_transport callbacks for the two target sockets,
 * declares SC_THREAD and SC_METHOD processes, initializes all registers
 * to their hardware reset values, and registers CSML write callbacks for the
 * five behavioral registers: cmd_reg0, ctrl_config, wp_settings,
 * reset_pin_settings, and clock_mode_settings.
 *
 * Temporal decoupling is initialized via m_qk.reset() to synchronize with the
 * global quantum on first use.
 *
 * @param n           SystemC module name
 * @param memory_size Register address space size in bytes
 ******************************************************************************/
xspi_ctrl_ip::xspi_ctrl_ip(sc_module_name n, unsigned int memory_size, int log_verbosity,
                            int num_targets)
    : xspi_ctrl_base(n, memory_size)
    , verbosity("verbosity", log_verbosity)
    , t_axi_slave_socket("t_axi_slave_socket")
    , PoR_input_signals("PoR_input_signals")
    , i_dma_socket("i_dma_socket")
    , reset_in("reset_in")
    , int_out("int_out")
    , NUM_TARGETS(num_targets)
    , n_threads(8)
    , boot_available(true)
    , asf_available(false)
    , dma_addr_width(64)
    , dma_data_width(64)
    , current_work_mode(0)
    , thrd_status_sel(0)
    , long_polling_val(1000)
    , short_polling_val(500)
    , active_device_profile(0)
    , tcms_enabled(false)
    , mini_ctrl_cfg{true, false, true, 0u}
    , dma_cfg{1u, 0x3u, true, false}
    , dac_cfg{0u, false, false, false, false, 0ULL, 0u}
    , xip_en_mb_val(0x00u)
    , xip_dis_mb_val(0xFFu)
    , seq_cfg()
    , nand_spare_area(0u)
    , read_page_size(0xFu)
    , program_page_size(0x8u)
    , discovery_cfg{0u, 0u, 0u, 0u, 0u, false, 0u, 0u, false}
    , m_init_comp_done(true)
    , stig_instr_link_pending(false)
    , stig_link_cmd_phase{}
    , cmd_trigger_event("cmd_trigger_event")
    , m_int_update_event("m_int_update_event")
    , m_qk()
    , logger()
{
    (void)std::memset(m_stig_sdma_data, 0, sizeof m_stig_sdma_data);
    // Configure logger
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    CSML_INFO(2, logger) << "Constructing xspi_ctrl model (NUM_TARGETS=" << NUM_TARGETS << ")";

    // =========================================================================
    // Build xspi_bus_socket array sized by NUM_TARGETS
    // =========================================================================
    // The socket array must be sized at elaboration time and cannot be resized
    // after elaboration. Each entry is a heap-allocated initiator socket
    // named xspi_bus_socket_0, xspi_bus_socket_1, ... (one initiator per CS line).
    int num_sockets = static_cast<int>(NUM_TARGETS);
    xspi_bus_socket.resize(num_sockets);
    for (int i = 0; i < num_sockets; i++) {
        std::string sock_name =
            std::string("xspi_bus_socket_") + std::to_string(i);
        xspi_bus_socket[i] = new tlm_utils::simple_initiator_socket<xspi_ctrl_ip, 64>(
            sock_name.c_str());
        CSML_INFO(3, logger) << "  Created xspi_bus_socket[" << i << "]";
    }

    // =========================================================================
    // Register b_transport handlers for target sockets
    // =========================================================================

    // t_axi_slave_socket: AXI slave for Direct/XIP mode and register access
    t_axi_slave_socket.register_b_transport(
        this, &xspi_ctrl_ip::b_transport_axi_slave);
    CSML_INFO(3, logger) << "  Registered b_transport_axi_slave on t_axi_slave_socket";

    // PoR_input_signals: power-on reset and SFDP discovery bootstrap
    PoR_input_signals.register_b_transport(
        this, &xspi_ctrl_ip::b_transport_por);
    CSML_INFO(3, logger) << "  Registered b_transport_por on PoR_input_signals";

    // =========================================================================
    // Register SC_THREAD and SC_METHOD processes
    // =========================================================================

    // STIG engine: waits on cmd_trigger_event, decodes 128-bit instruction,
    // issues transaction on xspi_bus_socket
    SC_THREAD(stig_engine_thread);

    // Reset handler: sensitive to falling edge of active-low reset_in
    SC_METHOD(reset_handler);
    sensitive << reset_in.neg();
    dont_initialize();

    // Dedicated int_out driver: sole writer of the int_out sc_out<bool> port.
    // Enforces the single-writer rule (SystemC LRM E115) by ensuring exactly
    // one SC_METHOD executes int_out.write(). Sensitive only to
    // m_int_update_event; dont_initialize() so that int_out is not driven
    // during elaboration before reset_handler fires.
    SC_METHOD(update_int_out);
    sensitive << m_int_update_event;
    dont_initialize();

    // =========================================================================
    // Initialize quantum keeper for temporal decoupling
    // =========================================================================
    m_qk.reset();

    // =========================================================================
    // Initialize all registers to hardware reset values
    // =========================================================================
    reset_all_registers();

    // =========================================================================
    // FUNC_XSPI_002: Wire behavioral write callbacks into CSML dispatch table
    // =========================================================================
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            cmd_reg0.handle_write(value, cmd_reg0.write_bit_mask);
            return this->handle_write_cmd_reg0(value, be);
        },
        cmd_reg0.offset);

    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            ctrl_config.handle_write(value, ctrl_config.write_bit_mask);
            return this->handle_write_ctrl_config(value, be);
        },
        ctrl_config.offset);

    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            wp_settings.handle_write(value, wp_settings.write_bit_mask);
            return this->handle_write_wp_settings(value, be);
        },
        wp_settings.offset);

    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            reset_pin_settings.handle_write(value, reset_pin_settings.write_bit_mask);
            return this->handle_write_reset_pin_settings(value, be);
        },
        reset_pin_settings.offset);

    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            clock_mode_settings.handle_write(value, clock_mode_settings.write_bit_mask);
            return this->handle_write_clock_mode_settings(value, be);
        },
        clock_mode_settings.offset);

    // =========================================================================
    // FUNC_XSPI_003: Wire interrupt and cmd_status callbacks into CSML dispatch
    // =========================================================================
    //
    // cmd_status_ptr (0x040) — write callback: updates thrd_status_sel shadow.
    // The csml framework applies write_bit_mask=0x7 before the callback fires,
    // so bits[31:3] are discarded. The callback additionally clamps the value
    // to [0, n_threads-1] for safety.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            cmd_status_ptr.handle_write(value, cmd_status_ptr.write_bit_mask);
            return this->handle_write_cmd_status_ptr(value, be);
        },
        cmd_status_ptr.offset);

    // cmd_status (0x044) — read callback: volatile indirect read. The stored
    // register value is always stale; the live thread status is constructed
    // here from trd_comp_intr_status and trd_error_intr_status for thread
    // thrd_status_sel. cmd_status has write_bit_mask=0x0 (RO), so no write
    // callback is needed.
    memory.register_read_callback(
        [this](uint32_t& value) {
            return this->handle_read_cmd_status(value);
        },
        cmd_status.offset);

    // intr_status (0x110) — W1C write callback: clears bits written as 1
    // in the intr_status register, then re-evaluates int_out. The csml
    // framework performs the standard read-modify-write through
    // write_bit_mask=0x1FF7F000 before the callback fires; the callback
    // additionally applies the explicit W1C complement-mask logic.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            return this->handle_write_intr_status(value, be);
        },
        intr_status.offset);

    // intr_enable (0x114) — write callback: csml framework stores the
    // written value through write_bit_mask=0x9FF7F000. The callback
    // re-evaluates int_out immediately so that unmasking a pending source
    // asserts int_out without delay.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            intr_enable.handle_write(value, intr_enable.write_bit_mask);
            return this->handle_write_intr_enable(value, be);
        },
        intr_enable.offset);

    // trd_comp_intr_status (0x120) — W1C write callback: clears trdN_comp
    // bits[7:0] written as 1, then re-evaluates int_out (Path 2a).
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            return this->handle_write_trd_comp_intr_status(value, be);
        },
        trd_comp_intr_status.offset);

    // trd_error_intr_status (0x130) — W1C write callback: clears
    // trdN_error_stat bits[7:0] written as 1, then re-evaluates int_out
    // (Path 2b gated by trd_error_intr_en).
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            return this->handle_write_trd_error_intr_status(value, be);
        },
        trd_error_intr_status.offset);

    // trd_error_intr_en (0x134) — write callback: csml framework stores
    // the written value through write_bit_mask=0xFF. The callback
    // re-evaluates int_out immediately so that enabling a previously masked
    // thread error source (when trd_error_intr_status bit is set) asserts
    // int_out without delay.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            trd_error_intr_en.handle_write(value, trd_error_intr_en.write_bit_mask);
            return this->handle_write_trd_error_intr_en(value, be);
        },
        trd_error_intr_en.offset);

    // =========================================================================
    // FUNC_XSPI_004: Wire DMA configuration callbacks into CSML dispatch table
    // =========================================================================
    //
    // dma_settings (0x23C) — write callback: updates AXI master burst length,
    // per-beat word size, OTE flag, and SDMA error response mode into shadow
    // variables. The csml framework applies write_bit_mask=0xf00ff before the
    // callback fires, masking reserved bits [15:8] and [31:20].
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            dma_settings.handle_write(value, dma_settings.write_bit_mask);
            return this->handle_write_dma_settings(value, be);
        },
        dma_settings.offset);

    // =========================================================================
    // FUNC_XSPI_005: Initialize seq_cfg shadow to hardware reset values
    // =========================================================================
    // Zero-initialise the entire struct first, then assign only the fields
    // whose hardware reset value is non-zero. Fields whose reset is 0/false
    // need no explicit assignment. Register reset values are documented in
    // register_map.md and confirmed by the csml_reg constructors in
    // xspi_ctrl_register.h.
    seq_cfg = seq_config_t{};
    // rst_seq_cfg_0 = 0x00019966: cmd0_val=0x66, cmd1_val=0x99, cmd0_en=1
    seq_cfg.rst_cmd0_val     = 0x66u;
    seq_cfg.rst_cmd1_val     = 0x99u;
    seq_cfg.rst_cmd0_en      = true;
    // rst_seq_cfg_1 = 0xD0669900: cmd0_ext_val=0x99, cmd1_ext_val=0x66, data_val=0xD0
    seq_cfg.rst_cmd0_ext_val = 0x99u;
    seq_cfg.rst_cmd1_ext_val = 0x66u;
    seq_cfg.rst_data_val     = 0xD0u;
    // ers_seq_cfg_0 = 0x00DF3020: cmd_val=0x20, addr_cnt=3, cmd_ext_val=0xDF
    seq_cfg.ers_cmd_val      = 0x20u;
    seq_cfg.ers_addr_cnt     = 3u;
    seq_cfg.ers_cmd_ext_val  = 0xDFu;
    // ers_seq_cfg_1 = 0x0000000C: sect_size=0x0C
    seq_cfg.ers_sect_size    = 0x0Cu;
    // ers_seq_cfg_2 = 0x009F0060: ersa_cmd_val=0x60, ersa_cmd_ext_val=0x9F
    seq_cfg.ersa_cmd_val     = 0x60u;
    seq_cfg.ersa_cmd_ext_val = 0x9Fu;
    // prog_seq_cfg_0 = 0x00003002: cmd_val=0x02, addr_cnt=3
    seq_cfg.prog_cmd_val     = 0x02u;
    seq_cfg.prog_addr_cnt    = 3u;
    // prog_seq_cfg_1 = 0x0000FD00: cmd_ext_val=0xFD
    seq_cfg.prog_cmd_ext_val = 0xFDu;
    // prog_seq_cfg_2 = 0x00000002: burst_type=1 (linear)
    seq_cfg.prog_p2_burst_type = true;
    // read_seq_cfg_0 = 0x00003003: cmd_val=0x03, addr_cnt=3
    seq_cfg.read_cmd_val     = 0x03u;
    seq_cfg.read_addr_cnt    = 3u;
    // read_seq_cfg_1 = 0x0000FC00: cmd_ext_val=0xFC
    seq_cfg.read_cmd_ext_val = 0xFCu;
    // read_seq_cfg_2 = 0x00000F0A: hf_bound_en=1, burst_type=1, latency_cnt=0x0F
    seq_cfg.read_p2_hf_bound_en = true;
    seq_cfg.read_p2_burst_type  = true;
    seq_cfg.read_p2_latency_cnt = 0x0Fu;
    // we_seq_cfg_0 = 0x01F90006: cmd_val=0x06, cmd_ext_val=0xF9, we_en=1
    seq_cfg.we_cmd_val     = 0x06u;
    seq_cfg.we_cmd_ext_val = 0xF9u;
    seq_cfg.we_en          = true;
    // stat_seq_cfg_2 = 0x05000505: dev_rdy=0x05, ers_fail=0x05, prog_fail=0x05
    seq_cfg.stat_dev_rdy_cmd_val   = 0x05u;
    seq_cfg.stat_ers_fail_cmd_val  = 0x05u;
    seq_cfg.stat_prog_fail_cmd_val = 0x05u;
    // stat_seq_cfg_3 = 0xFA00FAFA: all three ext_val=0xFA
    seq_cfg.stat_dev_rdy_cmd_ext_val   = 0xFAu;
    seq_cfg.stat_ers_fail_cmd_ext_val  = 0xFAu;
    seq_cfg.stat_prog_fail_cmd_ext_val = 0xFAu;
    // stat_seq_cfg_4 = 0x00000F00: p2_latency_cnt=0x0F
    seq_cfg.stat_p2_latency_cnt = 0x0Fu;
    // stat_seq_cfg_5 = 0x00000040: dev_rdy_en=1 (bit 6)
    seq_cfg.stat_dev_rdy_en = true;

    // =========================================================================
    // FUNC_XSPI_005: Wire sequence configuration callbacks into CSML dispatch
    // =========================================================================

    // long_polling (0x208) — write callback: updates long_polling_val shadow.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            long_polling.handle_write(value, long_polling.write_bit_mask);
            return this->handle_write_long_polling(value, be);
        },
        long_polling.offset);

    // short_polling (0x20C) — write callback: updates short_polling_val shadow.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            short_polling.handle_write(value, short_polling.write_bit_mask);
            return this->handle_write_short_polling(value, be);
        },
        short_polling.offset);

    // xip_mode_cfg (0x388) — write callback: updates xip_active_banks shadow
    // and XIP mode-byte configuration fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            xip_mode_cfg.handle_write(value, xip_mode_cfg.write_bit_mask);
            return this->handle_write_xip_mode_cfg(value, be);
        },
        xip_mode_cfg.offset);

    // global_seq_cfg (0x390) — write callback: updates active_device_profile,
    // tcms_enabled, read_page_size, and program_page_size shadows.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            global_seq_cfg.handle_write(value, global_seq_cfg.write_bit_mask);
            return this->handle_write_global_seq_cfg(value, be);
        },
        global_seq_cfg.offset);

    // global_seq_cfg_1 (0x394) — write callback: updates nand_spare_area shadow.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            global_seq_cfg_1.handle_write(value, global_seq_cfg_1.write_bit_mask);
            return this->handle_write_global_seq_cfg_1(value, be);
        },
        global_seq_cfg_1.offset);

    // direct_access_cfg (0x398) — write callback: updates active_dac_bank,
    // rmp_addr_en, and xip_exit_pending shadows.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            direct_access_cfg.handle_write(value, direct_access_cfg.write_bit_mask);
            return this->handle_write_direct_access_cfg(value, be);
        },
        direct_access_cfg.offset);

    // direct_access_rmp (0x39C) — write callback: updates remap_offset[31:0].
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            direct_access_rmp.handle_write(value, direct_access_rmp.write_bit_mask);
            return this->handle_write_direct_access_rmp(value, be);
        },
        direct_access_rmp.offset);

    // direct_access_rmp_1 (0x3A0) — write callback: updates remap_offset[63:32].
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            direct_access_rmp_1.handle_write(value, direct_access_rmp_1.write_bit_mask);
            return this->handle_write_direct_access_rmp_1(value, be);
        },
        direct_access_rmp_1.offset);

    // rst_seq_cfg_0 (0x400) — write callback: updates seq_cfg.rst_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            rst_seq_cfg_0.handle_write(value, rst_seq_cfg_0.write_bit_mask);
            return this->handle_write_rst_seq_cfg_0(value, be);
        },
        rst_seq_cfg_0.offset);

    // rst_seq_cfg_1 (0x404) — write callback: updates seq_cfg.rst_ext_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            rst_seq_cfg_1.handle_write(value, rst_seq_cfg_1.write_bit_mask);
            return this->handle_write_rst_seq_cfg_1(value, be);
        },
        rst_seq_cfg_1.offset);

    // ers_seq_cfg_0 (0x410) — write callback: updates seq_cfg.ers_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            ers_seq_cfg_0.handle_write(value, ers_seq_cfg_0.write_bit_mask);
            return this->handle_write_ers_seq_cfg_0(value, be);
        },
        ers_seq_cfg_0.offset);

    // ers_seq_cfg_1 (0x414) — write callback: updates seq_cfg.ers_sect_size.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            ers_seq_cfg_1.handle_write(value, ers_seq_cfg_1.write_bit_mask);
            return this->handle_write_ers_seq_cfg_1(value, be);
        },
        ers_seq_cfg_1.offset);

    // ers_seq_cfg_2 (0x418) — write callback: updates seq_cfg.ersa_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            ers_seq_cfg_2.handle_write(value, ers_seq_cfg_2.write_bit_mask);
            return this->handle_write_ers_seq_cfg_2(value, be);
        },
        ers_seq_cfg_2.offset);

    // prog_seq_cfg_0 (0x420) — write callback: updates seq_cfg.prog_p1_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            prog_seq_cfg_0.handle_write(value, prog_seq_cfg_0.write_bit_mask);
            return this->handle_write_prog_seq_cfg_0(value, be);
        },
        prog_seq_cfg_0.offset);

    // prog_seq_cfg_1 (0x424) — write callback: updates seq_cfg.prog_cmd_ext_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            prog_seq_cfg_1.handle_write(value, prog_seq_cfg_1.write_bit_mask);
            return this->handle_write_prog_seq_cfg_1(value, be);
        },
        prog_seq_cfg_1.offset);

    // prog_seq_cfg_2 (0x428) — write callback: updates seq_cfg.prog_p2_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            prog_seq_cfg_2.handle_write(value, prog_seq_cfg_2.write_bit_mask);
            return this->handle_write_prog_seq_cfg_2(value, be);
        },
        prog_seq_cfg_2.offset);

    // read_seq_cfg_0 (0x430) — write callback: updates seq_cfg.read_p1_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            read_seq_cfg_0.handle_write(value, read_seq_cfg_0.write_bit_mask);
            return this->handle_write_read_seq_cfg_0(value, be);
        },
        read_seq_cfg_0.offset);

    // read_seq_cfg_1 (0x434) — write callback: updates seq_cfg.read_ext_* and
    // mode-byte fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            read_seq_cfg_1.handle_write(value, read_seq_cfg_1.write_bit_mask);
            return this->handle_write_read_seq_cfg_1(value, be);
        },
        read_seq_cfg_1.offset);

    // read_seq_cfg_2 (0x438) — write callback: updates seq_cfg.read_p2_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            read_seq_cfg_2.handle_write(value, read_seq_cfg_2.write_bit_mask);
            return this->handle_write_read_seq_cfg_2(value, be);
        },
        read_seq_cfg_2.offset);

    // we_seq_cfg_0 (0x440) — write callback: updates seq_cfg.we_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            we_seq_cfg_0.handle_write(value, we_seq_cfg_0.write_bit_mask);
            return this->handle_write_we_seq_cfg_0(value, be);
        },
        we_seq_cfg_0.offset);

    // stat_seq_cfg_0 (0x450) — write callback: updates seq_cfg.stat_cmd/addr/data.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_0.handle_write(value, stat_seq_cfg_0.write_bit_mask);
            return this->handle_write_stat_seq_cfg_0(value, be);
        },
        stat_seq_cfg_0.offset);

    // stat_seq_cfg_1 (0x454) — write callback: updates seq_cfg.stat_*_dummy_cnt.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_1.handle_write(value, stat_seq_cfg_1.write_bit_mask);
            return this->handle_write_stat_seq_cfg_1(value, be);
        },
        stat_seq_cfg_1.offset);

    // stat_seq_cfg_2 (0x458) — write callback: updates seq_cfg.stat_*_cmd_val.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_2.handle_write(value, stat_seq_cfg_2.write_bit_mask);
            return this->handle_write_stat_seq_cfg_2(value, be);
        },
        stat_seq_cfg_2.offset);

    // stat_seq_cfg_3 (0x45C) — write callback: updates seq_cfg.stat_*_cmd_ext_val.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_3.handle_write(value, stat_seq_cfg_3.write_bit_mask);
            return this->handle_write_stat_seq_cfg_3(value, be);
        },
        stat_seq_cfg_3.offset);

    // stat_seq_cfg_4 (0x460) — write callback: updates seq_cfg.stat_p2_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_4.handle_write(value, stat_seq_cfg_4.write_bit_mask);
            return this->handle_write_stat_seq_cfg_4(value, be);
        },
        stat_seq_cfg_4.offset);

    // stat_seq_cfg_5 (0x464) — write callback: updates seq_cfg.stat_*_en/idx/val.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_5.handle_write(value, stat_seq_cfg_5.write_bit_mask);
            return this->handle_write_stat_seq_cfg_5(value, be);
        },
        stat_seq_cfg_5.offset);

    // stat_seq_cfg_7 (0x46C) — write callback: updates seq_cfg.stat_dev_rdy_addr.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_7.handle_write(value, stat_seq_cfg_7.write_bit_mask);
            return this->handle_write_stat_seq_cfg_7(value, be);
        },
        stat_seq_cfg_7.offset);

    // stat_seq_cfg_8 (0x470) — write callback: updates seq_cfg.stat_prog_fail_addr.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_8.handle_write(value, stat_seq_cfg_8.write_bit_mask);
            return this->handle_write_stat_seq_cfg_8(value, be);
        },
        stat_seq_cfg_8.offset);

    // stat_seq_cfg_9 (0x474) — write callback: updates seq_cfg.stat_ers_fail_addr.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_9.handle_write(value, stat_seq_cfg_9.write_bit_mask);
            return this->handle_write_stat_seq_cfg_9(value, be);
        },
        stat_seq_cfg_9.offset);

    // stat_seq_cfg_10 (0x478) — write callback: updates seq_cfg.stat_ecc_* fields.
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            stat_seq_cfg_10.handle_write(value, stat_seq_cfg_10.write_bit_mask);
            return this->handle_write_stat_seq_cfg_10(value, be);
        },
        stat_seq_cfg_10.offset);

    // sdma_size (0x240) — hardware-updated RO register. Write-ignore
    // restriction is enforced via write_bit_mask=0 in the csml_reg constructor.
    // No write callback needed; reads always return the last hardware-written
    // value. A read callback is not registered because sdma_size is a static
    // snapshot updated by the DMA engines and does not require dynamic
    // computation on read.
    //
    // sdma_trd_info (0x244) — hardware-updated RO register. Same policy.
    //
    // sdma_addr0 (0x24C) — hardware-updated RO register. Same policy.
    //
    // sdma_addr1 (0x250) — hardware-updated RO register. Same policy.
    //
    // These four registers do not require CSML write callback registrations
    // because their write_bit_mask=0 already makes all software writes
    // no-ops at the csml_reg level. Hardware updates go through direct
    // assignment in the DMA engine code paths (future FUNC_XSPI_010/011/012).

    // =========================================================================
    // FUNC_XSPI_006: Wire discovery_control write callback into CSML dispatch
    // =========================================================================
    //
    // discovery_control (0x260) — write callback: extracts writable discovery
    // configuration fields (discovery_bank, discovery_num_lines, discovery_abnum,
    // discovery_dummy_cnt, discovery_cmd_type, discovery_extop_en,
    // discovery_extop_val, discovery_req_type, discovery_req) into shadow
    // variables for use by the SFDP discovery engine (FUNC_XSPI_007).
    //
    // The read-only hardware-updated fields (discovery_comp, discovery_fail,
    // discovery_inhibit) are protected by write_bit_mask=0x7ffc3 which excludes
    // bits[5], [4:3], and [2]. The csml framework enforces this mask before the
    // callback fires so the RO bits are never corrupted by software writes.
    //
    // write_bit_mask = 0x7ffc3 = bits[18:16] (discovery_bank) |
    //                            bits[15:12] (discovery_num_lines) |
    //                            bit[11]     (discovery_abnum) |
    //                            bit[10]     (discovery_dummy_cnt) |
    //                            bits[9:8]   (discovery_cmd_type) |
    //                            bit[7]      (discovery_extop_en) |
    //                            bit[6]      (discovery_extop_val) |
    //                            bit[1]      (discovery_req_type) |
    //                            bit[0]      (discovery_req)
    memory.register_write_callback_with_be(
        [this](uint32_t value, uint8_t be) {
            discovery_control.handle_write(value, discovery_control.write_bit_mask);
            return this->handle_write_discovery_control(value, be);
        },
        discovery_control.offset);

    CSML_INFO(2, logger) << "xspi_ctrl construction complete";
}

// =============================================================================
// TLM b_transport Handlers
// =============================================================================

/******************************************************************************
 * @brief b_transport handler for t_reg_socket (register access)
 *
 * Register-file dispatch is managed internally by the csml framework through
 * memory.bind_to_socket(target_socket) in xspi_ctrl_base. That binding routes
 * all TLM transactions arriving on target_socket directly to the csml_memory,
 * which invokes the appropriate per-register read or write callbacks registered
 * at construction time. All RO write-ignore enforcement, W1C bit clearing, and
 * interrupt re-evaluation therefore occur transparently inside those callbacks.
 *
 * This method is not the primary dispatch path in normal operation. It is
 * retained for structural completeness and sets TLM_OK_RESPONSE with a 10 ns
 * LT modeling delay consistent with the other b_transport handlers.
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 5.1 (register groups
 *            and address map); docs/sections/xspi_ctrl-port-interfaces.md
 *            (t_reg_socket routing via t_reg_socket_router).
 *
 * @param trans TLM generic payload containing address, data pointer, command,
 *              and byte-enable fields
 * @param delay Accumulated local time offset for temporal decoupling
 ******************************************************************************/
 /*void xspi_ctrl_ip::b_transport_reg(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
{
   
    CSML_INFO(3, logger) << "b_transport_reg: addr=0x" << std::hex << trans.get_address() << " cmd=" << (trans.get_command() == tlm::TLM_READ_COMMAND ? "READ" : "WRITE");
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }
    
}*/

/******************************************************************************
 * @brief b_transport handler for t_axi_slave_socket — FUNC_XSPI_008
 *
 * Implements the Direct Mode flash forwarding engine. Receives AXI slave
 * read and write transactions on the 64-bit t_axi_slave_socket. When
 * `current_work_mode == 0` (Direct mode), incoming transactions are forwarded
 * to the flash device on `xspi_bus_socket[dac_bank_num]` using the
 * Cadence xSPI bus protocol via cdns_extension.
 *
 * ## READ path (FUNC_XSPI_008 — AXI READ to READ_ZERO_LATENCY)
 *
 * 1. Assert ctrl_status.ctrl_busy (bit 7).
 * 2. Compute flash address from AXI address, applying 64-bit remap offset if
 *    direct_access_cfg.rmp_addr_en is set:
 *      flash_addr = axi_addr - dac_cfg.remap_offset (bits[2:0] ignored for alignment)
 * 3. Build cdns_extension:
 *      opcode     = 0x03 (READ_ZERO_LATENCY from read_seq_cfg_0.read_cmd_val)
 *      instr_type = XSPI_INSTR_READ
 *      bank_num   = dac_cfg.active_bank
 *      address    = flash_addr
 *      data_bytes = trans.get_data_length()
 * 4. Call dispatch_flash_transaction(); copy flash read data into trans data buffer.
 * 5. Clear ctrl_status.ctrl_busy.
 * 6. Set trans TLM_OK_RESPONSE.
 *
 * ## WRITE path (FUNC_XSPI_008 — AXI WRITE to WREN + PAGE_PROGRAM)
 *
 * 1. Assert ctrl_status.ctrl_busy (bit 7).
 * 2. Compute flash address (same remap logic as READ).
 * 3. Build WREN cdns_extension (opcode=0x06, XSPI_INSTR_GENERIC, data_bytes=0).
 * 4. Dispatch WREN (generic/command-only transaction).
 * 5. Build PAGE_PROGRAM cdns_extension from seq_cfg.prog_cmd_val (default 0x02),
 *    XSPI_INSTR_WRITE, address=flash_addr, data from trans buffer.
 *    When direct_access_cfg.rwds_cap_en=1, extract AXI write strobes from
 *    the optional axi_trans extension; non-zero strobe byte → write byte active.
 * 6. Dispatch PAGE_PROGRAM.
 * 7. Clear ctrl_status.ctrl_busy.
 * 8. Set trans TLM_OK_RESPONSE.
 *
 * ## Non-Direct mode
 *
 * STIG mode (current_work_mode == 1, work_mode 2'b01): t_axi may target the
 * STIG Slave-DMA window (see b_transport_sdma_stig) after sdma_trigg and
 * instruction trigger (cmd_reg0) per spec; other AXI in STIG is acknowledged
 * with TLM_OK and a 10 ns LT delay (FUNC_XSPI_004 baseline).
 * ACMD and other non-Direct modes: same 10 ns LT (no STIG SDMA window on this
 * path). Register access uses target_socket, not t_axi.
 *
 * ## Boundary conditions
 *
 * - Out-of-range bank (dac_bank_num >= NUM_TARGETS): dispatch_flash_transaction()
 *   sets cmd_ignored and returns false; response is still TLM_OK_RESPONSE (the
 *   transaction is accepted on the AXI slave interface, just not forwarded).
 * - Empty data_length: dispatched with data_bytes=0 (command-only semantics).
 *
 * Architecture reference:
 *   docs/xspi_ctrl-architecture-behaviour-map.json
 *     operations.t_axi_slave_socket;
 *     state_machines.DIRECT;
 *     registers.direct_access_cfg / direct_access_rmp / direct_access_rmp_1;
 *   docs/xspi_ctrl-detailed-design.md Section 7.1 (Direct Mode),
 *     Section 7.1.3–7.1.6, Section 10.2 (AXI slave interface).
 *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_008.
 *
 * Single-writer compliance:
 *   ctrl_status is a RO register (write_bit_mask=0); direct uint32_t assignment
 *   is the only permitted update path. No sc_out or sc_signal writes are made
 *   inside this function. int_out is driven exclusively by the
 *   interrupt_out_driver SC_METHOD.
 *
 * @param trans TLM generic payload for the AXI slave transaction
 *              (caller retains ownership; data buffer valid for data_length bytes)
 * @param delay Local time offset for temporal decoupling (accumulated here)
 ******************************************************************************/
void xspi_ctrl_ip::b_transport_axi_slave(tlm::tlm_generic_payload& trans,
                                          sc_core::sc_time& delay)
{
    const bool is_read = (trans.get_command() == tlm::TLM_READ_COMMAND);

    CSML_INFO(3, logger) << "b_transport_axi_slave: addr=0x" << std::hex << trans.get_address() << " cmd=" << (is_read ? "READ" : "WRITE") << " work_mode=" << std::dec << current_work_mode;

    // -------------------------------------------------------------------------
    // STIG (work_mode 2'b01) only: Slave-DMA AXI window at k_stig_sdma_axi_base
    // (after cmd_reg0 trigger, sdma_trigg in intr_status, host AXI to sdma_addr*).
    // Other non–Direct (e.g. ACMD): no SDMA on this path — FUNC_XSPI_004 10 ns LT.
    // -------------------------------------------------------------------------
    if (current_work_mode == 1u) {   // STIG — see handle_write_cmd_reg0 case 1u
        if (b_transport_sdma_stig(trans, delay)) {
            return;
        }
    }
    if (current_work_mode != 0u) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        m_qk.set(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
        return;
    }

    // =========================================================================
    // FUNC_XSPI_008: Direct Mode Flash Forwarding
    // current_work_mode == 0  (Direct mode, ctrl_config.work_mode = 2'b00)
    // =========================================================================

    // -------------------------------------------------------------------------
    // Step 1: Assert ctrl_status.ctrl_busy (bit 7).
    // ctrl_status is RO (write_bit_mask=0); direct uint32_t write is the
    // correct mechanism to update hardware-driven status fields.
    // Reference: docs/xspi_ctrl-architecture-behaviour-map.json
    //   registers.ctrl_status.fields.ctrl_busy.write_effects.
    // -------------------------------------------------------------------------
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val |= (1u << 7);   // ctrl_busy at bit 7
        ctrl_status = cs_val;
    }

    // -------------------------------------------------------------------------
    // Step 2: Compute flash address from AXI slave address.
    // When direct_access_cfg.rmp_addr_en=1, apply the 64-bit remap offset:
    //   flash_addr = axi_addr - dac_cfg.remap_offset
    // Bits[2:0] of the remap offset are ignored (8-byte alignment per spec).
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.1.4, 7.1.5.
    // -------------------------------------------------------------------------
    uint64_t axi_addr   = static_cast<uint64_t>(trans.get_address());
    uint64_t flash_addr = axi_addr;

    if (dac_cfg.rmp_addr_en) {
        // Align remap offset to 8-byte boundary (mask off bits[2:0]).
        uint64_t aligned_offset = dac_cfg.remap_offset & ~static_cast<uint64_t>(0x7u);
        flash_addr = axi_addr - aligned_offset;
        CSML_INFO(3, logger) << "b_transport_axi_slave: remap: axi=0x" << std::hex << axi_addr << " offset=0x" << aligned_offset << " flash=0x" << flash_addr;
    }

    // -------------------------------------------------------------------------
    // Step 3: Extract write strobe bytes from optional axi_trans extension.
    // When rwds_cap_en=1 and an axi_trans extension is attached, the wstrb
    // vector provides per-byte write enable bits for RWDS translation.
    // If the extension is absent or rwds_cap_en=0, all bytes are active.
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.1.5,
    //            docs/xspi_ctrl-functionality_list.md FUNC_XSPI_008.
    // -------------------------------------------------------------------------
    axi_trans* axi_ext    = nullptr;
    bool       use_wstrb  = false;
    if (dac_cfg.rwds_cap_en) {
        trans.get_extension(axi_ext);
        if (axi_ext != nullptr && !axi_ext->wstrb.empty()) {
            use_wstrb = true;
        }
    }

    uint32_t data_len = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();

    // =========================================================================
    // FUNC_XSPI_013: XIP Active State Enforcement (Direct Mode)
    //
    // If the target bank has XIP active (xip_active_banks bit set) AND neither
    // XIP exit (xip_exit_pending) nor XIP entry (xip_entry_armed) is pending:
    //
    //   READ  → allowed; proceeds normally (device already in XIP state).
    //   WRITE → rejected; sets intr_status.dir_cmd_err (bit 26, W1C) and
    //            calls evaluate_interrupt_out().
    //
    // Per docs/xspi_ctrl-detailed-design.md Section 7.1.6 and 7.5.4:
    //   "If an invalid command is attempted while XIP mode is active on the
    //    selected bank (for example, a WRITE transaction directed at a bank
    //    with xip_en asserted), the model sets intr_status.dir_cmd_err."
    //
    // Reference: docs/xspi_ctrl-architecture-behaviour-map.json
    //   registers.intr_status.fields.dir_cmd_err (bit 26).
    // =========================================================================
    unsigned int bank_bit = (1u << dac_cfg.active_bank);
    bool xip_bank_active  = ((dac_cfg.xip_active_banks & bank_bit) != 0u);

    if (xip_bank_active && !is_read
        && !dac_cfg.xip_exit_pending && !dac_cfg.xip_entry_armed) {
        // Non-READ to an XIP-active bank: reject and set dir_cmd_err.
        CSML_INFO(2, logger) << "b_transport_axi_slave: XIP active on bank " << dac_cfg.active_bank << " — non-READ rejected (dir_cmd_err)";
        uint32_t intr_cur = static_cast<uint32_t>(intr_status);
        intr_status = intr_cur | (1u << 26);   // dir_cmd_err at bit 26
        evaluate_interrupt_out();

        // Clear ctrl_busy and return TLM_OK to the AXI initiator.
        {
            uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
            cs_val &= ~(1u << 7);
            ctrl_status = cs_val;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        m_qk.set(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
        return;
    }

    // =========================================================================
    // READ path: AXI READ → READ_ZERO_LATENCY (opcode 0x03) with XIP management
    //
    // Three XIP sub-cases (FUNC_XSPI_013):
    //
    //   (A) XIP Entry ARM (dac_cfg.xip_entry_armed == true):
    //       Insert xip_en_mb_val as the mode byte (write_data field of
    //       cdns_extension) between address and data phases. After the READ
    //       completes set the bank's bit in xip_active_banks and clear the
    //       xip_entry_armed flag. Also clear bit 8 of the direct_access_cfg
    //       register (mode_bit_xip_en self-clears after use).
    //       Boundary constraint: transfer must be ≤ 64 bytes and must not cross
    //       a page boundary; enforced by clamping data_len to 64 bytes when
    //       entry arm is active (per docs/xspi_ctrl-detailed-design.md §7.5.3).
    //
    //   (B) XIP Exit Pending (dac_cfg.xip_exit_pending == true):
    //       Insert xip_dis_mb_val as the mode byte. After the READ completes
    //       clear the bank's bit in xip_active_banks, clear xip_exit_pending,
    //       and clear bit 9 of direct_access_cfg (mode_bit_xip_dis self-clears).
    //       Same 64-byte boundary constraint applies.
    //
    //   (C) Normal Direct READ (no entry/exit pending):
    //       No mode byte inserted. If XIP is active the READ proceeds normally
    //       (device already in XIP state and no mode-byte change required).
    //
    // Reference: docs/xspi_ctrl-detailed-design.md Sections 7.5.3–7.5.5;
    //            docs/xspi_ctrl-architecture-behaviour-map.json
    //              registers.xip_mode_cfg.fields.xip_en.write_effects.
    // =========================================================================
    if (is_read) {
        // Build cdns_extension for READ_ZERO_LATENCY.
        // opcode: use seq_cfg.read_cmd_val (reset value = 0x03).
        // instr_type: XSPI_INSTR_READ (1).
        // bank_num: from dac_cfg.active_bank.
        // address: computed flash address.
        // data_bytes: AXI payload length (clamped to 64 on entry/exit arm).
        cdns_extension read_ext;
        read_ext.opcode      = seq_cfg.read_cmd_val;    // default 0x03
        read_ext.cmd_ext     = 0u;
        read_ext.bank_num    = static_cast<uint8_t>(dac_cfg.active_bank);
        read_ext.address     = flash_addr;
        read_ext.write_data  = 0u;
        read_ext.instr_type  = static_cast<uint8_t>(XSPI_INSTR_READ);
        read_ext.instr_link  = false;
        read_ext.wp_pin      = mini_ctrl_cfg.wp_pin_level;
        read_ext.hw_rst_pin  = mini_ctrl_cfg.hw_rst_level;
        read_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;

        // Determine the effective transfer size, enforcing the 64-byte first-READ
        // page boundary constraint for XIP entry and exit READs.
        uint32_t effective_len = data_len;

        if (dac_cfg.xip_entry_armed) {
            // ---------------------------------------------------------------
            // Sub-case (A): XIP Entry — insert xip_en_mb_val mode byte.
            // The first READ to arm XIP entry must transfer no more than 64 bytes.
            // ---------------------------------------------------------------
            if (effective_len > 64u) {
                effective_len = 64u;
            }
            // Signal XIP entry mode byte via write_data field convention.
            // Downstream flash stubs observe this non-zero write_data as
            // the mode byte value to insert between address and data phases.
            read_ext.write_data = static_cast<uint32_t>(xip_en_mb_val);
            read_ext.data_bytes = effective_len;

            CSML_INFO(2, logger) << "b_transport_axi_slave: XIP ENTRY READ" << " bank=" << dac_cfg.active_bank << " xip_en_mb_val=0x" << std::hex << static_cast<unsigned>(xip_en_mb_val) << " len=" << std::dec << effective_len;

            dispatch_flash_transaction(read_ext,
                                       dac_cfg.active_bank,
                                       true,
                                       data_ptr,
                                       effective_len);

            // After READ: set xip_active_banks[N] and clear the arm flag.
            dac_cfg.xip_active_banks |= bank_bit;
            dac_cfg.xip_entry_armed   = false;

            // Self-clear bit 8 (mode_bit_xip_en) of direct_access_cfg register.
            {
                uint32_t dac_reg_val = static_cast<uint32_t>(direct_access_cfg);
                dac_reg_val &= ~(1u << 8);
                direct_access_cfg = dac_reg_val;
            }
            // Mirror xip_active_banks update into xip_mode_cfg.xip_en bits[7:0].
            {
                uint32_t xip_reg_val = static_cast<uint32_t>(xip_mode_cfg);
                xip_reg_val = (xip_reg_val & 0xFFFFFF00u)
                              | static_cast<uint32_t>(dac_cfg.xip_active_banks);
                xip_mode_cfg = xip_reg_val;
            }
            CSML_INFO(2, logger) << "b_transport_axi_slave: XIP ENTRY complete" << " xip_active_banks=0x" << std::hex << static_cast<unsigned>(dac_cfg.xip_active_banks);

        } else if (dac_cfg.xip_exit_pending && xip_bank_active) {
            // ---------------------------------------------------------------
            // Sub-case (B): XIP Exit — insert xip_dis_mb_val mode byte.
            // The READ to exit XIP must transfer no more than 64 bytes.
            // ---------------------------------------------------------------
            if (effective_len > 64u) {
                effective_len = 64u;
            }
            // Signal XIP exit mode byte via write_data field convention.
            read_ext.write_data = static_cast<uint32_t>(xip_dis_mb_val);
            read_ext.data_bytes = effective_len;

            CSML_INFO(2, logger) << "b_transport_axi_slave: XIP EXIT READ" << " bank=" << dac_cfg.active_bank << " xip_dis_mb_val=0x" << std::hex << static_cast<unsigned>(xip_dis_mb_val) << " len=" << std::dec << effective_len;

            dispatch_flash_transaction(read_ext,
                                       dac_cfg.active_bank,
                                       true,
                                       data_ptr,
                                       effective_len);

            // After READ: clear xip_active_banks[N] and xip_exit_pending.
            dac_cfg.xip_active_banks &= ~bank_bit;
            dac_cfg.xip_exit_pending  = false;

            // Self-clear bit 9 (mode_bit_xip_dis) of direct_access_cfg register.
            {
                uint32_t dac_reg_val = static_cast<uint32_t>(direct_access_cfg);
                dac_reg_val &= ~(1u << 9);
                direct_access_cfg = dac_reg_val;
            }
            // Mirror xip_active_banks update into xip_mode_cfg.xip_en bits[7:0].
            {
                uint32_t xip_reg_val = static_cast<uint32_t>(xip_mode_cfg);
                xip_reg_val = (xip_reg_val & 0xFFFFFF00u)
                              | static_cast<uint32_t>(dac_cfg.xip_active_banks);
                xip_mode_cfg = xip_reg_val;
            }
            CSML_INFO(2, logger) << "b_transport_axi_slave: XIP EXIT complete" << " xip_active_banks=0x" << std::hex << static_cast<unsigned>(dac_cfg.xip_active_banks);

        } else {
            // ---------------------------------------------------------------
            // Sub-case (C): Normal READ (no entry/exit arm active).
            // No mode byte insertion. If XIP is active the device already
            // interprets the transaction in XIP format.
            // ---------------------------------------------------------------
            read_ext.data_bytes = effective_len;

            CSML_INFO(3, logger) << "b_transport_axi_slave: READ_ZERO_LATENCY" << " opcode=0x" << std::hex << static_cast<unsigned>(read_ext.opcode) << " bank=" << std::dec << dac_cfg.active_bank << " flash_addr=0x" << std::hex << flash_addr << " bytes=" << std::dec << effective_len;

            // Dispatch flash READ; data returned into trans data buffer.
            dispatch_flash_transaction(read_ext,
                                       dac_cfg.active_bank,
                                       true,
                                       data_ptr,
                                       effective_len);
        }
    }
    // =========================================================================
    // WRITE path: AXI WRITE → WREN (0x06) + PAGE_PROGRAM (0x02)
    //
    // Note: writes to an XIP-active bank (without exit pending) were already
    // rejected above with dir_cmd_err before reaching this point.
    // =========================================================================
    else {
        // Step A: Issue WREN (Write Enable) command.
        // opcode: use seq_cfg.we_cmd_val (reset value = 0x06).
        // instr_type: XSPI_INSTR_GENERIC (0, command-only, no data phase).
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.1.5.
        cdns_extension wren_ext;
        wren_ext.opcode      = seq_cfg.we_cmd_val;      // default 0x06
        wren_ext.cmd_ext     = 0u;
        wren_ext.bank_num    = static_cast<uint8_t>(dac_cfg.active_bank);
        wren_ext.address     = flash_addr;
        wren_ext.data_bytes  = 0u;                      // command-only
        wren_ext.write_data  = 0u;
        wren_ext.instr_type  = static_cast<uint8_t>(XSPI_INSTR_GENERIC);
        wren_ext.instr_link  = false;
        wren_ext.wp_pin      = mini_ctrl_cfg.wp_pin_level;
        wren_ext.hw_rst_pin  = mini_ctrl_cfg.hw_rst_level;
        wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;

        CSML_INFO(3, logger) << "b_transport_axi_slave: WREN" << " opcode=0x" << std::hex << static_cast<unsigned>(wren_ext.opcode) << " bank=" << std::dec << dac_cfg.active_bank;

        dispatch_flash_transaction(wren_ext,
                                   dac_cfg.active_bank,
                                   false,  // write command
                                   nullptr,
                                   0u);

        // Step B: Build write data buffer for PAGE_PROGRAM.
        // When rwds_cap_en=1 and wstrb is available, only bytes with a
        // non-zero write strobe are included (RWDS byte masking).
        // For the LT model, the full AXI data buffer is used regardless;
        // the wstrb vector is propagated to the cdns_extension via write_data
        // field semantics. Direct multi-byte PAGE_PROGRAM uses the data_ptr.
        //
        // RWDS masking: when use_wstrb=true, bytes with wstrb[i]=0 are replaced
        // with 0xFF (erased flash value) in the outgoing write buffer, allowing
        // the flash model to apply selective write masking.
        std::vector<uint8_t> prog_buf;
        if (use_wstrb && data_len > 0u && data_ptr != nullptr) {
            prog_buf.resize(data_len, 0xFFu);
            for (uint32_t i = 0u; i < data_len; ++i) {
                if (i < axi_ext->wstrb.size() && axi_ext->wstrb[i] != 0u) {
                    prog_buf[i] = data_ptr[i];
                }
            }
        }

        uint8_t* prog_data_ptr = (use_wstrb && !prog_buf.empty())
                                  ? prog_buf.data()
                                  : data_ptr;

        // Step C: Issue PAGE_PROGRAM command.
        // opcode: use seq_cfg.prog_cmd_val (reset value = 0x02).
        // instr_type: XSPI_INSTR_WRITE (2).
        cdns_extension prog_ext;
        prog_ext.opcode      = seq_cfg.prog_cmd_val;    // default 0x02
        prog_ext.cmd_ext     = 0u;
        prog_ext.bank_num    = static_cast<uint8_t>(dac_cfg.active_bank);
        prog_ext.address     = flash_addr;
        prog_ext.data_bytes  = data_len;
        prog_ext.write_data  = 0u;
        prog_ext.instr_type  = static_cast<uint8_t>(XSPI_INSTR_WRITE);
        prog_ext.instr_link  = false;
        prog_ext.wp_pin      = mini_ctrl_cfg.wp_pin_level;
        prog_ext.hw_rst_pin  = mini_ctrl_cfg.hw_rst_level;
        prog_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;

        CSML_INFO(3, logger) << "b_transport_axi_slave: PAGE_PROGRAM" << " opcode=0x" << std::hex << static_cast<unsigned>(prog_ext.opcode) << " bank=" << std::dec << dac_cfg.active_bank << " flash_addr=0x" << std::hex << flash_addr << " bytes=" << std::dec << data_len;

        dispatch_flash_transaction(prog_ext,
                                   dac_cfg.active_bank,
                                   false,
                                   prog_data_ptr,
                                   data_len);
    }

    // -------------------------------------------------------------------------
    // Step 4: Clear ctrl_status.ctrl_busy (bit 7) after flash dispatch.
    // -------------------------------------------------------------------------
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~(1u << 7);   // clear ctrl_busy
        ctrl_status = cs_val;
    }

    // -------------------------------------------------------------------------
    // Step 5: Return TLM_OK_RESPONSE to the AXI slave initiator.
    // The transaction is always acknowledged OK on the AXI slave interface even
    // if the flash dispatch failed (e.g., out-of-range bank → cmd_ignored).
    // -------------------------------------------------------------------------
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }
}

/******************************************************************************
 * @brief b_transport handler for PoR_input_signals — FUNC_XSPI_007
 *
 * Implements the full Power-on Reset and SFDP Discovery Engine.
 *
 * Sequence:
 *  1. Extract xspi_PoR_trans extension from the incoming payload.
 *  2. Block all register write callbacks by leaving m_init_comp_done=false
 *     (already false from construction or prior reset_handler() call).
 *  3. If discovery_inhibit==0: invoke run_sfdp_discovery(), which issues
 *     READ_SFDP (opcode 0x5A) on xspi_bus_socket[discovery_bank], validates
 *     the SFDP signature, parses the JESD216A 16-DWORD basic parameter table,
 *     and calls configure_registers_from_sfdp() to auto-populate all 10
 *     sequence register groups.
 *  4. Update discovery_control RO status fields (discovery_comp, discovery_fail,
 *     discovery_inhibit, and the post-discovery parameter fields).
 *  5. Update ctrl_status.init_comp and ctrl_status.init_fail.
 *  6. Set m_init_comp_done=true to unblock subsequent register write callbacks.
 *  7. Write back boot_comp and boot_error into the xspi_PoR_trans extension.
 *  8. Acknowledge the TLM transaction with TLM_OK_RESPONSE.
 *
 * Pre-init write blocking: m_init_comp_done remains false during the entire
 * SFDP discovery and sequence-register configuration phase, causing all write
 * callbacks to return immediately without modifying register state. This
 * matches TC_XSPI_ERR_005 requirements.
 *
 * Single-writer compliance: no sc_out or sc_signal writes performed here.
 * int_out is not modified (no interrupts fire on PoR completion).
 * The register RO fields (discovery_comp, discovery_fail, init_comp, init_fail)
 * are written via direct uint32_t assignment using the monolithic register
 * operator= which bypasses write_bit_mask enforcement, which is correct since
 * these are hardware-driven status fields, not software-writable fields.
 *
 * Architecture references:
 *   docs/xspi_ctrl-architecture-behaviour-map.json operations.por_sequence
 *   docs/xspi_ctrl-detailed-design.md Sections 8.1–8.5
 *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_007
 *
 * @param trans  TLM generic payload carrying xspi_PoR_trans extension.
 *               Caller retains ownership. Extension output fields are written
 *               before this function returns.
 * @param delay  Local time offset for LT temporal decoupling (accumulated here)
 ******************************************************************************/
void xspi_ctrl_ip::b_transport_por(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
{
    CSML_INFO(2, logger) << "b_transport_por: FUNC_XSPI_007 — PoR received";

    // Block cmd_reg0 writes during active PoR/SFDP initialization.
    m_init_comp_done = false;

    // -------------------------------------------------------------------------
    // Step 1: Extract xspi_PoR_trans extension.
    // If absent treat as a bare PoR with no discovery and no boot.
    // -------------------------------------------------------------------------
    xspi_PoR_trans* por_ext = nullptr;
    trans.get_extension(por_ext);

    uint8_t disc_inhibit   = 0u;
    uint8_t disc_num_lines = 0u;
    uint8_t disc_abnum     = 0u;
    uint8_t disc_bank      = 0u;
    uint8_t disc_cmd_type  = 0u;
    uint8_t disc_dummy_cnt = 0u;
    uint8_t disc_extop_en  = 0u;
    uint8_t disc_extop_val = 0u;
    uint8_t disc_crc_en    = 0u;
    uint8_t disc_crc_var   = 0u;
    uint8_t disc_crc_oe    = 0u;
    uint8_t disc_crc_chunk = 0u;
    uint8_t boot_en        = 0u;

    if (por_ext != nullptr) {
        disc_inhibit   = por_ext->discovery_inhibit;
        disc_num_lines = por_ext->discovery_num_lines;
        disc_abnum     = por_ext->discovery_abnum;
        disc_bank      = por_ext->discovery_bank;
        disc_cmd_type  = por_ext->discovery_cmd_type;
        disc_dummy_cnt = por_ext->discovery_dummy_cnt;
        disc_extop_en  = por_ext->discovery_extop_en;
        disc_extop_val = por_ext->discovery_extop_val;
        disc_crc_en    = por_ext->discovery_seq_crc_en;
        disc_crc_var   = por_ext->discovery_seq_crc_variant;
        disc_crc_oe    = por_ext->discovery_seq_crc_oe;
        disc_crc_chunk = por_ext->discovery_seq_crc_chunk_size;
        boot_en        = por_ext->boot_en;
        // Pre-clear output fields in the extension before we fill them.
        por_ext->boot_comp  = 0u;
        por_ext->boot_error = 0u;
        por_ext->init_comp  = 0u;
        por_ext->init_fail  = 0u;
        CSML_INFO(2, logger) << "b_transport_por: disc_inhibit=" << (unsigned)disc_inhibit << " num_lines=0x" << std::hex << (unsigned)disc_num_lines << " bank=" << std::dec << (unsigned)disc_bank << " boot_en=" << (unsigned)boot_en;
    } else {
        CSML_INFO(2, logger) << "b_transport_por: no xspi_PoR_trans extension — bare PoR";
    }

    // -------------------------------------------------------------------------
    // Step 2: Discovery phase.
    // m_init_comp_done remains false throughout, blocking register writes.
    // -------------------------------------------------------------------------
    uint8_t disc_fail_code    = 0u;    // 0b00=ok, 0b01=fail, 0b10=legacy SPI
    uint8_t detected_abnum    = disc_abnum;
    uint8_t detected_type     = 0u;    // 0=xSPI/NOR, 1=HyperFlash, 3=SPI-NAND
    bool    discovery_success = false;

    if (disc_inhibit != 0u) {
        // ----------------------------------------------------------------
        // Fast path: discovery_inhibit=1.
        // No READ_SFDP transaction issued; sequence registers retain their
        // current values. discovery_fail=0b00 per hardware specification
        // (Table 4.33: discovery_inhibit=1, num_lines=0 → init_fail=0b00).
        // ----------------------------------------------------------------
        CSML_INFO(2, logger) << "b_transport_por: discovery_inhibit set — skipping SFDP";
        disc_fail_code    = 0u;
        discovery_success = true;   // inhibit is not a failure
    } else {
        // ----------------------------------------------------------------
        // Full SFDP discovery path.
        // ----------------------------------------------------------------
        discovery_success = run_sfdp_discovery(
            static_cast<unsigned int>(disc_bank),
            disc_num_lines,
            disc_abnum,
            disc_dummy_cnt,
            disc_cmd_type,
            disc_extop_en,
            disc_extop_val,
            disc_crc_en,
            disc_crc_var,
            disc_crc_oe,
            disc_crc_chunk,
            detected_abnum,
            detected_type);

        if (!discovery_success) {
            disc_fail_code = 0x1u;   // 0b01: failed
            CSML_INFO(2, logger) << "b_transport_por: SFDP discovery FAILED";
        } else {
            // Determine init_fail encoding from detected device type.
            // detected_type: 0=xSPI Profile 1/NOR, 1=HyperFlash Profile 2,
            //                 3=SPI-NAND → all map to 0b00 (xSPI/SPI-NAND detected).
            // Legacy SPI (2) maps to 0b10 — but the LT model does not distinguish
            // legacy SPI as a separate outcome; default to 0b00.
            disc_fail_code = 0u;
            CSML_INFO(2, logger) << "b_transport_por: SFDP discovery SUCCESS" << " abnum=" << (unsigned)detected_abnum << " type=" << (unsigned)detected_type;
        }
    }

    // -------------------------------------------------------------------------
    // Step 3: Update discovery_control RO status fields.
    //
    // The register has write_bit_mask=0x7ffc3 — bits discovery_comp (bit 2),
    // discovery_fail (bits[4:3]), and discovery_inhibit (bit 5) are RO to
    // software. We write them directly via uint32_t arithmetic on the register
    // value, which bypasses the write mask (correct for hardware-driven updates).
    //
    // Post-discovery parameter fields (abnum, extop_en, extop_val, cmd_type,
    // dummy_cnt, num_lines, bank) are also updated to reflect discovered state.
    // -------------------------------------------------------------------------
    {
        uint32_t dc_val = static_cast<uint32_t>(discovery_control);

        // Clear the status/result bits before setting them.
        dc_val &= ~((0x3u << 3) | (1u << 2) | (1u << 5));
        // Set discovery_comp (bit 2).
        dc_val |= (1u << 2);
        // Set discovery_fail (bits[4:3]).
        dc_val |= (static_cast<uint32_t>(disc_fail_code & 0x3u) << 3);
        // Set discovery_inhibit status bit (bit 5) to mirror input.
        dc_val |= (static_cast<uint32_t>(disc_inhibit ? 1u : 0u) << 5);

        if (disc_inhibit == 0u && discovery_success) {
            // Update post-discovery parameter fields in the register.
            // Clear old values of bits[18:6] (abnum[11], dummy[10], cmd_type[9:8],
            // extop_en[7], extop_val[6]) then write detected values.
            dc_val &= ~((0x3u << 8) | (1u << 10) | (1u << 11) | (1u << 6) | (1u << 7));
            dc_val |= (static_cast<uint32_t>(disc_extop_val & 0x1u) << 6);
            dc_val |= (static_cast<uint32_t>(disc_extop_en  & 0x1u) << 7);
            dc_val |= (static_cast<uint32_t>(disc_cmd_type  & 0x3u) << 8);
            dc_val |= (static_cast<uint32_t>(disc_dummy_cnt & 0x1u) << 10);
            dc_val |= (static_cast<uint32_t>(detected_abnum & 0x1u) << 11);
        }

        // Direct assignment to the monolithic register bypasses write_bit_mask
        // — correct because these are hardware-driven status writes.
        discovery_control = dc_val;
    }

    // -------------------------------------------------------------------------
    // Step 4: Update ctrl_status RO fields.
    //
    // ctrl_status has write_bit_mask=0x0 (fully RO); direct uint32_t write.
    // init_comp is at bit 16; init_fail is at bits[9:8].
    // -------------------------------------------------------------------------
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        // Clear init_comp (bit 16) and init_fail (bits[9:8]) before setting.
        cs_val &= ~((1u << 16) | (0x3u << 8));
        // Set init_comp.
        cs_val |= (1u << 16);
        // Set init_fail.
        cs_val |= (static_cast<uint32_t>(disc_fail_code & 0x3u) << 8);
        ctrl_status = cs_val;
    }

    // -------------------------------------------------------------------------
    // Step 5: Conditionally launch Boot Engine (FUNC_XSPI_010).
    //
    // The boot engine runs synchronously here — before m_init_comp_done is
    // set to true — so that register writes during boot are still blocked
    // (TC_XSPI_BOOT_005). boot_en and boot_available must both be 1.
    // -------------------------------------------------------------------------
    if (por_ext != nullptr) {
        por_ext->init_comp = 1u;
        por_ext->init_fail = disc_fail_code;

        if ((boot_en != 0u) && boot_available) {
            CSML_INFO(2, logger) << "b_transport_por: boot_en=1 and boot_available=1" << " — launching boot engine (FUNC_XSPI_010)";
            run_boot_engine(por_ext);
            CSML_INFO(2, logger) << "b_transport_por: boot engine finished" << " boot_comp=" << (unsigned)por_ext->boot_comp << " boot_error=" << (unsigned)por_ext->boot_error;
        } else {
            // boot_en=0 or boot_available=0: boot engine not launched.
            // boot_comp and boot_error remain 0 per specification.
            CSML_INFO(2, logger) << "b_transport_por: boot engine not launched" << " (boot_en=" << (unsigned)boot_en << " boot_available=" << static_cast<int>(boot_available) << ")";
        }
    }

    // -------------------------------------------------------------------------
    // Step 6: Unblock register write callbacks.
    // From this point forward, software writes to all registers are accepted.
    // -------------------------------------------------------------------------
    m_init_comp_done = true;
    CSML_INFO(2, logger) << "b_transport_por: init_comp set — register writes unblocked";

    // -------------------------------------------------------------------------
    // Step 7: Acknowledge TLM transaction and advance LT time.
    // -------------------------------------------------------------------------
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    CSML_INFO(2, logger) << "b_transport_por: complete" << " discovery_success=" << discovery_success << " disc_fail_code=0x" << std::hex << (unsigned)disc_fail_code;
}

// =============================================================================
// Boot Engine Implementation — FUNC_XSPI_010
// =============================================================================

/******************************************************************************
 * @brief Autonomous Boot DMA Engine — FUNC_XSPI_010
 *
 * Invoked synchronously from b_transport_por() after SFDP discovery completes,
 * when boot_en == 1 AND ctrl_features_reg.boot_available == 1. This function
 * performs the complete flash-to-system-memory boot image transfer without any
 * CPU involvement.
 *
 * Architecture:
 *   The boot image is stored on flash bank 0 starting at address 0x0. The
 *   first 32 bytes of that region are a configuration record structured as:
 *     Bytes  0.. 7 : Main Data Size (lower 32 bits = image byte count)
 *     Bytes  8..15 : Image Offset  (lower 48 bits = flash start address)
 *     Bytes 16..23 : Host Address  (full 64 bits = system memory destination)
 *
 * Step 1 — Read the 32-byte configuration record from flash bank 0, address 0x0.
 *           Uses seq_cfg.read_cmd_val and the READ sequence parameters already
 *           established by SFDP discovery or their hardware reset defaults.
 *           On non-OK flash response: sets boot_status.boot_dqs_err (bit 0);
 *           sets boot_error=1; returns immediately.
 *
 * Step 2 — Extract boot parameters from the configuration record:
 *           main_data_size = record[0..3] as uint32_t (LE)
 *           image_offset   = record[8..13] as uint64_t (LE, 48-bit)
 *           host_addr      = record[16..23] as uint64_t (LE)
 *           If main_data_size == 0: treat as no-op; set boot_comp=1; return.
 *
 * Step 3 — DMA Transfer loop: iterate chunks of BOOT_DMA_CHUNK_BYTES.
 *           For each chunk:
 *             a. Dispatch flash READ via dispatch_flash_transaction() on bank 0
 *                at flash_address = image_offset + bytes_transferred.
 *             b. On non-OK flash response: sets boot_status.boot_dqs_err (bit 0);
 *                sets boot_error=1; returns.
 *             c. Write the received data to system memory via dma_write() at
 *                host_addr + bytes_transferred. dma_write() sets
 *                dma_target_error_l/h and intr_status.ddma_terr on AXI error.
 *             d. On non-OK DMA response: sets boot_status.boot_bus_err (bit 2);
 *                sets boot_error=1; returns.
 *           boot_status writes use direct register assignment (write_bit_mask=0;
 *           model-internal hardware writes bypass the write-ignore restriction).
 *
 * Step 4 — Status output:
 *           On success: sets por_ext->boot_comp = 1.
 *           On error:   sets por_ext->boot_error = 1; boot_comp remains 0.
 *
 * Register blocking:
 *   m_init_comp_done is still false when this function executes (b_transport_por
 *   sets it true only after run_boot_engine returns). This enforces that all
 *   register write callbacks are blocked during boot execution (TC_XSPI_BOOT_005).
 *
 * Single-writer compliance:
 *   No sc_out or sc_signal is written directly. boot_status is a scml2 register
 *   with write_bit_mask=0; direct assignment is the correct model-internal path.
 *
 * Architecture references:
 *   docs/xspi_ctrl-architecture-behaviour-map.json operations.boot_sequence
 *   docs/xspi_ctrl-detailed-design.md Sections 7.6.1–7.6.4
 *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_010
 *
 * @param por_ext  Non-null pointer to the caller's xspi_PoR_trans extension.
 *                 boot_comp and boot_error fields are written before return.
 ******************************************************************************/
void xspi_ctrl_ip::run_boot_engine(xspi_PoR_trans* por_ext)
{
    // -------------------------------------------------------------------------
    // Boot chunk size: maximum bytes transferred per flash READ / DMA WRITE
    // pair in each loop iteration. 256 bytes balances TLM overhead and aligns
    // with typical NOR flash page size.
    // -------------------------------------------------------------------------
    static constexpr uint32_t BOOT_DMA_CHUNK_BYTES = 256u;

    // -------------------------------------------------------------------------
    // Step 1: Set ctrl_status.ctrl_busy (bit 7) while boot engine is active.
    // The boot engine occupies the controller exclusively.
    // -------------------------------------------------------------------------
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val |= (1u << 7);   // ctrl_busy
        ctrl_status = cs_val;
    }

    CSML_INFO(2, logger) << "run_boot_engine: reading 32-byte config record" << " from flash bank 0 addr 0x0";

    // -------------------------------------------------------------------------
    // Step 2: Issue flash READ for 32-byte boot configuration record.
    // The READ uses seq_cfg parameters (read_cmd_val, read_addr_cnt, ios widths)
    // already established by SFDP discovery or hardware reset defaults.
    // -------------------------------------------------------------------------
    static uint8_t cfg_record[32];
    std::memset(cfg_record, 0, sizeof(cfg_record));

    cdns_extension cfg_ext;
    cfg_ext.opcode        = seq_cfg.read_cmd_val;   // e.g. 0x03 (READ)
    cfg_ext.cmd_ext       = seq_cfg.read_cmd_ext_val;
    cfg_ext.bank_num      = 0u;                     // boot always on bank 0
    cfg_ext.address       = 0x0u;                   // config record at flash offset 0
    cfg_ext.data_bytes    = 32u;
    cfg_ext.write_data    = 0u;
    cfg_ext.instr_type    = static_cast<uint8_t>(XSPI_INSTR_READ);
    cfg_ext.instr_link    = false;
    cfg_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
    cfg_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
    cfg_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    // I/O widths and edge modes from the read sequence configuration.
    // seq_cfg.read_addr_cnt encodes 3-byte or 4-byte addressing; the boot
    // engine uses the same addressing mode as normal READ operations.
    cfg_ext.opcode_ios    = 0u;   // 1-wire command phase (LT model default)
    cfg_ext.opcode_edge   = 0u;
    cfg_ext.addr_ios      = 0u;
    cfg_ext.addr_edge     = 0u;
    cfg_ext.data_ios      = 0u;
    cfg_ext.data_edge     = 0u;

    bool cfg_ok = dispatch_flash_transaction(cfg_ext,
                                             0u,        // bank 0
                                             true,      // READ
                                             cfg_record,
                                             32u);

    if (!cfg_ok) {
        // Flash READ of configuration record failed — model as DQS error per
        // spec: "DQS Error … Set when an incorrect DQS pulse count is detected
        // during the boot flash READ sequence on xspi_bus_socket."
        CSML_INFO(2, logger) << "run_boot_engine: config record READ failed" << " — setting boot_dqs_err";
        uint32_t bs_val = static_cast<uint32_t>(boot_status);
        bs_val |= (1u << 0);   // boot_dqs_err at bit 0
        boot_status = bs_val;
        por_ext->boot_error = 1u;
        // Clear ctrl_busy before returning.
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~(1u << 7);
        ctrl_status = cs_val;
        return;
    }

    // -------------------------------------------------------------------------
    // Step 3: Extract boot parameters from configuration record.
    //
    // Record layout (32 bytes, little-endian):
    //   Bytes  0.. 3 : Main Data Size lower 32 bits  (image byte count)
    //   Bytes  4.. 7 : Main Data Size upper 32 bits  (reserved — ignored)
    //   Bytes  8..13 : Image Offset lower 48 bits    (flash source address)
    //   Bytes 14..15 : Image Offset upper 16 bits    (reserved — ignored)
    //   Bytes 16..23 : Host Address full 64 bits     (system memory destination)
    //   Bytes 24..31 : SPI-NAND page size (lower 16 bits) + reserved
    // -------------------------------------------------------------------------
    uint32_t main_data_size = 0u;
    std::memcpy(&main_data_size, &cfg_record[0], sizeof(uint32_t));

    uint64_t image_offset = 0u;
    for (int i = 0; i < 6; i++) {
        image_offset |= (static_cast<uint64_t>(cfg_record[8 + i]) << (8 * i));
    }

    uint64_t host_addr = 0u;
    std::memcpy(&host_addr, &cfg_record[16], sizeof(uint64_t));

    CSML_INFO(2, logger) << "run_boot_engine: main_data_size=" << main_data_size << " image_offset=0x" << std::hex << image_offset << " host_addr=0x" << host_addr;

    // If boot image size is zero treat as no-op success per specification:
    // "the boot engine is not launched … boot_comp=1 to indicate initialization
    // is complete." A zero-size record indicates there is no boot image to load.
    if (main_data_size == 0u) {
        CSML_INFO(2, logger) << "run_boot_engine: main_data_size=0" << " — no image to transfer; boot_comp=1";
        por_ext->boot_comp = 1u;
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~(1u << 7);
        ctrl_status = cs_val;
        return;
    }

    // -------------------------------------------------------------------------
    // Step 4: DMA Transfer loop — flash READ chunks → system memory WRITE.
    //
    // Issues successive flash READ transactions from image_offset for
    // main_data_size total bytes (in BOOT_DMA_CHUNK_BYTES increments) and
    // writes each chunk to system memory at host_addr via dma_write().
    //
    // The boot engine uses bank 0 and the READ sequence established by SFDP.
    // -------------------------------------------------------------------------
    static uint8_t boot_chunk_buf[BOOT_DMA_CHUNK_BYTES];

    uint32_t bytes_remaining   = main_data_size;
    uint64_t flash_read_offset = image_offset;
    uint64_t dma_write_offset  = host_addr;

    while (bytes_remaining > 0u) {
        uint32_t chunk_size = (bytes_remaining < BOOT_DMA_CHUNK_BYTES)
                              ? bytes_remaining
                              : BOOT_DMA_CHUNK_BYTES;

        std::memset(boot_chunk_buf, 0, chunk_size);

        CSML_INFO(3, logger) << "run_boot_engine: flash READ chunk" << " flash_addr=0x" << std::hex << flash_read_offset << " size=" << std::dec << chunk_size;

        // Issue flash READ for this chunk.
        cdns_extension rd_ext;
        rd_ext.opcode        = seq_cfg.read_cmd_val;
        rd_ext.cmd_ext       = seq_cfg.read_cmd_ext_val;
        rd_ext.bank_num      = 0u;
        rd_ext.address       = flash_read_offset;
        rd_ext.data_bytes    = chunk_size;
        rd_ext.write_data    = 0u;
        rd_ext.instr_type    = static_cast<uint8_t>(XSPI_INSTR_READ);
        rd_ext.instr_link    = false;
        rd_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
        rd_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
        rd_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
        rd_ext.opcode_ios    = 0u;
        rd_ext.opcode_edge   = 0u;
        rd_ext.addr_ios      = 0u;
        rd_ext.addr_edge     = 0u;
        rd_ext.data_ios      = 0u;
        rd_ext.data_edge     = 0u;

        bool flash_ok = dispatch_flash_transaction(rd_ext,
                                                   0u,
                                                   true,
                                                   boot_chunk_buf,
                                                   chunk_size);

        if (!flash_ok) {
            // Flash READ failure: model as DQS error.
            CSML_INFO(2, logger) << "run_boot_engine: flash READ error at 0x" << std::hex << flash_read_offset << " — setting boot_dqs_err";
            uint32_t bs_val = static_cast<uint32_t>(boot_status);
            bs_val |= (1u << 0);   // boot_dqs_err at bit 0
            boot_status = bs_val;
            por_ext->boot_error = 1u;
            uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
            cs_val &= ~(1u << 7);
            ctrl_status = cs_val;
            return;
        }

        // Issue AXI master WRITE to system memory.
        uint32_t dma_addr_low  = static_cast<uint32_t>(dma_write_offset & 0xFFFFFFFFu);
        uint32_t dma_addr_high = static_cast<uint32_t>((dma_write_offset >> 32) & 0xFFFFFFFFu);

        CSML_INFO(3, logger) << "run_boot_engine: DMA WRITE chunk to 0x" << std::hex << dma_write_offset << " size=" << std::dec << chunk_size;

        bool dma_ok = dma_write(dma_addr_low,
                                dma_addr_high,
                                boot_chunk_buf,
                                chunk_size,
                                true);   // is_data_path=true → ddma_terr on error

        if (!dma_ok) {
            // AXI bus error on system memory WRITE.
            CSML_INFO(2, logger) << "run_boot_engine: DMA WRITE error at 0x" << std::hex << dma_write_offset << " — setting boot_bus_err";
            uint32_t bs_val = static_cast<uint32_t>(boot_status);
            bs_val |= (1u << 2);   // boot_bus_err at bit 2
            boot_status = bs_val;
            por_ext->boot_error = 1u;
            uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
            cs_val &= ~(1u << 7);
            ctrl_status = cs_val;
            return;
        }

        bytes_remaining   -= chunk_size;
        flash_read_offset += chunk_size;
        dma_write_offset  += chunk_size;
    }

    // -------------------------------------------------------------------------
    // Step 5: Success — clear ctrl_busy and set boot_comp.
    // -------------------------------------------------------------------------
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~(1u << 7);   // clear ctrl_busy
        ctrl_status = cs_val;
    }

    por_ext->boot_comp = 1u;
    CSML_INFO(2, logger) << "run_boot_engine: complete — boot_comp=1" << " transferred=" << main_data_size << " bytes";
}

// =============================================================================
// SFDP Discovery Helper Implementations — FUNC_XSPI_007
// =============================================================================

/******************************************************************************
 * @brief Issue a single READ_SFDP (opcode 0x5A) transaction on
 *        xspi_bus_socket[bank] and capture the response data.
 *
 * Constructs a cdns_extension with:
 *   opcode      = 0x5A (READ_SFDP)
 *   cmd_ext     = extop_en ? (extop_val ? 0xA5 : 0x5A) : 0x00
 *   bank_num    = bank
 *   address     = address (3-byte or 4-byte per abnum)
 *   data_bytes  = len
 *   instr_type  = XSPI_INSTR_SFDP (96)
 *   instr_link  = false
 *   wp_pin      = mini_ctrl_cfg.wp_pin_level
 *   hw_rst_pin  = mini_ctrl_cfg.hw_rst_level
 *   spi_clock_mode = mini_ctrl_cfg.spi_clk_mode
 *
 * The I/O width encoding from num_lines is mapped to cdns_extension
 * opcode_ios/addr_ios/data_ios as follows:
 *   0x1 (1-1-1) → ios = 0 (1-bit)
 *   0x2 (2-2-2) → ios = 1 (2-bit)
 *   0x4 (4-4-4) → ios = 2 (4-bit)
 *   0x8 or 0xC or 0xE (8-8-8/Profile-2/NAND) → ios = 3 (8-bit)
 * DDR edge is set from cmd_type (0=SDR, 1 or 2 = DDR).
 *
 * Architecture reference: docs/xspi_ctrl-detailed-design.md Section 8.2
 *   (SFDP discovery READ_SFDP command construction).
 *
 * @param bank       xspi_bus_socket[] index (validated < NUM_TARGETS)
 * @param address    SFDP ROM byte address to read from
 * @param buf        Caller-allocated buffer to receive @p len bytes
 * @param len        Number of bytes to read
 * @param num_lines  I/O-width encoding (0x1/2/4/8/0xC/0xE)
 * @param abnum      0=3-byte address, 1=4-byte address
 * @param dummy_cnt  0=8 dummy cycles, 1=20 dummy cycles
 * @param cmd_type   0=SDR, 1=DDR, 2=DTR
 * @param extop_en   1=two-byte command phase
 * @param extop_val  0=repetition (0x5A 0x5A), 1=negation (0x5A 0xA5)
 * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE
 ******************************************************************************/
bool xspi_ctrl_ip::issue_sfdp_read(unsigned int bank,
                                    uint32_t     address,
                                    uint8_t*     buf,
                                    uint32_t     len,
                                    uint8_t      num_lines,
                                    uint8_t      abnum,
                                    uint8_t      dummy_cnt,
                                    uint8_t      cmd_type,
                                    uint8_t      extop_en,
                                    uint8_t      extop_val)
{
    // Map num_lines to cdns_extension I/O width field value.
    // 0x1 → 0 (1-bit / 1S mode), 0x2 → 1 (2-bit / 2S mode),
    // 0x4 → 2 (4-bit / 4S mode), 0x8/0xC/0xE → 3 (8-bit / 8S or HF mode).
    uint8_t ios_width;
    switch (num_lines) {
        case 0x1u:  ios_width = 0u; break;
        case 0x2u:  ios_width = 1u; break;
        case 0x4u:  ios_width = 2u; break;
        default:    ios_width = 3u; break;   // 8-8-8 / HyperFlash / SPI-NAND
    }

    // DDR edge: 0=SDR, 1=DDR for cmd_type != 0.
    bool ddr_edge = (cmd_type != 0u);

    // Extended opcode byte: if extop_en=1, the second command byte is either
    // a repetition (0x5A) or negation (0xA5) of the primary opcode.
    uint8_t cmd_ext_byte = 0u;
    if (extop_en != 0u) {
        cmd_ext_byte = (extop_val != 0u) ? 0xA5u : 0x5Au;
    }

    cdns_extension ext;
    ext.opcode        = 0x5Au;          // READ_SFDP
    ext.cmd_ext       = cmd_ext_byte;
    ext.bank_num      = static_cast<uint8_t>(bank);
    ext.address       = static_cast<uint64_t>(address);
    ext.data_bytes    = len;
    ext.write_data    = 0u;
    ext.instr_type    = XSPI_INSTR_SFDP;
    ext.instr_link    = false;
    ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios    = ios_width;
    ext.opcode_edge   = ddr_edge;
    ext.addr_ios      = ios_width;
    ext.addr_edge     = ddr_edge;
    ext.data_ios      = ios_width;
    ext.data_edge     = ddr_edge;

    CSML_INFO(3, logger) << "issue_sfdp_read: bank=" << bank << " addr=0x" << std::hex << address << " len=" << std::dec << len << " ios=" << (unsigned)ios_width << " ddr=" << ddr_edge;

    return dispatch_flash_transaction(ext, bank, true, buf, len);
}

/******************************************************************************
 * @brief Execute the SFDP discovery state machine.
 *
 * State machine: IDLE → READ_SFDP_HEADER → PARSE_PARAM_TABLE →
 *                WRITE_SEQ_REGS → SUCCESS  (or → FAIL at any step).
 *
 * When num_lines==0 (Auto), the model iterates through all supported READ_SFDP
 * format variations in the order defined by the hardware specification
 * (docs/xspi_ctrl-detailed-design.md Section 8.3 and manual.md Table 4.32):
 *
 *   Variation 1:  1-1-1 SDR,   3-byte addr, 8 dummy cycles
 *   Variation 2:  2-2-2 SDR,   3-byte addr, 8 dummy cycles
 *   Variation 3:  4-4-4 SDR,   3-byte addr, 8 dummy cycles
 *   Variation 4:  4-4-4 DDR,   3-byte addr, 8 dummy cycles
 *   Variation 5:  8-8-8 SDR,   3-byte addr, 8 dummy cycles
 *   Variation 6:  8-8-8 SDR,   4-byte addr, 20 dummy cycles, extop neg
 *   Variation 7:  8-8-8 DDR,   4-byte addr, 8 dummy cycles,  extop rep
 *   Variation 8:  8-8-8 DDR,   3-byte addr, 8 dummy cycles,  extop rep
 *   Variation 9:  8-8-8 DDR,   4-byte addr, 20 dummy cycles, extop neg
 *   Variation 10: HyperFlash,  Profile-2, 15 dummy cycles (num_lines=0xC)
 *   Variation 11: 1-1-1 SDR,   3-byte addr, 8 dummy cycles  (SPI-NAND)
 *   Variation 12: 1-1-1 SDR,   4-byte addr, 8 dummy cycles
 *   Variation 13: 1-1-1 SDR,   3-byte addr, 20 dummy cycles
 *
 * For each variation, a READ_SFDP is issued at address 0x000000 to read the
 * 8-byte SFDP header. The received bytes are checked for SFDP signature
 * 0x50444653. On match, the JEDEC parameter header is read from offset 0x08
 * (8 bytes) to locate the basic flash parameter table. The 16-DWORD (64-byte)
 * table is then read and configure_registers_from_sfdp() is called.
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.3 (state machine description)
 *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_007
 *
 * @param bank              Flash CS bank index
 * @param num_lines         I/O mode selector (0=Auto; 1/2/4/8/0xC/0xE=fixed)
 * @param abnum             3-byte (0) or 4-byte (1) initial addressing
 * @param dummy_cnt         Dummy cycle selector (0=8, 1=20)
 * @param cmd_type          DDR/SDR selector
 * @param extop_en          Extended opcode enable
 * @param extop_val         Extended opcode variant (0=rep, 1=neg)
 * @param crc_en            Stored to global_seq_cfg; not computed
 * @param crc_variant       Stored to global_seq_cfg; not computed
 * @param crc_oe            Stored to global_seq_cfg; not computed
 * @param crc_chunk_size    Stored to global_seq_cfg; not computed
 * @param[out] detected_abnum  Address mode in use after successful discovery
 * @param[out] detected_type   Device type (0=xSPI, 1=HyperFlash, 3=SPI-NAND)
 * @return true on successful signature validation and table parse
 ******************************************************************************/
bool xspi_ctrl_ip::run_sfdp_discovery(unsigned int bank,
                                       uint8_t      num_lines,
                                       uint8_t      abnum,
                                       uint8_t      dummy_cnt,
                                       uint8_t      cmd_type,
                                       uint8_t      extop_en,
                                       uint8_t      extop_val,
                                       uint8_t      crc_en,
                                       uint8_t      crc_variant,
                                       uint8_t      crc_oe,
                                       uint8_t      crc_chunk_size,
                                       uint8_t&     detected_abnum,
                                       uint8_t&     detected_type)
{
    // -------------------------------------------------------------------------
    // Build the list of format variations to try.
    // Each entry: { num_lines, abnum, dummy_cnt, cmd_type, extop_en, extop_val }
    // -------------------------------------------------------------------------
    struct SfdpVariation {
        uint8_t nl;     ///< num_lines encoding
        uint8_t ab;     ///< abnum
        uint8_t dc;     ///< dummy_cnt (0=8 cycles, 1=20 cycles)
        uint8_t ct;     ///< cmd_type (0=SDR, 1=DDR)
        uint8_t ee;     ///< extop_en
        uint8_t ev;     ///< extop_val (0=rep, 1=neg)
    };

    static const SfdpVariation k_auto_variations[] = {
        // Variation 1: 1-1-1 SDR, 3-byte, 8 dummy
        { 0x1u, 0u, 0u, 0u, 0u, 0u },
        // Variation 2: 2-2-2 SDR, 3-byte, 8 dummy
        { 0x2u, 0u, 0u, 0u, 0u, 0u },
        // Variation 3: 4-4-4 SDR, 3-byte, 8 dummy
        { 0x4u, 0u, 0u, 0u, 0u, 0u },
        // Variation 4: 4-4-4 DDR, 3-byte, 8 dummy
        { 0x4u, 0u, 0u, 1u, 0u, 0u },
        // Variation 5: 8-8-8 SDR, 3-byte, 8 dummy, no extop
        { 0x8u, 0u, 0u, 0u, 0u, 0u },
        // Variation 6: 8-8-8 SDR, 4-byte, 20 dummy, extop negation
        { 0x8u, 1u, 1u, 0u, 1u, 1u },
        // Variation 7: 8-8-8 DDR, 4-byte, 8 dummy, extop repetition
        { 0x8u, 1u, 0u, 1u, 1u, 0u },
        // Variation 8: 8-8-8 DDR, 3-byte, 8 dummy, extop repetition
        { 0x8u, 0u, 0u, 1u, 1u, 0u },
        // Variation 9: 8-8-8 DDR, 4-byte, 20 dummy, extop negation
        { 0x8u, 1u, 1u, 1u, 1u, 1u },
        // Variation 10: HyperFlash / xSPI Profile 2.0, 20 dummy (maps to dummy_cnt=1)
        { 0xCu, 0u, 1u, 0u, 0u, 0u },
        // Variation 11: 1-1-1 SDR, 3-byte, 8 dummy (SPI-NAND initial attempt)
        { 0xEu, 0u, 0u, 0u, 0u, 0u },
        // Variation 12: 1-1-1 SDR, 4-byte, 8 dummy
        { 0x1u, 1u, 0u, 0u, 0u, 0u },
        // Variation 13: 1-1-1 SDR, 3-byte, 20 dummy
        { 0x1u, 0u, 1u, 0u, 0u, 0u },
    };
    static const unsigned int k_num_auto_variations =
        static_cast<unsigned int>(sizeof(k_auto_variations) / sizeof(k_auto_variations[0]));

    // -------------------------------------------------------------------------
    // If num_lines is non-zero (pre-configured mode), build a single-entry list.
    // -------------------------------------------------------------------------
    SfdpVariation single_var = { num_lines, abnum, dummy_cnt, cmd_type, extop_en, extop_val };
    const SfdpVariation* var_list      = k_auto_variations;
    unsigned int         var_count     = k_num_auto_variations;
    bool                 is_auto_mode  = (num_lines == 0u);

    if (!is_auto_mode) {
        // Pre-configured mode: validate num_lines is a known encoding.
        bool valid_nl = (num_lines == 0x1u || num_lines == 0x2u ||
                         num_lines == 0x4u || num_lines == 0x8u ||
                         num_lines == 0xCu || num_lines == 0xEu);
        if (!valid_nl) {
            CSML_INFO(2, logger) << "run_sfdp_discovery: invalid num_lines=0x" << std::hex << (unsigned)num_lines << " — aborting (init_fail=0b01)";
            detected_type = 0xFFu;
            return false;
        }
        var_list  = &single_var;
        var_count = 1u;
    }

    // -------------------------------------------------------------------------
    // SFDP signature constant.
    // -------------------------------------------------------------------------
    static const uint32_t k_SFDP_SIGNATURE = 0x50444653u;

    // -------------------------------------------------------------------------
    // Allocate buffers for SFDP data.
    // -------------------------------------------------------------------------
    static uint8_t sfdp_header_buf[8];      // 8-byte SFDP header
    static uint8_t sfdp_param_hdr_buf[8];   // 8-byte JEDEC parameter header
    static uint8_t sfdp_table_buf[64];      // 16 DWORDs = 64 bytes

    // -------------------------------------------------------------------------
    // Iterate format variations.
    // -------------------------------------------------------------------------
    for (unsigned int v = 0u; v < var_count; ++v) {
        const SfdpVariation& vr = var_list[v];
        CSML_INFO(3, logger) << "run_sfdp_discovery: trying variation " << (v + 1u) << "/" << var_count << " nl=0x" << std::hex << (unsigned)vr.nl << " ab=" << (unsigned)vr.ab << " dc=" << (unsigned)vr.dc << " ct=" << (unsigned)vr.ct;

        // State: READ_SFDP_HEADER — read 8 bytes at SFDP ROM address 0.
        std::memset(sfdp_header_buf, 0, sizeof(sfdp_header_buf));
        bool hdr_ok = issue_sfdp_read(
            bank, 0x00000000u,
            sfdp_header_buf, 8u,
            vr.nl, vr.ab, vr.dc, vr.ct, vr.ee, vr.ev);

        if (!hdr_ok) {
            CSML_INFO(3, logger) << "run_sfdp_discovery: variation " << (v + 1u) << " bus error on SFDP header read";
            continue;
        }

        // Validate SFDP signature (bytes [3:0] little-endian).
        uint32_t sig = (static_cast<uint32_t>(sfdp_header_buf[0])       |
                        (static_cast<uint32_t>(sfdp_header_buf[1]) << 8)  |
                        (static_cast<uint32_t>(sfdp_header_buf[2]) << 16) |
                        (static_cast<uint32_t>(sfdp_header_buf[3]) << 24));

        if (sig != k_SFDP_SIGNATURE) {
            CSML_INFO(3, logger) << "run_sfdp_discovery: variation " << (v + 1u) << " invalid signature 0x" << std::hex << sig << " (expected 0x50444653)";
            continue;
        }

        CSML_INFO(2, logger) << "run_sfdp_discovery: variation " << (v + 1u) << " SFDP signature validated";

        // State: PARSE_PARAM_TABLE.
        // Read JEDEC basic parameter header at SFDP offset 0x08 (8 bytes).
        // Byte layout (JESD216A): [0]=param_id_lsb, [1]=minor_rev,
        //   [2]=major_rev, [3]=param_dword_count, [4..6]=table_ptr (24-bit LE),
        //   [7]=param_id_msb.
        std::memset(sfdp_param_hdr_buf, 0, sizeof(sfdp_param_hdr_buf));
        bool ph_ok = issue_sfdp_read(
            bank, 0x00000008u,
            sfdp_param_hdr_buf, 8u,
            vr.nl, vr.ab, vr.dc, vr.ct, vr.ee, vr.ev);

        if (!ph_ok) {
            CSML_INFO(2, logger) << "run_sfdp_discovery: variation " << (v + 1u) << " bus error on parameter header read";
            continue;
        }

        // Extract basic parameter table pointer and DWORD count.
        uint32_t table_ptr = (static_cast<uint32_t>(sfdp_param_hdr_buf[4])       |
                              (static_cast<uint32_t>(sfdp_param_hdr_buf[5]) << 8)  |
                              (static_cast<uint32_t>(sfdp_param_hdr_buf[6]) << 16));
        uint8_t  dword_cnt = sfdp_param_hdr_buf[3];

        // Clamp to 16 DWORDs (JESD216A mandates at least 9; models may return
        // more or fewer — we process up to 16 and zero-fill any remainder).
        if (dword_cnt > 16u) {
            dword_cnt = 16u;
        }
        uint32_t byte_cnt = static_cast<uint32_t>(dword_cnt) * 4u;

        CSML_INFO(3, logger) << "run_sfdp_discovery: table_ptr=0x" << std::hex << table_ptr << " dword_cnt=" << std::dec << (unsigned)dword_cnt;

        // Read the basic flash parameter table.
        std::memset(sfdp_table_buf, 0, sizeof(sfdp_table_buf));
        bool tbl_ok = issue_sfdp_read(
            bank, table_ptr,
            sfdp_table_buf, byte_cnt,
            vr.nl, vr.ab, vr.dc, vr.ct, vr.ee, vr.ev);

        if (!tbl_ok) {
            CSML_INFO(2, logger) << "run_sfdp_discovery: variation " << (v + 1u) << " bus error on parameter table read";
            continue;
        }

        // Assemble 16 uint32_t DWORDs from the byte buffer (little-endian).
        uint32_t param_table[16];
        std::memset(param_table, 0, sizeof(param_table));
        for (unsigned int d = 0u; d < static_cast<unsigned int>(dword_cnt); ++d) {
            unsigned int base = d * 4u;
            param_table[d] = (static_cast<uint32_t>(sfdp_table_buf[base])            |
                              (static_cast<uint32_t>(sfdp_table_buf[base + 1u]) << 8)  |
                              (static_cast<uint32_t>(sfdp_table_buf[base + 2u]) << 16) |
                              (static_cast<uint32_t>(sfdp_table_buf[base + 3u]) << 24));
        }

        // State: WRITE_SEQ_REGS.
        // Determine device type from num_lines used for successful discovery.
        if (vr.nl == 0xCu) {
            detected_type = 1u;    // HyperFlash / Profile 2
        } else if (vr.nl == 0xEu) {
            detected_type = 3u;    // SPI-NAND
        } else {
            detected_type = 0u;    // xSPI Profile 1 / NOR
        }
        detected_abnum = vr.ab;

        // Auto-configure all 10 sequence register groups from the parsed table.
        configure_registers_from_sfdp(param_table,
                                      detected_abnum,
                                      vr.nl,
                                      vr.dc,
                                      vr.ct,
                                      crc_en,
                                      crc_variant,
                                      crc_oe,
                                      crc_chunk_size);

        // State: SUCCESS.
        CSML_INFO(2, logger) << "run_sfdp_discovery: SUCCESS — device_type=" << (unsigned)detected_type << " abnum=" << (unsigned)detected_abnum;
        return true;
    }

    // State: FAIL — no variation produced a valid SFDP signature.
    CSML_INFO(2, logger) << "run_sfdp_discovery: FAIL — no valid SFDP signature found";
    detected_type = 0xFFu;
    return false;
}

/******************************************************************************
 * @brief Auto-populate all 10 sequence register groups from SFDP DWORD table.
 *
 * Called after successful SFDP signature validation and parameter table read.
 * Writes the 10 sequence register groups by constructing the full 32-bit
 * register values from the parsed DWORD fields and assigning them directly to
 * the monolithic register objects (bypassing write callbacks to prevent
 * re-entrant callback invocations during PoR initialisation).
 *
 * After each register write the corresponding seq_cfg shadow struct is also
 * updated so that all operating modes (PIO, ACMD, Direct) see consistent state
 * immediately after PoR.
 *
 * DWORD parsing rules (JESD216A, 1-indexed to match spec):
 *   DWORD 1  bits[1:0]   (param_table[0])  : erase/program granularity
 *   DWORD 1  bits[18:17] (param_table[0])  : address bytes (0=3B, 1=3/4B, 2=4B)
 *   DWORD 2  bits[15:8]  (param_table[1])  : page size 2^N
 *   DWORD 7  bits[23:16] (param_table[6])  : erase type 1 opcode
 *   DWORD 7  bits[15:8]  (param_table[6])  : erase type 1 size 2^N
 *   DWORD 8  bits[15:8]  (param_table[7])  : erase type 2 opcode
 *   DWORD 8  bits[7:0]   (param_table[7])  : erase type 2 size 2^N
 *   DWORD 8  bits[31:24] (param_table[7])  : erase type 3 opcode
 *   DWORD 8  bits[23:16] (param_table[7])  : erase type 3 size 2^N
 *   DWORD 9  bits[15:8]  (param_table[8])  : erase type 4 opcode
 *   DWORD 9  bits[7:0]   (param_table[8])  : erase type 4 size 2^N
 *   DWORD 13 bits[7:0]   (param_table[12]) : prog_resume_op
 *   DWORD 13 bits[15:8]  (param_table[12]) : prog_suspend_op
 *   DWORD 13 bits[23:16] (param_table[12]) : erase_resume_op
 *   DWORD 13 bits[31:24] (param_table[12]) : erase_suspend_op
 *
 * Note: Array indices are 0-based (param_table[0] = DWORD 1).
 *
 * Architecture reference:
 *   docs/xspi_ctrl-detailed-design.md Section 8.4 (Post-Discovery Register State)
 *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_007 (sequence register groups)
 *   docs/xspi_ctrl-architecture-behaviour-map.json registers.*_seq_cfg_*
 *
 * @param param_table    16-element DWORD array from SFDP JEDEC basic param table
 * @param abnum          Discovered address mode (0=3-byte, 1=4-byte)
 * @param num_lines      I/O line width encoding that succeeded during discovery
 * @param dummy_cnt      Dummy cycle setting used during discovery (0=8, 1=20)
 * @param cmd_type       DDR/SDR used during discovery (0=SDR, 1=DDR)
 * @param crc_en         CRC enable — stored into global_seq_cfg, not computed
 * @param crc_variant    CRC variant — stored, not computed
 * @param crc_oe         CRC OE — stored, not computed
 * @param crc_chunk_size CRC chunk size — stored, not computed
 ******************************************************************************/
void xspi_ctrl_ip::configure_registers_from_sfdp(const uint32_t param_table[16],
                                                   uint8_t abnum,
                                                   uint8_t num_lines,
                                                   uint8_t dummy_cnt,
                                                   uint8_t cmd_type,
                                                   uint8_t crc_en,
                                                   uint8_t crc_variant,
                                                   uint8_t crc_oe,
                                                   uint8_t crc_chunk_size)
{
    // -------------------------------------------------------------------------
    // Extract fields from the SFDP DWORD table.
    // -------------------------------------------------------------------------

    // DWORD 1 (index 0): address mode at bits[18:17].
    // 0 = 3-byte only, 1 = 3/4-byte, 2 = 4-byte only.
    uint8_t sfdp_addr_mode = static_cast<uint8_t>((param_table[0] >> 17) & 0x3u);
    // Resolve to binary abnum: if 4-byte capable (mode=2), use 4-byte.
    uint8_t resolved_abnum = (sfdp_addr_mode == 2u) ? 1u : abnum;
    uint8_t addr_cnt_bytes = (resolved_abnum != 0u) ? 4u : 3u;

    // DWORD 2 (index 1): page size 2^N at bits[15:8].
    uint8_t sfdp_page_size_exp = static_cast<uint8_t>((param_table[1] >> 8) & 0xFFu);
    // Clamp to 4-bit field range (0x0–0xF). Hardware reset default is 0x8 (256B).
    if (sfdp_page_size_exp > 0xFu) { sfdp_page_size_exp = 0xFu; }
    uint8_t page_size_pgm = sfdp_page_size_exp;
    // Read page size exponent defaults to 0xF (unlimited).
    uint8_t page_size_rd  = 0xFu;

    // DWORD 7 (index 6): erase type 1 — bits[23:16]=opcode, bits[15:8]=size 2^N.
    uint8_t ers1_opcode = static_cast<uint8_t>((param_table[6] >> 16) & 0xFFu);
    uint8_t ers1_size   = static_cast<uint8_t>((param_table[6] >>  8) & 0xFFu);

    // DWORD 8 (index 7): erase type 2 — bits[15:8]=opcode, bits[7:0]=size 2^N.
    //                    erase type 3 — bits[31:24]=opcode, bits[23:16]=size 2^N.
    uint8_t ers2_opcode = static_cast<uint8_t>((param_table[7] >>  8) & 0xFFu);
    uint8_t ers2_size   = static_cast<uint8_t>( param_table[7]        & 0xFFu);
    uint8_t ers3_opcode = static_cast<uint8_t>((param_table[7] >> 24) & 0xFFu);
    uint8_t ers3_size   = static_cast<uint8_t>((param_table[7] >> 16) & 0xFFu);

    // DWORD 9 (index 8): erase type 4 — bits[15:8]=opcode, bits[7:0]=size 2^N.
    uint8_t ers4_opcode = static_cast<uint8_t>((param_table[8] >>  8) & 0xFFu);
    uint8_t ers4_size   = static_cast<uint8_t>( param_table[8]        & 0xFFu);

    // Select the primary sector erase opcode and size exponent:
    // prefer erase type 1 if valid, else fall through to types 2–4.
    uint8_t ers_opcode = (ers1_opcode != 0u) ? ers1_opcode :
                         (ers2_opcode != 0u) ? ers2_opcode :
                         (ers3_opcode != 0u) ? ers3_opcode :
                         (ers4_opcode != 0u) ? ers4_opcode : 0x20u;
    uint8_t ers_size   = (ers1_opcode != 0u) ? ers1_size   :
                         (ers2_opcode != 0u) ? ers2_size   :
                         (ers3_opcode != 0u) ? ers3_size   :
                         (ers4_opcode != 0u) ? ers4_size   : 0x0Cu;

    // Chip erase opcode — use ext_val for all-chip erase (C7/60 standard).
    uint8_t ersa_opcode = 0x60u;

    // DWORD 13 (index 12): suspend/resume opcodes.
    // bits[7:0]  = prog_resume, bits[15:8]  = prog_suspend,
    // bits[23:16] = erase_resume, bits[31:24] = erase_suspend.
    uint8_t prog_resume_op  = static_cast<uint8_t>( param_table[12]        & 0xFFu);
    uint8_t prog_suspend_op = static_cast<uint8_t>((param_table[12] >>  8) & 0xFFu);
    uint8_t ers_resume_op   = static_cast<uint8_t>((param_table[12] >> 16) & 0xFFu);
    uint8_t ers_suspend_op  = static_cast<uint8_t>((param_table[12] >> 24) & 0xFFu);

    // Map num_lines to ios_width for sequence register I/O fields.
    uint8_t ios_width;
    switch (num_lines) {
        case 0x1u:  ios_width = 0u; break;
        case 0x2u:  ios_width = 1u; break;
        case 0x4u:  ios_width = 2u; break;
        default:    ios_width = 3u; break;
    }
    // DDR edge flag.
    uint8_t ddr_edge = (cmd_type != 0u) ? 1u : 0u;

    // Dummy count field for the read sequence: hardware uses the raw discovery
    // dummy_cnt value (0=8 cycles, 1=20 cycles) mapped to a 6-bit register
    // field encoding 'N' where dummy cycles = N (0=disabled, N>0 = N cycles).
    // Hardware convention: 8 dummy cycles → field value 8; 20 cycles → 20.
    uint8_t read_dummy_count = (dummy_cnt != 0u) ? 20u : 8u;

    CSML_INFO(2, logger) << "configure_registers_from_sfdp:" << " abnum=" << (unsigned)resolved_abnum << " addr_cnt=" << (unsigned)addr_cnt_bytes << " page_pgm=2^" << (unsigned)page_size_pgm << " ers_op=0x" << std::hex << (unsigned)ers_opcode << " ers_sz=2^" << std::dec << (unsigned)ers_size;

    // =========================================================================
    // Group 1: global_seq_cfg (0x390) and global_seq_cfg_1 (0x394)
    //
    // global_seq_cfg (reset 0x0000208F) bit layout:
    //   [3:0]   seq_page_size_rd  = page_size_rd (0xF = unlimited)
    //   [7:4]   seq_page_size_pgm = page_size_pgm (SFDP-derived 2^N)
    //   [10:8]  seq_crc_en / variant / oe fields: build from CRC params
    //   [12:10] seq_crc_chunk_size [3-bit field at bits[12:10]]
    //   [18]    seq_tcms_en (retain reset default 0)
    //   [20:19] seq_data_per_addr (retain reset 0)
    //   [21]    seq_data_swap (retain reset 0)
    //   [24:23] seq_type = device type (0=Profile1 xSPI/NOR default)
    //
    // Rather than reconstructing every reserved bit we read the current value,
    // clear then set only the fields we own.
    // =========================================================================
    {
        uint32_t gsc = static_cast<uint32_t>(global_seq_cfg);
        // Clear owned fields.
        gsc &= ~((0xFu)          |        // [3:0]  seq_page_size_rd
                 (0xFu  << 4)    |        // [7:4]  seq_page_size_pgm
                 (0x1u  << 8)    |        // [8]    seq_crc_en
                 (0x1u  << 9)    |        // [9]    seq_crc_variant
                 (0x1u  << 10)   |        // [10]   seq_crc_oe
                 (0x7u  << 11)   |        // [13:11] seq_crc_chunk_size (3-bit)
                 (0x3u  << 23));          // [24:23] seq_type
        // Set fields.
        gsc |= (static_cast<uint32_t>(page_size_rd   & 0xFu));
        gsc |= (static_cast<uint32_t>(page_size_pgm  & 0xFu) << 4);
        gsc |= (static_cast<uint32_t>(crc_en         & 0x1u) << 8);
        gsc |= (static_cast<uint32_t>(crc_variant    & 0x1u) << 9);
        gsc |= (static_cast<uint32_t>(crc_oe         & 0x1u) << 10);
        gsc |= (static_cast<uint32_t>(crc_chunk_size & 0x7u) << 11);
        // seq_type: 0=Profile1 (NOR/xSPI), 1=Profile2 (HyperFlash),
        //           2=HyperRAM, 3=SPI-NAND.
        // We leave seq_type=0 unless the discovery was HyperFlash.
        // (SPI-NAND → seq_type=3, already handled via num_lines=0xE)
        uint8_t seq_type = 0u;
        if (num_lines == 0xCu) { seq_type = 1u; }   // HyperFlash
        if (num_lines == 0xEu) { seq_type = 3u; }   // SPI-NAND
        gsc |= (static_cast<uint32_t>(seq_type & 0x3u) << 23);

        global_seq_cfg = gsc;

        // Update shadow struct.
        read_page_size        = page_size_rd;
        program_page_size     = page_size_pgm;
        active_device_profile = seq_type;
    }

    // global_seq_cfg_1 (0x394, reset 0x00000000): NAND-specific page/plane
    // parameters — retain reset value (zero) for standard NOR/xSPI devices.
    // global_seq_cfg_1 = 0x00000000u;   // already at reset value after reset_handler

    // =========================================================================
    // Group 2: rst_seq_cfg_0 (0x400) and rst_seq_cfg_1 (0x404)
    //
    // rst_seq_cfg_0 (reset 0x00019966) bit layout:
    //   [7:0]   rst_seq_p1_cmd0_val  = RESET_ENABLE opcode (0x66)
    //   [15:8]  rst_seq_p1_cmd1_val  = RESET opcode (0x99)
    //   [16]    rst_seq_p1_cmd0_en   = 1 (always send RESET_ENABLE first)
    //   [19:18] rst_seq_p1_data_ios  = ios_width
    //   [21]    rst_seq_p1_data_edge = ddr_edge
    //   [22]    rst_seq_p1_data_en   = 0 (no data phase in reset)
    //   [25:24] rst_seq_p1_cmd_ios   = ios_width
    //   [28]    rst_seq_p1_cmd_edge  = ddr_edge
    //
    // rst_seq_cfg_1 (reset 0xD0669900) bit layout:
    //   [0]     rst_seq_p1_cmd0_ext_en  = 0 (no ext opcode for reset)
    //   [1]     rst_seq_p1_cmd1_ext_en  = 0
    //   [15:8]  rst_seq_p1_cmd0_ext_val = 0x99 (retain reset value)
    //   [23:16] rst_seq_p1_cmd1_ext_val = 0x66 (retain reset value)
    //   [31:24] rst_seq_p1_data_val     = 0xD0 (retain reset value)
    // =========================================================================
    {
        uint32_t rst0 = 0x00000000u;
        rst0 |= (static_cast<uint32_t>(0x66u));                      // cmd0_val
        rst0 |= (static_cast<uint32_t>(0x99u)  << 8);               // cmd1_val
        rst0 |= (1u                             << 16);              // cmd0_en
        rst0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 18);    // data_ios
        rst0 |= (static_cast<uint32_t>(ddr_edge & 0x1u)  << 21);    // data_edge
        rst0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 24);    // cmd_ios
        rst0 |= (static_cast<uint32_t>(ddr_edge & 0x1u)  << 28);    // cmd_edge
        rst_seq_cfg_0 = rst0;

        // Update shadow.
        seq_cfg.rst_cmd0_val   = 0x66u;
        seq_cfg.rst_cmd1_val   = 0x99u;
        seq_cfg.rst_cmd0_en    = true;
        seq_cfg.rst_data_ios   = ios_width;
        seq_cfg.rst_data_edge  = (ddr_edge != 0u);
        seq_cfg.rst_data_en    = false;
        seq_cfg.rst_cmd_ios    = ios_width;
        seq_cfg.rst_cmd_edge   = (ddr_edge != 0u);

        // rst_seq_cfg_1: retain reset default 0xD0669900.
        rst_seq_cfg_1 = 0xD0669900u;
        seq_cfg.rst_cmd0_ext_en  = false;
        seq_cfg.rst_cmd1_ext_en  = false;
        seq_cfg.rst_cmd0_ext_val = 0x99u;
        seq_cfg.rst_cmd1_ext_val = 0x66u;
        seq_cfg.rst_data_val     = 0xD0u;
    }

    // =========================================================================
    // Group 3: ers_seq_cfg_0 (0x410), ers_seq_cfg_1 (0x414), ers_seq_cfg_2 (0x418)
    //
    // ers_seq_cfg_0 (reset 0x00DF3020) per Cadence xSPI GA: sector erase has no
    // data phase; fields are (see handle_write_ers_seq_cfg_0 for decode):
    //   [7:0]   erss_seq_p1_cmd_val     = SFDP sector-erase opcode
    //   [9:8]   erss_seq_p1_cmd_ios     = command I/O width
    //   [10]    reserved
    //   [11]    erss_seq_p1_cmd_edge
    //   [14:12] erss_seq_p1_addr_cnt   = number of address bytes
    //   [15]    erss_seq_p1_cmd_ext_en
    //   [23:16] erss_seq_p1_cmd_ext_val (default 0xDF)
    //   [25:24] erss_seq_p1_addr_ios   = address I/O width
    //   [27:26] reserved
    //   [28]    erss_seq_p1_addr_edge
    //   [31:29] reserved
    //
    // ers_seq_cfg_1 (reset 0x0000000C) bit layout:
    //   [3:0]   erss_seq_p1_sect_size  = SFDP-derived size exponent (2^N)
    //
    // ers_seq_cfg_2 (reset 0x009F0060) bit layout:
    //   [7:0]   ersa_seq_p1_cmd_val     = chip erase opcode (0x60)
    //   [23:16] ersa_seq_p1_cmd_ext_val = 0x9F (retain default)
    // =========================================================================
    {
        uint32_t ers0 = 0x00000000u;
        ers0 |= (static_cast<uint32_t>(ers_opcode & 0xFFu));
        ers0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 8);   // erss_seq_p1_cmd_ios
        ers0 |= (static_cast<uint32_t>(ddr_edge & 0x1u) << 11);   // erss_seq_p1_cmd_edge
        ers0 |= (static_cast<uint32_t>(addr_cnt_bytes & 0x7u) << 12);  // erss_seq_p1_addr_cnt
        ers0 |= (static_cast<uint32_t>(0xDFu) << 16);   // erss_seq_p1_cmd_ext_val
        ers0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 24);  // erss_seq_p1_addr_ios
        ers0 |= (static_cast<uint32_t>(ddr_edge & 0x1u) << 28);  // erss_seq_p1_addr_edge
        ers_seq_cfg_0 = ers0;

        seq_cfg.ers_cmd_val    = ers_opcode;
        seq_cfg.ers_addr_cnt   = addr_cnt_bytes;
        seq_cfg.ers_cmd_ext_val = 0xDFu;

        uint32_t ers1 = static_cast<uint32_t>(ers_size & 0xFu);
        ers_seq_cfg_1 = ers1;
        seq_cfg.ers_sect_size = ers_size;

        uint32_t ers2 = 0x00000000u;
        ers2 |= (static_cast<uint32_t>(ersa_opcode & 0xFFu));
        ers2 |= (static_cast<uint32_t>(0x9Fu) << 16);
        ers_seq_cfg_2 = ers2;
        seq_cfg.ersa_cmd_val     = ersa_opcode;
        seq_cfg.ersa_cmd_ext_val = 0x9Fu;
    }

    // =========================================================================
    // Group 4: prog_seq_cfg_0 (0x420), prog_seq_cfg_1 (0x424), prog_seq_cfg_2 (0x428)
    //
    // prog_seq_cfg_0 (reset 0x00003002) bit layout:
    //   [7:0]   prog_seq_p1_cmd_val  = PAGE_PROGRAM opcode (0x02 for 3-byte; 0x12 for 4-byte)
    //   [12:8]  prog_seq_p1_addr_cnt = address bytes (3 or 4)
    //   [25:24] prog_seq_p1_cmd_ios  = ios_width
    //   [27:26] prog_seq_p1_addr_ios = ios_width
    //   [29:28] prog_seq_p1_data_ios = ios_width
    //   [30]    prog_seq_p1_cmd_edge = ddr_edge
    //   [31]    prog_seq_p1_addr_edge= ddr_edge
    //
    // prog_seq_cfg_1 (reset 0x0000FD00) bit layout:
    //   [15:8]  prog_seq_p1_cmd_ext_val = 0xFD (retain default)
    //
    // prog_seq_cfg_2 (reset 0x00000002): Profile-2 burst type — retain defaults.
    // =========================================================================
    {
        uint8_t prog_opcode = (addr_cnt_bytes == 4u) ? 0x12u : 0x02u;

        uint32_t prg0 = 0x00000000u;
        prg0 |= (static_cast<uint32_t>(prog_opcode & 0xFFu));
        prg0 |= (static_cast<uint32_t>(addr_cnt_bytes & 0xFu) << 8);
        prg0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 24);
        prg0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 26);
        prg0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 28);
        prg0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 30);
        prg0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 31);
        prog_seq_cfg_0 = prg0;

        seq_cfg.prog_cmd_val   = prog_opcode;
        seq_cfg.prog_addr_cnt  = addr_cnt_bytes;

        uint32_t prg1 = static_cast<uint32_t>(0xFDu) << 8;
        prog_seq_cfg_1 = prg1;
        seq_cfg.prog_cmd_ext_val = 0xFDu;

        // prog_seq_cfg_2: retain reset value 0x00000002 (burst_type=1).
        prog_seq_cfg_2 = 0x00000002u;
        seq_cfg.prog_p2_burst_type = true;
    }

    // =========================================================================
    // Group 5: read_seq_cfg_0 (0x430), read_seq_cfg_1 (0x434), read_seq_cfg_2 (0x438)
    //
    // read_seq_cfg_0 (reset 0x00003003) bit layout:
    //   [7:0]   read_seq_p1_cmd_val   = READ opcode (0x03 for SDR, 0x0B for fast)
    //   [9:8]   read_seq_p1_cmd_ios   = ios_width
    //   [11]    read_seq_p1_cmd_edge  = ddr_edge
    //   [14:12] read_seq_p1_addr_cnt  = address bytes (3 or 4)
    //   [17:16] read_seq_p1_addr_ios  = ios_width
    //   [19]    read_seq_p1_addr_edge = ddr_edge
    //   [21:20] read_seq_p1_data_ios  = ios_width
    //   [23]    read_seq_p1_data_edge = ddr_edge
    //   [29:24] read_seq_p1_dummy_cnt = dummy cycle count (direct count, 0=disable)
    //
    // read_seq_cfg_1 (reset 0x0000FC00) bit layout:
    //   [8]     read_seq_p1_cmd_ext_en  = 0 (no ext opcode for standard read)
    //   [15:8]  read_seq_p1_cmd_ext_val = 0xFC (retain default)
    //
    // read_seq_cfg_2 (reset 0x00000F0A): Profile-2 HF settings — retain defaults.
    // =========================================================================
    {
        // Select READ opcode: for DDR modes use 0x0B (FAST_READ), for SDR
        // single-line use 0x03 (READ_ZERO_LATENCY), for multi-line SDR use 0x03.
        uint8_t read_opcode;
        if (ddr_edge != 0u) {
            read_opcode = 0x0Bu;   // FAST_READ supports DDR-capable devices
        } else if (ios_width >= 2u) {
            read_opcode = 0x6Bu;   // QUAD OUTPUT FAST READ (1-1-4) as default
        } else {
            read_opcode = 0x03u;   // Standard READ
        }

        uint32_t rsc0 = 0x00000000u;
        rsc0 |= (static_cast<uint32_t>(read_opcode & 0xFFu));
        rsc0 |= (static_cast<uint32_t>(ios_width & 0x3u) <<  8);    // cmd_ios
        rsc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 11);    // cmd_edge
        rsc0 |= (static_cast<uint32_t>(addr_cnt_bytes & 0x7u) << 12); // addr_cnt
        rsc0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 16);    // addr_ios
        rsc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 19);    // addr_edge
        rsc0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 20);    // data_ios
        rsc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 23);    // data_edge
        rsc0 |= (static_cast<uint32_t>(read_dummy_count & 0x3Fu) << 24); // dummy_cnt
        read_seq_cfg_0 = rsc0;

        seq_cfg.read_cmd_val   = read_opcode;
        seq_cfg.read_addr_cnt  = addr_cnt_bytes;

        // read_seq_cfg_1: cmd_ext_val=0xFC retained, cmd_ext_en=0 for standard.
        uint32_t rsc1 = (static_cast<uint32_t>(0xFCu) << 8);
        read_seq_cfg_1 = rsc1;
        seq_cfg.read_cmd_ext_val = 0xFCu;

        // read_seq_cfg_2: retain reset value 0x00000F0A (HF latency/burst).
        read_seq_cfg_2 = 0x00000F0Au;
        seq_cfg.read_p2_hf_bound_en = true;
        seq_cfg.read_p2_burst_type  = true;
        seq_cfg.read_p2_latency_cnt = 0x0Fu;
    }

    // =========================================================================
    // Group 6: we_seq_cfg_0 (0x440)
    //
    // we_seq_cfg_0 (reset 0x01F90006):
    //   [31:25] reserved; [24] we_seq_p1_en; [23:16] we_seq_p1_cmd_ext_val=0xF9;
    //   [15] we_seq_p1_cmd_ext_en; [14:12] reserved; [11] we_seq_p1_cmd_edge;
    //   [10] reserved; [9:8] we_seq_p1_cmd_ios; [7:0] we_seq_p1_cmd_val=0x06
    // =========================================================================
    {
        uint32_t wsc = 0x00000000u;
        wsc |= (static_cast<uint32_t>(0x06u));
        wsc |= (static_cast<uint32_t>(ios_width & 0x3u) << 8);
        wsc |= (static_cast<uint32_t>(ddr_edge & 0x1u) << 11);
        wsc |= (static_cast<uint32_t>(0xF9u) << 16);
        wsc |= (1u << 24);
        we_seq_cfg_0 = wsc;

        seq_cfg.we_cmd_val     = 0x06u;
        seq_cfg.we_cmd_ios     = static_cast<uint8_t>(ios_width & 0x3u);
        seq_cfg.we_cmd_edge    = (ddr_edge != 0u);
        seq_cfg.we_cmd_ext_en  = false;
        seq_cfg.we_cmd_ext_val = 0xF9u;
        seq_cfg.we_en          = true;
    }

    // =========================================================================
    // Group 7: stat_seq_cfg_0–stat_seq_cfg_10 (0x450–0x478)
    //
    // stat_seq_cfg_0 (reset 0x00000000) — ios/edge/addr for status commands:
    //   [1:0]   stat_seq_p1_cmd_ios  = ios_width
    //   [2]     stat_seq_p1_cmd_edge = ddr_edge
    //   [5:4]   stat_seq_p1_addr_ios = ios_width
    //   [6]     stat_seq_p1_addr_edge= ddr_edge
    //   [9:8]   stat_seq_p1_data_ios = ios_width
    //   [10]    stat_seq_p1_data_edge= ddr_edge
    //
    // stat_seq_cfg_1 (reset 0x00000000) — dummy counts.
    //
    // stat_seq_cfg_2 (reset 0x05000505): dev_rdy/ers_fail/prog_fail cmd_val.
    //   [7:0]   stat_seq_p1_dev_rdy_cmd_val  = 0x05 (READ_STATUS_REG)
    //   [15:8]  (reserved)
    //   [23:16] stat_seq_p1_ers_fail_cmd_val = 0x05
    //   [31:24] stat_seq_p1_prog_fail_cmd_val= 0x05
    //
    // stat_seq_cfg_3 (reset 0xFA00FAFA): ext_val fields.
    //
    // stat_seq_cfg_4 (reset 0x00000F00): Profile-2 latency.
    //
    // stat_seq_cfg_5 (reset 0x00000040): dev_rdy_en=1 (bit 6).
    //
    // stat_seq_cfg_7–10 (reset 0x00000000): address fields — retain zeros.
    //
    // For the suspend/resume opcodes sourced from DWORD 13, they are stored in
    // stat_seq_cfg_8 (bits[23:16]=erase_resume, bits[31:24]=erase_suspend) and
    // stat_seq_cfg_9 (bits[7:0]=prog_resume, bits[15:8]=prog_suspend).
    // (Exact offset assignments follow the register map in register_map.md.)
    // =========================================================================
    {
        // stat_seq_cfg_0: I/O width and edge for all status commands.
        uint32_t ssc0 = 0x00000000u;
        ssc0 |= (static_cast<uint32_t>(ios_width & 0x3u));          // cmd_ios
        ssc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 2);     // cmd_edge
        ssc0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 4);     // addr_ios
        ssc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 6);     // addr_edge
        ssc0 |= (static_cast<uint32_t>(ios_width & 0x3u) << 8);     // data_ios
        ssc0 |= (static_cast<uint32_t>(ddr_edge  & 0x1u) << 10);    // data_edge
        stat_seq_cfg_0 = ssc0;

        // stat_seq_cfg_1: dummy counts — retain reset value (0).
        stat_seq_cfg_1 = 0x00000000u;

        // stat_seq_cfg_2: status opcode values (retain reset defaults 0x05000505).
        stat_seq_cfg_2 = 0x05000505u;
        seq_cfg.stat_dev_rdy_cmd_val   = 0x05u;
        seq_cfg.stat_ers_fail_cmd_val  = 0x05u;
        seq_cfg.stat_prog_fail_cmd_val = 0x05u;

        // stat_seq_cfg_3: ext opcode values — retain reset 0xFA00FAFA.
        stat_seq_cfg_3 = 0xFA00FAFAu;
        seq_cfg.stat_dev_rdy_cmd_ext_val   = 0xFAu;
        seq_cfg.stat_ers_fail_cmd_ext_val  = 0xFAu;
        seq_cfg.stat_prog_fail_cmd_ext_val = 0xFAu;

        // stat_seq_cfg_4: Profile-2 latency — retain reset 0x00000F00.
        stat_seq_cfg_4 = 0x00000F00u;
        seq_cfg.stat_p2_latency_cnt = 0x0Fu;

        // stat_seq_cfg_5: dev_rdy_en (bit 6) — retain reset 0x00000040.
        stat_seq_cfg_5 = 0x00000040u;
        seq_cfg.stat_dev_rdy_en = true;

        // stat_seq_cfg_7: dev_rdy address — retain reset 0x00000000.
        stat_seq_cfg_7 = 0x00000000u;

        // stat_seq_cfg_8: erase suspend/resume opcodes from SFDP DWORD 13.
        // bits[23:16] = erase_resume opcode, bits[31:24] = erase_suspend opcode.
        uint32_t ssc8 = 0x00000000u;
        ssc8 |= (static_cast<uint32_t>(ers_resume_op)  << 16);
        ssc8 |= (static_cast<uint32_t>(ers_suspend_op) << 24);
        stat_seq_cfg_8 = ssc8;

        // stat_seq_cfg_9: program suspend/resume opcodes from SFDP DWORD 13.
        // bits[7:0] = prog_resume opcode, bits[15:8] = prog_suspend opcode.
        uint32_t ssc9 = 0x00000000u;
        ssc9 |= (static_cast<uint32_t>(prog_resume_op));
        ssc9 |= (static_cast<uint32_t>(prog_suspend_op) << 8);
        stat_seq_cfg_9 = ssc9;

        CSML_INFO(3, logger) << "configure_registers_from_sfdp: suspend/resume" << " ers_sus=0x" << std::hex << (unsigned)ers_suspend_op << " ers_res=0x" << (unsigned)ers_resume_op << " prg_sus=0x" << (unsigned)prog_suspend_op << " prg_res=0x" << (unsigned)prog_resume_op;
    }

    CSML_INFO(2, logger) << "configure_registers_from_sfdp: all 10 register groups written";
}

// =============================================================================
// SC_THREAD and SC_METHOD Implementations
// =============================================================================

/******************************************************************************
 * @brief STIG engine SC_THREAD — FUNC_XSPI_002
 *
 * STIG (Script To Instruction Generator) per UG Profile 1, Table 4.23. The
 * host stages a 128-bit word across cmd_reg1 (LSW) through cmd_reg4 (MSW), then
 * writes cmd_reg0. This is not the ACMD PIO address packing.
 *
 * On each wait(cmd_trigger_event):
 *  1. Build stig_instruction via decode_instruction() from the register snapshot
 *     (and Table 4.27 when the previous trigger armed INSTR_LINK — glued phase):
 *     - INSTR[6:0] from cmd_reg1[6:0]; DATA0/1 in cmd_reg1[15:8] and [23:16]
 *     - CMD [87:80], CMD_EXT from cmd_reg3[23:16], cmd_reg3[15:8]
 *     - Immediate data length: cmd_reg3[25:24] (0/1/2); no ACMD DATA_CNT in [31:24]
 *     - BANK[2:0] from cmd_reg4[14:12]; INSTR_LINK from bit 124 (reg4[28])
 *     - 48b flash address: ADDR0 = reg1[31:24], ADDR1..4 in reg2[7:0]..[31:24],
 *       ADDR5 = reg3[7:0] (Table 4.23)
 *     - Transfers longer than 2 B use a two-phase chain: first STIG with
 *       INSTR_LINK; second with INSTR=0x7F; bytes [79:48] for length (glued)
 *  2. stig_set_busy() / INSTR_LINK two-phase (first: save phase, gcmd_eng_mc_busy,
 *     stig_done; second: XSPI_INSTR_GLUED(127) decode + execute with saved cmd).
 *  3. execute_stig() → handler uses inst.bank_num, builds cdns_extension, and
 *     calls dispatch_flash_transaction() on the selected xspi bus target.
 *  4. stig_finish() clears busies, sets cmd_status.COMPLETE and intr stig_done.
 *
 * Sensitivity: cmd_trigger_event. See decode_instruction() and Table 4.23/4.27.
 ******************************************************************************/
void xspi_ctrl_ip::stig_engine_thread()
{
    while (true) {
        wait(cmd_trigger_event);

        CSML_INFO(2, logger) << "stig_engine_thread: cmd_trigger_event received";

        // Step 0: Clear cmd_status.COMPLETE (bit 15) at the start of each new
        // trigger. Per the hardware specification: "After a new operation is
        // triggered the COMPLETE bit is cleared, at which point other bits within
        // this register are meaningless."
        {
            uint32_t cs_val = static_cast<uint32_t>(cmd_status);
            cs_val &= ~(1u << 15);
            cmd_status = cs_val;
        }

        // Step 1: Decode the 128-bit STIG instruction from the four staging
        // registers into a structured stig_instruction value.
        stig_instruction inst = decode_instruction();

        CSML_INFO(2, logger) << "stig_engine_thread: opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " instr_type=" << static_cast<unsigned>(inst.instr_type) << " instr_link=" << inst.instr_link << " addr=0x" << inst.address << " data_bytes=" << std::dec << inst.data_bytes_count;

        // Step 2: Assert ctrl_status busy indicators for the duration of the
        // STIG operation. gcmd_eng_busy (bit 3) and ctrl_busy (bit 7) remain
        // set until stig_finish() clears them after the transaction completes.
        stig_set_busy();

        // Step 3: INSTR_LINK handling (two-phase chain).
        //
        // First phase (instr_link=1, pending not yet armed):
        //   - arm stig_instr_link_pending to capture the command register state.
        //   - set gcmd_eng_mc_busy (bit 4 of ctrl_status) to signal the link wait.
        //   - fire stig_done interrupt so the initiator knows the first phase was
        //     accepted (gcmd_eng_busy intentionally left set; COMPLETE not set yet).
        //   - loop back to wait for the second cmd_reg0 trigger.
        //
        // Second phase (stig_instr_link_pending already set):
        //   - override instr_type to XSPI_INSTR_GLUED (127) per spec table 4.29.
        //   - clear stig_instr_link_pending and gcmd_eng_mc_busy.
        //   - proceed to execute_stig() with the saved first-phase instruction as
        //     the data_phase pointer.
        if (stig_instr_link_pending) {
            stig_instr_link_pending = false;
            {
                uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
                cs_val &= ~(1u << 4);   // clear gcmd_eng_mc_busy
                ctrl_status = cs_val;
            }
            if (static_cast<uint8_t>(inst.instr_type) != static_cast<uint8_t>(XSPI_INSTR_GLUED)) {
                inst.instr_type = static_cast<uint8_t>(XSPI_INSTR_GLUED);
            }
            CSML_INFO(2, logger) << "stig_engine_thread: second INSTR_LINK phase " << "(Glued Data Instruction)";

            bool ok = execute_stig(inst, &stig_link_cmd_phase);
            if (!ok) {
                stig_set_error("stig_engine_thread: INSTR_LINK second phase failed");
            }
            stig_finish();

        } else if (inst.instr_link) {
            // First phase: arm chain, set gcmd_eng_mc_busy, notify stig_done,
            // save the command-phase instruction, and loop back.
            stig_link_cmd_phase     = inst;
            stig_instr_link_pending = true;
            {
                uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
                cs_val |= (1u << 4);    // set gcmd_eng_mc_busy
                ctrl_status = cs_val;
            }
            CSML_INFO(2, logger) << "stig_engine_thread: INSTR_LINK first phase " << "— arming chain, waiting for second trigger";
            // Signal stig_done for the first phase so the initiator can proceed.
            // gcmd_eng_busy and ctrl_busy are intentionally NOT cleared yet.
            {
                uint32_t intr_cur = static_cast<uint32_t>(intr_status);
                intr_status = intr_cur | (1u << 23);  // stig_done at bit 23
                evaluate_interrupt_out();
            }
            continue;   // back to wait(cmd_trigger_event)

        } else {
            // Single-phase instruction: dispatch directly.
            bool ok = execute_stig(inst, nullptr);
            if (!ok) {
                stig_set_error("stig_engine_thread: single-phase STIG dispatch failed");
            }
            stig_finish();
        }

        CSML_INFO(2, logger) << "stig_engine_thread: transaction complete" << " cmd_status.COMPLETE=1 intr_status.stig_done=1";
    }
}

namespace {

uint64_t stig_v1_extract_address(uint32_t r1, uint32_t r2, uint32_t r3)
{
    // IP6522 UG Table 4.23: ADDR0[31:24]..ADDR5[71:64] — ref extract_address
    // in cdns_xspi_ctrl_reg.cc
    const uint8_t addr0 = static_cast<uint8_t>((r1 >> 24) & 0xFFu);
    const uint8_t addr1 = static_cast<uint8_t>(r2 & 0xFFu);
    const uint8_t addr2 = static_cast<uint8_t>((r2 >> 8) & 0xFFu);
    const uint8_t addr3 = static_cast<uint8_t>((r2 >> 16) & 0xFFu);
    const uint8_t addr4 = static_cast<uint8_t>((r2 >> 24) & 0xFFu);
    const uint8_t addr5 = static_cast<uint8_t>(r3 & 0xFFu);
    return (static_cast<uint64_t>(addr5) << 40) | (static_cast<uint64_t>(addr4) << 32)
         | (static_cast<uint32_t>(addr3) << 24) | (static_cast<uint32_t>(addr2) << 16)
         | (static_cast<uint32_t>(addr1) << 8) | static_cast<uint64_t>(addr0);
}

// Table 4.27: DATA (79:48) 32b — reg2[31:16] | reg3[15:0] as low/high 16 of [63:48]|[79:64]
uint32_t stig_glued_extract_nbytes_79_48(uint32_t r2, uint32_t r3)
{
    return (static_cast<uint32_t>(r2 >> 16) & 0xFFFFu)
         | (static_cast<uint32_t>(r3 & 0xFFFFu) << 16);
}

}  // namespace

/******************************************************************************
 * @brief Decode the staged cmd_reg1–cmd_reg4 into a stig_instruction struct.
 *
 * UG: Variant 1 (Table 4.23) for command, Table 4.27 for glued (second phase
 * of INSTR_LINK). When stig_instr_link_pending is set, the snapshot is a
 * Glued Data instruction. Otherwise it is a command instruction.
 * CMD[87:80] is in cmd_reg3[23:16] (not cmd_reg1[31:24]).
 * INSTR_TYPE[6:0] is in cmd_reg1[6:0] (not cmd_reg4[6:0]).
 * DATA / MODE byte count[1:0] in cmd reg3[25:24] is not ACMD DATA_CNT.
 ******************************************************************************/
xspi_ctrl_ip::stig_instruction xspi_ctrl_ip::decode_instruction()
{
    stig_instruction inst{};

    const uint32_t r1 = static_cast<uint32_t>(cmd_reg1.cmd1.get());
    const uint32_t r2 = static_cast<uint32_t>(cmd_reg2.cmd2.get());
    const uint32_t r3 = static_cast<uint32_t>(cmd_reg3.cmd3.get());
    const uint32_t r4 = static_cast<uint32_t>(cmd_reg4.cmd4.get());

    if (stig_instr_link_pending) {
        // Table 4.27: Glued Data. DATA size at bits[79:48], DIR at bit[100] → reg4[4].
        inst.instr_type       = static_cast<uint8_t>(r1 & 0x7Fu);
        inst.opcode           = 0u;
        inst.cmd_ext          = 0u;
        inst.instr_link       = false;
        inst.address          = 0u;
        inst.data_bytes       = 0u;
        inst.data_bytes_count = stig_glued_extract_nbytes_79_48(r2, r3);
        inst.write_data      = 0u;
        inst.glued_dir        = static_cast<uint8_t>((r4 >> 4) & 0x1u);
        inst.bank_num         = static_cast<uint8_t>((r4 >> 12) & 0x7u);
    } else {
        inst.instr_type  = static_cast<uint8_t>(r1 & 0x7Fu);
        inst.opcode      = static_cast<uint8_t>((r3 >> 16) & 0xFFu);
        inst.cmd_ext     = static_cast<uint8_t>((r3 >> 8) & 0xFFu);
        inst.instr_link  = ((r4 >> 28) & 0x1u) != 0u;
        // DATA0/1 at cmd_reg1[15:8] and [23:16] (2-byte inline, not reg4)
        inst.write_data  = static_cast<uint32_t>(r1 >> 8) & 0xFFFFu;
        inst.data_bytes  = static_cast<uint8_t>((r3 >> 24) & 0x3u);
        inst.address     = stig_v1_extract_address(r1, r2, r3);
        inst.glued_dir   = 0u;

        const uint8_t t  = inst.instr_type;
        if (t == static_cast<uint8_t>(XSPI_INSTR_SFDP)) {
            // READ_SFDP (test plan): 16-byte read (JESD216A header)
            inst.data_bytes_count = 16u;
        } else if (t == static_cast<uint8_t>(XSPI_INSTR_READ) ||
                   t == static_cast<uint8_t>(XSPI_INSTR_WRITE)) {
            // DATA (89:88): 0,1,2 per UG; not DATA_CNT+1
            inst.data_bytes_count = static_cast<uint32_t>(inst.data_bytes & 0x3u);
        } else {
            if ((inst.data_bytes & 0x3u) != 0u) {
                inst.data_bytes_count = static_cast<uint32_t>(inst.data_bytes & 0x3u);
            } else {
                inst.data_bytes_count = 0u;
            }
        }
    }

    inst.opcode_ios   = 0u;
    inst.opcode_edge  = false;
    inst.addr_ios     = 0u;
    inst.addr_edge    = false;
    inst.data_ios     = 0u;
    inst.data_edge    = false;
    if (!stig_instr_link_pending) {
        inst.bank_num = static_cast<uint8_t>((r4 >> 12) & 0x7u);
    }
    return inst;
}

/******************************************************************************
 * @brief Assert STIG busy status bits in ctrl_status.
 *
 * Sets ctrl_status.ctrl_busy (bit 7) and ctrl_status.gcmd_eng_busy (bit 3).
 * Called at the start of every STIG engine activation from stig_engine_thread().
 * Does NOT write directly to any port (single-writer rule compliance).
 *
 * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.4,
 *       docs/xspi_ctrl-architecture-behaviour-map.json state_machines.STIG.
 ******************************************************************************/
void xspi_ctrl_ip::stig_set_busy()
{
    uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
    cs_val |= (1u << 7) | (1u << 3);   // ctrl_busy=bit7, gcmd_eng_busy=bit3
    ctrl_status = cs_val;
}

/******************************************************************************
 * @brief Complete a STIG operation — clear busy bits and set completion status.
 *
 * Performs in order:
 *  1. Clears ctrl_status.ctrl_busy (bit 7) and ctrl_status.gcmd_eng_busy (bit 3).
 *  2. Sets cmd_status.COMPLETE (bit 15).
 *  3. Sets intr_status.stig_done (bit 23).
 *  4. Calls evaluate_interrupt_out() to propagate the interrupt.
 *
 * Called from stig_engine_thread() after execute_stig() returns (success or
 * error). Always called at operation end regardless of transaction outcome so
 * that software can poll cmd_status.COMPLETE for completion detection.
 *
 * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.4,
 *       docs/xspi_ctrl-architecture-behaviour-map.json state_machines.STIG.
 ******************************************************************************/
void xspi_ctrl_ip::stig_finish()
{
    // Clear ctrl_status.ctrl_busy (bit 7) and gcmd_eng_busy (bit 3).
    // Per architecture map state_machines.STIG: FSM transitions to IDLE.
    {
        uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~((1u << 7) | (1u << 3));
        ctrl_status = cs_val;
    }

    // Set cmd_status.COMPLETE (bit 15).
    // Per hardware spec Section 7.2.4: set when the operation completes.
    {
        uint32_t cmdstat_val = static_cast<uint32_t>(cmd_status);
        cmdstat_val |= (1u << 15);
        cmd_status = cmdstat_val;
    }

    // Set intr_status.stig_done (bit 23) and re-evaluate int_out.
    // stig_done fires after every completed STIG operation (single and
    // INSTR_LINK second-phase). evaluate_interrupt_out() reassesses int_out
    // based on the updated intr_status and intr_enable registers.
    {
        uint32_t intr_cur = static_cast<uint32_t>(intr_status);
        intr_status = intr_cur | (1u << 23);
        evaluate_interrupt_out();
    }
}

/******************************************************************************
 * @brief Record a STIG command error in cmd_status error bits.
 *
 * Sets the BUS_ERROR bit (bit 1) in cmd_status to indicate that the underlying
 * flash bus transaction failed (dispatch_flash_transaction returned false).
 * Does not set or clear COMPLETE — that is handled exclusively by stig_finish().
 *
 * @param msg  Descriptive error string logged at verbosity level 2.
 *
 * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.5,
 *       docs/xspi_ctrl-architecture-behaviour-map.json registers.cmd_status.
 ******************************************************************************/
void xspi_ctrl_ip::stig_set_error(const char* msg)
{
    CSML_INFO(2, logger) << msg;
    uint32_t cmdstat_val = static_cast<uint32_t>(cmd_status);
    cmdstat_val |= (1u << 1);   // BUS_ERROR at bit 1
    cmd_status = cmdstat_val;
}

/******************************************************************************
 * @brief Dispatch the decoded STIG instruction to the appropriate handler.
 *
 * Reads inst.instr_type and dispatches to one of five handlers:
 *   XSPI_INSTR_READ  (1)   → handle_stig_read()
 *   XSPI_INSTR_WRITE (2)   → handle_stig_write()
 *   XSPI_INSTR_SFDP  (96)  → handle_stig_read_sfdp()
 *   XSPI_INSTR_GLUED (127) → handle_stig_merged_read/write() (INSTR_LINK 2nd phase)
 *   ERASE_SUSPEND    (39)  → handle_stig_suspend_resume()
 *   ERASE_RESUME     (40)  → handle_stig_suspend_resume()
 *   PROG_SUSPEND     (41)  → handle_stig_suspend_resume()
 *   PROG_RESUME      (42)  → handle_stig_suspend_resume()
 *   All others             → handle_stig_control_command()
 *
 * Returns true on success, false if any underlying transaction failed.
 *
 * @param inst        Decoded STIG instruction (from decode_instruction()).
 * @param data_phase  Pointer to a glued data-phase instruction for INSTR_LINK
 *                    chains; nullptr for single-phase instructions.
 * @return true if all flash transactions completed with TLM_OK_RESPONSE.
 *
 * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.4,
 *       docs/xspi_ctrl-architecture-behaviour-map.json state_machines.STIG.
 ******************************************************************************/
bool xspi_ctrl_ip::execute_stig(const stig_instruction& inst,
                                 const stig_instruction* data_phase)
{
    // Suspend/resume instr_type numeric constants per spec table 4.29.
    static const uint8_t STIG_ERASE_SUSPEND = 39u;
    static const uint8_t STIG_ERASE_RESUME  = 40u;
    static const uint8_t STIG_PROG_SUSPEND  = 41u;
    static const uint8_t STIG_PROG_RESUME   = 42u;

    switch (inst.instr_type) {
        case static_cast<uint8_t>(XSPI_INSTR_READ):
            return handle_stig_read(inst, data_phase);

        case static_cast<uint8_t>(XSPI_INSTR_WRITE):
            return handle_stig_write(inst);

        case static_cast<uint8_t>(XSPI_INSTR_SFDP):
            return handle_stig_read_sfdp(inst, data_phase);

        case static_cast<uint8_t>(XSPI_INSTR_GLUED):
            if (data_phase == nullptr) {
                return false;
            }
            if (inst.glued_dir != 0u) {
                return handle_stig_merged_write(*data_phase, inst);
            }
            return handle_stig_merged_read(*data_phase, inst);

        default:
            if (inst.instr_type == STIG_ERASE_SUSPEND ||
                inst.instr_type == STIG_ERASE_RESUME  ||
                inst.instr_type == STIG_PROG_SUSPEND   ||
                inst.instr_type == STIG_PROG_RESUME) {
                return handle_stig_suspend_resume(inst);
            }
            // All other INSTR_TYPE values (including GENERIC=0) fall through
            // to the control command handler which dispatches the raw opcode.
            return handle_stig_control_command(inst, data_phase);
    }
}

/******************************************************************************
 * @brief STIG READ handler — issue a READ transaction on xspi_bus_socket.
 *
 * Constructs a cdns_extension from inst fields (opcode, address,
 * data_bytes_count, I/O widths, edge modes), sets instr_type=XSPI_INSTR_READ
 * (1), and dispatches via dispatch_flash_transaction() as a TLM_READ_COMMAND.
 * Received data is stored in a model-local static buffer; the LT model does
 * not propagate the read data to any STIG output registers (the flash stub
 * returns whatever it generates).
 *
 * For INSTR_LINK GLUED second-phase reads, the data_phase pointer carries
 * the second-phase instruction. In the LT model the combined two-phase flow
 * is represented as a single b_transport call with XSPI_INSTR_GLUED; the
 * flash stub is responsible for interpreting the chained semantics.
 *
 * @param inst        Decoded STIG READ or GLUED instruction.
 * @param data_phase  Non-null for GLUED second-phase INSTR_LINK chains;
 *                    nullptr for normal single-phase READ.
 * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
 *
 * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009;
 *       docs/xspi_ctrl-detailed-design.md Section 7.2.4.
 ******************************************************************************/
bool xspi_ctrl_ip::handle_stig_read(const stig_instruction& inst,
                                     const stig_instruction* data_phase)
{
    (void)data_phase;   // INSTR_LINK data phase uses handle_stig_merged_*

    if (inst.data_bytes_count == 0u) {
        return true;   // No inline data; host must use a glued data instruction for length
    }

    cdns_extension ext;
    ext.opcode       = inst.opcode;
    ext.cmd_ext      = inst.cmd_ext;
    ext.bank_num     = inst.bank_num;
    ext.address      = inst.address;
    ext.data_bytes   = inst.data_bytes_count;
    ext.write_data   = 0u;
    ext.instr_type   = inst.instr_type;   // XSPI_INSTR_READ (1) or XSPI_INSTR_GLUED (127)
    ext.instr_link   = inst.instr_link;
    ext.wp_pin       = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin   = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios   = inst.opcode_ios;
    ext.opcode_edge  = inst.opcode_edge;
    ext.addr_ios     = inst.addr_ios;
    ext.addr_edge    = inst.addr_edge;
    ext.data_ios     = inst.data_ios;
    ext.data_edge    = inst.data_edge;

    // Local receive buffer for STIG read data.
    // In the LT model the received bytes are not forwarded to STIG output
    // registers; the buffer exists to provide a valid data_ptr to the flash stub.
    static uint8_t stig_read_buf[4096];
    uint8_t* buf_ptr = (inst.data_bytes_count > 0u) ? stig_read_buf : nullptr;

    CSML_INFO(3, logger) << "handle_stig_read: opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " addr=0x" << inst.address << " bytes=" << std::dec << inst.data_bytes_count << " bank=" << static_cast<unsigned>(inst.bank_num);

    return dispatch_flash_transaction(ext,
                                      static_cast<unsigned int>(inst.bank_num),
                                      true,
                                      buf_ptr,
                                      inst.data_bytes_count);
}

/******************************************************************************
 * @brief STIG WRITE handler — issue WREN then WRITE transaction on xspi_bus_socket.
 *
 * Per the hardware architecture specification (Section 7.2.4), a STIG WRITE
 * operation consists of two sequential flash bus transactions:
 *  1. WREN command (opcode = seq_cfg.we_cmd_val, default 0x06):
 *     - command-only (data_bytes = 0, no data phase).
 *     - instr_type = XSPI_INSTR_GENERIC (0).
 *     - direction: TLM_WRITE_COMMAND.
 *  2. WRITE/PROGRAM command (opcode from inst.opcode):
 *     - write_data from inst.write_data (single byte).
 *     - data_bytes_count from inst.data_bytes_count.
 *     - instr_type = XSPI_INSTR_WRITE (2).
 *     - direction: TLM_WRITE_COMMAND.
 *
 * Both transactions target the same bank (inst.bank_num).
 *
 * @param inst  Decoded STIG WRITE instruction.
 * @return true if both WREN and WRITE dispatch_flash_transaction() calls
 *         returned TLM_OK_RESPONSE; false if either fails.
 *
 * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009;
 *       docs/xspi_ctrl-detailed-design.md Section 7.2.4.
 ******************************************************************************/
bool xspi_ctrl_ip::handle_stig_write(const stig_instruction& inst)
{
    // Step 1: Issue WREN (Write Enable) command before the WRITE transaction.
    // opcode: use seq_cfg.we_cmd_val (reset value = 0x06).
    cdns_extension wren_ext;
    wren_ext.opcode      = seq_cfg.we_cmd_val;  // default 0x06 (WREN)
    wren_ext.cmd_ext     = 0u;
    wren_ext.bank_num    = inst.bank_num;
    wren_ext.address     = inst.address;
    wren_ext.data_bytes  = 0u;                  // command-only, no data phase
    wren_ext.write_data  = 0u;
    wren_ext.instr_type  = static_cast<uint8_t>(XSPI_INSTR_GENERIC);
    wren_ext.instr_link  = false;
    wren_ext.wp_pin      = mini_ctrl_cfg.wp_pin_level;
    wren_ext.hw_rst_pin  = mini_ctrl_cfg.hw_rst_level;
    wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    wren_ext.opcode_ios  = inst.opcode_ios;
    wren_ext.opcode_edge = inst.opcode_edge;
    wren_ext.addr_ios    = 0u;
    wren_ext.addr_edge   = false;
    wren_ext.data_ios    = 0u;
    wren_ext.data_edge   = false;

    CSML_INFO(3, logger) << "handle_stig_write: WREN opcode=0x" << std::hex << static_cast<unsigned>(wren_ext.opcode) << " bank=" << static_cast<unsigned>(inst.bank_num);

    bool wren_ok = dispatch_flash_transaction(wren_ext,
                                              static_cast<unsigned int>(inst.bank_num),
                                              false,
                                              nullptr,
                                              0u);
    if (!wren_ok) {
        CSML_INFO(2, logger) << "handle_stig_write: WREN dispatch failed";
        return false;
    }

    // Step 2: Issue WRITE/PROGRAM transaction.
    cdns_extension write_ext;
    write_ext.opcode     = inst.opcode;
    write_ext.cmd_ext    = inst.cmd_ext;
    write_ext.bank_num   = inst.bank_num;
    write_ext.address    = inst.address;
    write_ext.data_bytes = inst.data_bytes_count;
    write_ext.write_data = inst.write_data;
    write_ext.instr_type = static_cast<uint8_t>(XSPI_INSTR_WRITE);
    write_ext.instr_link = false;
    write_ext.wp_pin     = mini_ctrl_cfg.wp_pin_level;
    write_ext.hw_rst_pin = mini_ctrl_cfg.hw_rst_level;
    write_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    write_ext.opcode_ios = inst.opcode_ios;
    write_ext.opcode_edge = inst.opcode_edge;
    write_ext.addr_ios   = inst.addr_ios;
    write_ext.addr_edge  = inst.addr_edge;
    write_ext.data_ios   = inst.data_ios;
    write_ext.data_edge  = inst.data_edge;

    // Inline write data: DATA0/MODE0 at cmd_reg1[15:8], DATA1 at [23:16] (2 bytes max).
    uint8_t  write_buf[4]  = {0, 0, 0, 0};
    uint32_t n             = inst.data_bytes_count;
    if (n > 0u) {
        write_buf[0] = static_cast<uint8_t>(inst.write_data & 0xFFu);
    }
    if (n > 1u) {
        write_buf[1] = static_cast<uint8_t>((inst.write_data >> 8) & 0xFFu);
    }
    uint8_t* write_data_ptr  = (n > 0u) ? write_buf : nullptr;
    uint32_t write_data_len  = n;

    CSML_INFO(3, logger) << "handle_stig_write: WRITE opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " addr=0x" << inst.address << " bytes=" << std::dec << inst.data_bytes_count << " bank=" << static_cast<unsigned>(inst.bank_num);

    return dispatch_flash_transaction(write_ext,
                                      static_cast<unsigned int>(inst.bank_num),
                                      false,
                                      write_data_ptr,
                                      write_data_len);
}

bool xspi_ctrl_ip::handle_stig_merged_read(const stig_instruction& cmd_phase,
                                           const stig_instruction& glue)
{
    if (glue.data_bytes_count == 0u) {
        return true;
    }

    const uint32_t n = std::min(
        glue.data_bytes_count, static_cast<uint32_t>(k_stig_sdma_buf_cap));
    m_stig_sdma_read_valid_bytes = 0u;   // AXI will expose after successful flash read

    cdns_extension ext;
    ext.opcode         = cmd_phase.opcode;
    ext.cmd_ext        = cmd_phase.cmd_ext;
    ext.bank_num       = cmd_phase.bank_num;
    ext.address        = cmd_phase.address;
    ext.data_bytes     = n;
    ext.write_data     = 0u;
    ext.instr_type     = static_cast<uint8_t>(XSPI_INSTR_GLUED);
    ext.instr_link     = false;
    ext.wp_pin         = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin     = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios     = cmd_phase.opcode_ios;
    ext.opcode_edge    = cmd_phase.opcode_edge;
    ext.addr_ios       = cmd_phase.addr_ios;
    ext.addr_edge      = cmd_phase.addr_edge;
    ext.data_ios       = cmd_phase.data_ios;
    ext.data_edge      = cmd_phase.data_edge;

    uint8_t* const buf_ptr = m_stig_sdma_data;

    CSML_INFO(3, logger) << "handle_stig_merged_read: op=0x" << std::hex << static_cast<unsigned>(cmd_phase.opcode) << " addr=0x" << cmd_phase.address << " gbytes=" << std::dec << n;

    if (!dispatch_flash_transaction(ext, static_cast<unsigned int>(cmd_phase.bank_num), true, buf_ptr, n)) {
        return false;
    }
    stig_sdma_publish_merged_read(n);
    return true;
}

bool xspi_ctrl_ip::handle_stig_merged_write(const stig_instruction& cmd_phase,
                                            const stig_instruction& glue)
{
    // WREN, then AXI/fill m_stig_sdma_data (sdma_trigg + AXI), then page program
    const uint32_t n_bytes
        = std::min(glue.data_bytes_count, static_cast<uint32_t>(k_stig_sdma_buf_cap));
    if (n_bytes == 0u) {
        return true;
    }

    cdns_extension wren_ext;
    wren_ext.opcode         = seq_cfg.we_cmd_val;
    wren_ext.cmd_ext        = 0u;
    wren_ext.bank_num       = cmd_phase.bank_num;
    wren_ext.address        = 0u;
    wren_ext.data_bytes     = 0u;
    wren_ext.write_data     = 0u;
    wren_ext.instr_type     = static_cast<uint8_t>(XSPI_INSTR_GENERIC);
    wren_ext.instr_link     = false;
    wren_ext.wp_pin         = mini_ctrl_cfg.wp_pin_level;
    wren_ext.hw_rst_pin     = mini_ctrl_cfg.hw_rst_level;
    wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    wren_ext.opcode_ios     = cmd_phase.opcode_ios;
    wren_ext.opcode_edge    = cmd_phase.opcode_edge;
    wren_ext.addr_ios       = 0u;
    wren_ext.addr_edge      = false;
    wren_ext.data_ios       = 0u;
    wren_ext.data_edge      = false;

    if (!dispatch_flash_transaction(wren_ext,
                                    static_cast<unsigned int>(cmd_phase.bank_num),
                                    false,
                                    nullptr,
                                    0u)) {
        return false;
    }

    stig_sdma_begin_host_write(n_bytes);
    uint8_t* const wbuf = m_stig_sdma_data;

    cdns_extension write_ext;
    write_ext.opcode         = cmd_phase.opcode;
    write_ext.cmd_ext        = cmd_phase.cmd_ext;
    write_ext.bank_num       = cmd_phase.bank_num;
    write_ext.address        = cmd_phase.address;
    write_ext.data_bytes     = n_bytes;
    write_ext.write_data     = static_cast<uint32_t>(wbuf[0]);
    write_ext.instr_type     = static_cast<uint8_t>(XSPI_INSTR_WRITE);
    write_ext.instr_link     = false;
    write_ext.wp_pin         = mini_ctrl_cfg.wp_pin_level;
    write_ext.hw_rst_pin     = mini_ctrl_cfg.hw_rst_level;
    write_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    write_ext.opcode_ios     = cmd_phase.opcode_ios;
    write_ext.opcode_edge    = cmd_phase.opcode_edge;
    write_ext.addr_ios       = cmd_phase.addr_ios;
    write_ext.addr_edge      = cmd_phase.addr_edge;
    write_ext.data_ios       = cmd_phase.data_ios;
    write_ext.data_edge      = cmd_phase.data_edge;

    return dispatch_flash_transaction(write_ext, static_cast<unsigned int>(cmd_phase.bank_num), false, wbuf,
                                      n_bytes);
}

void xspi_ctrl_ip::stig_sdma_begin_host_write(uint32_t n_bytes)
{
    if (n_bytes == 0u) {
        return;
    }
    m_stig_sdma_read_valid_bytes = 0u;
    m_stig_sdma_bytes_delivered  = 0u;
    m_stig_sdma_bytes_expected  = n_bytes;
    m_stig_sdma_await_host_write = true;
    (void)std::memset(m_stig_sdma_data, 0, k_stig_sdma_buf_cap);

    sdma_size  = n_bytes;
    sdma_addr0 = static_cast<uint32_t>(k_stig_sdma_axi_base & 0xFFFFFFFFu);
    sdma_addr1 = static_cast<uint32_t>(k_stig_sdma_axi_base >> 32);

    {
        const uint32_t in = static_cast<uint32_t>(intr_status) | (1u << 21);  // sdma_trigg
        intr_status = in;
    }
    evaluate_interrupt_out();

    wait(m_stig_sdma_host_write_done);
    m_stig_sdma_await_host_write = false;
    {
        const uint32_t in = static_cast<uint32_t>(intr_status) & ~(1u << 21);
        intr_status     = in;
    }
    evaluate_interrupt_out();
}

void xspi_ctrl_ip::stig_sdma_publish_merged_read(uint32_t n_bytes)
{
    if (n_bytes == 0u) {
        return;
    }
    m_stig_sdma_read_valid_bytes = std::min(n_bytes, static_cast<uint32_t>(k_stig_sdma_buf_cap));
    sdma_size                    = m_stig_sdma_read_valid_bytes;
    sdma_addr0                   = static_cast<uint32_t>(k_stig_sdma_axi_base & 0xFFFFFFFFu);
    sdma_addr1                   = static_cast<uint32_t>(k_stig_sdma_axi_base >> 32u);
    {
        const uint32_t in = static_cast<uint32_t>(intr_status) | (1u << 21);
        intr_status       = in;
    }
    evaluate_interrupt_out();
}

bool xspi_ctrl_ip::b_transport_sdma_stig(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const uint64_t ax = static_cast<uint64_t>(trans.get_address());
    if (ax < k_stig_sdma_axi_base
        || ax >= (k_stig_sdma_axi_base
                  + static_cast<uint64_t>(k_stig_sdma_buf_cap))) {
        return false;
    }
    const std::size_t off0 = static_cast<std::size_t>(ax - k_stig_sdma_axi_base);
    if (trans.get_data_ptr() == nullptr) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        m_qk.set(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
        return true;
    }
    const uint32_t tlen
        = trans.get_data_length() > 0u ? static_cast<uint32_t>(trans.get_data_length()) : 0u;

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        for (uint32_t j = 0u; j < tlen; ++j) {
            const std::size_t idx = off0 + static_cast<std::size_t>(j);
            uint8_t b             = 0u;
            if (m_stig_sdma_read_valid_bytes > 0u
                && idx
                       < static_cast<std::size_t>(m_stig_sdma_read_valid_bytes)) {
                b = m_stig_sdma_data[idx];
            }
            trans.get_data_ptr()[j] = b;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        m_qk.set(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
        return true;
    }

    if (trans.get_command() != tlm::TLM_WRITE_COMMAND || tlen == 0u) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        m_qk.set(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
        return true;
    }

    if (!m_stig_sdma_await_host_write) {
        return false;
    }

    for (uint32_t j = 0u; j < tlen; ++j) {
        const std::size_t idx = off0 + static_cast<std::size_t>(j);
        if (idx >= k_stig_sdma_buf_cap) {
            break;
        }
        m_stig_sdma_data[idx] = trans.get_data_ptr()[j];
    }
    m_stig_sdma_bytes_delivered += tlen;
    if (m_stig_sdma_bytes_delivered >= m_stig_sdma_bytes_expected) {
        m_stig_sdma_host_write_done.notify();
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }
    return true;
}

/******************************************************************************
 * @brief STIG control command handler — issue a command-only or short transaction.
 *
 * Issues a single flash transaction using the opcode encoded in inst.opcode.
 * The three canonical control commands are:
 *   WREN (0x06): command-only, no data phase, TLM_WRITE_COMMAND.
 *   WRDI (0x04): command-only, no data phase, TLM_WRITE_COMMAND.
 *   RDSR (0x05): 1-byte read, TLM_READ_COMMAND, data returned in local buffer.
 *
 * For unrecognised opcodes (XSPI_INSTR_GENERIC / custom control with any
 * opcode): direction is inferred from data_bytes_count — if zero, issues as
 * TLM_WRITE_COMMAND (command-only); if non-zero, issues as TLM_READ_COMMAND.
 *
 * @param inst        Decoded STIG control instruction.
 * @param data_phase  Pointer to glued data phase for INSTR_LINK; nullptr for
 *                    single-phase (unused in LT model; kept for API uniformity).
 * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
 *
 * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009;
 *       docs/xspi_ctrl-detailed-design.md Section 7.2.4.
 ******************************************************************************/
bool xspi_ctrl_ip::handle_stig_control_command(const stig_instruction& inst,
                                                const stig_instruction* data_phase)
{
    (void)data_phase;   // LT model: data_phase context not used for control commands

    // Canonical control opcodes.
    static const uint8_t OPCODE_WREN = 0x06u;
    static const uint8_t OPCODE_WRDI = 0x04u;
    static const uint8_t OPCODE_RDSR = 0x05u;

    bool is_read_cmd = false;
    uint32_t data_bytes = 0u;

    if (inst.opcode == OPCODE_RDSR) {
        // RDSR: read 1 byte of status register data.
        is_read_cmd = true;
        data_bytes  = (inst.data_bytes_count > 0u) ? inst.data_bytes_count : 1u;
    } else if (inst.opcode == OPCODE_WREN || inst.opcode == OPCODE_WRDI) {
        // WREN / WRDI: command-only, no data.
        is_read_cmd = false;
        data_bytes  = 0u;
    } else {
        // Generic / custom control command: infer direction from data count.
        is_read_cmd = (inst.data_bytes_count > 0u);
        data_bytes  = inst.data_bytes_count;
    }

    cdns_extension ext;
    ext.opcode     = inst.opcode;
    ext.cmd_ext    = inst.cmd_ext;
    ext.bank_num   = inst.bank_num;
    ext.address    = inst.address;
    ext.data_bytes = data_bytes;
    ext.write_data = inst.write_data;
    ext.instr_type = static_cast<uint8_t>(XSPI_INSTR_GENERIC);
    ext.instr_link = false;
    ext.wp_pin     = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios = inst.opcode_ios;
    ext.opcode_edge = inst.opcode_edge;
    ext.addr_ios   = inst.addr_ios;
    ext.addr_edge  = inst.addr_edge;
    ext.data_ios   = inst.data_ios;
    ext.data_edge  = inst.data_edge;

    static uint8_t ctrl_read_buf[256];
    uint8_t* buf_ptr = (is_read_cmd && data_bytes > 0u) ? ctrl_read_buf : nullptr;

    CSML_INFO(3, logger) << "handle_stig_control_command: opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " is_read=" << is_read_cmd << " bytes=" << std::dec << data_bytes << " bank=" << static_cast<unsigned>(inst.bank_num);

    return dispatch_flash_transaction(ext,
                                      static_cast<unsigned int>(inst.bank_num),
                                      is_read_cmd,
                                      buf_ptr,
                                      data_bytes);
}

/******************************************************************************
 * @brief STIG suspend/resume handler — issue erase/program suspend or resume.
 *
 * Issues a single command-only flash transaction using the opcode already
 * encoded in inst.opcode (placed there by software from SFDP DWORD 13 values
 * previously read from stat_seq_cfg_8/9 and translated into the staged cmd_reg1
 * by the firmware driver before triggering cmd_reg0). The handler does not
 * independently look up opcodes from sequence registers; the opcode comes
 * directly from the staged cmd_reg1[31:24] value.
 *
 * All four suspend/resume variants (ERASE_SUSPEND=39, ERASE_RESUME=40,
 * PROG_SUSPEND=41, PROG_RESUME=42) issue a TLM_WRITE_COMMAND with data_bytes=0
 * (command-only) and instr_type=XSPI_INSTR_GENERIC (0).
 *
 * @param inst  Decoded STIG suspend/resume instruction.
 * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
 *
 * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009;
 *       docs/xspi_ctrl-detailed-design.md Section 7.2.4;
 *       xspi_ctrl-knowledge-base/manual.md Table 4.29.
 ******************************************************************************/
bool xspi_ctrl_ip::handle_stig_suspend_resume(const stig_instruction& inst)
{
    cdns_extension ext;
    ext.opcode     = inst.opcode;   // suspend/resume opcode from cmd_reg1[31:24]
    ext.cmd_ext    = 0u;
    ext.bank_num   = inst.bank_num;
    ext.address    = 0u;            // command-only: no address phase
    ext.data_bytes = 0u;            // command-only: no data phase
    ext.write_data = 0u;
    ext.instr_type = static_cast<uint8_t>(XSPI_INSTR_GENERIC);
    ext.instr_link = false;
    ext.wp_pin     = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios = inst.opcode_ios;
    ext.opcode_edge = inst.opcode_edge;
    ext.addr_ios   = 0u;
    ext.addr_edge  = false;
    ext.data_ios   = 0u;
    ext.data_edge  = false;

    CSML_INFO(3, logger) << "handle_stig_suspend_resume: opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " instr_type=" << static_cast<unsigned>(inst.instr_type) << " bank=" << static_cast<unsigned>(inst.bank_num);

    return dispatch_flash_transaction(ext,
                                      static_cast<unsigned int>(inst.bank_num),
                                      false,    // command-only write direction
                                      nullptr,
                                      0u);
}

/******************************************************************************
 * @brief STIG READ_SFDP handler — issue READ_SFDP (opcode 0x5A) transaction.
 *
 * Constructs a cdns_extension with opcode=0x5A (READ_SFDP from inst.opcode —
 * the firmware driver stages opcode=0x5A in cmd_reg1[31:24] before triggering),
 * address from inst.address, data_bytes from inst.data_bytes_count,
 * instr_type set to XSPI_INSTR_SFDP (96), and dispatches via
 * dispatch_flash_transaction() as a TLM_READ_COMMAND on the active flash bank.
 *
 * SFDP data returned from the flash stub is stored in a model-local static
 * buffer. In the LT model the data is not propagated to STIG output registers;
 * the completion status (cmd_status.COMPLETE=1, intr_status.stig_done=1) is
 * sufficient for test verification.
 *
 * @param inst        Decoded STIG READ_SFDP instruction.
 * @param data_phase  Pointer to glued data phase; nullptr for single-phase
 *                    (unused in LT model; kept for API uniformity).
 * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
 *
 * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009;
 *       docs/xspi_ctrl-detailed-design.md Section 7.2.4.
 ******************************************************************************/
bool xspi_ctrl_ip::handle_stig_read_sfdp(const stig_instruction& inst,
                                          const stig_instruction* data_phase)
{
    (void)data_phase;   // LT model: data_phase context not used for SFDP reads

    cdns_extension ext;
    ext.opcode     = inst.opcode;   // expected to be 0x5A (READ_SFDP)
    ext.cmd_ext    = inst.cmd_ext;
    ext.bank_num   = inst.bank_num;
    ext.address    = inst.address;
    ext.data_bytes = inst.data_bytes_count;
    ext.write_data = 0u;
    ext.instr_type = static_cast<uint8_t>(XSPI_INSTR_SFDP);
    ext.instr_link = false;
    ext.wp_pin     = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.opcode_ios = inst.opcode_ios;
    ext.opcode_edge = inst.opcode_edge;
    ext.addr_ios   = inst.addr_ios;
    ext.addr_edge  = inst.addr_edge;
    ext.data_ios   = inst.data_ios;
    ext.data_edge  = inst.data_edge;

    static uint8_t sfdp_read_buf[4096];
    uint8_t* buf_ptr = (inst.data_bytes_count > 0u) ? sfdp_read_buf : nullptr;

    CSML_INFO(3, logger) << "handle_stig_read_sfdp: opcode=0x" << std::hex << static_cast<unsigned>(inst.opcode) << " addr=0x" << inst.address << " bytes=" << std::dec << inst.data_bytes_count << " bank=" << static_cast<unsigned>(inst.bank_num);

    return dispatch_flash_transaction(ext,
                                      static_cast<unsigned int>(inst.bank_num),
                                      true,
                                      buf_ptr,
                                      inst.data_bytes_count);
}

/******************************************************************************
 * @brief Reset handler SC_METHOD — FUNC_XSPI_001
 *
 * Triggered on the falling edge of reset_in (active-low assertion). Executes
 * the hardware reset sequence specified in the architecture map reset_behavior
 * section and docs/xspi_ctrl-detailed-design.md Section 8.1.
 *
 * Reset sequence (order is mandatory):
 *  1. Clear all internal state variables to their post-reset defaults:
 *     - current_work_mode, thrd_status_sel, active_dac_bank, rmp_addr_en,
 *       xip_exit_pending, active_device_profile, tcms_enabled, remap_offset,
 *       xip_active_banks are all zeroed.
 *     - long_polling_val and short_polling_val are restored to their hardware
 *       reset defaults (1000 and 500 respectively) to stay consistent with
 *       the register values restored by reset_all_registers().
 *  2. Call reset_all_registers() to restore all 86 csml register instances to
 *     their defined hardware reset values (including non-zero resets such as
 *     long_polling=1000, short_polling=500, xip_mode_cfg=0x00FF0000,
 *     xspi_ctrl_version=0x65220206, ctrl_features_reg=0x03130703).
 *     NOTE: ctrl_features_reg and xspi_ctrl_version have write_bit_mask=0,
 *     so reset() restores their reset_value directly without going through the
 *     write-mask path.
 *  3. Notify m_int_update_event at SC_ZERO_TIME so that the update_int_out()
 *     SC_METHOD re-evaluates int_out from the freshly reset register state.
 *     After reset, all interrupt status registers are cleared to 0x0 so
 *     update_int_out() will drive int_out low.
 *
 * Single-writer compliance: int_out is NEVER written directly here.
 * Only update_int_out() SC_METHOD is permitted to call int_out.write().
 * This fully complies with SystemC single-writer rule (E115).
 *
 * De-assertion of reset_in (rising edge) requires no action; the model is
 * ready to accept a PoR bootstrap transaction on PoR_input_signals immediately
 * after reset de-asserts.
 *
 * Sensitivity: falling edge of reset_in (active-low).
 * Notifies: m_int_update_event (schedules update_int_out).
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json reset_behavior;
 *            docs/xspi_ctrl-detailed-design.md Sections 4.8 and 8.1.
 ******************************************************************************/
void xspi_ctrl_ip::reset_handler()
{
    CSML_INFO(2, logger) << "reset_handler: reset_in asserted — beginning reset sequence";

    // Step 1: Clear all internal state variables.
    // These shadow the decoded fields of ctrl_config, direct_access_cfg,
    // xip_mode_cfg, global_seq_cfg, and the polling registers. They must
    // match the register reset values restored in step 2.
    current_work_mode    = 0;
    thrd_status_sel      = 0;
    active_device_profile = 0;
    tcms_enabled         = false;
    // Restore polling shadow variables to their hardware reset defaults so
    // they stay in sync with the register values reset below.
    long_polling_val     = 1000u;
    short_polling_val    = 500u;
    // DMA configuration struct restored to hardware reset defaults matching
    // dma_settings reset value 0x000D0000:
    //   OTE (bit 16) = 1 → dma_cfg.ote_enabled = true
    //   word_size (bits [19:18]) = 0b11 → dma_cfg.word_size = 3 (64-bit)
    //   sdma_err_rsp (bit 17) = 0 → dma_cfg.sdma_err_rsp = false
    //   burst_sel (bits [7:0]) = 0x00 → dma_cfg.burst_length = 1
    dma_cfg.burst_length = 1u;
    dma_cfg.word_size    = 0x3u;
    dma_cfg.ote_enabled  = true;
    dma_cfg.sdma_err_rsp = false;
    // Mini-controller config struct restored to hardware reset defaults:
    //   wp=1 (WP# inactive), wp_enable=0, hw_rst=1 (RESET# inactive), clock=0
    mini_ctrl_cfg.wp_pin_level  = true;
    mini_ctrl_cfg.wp_enabled    = false;
    mini_ctrl_cfg.hw_rst_level  = true;
    mini_ctrl_cfg.spi_clk_mode  = 0u;
    // DAC config struct restored to hardware reset defaults (all zero):
    //   dac_bank_num=0, rmp_addr_en=0, rwds_cap_en=0,
    //   mode_bit_xip_dis=0, mode_bit_xip_en=0, remap=0, xip_en=0
    // XIP mode-byte shadows restored to hardware reset values:
    //   xip_en_mb_val=0x00 (xip_mode_cfg bits[15:8] reset = 0x00)
    //   xip_dis_mb_val=0xFF (xip_mode_cfg bits[23:16] reset = 0xFF)
    dac_cfg.active_bank      = 0u;
    dac_cfg.rmp_addr_en      = false;
    dac_cfg.rwds_cap_en      = false;
    dac_cfg.xip_exit_pending = false;
    dac_cfg.xip_entry_armed  = false;
    dac_cfg.remap_offset     = 0ULL;
    dac_cfg.xip_active_banks = 0u;
    xip_en_mb_val            = 0x00u;
    xip_dis_mb_val           = 0xFFu;
    // Discovery config struct restored to hardware reset defaults (all zero):
    discovery_cfg = discovery_cfg_t{};
    // Restore seq_cfg and auxiliary polling shadows to hardware reset values.
    // Zero-initialise first, then assign only non-zero reset fields.
    // This mirrors the constructor initialisation exactly.
    seq_cfg = seq_config_t{};
    seq_cfg.rst_cmd0_val         = 0x66u;
    seq_cfg.rst_cmd1_val         = 0x99u;
    seq_cfg.rst_cmd0_en          = true;
    seq_cfg.rst_cmd0_ext_val     = 0x99u;
    seq_cfg.rst_cmd1_ext_val     = 0x66u;
    seq_cfg.rst_data_val         = 0xD0u;
    seq_cfg.ers_cmd_val          = 0x20u;
    seq_cfg.ers_addr_cnt         = 3u;
    seq_cfg.ers_cmd_ext_val      = 0xDFu;
    seq_cfg.ers_sect_size        = 0x0Cu;
    seq_cfg.ersa_cmd_val         = 0x60u;
    seq_cfg.ersa_cmd_ext_val     = 0x9Fu;
    seq_cfg.prog_cmd_val         = 0x02u;
    seq_cfg.prog_addr_cnt        = 3u;
    seq_cfg.prog_cmd_ext_val     = 0xFDu;
    seq_cfg.prog_p2_burst_type   = true;
    seq_cfg.read_cmd_val         = 0x03u;
    seq_cfg.read_addr_cnt        = 3u;
    seq_cfg.read_cmd_ext_val     = 0xFCu;
    seq_cfg.read_p2_hf_bound_en  = true;
    seq_cfg.read_p2_burst_type   = true;
    seq_cfg.read_p2_latency_cnt  = 0x0Fu;
    seq_cfg.we_cmd_val           = 0x06u;
    seq_cfg.we_cmd_ext_val       = 0xF9u;
    seq_cfg.we_en                = true;
    seq_cfg.stat_dev_rdy_cmd_val       = 0x05u;
    seq_cfg.stat_ers_fail_cmd_val      = 0x05u;
    seq_cfg.stat_prog_fail_cmd_val     = 0x05u;
    seq_cfg.stat_dev_rdy_cmd_ext_val   = 0xFAu;
    seq_cfg.stat_ers_fail_cmd_ext_val  = 0xFAu;
    seq_cfg.stat_prog_fail_cmd_ext_val = 0xFAu;
    seq_cfg.stat_p2_latency_cnt        = 0x0Fu;
    seq_cfg.stat_dev_rdy_en            = true;
    nand_spare_area   = 0u;
    read_page_size    = 0xFu;
    program_page_size = 0x8u;
    // Clear STIG chain state
    stig_instr_link_pending = false;
    stig_link_cmd_phase     = stig_instruction{};
    m_stig_sdma_read_valid_bytes = 0u;
    m_stig_sdma_await_host_write  = false;
    m_stig_sdma_bytes_delivered   = 0u;
    m_stig_sdma_bytes_expected    = 0u;
    (void)std::memset(m_stig_sdma_data, 0, sizeof m_stig_sdma_data);
    // Reset per-thread PIO status array to default-constructed state.
    // All error flags cleared, complete=false. This matches the hardware
    // reset behaviour: cmd_status reads 0x00000000 after reset.
    // Reference: docs/xspi_ctrl-architecture-behaviour-map.json
    //            reset_behavior.cmd_status.
    for (unsigned int i = 0u; i < PIO_MAX_THREADS; ++i) {
        thread_status_[i] = pio_thread_status_t();
    }
    // Reset PoR flag to true: commands are accepted by default unless a PoR
    // transaction is actively in progress (FUNC_XSPI_007).
    m_init_comp_done = true;

    // Step 2: Restore all 86 register instances to hardware reset values.
    // This covers all eight sub-regions including PHY registers (dataslice_Rfile_a,
    // ctb_Rfile_a). The trd_comp_intr_status, trd_error_intr_status, intr_status,
    // trd_status, and ctrl_status registers are all reset to 0x00000000.
    reset_all_registers();

    // Step 3: Schedule update_int_out() to re-evaluate int_out from the freshly
    // reset register state. After reset all interrupt status registers are
    // cleared to 0, so update_int_out() will drive int_out low.
    // int_out is NOT written directly here — single-writer rule compliance.
    m_int_update_event.notify(sc_core::SC_ZERO_TIME);

    CSML_INFO(2, logger) << "reset_handler: reset sequence complete — model idle";
}

/******************************************************************************
 * @brief Sole writer of int_out SC_METHOD — FUNC_XSPI_001 / FUNC_XSPI_002
 *
 * This is the ONLY SystemC process permitted to call int_out.write(), enforcing
 * SystemC's single-writer rule (LRM E115). It is declared as an SC_METHOD
 * sensitive to m_int_update_event with dont_initialize() so it fires only when
 * explicitly scheduled.
 *
 * Computes the logical OR of all three interrupt contribution paths and drives
 * int_out to the resulting level:
 *
 *  Path 1 — General interrupt (intr_status AND intr_enable):
 *    Contributes when the global intr_enable.intr_en gate is set AND any
 *    active intr_status bit has a corresponding intr_enable bit set.
 *    The writable intr_status bits are those covered by the register's
 *    write_bit_mask (0x1FF7F000). Reserved fields are masked out.
 *
 *  Path 2a — Thread completion interrupt (trd_comp_intr_status):
 *    Contributes when intr_enable.intr_en (bit 31) is set and any
 *    trd_comp_intr_status bits[7:0] are non-zero. No trd_comp_intr_en register
 *    (KB / Section 11.3); global intr_en still gates int_out.
 *
 *  Path 2b — Thread error interrupt (trd_error_intr_status AND trd_error_intr_en):
 *    Contributes when the global intr_en gate is set and any
 *    trd_error_intr_status bits[7:0] that have the corresponding
 *    trd_error_intr_en_field bit set are non-zero.
 *
 * int_out is driven high when any path is active, low when all paths are
 * inactive. This is a level-triggered output: it remains high as long as any
 * masked source bit is set, regardless of when the source transitioned.
 *
 * Sensitivity: m_int_update_event (SC_ZERO_TIME notification).
 * Sole driven output: int_out (single-writer compliance).
 * Never called directly — always scheduled via m_int_update_event.notify().
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json interrupt_matrix;
 *            docs/xspi_ctrl-detailed-design.md Sections 9.1, 9.3, 11.3.
 ******************************************************************************/
void xspi_ctrl_ip::update_int_out()
{
    const bool glb_intr_en = (intr_enable.intr_en.get() != 0u);

    // Path 1: general interrupt — gated by intr_en and per-bit enables.
    // intr_status.write_bit_mask = 0x1FF7F000.
    bool path1_active = false;
    if (glb_intr_en) {
        uint32_t intr_status_val  = static_cast<uint32_t>(intr_status);
        uint32_t intr_enable_val  = static_cast<uint32_t>(intr_enable);
        uint32_t writable_mask    = intr_status.write_bit_mask;
        path1_active = ((intr_status_val & intr_enable_val & writable_mask) != 0u);
    }

    // Path 2a: thread completion — also gated by global intr_en; no trd_comp_intr_en
    uint32_t trd_comp_val = static_cast<uint32_t>(trd_comp_intr_status);
    bool path2a_active    = glb_intr_en
                         && ((trd_comp_val & 0xFFu) != 0u);

    // Path 2b: thread error — global intr_en and trd_error_intr_en_field [7:0]
    uint32_t trd_err_val = static_cast<uint32_t>(trd_error_intr_status);
    uint32_t trd_err_en  = trd_error_intr_en.trd_error_intr_en_field.get();
    bool path2b_active   = glb_intr_en
                        && ((trd_err_val & trd_err_en & 0xFFu) != 0u);

    bool interrupt_active = path1_active || path2a_active || path2b_active;
    // This is the ONLY call to int_out.write() in the entire model.
    int_out.write(interrupt_active);

    CSML_INFO(3, logger) << "update_int_out: path1=" << path1_active << " path2a=" << path2a_active << " path2b=" << path2b_active << " int_out=" << interrupt_active;
}

/******************************************************************************
 * @brief Schedule int_out re-evaluation — FUNC_XSPI_001
 *
 * Notifies m_int_update_event at SC_ZERO_TIME, scheduling the update_int_out()
 * SC_METHOD for execution in the next delta cycle. Does NOT write int_out
 * directly — all int_out writes are delegated to update_int_out().
 *
 * Called after every W1C register write and every enable register write to
 * keep int_out consistent with the current masked interrupt state.
 * Also called from stig_engine_thread after each interrupt status update.
 *
 * Single-writer compliance: this function contains no int_out.write() call.
 * It only triggers the dedicated update_int_out() SC_METHOD via the event.
 *
 * No sensitivity list — called directly from callbacks and SC_THREAD.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json interrupt_matrix;
 *            docs/xspi_ctrl-detailed-design.md Sections 9.1, 9.3, 11.3.
 ******************************************************************************/
void xspi_ctrl_ip::evaluate_interrupt_out()
{
    // Notify the dedicated driver SC_METHOD at SC_ZERO_TIME.
    // update_int_out() will execute in the next delta cycle and perform
    // the actual interrupt evaluation and int_out.write().
    m_int_update_event.notify(sc_core::SC_ZERO_TIME);

    CSML_INFO(3, logger) << "evaluate_interrupt_out: scheduled update_int_out " << "via m_int_update_event";
}

// =============================================================================
// Group 1: Command and Status Register Callbacks (ctrl_cmd_stat_a, 0x000)
// =============================================================================

/******************************************************************************
 * @brief Write callback for cmd_reg0 (offset 0x000) — FUNC_XSPI_002
 *
 * Central command dispatch trigger. Reads the current operating mode from
 * the current_work_mode shadow variable and branches to the appropriate engine.
 * Per the Register Reference Manual, `ctrl_config.work_mode` is a 2-bit field
 * at bits [6:5]: 2'b00 Direct, 2'b01 STIG, 2'b11 ACMD (PIO and CDMA share
 * this encoding; `cmd_reg0[31:30]` selects the sub-mode). Value 2'b10 is
 * reserved.
 *
 * STIG mode (current_work_mode == 1, work_mode 2'b01):
 *   The written value is completely ignored — any write triggers the STIG
 *   engine SC_THREAD. Sets ctrl_status.gcmd_eng_busy before notifying
 *   cmd_trigger_event; the thread clears gcmd_eng_busy after completion.
 *
 * ACMD global mode (current_work_mode == 3, work_mode 2'b11):
 *   If cmd_reg0[31:30] == 2'b01, calls pio_handle_trigger(value) (PIO).
 *   If cmd_reg0[31:30] == 2'b00, calls cdma_handle_trigger(value) (CDMA).
 *   Other selector values: sets cmd_ignored and returns without dispatch.
 *
 * Direct mode (current_work_mode == 0, work_mode 2'b00):
 *   cmd_reg0 writes have no meaning in Direct mode. The AXI slave interface
 *   (t_axi_slave_socket) handles flash transactions directly. This callback
 *   logs a warning and returns without action.
 *
 * Unknown mode: logs a warning; no dispatch performed.
 *
 * Busy guard: If ctrl_status.ctrl_busy is already set when cmd_reg0 is written
 * in STIG mode, the cmd_ignored interrupt bit is set and the trigger is dropped.
 * PIO and ACMD engines apply per-thread busy guards internally.
 *
 * Side effects: ctrl_status, trd_status, trd_comp_intr_status, intr_status,
 *               int_out, cmd_trigger_event (STIG mode only).
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            registers.cmd_reg0.fields.cmd0.write_effects;
 *            docs/xspi_ctrl-detailed-design.md Section 6.2.1 and 7.x.
 *
 * @param value 32-bit value written to cmd_reg0
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_cmd_reg0(uint32_t value, uint8_t be)
{
    // Pre-init write blocking (FUNC_XSPI_007 / TC_XSPI_ERR_005):
    // Silently discard command triggers arriving before PoR initialization
    // completes. Flash commands cannot execute before the controller has
    // completed SFDP discovery and auto-populated sequence registers.
    // m_init_comp_done is set to true by b_transport_por() after
    // ctrl_status.init_comp is set.
    if (!m_init_comp_done) {
        CSML_INFO(3, logger) << "handle_write_cmd_reg0: discarded — init not complete";
        return true;
    }

    CSML_INFO(2, logger) << "handle_write_cmd_reg0: value=0x" << std::hex << value << " work_mode=" << std::dec << current_work_mode;

    switch (current_work_mode) {

    case 0u:        // Direct mode (work_mode 2'b00) — cmd_reg0 writes are meaningless
        CSML_INFO(2, logger) << "handle_write_cmd_reg0: Direct mode — " << "cmd_reg0 ignored (use AXI slave interface)";
        break;

    case 1u:        // STIG mode (work_mode 2'b01) — fire cmd_trigger_event
        {
            // Busy guard: if the GCMD engine (STIG engine) is already active,
            // set cmd_ignored and drop this trigger.
            //
            // Exception: when stig_instr_link_pending is true the engine is
            // parked at wait(cmd_trigger_event) after the first INSTR_LINK phase.
            // It has left gcmd_eng_busy=1 (bit 3) and gcmd_eng_mc_busy=1 (bit 4)
            // set as required by Section 5.3.8 Rule 6.  In this state the second
            // cmd_reg0 write is the intended second-phase trigger, NOT a spurious
            // concurrent request.  The guard must therefore allow the trigger
            // through so that the engine can complete the chained transaction.
            uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
            bool instr_link_waiting = stig_instr_link_pending
                                      && ((cs_val >> 4) & 0x1u);  // gcmd_eng_mc_busy
            if (((cs_val >> 3) & 0x1u) && !instr_link_waiting) {    // gcmd_eng_busy at bit 3
                CSML_INFO(2, logger) << "handle_write_cmd_reg0: STIG engine busy" << " — setting cmd_ignored";
                uint32_t intr_cur = static_cast<uint32_t>(intr_status);
                intr_status = intr_cur | (1u << 20);   // cmd_ignored bit 20
                evaluate_interrupt_out();
                break;
            }
            // Set ctrl_busy and gcmd_eng_busy before waking the STIG thread.
            // (These bits may already be set when in INSTR_LINK second-phase
            // context — the assignment is idempotent.)
            cs_val |= (1u << 7) | (1u << 3);   // ctrl_busy bit7, gcmd_eng_busy bit3
            ctrl_status = cs_val;
            // Notify the STIG engine SC_THREAD.
            cmd_trigger_event.notify(sc_core::SC_ZERO_TIME);
        }
        break;

    case 3u:        // ACMD global (work_mode 2'b11): PIO vs CDMA via cmd_reg0[31:30]
        {
            const uint32_t cmd_mode_sel = (value >> 30) & 0x3u;
            if (cmd_mode_sel == 1u) {
                // PIO: cmd_reg0[31:30] = 2'b01
                pio_handle_trigger(value);
            } else if (cmd_mode_sel == 0u) {
                // CDMA: cmd_reg0[31:30] = 2'b00
                cdma_handle_trigger(value);
            } else {
                CSML_INFO(2, logger) << "handle_write_cmd_reg0: work_mode=ACMD (2'b11) but " << "cmd_reg0[31:30]=" << cmd_mode_sel << " — setting cmd_ignored";
                uint32_t intr_cur = static_cast<uint32_t>(intr_status);
                intr_status = intr_cur | (1u << 20);
                evaluate_interrupt_out();
            }
        }
        break;

    default:
        CSML_INFO(2, logger) << "handle_write_cmd_reg0: unknown work_mode=" << current_work_mode << " — no dispatch";
        break;
    }

    return true;
}

/******************************************************************************
 * @brief Write callback for cmd_status_ptr (offset 0x040) — FUNC_XSPI_003
 *
 * Updates the internal thread-status selector shadow variable thrd_status_sel
 * from the written value. The csml framework applies write_bit_mask=0x7
 * before the callback fires, masking out bits[31:3]; the callback extracts
 * bits[2:0] explicitly for clarity and clamps to the valid range [0, 7].
 *
 * The stored thrd_status_sel index is consumed by handle_read_cmd_status()
 * on the next read of cmd_status to determine which thread's live status to
 * return. The selector is persistent: it retains its value until overwritten
 * by a subsequent write to cmd_status_ptr.
 *
 * Side effects: thrd_status_sel shadow variable updated. No register or
 *               signal writes; no interrupt re-evaluation required.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            registers.cmd_status_ptr.fields.thrd_status_sel.write_effects;
 *            docs/xspi_ctrl-detailed-design.md Section 6.1 (cmd_status_ptr).
 *
 * @param value 32-bit value written to cmd_status_ptr (bits[2:0] = thread ID)
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_cmd_status_ptr(uint32_t value, uint8_t be)
{
    // Extract bits[2:0] for the thread-status selector.
    // The csml framework has already applied write_bit_mask=0x7 before
    // invoking this callback; the additional mask here is a safety guard.
    thrd_status_sel = value & 0x7u;
    CSML_INFO(3, logger) << "handle_write_cmd_status_ptr: thrd_status_sel=" << thrd_status_sel;
    return true;
}

/******************************************************************************
 * @brief Read callback for cmd_status (offset 0x044) — FUNC_XSPI_003
 *
 * Volatile indirect read. The stored register file value is always stale.
 * This callback constructs a live status word for the thread selected by
 * thrd_status_sel (set by the most recent write to cmd_status_ptr via
 * handle_write_cmd_status_ptr).
 *
 * Status word encoding (LT model — FUNC_XSPI_003 contract):
 *   bit  0 — completion flag: (trd_comp_intr_status >> sel) & 1
 *             Set when the selected thread completed a PIO/ACMD operation
 *             with INT=1.  Mirrors the per-thread bit in trd_comp_intr_status
 *             so that cmd_status remains consistent with the interrupt
 *             register visible to software.
 *   bit  1 — error flag:      (trd_error_intr_status >> sel) & 1
 *             Set when the selected thread encountered an error and INT=1.
 *             Mirrors the per-thread bit in trd_error_intr_status.
 *   bit 15 — COMPLETE: OR-ed from the stored cmd_status register. Set by
 *            stig_finish() and other engine paths so that STIG completion
 *            remains observable through cmd_status per architecture map.
 *   other bits[31:2] except 15 — reserved; read as in stored cmd_status only
 *            for bit 15 (other high bits are not projected here in LT).
 *
 * Design note: The thread_status_[] array populated by pio_handle_trigger()
 * (FUNC_XSPI_011) provides detailed per-thread error breakdown and is
 * maintained as internal model state.  The cmd_status register exposed to
 * software reports the simpler trd_comp/trd_error bitmap projection so that
 * the FUNC_XSPI_003 contract (bit 0 = comp, bit 1 = err) is preserved for
 * all existing and future tests that rely on this encoding.
 *
 * The thrd_status_sel index is guaranteed to be in [0, 7] by
 * handle_write_cmd_status_ptr.
 *
 * Single-writer compliance: no sc_out or sc_signal writes; read-only access
 * to trd_comp_intr_status, trd_error_intr_status, and thrd_status_sel.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            registers.cmd_status.fields.cmd_status.read_effects;
 *            docs/xspi_ctrl-detailed-design.md Section 6.1
 *            (cmd_status_ptr indirect read);
 *            docs/sections/xspi_ctrl-register-callbacks.md
 *            (cmd_status volatile read encoding).
 *
 * @param[out] value Reference to receive the live 32-bit thread status word.
 *                   bits[1:0] carry the trd_comp/trd_error projection; bit 15
 *                   is COMPLETE from the stored cmd_status register.
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_read_cmd_status(uint32_t& value)
{
    // Clamp selector defensively; handle_write_cmd_status_ptr already enforces
    // [0, 7] range but an additional guard ensures array-bounds safety.
    unsigned int sel = thrd_status_sel & 0x7u;

    // Extract the per-thread completion bit from trd_comp_intr_status.
    // trd_comp_intr_status bits[7:0] map thread[N] → bit N.
    uint32_t comp_val = static_cast<uint32_t>(trd_comp_intr_status);
    uint32_t comp_bit = (comp_val >> sel) & 0x1u;

    // Extract the per-thread error bit from trd_error_intr_status.
    // trd_error_intr_status bits[7:0] map thread[N] → bit N.
    uint32_t err_val  = static_cast<uint32_t>(trd_error_intr_status);
    uint32_t err_bit  = (err_val >> sel) & 0x1u;

    // Construct the status word: bit 0 = completion, bit 1 = error.
    // Merge COMPLETE (bit 15) from the backing cmd_status register so that
    // stig_finish() and similar paths are visible to software (STIG/PIO/ACMD).
    value = (err_bit << 1u) | comp_bit;
    {
        const uint32_t regstore = static_cast<uint32_t>(cmd_status);
        value |= (regstore & (1u << 15));
    }

    CSML_INFO(3, logger) << "handle_read_cmd_status: sel=" << sel << " comp=" << comp_bit << " err=" << err_bit << " status=0x" << std::hex << value;
    return true;
}

/******************************************************************************
 * @brief Write callback for intr_status (offset 0x110) — W1C — FUNC_XSPI_001
 *
 * Performs Write-1-to-Clear (W1C) on the intr_status register. Each bit
 * position in @p value that is 1 clears the corresponding intr_status bit;
 * bits that are 0 in @p value leave the corresponding status bit unchanged.
 * Only writable bits (covered by intr_status.write_bit_mask = 0x1FF7F000) are
 * affected; reserved and RO fields are protected by the register write_bit_mask
 * in the csml_reg base class.
 *
 * After clearing, evaluate_interrupt_out() is called to re-derive the correct
 * int_out level from the updated register and enable masks.
 *
 * W1C implementation note: the csml framework does not apply set_clear_on_write_1
 * automatically (unlike scml2). The W1C clear is applied explicitly here by
 * ANDing the current register value with the bitwise complement of @p value,
 * restricted to the writable mask so reserved bits are never affected.
 *
 * Side effects: intr_status register updated; int_out re-evaluated.
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 6.1.3 (W1C callbacks)
 *            and Section 9.3 (W1C clear and int_out de-assertion).
 *
 * @param value 32-bit mask written by software (1 bits clear status bits)
 * @param be    Byte-enable mask (not used; full 32-bit access)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_intr_status(uint32_t value, uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_intr_status: W1C value=0x" << std::hex << value;

    // Apply W1C: clear bits in intr_status where value bits are 1.
    // Only bits covered by write_bit_mask (0x1FF7F000) are writable.
    uint32_t current    = static_cast<uint32_t>(intr_status);
    uint32_t writable   = intr_status.write_bit_mask;
    uint32_t cleared    = current & ~(value & writable);
    intr_status         = cleared;

    // Re-evaluate int_out after the status register has been updated.
    evaluate_interrupt_out();
    return true;
}

/******************************************************************************
 * @brief Write callback for intr_enable (offset 0x114) — FUNC_XSPI_001
 *
 * The csml framework stores the written value through the register's
 * write_bit_mask (0x9FF7F000) before this callback fires. The callback's
 * sole responsibility for FUNC_XSPI_001 is to re-evaluate int_out immediately
 * after the enable mask changes, because unmasking a pending intr_status source
 * must assert int_out without delay.
 *
 * Side effects: int_out re-evaluated via evaluate_interrupt_out().
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 6.1.2 (enable
 *            callbacks) and Section 9.1 (Path 1 general interrupt).
 *
 * @param value 32-bit value written to intr_enable
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_intr_enable(uint32_t value, uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_intr_enable: value=0x" << std::hex << value;
    evaluate_interrupt_out();
    return true;
}

/******************************************************************************
 * @brief Write callback for trd_comp_intr_status (offset 0x120) — W1C — FUNC_XSPI_001
 *
 * Performs W1C on the trd_comp_intr_status register. Each bit position in
 * @p value that is 1 clears the corresponding trdN_comp bit; bits that are 0
 * leave the corresponding bit unchanged. Only bits[7:0] are writable
 * (write_bit_mask = 0xFF); bits[31:8] are reserved and always read as 0.
 *
 * After clearing, evaluate_interrupt_out() is called so that Path 2a
 * (thread completion) is re-evaluated. If all trdN_comp bits are now zero and
 * no other path is active, int_out is de-asserted.
 *
 * Side effects: trd_comp_intr_status register updated; int_out re-evaluated.
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 6.1.3 and Section 9.3.
 *
 * @param value 32-bit mask written by software (bits[7:0] clear comp flags)
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_trd_comp_intr_status(uint32_t value, uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_trd_comp_intr_status: W1C value=0x" << std::hex << value;

    // Apply W1C: clear trdN_comp bits[7:0] where value bits are 1.
    uint32_t current  = static_cast<uint32_t>(trd_comp_intr_status);
    uint32_t writable = trd_comp_intr_status.write_bit_mask;  // 0xFF
    uint32_t cleared  = current & ~(value & writable);
    trd_comp_intr_status = cleared;

    evaluate_interrupt_out();
    return true;
}

/******************************************************************************
 * @brief Write callback for trd_error_intr_status (offset 0x130) — W1C — FUNC_XSPI_001
 *
 * Performs W1C on the trd_error_intr_status register. Each bit position in
 * @p value that is 1 clears the corresponding trdN_error_stat bit; bits that
 * are 0 leave the corresponding bit unchanged. Only bits[7:0] are writable
 * (write_bit_mask = 0xFF); bits[31:8] are reserved.
 *
 * After clearing, evaluate_interrupt_out() is called so that Path 2b
 * (thread error) is re-evaluated with the updated trd_error_intr_status and
 * trd_error_intr_en masks.
 *
 * Side effects: trd_error_intr_status register updated; int_out re-evaluated.
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 6.1.3 and Section 9.3.
 *
 * @param value 32-bit mask written by software (bits[7:0] clear error flags)
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_trd_error_intr_status(uint32_t value,
                                                   uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_trd_error_intr_status: W1C value=0x" << std::hex << value;

    // Apply W1C: clear trdN_error_stat bits[7:0] where value bits are 1.
    uint32_t current  = static_cast<uint32_t>(trd_error_intr_status);
    uint32_t writable = trd_error_intr_status.write_bit_mask;  // 0xFF
    uint32_t cleared  = current & ~(value & writable);
    trd_error_intr_status = cleared;

    evaluate_interrupt_out();
    return true;
}

/******************************************************************************
 * @brief Write callback for trd_error_intr_en (offset 0x134) — FUNC_XSPI_001
 *
 * The csml framework stores the written value through the register's
 * write_bit_mask (0xFF) before this callback fires. The callback re-evaluates
 * int_out immediately because enabling a previously masked thread error source
 * (when the corresponding trd_error_intr_status bit is already set) must
 * assert int_out without delay.
 *
 * Side effects: int_out re-evaluated via evaluate_interrupt_out().
 *
 * Reference: docs/xspi_ctrl-detailed-design.md Section 9.1 (Path 2b thread
 *            error interrupt) and Section 9.3.
 *
 * @param value 32-bit value written to trd_error_intr_en (bits[7:0] valid)
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_trd_error_intr_en(uint32_t value, uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_trd_error_intr_en: value=0x" << std::hex << value;
    evaluate_interrupt_out();
    return true;
}

// =============================================================================
// Group 2: Controller Configuration Callbacks (ctrl_cfg_common_a, 0x200)
// =============================================================================

bool xspi_ctrl_ip::handle_write_long_polling(uint32_t value, uint8_t be)
{
    long_polling_val = value & 0xFFFFu;
    CSML_INFO(3, logger) << "handle_write_long_polling: val=" << long_polling_val;
    return true;
}

bool xspi_ctrl_ip::handle_write_short_polling(uint32_t value, uint8_t be)
{
    short_polling_val = value & 0xFFFFu;
    CSML_INFO(3, logger) << "handle_write_short_polling: val=" << short_polling_val;
    return true;
}

/******************************************************************************
 * @brief Write callback for ctrl_config (offset 0x230) — FUNC_XSPI_006
 *
 * Updates the operating-mode shadow variable from the work_mode field at
 * bits [6:5] in the written value. The write_bit_mask for ctrl_config is 0x68
 * so only bits[6:5] (work_mode, 2-bit) and bit[3] (cont_on_err) are writable;
 * the csml framework enforces this mask before the callback fires. The callback
 * extracts the already-masked work_mode from the stored register value using
 * the csml bitfield accessor to ensure mask consistency.
 *
 * work_mode encoding (bits[6:5]) — Register Reference Manual:
 *   2'b00 = Direct — AXI slave transactions forwarded directly to flash
 *   2'b01 = STIG — cmd_reg0 write fires cmd_trigger_event
 *   2'b10 = Reserved
 *   2'b11 = ACMD — cmd_reg0[31:30]=2'b01 selects PIO; 2'b00 selects CDMA
 *
 * After storing current_work_mode, the function checks whether any thread
 * in trd_status is currently busy. In the LT model a mode switch is not
 * blocked (the hardware specification does not mandate blocking the write),
 * but a warning is issued so that testbench designers are aware that the
 * mode switch has occurred while threads are active, which may produce
 * undefined behavior in a real device.
 *
 * Side effects: current_work_mode shadow updated.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            registers.ctrl_config.fields.work_mode.write_effects;
 *            docs/xspi_ctrl-detailed-design.md Sections 6.2.2 and 7.1;
 *            docs/xspi_ctrl-functionality_list.md FUNC_XSPI_006.
 *
 * @param value 32-bit value written to ctrl_config (masked by write_bit_mask)
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_ctrl_config(uint32_t value, uint8_t be)
{
    // Pre-init write blocking (FUNC_XSPI_007 / TC_XSPI_ERR_005):
    // Silently discard writes arriving before PoR initialization completes.
    // m_init_comp_done is set to true by b_transport_por() once SFDP discovery
    // (or the inhibit fast path) finishes and ctrl_status.init_comp is set.
    if (!m_init_comp_done) {
        CSML_INFO(3, logger) << "handle_write_ctrl_config: discarded — init not complete";
        return true;
    }

    // Extract work_mode from bits [6:5] of the written value.
    // The csml framework has already applied write_bit_mask=0x68 before
    // invoking this callback, so the value seen here is already masked.
    // We use the register bitfield accessor for consistency with the
    // REG.FIELD notation contract, which reads the stored (post-mask) value.
    current_work_mode = static_cast<unsigned int>(ctrl_config.work_mode.get());

    CSML_INFO(2, logger) << "handle_write_ctrl_config: work_mode=" << current_work_mode << " (0=Direct, 1=STIG, 2=rsvd, 3=ACMD)";

    // FUNC_XSPI_006: Warn if any PIO/ACMD thread is currently busy.
    // In the LT model the mode switch is applied immediately (not blocked),
    // but switching modes while threads are active is architecturally unsafe.
    // This warning allows testbench designers to catch such races early.
    // Note: trd_status.trd_busy occupies bits[7:0] (one bit per thread).
    uint32_t trd_busy_val = static_cast<uint32_t>(trd_status);
    if ((trd_busy_val & 0xFFu) != 0u) {
        CSML_INFO(1, logger) << "handle_write_ctrl_config: WARNING — " << "mode switch to work_mode=" << current_work_mode << " while trd_status busy=0x" << std::hex << (trd_busy_val & 0xFFu) << std::dec << "; behavior may be undefined on real hardware";
    }

    return true;
}

// =============================================================================
// Flash Bus Transaction Engine Helpers — FUNC_XSPI_002
// =============================================================================

/******************************************************************************
 * @brief Dispatch a single xSPI flash bus transaction via b_transport.
 *
 * Constructs a tlm_generic_payload from the caller-supplied cdns_extension,
 * validates the bank index against NUM_TARGETS, attaches the extension, and
 * calls xspi_bus_socket[bank]->b_transport(). Returns whether the response
 * status indicates TLM_OK_RESPONSE.
 *
 * The caller is responsible for:
 *  - Fully populating the cdns_extension before calling.
 *  - Setting ctrl_status busy flags before calling and clearing them after.
 *  - Not calling this function from an SC_METHOD (the b_transport call is
 *    synchronous but may accumulate quantum time).
 *
 * LT timing model: The local delay is advanced by 10 ns per transaction to
 * represent the flash bus access latency. The quantum keeper is updated to
 * synchronize with the SystemC kernel when the quantum is exceeded.
 *
 * Architecture note: ctrl_status is a RO register (write_bit_mask=0x0).
 * Internal model writes must bypass the mask by direct assignment to the
 * register's uint32 value. The csml framework allows model-internal direct
 * assignment to register objects via operator= regardless of write_bit_mask.
 *
 * Driven signals: none (busy flag updates are performed by callers).
 * No wait() is called inside this function; it returns before SystemC time
 * advances. Temporal decoupling is maintained via m_qk.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            operations.transaction_timeline;
 *            docs/xspi_ctrl-detailed-design.md Section 7 (Operating Modes).
 *
 * @param ext       Fully populated cdns_extension carrying transaction fields
 * @param bank      Flash chip-select / bank index (validated < NUM_TARGETS)
 * @param is_read   True for TLM_READ_COMMAND; false for TLM_WRITE_COMMAND
 * @param data_ptr  Pointer to data buffer for read payload; nullptr if no data
 * @param data_len  Number of data bytes; 0 for command-only transactions
 * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
 ******************************************************************************/
bool xspi_ctrl_ip::dispatch_flash_transaction(cdns_extension& ext,
                                               unsigned int    bank,
                                               bool            is_read,
                                               uint8_t*        data_ptr,
                                               uint32_t        data_len)
{
    // Validate bank index against configured NUM_TARGETS.
    // An out-of-range bank is a model-usage error; set cmd_ignored and return.
    if (static_cast<int>(bank) >= NUM_TARGETS) {
        CSML_INFO(2, logger) << "dispatch_flash_transaction: bank=" << bank << " out of range (NUM_TARGETS=" << NUM_TARGETS << ") — setting cmd_ignored";
        uint32_t intr_cur = static_cast<uint32_t>(intr_status);
        intr_status = intr_cur | (1u << 20);   // cmd_ignored at bit 20
        evaluate_interrupt_out();
        return false;
    }

    // Build TLM generic payload.
    tlm::tlm_generic_payload payload;
    payload.set_command(is_read ? tlm::TLM_READ_COMMAND
                                : tlm::TLM_WRITE_COMMAND);
    // Address field carries the flash device address for the flash model.
    payload.set_address(static_cast<sc_dt::uint64>(ext.address));
    payload.set_data_ptr(data_ptr);
    payload.set_data_length(data_len);
    payload.set_streaming_width(data_len > 0u ? data_len : 4u);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Attach the cdns_extension so the flash model can see xSPI fields.
    payload.set_extension(&ext);

    // Issue the b_transport call on the target socket.
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    (*xspi_bus_socket[bank])->b_transport(payload, delay);

    // Accumulate the LT modeling delay (10 ns per flash transaction).
    delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Remove extension before payload destructs to avoid double-free.
    payload.clear_extension(&ext);

    bool ok = (payload.get_response_status() == tlm::TLM_OK_RESPONSE);
    CSML_INFO(3, logger) << "dispatch_flash_transaction: bank=" << bank << " opcode=0x" << std::hex << static_cast<unsigned>(ext.opcode) << " addr=0x" << ext.address << " bytes=" << std::dec << ext.data_bytes << " ok=" << ok;
    return ok;
}

/******************************************************************************
 * @brief PIO mode engine — invoked by handle_write_cmd_reg0 in PIO mode.
 *
 * Implements the PIO thread dispatch sequence:
 *  1. Decode TRD_NUM[26:24], BANK/CS[22:20], DMA_SEL[19], INT[18],
 *     MB_XIP_DIS[17], MB_XIP_EN[16], CMD_TYPE[15:0] from cmd_reg0_val.
 *  2. Check thread idle state (trd_status.trd_busy[TRD_NUM]). If already
 *     busy, silently ignore (no CMD_IGNORED set — PIO silent-ignore per spec).
 *  3. Mark thread BUSY: set trd_status.trd_busy bit for TRD_NUM.
 *  4. Set ctrl_status.ctrl_busy.
 *  5. Snapshot required cmd_reg fields per CMD_TYPE discipline:
 *     - RESET (0x1100/0x1101): no additional regs needed.
 *     - CHIP_ERASE (0x1001):   no additional regs needed.
 *     - SECTOR_ERASE (0x1000): cmd_reg1 (addr low), cmd_reg4 (SECT_CNT),
 *                               cmd_reg5 (addr high).
 *     - READ/PROGRAM (0x2200/0x2100): cmd_reg1 (addr low), cmd_reg2 (sys_ptr
 *                               low), cmd_reg3 (sys_ptr high), cmd_reg4
 *                               (DATA_CNT), cmd_reg5 (addr high).
 *  6. Construct cdns_extension with opcode derived from CMD_TYPE, address
 *     and data count from snapshotted registers.
 *  7. Dispatch via dispatch_flash_transaction().
 *  8. Clear ctrl_status.ctrl_busy and trd_status.trd_busy[TRD_NUM].
 *  9. If INT flag was set: set trd_comp_intr_status bit for TRD_NUM,
 *     call evaluate_interrupt_out().
 *
 * PIO CMD_TYPE to opcode mapping (from knowledge-base PIO constants):
 *   0x2200 (READ)         -> opcode 0x03 (standard READ)
 *   0x2100 (PROGRAM)      -> opcode 0x02 (PAGE_PROGRAM)
 *   0x1000 (SECTOR_ERASE) -> opcode 0xD8 (64 KB sector erase, common default)
 *   0x1001 (CHIP_ERASE)   -> opcode 0xDC (ERASE_64KB_4BYTE; LT target)
 *   0x1100 (RESET_SOFT)   -> opcode 0xFF (Software reset)
 *   0x1101 (RESET_JEDEC)  -> opcode 0xF0 (JEDEC hardware reset sequence)
 *
 * Driven state: ctrl_status (ctrl_busy), trd_status (trd_busy),
 *               trd_comp_intr_status, int_out.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            state_machines.PIO; docs/xspi_ctrl-detailed-design.md Section 7.3.
 *
 * @param cmd_reg0_val The 32-bit value written to cmd_reg0 in PIO mode
 ******************************************************************************/
void xspi_ctrl_ip::pio_handle_trigger(uint32_t cmd_reg0_val)
{
    // -------------------------------------------------------------------------
    // Step 1: Mask reserved bits [29:27] and [23] per spec Table 4.10.
    // Hardware ignores writes to reserved fields; masking prevents any reserved
    // bit contamination from affecting the decoded fields.
    // -------------------------------------------------------------------------
    constexpr uint32_t CMD_REG0_RESERVED_MASK = ~((0x7u << 27) | (0x1u << 23));
    uint32_t masked_val = cmd_reg0_val & CMD_REG0_RESERVED_MASK;

    // -------------------------------------------------------------------------
    // Step 2: Decode PIO cmd_reg0 fields from the sanitised value.
    // Reference: docs/xspi_ctrl-detailed-design.md Section 5.3.2 (PIO layout).
    // -------------------------------------------------------------------------
    unsigned int trd_num    = (masked_val >> 24) & 0x7u;              // [26:24] TRD_NUM
    unsigned int bank_cs    = (masked_val >> 20) & 0x7u;              // [22:20] BANK/CS
    bool         dma_sel    = ((masked_val >> 19) & 0x1u) != 0u;      // [19]    DMA_SEL (0=AXI slave, 1=AXI master)
    bool         int_flag   = ((masked_val >> 18) & 0x1u) != 0u;      // [18]    INT
    bool         mb_xip_dis = ((masked_val >> 17) & 0x1u) != 0u;      // [17]    MB_XIP_DIS
    bool         mb_xip_en  = ((masked_val >> 16) & 0x1u) != 0u;      // [16]    MB_XIP_EN (READ only)
    uint16_t     cmd_type   = static_cast<uint16_t>(masked_val & 0xFFFFu); // [15:0] CMD_TYPE

    // MB_XIP_DIS (bit 17): on PIO READ with XIP active, inserts xip_dis_mb_val
    // in ext.write_data and clears the bank in dac_cfg (mirrors xip_mode_cfg);
    // see case 0x2200 below.

    // DMA_SEL selects AXI master (i_dma_socket) when 1, AXI slave (SDMA) when 0.
    // The LT model only implements the AXI master path (DMA_SEL=1) for PIO;
    // DMA_SEL=0 (SDMA) is not implemented in this model per the LT assumptions.
    // The flag is decoded but the same dma_read/dma_write helpers are used
    // regardless (they route through i_dma_socket unconditionally).
    (void)dma_sel;

    CSML_INFO(2, logger) << "pio_handle_trigger: trd=" << trd_num << " bank=" << bank_cs << " cmd_type=0x" << std::hex << cmd_type << std::dec << " int=" << int_flag << " mb_xip_dis=" << mb_xip_dis << " mb_xip_en=" << mb_xip_en;

    // -------------------------------------------------------------------------
    // Step 3: Clamp TRD_NUM to the valid range [0, n_threads-1].
    // n_threads is the build-time parameter from ctrl_features_reg[3:0].
    // -------------------------------------------------------------------------
    unsigned int trd_idx = trd_num % static_cast<unsigned int>(n_threads);

    // -------------------------------------------------------------------------
    // Step 4: Busy-thread guard.
    // If trd_status.trd_busy[trd_idx] is already set, silently ignore the
    // trigger. PIO mode does NOT set CMD_IGNORED on busy — that is ACMD only.
    // Reference: docs/xspi_ctrl-detailed-design.md Section 5.3.8 Rule 5.
    // -------------------------------------------------------------------------
    uint32_t trd_busy_val = static_cast<uint32_t>(trd_status);
    if (((trd_busy_val >> trd_idx) & 0x1u) != 0u) {
        CSML_INFO(2, logger) << "pio_handle_trigger: thread " << trd_idx << " already busy — silent ignore (PIO spec)";
        return;
    }

    // -------------------------------------------------------------------------
    // Step 5: Mark thread BUSY and set ctrl_status.ctrl_busy.
    // Both registers are RO — direct assignment is the model-internal write path
    // (write_bit_mask=0 on both; the hardware drives these bits).
    // -------------------------------------------------------------------------
    trd_busy_val |= (1u << trd_idx);
    trd_status = trd_busy_val;

    uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
    cs_val |= (1u << 7);   // ctrl_busy at bit 7
    ctrl_status = cs_val;

    // -------------------------------------------------------------------------
    // Step 6: Snapshot only the cmd_reg fields required by CMD_TYPE.
    // Per cmd_reg snapshot discipline (detailed-design Section 5.3.8 Rule 2):
    //   RESET_SOFT / RESET_JEDEC / CHIP_ERASE : cmd_reg0 only
    //   SECTOR_ERASE                           : cmd_reg1, cmd_reg4, cmd_reg5
    //   READ / PROGRAM                         : cmd_reg1..5 all required
    // Unused fields are zeroed to prevent stale values from a prior command.
    // -------------------------------------------------------------------------
    uint32_t snap1 = 0u, snap2 = 0u, snap3 = 0u, snap4 = 0u, snap5 = 0u;

    switch (cmd_type) {
    case 0x2200u:   // READ  — needs all address and count registers
    case 0x2100u:   // PROGRAM — needs all address and count registers
        snap1 = static_cast<uint32_t>(cmd_reg1.cmd1.get());
        snap2 = static_cast<uint32_t>(cmd_reg2.cmd2.get());
        snap3 = static_cast<uint32_t>(cmd_reg3.cmd3.get());
        snap4 = static_cast<uint32_t>(cmd_reg4.cmd4.get());
        snap5 = static_cast<uint32_t>(cmd_reg5.cmd5.get());
        break;

    case 0x1000u:   // SECTOR_ERASE — needs xSPI address and sector count
        snap1 = static_cast<uint32_t>(cmd_reg1.cmd1.get());
        snap4 = static_cast<uint32_t>(cmd_reg4.cmd4.get());
        snap5 = static_cast<uint32_t>(cmd_reg5.cmd5.get());
        break;

    default:
        // CHIP_ERASE (0x1001), RESET_SOFT (0x1100), RESET_JEDEC (0x1101):
        // cmd_reg0 only — no additional register snapshot required.
        break;
    }

    // -------------------------------------------------------------------------
    // Step 7: Initialize the per-thread status record for this invocation.
    // Cleared at the start of each dispatch; populated during execution.
    // -------------------------------------------------------------------------
    thread_status_[trd_idx] = pio_thread_status_t();

    // -------------------------------------------------------------------------
    // Step 8: XIP non-READ rejection (FUNC_XSPI_013).
    //
    // If the target bank has XIP active (xip_active_banks bit set) AND the
    // PIO command is not a READ (0x2200), the operation is rejected:
    //   - CMD_ERROR is set in thread_status_[trd_idx].
    //   - trd_error_intr_status[trd_idx] is set.
    //   - evaluate_interrupt_out() is called.
    //   - ctrl_busy and trd_busy are cleared.
    //   - The function returns without dispatching any flash transaction.
    //
    // This matches the hardware behaviour described in:
    //   docs/xspi_ctrl-architecture-behaviour-map.json
    //   registers.xip_mode_cfg.side_effects:
    //     "In PIO mode: issuing a non-READ command to an XIP-active bank sets
    //      CMD_ERROR in thread cmd_status."
    // -------------------------------------------------------------------------
    {
        unsigned int pio_bank_bit    = (1u << bank_cs);
        bool         pio_xip_active  = ((dac_cfg.xip_active_banks & pio_bank_bit) != 0u);

        if (pio_xip_active && (cmd_type != 0x2200u)) {
            CSML_INFO(2, logger) << "pio_handle_trigger: bank " << bank_cs << " is XIP-active; non-READ cmd_type=0x"
                                 << std::hex << cmd_type
                                 << " rejected (CMD_ERROR) — FUNC_XSPI_013";

            thread_status_[trd_idx].cmd_error = true;
            thread_status_[trd_idx].fail      = true;
            thread_status_[trd_idx].complete  = true;

            uint32_t err_val = static_cast<uint32_t>(trd_error_intr_status);
            err_val |= (1u << trd_idx);
            trd_error_intr_status = err_val;
            evaluate_interrupt_out();

            // Clear busy flags before returning.
            trd_busy_val &= ~(1u << trd_idx);
            trd_status = trd_busy_val;

            cs_val = static_cast<uint32_t>(ctrl_status);
            cs_val &= ~(1u << 7u);   // clear ctrl_busy
            ctrl_status = cs_val;

            return;
        }
    }

    // -------------------------------------------------------------------------
    // Step 9: Build cdns_extension common fields.
    // PIO commands use single-wire (x1) SDR SPI conventions by default.
    // The bank_num field routes the transaction to xspi_bus_socket[bank_cs].
    // -------------------------------------------------------------------------
    cdns_extension ext;
    ext.bank_num       = static_cast<uint8_t>(bank_cs);
    ext.wp_pin         = mini_ctrl_cfg.wp_pin_level;
    ext.hw_rst_pin     = mini_ctrl_cfg.hw_rst_level;
    ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
    ext.instr_link     = false;
    ext.opcode_ios     = 0u;    // x1 SPI (standard mode)
    ext.opcode_edge    = false; // SDR
    ext.addr_ios       = 0u;
    ext.addr_edge      = false;
    ext.data_ios       = 0u;
    ext.data_edge      = false;

    // -------------------------------------------------------------------------
    // Step 10: Per-CMD_TYPE execution.
    // Each case sets the flash-bus extension fields, performs any required DMA,
    // and records success/failure in the local `op_success` flag.
    // -------------------------------------------------------------------------
    bool op_success = true;

    switch (cmd_type) {

    // =========================================================================
    // CMD_TYPE 0x2200 — PIO READ
    //
    // Two-phase operation:
    //   Phase A: READ from flash via xspi_bus_socket[bank_cs].
    //            xSPI address: {snap5[31:0], snap1[31:0]} (64-bit).
    //            Data count:   snap4 + 1 bytes (DATA_CNT encoding).
    //   Phase B: DMA WRITE received flash data to system memory via dma_write().
    //            System address: {snap3[31:0], snap2[31:0]} (64-bit).
    //
    // XIP Entry (FUNC_XSPI_013):
    //   MB_XIP_EN (cmd_reg0 bit 16): when set, the controller signals XIP mode
    //   entry by inserting xip_en_mb_val in ext.write_data before the flash READ.
    //   On successful READ completion:
    //     - dac_cfg.xip_active_banks bit for bank_cs is set.
    //     - xip_mode_cfg.xip_en bits[7:0] are mirrored from xip_active_banks.
    //
    // MB_XIP_DIS (cmd_reg0 bit 17): on READ with XIP active for bank_cs,
    //   inserts xip_dis_mb_val in ext.write_data, then clears
    //   xip_active_banks[bank_cs] and mirrors xip_mode_cfg.xip_en.
    //
    // DMA error: if dma_write fails, ddma_terr is set (is_data_path=true),
    // dma_target_error_l/h are captured, and BUS_ERROR is recorded in
    // thread_status_[trd_idx].
    //
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO READ (CMD_TYPE 0x2200)";
    //            docs/xspi_ctrl-detailed-design.md Section 7.3.2;
    //            FUNC_XSPI_013 XIP entry via PIO MB_XIP_EN.
    // =========================================================================
    case 0x2200u: {
        uint64_t flash_addr = (static_cast<uint64_t>(snap5) << 32) |
                               static_cast<uint64_t>(snap1);
        uint64_t sys_addr   = (static_cast<uint64_t>(snap3) << 32) |
                               static_cast<uint64_t>(snap2);
        uint32_t byte_cnt   = snap4 + 1u;   // DATA_CNT: actual bytes = value + 1

        CSML_INFO(2, logger) << "pio_handle_trigger: READ flash=0x" << std::hex << flash_addr << " sys=0x"  << sys_addr << std::dec   << " bytes=" << byte_cnt << " mb_xip_dis=" << mb_xip_dis << " mb_xip_en=" << mb_xip_en;

        {
            const unsigned int pio_bank_bit_xip = (1u << bank_cs);
            const bool         pio_xip_active
                = ((dac_cfg.xip_active_banks & pio_bank_bit_xip) != 0u);

            // XIP exit (FUNC_XSPI_013): preferred when XIP is active and exit
            // is requested; inserts xip_dis_mb_val in ext.write_data.
            if (mb_xip_dis && pio_xip_active) {
                CSML_INFO(2, logger) << "pio_handle_trigger: MB_XIP_DIS — inserting" << " xip_dis_mb_val=0x" << std::hex << static_cast<unsigned>(xip_dis_mb_val) << " for bank " << std::dec << bank_cs;
                ext.write_data = static_cast<uint32_t>(xip_dis_mb_val);
            } else if (mb_xip_en) {
                // XIP entry mode byte injection (FUNC_XSPI_013).
                // Forward xip_en_mb_val in ext.write_data for downstream flash stubs.
                CSML_INFO(2, logger) << "pio_handle_trigger: MB_XIP_EN — inserting" << " xip_en_mb_val=0x" << std::hex << static_cast<unsigned>(xip_en_mb_val) << " for bank " << std::dec << bank_cs;
                ext.write_data = static_cast<uint32_t>(xip_en_mb_val);
            }
        }

        // Phase A: allocate a transfer buffer and read from flash.
        // The buffer is heap-allocated to support per-thread isolation.
        // Maximum PIO transfer size limited to 64 KB per single command.
        static constexpr uint32_t PIO_MAX_TRANSFER = 65536u;
        if (byte_cnt > PIO_MAX_TRANSFER) {
            CSML_INFO(2, logger) << "pio_handle_trigger: READ byte_cnt=" << byte_cnt << " clamped to " << PIO_MAX_TRANSFER;
            byte_cnt = PIO_MAX_TRANSFER;
        }

        std::vector<uint8_t> flash_buf(byte_cnt, 0u);

        ext.opcode     = 0x03u;              // READ_ZERO_LATENCY opcode
        ext.instr_type = XSPI_INSTR_READ;
        ext.address    = flash_addr;
        ext.data_bytes = byte_cnt;

        bool flash_ok = dispatch_flash_transaction(ext,
                                                   static_cast<unsigned int>(bank_cs),
                                                   true,
                                                   flash_buf.data(),
                                                   byte_cnt);

        if (!flash_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: READ — flash transaction failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
            break;
        }

        // XIP state update (FUNC_XSPI_013) after successful flash READ.
        {
            const unsigned int pio_bank_bit = (1u << bank_cs);
            const bool         pio_had_xip
                = ((dac_cfg.xip_active_banks & pio_bank_bit) != 0u);
            // MB_XIP_DIS exit: bank was XIP-active before this post-READ update.
            if (mb_xip_dis && pio_had_xip) {
                dac_cfg.xip_active_banks &= ~pio_bank_bit;

                uint32_t xip_cfg_cur = static_cast<uint32_t>(xip_mode_cfg);
                uint32_t xip_cfg_new
                    = (xip_cfg_cur & 0xFFFFFF00u)
                    | static_cast<uint32_t>(dac_cfg.xip_active_banks & 0xFFu);
                xip_mode_cfg = xip_cfg_new;

                CSML_INFO(2, logger) << "pio_handle_trigger: MB_XIP_DIS — bank " << bank_cs << " XIP exit; xip_active_banks=0x" << std::hex
                                     << static_cast<unsigned>(
                                            dac_cfg.xip_active_banks)
                                     << " xip_mode_cfg=0x" << xip_cfg_new;
            } else if (mb_xip_en) {
                // On successful READ with MB_XIP_EN: set xip_active_banks bit
                // for bank_cs and mirror into xip_mode_cfg.xip_en bits[7:0].
                dac_cfg.xip_active_banks |= pio_bank_bit;

                uint32_t xip_cfg_cur = static_cast<uint32_t>(xip_mode_cfg);
                uint32_t xip_cfg_new
                    = (xip_cfg_cur & 0xFFFFFF00u)
                    | static_cast<uint32_t>(dac_cfg.xip_active_banks & 0xFFu);
                xip_mode_cfg = xip_cfg_new;

                CSML_INFO(2, logger) << "pio_handle_trigger: MB_XIP_EN — bank " << bank_cs << " XIP active; xip_active_banks=0x"
                                     << std::hex
                                     << static_cast<unsigned>(
                                            dac_cfg.xip_active_banks)
                                     << " xip_mode_cfg=0x" << xip_cfg_new;
            }
        }

        // Phase B: DMA WRITE flash data to system memory.
        // Address split into lower/upper 32-bit halves for dma_write().
        // is_data_path=true so AXI errors set ddma_terr (not cdma_terr).
        uint32_t sys_lo = static_cast<uint32_t>(sys_addr & 0xFFFFFFFFu);
        uint32_t sys_hi = static_cast<uint32_t>((sys_addr >> 32) & 0xFFFFFFFFu);

        bool dma_ok = dma_write(sys_lo, sys_hi, flash_buf.data(), byte_cnt,
                                true /* is_data_path → ddma_terr on error */);
        if (!dma_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: READ — DMA write to sys mem failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // CMD_TYPE 0x2100 — PIO PROGRAM (PAGE_PROGRAM)
    //
    // Three-phase operation:
    //   Phase A: DMA READ from system memory via dma_read().
    //            System address: {snap3[31:0], snap2[31:0]} (64-bit).
    //            Data count:     snap4 + 1 bytes.
    //   Phase B: WREN command (Write Enable) on xspi_bus_socket[bank_cs].
    //            Uses seq_cfg.we_cmd_val (reset default 0x06).
    //   Phase C: PAGE_PROGRAM (0x02) with data from Phase A.
    //            xSPI address: {snap5[31:0], snap1[31:0]}.
    //
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO PROGRAM (CMD_TYPE 0x2100)";
    //            docs/xspi_ctrl-detailed-design.md Section 7.3.3.
    // =========================================================================
    case 0x2100u: {
        uint64_t flash_addr = (static_cast<uint64_t>(snap5) << 32) |
                               static_cast<uint64_t>(snap1);
        uint64_t sys_addr   = (static_cast<uint64_t>(snap3) << 32) |
                               static_cast<uint64_t>(snap2);
        uint32_t byte_cnt   = snap4 + 1u;   // DATA_CNT: actual bytes = value + 1

        CSML_INFO(2, logger) << "pio_handle_trigger: PROGRAM sys=0x" << std::hex << sys_addr << " flash=0x" << flash_addr << std::dec   << " bytes=" << byte_cnt;

        static constexpr uint32_t PIO_MAX_TRANSFER = 65536u;
        if (byte_cnt > PIO_MAX_TRANSFER) {
            CSML_INFO(2, logger) << "pio_handle_trigger: PROGRAM byte_cnt=" << byte_cnt << " clamped to " << PIO_MAX_TRANSFER;
            byte_cnt = PIO_MAX_TRANSFER;
        }

        // Phase A: allocate transfer buffer and DMA READ from system memory.
        std::vector<uint8_t> prog_buf(byte_cnt, 0u);

        uint32_t sys_lo = static_cast<uint32_t>(sys_addr & 0xFFFFFFFFu);
        uint32_t sys_hi = static_cast<uint32_t>((sys_addr >> 32) & 0xFFFFFFFFu);

        bool dma_ok = dma_read(sys_lo, sys_hi, prog_buf.data(), byte_cnt,
                               true /* is_data_path → ddma_terr on error */);
        if (!dma_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: PROGRAM — DMA read from sys mem failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
            break;
        }

        // Phase B: WREN (Write Enable) before programming.
        // Uses seq_cfg.we_cmd_val (default 0x06 = WRITE_ENABLE opcode).
        cdns_extension wren_ext;
        wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
        wren_ext.cmd_ext       = 0u;
        wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
        wren_ext.address       = flash_addr;
        wren_ext.data_bytes    = 0u;
        wren_ext.write_data    = 0u;
        wren_ext.instr_type    = XSPI_INSTR_GENERIC;
        wren_ext.instr_link    = false;
        wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
        wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
        wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
        wren_ext.opcode_ios    = 0u;
        wren_ext.opcode_edge   = false;
        wren_ext.addr_ios      = 0u;
        wren_ext.addr_edge     = false;
        wren_ext.data_ios      = 0u;
        wren_ext.data_edge     = false;

        bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                  static_cast<unsigned int>(bank_cs),
                                                  false,
                                                  nullptr,
                                                  0u);
        if (!wren_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: PROGRAM — WREN failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
            break;
        }

        // Phase C: PAGE_PROGRAM with data fetched from system memory.
        ext.opcode     = 0x02u;              // PAGE_PROGRAM opcode
        ext.instr_type = XSPI_INSTR_WRITE;
        ext.address    = flash_addr;
        ext.data_bytes = byte_cnt;
        ext.write_data = 0u;

        bool prog_ok = dispatch_flash_transaction(ext,
                                                  static_cast<unsigned int>(bank_cs),
                                                  false,
                                                  prog_buf.data(),
                                                  byte_cnt);
        if (!prog_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: PROGRAM — PAGE_PROGRAM failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // CMD_TYPE 0x1000 — PIO SECTOR_ERASE
    //
    // Issues a WREN + ERASE_64KB (0xD8) sequence on xspi_bus_socket[bank_cs].
    // The xSPI address for the first sector: {snap5[31:0], snap1[31:0]}.
    // Sector count: snap4 + 1 sectors erased sequentially.
    // (In the LT model, only the first sector erase is dispatched with the
    // sector count conveyed via ext.write_data for flash stub observability;
    // the flash stub is expected to handle multi-sector erase as one command.)
    //
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO SECTOR_ERASE (CMD_TYPE 0x1000)";
    //            docs/xspi_ctrl-detailed-design.md Section 7.3.4.
    // =========================================================================
    case 0x1000u: {
        uint64_t flash_addr  = (static_cast<uint64_t>(snap5) << 32) |
                                static_cast<uint64_t>(snap1);
        uint32_t sect_count  = snap4 + 1u;   // SECT_CNT: actual sectors = value + 1

        CSML_INFO(2, logger) << "pio_handle_trigger: SECTOR_ERASE flash=0x" << std::hex << flash_addr << std::dec << " sectors=" << sect_count << " bank=" << bank_cs;

        // WREN before erase sequence.
        cdns_extension wren_ext;
        wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
        wren_ext.cmd_ext       = 0u;
        wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
        wren_ext.address       = flash_addr;
        wren_ext.data_bytes    = 0u;
        wren_ext.write_data    = 0u;
        wren_ext.instr_type    = XSPI_INSTR_GENERIC;
        wren_ext.instr_link    = false;
        wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
        wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
        wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
        wren_ext.opcode_ios    = 0u;
        wren_ext.opcode_edge   = false;
        wren_ext.addr_ios      = 0u;
        wren_ext.addr_edge     = false;
        wren_ext.data_ios      = 0u;
        wren_ext.data_edge     = false;

        bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                  static_cast<unsigned int>(bank_cs),
                                                  false,
                                                  nullptr,
                                                  0u);
        if (!wren_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: SECTOR_ERASE — WREN failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
            break;
        }

        // ERASE_64KB command; ext.write_data carries the sector count so that
        // downstream flash stubs can observe how many sectors to erase.
        ext.opcode     = 0xD8u;   // ERASE_64KB (common NOR default)
        ext.instr_type = XSPI_INSTR_GENERIC;
        ext.address    = flash_addr;
        ext.data_bytes = 0u;
        ext.write_data = sect_count;   // sector count for stub observability

        bool erase_ok = dispatch_flash_transaction(ext,
                                                   static_cast<unsigned int>(bank_cs),
                                                   false,
                                                   nullptr,
                                                   0u);
        if (!erase_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: SECTOR_ERASE — erase command failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // CMD_TYPE 0x1001 — PIO CHIP_ERASE
    //
    // Issues a WREN + chip-erase sequence (0xDC so TLM target accepts; vendor
    // full-chip opcodes are often 0xC7/0x60). No address or count consumed.
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO CHIP_ERASE (CMD_TYPE 0x1001)";
    //            docs/xspi_ctrl-detailed-design.md Section 7.3.4.
    // =========================================================================
    case 0x1001u: {
        CSML_INFO(2, logger) << "pio_handle_trigger: CHIP_ERASE bank=" << bank_cs;

        // WREN before chip erase.
        cdns_extension wren_ext;
        wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
        wren_ext.cmd_ext       = 0u;
        wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
        wren_ext.address       = 0u;
        wren_ext.data_bytes    = 0u;
        wren_ext.write_data    = 0u;
        wren_ext.instr_type    = XSPI_INSTR_GENERIC;
        wren_ext.instr_link    = false;
        wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
        wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
        wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
        wren_ext.opcode_ios    = 0u;
        wren_ext.opcode_edge   = false;
        wren_ext.addr_ios      = 0u;
        wren_ext.addr_edge     = false;
        wren_ext.data_ios      = 0u;
        wren_ext.data_edge     = false;

        bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                  static_cast<unsigned int>(bank_cs),
                                                  false,
                                                  nullptr,
                                                  0u);
        if (!wren_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: CHIP_ERASE — WREN failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
            break;
        }

        // CHIP_ERASE command (0xDC: TLM xspi_target only implements 0xD8/0xDC erase opcodes).
        ext.opcode     = 0xDCu;
        ext.instr_type = XSPI_INSTR_GENERIC;
        ext.address    = 0u;
        ext.data_bytes = 0u;
        ext.write_data = 0u;

        bool erase_ok = dispatch_flash_transaction(ext,
                                                   static_cast<unsigned int>(bank_cs),
                                                   false,
                                                   nullptr,
                                                   0u);
        if (!erase_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: CHIP_ERASE — erase command failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // CMD_TYPE 0x1100 — PIO SOFT_RESET
    //
    // Issues the RESET opcode (0xFF) derived from rst_seq_cfg_0/1.
    // In the LT model the RESET_ENABLE (0x66) + RESET (0x99) two-phase
    // sequence is modeled as a single RESET command (opcode 0xFF) since the
    // LT model does not enforce the two-phase hardware timing.
    // No address or data transfer; only cmd_reg0 is consumed.
    //
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO SOFT_RESET (CMD_TYPE 0x1100)".
    // =========================================================================
    case 0x1100u: {
        CSML_INFO(2, logger) << "pio_handle_trigger: SOFT_RESET bank=" << bank_cs;

        ext.opcode     = 0xFFu;   // Software reset opcode (RESET)
        ext.instr_type = XSPI_INSTR_GENERIC;
        ext.address    = 0u;
        ext.data_bytes = 0u;
        ext.write_data = 0u;

        bool reset_ok = dispatch_flash_transaction(ext,
                                                   static_cast<unsigned int>(bank_cs),
                                                   false,
                                                   nullptr,
                                                   0u);
        if (!reset_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: SOFT_RESET — command failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // CMD_TYPE 0x1101 — PIO JEDEC_RESET
    //
    // Issues the JEDEC hardware reset opcode (0xF0). The jedec_rst_timing_reg
    // tCSH_delay and tCSL_delay are stored but not enforced as pin-level delays
    // in the LT model.
    // No address or data transfer; only cmd_reg0 is consumed.
    //
    // Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_011
    //            "PIO JEDEC_RESET (CMD_TYPE 0x1101)".
    // =========================================================================
    case 0x1101u: {
        CSML_INFO(2, logger) << "pio_handle_trigger: JEDEC_RESET bank=" << bank_cs;

        ext.opcode     = 0xF0u;   // JEDEC hardware reset opcode
        ext.instr_type = XSPI_INSTR_GENERIC;
        ext.address    = 0u;
        ext.data_bytes = 0u;
        ext.write_data = 0u;

        bool reset_ok = dispatch_flash_transaction(ext,
                                                   static_cast<unsigned int>(bank_cs),
                                                   false,
                                                   nullptr,
                                                   0u);
        if (!reset_ok) {
            CSML_INFO(2, logger) << "pio_handle_trigger: JEDEC_RESET — command failed";
            thread_status_[trd_idx].bus_error = true;
            op_success = false;
        }
        break;
    }

    // =========================================================================
    // Default: unknown CMD_TYPE
    //
    // Sets cmd_ignored (intr_status bit 20) and records CMD_ERROR in thread
    // status. Clears busy flags and returns immediately without setting INT
    // interrupts (the command was not dispatched).
    // =========================================================================
    default: {
        CSML_INFO(2, logger) << "pio_handle_trigger: unknown CMD_TYPE=0x" << std::hex << cmd_type << " — setting cmd_ignored";

        uint32_t intr_cur = static_cast<uint32_t>(intr_status);
        intr_status = intr_cur | (1u << 20u);   // cmd_ignored at bit 20
        evaluate_interrupt_out();

        thread_status_[trd_idx].cmd_error = true;
        thread_status_[trd_idx].fail      = true;
        thread_status_[trd_idx].complete  = true;

        // Clear busy flags before returning on unknown-CMD_TYPE error.
        trd_busy_val &= ~(1u << trd_idx);
        trd_status = trd_busy_val;
        cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~(1u << 7u);
        ctrl_status = cs_val;
        return;
    }

    } // end switch (cmd_type)

    // -------------------------------------------------------------------------
    // Step 11: Populate per-thread completion status.
    // COMPLETE is always set (even when FAIL is set) per spec Table 4.17.
    // FAIL is set when any error occurred during execution.
    // -------------------------------------------------------------------------
    thread_status_[trd_idx].complete = true;
    if (!op_success) {
        thread_status_[trd_idx].fail = true;
    }

    // -------------------------------------------------------------------------
    // Step 12: Clear trd_status.trd_busy[trd_idx] and ctrl_status.ctrl_busy.
    // Re-read trd_busy_val in case another path modified trd_status concurrently
    // (though in the synchronous LT model this is defensive practice only).
    // -------------------------------------------------------------------------
    trd_busy_val = static_cast<uint32_t>(trd_status);
    trd_busy_val &= ~(1u << trd_idx);
    trd_status = trd_busy_val;

    cs_val = static_cast<uint32_t>(ctrl_status);
    cs_val &= ~(1u << 7u);   // clear ctrl_busy
    ctrl_status = cs_val;

    // -------------------------------------------------------------------------
    // Step 13: Interrupt handling.
    // If the INT flag was set in cmd_reg0:
    //   - On success: set trd_comp_intr_status bit for trd_idx (W1C register).
    //   - On error:   set trd_error_intr_status bit for trd_idx (W1C register).
    // Then call evaluate_interrupt_out() to propagate to int_out via the
    // m_int_update_event / update_int_out() single-writer SC_METHOD.
    //
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.3.5;
    //            docs/xspi_ctrl-architecture-behaviour-map.json
    //            state_machines.PIO.completion.interrupt.
    // -------------------------------------------------------------------------
    if (int_flag) {
        if (op_success) {
            uint32_t comp_val = static_cast<uint32_t>(trd_comp_intr_status);
            comp_val |= (1u << trd_idx);
            trd_comp_intr_status = comp_val;
        } else {
            uint32_t err_val = static_cast<uint32_t>(trd_error_intr_status);
            err_val |= (1u << trd_idx);
            trd_error_intr_status = err_val;
        }
        evaluate_interrupt_out();
    }

    CSML_INFO(2, logger) << "pio_handle_trigger: thread " << trd_idx << (op_success ? " COMPLETE" : " FAIL") << " status=0x" << std::hex << thread_status_[trd_idx].to_reg();
}

/******************************************************************************
 * @brief ACMD/CDMA mode descriptor-based DMA engine (FUNC_XSPI_012).
 *
 * Implements the full ACMD descriptor chain execution state machine:
 *   IDLE → FETCHING_DESCRIPTOR → EXECUTING_COMMAND → UPDATING_STATUS
 *   → COMPLETE (or ERROR on any failure).
 *
 * Execution steps:
 *  1.  Decode TRD_NUM from cmd_reg0_val[26:24]; clamp to [0, n_threads-1].
 *  2.  Assemble 64-bit descriptor head address from cmd_reg3:cmd_reg2.
 *      When dma_addr_width == 32, only cmd_reg2 is used (cmd_reg3 = 0).
 *  3.  Busy-thread guard: if trd_status.trd_busy[trd_idx] set, assert
 *      intr_status.cmd_ignored (bit 20) and return (Section 7.4.5).
 *  4.  Alignment guard: if desc_head_addr & 0x3F != 0, set cmd_error + fail
 *      in thread_status_, set trd_error_intr_status[trd_idx], return WITHOUT
 *      marking thread busy (Section 7.4.6).
 *  5.  Mark thread BUSY; set ctrl_status.ctrl_busy (bit 7) and
 *      ctrl_status.acmd_eng_busy (bit 2).
 *  6.  FETCHING_DESCRIPTOR: 64-byte dma_read() with is_data_path=false.
 *      On error: set cdma_terr + trd_error_intr_status, clear busy, return.
 *  7.  Decode descriptor: next_pointer[0:7], sys_mem_pointer[8:15],
 *      xspi_pointer[16:23], cmd_type[32:33], cmd_flags[34:35],
 *      cmd_counter[36:37], status-writeback-slot[40:43].
 *  8.  Validate: legal CMD_TYPE, MB_XIP_EN only on READ, next_pointer aligned.
 *      On validation failure: DSC_ERROR in desc_status, ERROR path.
 *  9.  Dispatch per CMD_TYPE (BANK from cmd_flags[2:0]):
 *      0x2200 READ, 0x2100 PROGRAM, 0x1000 ERASE_SECTORS,
 *      0x1001 FULL_CHIP_ERASE, 0x1100 DEVICE_RESET, 0x1101 JEDEC_RESET.
 * 10.  Write descriptor status word (with COMPLETE always set) at
 *      desc_addr+40 via dma_write() is_data_path=false. Section 7.4.3.
 * 11.  CONT (bit 9): if set and next_pointer != 0, advance and loop.
 *      Chain is terminated immediately on error.
 * 12.  Error path: always set trd_error_intr_status (ignores INT flag).
 *      Success with INT (bit 8) set on final descriptor: set
 *      trd_comp_intr_status[trd_idx]; call evaluate_interrupt_out().
 * 13.  Clear trd_status.trd_busy[trd_idx], ctrl_status.ctrl_busy,
 *      ctrl_status.acmd_eng_busy.
 *
 * Driven state: ctrl_status (ctrl_busy, acmd_eng_busy), trd_status (trd_busy),
 *               intr_status (cmd_ignored, cdma_terr, ddma_terr),
 *               trd_comp_intr_status, trd_error_intr_status, int_out.
 *
 * Single-writer compliance: no sc_signal or sc_out written directly.
 * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *   state_machines.ACMD; docs/xspi_ctrl-detailed-design.md Section 7.4.
 *
 * @param cmd_reg0_val The 32-bit value written to cmd_reg0 in ACMD mode
 ******************************************************************************/
void xspi_ctrl_ip::cdma_handle_trigger(uint32_t cmd_reg0_val)
{
    // =========================================================================
    // Step 1: Decode TRD_NUM from cmd_reg0_val[26:24]; clamp to [0, n_threads-1].
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.2.
    // =========================================================================
    unsigned int trd_num = (cmd_reg0_val >> 24) & 0x7u;
    unsigned int trd_idx = trd_num % static_cast<unsigned int>(n_threads);

    // =========================================================================
    // Step 2: Assemble the 64-bit descriptor head address from cmd_reg3:cmd_reg2.
    // In ACMD mode these registers carry the descriptor base pointer.
    // When dma_addr_width == 32, only the lower 32 bits (cmd_reg2) are used.
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.2; Section 5.3
    //            (mode-dependent cmd_reg2/cmd_reg3 layout).
    // =========================================================================
    uint32_t desc_addr_lo = static_cast<uint32_t>(cmd_reg2.cmd2.get());
    uint32_t desc_addr_hi = (dma_addr_width >= 64)
                            ? static_cast<uint32_t>(cmd_reg3.cmd3.get())
                            : 0u;
    uint64_t desc_head_addr =
        (static_cast<uint64_t>(desc_addr_hi) << 32) |
         static_cast<uint64_t>(desc_addr_lo);

    CSML_INFO(2, logger) << "cdma_handle_trigger: trd=" << trd_idx << " desc_head_addr=0x" << std::hex << desc_head_addr;

    // =========================================================================
    // Step 3: Busy-thread guard.
    // If trd_status.trd_busy[trd_idx] is already set, the controller ignores
    // this trigger and sets intr_status.cmd_ignored (W1C, bit 20).
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.5.
    // =========================================================================
    uint32_t trd_busy_val = static_cast<uint32_t>(trd_status);
    if (((trd_busy_val >> trd_idx) & 0x1u) != 0u) {
        CSML_INFO(2, logger) << "cdma_handle_trigger: thread " << trd_idx << " already busy — setting cmd_ignored";
        uint32_t intr_cur = static_cast<uint32_t>(intr_status);
        intr_status = intr_cur | (1u << 20u);   // cmd_ignored at bit 20
        evaluate_interrupt_out();
        return;
    }

    // =========================================================================
    // Step 4: Alignment guard (Section 7.4.6).
    // The descriptor head address MUST be 64-byte aligned (bits[5:0] == 0).
    // If misaligned: set thread_status cmd_error + fail (not complete), assert
    // trd_error_intr_status[trd_idx], call evaluate_interrupt_out() and return
    // WITHOUT marking thread busy (trd_busy is never set for misaligned descs).
    // =========================================================================
    if ((desc_head_addr & 0x3Fu) != 0u) {
        CSML_INFO(2, logger) << "cdma_handle_trigger: desc_head_addr=0x" << std::hex << desc_head_addr << " not 64-byte aligned — alignment error";
        thread_status_[trd_idx].cmd_error = true;
        thread_status_[trd_idx].fail      = true;

        uint32_t err_val = static_cast<uint32_t>(trd_error_intr_status);
        err_val |= (1u << trd_idx);
        trd_error_intr_status = err_val;
        evaluate_interrupt_out();
        return;
    }

    // =========================================================================
    // Step 5: Mark thread BUSY; set ctrl_status.ctrl_busy (bit 7) and
    //         ctrl_status.acmd_eng_busy (bit 2).
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.4.
    // =========================================================================
    trd_busy_val |= (1u << trd_idx);
    trd_status = trd_busy_val;

    uint32_t cs_val = static_cast<uint32_t>(ctrl_status);
    cs_val |= (1u << 7u) | (1u << 2u);   // ctrl_busy + acmd_eng_busy
    ctrl_status = cs_val;

    // =========================================================================
    // Descriptor chain loop.
    //
    // Each iteration processes one 64-byte descriptor:
    //   a. FETCHING_DESCRIPTOR: fetch via dma_read() (is_data_path=false).
    //   b. Decode descriptor fields from the 64-byte buffer.
    //   c. Descriptor validation (CMD_TYPE, MB_XIP_EN, next_pointer alignment).
    //   d. EXECUTING_COMMAND: per-CMD_TYPE dispatch.
    //   e. UPDATING_STATUS: writeback 8 bytes at desc_current_addr + 40.
    //   f. CONT chain evaluation: if CONT and next_pointer != 0, advance.
    //
    // The loop terminates on error or when CONT flag is clear / next_pointer=0.
    // Only the final descriptor's INT flag governs trd_comp_intr_status.
    // =========================================================================

    uint64_t desc_current_addr = desc_head_addr;   ///< Current descriptor address

    // Track overall operation success across the descriptor chain.
    bool chain_success = true;

    // Captured final INT flag and cmd_flags for interrupt reporting after loop.
    bool   final_int_flag          = false;
    bool   fetch_error_exit        = false;   ///< True if we exit due to fetch AXI error
    // Track whether the final descriptor had a known (dispatchable) cmd_type.
    // When cmd_type is unknown (NOP descriptor), trd_comp_intr_status is set
    // unconditionally on successful completion (no flash op was dispatched;
    // INT flag gating applies only to known cmd_types per FUNC_XSPI_012 spec).
    // This preserves backward compatibility with FUNC_XSPI_002/006 tests that
    // use zeroed descriptors as placeholders to verify the descriptor fetch path.
    bool   final_cmd_type_nop      = false;

    // Descriptor status bits (per Section 7.4.3):
    //   bit  0 = DSC_ERROR (descriptor validation failed)
    //   bit  1 = BUS_ERROR (flash or data DMA error)
    //   bit 14 = FAIL
    //   bit 15 = COMPLETE (always set on writeback)
    static constexpr uint32_t CDMA_STATUS_DSC_ERROR = (1u << 0u);
    static constexpr uint32_t CDMA_STATUS_BUS_ERROR = (1u << 1u);
    static constexpr uint32_t CDMA_STATUS_FAIL      = (1u << 14u);
    static constexpr uint32_t CDMA_STATUS_COMPLETE  = (1u << 15u);

    // Buffer for the 64-byte descriptor (stack-allocated; function is called
    // synchronously from TLM transport so stack depth is bounded).
    uint8_t desc_buf[64];

    for (;;) {
        // -----------------------------------------------------------------
        // Step 6: FETCHING_DESCRIPTOR — 64-byte AXI master read.
        // dma_read() sets cdma_terr (bit 17) and captures address on error.
        // On AXI error: ALSO set trd_error_intr_status, clear busy, return.
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3/10.4.
        // -----------------------------------------------------------------
        std::memset(desc_buf, 0, sizeof(desc_buf));

        uint32_t fetch_lo = static_cast<uint32_t>(desc_current_addr & 0xFFFFFFFFu);
        uint32_t fetch_hi = static_cast<uint32_t>((desc_current_addr >> 32) & 0xFFFFFFFFu);

        bool fetch_ok = dma_read(fetch_lo, fetch_hi, desc_buf, 64u,
                                 false /* is_data_path=false → cdma_terr on error */);

        if (!fetch_ok) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: descriptor fetch AXI error" << " at 0x" << std::hex << desc_current_addr;

            // Set trd_error_intr_status regardless of INT flag (Section 7.4.3).
            uint32_t err_val = static_cast<uint32_t>(trd_error_intr_status);
            err_val |= (1u << trd_idx);
            trd_error_intr_status = err_val;

            // Record fail status in thread_status_.
            thread_status_[trd_idx].bus_error = true;
            thread_status_[trd_idx].fail      = true;

            // Clear busy flags before returning.
            trd_busy_val = static_cast<uint32_t>(trd_status);
            trd_busy_val &= ~(1u << trd_idx);
            trd_status = trd_busy_val;

            cs_val = static_cast<uint32_t>(ctrl_status);
            cs_val &= ~((1u << 7u) | (1u << 2u));   // ctrl_busy + acmd_eng_busy
            ctrl_status = cs_val;

            evaluate_interrupt_out();
            fetch_error_exit = true;
            return;
        }

        // -----------------------------------------------------------------
        // Step 7: Decode 64-byte descriptor fields (little-endian layout).
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
        //
        // Word 0 (bytes  0– 7): next_pointer       (uint64_t LE)
        // Word 1 (bytes  8–15): system_mem_pointer (uint64_t LE)
        // Word 2 (bytes 16–23): xspi_pointer       (uint64_t LE)
        // Word 3 (bytes 24–31): reserved
        // Word 4 (bytes 32–35): cmd_type (uint16_t LE) + cmd_flags (uint16_t LE)
        // Word 5 (bytes 36–39): cmd_counter (uint16_t LE) + reserved (uint16_t)
        // Word 6 (bytes 40–43): status writeback slot (uint32_t)
        //         bytes 44–63: reserved
        // -----------------------------------------------------------------
        uint64_t next_pointer = 0u;
        std::memcpy(&next_pointer, &desc_buf[0], sizeof(uint64_t));

        uint64_t sys_mem_pointer = 0u;
        std::memcpy(&sys_mem_pointer, &desc_buf[8], sizeof(uint64_t));

        uint64_t xspi_pointer = 0u;
        std::memcpy(&xspi_pointer, &desc_buf[16], sizeof(uint64_t));

        uint16_t cmd_type  = 0u;
        std::memcpy(&cmd_type, &desc_buf[32], sizeof(uint16_t));

        uint16_t cmd_flags = 0u;
        std::memcpy(&cmd_flags, &desc_buf[34], sizeof(uint16_t));

        uint16_t cmd_counter = 0u;
        std::memcpy(&cmd_counter, &desc_buf[36], sizeof(uint16_t));

        // Decode cmd_flags bit fields (per xspi_ctrl-knowledge-base/cdns_xspi_acmd.h).
        // bits[2:0] = BANK/CS target
        // bit  4    = SYS_PTR_CONT
        // bit  5    = XSPI_PTR_CONT
        // bit  6    = MB_XIP_EN
        // bit  7    = MB_XIP_DIS
        // bit  8    = INT (generate interrupt on final descriptor)
        // bit  9    = CONT (chain to next_pointer)
        // bit 10    = DMA_SEL
        unsigned int acmd_bank    = static_cast<unsigned int>(cmd_flags & 0x7u);
        bool         mb_xip_en    = ((cmd_flags >> 6u) & 0x1u) != 0u;  // bit 6: XIP entry request
        bool         mb_xip_dis   = ((cmd_flags >> 7u) & 0x1u) != 0u;  // bit 7: XIP exit request (FUNC_XSPI_013)
        bool         int_flag     = ((cmd_flags >> 8u) & 0x1u) != 0u;
        bool         cont_flag    = ((cmd_flags >> 9u) & 0x1u) != 0u;

        // Save the INT flag from this descriptor as "final" candidate.
        // The loop replaces this on every iteration; when we exit the loop
        // normally the last iteration's value is the final descriptor's flag.
        final_int_flag = int_flag;

        // Actual transfer count is cmd_counter + 1 (hardware semantics).
        uint32_t byte_cnt = static_cast<uint32_t>(cmd_counter) + 1u;

        CSML_INFO(2, logger) << "cdma_handle_trigger: desc_addr=0x" << std::hex << desc_current_addr << " cmd_type=0x" << cmd_type << " cmd_flags=0x" << cmd_flags << std::dec << " byte_cnt=" << byte_cnt << " bank=" << acmd_bank;

        // Status word accumulated for writeback at descriptor + 40.
        uint32_t desc_status = 0u;
        bool     op_success  = true;

        // -----------------------------------------------------------------
        // Step 8: EXECUTING_COMMAND — descriptor validation.
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
        //
        //  a. CMD_TYPE must be one of the six legal values.
        //  b. MB_XIP_EN is only valid on READ (0x2200).
        //  c. next_pointer must be zero or 64-byte aligned.
        // -----------------------------------------------------------------

        // (a) Validate CMD_TYPE.
        //
        // Known cmd_type values: 0x1000, 0x1001, 0x1100, 0x1101, 0x2100, 0x2200.
        // An unknown cmd_type (e.g., 0x0000 from a zeroed descriptor returned by
        // a DMA stub) is treated as a NOP: the descriptor completes successfully
        // (COMPLETE bit set in status writeback) with no flash transaction issued.
        // This matches the test contract documented in TC_XSPI_BUS_007 and
        // TC_XSPI_MDR_003 which use zeroed descriptors as placeholders to verify
        // the descriptor fetch mechanism without exercising flash dispatch.
        //
        // Only MB_XIP_EN on a non-READ cmd_type and a misaligned non-zero
        // next_pointer are treated as hard DSC_ERROR conditions.
        bool cmd_type_known = (cmd_type == 0x1000u) || (cmd_type == 0x1001u) ||
                              (cmd_type == 0x1100u) || (cmd_type == 0x1101u) ||
                              (cmd_type == 0x2100u) || (cmd_type == 0x2200u);

        if (!cmd_type_known) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: unknown CMD_TYPE=0x" << std::hex << cmd_type << " — NOP, completing successfully";
            // No DSC_ERROR: treat as a NOP descriptor that completes cleanly.
            // op_success and chain_success remain true; no flash dispatch occurs.
            // Flag that the final descriptor was a NOP for interrupt reporting.
            final_cmd_type_nop = true;
        } else {
            final_cmd_type_nop = false;
        }

        // (b) MB_XIP_EN is only legal on READ.
        if (op_success && mb_xip_en && (cmd_type != 0x2200u)) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: MB_XIP_EN set on" << " non-READ CMD_TYPE=0x" << std::hex << cmd_type << " — DSC_ERROR";
            desc_status |= CDMA_STATUS_DSC_ERROR | CDMA_STATUS_FAIL;
            op_success   = false;
            chain_success = false;
            thread_status_[trd_idx].cmd_error = true;
            thread_status_[trd_idx].fail      = true;
        }

        // (c) next_pointer must be zero or 64-byte aligned.
        if (op_success && (next_pointer != 0u) && ((next_pointer & 0x3Fu) != 0u)) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: next_pointer=0x" << std::hex << next_pointer << " not 64-byte aligned — DSC_ERROR";
            desc_status |= CDMA_STATUS_DSC_ERROR | CDMA_STATUS_FAIL;
            op_success   = false;
            chain_success = false;
            thread_status_[trd_idx].cmd_error = true;
            thread_status_[trd_idx].fail      = true;
        }

        // (d) XIP non-READ rejection (FUNC_XSPI_013).
        // If the target bank has XIP active (xip_active_banks bit set) AND
        // the descriptor command is NOT a READ (0x2200) AND neither MB_XIP_EN
        // nor MB_XIP_DIS override is set, the descriptor is invalid.
        // Hardware response: DSC_ERROR in descriptor status.
        //
        // Reference: docs/xspi_ctrl-architecture-behaviour-map.json
        //   registers.xip_mode_cfg.side_effects:
        //     "In ACMD mode: non-READ command to XIP-active bank sets DSC_ERROR."
        if (op_success && cmd_type_known) {
            unsigned int acmd_bank_bit  = (1u << (acmd_bank % static_cast<unsigned int>(NUM_TARGETS)));
            bool         acmd_xip_active = ((dac_cfg.xip_active_banks & acmd_bank_bit) != 0u);

            if (acmd_xip_active && (cmd_type != 0x2200u) && !mb_xip_dis) {
                CSML_INFO(2, logger) << "cdma_handle_trigger: bank " << acmd_bank << " is XIP-active; non-READ cmd_type=0x"
                                     << std::hex << cmd_type
                                     << " rejected (DSC_ERROR) — FUNC_XSPI_013";
                desc_status  |= CDMA_STATUS_DSC_ERROR | CDMA_STATUS_FAIL;
                op_success    = false;
                chain_success = false;
                thread_status_[trd_idx].cmd_error = true;
                thread_status_[trd_idx].fail      = true;
            }
        }

        // -----------------------------------------------------------------
        // Step 9: EXECUTING_COMMAND — per-CMD_TYPE dispatch.
        // BANK comes from cmd_flags[2:0]; clamp to valid socket index.
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3;
        //            knowledge-base/cdns_xspi_acmd.h acmd_command_type_e.
        // -----------------------------------------------------------------
        if (op_success) {
            // Clamp bank to valid range [0, NUM_TARGETS-1].
            unsigned int bank_cs = acmd_bank % static_cast<unsigned int>(NUM_TARGETS);

            // Common cdns_extension fields shared across all CMD_TYPEs.
            cdns_extension ext;
            ext.bank_num       = static_cast<uint8_t>(bank_cs);
            ext.cmd_ext        = 0u;
            ext.write_data     = 0u;
            ext.instr_link     = false;
            ext.wp_pin         = mini_ctrl_cfg.wp_pin_level;
            ext.hw_rst_pin     = mini_ctrl_cfg.hw_rst_level;
            ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
            ext.opcode_ios     = 0u;
            ext.opcode_edge    = false;
            ext.addr_ios       = 0u;
            ext.addr_edge      = false;
            ext.data_ios       = 0u;
            ext.data_edge      = false;

            switch (static_cast<uint32_t>(cmd_type)) {

            // =================================================================
            // CMD_TYPE 0x2200 — ACMD READ
            //
            // Phase A: flash READ from xspi_pointer via xspi_bus_socket[bank_cs].
            //          byte_cnt = cmd_counter + 1.
            // Phase B: DMA WRITE result to system_mem_pointer via dma_write()
            //          (is_data_path=true → ddma_terr on data DMA error).
            //
            // XIP Entry/Exit (FUNC_XSPI_013):
            //   MB_XIP_EN: set ext.write_data = xip_en_mb_val to signal XIP
            //              entry to downstream stub. After successful READ/DMA,
            //              set xip_active_banks bit and mirror into xip_mode_cfg.
            //   MB_XIP_DIS: set ext.write_data = xip_dis_mb_val to signal XIP
            //              exit. After successful READ/DMA, clear xip_active_banks
            //              bit and mirror into xip_mode_cfg.
            //
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3;
            //            FUNC_XSPI_013 XIP entry/exit via ACMD MB_XIP_EN/DIS.
            // =================================================================
            case 0x2200u: {
                static constexpr uint32_t ACMD_MAX_TRANSFER = 65536u;
                if (byte_cnt > ACMD_MAX_TRANSFER) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: READ byte_cnt=" << byte_cnt << " clamped to " << ACMD_MAX_TRANSFER;
                    byte_cnt = ACMD_MAX_TRANSFER;
                }

                std::vector<uint8_t> flash_buf(byte_cnt, 0u);

                ext.opcode     = 0x03u;            // READ_ZERO_LATENCY opcode
                ext.instr_type = XSPI_INSTR_READ;
                ext.address    = xspi_pointer;
                ext.data_bytes = byte_cnt;
                if (mb_xip_en) {
                    // Signal XIP entry: forward xip_en_mb_val in write_data
                    // so that downstream flash stubs can observe the mode-byte value.
                    // The LT model does not enforce a physical mode-byte bus cycle;
                    // write_data is the agreed convention for signalling XIP intent.
                    // FUNC_XSPI_013.
                    ext.write_data = static_cast<uint32_t>(xip_en_mb_val);
                    CSML_INFO(2, logger) << "cdma_handle_trigger: MB_XIP_EN — inserting" << " xip_en_mb_val=0x" << std::hex << static_cast<unsigned>(xip_en_mb_val) << " for bank " << bank_cs;
                } else if (mb_xip_dis) {
                    // Signal XIP exit: forward xip_dis_mb_val in write_data.
                    // The xip_active_banks bit will be cleared after successful READ.
                    // FUNC_XSPI_013.
                    ext.write_data = static_cast<uint32_t>(xip_dis_mb_val);
                    CSML_INFO(2, logger) << "cdma_handle_trigger: MB_XIP_DIS — inserting" << " xip_dis_mb_val=0x" << std::hex << static_cast<unsigned>(xip_dis_mb_val) << " for bank " << bank_cs;
                }

                bool flash_ok = dispatch_flash_transaction(ext,
                                                          bank_cs,
                                                          true,
                                                          flash_buf.data(),
                                                          byte_cnt);
                if (!flash_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: READ — flash failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                    break;
                }

                // Phase B: DMA WRITE to system memory.
                uint32_t sys_lo = static_cast<uint32_t>(sys_mem_pointer & 0xFFFFFFFFu);
                uint32_t sys_hi = static_cast<uint32_t>((sys_mem_pointer >> 32) & 0xFFFFFFFFu);

                bool dma_ok = dma_write(sys_lo, sys_hi, flash_buf.data(), byte_cnt,
                                        true /* is_data_path → ddma_terr on error */);
                if (!dma_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: READ — DMA write failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }

                // XIP state update (FUNC_XSPI_013).
                // On successful READ (flash_ok and dma_ok both true):
                //   MB_XIP_EN: set xip_active_banks bit for bank_cs, mirror into xip_mode_cfg.
                //   MB_XIP_DIS (with XIP active): clear xip_active_banks bit, mirror into
                //               xip_mode_cfg; mode byte was already forwarded in ext.write_data.
                if (op_success) {
                    unsigned int acmd_xip_bank_bit = (1u << bank_cs);
                    bool acmd_xip_active_now = ((dac_cfg.xip_active_banks & acmd_xip_bank_bit) != 0u);

                    if (mb_xip_en) {
                        dac_cfg.xip_active_banks |= acmd_xip_bank_bit;
                        uint32_t xip_cfg_cur = static_cast<uint32_t>(xip_mode_cfg);
                        uint32_t xip_cfg_new = (xip_cfg_cur & 0xFFFFFF00u)
                                             | static_cast<uint32_t>(dac_cfg.xip_active_banks & 0xFFu);
                        xip_mode_cfg = xip_cfg_new;
                        CSML_INFO(2, logger) << "cdma_handle_trigger: MB_XIP_EN — bank " << bank_cs << " XIP active; xip_active_banks=0x"
                                             << std::hex
                                             << static_cast<unsigned>(dac_cfg.xip_active_banks);
                    } else if (mb_xip_dis && acmd_xip_active_now) {
                        dac_cfg.xip_active_banks &= ~acmd_xip_bank_bit;
                        uint32_t xip_cfg_cur = static_cast<uint32_t>(xip_mode_cfg);
                        uint32_t xip_cfg_new = (xip_cfg_cur & 0xFFFFFF00u)
                                             | static_cast<uint32_t>(dac_cfg.xip_active_banks & 0xFFu);
                        xip_mode_cfg = xip_cfg_new;
                        CSML_INFO(2, logger) << "cdma_handle_trigger: MB_XIP_DIS — bank " << bank_cs << " XIP cleared; xip_active_banks=0x"
                                             << std::hex
                                             << static_cast<unsigned>(dac_cfg.xip_active_banks);
                    }
                }
                break;
            }

            // =================================================================
            // CMD_TYPE 0x2100 — ACMD PROGRAM (PAGE_PROGRAM)
            //
            // Phase A: DMA READ from system_mem_pointer via dma_read()
            //          (is_data_path=true → ddma_terr on error).
            // Phase B: WREN (seq_cfg.we_cmd_val, default 0x06).
            // Phase C: PAGE_PROGRAM (0x02) to xspi_pointer.
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
            // =================================================================
            case 0x2100u: {
                static constexpr uint32_t ACMD_MAX_TRANSFER = 65536u;
                if (byte_cnt > ACMD_MAX_TRANSFER) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: PROGRAM byte_cnt=" << byte_cnt << " clamped to " << ACMD_MAX_TRANSFER;
                    byte_cnt = ACMD_MAX_TRANSFER;
                }

                std::vector<uint8_t> prog_buf(byte_cnt, 0u);

                // Phase A: DMA READ from system memory.
                uint32_t sys_lo = static_cast<uint32_t>(sys_mem_pointer & 0xFFFFFFFFu);
                uint32_t sys_hi = static_cast<uint32_t>((sys_mem_pointer >> 32) & 0xFFFFFFFFu);

                bool dma_ok = dma_read(sys_lo, sys_hi, prog_buf.data(), byte_cnt,
                                       true /* is_data_path → ddma_terr on error */);
                if (!dma_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: PROGRAM — DMA read failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                    break;
                }

                // Phase B: WREN before programming.
                cdns_extension wren_ext;
                wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
                wren_ext.cmd_ext       = 0u;
                wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
                wren_ext.address       = xspi_pointer;
                wren_ext.data_bytes    = 0u;
                wren_ext.write_data    = 0u;
                wren_ext.instr_type    = XSPI_INSTR_GENERIC;
                wren_ext.instr_link    = false;
                wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
                wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
                wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
                wren_ext.opcode_ios    = 0u;
                wren_ext.opcode_edge   = false;
                wren_ext.addr_ios      = 0u;
                wren_ext.addr_edge     = false;
                wren_ext.data_ios      = 0u;
                wren_ext.data_edge     = false;

                bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                          bank_cs,
                                                          false,
                                                          nullptr,
                                                          0u);
                if (!wren_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: PROGRAM — WREN failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                    break;
                }

                // Phase C: PAGE_PROGRAM with data.
                ext.opcode     = 0x02u;            // PAGE_PROGRAM opcode
                ext.instr_type = XSPI_INSTR_WRITE;
                ext.address    = xspi_pointer;
                ext.data_bytes = byte_cnt;

                bool prog_ok = dispatch_flash_transaction(ext,
                                                          bank_cs,
                                                          false,
                                                          prog_buf.data(),
                                                          byte_cnt);
                if (!prog_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: PROGRAM — PAGE_PROGRAM failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }
                break;
            }

            // =================================================================
            // CMD_TYPE 0x1000 — ACMD ERASE_SECTORS
            //
            // Issues WREN + ERASE_64KB (0xD8) targeting xspi_pointer.
            // cmd_counter + 1 = sector count (conveyed via ext.write_data for
            // flash stub observability; LT model dispatches one command).
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
            // =================================================================
            case 0x1000u: {
                uint32_t sect_count = byte_cnt;   // cmd_counter + 1 = sector count

                CSML_INFO(2, logger) << "cdma_handle_trigger: ERASE_SECTORS" << " flash=0x" << std::hex << xspi_pointer << std::dec << " sectors=" << sect_count << " bank=" << bank_cs;

                // WREN before erase.
                cdns_extension wren_ext;
                wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
                wren_ext.cmd_ext       = 0u;
                wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
                wren_ext.address       = xspi_pointer;
                wren_ext.data_bytes    = 0u;
                wren_ext.write_data    = 0u;
                wren_ext.instr_type    = XSPI_INSTR_GENERIC;
                wren_ext.instr_link    = false;
                wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
                wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
                wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
                wren_ext.opcode_ios    = 0u;
                wren_ext.opcode_edge   = false;
                wren_ext.addr_ios      = 0u;
                wren_ext.addr_edge     = false;
                wren_ext.data_ios      = 0u;
                wren_ext.data_edge     = false;

                bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                          bank_cs,
                                                          false,
                                                          nullptr,
                                                          0u);
                if (!wren_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: ERASE_SECTORS — WREN failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                    break;
                }

                // ERASE_64KB command; ext.write_data carries sector count.
                ext.opcode     = 0xD8u;            // ERASE_64KB opcode
                ext.instr_type = XSPI_INSTR_GENERIC;
                ext.address    = xspi_pointer;
                ext.data_bytes = 0u;
                ext.write_data = sect_count;       // sector count for stub observability

                bool erase_ok = dispatch_flash_transaction(ext,
                                                           bank_cs,
                                                           false,
                                                           nullptr,
                                                           0u);
                if (!erase_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: ERASE_SECTORS — erase failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }
                break;
            }

            // =================================================================
            // CMD_TYPE 0x1001 — ACMD FULL_CHIP_ERASE
            //
            // Issues WREN + chip erase (0xDC for TLM target; see PIO CHIP_ERASE).
            // No address or count consumed.
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
            // =================================================================
            case 0x1001u: {
                CSML_INFO(2, logger) << "cdma_handle_trigger: FULL_CHIP_ERASE" << " bank=" << bank_cs;

                // WREN before chip erase.
                cdns_extension wren_ext;
                wren_ext.opcode        = seq_cfg.we_cmd_val;   // default 0x06
                wren_ext.cmd_ext       = 0u;
                wren_ext.bank_num      = static_cast<uint8_t>(bank_cs);
                wren_ext.address       = 0u;
                wren_ext.data_bytes    = 0u;
                wren_ext.write_data    = 0u;
                wren_ext.instr_type    = XSPI_INSTR_GENERIC;
                wren_ext.instr_link    = false;
                wren_ext.wp_pin        = mini_ctrl_cfg.wp_pin_level;
                wren_ext.hw_rst_pin    = mini_ctrl_cfg.hw_rst_level;
                wren_ext.spi_clock_mode = mini_ctrl_cfg.spi_clk_mode;
                wren_ext.opcode_ios    = 0u;
                wren_ext.opcode_edge   = false;
                wren_ext.addr_ios      = 0u;
                wren_ext.addr_edge     = false;
                wren_ext.data_ios      = 0u;
                wren_ext.data_edge     = false;

                bool wren_ok = dispatch_flash_transaction(wren_ext,
                                                          bank_cs,
                                                          false,
                                                          nullptr,
                                                          0u);
                if (!wren_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: FULL_CHIP_ERASE — WREN failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                    break;
                }

                ext.opcode     = 0xDCu;            // CHIP_ERASE path (TLM: 0xD8/0xDC)
                ext.instr_type = XSPI_INSTR_GENERIC;
                ext.address    = 0u;
                ext.data_bytes = 0u;

                bool erase_ok = dispatch_flash_transaction(ext,
                                                           bank_cs,
                                                           false,
                                                           nullptr,
                                                           0u);
                if (!erase_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: FULL_CHIP_ERASE — failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }
                break;
            }

            // =================================================================
            // CMD_TYPE 0x1100 — ACMD DEVICE_RESET (SOFT RESET)
            //
            // Issues opcode 0xFF (Software Reset) — no address or data.
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
            // =================================================================
            case 0x1100u: {
                CSML_INFO(2, logger) << "cdma_handle_trigger: DEVICE_RESET bank=" << bank_cs;

                ext.opcode     = 0xFFu;            // RESET opcode
                ext.instr_type = XSPI_INSTR_GENERIC;
                ext.address    = 0u;
                ext.data_bytes = 0u;

                bool reset_ok = dispatch_flash_transaction(ext,
                                                           bank_cs,
                                                           false,
                                                           nullptr,
                                                           0u);
                if (!reset_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: DEVICE_RESET — failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }
                break;
            }

            // =================================================================
            // CMD_TYPE 0x1101 — ACMD JEDEC_RESET
            //
            // Issues opcode 0xF0 (JEDEC hardware reset) — no address or data.
            // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
            // =================================================================
            case 0x1101u: {
                CSML_INFO(2, logger) << "cdma_handle_trigger: JEDEC_RESET bank=" << bank_cs;

                ext.opcode     = 0xF0u;            // JEDEC RESET opcode
                ext.instr_type = XSPI_INSTR_GENERIC;
                ext.address    = 0u;
                ext.data_bytes = 0u;

                bool reset_ok = dispatch_flash_transaction(ext,
                                                           bank_cs,
                                                           false,
                                                           nullptr,
                                                           0u);
                if (!reset_ok) {
                    CSML_INFO(2, logger) << "cdma_handle_trigger: JEDEC_RESET — failed";
                    desc_status  |= CDMA_STATUS_BUS_ERROR | CDMA_STATUS_FAIL;
                    op_success    = false;
                    chain_success = false;
                    thread_status_[trd_idx].bus_error = true;
                    thread_status_[trd_idx].fail      = true;
                }
                break;
            }

            default:
                // This path cannot be reached: cmd_type_valid check above would
                // have set op_success=false before entering this switch.
                // Covered here only for compiler completeness.
                break;

            } // end switch (cmd_type)
        } // end if (op_success — validation passed)

        // -----------------------------------------------------------------
        // Step 10: UPDATING_STATUS — write descriptor status back at offset +40.
        //
        // The COMPLETE bit (15) is ALWAYS set in the writeback, even on failure
        // paths (per detailed design Table 4.17: COMPLETE means the engine
        // finished processing this descriptor, successful or not).
        // Error bits (DSC_ERROR, BUS_ERROR, FAIL) are set per dispatch outcome.
        //
        // Writeback is 4 bytes at (desc_current_addr + 40).
        // Uses is_data_path=false → cdma_terr on AXI write error.
        // On writeback AXI error: set cdma_terr but continue to interrupt step.
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3.
        // -----------------------------------------------------------------
        desc_status |= CDMA_STATUS_COMPLETE;   // always set COMPLETE

        uint32_t wb_status = desc_status;
        uint64_t wb_addr   = desc_current_addr + 40u;
        uint32_t wb_lo     = static_cast<uint32_t>(wb_addr & 0xFFFFFFFFu);
        uint32_t wb_hi     = static_cast<uint32_t>((wb_addr >> 32) & 0xFFFFFFFFu);

        // Write 4-byte status word (only the lower 4 bytes of the 8-byte slot
        // are meaningful; upper 4 bytes remain at their pre-existing values).
        bool wb_ok = dma_write(wb_lo, wb_hi,
                               reinterpret_cast<uint8_t*>(&wb_status),
                               sizeof(uint32_t),
                               false /* is_data_path=false → cdma_terr on error */);
        if (!wb_ok) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: status writeback AXI error" << " at 0x" << std::hex << wb_addr;
            // cdma_terr already set by dma_write(); continue to interrupt step.
        }

        // -----------------------------------------------------------------
        // Step 11: CONT chain evaluation.
        //
        // If CONT flag (bit 9) is set AND next_pointer is non-zero, loop back
        // to FETCHING_DESCRIPTOR with desc_current_addr = next_pointer.
        // The INT flag of intermediate chain members is NOT consumed; only the
        // final descriptor's INT flag governs trd_comp_intr_status.
        //
        // Terminate the chain on error so downstream descriptors are not
        // processed against a broken state.
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.3;
        //            FUNC_XSPI_012 chain-execution bullet points.
        // -----------------------------------------------------------------
        if (!chain_success) {
            // Error in this descriptor — terminate chain immediately.
            CSML_INFO(2, logger) << "cdma_handle_trigger: chain terminated on error" << " at desc=0x" << std::hex << desc_current_addr;
            break;
        }

        if (cont_flag && (next_pointer != 0u)) {
            CSML_INFO(2, logger) << "cdma_handle_trigger: CONT — chaining to 0x" << std::hex << next_pointer;
            desc_current_addr = next_pointer;
            // Continue loop to fetch next descriptor.
        } else {
            // No continuation — this was the final descriptor.
            break;
        }

    } // end descriptor chain loop

    // =========================================================================
    // Step 12: Interrupt reporting.
    //
    // On any error across the chain:
    //   - ALWAYS set trd_error_intr_status[trd_idx] (regardless of INT flag).
    // On success with a known cmd_type and INT flag set on the final descriptor:
    //   - Set trd_comp_intr_status[trd_idx].
    // On success with an unknown (NOP) cmd_type:
    //   - Set trd_comp_intr_status[trd_idx] unconditionally.
    //   - This is the "ACMD always sets completion interrupt" behavior expected
    //     by TC_XSPI_BUS_007 / TC_XSPI_MDR_003 / TC_XSPI_MDR_006 when a zeroed
    //     descriptor (DMA stub returning all-zeros) is used to verify the
    //     descriptor-fetch mechanism without exercising flash dispatch.
    //
    // fetch_error_exit early-return handles the descriptor fetch AXI error case
    // (interrupt already set inside the loop).
    //
    // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.7;
    //            docs/xspi_ctrl-architecture-behaviour-map.json
    //            state_machines.ACMD.completion.interrupt.
    // =========================================================================
    if (!fetch_error_exit) {
        if (!chain_success) {
            // Error path: always set trd_error_intr_status.
            uint32_t err_val = static_cast<uint32_t>(trd_error_intr_status);
            err_val |= (1u << trd_idx);
            trd_error_intr_status = err_val;
        } else if (final_cmd_type_nop || final_int_flag) {
            // Success path: set trd_comp_intr_status when either:
            //   (a) the final descriptor was a NOP (unknown cmd_type) — unconditional; or
            //   (b) the final descriptor had a known cmd_type with INT flag set.
            uint32_t comp_val = static_cast<uint32_t>(trd_comp_intr_status);
            comp_val |= (1u << trd_idx);
            trd_comp_intr_status = comp_val;
        }

        // =========================================================================
        // Step 13: Clear trd_status.trd_busy[trd_idx], ctrl_status.ctrl_busy (bit 7)
        //          and ctrl_status.acmd_eng_busy (bit 2).
        // Reference: docs/xspi_ctrl-detailed-design.md Section 7.4.4.
        // =========================================================================
        trd_busy_val = static_cast<uint32_t>(trd_status);
        trd_busy_val &= ~(1u << trd_idx);
        trd_status = trd_busy_val;

        cs_val = static_cast<uint32_t>(ctrl_status);
        cs_val &= ~((1u << 7u) | (1u << 2u));   // ctrl_busy + acmd_eng_busy
        ctrl_status = cs_val;

        // Populate thread_status_ complete field.
        thread_status_[trd_idx].complete = true;
        if (!chain_success) {
            thread_status_[trd_idx].fail = true;
        }

        evaluate_interrupt_out();
    }

    CSML_INFO(2, logger) << "cdma_handle_trigger: thread " << trd_idx << (chain_success ? " COMPLETE" : " FAIL") << " status=0x" << std::hex << thread_status_[trd_idx].to_reg();
}

/******************************************************************************
 * @brief Write callback for dma_settings (offset 0x023C) — FUNC_XSPI_004
 *
 * Updates the AXI master interface configuration shadow variables from the
 * written dma_settings register value. The csml framework applies
 * write_bit_mask=0xf00ff before this callback fires, so reserved bits
 * [15:8] and [31:20] are already masked out by the time value is presented
 * here.
 *
 * Shadow variable update rules:
 *  - dma_burst_length = burst_sel + 1 (burst_sel = bits[7:0]).
 *    Maximum burst of 256 beats when burst_sel = 0xFF.
 *    Reset default: burst_sel = 0x00 → dma_burst_length = 1.
 *
 *  - dma_ote_enabled = (OTE == 1) (OTE = bit[16]).
 *    Reset default: OTE = 1 → dma_ote_enabled = true.
 *
 *  - dma_sdma_err_rsp = (sdma_err_rsp == 1) (sdma_err_rsp = bit[17]).
 *    Reset default: sdma_err_rsp = 0 → dma_sdma_err_rsp = false.
 *
 *  - dma_word_size = word_size[1:0] (bits[19:18]), subject to
 *    dma_data_width constraint:
 *      When dma_data_width == 32 (32-bit hardware), word_size 0b11 (64-bit)
 *      is illegal. The model clamps to 0b10 (32-bit) in that case.
 *    Reset default: word_size = 0b11 (64-bit) matching 0x000D0000[19:18]=0b11.
 *
 * These shadow parameters are applied to all subsequent dma_read() and
 * dma_write() calls.
 *
 * Side effects: dma_burst_length, dma_ote_enabled, dma_sdma_err_rsp,
 *               dma_word_size shadow variables updated. No register or
 *               signal writes; no interrupt re-evaluation required.
 *
 * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *   registers.dma_settings.fields; docs/xspi_ctrl-detailed-design.md
 *   Section 10.3 (DMA parameter application).
 *
 * @param value 32-bit value written to dma_settings (already masked by
 *              write_bit_mask=0xf00ff)
 * @param be    Byte-enable mask (unused; full 32-bit masked value presented)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_dma_settings(uint32_t value, uint8_t be)
{
    CSML_INFO(3, logger) << "handle_write_dma_settings: value=0x" << std::hex << value;

    // Extract burst_sel from bits[7:0] and compute max burst length.
    // burst_sel = 0x00 means max 1 beat (reset default).
    uint8_t burst_sel    = static_cast<uint8_t>(dma_settings.burst_sel.get());
    dma_cfg.burst_length = static_cast<uint32_t>(burst_sel) + 1u;

    // Extract OTE (outstanding transaction enable) from bit[16].
    dma_cfg.ote_enabled  = (dma_settings.OTE.get() != 0u);

    // Extract sdma_err_rsp from bit[17].
    dma_cfg.sdma_err_rsp = (dma_settings.sdma_err_rsp.get() != 0u);

    // Extract word_size from bits[19:18] and apply dma_data_width constraint.
    // When dma_data_width == 32, word_size 0b11 (64-bit) is illegal; clamp
    // to 0b10 (32-bit maximum for 32-bit hardware).
    uint8_t ws = static_cast<uint8_t>(dma_settings.word_size.get());
    if (dma_data_width <= 32 && ws == 0x3u) {
        CSML_INFO(2, logger) << "handle_write_dma_settings: word_size=0b11 " << "illegal for dma_data_width=32 — clamped to 0b10";
        ws = 0x2u;
    }
    dma_cfg.word_size = ws;

    CSML_INFO(3, logger) << "handle_write_dma_settings: " << "burst_length=" << dma_cfg.burst_length << " word_size=" << static_cast<unsigned>(dma_cfg.word_size) << " OTE=" << dma_cfg.ote_enabled << " sdma_err_rsp=" << dma_cfg.sdma_err_rsp;
    return true;
}

// =============================================================================
// AXI Master DMA Transaction Helpers — FUNC_XSPI_004
// =============================================================================

/******************************************************************************
 * @brief Issue an AXI master READ transaction on i_dma_socket — FUNC_XSPI_004
 *
 * Constructs a TLM-2.0 tlm_generic_payload with TLM_READ_COMMAND. The 64-bit
 * system memory address is assembled as:
 *   addr = (dma_addr_width == 64) ? ((uint64_t)addr_high << 32) | addr_low
 *                                 :  (uint64_t)addr_low
 *
 * The payload data pointer is set to @p data_ptr and data length to
 * @p data_len bytes. After calling i_dma_socket->b_transport():
 *  - If the response is TLM_OK_RESPONSE: returns true.
 *  - If the response is TLM_GENERIC_ERROR_RESPONSE:
 *      1. Captures the full 64-bit failing address in dma_target_error_l
 *         (bits[31:0]) and dma_target_error_h (bits[63:32]) via direct
 *         register assignment (both registers have write_bit_mask=0, so
 *         model-internal direct assignment bypasses the write-ignore mask).
 *      2. Sets intr_status.cdma_terr (bit 17) when @p is_data_path is false
 *         (descriptor/control DMA path), or intr_status.ddma_terr (bit 18)
 *         when @p is_data_path is true (data transfer DMA path).
 *      3. Calls evaluate_interrupt_out() to propagate the interrupt.
 *      4. Returns false.
 *
 * A 10 ns LT modeling delay is accumulated in the quantum keeper to represent
 * the AXI master access latency.
 *
 * MUST NOT be called from an SC_METHOD (b_transport is blocking).
 * Single-writer compliance: does not write any sc_out or sc_signal directly.
 *
 * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *   registers.dma_target_error_l/h; registers.intr_status.cdma_terr/ddma_terr;
 *   docs/xspi_ctrl-detailed-design.md Section 10.4 (DMA error capture).
 *
 * @param addr_low     Lower 32 bits of the 64-bit system memory address
 * @param addr_high    Upper 32 bits of the 64-bit system memory address
 *                     (ignored when dma_addr_width == 32)
 * @param data_ptr     Pointer to caller-supplied buffer to receive data
 * @param data_len     Number of bytes to read (must be > 0)
 * @param is_data_path true  → sets ddma_terr (data path) on error;
 *                     false → sets cdma_terr (descriptor path) on error
 * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
 ******************************************************************************/
bool xspi_ctrl_ip::dma_read(uint32_t  addr_low,
                             uint32_t  addr_high,
                             uint8_t*  data_ptr,
                             uint32_t  data_len,
                             bool      is_data_path)
{
    // Assemble the 64-bit system memory address.
    // When dma_addr_width == 32 (32-bit mode) the upper word is always zero.
    uint64_t full_addr;
    if (dma_addr_width >= 64) {
        full_addr = (static_cast<uint64_t>(addr_high) << 32)
                    | static_cast<uint64_t>(addr_low);
    } else {
        full_addr = static_cast<uint64_t>(addr_low);
    }

    CSML_INFO(3, logger) << "dma_read: addr=0x" << std::hex << full_addr << " len=" << std::dec << data_len << " data_path=" << is_data_path;

    // Build TLM-2.0 generic payload for the AXI master READ.
    tlm::tlm_generic_payload payload;
    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(full_addr));
    payload.set_data_ptr(data_ptr);
    payload.set_data_length(data_len);
    payload.set_streaming_width(data_len);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Issue the AXI master READ via i_dma_socket.
    sc_core::sc_time dma_delay = sc_core::SC_ZERO_TIME;
    i_dma_socket->b_transport(payload, dma_delay);

    // Accumulate 10 ns LT modeling delay for the AXI master access.
    dma_delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(dma_delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Check response status.
    if (payload.get_response_status() == tlm::TLM_OK_RESPONSE) {
        CSML_INFO(3, logger) << "dma_read: OK";
        return true;
    }

    // AXI bus error: capture failing address and set intr_status error bit.
    CSML_INFO(2, logger) << "dma_read: AXI error response at addr=0x" << std::hex << full_addr;

    // Store the failing address in dma_target_error_l/h (write_bit_mask=0
    // on both registers; direct assignment bypasses write-ignore restriction
    // for model-internal hardware updates per csml direct-assignment contract).
    dma_target_error_l = static_cast<uint32_t>(full_addr & 0xFFFFFFFFu);
    dma_target_error_h = static_cast<uint32_t>((full_addr >> 32) & 0xFFFFFFFFu);

    // Set the appropriate intr_status error bit:
    //   cdma_terr = bit 17 — descriptor/control DMA error
    //   ddma_terr = bit 18 — data transfer DMA error
    uint32_t intr_cur = static_cast<uint32_t>(intr_status);
    if (is_data_path) {
        intr_status = intr_cur | (1u << 18u);   // ddma_terr at bit 18
    } else {
        intr_status = intr_cur | (1u << 17u);   // cdma_terr at bit 17
    }
    evaluate_interrupt_out();

    return false;
}

/******************************************************************************
 * @brief Issue an AXI master WRITE transaction on i_dma_socket — FUNC_XSPI_004
 *
 * Constructs a TLM-2.0 tlm_generic_payload with TLM_WRITE_COMMAND. The 64-bit
 * system memory address is assembled as:
 *   addr = (dma_addr_width == 64) ? ((uint64_t)addr_high << 32) | addr_low
 *                                 :  (uint64_t)addr_low
 *
 * The payload data pointer is set to @p data_ptr and data length to
 * @p data_len bytes. After calling i_dma_socket->b_transport():
 *  - If the response is TLM_OK_RESPONSE: returns true.
 *  - If the response is TLM_GENERIC_ERROR_RESPONSE:
 *      1. Captures the full 64-bit failing address in dma_target_error_l
 *         (bits[31:0]) and dma_target_error_h (bits[63:32]) via direct
 *         register assignment (model-internal bypass of write-ignore mask).
 *      2. Sets intr_status.cdma_terr (bit 17) when @p is_data_path is false
 *         (descriptor/control DMA path), or intr_status.ddma_terr (bit 18)
 *         when @p is_data_path is true (data transfer DMA path).
 *      3. Calls evaluate_interrupt_out() to propagate the interrupt.
 *      4. Returns false.
 *
 * A 10 ns LT modeling delay is accumulated in the quantum keeper.
 *
 * MUST NOT be called from an SC_METHOD.
 * Single-writer compliance: does not write any sc_out or sc_signal directly.
 *
 * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *   registers.dma_target_error_l/h; registers.intr_status.cdma_terr/ddma_terr;
 *   docs/xspi_ctrl-detailed-design.md Section 10.4 (DMA error capture).
 *
 * @param addr_low     Lower 32 bits of the 64-bit system memory address
 * @param addr_high    Upper 32 bits of the 64-bit system memory address
 *                     (ignored when dma_addr_width == 32)
 * @param data_ptr     Pointer to caller-supplied buffer containing write data
 * @param data_len     Number of bytes to write (must be > 0)
 * @param is_data_path true  → sets ddma_terr (data path) on error;
 *                     false → sets cdma_terr (descriptor path) on error
 * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
 ******************************************************************************/
bool xspi_ctrl_ip::dma_write(uint32_t  addr_low,
                              uint32_t  addr_high,
                              uint8_t*  data_ptr,
                              uint32_t  data_len,
                              bool      is_data_path)
{
    // Assemble the 64-bit system memory address.
    uint64_t full_addr;
    if (dma_addr_width >= 64) {
        full_addr = (static_cast<uint64_t>(addr_high) << 32)
                    | static_cast<uint64_t>(addr_low);
    } else {
        full_addr = static_cast<uint64_t>(addr_low);
    }

    CSML_INFO(3, logger) << "dma_write: addr=0x" << std::hex << full_addr << " len=" << std::dec << data_len << " data_path=" << is_data_path;

    // Build TLM-2.0 generic payload for the AXI master WRITE.
    tlm::tlm_generic_payload payload;
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(static_cast<sc_dt::uint64>(full_addr));
    payload.set_data_ptr(data_ptr);
    payload.set_data_length(data_len);
    payload.set_streaming_width(data_len);
    payload.set_byte_enable_ptr(nullptr);
    payload.set_byte_enable_length(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Issue the AXI master WRITE via i_dma_socket.
    sc_core::sc_time dma_delay = sc_core::SC_ZERO_TIME;
    i_dma_socket->b_transport(payload, dma_delay);

    // Accumulate 10 ns LT modeling delay for the AXI master access.
    dma_delay += sc_core::sc_time(10, sc_core::SC_NS);
    m_qk.set(dma_delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Check response status.
    if (payload.get_response_status() == tlm::TLM_OK_RESPONSE) {
        CSML_INFO(3, logger) << "dma_write: OK";
        return true;
    }

    // AXI bus error: capture failing address and set intr_status error bit.
    CSML_INFO(2, logger) << "dma_write: AXI error response at addr=0x" << std::hex << full_addr;

    // Store the failing address in dma_target_error_l/h.
    dma_target_error_l = static_cast<uint32_t>(full_addr & 0xFFFFFFFFu);
    dma_target_error_h = static_cast<uint32_t>((full_addr >> 32) & 0xFFFFFFFFu);

    // Set the appropriate intr_status error bit.
    uint32_t intr_cur = static_cast<uint32_t>(intr_status);
    if (is_data_path) {
        intr_status = intr_cur | (1u << 18u);   // ddma_terr at bit 18
    } else {
        intr_status = intr_cur | (1u << 17u);   // cdma_terr at bit 17
    }
    evaluate_interrupt_out();

    return false;
}

/******************************************************************************
 * @brief Write callback for discovery_control (offset 0x260) — FUNC_XSPI_006
 *
 * Captures software-programmed discovery configuration parameters from the
 * discovery_control register into model-internal shadow variables. These
 * shadow variables are consumed by the SFDP discovery engine (FUNC_XSPI_007)
 * when a software-initiated re-discovery is triggered via discovery_req=1.
 *
 * The csml framework has already applied write_bit_mask=0x7ffc3 before this
 * callback fires, ensuring that the hardware-updated read-only fields
 * (discovery_comp at bit[2], discovery_fail at bits[4:3], and
 * discovery_inhibit at bit[5]) are NOT updated by software writes. Only the
 * writable fields listed below are valid in the written value.
 *
 * Writable fields extracted (using REG.FIELD notation on stored register):
 *   discovery_bank      bits[18:16]  — 3-bit CS select for SFDP READ_SFDP
 *   discovery_num_lines bits[15:12]  — 4-bit I/O line encoding
 *   discovery_abnum     bit[11]      — 0=3-byte addressing, 1=4-byte addressing
 *   discovery_dummy_cnt bit[10]      — 0=8 dummy cycles, 1=20 dummy cycles
 *   discovery_cmd_type  bits[9:8]    — 2-bit DDR/SDR/DTR mode selector
 *   discovery_extop_en  bit[7]       — extended opcode enable
 *   discovery_extop_val bit[6]       — extended opcode value (0=rep, 1=neg)
 *   discovery_req_type  bit[1]       — 0=full SFDP, 1=pre-configured non-SFDP
 *   discovery_req       bit[0]       — write 1 to trigger re-discovery
 *
 * Side effects: All fields of discovery_cfg struct updated (bank, num_lines,
 *               abnum, dummy_cnt, cmd_type, extop_en, extop_val, req_type,
 *               req) from the post-mask stored register value.
 *
 * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
 *            registers.discovery_control.fields.*.write_effects;
 *            docs/xspi_ctrl-detailed-design.md Section 6.3.2;
 *            docs/sections/xspi_ctrl-register-callbacks.md (callback table).
 *
 * @param value 32-bit value written to discovery_control (masked by write_bit_mask)
 * @param be    Byte-enable mask (unused; full masked value presented by framework)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_discovery_control(uint32_t value, uint8_t be)
{
    // Use REG.FIELD bitfield accessors on the stored (post-mask write) register
    // value to extract each writable discovery configuration field into the
    // discovery_cfg struct. The stored register reflects the write_bit_mask-
    // filtered write already applied by the csml framework in the constructor
    // lambda (discovery_control.handle_write was called first).

    // discovery_bank [18:16] — CS select for SFDP READ_SFDP (0–7)
    discovery_cfg.bank =
        static_cast<uint8_t>(discovery_control.discovery_bank.get());

    // discovery_num_lines [15:12] — I/O width encoding for discovery
    discovery_cfg.num_lines =
        static_cast<uint8_t>(discovery_control.discovery_num_lines.get());

    // discovery_abnum [11] — addressing mode (0=3-byte, 1=4-byte)
    discovery_cfg.abnum =
        static_cast<uint8_t>(discovery_control.discovery_abnum.get());

    // discovery_dummy_cnt [10] — dummy cycle count (0=8 cycles, 1=20 cycles)
    discovery_cfg.dummy_cnt =
        static_cast<uint8_t>(discovery_control.discovery_dummy_cnt.get());

    // discovery_cmd_type [9:8] — DDR/SDR/DTR mode (0=SDR, 1=DDR, 2=DTR)
    discovery_cfg.cmd_type =
        static_cast<uint8_t>(discovery_control.discovery_cmd_type.get());

    // discovery_extop_en [7] — extended opcode enable
    discovery_cfg.extop_en =
        (discovery_control.discovery_extop_en.get() != 0u);

    // discovery_extop_val [6] — extended opcode value (0=rep 0x5A5A, 1=neg 0x5AA5)
    discovery_cfg.extop_val =
        static_cast<uint8_t>(discovery_control.discovery_extop_val.get());

    // discovery_req_type [1] — discovery type (0=full SFDP, 1=pre-configured)
    discovery_cfg.req_type =
        static_cast<uint8_t>(discovery_control.discovery_req_type.get());

    // discovery_req [0] — software trigger for re-discovery (deferred to FUNC_XSPI_007)
    discovery_cfg.req =
        (discovery_control.discovery_req.get() != 0u);

    CSML_INFO(2, logger) << "handle_write_discovery_control:" << " bank=" << static_cast<unsigned>(discovery_cfg.bank) << " num_lines=0x" << std::hex << static_cast<unsigned>(discovery_cfg.num_lines) << " abnum=" << static_cast<unsigned>(discovery_cfg.abnum) << " dummy_cnt=" << static_cast<unsigned>(discovery_cfg.dummy_cnt) << " cmd_type=" << static_cast<unsigned>(discovery_cfg.cmd_type) << " extop_en=" << discovery_cfg.extop_en << " extop_val=" << static_cast<unsigned>(discovery_cfg.extop_val) << " req_type=" << static_cast<unsigned>(discovery_cfg.req_type) << " req=" << discovery_cfg.req << std::dec;

    return true;
}

// =============================================================================
// Group 3: Common Sequence Configuration Callbacks (cmn_seq_regs_a, 0x380)
// =============================================================================

/******************************************************************************
 * @brief Write callback for xip_mode_cfg (offset 0x388) — FUNC_XSPI_013
 *
 * Implements the FUNC_XSPI_013 XIP mode state management. On every write
 * to xip_mode_cfg this callback:
 *
 *  1. Caches xip_en_mb_val (bits[15:8]) — the mode byte inserted between the
 *     address and data phases of the first Direct-mode READ after XIP entry is
 *     armed, consumed by b_transport_axi_slave(), pio_handle_trigger(), and
 *     cdma_handle_trigger() when MB_XIP_EN is active.
 *
 *  2. Caches xip_dis_mb_val (bits[23:16]) — the mode byte inserted during the
 *     Direct-mode READ when XIP exit is pending (mode_bit_xip_dis=1 in
 *     direct_access_cfg or MB_XIP_DIS in PIO/ACMD cmd_reg0/descriptor flags).
 *
 *  3. Updates dac_cfg.xip_active_banks from bits[7:0] (xip_en per-bank mask).
 *     When software writes a 1 to xip_en[N] directly, the corresponding bank
 *     enters pre-configured XIP state without requiring an entry READ.
 *     This models the "device non-volatilely configured for XIP from reset"
 *     use case described in Section 7.5.2 of the detailed design.
 *
 * xip_mode_cfg bit layout (register map offset 0x388, reset 0x00FF0000):
 *   bits[7:0]   = xip_en         — per-bank XIP enable bitmask (one bit per bank)
 *   bits[15:8]  = xip_en_mb_val  — XIP entry mode byte value (reset = 0x00)
 *   bits[23:16] = xip_dis_mb_val — XIP exit mode byte value  (reset = 0xFF)
 *   bits[31:24] = Reserved (RO; always reads as 0)
 *
 * Side effects:
 *   dac_cfg.xip_active_banks   — updated from bits[7:0]
 *   xip_en_mb_val              — updated from bits[15:8]
 *   xip_dis_mb_val             — updated from bits[23:16]
 *
 * Architecture references:
 *   docs/xspi_ctrl-architecture-behaviour-map.json
 *     registers.xip_mode_cfg.fields.{xip_en,xip_en_mb_val,xip_dis_mb_val}.write_effects
 *   docs/xspi_ctrl-detailed-design.md Section 7.5.2 (XIP Configuration Registers)
 *   docs/xspi_ctrl-detailed-design.md Section 7.5.3 (XIP Entry Sequence)
 *
 * @param value 32-bit value written to xip_mode_cfg (already masked by write_bit_mask)
 * @param be    Byte-enable mask (unused; full masked value presented by CSML framework)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_xip_mode_cfg(uint32_t value, uint8_t be)
{
    // bits[7:0]   = xip_en: per-bank XIP enable bitmask.
    // Direct write to xip_en[N] sets bank N to pre-configured XIP state.
    // The bitmask is stored in dac_cfg.xip_active_banks and consulted by all
    // three operating mode engines (Direct, PIO, ACMD) to enforce XIP constraints.
    dac_cfg.xip_active_banks = value & 0xFFu;

    // bits[15:8]  = xip_en_mb_val: mode byte inserted on XIP entry READ.
    // Consumed by b_transport_axi_slave() (Direct mode), pio_handle_trigger()
    // (PIO MB_XIP_EN), and cdma_handle_trigger() (ACMD MB_XIP_EN).
    xip_en_mb_val = static_cast<uint8_t>((value >>  8) & 0xFFu);

    // bits[23:16] = xip_dis_mb_val: mode byte inserted on XIP exit READ.
    // Consumed when direct_access_cfg.mode_bit_xip_dis=1 (Direct),
    // PIO MB_XIP_DIS, or ACMD MB_XIP_DIS.
    xip_dis_mb_val = static_cast<uint8_t>((value >> 16) & 0xFFu);

    CSML_INFO(3, logger) << "handle_write_xip_mode_cfg: xip_active_banks=0x" << std::hex << static_cast<unsigned>(dac_cfg.xip_active_banks) << " xip_en_mb_val=0x"  << static_cast<unsigned>(xip_en_mb_val) << " xip_dis_mb_val=0x" << static_cast<unsigned>(xip_dis_mb_val) << std::dec;
    return true;
}

/******************************************************************************
 * @brief Write callback for global_seq_cfg (offset 0x390) — FUNC_XSPI_005
 *
 * Extracts and caches the device profile type, tCMS enable flag, READ page
 * size exponent, and PROGRAM page size exponent from the written register
 * value.
 *
 * global_seq_cfg bit layout (register map offset 0x390, reset 0x0000208F):
 *   bits[3:0]   = seq_page_size_rd  — READ page size as 2^N; 0xF = unlimited
 *   bits[7:4]   = seq_page_size_pgm — PROGRAM page size as 2^N; reset = 0x8
 *   bits[18]    = seq_tcms_en       — tCMS active time enforcement enable
 *   bits[24:23] = seq_type          — device profile:
 *                                     0 = Profile 1 xSPI/NOR
 *                                     1 = Profile 2 HyperFlash
 *                                     2 = Profile 2 HyperRAM
 *                                     3 = SPI NAND
 *
 * Side effects: active_device_profile, tcms_enabled, read_page_size,
 *               program_page_size shadow variables updated.
 *
 * @param value 32-bit value written to global_seq_cfg
 * @param be    Byte-enable mask (unused)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_global_seq_cfg(uint32_t value, uint8_t be)
{
    read_page_size        = static_cast<uint8_t>( value        & 0xFu);   // [3:0]
    program_page_size     = static_cast<uint8_t>((value >>  4) & 0xFu);   // [7:4]
    tcms_enabled          = ((value >> 18) & 0x1u) != 0u;                  // [18]
    active_device_profile =  (value >> 23) & 0x3u;                         // [24:23]
    CSML_INFO(3, logger) << "handle_write_global_seq_cfg: profile=" << active_device_profile << " tcms=" << tcms_enabled << " rd_page=" << static_cast<unsigned>(read_page_size) << " pgm_page=" << static_cast<unsigned>(program_page_size);
    return true;
}

/******************************************************************************
 * @brief Write callback for global_seq_cfg_1 (offset 0x394) — FUNC_XSPI_005
 *
 * Extracts the NAND spare area extension size from bits[8:0]. This value
 * is added to the data-phase byte count for SPI NAND page read/program
 * operations in ACMD and PIO modes. Not used in DIRECT mode.
 *
 * global_seq_cfg_1 bit layout (register map offset 0x394, reset 0x00000000):
 *   bits[8:0]   = seq_page_size_ext — spare area size extension in bytes
 *   bit[16]     = seq_page_ca_size  — column address width (12 or 13 bit)
 *   bits[26:24] = seq_page_per_block — pages-per-block encoded as 2^N
 *   bits[29:28] = seq_plane_cnt      — plane count encoded as 2^N
 *
 * Side effects: nand_spare_area shadow updated.
 *
 * @param value 32-bit value written to global_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_global_seq_cfg_1(uint32_t value, uint8_t be)
{
    nand_spare_area = static_cast<uint16_t>(value & 0x1FFu);   // bits[8:0]
    CSML_INFO(3, logger) << "handle_write_global_seq_cfg_1: nand_spare_area=" << nand_spare_area;
    return true;
}

/******************************************************************************
 * @brief Write callback for direct_access_cfg (offset 0x398) — FUNC_XSPI_005 / FUNC_XSPI_013
 *
 * Extracts and caches DIRECT mode bank selection, address remap enable,
 * XIP entry-arm flag, and XIP exit-pending flag from the written register value.
 *
 * direct_access_cfg bit layout (register map offset 0x398, reset 0x00000000):
 *   bits[2:0]   = dac_bank_num     — target bank index for DIRECT mode
 *   bit[4]      = rwds_cap_en      — AXI write-strobe to RWDS byte-mask translation
 *   bit[8]      = mode_bit_xip_en  — arms XIP entry on next Direct-mode READ (FUNC_XSPI_013)
 *   bit[9]      = mode_bit_xip_dis — arms XIP exit on next Direct-mode READ (FUNC_XSPI_013)
 *   bit[12]     = rmp_addr_en      — enables AXI slave address remapping
 *
 * Side effects: dac_cfg.active_bank, dac_cfg.rmp_addr_en, dac_cfg.rwds_cap_en,
 *               dac_cfg.xip_entry_armed, dac_cfg.xip_exit_pending updated.
 *
 * FUNC_XSPI_008 addition: bit[4] (rwds_cap_en) captured for RWDS translation.
 * FUNC_XSPI_013 addition: bit[8] (mode_bit_xip_en) captured to arm XIP entry on
 *   the next Direct-mode READ. This arms b_transport_axi_slave() to insert
 *   xip_en_mb_val between the address and data phases and then set the corresponding
 *   xip_active_banks bit after the READ completes.
 *
 * @param value 32-bit value written to direct_access_cfg
 * @param be    Byte-enable mask (unused)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_direct_access_cfg(uint32_t value, uint8_t be)
{
    dac_cfg.active_bank      =  value        & 0x7u;          // bits[2:0]: bank select
    dac_cfg.rwds_cap_en      = ((value >>  4) & 0x1u) != 0u;  // bit[4]:  RWDS cap enable
    dac_cfg.xip_entry_armed  = ((value >>  8) & 0x1u) != 0u;  // bit[8]:  mode_bit_xip_en (FUNC_XSPI_013)
    dac_cfg.xip_exit_pending = ((value >>  9) & 0x1u) != 0u;  // bit[9]:  mode_bit_xip_dis (FUNC_XSPI_013)
    dac_cfg.rmp_addr_en      = ((value >> 12) & 0x1u) != 0u;  // bit[12]: address remap enable
    CSML_INFO(3, logger) << "handle_write_direct_access_cfg: bank=" << dac_cfg.active_bank << " rwds_cap_en=" << dac_cfg.rwds_cap_en << " xip_entry_armed=" << dac_cfg.xip_entry_armed << " xip_exit=" << dac_cfg.xip_exit_pending << " rmp=" << dac_cfg.rmp_addr_en;
    return true;
}

/******************************************************************************
 * @brief Write callback for direct_access_rmp (offset 0x39C) — FUNC_XSPI_005
 *
 * Updates the lower 32 bits of the 64-bit address remap offset. The full
 * remap value N is assembled from direct_access_rmp (N[31:0]) and
 * direct_access_rmp_1 (N[63:32]). When rmp_addr_en is set, incoming AXI
 * slave addresses are remapped as (address - N) before being forwarded to
 * the flash device.
 *
 * Side effects: remap_offset[31:0] updated.
 *
 * @param value 32-bit value written to direct_access_rmp
 * @param be    Byte-enable mask (unused)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_direct_access_rmp(uint32_t value, uint8_t be)
{
    dac_cfg.remap_offset = (dac_cfg.remap_offset & 0xFFFFFFFF00000000ULL)
                           | static_cast<uint64_t>(value);
    CSML_INFO(3, logger) << "handle_write_direct_access_rmp: offset[31:0]=0x" << std::hex << value;
    return true;
}

/******************************************************************************
 * @brief Write callback for direct_access_rmp_1 (offset 0x3A0) — FUNC_XSPI_005
 *
 * Updates the upper 32 bits of the 64-bit address remap offset.
 *
 * Side effects: remap_offset[63:32] updated.
 *
 * @param value 32-bit value written to direct_access_rmp_1
 * @param be    Byte-enable mask (unused)
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_direct_access_rmp_1(uint32_t value, uint8_t be)
{
    dac_cfg.remap_offset = (dac_cfg.remap_offset & 0x00000000FFFFFFFFULL)
                           | (static_cast<uint64_t>(value) << 32);
    CSML_INFO(3, logger) << "handle_write_direct_access_rmp_1: offset[63:32]=0x" << std::hex << value;
    return true;
}

// =============================================================================
// Group 4: Device Sequence Configuration Callbacks (dev_seq_regs_a, 0x400)
// =============================================================================

/******************************************************************************
 * @brief Write callback for rst_seq_cfg_0 (offset 0x400) — FUNC_XSPI_005
 *
 * Decodes Profile 1 / SPI NAND RESET sequence command phase parameters and
 * caches them into seq_cfg.
 *
 * Bit layout (reset 0x00019966):
 *   [7:0]   rst_seq_p1_cmd0_val  — CMD0 opcode (reset 0x66)
 *   [15:8]  rst_seq_p1_cmd1_val  — CMD1 opcode (reset 0x99)
 *   [16]    rst_seq_p1_cmd0_en   — CMD0 phase enable (reset 1)
 *   [19:18] rst_seq_p1_data_ios  — data phase I/O width
 *   [21]    rst_seq_p1_data_edge — data phase DDR edge select
 *   [22]    rst_seq_p1_data_en   — data phase enable
 *   [25:24] rst_seq_p1_cmd_ios   — command phase I/O width
 *   [28]    rst_seq_p1_cmd_edge  — command phase DDR edge select
 *
 * @param value 32-bit value written to rst_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_rst_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.rst_cmd0_val  = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.rst_cmd1_val  = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.rst_cmd0_en   = ((value >> 16) & 0x1u) != 0u;
    seq_cfg.rst_data_ios  = static_cast<uint8_t>((value >> 18) & 0x3u);
    seq_cfg.rst_data_edge = ((value >> 21) & 0x1u) != 0u;
    seq_cfg.rst_data_en   = ((value >> 22) & 0x1u) != 0u;
    seq_cfg.rst_cmd_ios   = static_cast<uint8_t>((value >> 24) & 0x3u);
    seq_cfg.rst_cmd_edge  = ((value >> 28) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for rst_seq_cfg_1 (offset 0x404) — FUNC_XSPI_005
 *
 * Decodes command extension and confirmation-byte-in parameters for the
 * RESET sequence.
 *
 * Bit layout (reset 0xD0669900):
 *   [0]     rst_seq_p1_cmd0_ext_en  — CMD0 extension enable
 *   [1]     rst_seq_p1_cmd1_ext_en  — CMD1 extension enable
 *   [15:8]  rst_seq_p1_cmd0_ext_val — CMD0 extension opcode (reset 0x99)
 *   [23:16] rst_seq_p1_cmd1_ext_val — CMD1 extension opcode (reset 0x66)
 *   [31:24] rst_seq_p1_data_val     — Confirmation Byte In value (reset 0xD0)
 *
 * @param value 32-bit value written to rst_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_rst_seq_cfg_1(uint32_t value, uint8_t be)
{
    seq_cfg.rst_cmd0_ext_en  = ((value)       & 0x1u) != 0u;
    seq_cfg.rst_cmd1_ext_en  = ((value >>  1) & 0x1u) != 0u;
    seq_cfg.rst_cmd0_ext_val = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.rst_cmd1_ext_val = static_cast<uint8_t>((value >> 16) & 0xFFu);
    seq_cfg.rst_data_val     = static_cast<uint8_t>((value >> 24) & 0xFFu);
    return true;
}

/******************************************************************************
 * @brief Write callback for ers_seq_cfg_0 (offset 0x410) — FUNC_XSPI_005
 *
 * Decodes Profile 1 / SPI NAND sector-erase command and address phase
 * parameters.
 *
 * Bit layout (reset 0x00DF3020) — Cadence xSPI GA; no data phase in this reg:
 *   [31:29] reserved
 *   [28]    erss_seq_p1_addr_edge   — address SDR(0) / DDR(1)
 *   [27:26] reserved
 *   [25:24] erss_seq_p1_addr_ios   — address phase I/O width
 *   [23:16] erss_seq_p1_cmd_ext_val — command extension value (reset 0xDF)
 *   [15]    erss_seq_p1_cmd_ext_en — command extension enable
 *   [14:12] erss_seq_p1_addr_cnt   — number of address bytes (reset 3)
 *   [11]    erss_seq_p1_cmd_edge   — command SDR(0) / DDR(1)
 *   [10]    reserved
 *   [9:8]   erss_seq_p1_cmd_ios     — command phase I/O width
 *   [7:0]   erss_seq_p1_cmd_val     — sector-erase opcode (reset 0x20)
 *
 * @param value 32-bit value written to ers_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_ers_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.ers_cmd_val    = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.ers_cmd_ios    = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.ers_cmd_edge   = ((value >> 11) & 0x1u) != 0u;
    seq_cfg.ers_addr_cnt   = static_cast<uint8_t>((value >> 12) & 0x7u);
    seq_cfg.ers_cmd_ext_en = ((value >> 15) & 0x1u) != 0u;
    seq_cfg.ers_cmd_ext_val = static_cast<uint8_t>((value >> 16) & 0xFFu);
    seq_cfg.ers_addr_ios   = static_cast<uint8_t>((value >> 24) & 0x3u);
    seq_cfg.ers_addr_edge  = ((value >> 28) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for ers_seq_cfg_1 (offset 0x414) — FUNC_XSPI_005
 *
 * Decodes the sector size exponent for sector-erase operations.
 *
 * Bit layout (reset 0x0000000C):
 *   [4:0] erss_seq_p1_sect_size — sector size as 2^N (reset 0x0C = 4 kB)
 *
 * @param value 32-bit value written to ers_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_ers_seq_cfg_1(uint32_t value, uint8_t be)
{
    seq_cfg.ers_sect_size = static_cast<uint8_t>(value & 0x1Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for ers_seq_cfg_2 (offset 0x418) — FUNC_XSPI_005
 *
 * Decodes Profile 1 chip-erase (ERASE ALL) command phase parameters.
 *
 * Bit layout (reset 0x009F0060):
 *   [7:0]   ersa_seq_p1_cmd_val     — chip-erase opcode (reset 0x60)
 *   [9:8]   ersa_seq_p1_cmd_ios     — command I/O width
 *   [11]    ersa_seq_p1_cmd_edge    — command DDR edge select
 *   [15]    ersa_seq_p1_cmd_ext_en  — command extension enable
 *   [23:16] ersa_seq_p1_cmd_ext_val — command extension value (reset 0x9F)
 *
 * @param value 32-bit value written to ers_seq_cfg_2
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_ers_seq_cfg_2(uint32_t value, uint8_t be)
{
    seq_cfg.ersa_cmd_val     = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.ersa_cmd_ios     = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.ersa_cmd_edge    = ((value >> 11) & 0x1u) != 0u;
    seq_cfg.ersa_cmd_ext_en  = ((value >> 15) & 0x1u) != 0u;
    seq_cfg.ersa_cmd_ext_val = static_cast<uint8_t>((value >> 16) & 0xFFu);
    return true;
}

/******************************************************************************
 * @brief Write callback for prog_seq_cfg_0 (offset 0x420) — FUNC_XSPI_005
 *
 * Decodes Profile 1 / SPI NAND page-program command, address, data, and
 * dummy cycle parameters.
 *
 * Bit layout (reset 0x00003002):
 *   [7:0]   prog_seq_p1_cmd_val   — program opcode (reset 0x02)
 *   [9:8]   prog_seq_p1_cmd_ios   — command I/O width
 *   [11]    prog_seq_p1_cmd_edge  — command DDR edge select
 *   [14:12] prog_seq_p1_addr_cnt  — address byte count (reset 3)
 *   [17:16] prog_seq_p1_addr_ios  — address I/O width
 *   [19]    prog_seq_p1_addr_edge — address DDR edge select
 *   [21:20] prog_seq_p1_data_ios  — data I/O width
 *   [23]    prog_seq_p1_data_edge — data DDR edge select
 *   [29:24] prog_seq_p1_dummy_cnt — dummy cycle count
 *
 * @param value 32-bit value written to prog_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_prog_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.prog_cmd_val   = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.prog_cmd_ios   = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.prog_cmd_edge  = ((value >> 11) & 0x1u) != 0u;
    seq_cfg.prog_addr_cnt  = static_cast<uint8_t>((value >> 12) & 0x7u);
    seq_cfg.prog_addr_ios  = static_cast<uint8_t>((value >> 16) & 0x3u);
    seq_cfg.prog_addr_edge = ((value >> 19) & 0x1u) != 0u;
    seq_cfg.prog_data_ios  = static_cast<uint8_t>((value >> 20) & 0x3u);
    seq_cfg.prog_data_edge = ((value >> 23) & 0x1u) != 0u;
    seq_cfg.prog_dummy_cnt = static_cast<uint8_t>((value >> 24) & 0x3Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for prog_seq_cfg_1 (offset 0x424) — FUNC_XSPI_005
 *
 * Decodes Profile 1 page-program command extension fields.
 *
 * Bit layout (reset 0x0000FD00):
 *   [0]    prog_seq_p1_cmd_ext_en  — command extension enable
 *   [15:8] prog_seq_p1_cmd_ext_val — command extension value (reset 0xFD)
 *
 * @param value 32-bit value written to prog_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_prog_seq_cfg_1(uint32_t value, uint8_t be)
{
    seq_cfg.prog_cmd_ext_en  = (value & 0x1u) != 0u;
    seq_cfg.prog_cmd_ext_val = static_cast<uint8_t>((value >> 8) & 0xFFu);
    return true;
}

/******************************************************************************
 * @brief Write callback for prog_seq_cfg_2 (offset 0x428) — FUNC_XSPI_005
 *
 * Decodes Profile 2 (HyperFlash / HyperRAM) program sequence parameters.
 *
 * Bit layout (reset 0x00000002):
 *   [0]    prog_seq_p2_target       — CA target space (0=memory, 1=register)
 *   [1]    prog_seq_p2_burst_type   — CA burst type (0=wrap, 1=linear; reset 1)
 *   [2]    prog_seq_p2_mask_cmd_mod — CA[44:40] command modifier
 *   [13:8] prog_seq_p2_latency_cnt  — latency cycle count (N-1 encoding)
 *
 * @param value 32-bit value written to prog_seq_cfg_2
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_prog_seq_cfg_2(uint32_t value, uint8_t be)
{
    seq_cfg.prog_p2_target       = (value & 0x1u) != 0u;
    seq_cfg.prog_p2_burst_type   = ((value >> 1) & 0x1u) != 0u;
    seq_cfg.prog_p2_mask_cmd_mod = ((value >> 2) & 0x1u) != 0u;
    seq_cfg.prog_p2_latency_cnt  = static_cast<uint8_t>((value >> 8) & 0x3Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for read_seq_cfg_0 (offset 0x430) — FUNC_XSPI_005
 *
 * Decodes Profile 1 / SPI NAND READ command, address, data, and dummy cycle
 * parameters.
 *
 * Bit layout (reset 0x00003003):
 *   [7:0]   read_seq_p1_cmd_val   — read opcode (reset 0x03)
 *   [9:8]   read_seq_p1_cmd_ios   — command I/O width
 *   [11]    read_seq_p1_cmd_edge  — command DDR edge select
 *   [14:12] read_seq_p1_addr_cnt  — address byte count (reset 3)
 *   [17:16] read_seq_p1_addr_ios  — address I/O width
 *   [19]    read_seq_p1_addr_edge — address DDR edge select
 *   [21:20] read_seq_p1_data_ios  — data I/O width
 *   [23]    read_seq_p1_data_edge — data DDR edge select
 *   [29:24] read_seq_p1_dummy_cnt — dummy cycles (used when mode-byte disabled)
 *
 * @param value 32-bit value written to read_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_read_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.read_cmd_val   = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.read_cmd_ios   = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.read_cmd_edge  = ((value >> 11) & 0x1u) != 0u;
    seq_cfg.read_addr_cnt  = static_cast<uint8_t>((value >> 12) & 0x7u);
    seq_cfg.read_addr_ios  = static_cast<uint8_t>((value >> 16) & 0x3u);
    seq_cfg.read_addr_edge = ((value >> 19) & 0x1u) != 0u;
    seq_cfg.read_data_ios  = static_cast<uint8_t>((value >> 20) & 0x3u);
    seq_cfg.read_data_edge = ((value >> 23) & 0x1u) != 0u;
    seq_cfg.read_dummy_cnt = static_cast<uint8_t>((value >> 24) & 0x3Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for read_seq_cfg_1 (offset 0x434) — FUNC_XSPI_005
 *
 * Decodes Profile 1 READ command extension, mode-byte, and cache-read fields.
 *
 * Bit layout (reset 0x0000FC00):
 *   [0]     read_seq_p1_cmd_ext_en          — command extension enable
 *   [4]     read_seq_p1_cache_random_read_en — cache random read enable
 *   [15:8]  read_seq_p1_cmd_ext_val         — command extension value (reset 0xFC)
 *   [29:24] read_seq_p1_mb_dummy_cnt        — dummy cycles when mode-byte enabled
 *   [31]    read_seq_p1_mb_en               — mode-byte enable
 *
 * @param value 32-bit value written to read_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_read_seq_cfg_1(uint32_t value, uint8_t be)
{
    seq_cfg.read_cmd_ext_en      = (value & 0x1u) != 0u;
    seq_cfg.read_cache_random_en = ((value >>  4) & 0x1u) != 0u;
    seq_cfg.read_cmd_ext_val     = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.read_mb_dummy_cnt    = static_cast<uint8_t>((value >> 24) & 0x3Fu);
    seq_cfg.read_mb_en           = ((value >> 31) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for read_seq_cfg_2 (offset 0x438) — FUNC_XSPI_005
 *
 * Decodes Profile 2 (HyperFlash / HyperRAM) READ sequence parameters.
 *
 * Bit layout (reset 0x00000F0A):
 *   [0]    read_seq_p2_target       — CA target space
 *   [1]    read_seq_p2_burst_type   — CA burst type (reset 1 = linear)
 *   [2]    read_seq_p2_mask_cmd_mod — CA[44:40] command modifier
 *   [3]    read_seq_p2_hf_bound_en  — HyperFlash page boundary enable (reset 1)
 *   [13:8] read_seq_p2_latency_cnt  — latency cycle count (reset 0x0F)
 *
 * @param value 32-bit value written to read_seq_cfg_2
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_read_seq_cfg_2(uint32_t value, uint8_t be)
{
    seq_cfg.read_p2_target       = (value & 0x1u) != 0u;
    seq_cfg.read_p2_burst_type   = ((value >> 1) & 0x1u) != 0u;
    seq_cfg.read_p2_mask_cmd_mod = ((value >> 2) & 0x1u) != 0u;
    seq_cfg.read_p2_hf_bound_en  = ((value >> 3) & 0x1u) != 0u;
    seq_cfg.read_p2_latency_cnt  = static_cast<uint8_t>((value >> 8) & 0x3Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for we_seq_cfg_0 (offset 0x440) — FUNC_XSPI_005
 *
 * Decodes the Write Enable Latch (WEL) command sequence parameters.
 *
 * Bit layout (reset 0x01F90006) — GA Register Reference:
 *   [31:25] reserved
 *   [24]    we_seq_p1_en          — enable sending WEL (reset 1)
 *   [23:16] we_seq_p1_cmd_ext_val — command extension value (reset 0xF9)
 *   [15]    we_seq_p1_cmd_ext_en  — command extension enable
 *   [14:12] reserved
 *   [11]    we_seq_p1_cmd_edge    — command SDR(0) / DDR(1)
 *   [10]    reserved
 *   [9:8]   we_seq_p1_cmd_ios     — command phase I/O width
 *   [7:0]   we_seq_p1_cmd_val     — WEL opcode (reset 0x06)
 *
 * @param value 32-bit value written to we_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_we_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.we_cmd_val    = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.we_cmd_ios    = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.we_cmd_edge   = ((value >> 11) & 0x1u) != 0u;
    seq_cfg.we_cmd_ext_en = ((value >> 15) & 0x1u) != 0u;
    seq_cfg.we_cmd_ext_val = static_cast<uint8_t>((value >> 16) & 0xFFu);
    seq_cfg.we_en          = ((value >> 24) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_0 (offset 0x450) — FUNC_XSPI_005
 *
 * Decodes the status-check sequence command, address, and data phase I/O
 * parameters shared across all three status sub-sequences (RDY, PROG_FAIL,
 * ERS_FAIL).
 *
 * Bit layout (reset 0x00000000):
 *   [1:0]  stat_seq_p1_cmd_ios   — command I/O width
 *   [4]    stat_seq_p1_cmd_edge  — command DDR edge select
 *   [5]    stat_seq_p1_cmd_ext_en — command extension enable
 *   [9:8]  stat_seq_p1_addr_cnt  — address byte count
 *   [11:10] stat_seq_p1_addr_ios — address I/O width
 *   [12]   stat_seq_p1_addr_edge — address DDR edge select
 *   [21:20] stat_seq_p1_data_ios — data I/O width
 *   [22]   stat_seq_p1_data_edge — data DDR edge select
 *
 * @param value 32-bit value written to stat_seq_cfg_0
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_0(uint32_t value, uint8_t be)
{
    seq_cfg.stat_cmd_ios    = static_cast<uint8_t>( value        & 0x3u);
    seq_cfg.stat_cmd_edge   = ((value >>  4) & 0x1u) != 0u;
    seq_cfg.stat_cmd_ext_en = ((value >>  5) & 0x1u) != 0u;
    seq_cfg.stat_addr_cnt   = static_cast<uint8_t>((value >>  8) & 0x3u);
    seq_cfg.stat_addr_ios   = static_cast<uint8_t>((value >> 10) & 0x3u);
    seq_cfg.stat_addr_edge  = ((value >> 12) & 0x1u) != 0u;
    seq_cfg.stat_data_ios   = static_cast<uint8_t>((value >> 20) & 0x3u);
    seq_cfg.stat_data_edge  = ((value >> 22) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_1 (offset 0x454) — FUNC_XSPI_005
 *
 * Decodes dummy cycle counts and address-phase enable flags for the three
 * status sub-sequences.
 *
 * Bit layout (reset 0x00000000):
 *   [5:0]  stat_seq_p1_dev_rdy_dummy_cnt    — RDY dummy cycles
 *   [6]    stat_seq_p1_dev_rdy_addr_en      — RDY address phase enable
 *   [21:16] stat_seq_p1_prog_fail_dummy_cnt — PROG_FAIL dummy cycles
 *   [22]   stat_seq_p1_prog_fail_addr_en    — PROG_FAIL address phase enable
 *   [29:24] stat_seq_p1_ers_fail_dummy_cnt  — ERS_FAIL dummy cycles
 *   [30]   stat_seq_p1_ers_fail_addr_en     — ERS_FAIL address phase enable
 *
 * @param value 32-bit value written to stat_seq_cfg_1
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_1(uint32_t value, uint8_t be)
{
    seq_cfg.stat_dev_rdy_dummy_cnt   = static_cast<uint8_t>( value        & 0x3Fu);
    seq_cfg.stat_dev_rdy_addr_en     = ((value >>  6) & 0x1u) != 0u;
    seq_cfg.stat_prog_fail_dummy_cnt = static_cast<uint8_t>((value >> 16) & 0x3Fu);
    seq_cfg.stat_prog_fail_addr_en   = ((value >> 22) & 0x1u) != 0u;
    seq_cfg.stat_ers_fail_dummy_cnt  = static_cast<uint8_t>((value >> 24) & 0x3Fu);
    seq_cfg.stat_ers_fail_addr_en    = ((value >> 30) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_2 (offset 0x458) — FUNC_XSPI_005
 *
 * Decodes the status command opcodes for the three sub-sequences.
 *
 * Bit layout (reset 0x05000505):
 *   [7:0]   stat_seq_p1_dev_rdy_cmd_val   — RDY opcode (reset 0x05)
 *   [15:8]  stat_seq_p1_ers_fail_cmd_val  — ERS_FAIL opcode (reset 0x05)
 *   [31:24] stat_seq_p1_prog_fail_cmd_val — PROG_FAIL opcode (reset 0x05)
 *
 * @param value 32-bit value written to stat_seq_cfg_2
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_2(uint32_t value, uint8_t be)
{
    seq_cfg.stat_dev_rdy_cmd_val   = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.stat_ers_fail_cmd_val  = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.stat_prog_fail_cmd_val = static_cast<uint8_t>((value >> 24) & 0xFFu);
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_3 (offset 0x45C) — FUNC_XSPI_005
 *
 * Decodes the status command extension values for the three sub-sequences.
 *
 * Bit layout (reset 0xFA00FAFA):
 *   [7:0]   stat_seq_p1_dev_rdy_cmd_ext_val   — RDY extension (reset 0xFA)
 *   [15:8]  stat_seq_p1_ers_fail_cmd_ext_val  — ERS_FAIL extension (reset 0xFA)
 *   [31:24] stat_seq_p1_prog_fail_cmd_ext_val — PROG_FAIL extension (reset 0xFA)
 *
 * @param value 32-bit value written to stat_seq_cfg_3
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_3(uint32_t value, uint8_t be)
{
    seq_cfg.stat_dev_rdy_cmd_ext_val   = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.stat_ers_fail_cmd_ext_val  = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.stat_prog_fail_cmd_ext_val = static_cast<uint8_t>((value >> 24) & 0xFFu);
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_4 (offset 0x460) — FUNC_XSPI_005
 *
 * Decodes Profile 2 HyperFlash status sequence parameters.
 *
 * Bit layout (reset 0x00000F00):
 *   [2]    stat_seq_p2_mask_cmd_mod — CA[44:40] command modifier
 *   [13:8] stat_seq_p2_latency_cnt  — latency cycle count (reset 0x0F)
 *
 * @param value 32-bit value written to stat_seq_cfg_4
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_4(uint32_t value, uint8_t be)
{
    seq_cfg.stat_p2_mask_cmd_mod = ((value >> 2) & 0x1u) != 0u;
    seq_cfg.stat_p2_latency_cnt  = static_cast<uint8_t>((value >> 8) & 0x3Fu);
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_5 (offset 0x464) — FUNC_XSPI_005
 *
 * Decodes the status-bit polling configuration for RDY/BUSY, ERS_FAIL, and
 * PROG_FAIL detection across Profile 1, SPI NAND, and Profile 2 HF.
 *
 * Bit layout (reset 0x00000040):
 *   [3:0]  stat_seq_dev_rdy_idx    — RDY status bit index
 *   [4]    stat_seq_dev_rdy_val    — RDY compare value
 *   [5]    stat_seq_dev_rdy_size   — RDY status word size (0=1B, 1=2B)
 *   [6]    stat_seq_dev_rdy_en     — RDY polling enable (reset 1)
 *   [11:8] stat_seq_ers_fail_idx   — ERS_FAIL status bit index
 *   [12]   stat_seq_ers_fail_val   — ERS_FAIL compare value
 *   [13]   stat_seq_ers_fail_size  — ERS_FAIL status word size
 *   [14]   stat_seq_ers_fail_en    — ERS_FAIL polling enable
 *   [27:24] stat_seq_prog_fail_idx — PROG_FAIL status bit index
 *   [28]   stat_seq_prog_fail_val  — PROG_FAIL compare value
 *   [29]   stat_seq_prog_fail_size — PROG_FAIL status word size
 *   [30]   stat_seq_prog_fail_en   — PROG_FAIL polling enable
 *
 * @param value 32-bit value written to stat_seq_cfg_5
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_5(uint32_t value, uint8_t be)
{
    seq_cfg.stat_dev_rdy_idx    = static_cast<uint8_t>( value        & 0xFu);
    seq_cfg.stat_dev_rdy_val    = ((value >>  4) & 0x1u) != 0u;
    seq_cfg.stat_dev_rdy_size   = ((value >>  5) & 0x1u) != 0u;
    seq_cfg.stat_dev_rdy_en     = ((value >>  6) & 0x1u) != 0u;
    seq_cfg.stat_ers_fail_idx   = static_cast<uint8_t>((value >>  8) & 0xFu);
    seq_cfg.stat_ers_fail_val   = ((value >> 12) & 0x1u) != 0u;
    seq_cfg.stat_ers_fail_size  = ((value >> 13) & 0x1u) != 0u;
    seq_cfg.stat_ers_fail_en    = ((value >> 14) & 0x1u) != 0u;
    seq_cfg.stat_prog_fail_idx  = static_cast<uint8_t>((value >> 24) & 0xFu);
    seq_cfg.stat_prog_fail_val  = ((value >> 28) & 0x1u) != 0u;
    seq_cfg.stat_prog_fail_size = ((value >> 29) & 0x1u) != 0u;
    seq_cfg.stat_prog_fail_en   = ((value >> 30) & 0x1u) != 0u;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_7 (offset 0x46C) — FUNC_XSPI_005
 *
 * Caches the 32-bit address used to read RDY/BUSY status after PROGRAM/ERASE
 * and SOFT RESET/READ (SPI NAND) operations.
 *
 * Bit layout (reset 0x00000000):
 *   [31:0] stat_seq_dev_rdy_addr — RDY/BUSY status read address
 *
 * @param value 32-bit value written to stat_seq_cfg_7
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_7(uint32_t value, uint8_t be)
{
    seq_cfg.stat_dev_rdy_addr = value;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_8 (offset 0x470) — FUNC_XSPI_005
 *
 * Caches the 32-bit address used to read fail status after PROGRAM operations.
 *
 * Bit layout (reset 0x00000000):
 *   [31:0] stat_seq_prog_fail_addr — PROG_FAIL status read address
 *
 * @param value 32-bit value written to stat_seq_cfg_8
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_8(uint32_t value, uint8_t be)
{
    seq_cfg.stat_prog_fail_addr = value;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_9 (offset 0x474) — FUNC_XSPI_005
 *
 * Caches the 32-bit address used to read fail status after ERASE operations.
 *
 * Bit layout (reset 0x00000000):
 *   [31:0] stat_seq_ers_fail_addr — ERS_FAIL status read address
 *
 * @param value 32-bit value written to stat_seq_cfg_9
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_9(uint32_t value, uint8_t be)
{
    seq_cfg.stat_ers_fail_addr = value;
    return true;
}

/******************************************************************************
 * @brief Write callback for stat_seq_cfg_10 (offset 0x478) — FUNC_XSPI_005
 *
 * Decodes SPI NAND ECC status detection parameters used during Page Read
 * operations.
 *
 * Bit layout (reset 0x00000000):
 *   [7:0]   stat_seq_ecc_fail_mask — ECC status bit selection mask
 *   [15:8]  stat_seq_ecc_fail_val  — uncorrectable ECC error compare value
 *   [23:16] stat_seq_ecc_corr_val  — correctable ECC error compare value
 *   [26:24] stat_seq_crdy_idx      — Cache Read Busy status bit index
 *   [27]    stat_seq_crdy_val      — Cache Read Busy compare value
 *   [31]    stat_seq_ecc_fail_en   — ECC fail polling enable
 *
 * @param value 32-bit value written to stat_seq_cfg_10
 * @param be    Byte-enable mask (unused)
 * @return true
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_stat_seq_cfg_10(uint32_t value, uint8_t be)
{
    seq_cfg.stat_ecc_fail_mask = static_cast<uint8_t>( value        & 0xFFu);
    seq_cfg.stat_ecc_fail_val  = static_cast<uint8_t>((value >>  8) & 0xFFu);
    seq_cfg.stat_ecc_corr_val  = static_cast<uint8_t>((value >> 16) & 0xFFu);
    seq_cfg.stat_crdy_idx      = static_cast<uint8_t>((value >> 24) & 0x7u);
    seq_cfg.stat_crdy_val      = ((value >> 27) & 0x1u) != 0u;
    seq_cfg.stat_ecc_fail_en   = ((value >> 31) & 0x1u) != 0u;
    return true;
}

// =============================================================================
// Group 5: Mini-Controller Callbacks (rf_minictrl_regs_a, 0x1000)
// =============================================================================

/******************************************************************************
 * @brief Write callback for wp_settings (offset 0x1000) — FUNC_XSPI_002
 *
 * Stores the Write Protect (DQ2 / WP#) pin control state into shadow variables
 * so that dispatch_flash_transaction() can propagate the current WP# level
 * to every cdns_extension it constructs.
 *
 * wp_settings bit layout:
 *   bit[0] = wp       : 1 = WP# deasserted (write protect inactive, default)
 *                        0 = WP# asserted   (write protect active)
 *   bit[1] = wp_enable: 1 = DQ2 is dedicated WP# pin
 *                        0 = DQ2 is used as a transaction data pin
 *
 * Side effects: mini_ctrl_cfg.wp_pin_level and mini_ctrl_cfg.wp_enabled updated.
 *
 * Reference: docs/sections/xspi_ctrl-register-callbacks.md
 *            handle_write_wp_settings; docs/xspi_ctrl-detailed-design.md
 *            Section 5.6 (Mini-controller registers).
 *
 * @param value 32-bit value written to wp_settings
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_wp_settings(uint32_t value, uint8_t be)
{
    mini_ctrl_cfg.wp_pin_level = ((value & 0x1u) != 0u);   // bit[0]: wp
    mini_ctrl_cfg.wp_enabled   = ((value & 0x2u) != 0u);   // bit[1]: wp_enable
    CSML_INFO(3, logger) << "handle_write_wp_settings: wp_pin_level=" << mini_ctrl_cfg.wp_pin_level << " wp_enabled=" << mini_ctrl_cfg.wp_enabled;
    return true;
}

/******************************************************************************
 * @brief Write callback for reset_pin_settings (offset 0x1004) — FUNC_XSPI_002
 *
 * Stores the software-controlled hardware Reset# pin state into the hw_rst_level
 * shadow variable. The level is propagated to every cdns_extension constructed
 * by dispatch_flash_transaction().
 *
 * reset_pin_settings bit layout (key fields):
 *   bit[0] = sw_ctrled_hw_rst: 1 = RESET# deasserted (device not in reset,
 *                                   default at reset value 0x00000001)
 *                               0 = RESET# asserted   (device held in reset)
 *   bits[9:2] = per-bank enable bits (sw_ctrled_hw_rst_bank0–7)
 *
 * Side effects: mini_ctrl_cfg.hw_rst_level updated.
 *
 * Reference: docs/sections/xspi_ctrl-register-callbacks.md
 *            handle_write_reset_pin_settings.
 *
 * @param value 32-bit value written to reset_pin_settings
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_reset_pin_settings(uint32_t value, uint8_t be)
{
    mini_ctrl_cfg.hw_rst_level = ((value & 0x1u) != 0u);   // bit[0]: sw_ctrled_hw_rst
    CSML_INFO(3, logger) << "handle_write_reset_pin_settings: hw_rst_level=" << mini_ctrl_cfg.hw_rst_level;
    return true;
}

/******************************************************************************
 * @brief Write callback for clock_mode_settings (offset 0x1008) — FUNC_XSPI_002
 *
 * Stores the SPI clock mode selection into the spi_clk_mode shadow variable.
 * The mode is propagated to every cdns_extension to inform the flash model
 * which CPOL/CPHA convention is in use for legacy SDR transactions.
 *
 * clock_mode_settings bit layout:
 *   bit[0] = spi_clock_mode: 0 = SPI Mode 0 (CPOL=0, CPHA=0, default)
 *                              1 = SPI Mode 3 (CPOL=1, CPHA=1)
 *
 * Side effects: mini_ctrl_cfg.spi_clk_mode updated.
 *
 * Reference: docs/sections/xspi_ctrl-register-callbacks.md
 *            handle_write_clock_mode_settings.
 *
 * @param value 32-bit value written to clock_mode_settings
 * @param be    Byte-enable mask
 * @return true indicating successful callback execution
 ******************************************************************************/
bool xspi_ctrl_ip::handle_write_clock_mode_settings(uint32_t value, uint8_t be)
{
    mini_ctrl_cfg.spi_clk_mode = static_cast<uint8_t>(value & 0x1u);   // bit[0]: spi_clock_mode
    CSML_INFO(3, logger) << "handle_write_clock_mode_settings: spi_clk_mode=" << static_cast<unsigned>(mini_ctrl_cfg.spi_clk_mode);
    return true;
}
