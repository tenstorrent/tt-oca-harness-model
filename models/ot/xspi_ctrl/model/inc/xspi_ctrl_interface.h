/******************************************************************************
 * @file xspi_ctrl_interface.h
 * @brief Abstract interface definitions for xspi_ctrl SystemC TLM register
 *        callbacks
 *
 * This file defines the pure-virtual interface class xspi_ctrl_if for the
 * Cadence XSPI Controller (IP6522 + IP6182 Soft PHY) TLM model. It declares
 * every register callback method required by the xspi_ctrl functional model,
 * covering all 55 write callbacks and 1 read callback across the six register
 * sub-regions that carry behavioral side effects.
 *
 * The separation between this interface class and the concrete xspi_ctrl model
 * class ensures that the test harness and higher-level integration environments
 * can substitute or mock register-level behavior without depending on
 * implementation details.
 *
 * Reference:
 *   - docs/sections/xspi_ctrl-register-callbacks.md
 *   - docs/xspi_ctrl-detailed-design.md Section 6
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include <cstdint>

/******************************************************************************
 * @class xspi_ctrl_if
 * @brief Abstract interface declaring all register callback methods for the
 *        xspi_ctrl TLM model
 *
 * All methods are pure virtual. The xspi_ctrl model class inherits from this
 * interface and provides concrete implementations. Test stubs or mock objects
 * may also inherit from this interface.
 *
 * Callback naming convention: handle_write_<REG_NAME> / handle_read_<REG_NAME>
 * where REG_NAME matches the scml2 register declaration name exactly.
 ******************************************************************************/
class xspi_ctrl_if
{
public:
    /// @brief Virtual destructor
    virtual ~xspi_ctrl_if() = default;

    // =========================================================================
    // Group 1: Command and Status Registers (ctrl_cmd_stat_a, base = 0x000)
    // =========================================================================

    /**
     * @brief Write callback for cmd_reg0 (offset 0x000) — primary dispatch
     *        trigger
     *
     * Mode-dependent dispatch. Reads current_work_mode and branches:
     * STIG fires cmd_trigger_event; PIO calls pio_handle_trigger(); ACMD
     * calls cdma_handle_trigger(). Sets intr_status.cmd_ignored if the
     * target thread is already busy.
     *
     * @param value 32-bit value written to cmd_reg0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_cmd_reg0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for cmd_status_ptr (offset 0x040)
     *
     * Updates the internal thread-selector variable thrd_status_sel,
     * clamped to [0, MAX_THREADS-1]. Controls which thread's status is
     * returned on the next read of cmd_status.
     *
     * @param value 32-bit value written to cmd_status_ptr
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_cmd_status_ptr(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Read callback for cmd_status (offset 0x044)
     *
     * Volatile indirect read. Returns live thread_status_[thrd_status_sel]
     * for ACMD/PIO modes, or STIG engine status for STIG mode. The stored
     * register value is stale; the live thread-status array is always
     * consulted.
     *
     * @param[out] value Reference to receive the dynamic register value
     * @return true on successful callback execution
     */
    virtual bool handle_read_cmd_status(uint32_t& value) = 0;

    /**
     * @brief Write callback for intr_status (offset 0x110) — W1C
     *
     * Clears interrupt status bits written as 1 via scml2 set_clear_on_write_1.
     * After clearing, re-evaluates interrupt_out gated by intr_enable.
     * Note: intr_status does not directly drive int_out; only
     * trd_comp_intr_status and trd_error_intr_status drive int_out.
     *
     * @param value 32-bit mask written to intr_status (bits set are cleared)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_intr_status(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for intr_enable (offset 0x114)
     *
     * Updating the per-bit interrupt enable mask requires immediate
     * re-evaluation of interrupt_out. Enables or disables individual
     * interrupt source contributions to int_out.
     *
     * @param value 32-bit value written to intr_enable
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_intr_enable(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for trd_comp_intr_status (offset 0x120) — W1C
     *
     * Clears thread completion interrupt bits[7:0] written as 1. After
     * clearing, re-evaluates int_out: if all masked bits are zero and
     * trd_error_intr_status is also zero, de-asserts int_out.
     *
     * @param value 32-bit mask written (bits set clear corresponding thread
     *              completion interrupt flags)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_trd_comp_intr_status(uint32_t value,
                                                   uint8_t be) = 0;

    /**
     * @brief Write callback for trd_error_intr_status (offset 0x130) — W1C
     *
     * Clears thread error interrupt bits[7:0] written as 1. After clearing,
     * re-evaluates int_out with the same logic as handle_write_trd_comp_intr_status.
     *
     * @param value 32-bit mask written (bits set clear corresponding thread
     *              error interrupt flags)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_trd_error_intr_status(uint32_t value,
                                                    uint8_t be) = 0;

    /**
     * @brief Write callback for trd_error_intr_en (offset 0x134)
     *
     * Updating the per-thread error interrupt enable mask requires immediate
     * re-evaluation of int_out. Enables or disables individual thread error
     * interrupt contributions to int_out.
     *
     * @param value 32-bit value written to trd_error_intr_en
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_trd_error_intr_en(uint32_t value,
                                                uint8_t be) = 0;

    // =========================================================================
    // Group 2: Controller Configuration Registers (ctrl_cfg_common_a,
    //          base = 0x200)
    // =========================================================================

    /**
     * @brief Write callback for long_polling (offset 0x208)
     *
     * Captures new long-polling wait count into model-internal long_polling_val.
     * In LT model the polling loop is not cycle-accurate; value is used in
     * flash status-check sequences. Reset value 1000 (0x3E8).
     *
     * @param value 32-bit value written to long_polling
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_long_polling(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for short_polling (offset 0x20C)
     *
     * Captures new short-polling cycle count into short_polling_val. Same
     * LT abstraction as long_polling. Reset value 500 (0x1F4).
     *
     * @param value 32-bit value written to short_polling
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_short_polling(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for ctrl_config (offset 0x230) — operating mode
     *        switch
     *
     * Updates current_work_mode from ctrl_config.work_mode bits [6:5].
     * Encoding: 2'b00=DIRECT, 2'b01=STIG, 2'b10=reserved, 2'b11=ACMD
     * (PIO: cmd_reg0[31:30]=2'b01; CDMA: cmd_reg0[31:30]=2'b00).
     * Also applies cont_on_err flag to ACMD/PIO error-recovery policy.
     * Must complete before any cmd_reg0 write is processed.
     *
     * @param value 32-bit value written to ctrl_config
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_ctrl_config(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for dma_settings (offset 0x23C)
     *
     * Updates AXI master interface live parameters: burst_sel (max burst
     * length = burst_sel+1), OTE (outstanding transaction enable), word_size
     * (byte/16-bit/32-bit/64-bit), sdma_err_rsp (AXI slave error response
     * type). Applied to i_dma_socket on the next DMA call. Reset value
     * 0x000D0000.
     *
     * @param value 32-bit value written to dma_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_dma_settings(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for discovery_control (offset 0x260)
     *
     * Captures software-programmed discovery parameters: discovery_extop_en,
     * discovery_extop_val, discovery_cmd_type, discovery_dummy_cnt,
     * discovery_abnum. Read-only sub-fields (discovery_comp, discovery_fail,
     * discovery_inhibit) are write-ignored via set_write_ignore_restriction.
     *
     * @param value 32-bit value written to discovery_control
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_discovery_control(uint32_t value,
                                                uint8_t be) = 0;

    // =========================================================================
    // Group 3: Common Sequence Configuration Registers (cmn_seq_regs_a,
    //          base = 0x380)
    // =========================================================================

    /**
     * @brief Write callback for xip_mode_cfg (offset 0x388)
     *
     * Arms XIP entry sequence by setting xip_active_banks bitmask per the
     * written xip_en field. On the next READ to an XIP-armed bank, the model
     * inserts xip_en_mb_val as a mode byte. Updates xip_dis_mb_val. Enforces
     * that only READ sequences are valid while XIP is active.
     *
     * @param value 32-bit value written to xip_mode_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_xip_mode_cfg(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for global_seq_cfg (offset 0x390)
     *
     * Updates active_device_profile from seq_type (PROFILE 1 xSPI/NOR,
     * PROFILE 2 HyperFlash, SPI NAND). Updates read_page_size and
     * program_page_size bounds. Sets tcms_enabled flag from seq_tcms_en.
     * Auto-populated by SFDP discovery at PoR.
     *
     * @param value 32-bit value written to global_seq_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_global_seq_cfg(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for global_seq_cfg_1 (offset 0x394)
     *
     * Updates nand_spare_area parameter that extends page data byte counts
     * for SPI NAND READ PAGE and PROGRAM PAGE operations in ACMD and PIO
     * modes. Auto-populated by SFDP discovery.
     *
     * @param value 32-bit value written to global_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_global_seq_cfg_1(uint32_t value,
                                               uint8_t be) = 0;

    /**
     * @brief Write callback for direct_access_cfg (offset 0x398)
     *
     * Updates active_dac_bank from dac_bank_num[2:0], selecting the
     * xspi_bus_socket[N] target for all Direct-mode transactions. Sets
     * rmp_addr_en flag. Sets xip_exit_pending flag when mode_bit_xip_dis
     * is written. Updates rwds_cap_en for octal DDR mode.
     *
     * @param value 32-bit value written to direct_access_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_direct_access_cfg(uint32_t value,
                                                uint8_t be) = 0;

    /**
     * @brief Write callback for direct_access_rmp (offset 0x39C)
     *
     * Updates remap_offset[31:0]: the lower 32 bits of the 64-bit
     * subtraction operand N used in Direct-mode address remapping. When
     * rmp_addr_en is set, the flash address = AXI_address minus N.
     *
     * @param value 32-bit value written to direct_access_rmp
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_direct_access_rmp(uint32_t value,
                                                uint8_t be) = 0;

    /**
     * @brief Write callback for direct_access_rmp_1 (offset 0x3A0)
     *
     * Updates remap_offset[63:32]: the upper 32 bits of the 64-bit remap
     * offset N. Paired with handle_write_direct_access_rmp to form the
     * full 64-bit subtraction operand.
     *
     * @param value 32-bit value written to direct_access_rmp_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_direct_access_rmp_1(uint32_t value,
                                                  uint8_t be) = 0;

    // =========================================================================
    // Group 4: Device Sequence Configuration Registers (dev_seq_regs_a,
    //          base = 0x400)
    // =========================================================================

    /**
     * @brief Write callback for rst_seq_cfg_0 (offset 0x400)
     *
     * Updates RESET sequence opcode and address count parameters for
     * Profile 1 soft reset and JEDEC reset sequences. Auto-populated by
     * SFDP discovery. Default reset value 0x00019966.
     *
     * @param value 32-bit value written to rst_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_rst_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for rst_seq_cfg_1 (offset 0x404)
     *
     * Updates RESET sequence extension opcode and additional profile 1
     * reset parameters. Auto-populated by SFDP discovery. Default reset
     * value 0xD0669900.
     *
     * @param value 32-bit value written to rst_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_rst_seq_cfg_1(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for ers_seq_cfg_0 (offset 0x410)
     *
     * Updates ERASE sequence parameters: SECTOR_ERASE opcode (0xD8),
     * I/O width, address count, and extension value. Auto-populated by
     * SFDP discovery. Default reset value 0x00DF3020.
     *
     * @param value 32-bit value written to ers_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_ers_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for ers_seq_cfg_1 (offset 0x414)
     *
     * Updates ERASE sequence timing parameters for Profile 1 erase
     * operations. Auto-populated by SFDP discovery. Default reset value
     * 0x0000000C.
     *
     * @param value 32-bit value written to ers_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_ers_seq_cfg_1(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for ers_seq_cfg_2 (offset 0x418)
     *
     * Updates ERASE sequence PROFILE 2 parameters: HyperFlash chip-erase
     * opcode and latency. Auto-populated by SFDP discovery. Default reset
     * value 0x009F0060.
     *
     * @param value 32-bit value written to ers_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_ers_seq_cfg_2(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for prog_seq_cfg_0 (offset 0x420)
     *
     * Updates PROGRAM sequence parameters: PAGE_PROGRAM opcode (0x02),
     * I/O width, and address count. Auto-populated by SFDP discovery.
     * Default reset value 0x00003002.
     *
     * @param value 32-bit value written to prog_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_prog_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for prog_seq_cfg_1 (offset 0x424)
     *
     * Updates PROGRAM sequence extension opcode and Profile 1 program
     * timing parameters. Auto-populated by SFDP discovery. Default reset
     * value 0x0000FD00.
     *
     * @param value 32-bit value written to prog_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_prog_seq_cfg_1(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for prog_seq_cfg_2 (offset 0x428)
     *
     * Updates PROGRAM sequence PROFILE 2 (HyperFlash) parameters: program
     * latency count and burst type configuration. Auto-populated by SFDP
     * discovery. Default reset value 0x00000002.
     *
     * @param value 32-bit value written to prog_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_prog_seq_cfg_2(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for read_seq_cfg_0 (offset 0x430)
     *
     * Updates READ sequence opcode (0x03) and 3-byte address count for
     * PROFILE 1/NAND. Auto-populated by SFDP discovery. Default reset
     * value 0x00003003.
     *
     * @param value 32-bit value written to read_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_read_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for read_seq_cfg_1 (offset 0x434)
     *
     * Updates READ sequence extension opcode (0xFB), cache random read
     * enable, and mode-byte dummy count for PROFILE 1. Auto-populated by
     * SFDP discovery. Default reset value 0x0000FC00.
     *
     * @param value 32-bit value written to read_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_read_seq_cfg_1(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for read_seq_cfg_2 (offset 0x438)
     *
     * Updates READ sequence PROFILE 2 parameters: burst type, HyperFlash
     * boundary enable, and latency count (default 15 cycles). Auto-populated
     * by SFDP discovery. Default reset value 0x00000F0A.
     *
     * @param value 32-bit value written to read_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_read_seq_cfg_2(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for we_seq_cfg_0 (offset 0x440)
     *
     * Updates WRITE ENABLE sequence: opcode (WREN 0x06), we_seq_p1_en flag
     * (when set the model automatically prefixes every PROGRAM and ERASE
     * command with a WREN transaction), and extension value (0xF9).
     * Default reset value 0x01F90006.
     *
     * @param value 32-bit value written to we_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_we_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_0 (offset 0x450)
     *
     * Updates STATUS CHECK I/O configuration for PROFILE 1: command I/O
     * width, edge mode, address count, and data I/O width. Used for flash
     * device-ready polling after PROGRAM/ERASE.
     *
     * @param value 32-bit value written to stat_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_0(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_1 (offset 0x454)
     *
     * Updates STATUS CHECK dummy count and address enable for device-ready,
     * program-fail, and erase-fail status checks (PROFILE 1).
     *
     * @param value 32-bit value written to stat_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_1(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_2 (offset 0x458)
     *
     * Updates READ_STATUS_REG opcodes (default 0x05) for device-ready,
     * erase-fail, and program-fail status polling (PROFILE 1). Default
     * reset value 0x05000505.
     *
     * @param value 32-bit value written to stat_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_2(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_3 (offset 0x45C)
     *
     * Updates STATUS CHECK extension opcode values (0xFA) for device-ready,
     * erase-fail, and program-fail sequences (PROFILE 1). Default reset
     * value 0xFA00FAFA.
     *
     * @param value 32-bit value written to stat_seq_cfg_3
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_3(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_4 (offset 0x460)
     *
     * Updates STATUS CHECK PROFILE 2 (HyperFlash) latency count (default
     * 15) and command modifier mask for stat_seq_p2_latency_cnt and
     * stat_seq_p2_mask_cmd_mod.
     *
     * @param value 32-bit value written to stat_seq_cfg_4
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_4(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_5 (offset 0x464)
     *
     * Updates STATUS CHECK bit-field mask and ready-value for PROFILE 1
     * device-ready polling: stat_seq_p1_dev_rdy_mask and
     * stat_seq_p1_dev_rdy_val.
     *
     * @param value 32-bit value written to stat_seq_cfg_5
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_5(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_7 (offset 0x46C)
     *
     * Updates STATUS CHECK parameters for SPI NAND: program-fail and
     * erase-fail bit-field mask and expected value. Note: stat_seq_cfg_6
     * at offset 0x468 is intentionally absent (storage-only, no callback).
     *
     * @param value 32-bit value written to stat_seq_cfg_7
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_7(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_8 (offset 0x470)
     *
     * Updates STATUS CHECK PROFILE 1 erase-fail detection parameters:
     * bit-field mask and expected fail-value for the erase-fail status bit.
     *
     * @param value 32-bit value written to stat_seq_cfg_8
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_8(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_9 (offset 0x474)
     *
     * Updates STATUS CHECK PROFILE 1 program-fail detection parameters:
     * bit-field mask and expected fail-value for the program-fail status bit.
     *
     * @param value 32-bit value written to stat_seq_cfg_9
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_9(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for stat_seq_cfg_10 (offset 0x478)
     *
     * Updates STATUS CHECK SPI NAND ECC error detection masks, values, and
     * cache-ready detection parameters for NAND device polling.
     *
     * @param value 32-bit value written to stat_seq_cfg_10
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_stat_seq_cfg_10(uint32_t value, uint8_t be) = 0;

    // =========================================================================
    // Group 5: Mini-Controller Registers (rf_minictrl_regs_a, base = 0x1000)
    // =========================================================================

    /**
     * @brief Write callback for wp_settings (offset 0x1000)
     *
     * Updates Write Protect pin (DQ2) control: wp value (1=WP# deasserted,
     * 0=WP# asserted) and wp_enable (gates whether DQ2 is used for WP or
     * as a data pin). Effective only in single/dual SPI legacy modes.
     *
     * @param value 32-bit value written to wp_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_wp_settings(uint32_t value, uint8_t be) = 0;

    /**
     * @brief Write callback for reset_pin_settings (offset 0x1004)
     *
     * Updates software-controlled hardware reset pin control:
     * sw_ctrled_hw_rst (RESET# pin level), sw_ctrled_hw_rst_option (RESET#
     * pin vs. DQ3), rst_dq3_enable (DQ3 direction gate), and per-bank
     * enable bits sw_ctrled_hw_rst_bank0-7.
     *
     * @param value 32-bit value written to reset_pin_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_reset_pin_settings(uint32_t value,
                                                 uint8_t be) = 0;

    /**
     * @brief Write callback for clock_mode_settings (offset 0x1008)
     *
     * Updates SPI clock mode selection: spi_clock_mode=0 selects SPI Mode 0
     * (CPOL=0, CPHA=0); spi_clock_mode=1 selects SPI Mode 3 (CPOL=1,
     * CPHA=1). Used to condition cdns_extension clock-edge encoding for
     * legacy SDR transactions.
     *
     * @param value 32-bit value written to clock_mode_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    virtual bool handle_write_clock_mode_settings(uint32_t value,
                                                  uint8_t be) = 0;
};
