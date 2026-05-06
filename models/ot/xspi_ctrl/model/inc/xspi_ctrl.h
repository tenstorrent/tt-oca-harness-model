/******************************************************************************
 * @file xspi_ctrl.h
 * @brief xspi_ctrl SystemC TLM model header
 *
 * This file defines the xspi_ctrl_ip SystemC TLM model class for the Cadence
 * XSPI Controller (IP6522 + IP6182 Soft PHY). The class inherits from
 * xspi_ctrl_base (auto-generated scml2 register file) and xspi_ctrl_if
 * (abstract callback interface), adds the seven port interfaces specified in
 * docs/sections/xspi_ctrl-port-interfaces.md, six scml_property configuration
 * parameters, two SC_THREAD/SC_METHOD processes, and stub declarations for all
 * 56 register callbacks.
 *
 * The class is named xspi_ctrl_ip (not xspi_ctrl) to avoid a name collision
 * with the 'namespace xspi_ctrl' declared in xspi_ctrl_register.h, which
 * contains the scml2 register type definitions.
 *
 * Reference:
 *   - docs/sections/xspi_ctrl-port-interfaces.md
 *   - docs/sections/xspi_ctrl-config-parameters.md
 *   - docs/sections/xspi_ctrl-register-callbacks.md
 *   - kmac/model/inc/kmac.h (structural reference)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "xspi_ctrl_base.h"
#include "xspi_ctrl_interface.h"
#include "cdns_extension.h"
#include "csml_logger.h"
#include "csml_parameter.h"

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>
#include <systemc.h>
#include <vector>
#include <string>
#include <cstdint>

/******************************************************************************
 * @class xspi_ctrl_ip
 * @brief Cadence XSPI Controller (IP6522 + IP6182) SystemC TLM model
 *
 * Top-level xspi_ctrl TLM model providing all six operating modes (Direct,
 * STIG, PIO, ACMD, XIP, Boot), PoR/SFDP discovery, AXI DMA interface, and
 * interrupt management. Inherits the scml2 register file from xspi_ctrl_base
 * and implements the xspi_ctrl_if callback interface.
 *
 * Port connectivity:
 *  - target_socket      (base) : CPU register access (scml2 ft_target_socket<32>)
 *  - t_axi_slave_socket        : AXI slave for Direct/XIP memory transactions
 *  - PoR_input_signals         : Power-on reset and SFDP discovery bootstrap
 *  - xspi_bus_socket[N]        : Per-CS xSPI flash bus (sc_vector, sized by NUM_TARGETS)
 *  - i_dma_socket              : AXI master DMA interface
 *  - reset_in                  : Active-low hardware reset input
 *  - int_out                   : Interrupt output (asserted when enabled interrupt pending)
 ******************************************************************************/
class xspi_ctrl_ip : public xspi_ctrl_base, public xspi_ctrl_if
{
public:
    SC_HAS_PROCESS(xspi_ctrl_ip);

    /**
     * @brief xspi_ctrl_ip constructor
     *
     * Initializes all port interfaces, sizes the xspi_bus_socket vector to
     * NUM_TARGETS, registers b_transport callbacks on the two target sockets,
     * declares SC_THREAD and SC_METHOD processes, and initializes all registers
     * to hardware reset values via reset_all_registers().
     *
     * @param n            SystemC module name passed to xspi_ctrl_base
     * @param memory_size  Register address space size in bytes (default 0x3000)
     * @param log_verbosity CSML log verbosity
     * @param num_targets  Number of xspi_bus_socket[] / CS lines (default 1).
     *                     Common configurations use 1–4 targets; the array is
     *                     sized exactly to this count.
     */
    xspi_ctrl_ip(sc_module_name n, unsigned int memory_size = 0x3000,
                 int log_verbosity = CSML_DEFAULT_VERBOSITY,
                 int num_targets = 1);

    /// @brief CCI-backed verbosity parameter (readable from testbench/platform)
    csml_param<int> verbosity;

    // =========================================================================
    // Port Interfaces
    // =========================================================================

    /**
     * @brief AXI slave target socket for Direct-mode and XIP memory transactions.
     *
     * Receives AXI read/write transactions from the system bus. In Direct mode
     * the address is remapped and forwarded to xspi_bus_socket[active_bank].
     * In XIP mode read transactions are served from flash using the READ
     * sequence configured in read_seq_cfg_0/1/2.
     *
     * Connected: testbench::t_axi_slave_initiator → this socket.
     */
    tlm_utils::simple_target_socket<xspi_ctrl_ip, 64> t_axi_slave_socket;

    /**
     * @brief PoR input signals target socket for power-on reset bootstrap.
     *
     * Receives the xspi_PoR_trans custom extension at power-on. Triggers SFDP
     * discovery and, if boot_available=true, the optional boot sequence.
     * boot_comp and boot_error are returned in the extension fields.
     *
     * Connected: testbench::por_initiator → this socket.
     */
    tlm_utils::simple_target_socket<xspi_ctrl_ip, 64> PoR_input_signals;

    /**
     * @brief xSPI flash bus initiator socket vector (one per chip-select).
     *
     * Sized at construction time to NUM_TARGETS. Each element is a heap-
     * allocated tagged initiator socket carrying cdns_extension transactions
     * to the flash device model for the corresponding CS.
     *
     * Connected: this socket[i] → xspi_target_sc_module::target_socket (testbench).
     */
    std::vector<tlm_utils::simple_initiator_socket<xspi_ctrl_ip, 64>*>
        xspi_bus_socket;

    /**
     * @brief AXI master DMA initiator socket.
     *
     * Issues DMA read/write transactions to system memory on behalf of the
     * ACMD/PIO DMA engines (SDMA and DDMA). Data width is controlled by
     * dma_data_width scml_property.
     *
     * Connected: this socket → testbench::dma_target_socket.
     */
    tlm_utils::simple_initiator_socket<xspi_ctrl_ip, 64> i_dma_socket;

    /// @brief Active-low reset input (falling edge triggers reset_handler SC_METHOD)
    sc_core::sc_in<bool> reset_in;

    /// @brief Interrupt output (asserted when any enabled interrupt source is pending)
    sc_core::sc_out<bool> int_out;

    // =========================================================================
    // Configuration Parameters (scml_property)
    // =========================================================================

    /**
     * @brief Number of flash chip-selects / xSPI bus socket count.
     *
     * Sizes the xspi_bus_socket vector at construction (e.g. 1–4 chip selects).
     * Default: 1.
     */
    const int NUM_TARGETS;

    /**
     * @brief Number of concurrent operation threads supported.
     *
     * Gates the TRD_STATUS bitmap width and thread-state arrays.
     * Default: 8.
     */
    const int n_threads;

    /**
     * @brief Boot mode availability flag.
     *
     * When true, the PoR sequence optionally executes the boot engine to
     * pre-load a boot payload from flash into system memory via SDMA.
     * Default: true.
     */
    const bool boot_available;

    /**
     * @brief Adaptive Safety Features availability flag.
     *
     * When true, ASF error reporting paths and CRC/ECC alert registers are
     * activated. In the LT model this flag gates ASF register callback paths.
     * Default: false.
     */
    const bool asf_available;

    /**
     * @brief DMA address bus width in bits.
     *
     * Controls whether the i_dma_socket uses 32-bit or 64-bit addressing for
     * AXI master transactions. Affects sdma_addr0/sdma_addr1 interpretation.
     * Default: 64.
     */
    const int dma_addr_width;

    /**
     * @brief DMA data bus width in bits.
     *
     * Controls the AXI data width on i_dma_socket (32 or 64 bits).
     * Affects burst size and payload packing for SDMA/DDMA transfers.
     * Default: 64.
     */
    const int dma_data_width;

private:
    // ---- STIG Slave-DMA: AXI slave window k_stig_sdma_axi_base + [0, cap) ----
    static constexpr uint64_t  k_stig_sdma_axi_base  = 0xA0000000ULL;
    static constexpr std::size_t k_stig_sdma_buf_cap  = 4096u;
    uint8_t  m_stig_sdma_data[k_stig_sdma_buf_cap];
    uint32_t m_stig_sdma_bytes_expected{0u};
    uint32_t m_stig_sdma_bytes_delivered{0u};
    bool     m_stig_sdma_await_host_write{false};
    uint32_t m_stig_sdma_read_valid_bytes{0u};
    sc_core::sc_event m_stig_sdma_host_write_done;
    void stig_sdma_begin_host_write(uint32_t n_bytes);
    void stig_sdma_publish_merged_read(uint32_t n_bytes);
    bool b_transport_sdma_stig(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // =========================================================================
    // Internal State Variables
    // =========================================================================

    /// @brief Operating mode decoded from ctrl_config.work_mode bits [6:5]
    unsigned int current_work_mode;

    /// @brief Thread status selector — index of thread whose status is returned by cmd_status
    unsigned int thrd_status_sel;

    /// @brief Shadow copy of long_polling register value for internal use
    unsigned int long_polling_val;

    /// @brief Shadow copy of short_polling register value for internal use
    unsigned int short_polling_val;

    /// @brief Currently active Direct-mode bank index (from direct_access_cfg.dac_bank_num)
    unsigned int active_device_profile;

    /// @brief TCMS (tCMS active time limit) enable flag
    bool tcms_enabled;

    // =========================================================================
    // PIO Per-Thread Status Storage (FUNC_XSPI_011)
    // =========================================================================

    /**
     * @struct pio_thread_status_t
     * @brief Per-thread completion and error status for PIO operations.
     *
     * Populated by pio_handle_trigger() at command completion. The fields
     * mirror the cmd_status register encoding defined in Table 4.17 of the
     * xSPI controller specification:
     *
     *   Bit  0 — CMD_ERROR      : invalid CMD_TYPE or dispatch failure
     *   Bit  1 — BUS_ERROR      : system bus (AXI DMA) error
     *   Bit  2 — CRC_ERROR      : CRC error (not computed in LT model)
     *   Bit  3 — DQS_ERROR      : DQS error (not computed in LT model)
     *   Bit  4 — DEVICE_ERROR   : flash device error
     *   Bit  5 — ECC_CORR_ERROR : ECC correctable error (not computed in LT model)
     *   Bits[13:6] — reserved
     *   Bit 14 — FAIL           : set on any error; always set when COMPLETE=1 and error
     *   Bit 15 — COMPLETE       : set when operation finishes (even on failure)
     *   Bits[23:16] — ECC_STAT  : ECC statistics (zero in LT model)
     *
     * Retrieved by software via indirect read: write TRD_NUM to cmd_status_ptr
     * (0x040), then read cmd_status (0x044). The handle_read_cmd_status callback
     * returns this->thread_status_[thrd_status_sel].to_reg() for the selected
     * thread, replacing the simple trd_comp/error bit extraction used before
     * FUNC_XSPI_011 was implemented.
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.cmd_status; docs/xspi_ctrl-detailed-design.md
     *            Section 7.3.5 (Completion, Status, and Interrupt Handling).
     */
    struct pio_thread_status_t {
        bool    cmd_error;       ///< Bit  0 — CMD_ERROR: unknown CMD_TYPE or fatal dispatch error
        bool    bus_error;       ///< Bit  1 — BUS_ERROR: AXI DMA transaction returned error response
        bool    crc_error;       ///< Bit  2 — CRC_ERROR: CRC mismatch (LT model: always false)
        bool    dqs_error;       ///< Bit  3 — DQS_ERROR: DQS calibration failure (LT model: always false)
        bool    device_error;    ///< Bit  4 — DEVICE_ERROR: flash device returned error (LT model: always false)
        bool    ecc_corr_error;  ///< Bit  5 — ECC_CORR_ERROR: ECC correctable error (LT model: always false)
        bool    fail;            ///< Bit 14 — FAIL: set when any error occurred
        bool    complete;        ///< Bit 15 — COMPLETE: set on operation finish (success or failure)
        uint8_t ecc_stat;        ///< Bits[23:16] — ECC statistics (zero in LT model)

        /// @brief Default constructor — all error flags clear, not complete.
        pio_thread_status_t()
            : cmd_error(false), bus_error(false), crc_error(false),
              dqs_error(false), device_error(false), ecc_corr_error(false),
              fail(false), complete(false), ecc_stat(0u)
        {}

        /**
         * @brief Encode the status struct into the 32-bit cmd_status register value.
         *
         * Packs all flag fields into a 32-bit word matching the hardware
         * register encoding for cmd_status (Table 4.17):
         *
         *   bits[5:0]  — error flags (cmd_error through ecc_corr_error)
         *   bits[13:6] — reserved (zero)
         *   bit 14     — FAIL
         *   bit 15     — COMPLETE
         *   bits[23:16] — ecc_stat
         *   bits[31:24] — reserved (zero)
         *
         * @return 32-bit packed status word suitable for cmd_status register read.
         */
        uint32_t to_reg() const {
            uint32_t val = 0u;
            if (cmd_error)      val |= (1u << 0);
            if (bus_error)      val |= (1u << 1);
            if (crc_error)      val |= (1u << 2);
            if (dqs_error)      val |= (1u << 3);
            if (device_error)   val |= (1u << 4);
            if (ecc_corr_error) val |= (1u << 5);
            // bits[13:6] reserved — always zero
            if (fail)           val |= (1u << 14);
            if (complete)       val |= (1u << 15);
            val |= (static_cast<uint32_t>(ecc_stat) << 16);
            return val;
        }
    };

    /**
     * @brief Per-thread status storage array — indexed by TRD_NUM (0–7).
     *
     * Each element holds the final completion status for the corresponding PIO
     * thread (also used for ACMD threads sharing the same cmd_status read path).
     * Written by pio_handle_trigger() at command completion. Read by
     * handle_read_cmd_status() via the thrd_status_sel indirection.
     *
     * Initialized to default-constructed pio_thread_status_t (all flags clear)
     * at construction time and restored to default on reset by reset_handler().
     *
     * Maximum 8 threads matching TRD_NUM field width [26:24] in cmd_reg0.
     *
     * Reference: docs/xspi_ctrl-detailed-design.md Section 7.3.5;
     *            docs/xspi_ctrl-architecture-behaviour-map.json
     *            state_machines.PIO.completion.
     */
    static constexpr unsigned int PIO_MAX_THREADS = 8u;
    pio_thread_status_t thread_status_[PIO_MAX_THREADS];

    // =========================================================================
    // Mini-Controller Configuration Shadow Struct (FUNC_XSPI_002)
    // =========================================================================

    /**
     * @struct mini_ctrl_cfg_t
     * @brief Groups the four mini-controller pin/clock shadow variables decoded
     *        from the rf_minictrl_regs_a register group.
     *
     * All four fields are updated by the corresponding write callbacks and
     * consumed by the flash transaction helpers to populate cdns_extension
     * fields before every xspi_bus_socket b_transport call.
     *
     * Reset defaults match the hardware reset values:
     *   wp_pin_level  = true  (wp=1; WP# deasserted)
     *   wp_enabled    = false (wp_enable=0; DQ2 used as data pin)
     *   hw_rst_level  = true  (sw_ctrled_hw_rst=1; RESET# deasserted)
     *   spi_clk_mode  = 0     (spi_clock_mode=0; SPI Mode 0)
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.wp_settings, reset_pin_settings,
     *            clock_mode_settings;
     *            docs/xspi_ctrl-detailed-design.md Section 1.x (FUNC_XSPI_002).
     */
    struct mini_ctrl_cfg_t {
        bool    wp_pin_level;    ///< WP# deasserted (true=inactive); from wp_settings.wp bit[0]
        bool    wp_enabled;      ///< DQ2 used as WP# pin; from wp_settings.wp_enable bit[1]
        bool    hw_rst_level;    ///< RESET# deasserted (true=inactive); from reset_pin_settings.sw_ctrled_hw_rst bit[0]
        uint8_t spi_clk_mode;    ///< 0=Mode0 (CPOL=0/CPHA=0), 1=Mode3 (CPOL=1/CPHA=1); from clock_mode_settings.spi_clock_mode bit[0]
    };

    /// @brief Mini-controller configuration shadow, updated by handle_write_wp_settings(),
    ///        handle_write_reset_pin_settings(), and handle_write_clock_mode_settings().
    mini_ctrl_cfg_t mini_ctrl_cfg;

    // =========================================================================
    // DMA Configuration Shadow Struct (FUNC_XSPI_004)
    // =========================================================================

    /**
     * @struct dma_config_t
     * @brief Groups the four DMA configuration shadow variables decoded from
     *        the dma_settings register (offset 0x23C).
     *
     * All four fields are updated atomically by handle_write_dma_settings()
     * and applied to all subsequent i_dma_socket transactions.
     *
     * Reset defaults match dma_settings reset value 0x000D0000:
     *   burst_length  = 1      (burst_sel[7:0] = 0x00 → burst_sel+1 = 1)
     *   word_size     = 0x3    (bits[19:18] = 0b11 → 64-bit)
     *   ote_enabled   = true   (OTE bit[16] = 1)
     *   sdma_err_rsp  = false  (sdma_err_rsp bit[17] = 0)
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.dma_settings;
     *            docs/xspi_ctrl-detailed-design.md Section 10.3.
     */
    struct dma_config_t {
        uint32_t burst_length;   ///< AXI burst length (burst_sel+1 beats); from dma_settings.burst_sel[7:0]
        uint8_t  word_size;      ///< Per-beat data width encoding (0=8b,1=16b,2=32b,3=64b); from dma_settings.word_size[1:0]
        bool     ote_enabled;    ///< Outstanding Transaction Enable; from dma_settings.OTE bit[16]
        bool     sdma_err_rsp;   ///< AXI slave error response mode; from dma_settings.sdma_err_rsp bit[17]
    };

    /// @brief DMA configuration shadow, updated atomically by handle_write_dma_settings().
    dma_config_t dma_cfg;

    // =========================================================================
    // DAC (Direct Access Configuration) Shadow Struct (FUNC_XSPI_005)
    // =========================================================================

    /**
     * @struct dac_config_t
     * @brief Groups the five Direct-mode and XIP configuration shadow variables
     *        decoded from direct_access_cfg, direct_access_rmp/rmp_1, and
     *        xip_mode_cfg.
     *
     * Reset defaults:
     *   active_bank         = 0      (dac_bank_num[2:0] = 0)
     *   rmp_addr_en         = false  (direct_access_cfg.rmp_addr_en = 0)
     *   rwds_cap_en         = false  (direct_access_cfg.rwds_cap_en bit[4] = 0)
     *   xip_exit_pending    = false  (direct_access_cfg.mode_bit_xip_dis = 0)
     *   xip_entry_armed     = false  (direct_access_cfg.mode_bit_xip_en = 0)
     *   remap_offset        = 0      (direct_access_rmp / rmp_1 = 0x00000000)
     *   xip_active_banks    = 0      (xip_mode_cfg.xip_en[7:0] = 0)
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.direct_access_cfg, xip_mode_cfg;
     *            docs/xspi_ctrl-detailed-design.md Section 5.x (FUNC_XSPI_005).
     */
    struct dac_config_t {
        unsigned int active_bank;         ///< Direct-mode bank index; from direct_access_cfg.dac_bank_num[2:0]
        bool         rmp_addr_en;         ///< Address remap enable; from direct_access_cfg.rmp_addr_en bit[12]
        bool         rwds_cap_en;         ///< RWDS byte-masking enable; from direct_access_cfg.rwds_cap_en bit[4]
        bool         xip_exit_pending;    ///< XIP exit pending; set when direct_access_cfg.mode_bit_xip_dis=1
        bool         xip_entry_armed;     ///< XIP entry armed for active_bank; set by direct_access_cfg.mode_bit_xip_en (bit 8); cleared after entry READ completes — FUNC_XSPI_013
        uint64_t     remap_offset;        ///< 64-bit remap offset N; from direct_access_rmp[31:0] / rmp_1[63:32]
        uint8_t      xip_active_banks;    ///< XIP per-bank enable bitmask (one bit per bank, 8 banks max); from xip_mode_cfg.xip_en[7:0]; updated by handle_write_xip_mode_cfg() and XIP entry/exit sequences — FUNC_XSPI_013
    };

    /// @brief Direct-access and XIP configuration shadow, updated by
    ///        handle_write_direct_access_cfg() (active_bank, rwds_cap_en, xip_exit_pending,
    ///        xip_entry_armed), handle_write_direct_access_rmp() (remap_offset[31:0]),
    ///        handle_write_direct_access_rmp_1() (remap_offset[63:32]),
    ///        handle_write_xip_mode_cfg() (xip_active_banks),
    ///        and the FUNC_XSPI_013 XIP entry/exit sequences in b_transport_axi_slave(),
    ///        pio_handle_trigger(), and cdma_handle_trigger().
    dac_config_t dac_cfg;

    // =========================================================================
    // XIP Mode-Byte Value Shadows (FUNC_XSPI_013)
    // =========================================================================

    /**
     * @brief XIP entry mode-byte value shadow (from xip_mode_cfg bits[15:8]).
     *
     * 8-bit mode byte value inserted between the address phase and data phase of
     * the first Direct-mode READ when XIP entry is armed (via
     * direct_access_cfg.mode_bit_xip_en or MB_XIP_EN in PIO/ACMD).
     * After insertion the corresponding xip_active_banks bit is set and the arm
     * flag is cleared.
     *
     * Reset: 0x00  (xip_mode_cfg hardware reset value 0x00FF0000; bits[15:8]=0x00).
     * Updated by handle_write_xip_mode_cfg() on every write to xip_mode_cfg.
     *
     * Reference: docs/xspi_ctrl-detailed-design.md Section 7.5.2;
     *            docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.xip_mode_cfg.fields.xip_en_mb_val.
     */
    uint8_t xip_en_mb_val;

    /**
     * @brief XIP exit mode-byte value shadow (from xip_mode_cfg bits[23:16]).
     *
     * 8-bit mode byte value inserted between the address phase and data phase of
     * the Direct-mode READ when XIP exit is pending (via
     * direct_access_cfg.mode_bit_xip_dis or MB_XIP_DIS in PIO/ACMD).
     * After insertion the corresponding xip_active_banks bit is cleared and the
     * xip_exit_pending flag is cleared in dac_cfg.
     *
     * Reset: 0xFF  (xip_mode_cfg hardware reset value 0x00FF0000; bits[23:16]=0xFF).
     * Updated by handle_write_xip_mode_cfg() on every write to xip_mode_cfg.
     *
     * Reference: docs/xspi_ctrl-detailed-design.md Section 7.5.2;
     *            docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.xip_mode_cfg.fields.xip_dis_mb_val.
     */
    uint8_t xip_dis_mb_val;

    // =========================================================================
    // Sequence Configuration Shadow Structure (FUNC_XSPI_005)
    // =========================================================================

    /**
     * @struct seq_config_t
     * @brief Shadow structure caching all hardware sequence configuration
     *        parameters decoded from the dev_seq_regs_a register group.
     *
     * Each field group corresponds to one operation type (RST, ERASE, PROGRAM,
     * READ, WE, STATUS). Fields are populated by the Group 4 register write
     * callbacks and consumed by the ACMD, PIO, DIRECT, and XIP operating mode
     * engines implemented in later functionalities.
     *
     * Bitfield extraction source registers:
     *   rst:   rst_seq_cfg_0 (0x400), rst_seq_cfg_1 (0x404)
     *   ers:   ers_seq_cfg_0 (0x410), ers_seq_cfg_1 (0x414), ers_seq_cfg_2 (0x418)
     *   prog:  prog_seq_cfg_0 (0x420), prog_seq_cfg_1 (0x424), prog_seq_cfg_2 (0x428)
     *   read:  read_seq_cfg_0 (0x430), read_seq_cfg_1 (0x434), read_seq_cfg_2 (0x438)
     *   we:    we_seq_cfg_0 (0x440)
     *   stat:  stat_seq_cfg_0–10 (0x450–0x478)
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.rst_seq_cfg_0/1 … stat_seq_cfg_10;
     *            docs/xspi_ctrl-detailed-design.md Section 5.3 (device sequence
     *            config registers).
     */
    struct seq_config_t {

        // -------------------------------------------------------------------
        // RST (Reset) Sequence — Profile 1 / SPI NAND
        // Populated by handle_write_rst_seq_cfg_0() and
        // handle_write_rst_seq_cfg_1().
        // -------------------------------------------------------------------

        /// @brief CMD0 opcode value (rst_seq_cfg_0 bits[7:0])
        uint8_t  rst_cmd0_val;
        /// @brief CMD1 opcode value (rst_seq_cfg_0 bits[15:8])
        uint8_t  rst_cmd1_val;
        /// @brief CMD0 phase enable flag (rst_seq_cfg_0 bit[16])
        bool     rst_cmd0_en;
        /// @brief I/O width for both command phases (rst_seq_cfg_0 bits[25:24])
        uint8_t  rst_cmd_ios;
        /// @brief DDR edge select for both command phases (rst_seq_cfg_0 bit[28])
        bool     rst_cmd_edge;
        /// @brief Data phase I/O width (rst_seq_cfg_0 bits[19:18])
        uint8_t  rst_data_ios;
        /// @brief Data phase DDR edge select (rst_seq_cfg_0 bit[21])
        bool     rst_data_edge;
        /// @brief Data phase enable flag (rst_seq_cfg_0 bit[22])
        bool     rst_data_en;
        /// @brief CMD0 extension enable (rst_seq_cfg_1 bit[0])
        bool     rst_cmd0_ext_en;
        /// @brief CMD1 extension enable (rst_seq_cfg_1 bit[1])
        bool     rst_cmd1_ext_en;
        /// @brief CMD0 extension opcode value (rst_seq_cfg_1 bits[15:8])
        uint8_t  rst_cmd0_ext_val;
        /// @brief CMD1 extension opcode value (rst_seq_cfg_1 bits[23:16])
        uint8_t  rst_cmd1_ext_val;
        /// @brief Confirmation Byte In value (rst_seq_cfg_1 bits[31:24])
        uint8_t  rst_data_val;

        // -------------------------------------------------------------------
        // ERASE Sequence — Profile 1 / SPI NAND (Sector + Chip Erase)
        // Populated by handle_write_ers_seq_cfg_0/1/2().
        // -------------------------------------------------------------------

        /// @brief Sector-erase command opcode (ers_seq_cfg_0 bits[7:0])
        uint8_t  ers_cmd_val;
        /// @brief Sector-erase command I/O width (ers_seq_cfg_0 bits[9:8])
        uint8_t  ers_cmd_ios;
        /// @brief Sector-erase command DDR edge select (ers_seq_cfg_0 bit[11])
        bool     ers_cmd_edge;
        /// @brief Sector-erase address byte count (ers_seq_cfg_0 bits[14:12])
        uint8_t  ers_addr_cnt;
        /// @brief Sector-erase command extension enable (ers_seq_cfg_0 bit[15])
        bool     ers_cmd_ext_en;
        /// @brief Sector-erase command extension value (ers_seq_cfg_0 bits[23:16])
        uint8_t  ers_cmd_ext_val;
        /// @brief Sector-erase address I/O width (ers_seq_cfg_0 bits[25:24])
        uint8_t  ers_addr_ios;
        /// @brief Sector-erase address DDR edge select (ers_seq_cfg_0 bit[28])
        bool     ers_addr_edge;
        /// @brief Sector size encoded as 2^N (ers_seq_cfg_1 bits[4:0])
        uint8_t  ers_sect_size;
        /// @brief Chip-erase command opcode (ers_seq_cfg_2 bits[7:0])
        uint8_t  ersa_cmd_val;
        /// @brief Chip-erase command I/O width (ers_seq_cfg_2 bits[9:8])
        uint8_t  ersa_cmd_ios;
        /// @brief Chip-erase command DDR edge select (ers_seq_cfg_2 bit[11])
        bool     ersa_cmd_edge;
        /// @brief Chip-erase command extension enable (ers_seq_cfg_2 bit[15])
        bool     ersa_cmd_ext_en;
        /// @brief Chip-erase command extension value (ers_seq_cfg_2 bits[23:16])
        uint8_t  ersa_cmd_ext_val;

        // -------------------------------------------------------------------
        // PROGRAM Sequence — Profile 1 / SPI NAND (Profile 1) and
        //                    Profile 2 HyperFlash/HyperRAM fields
        // Populated by handle_write_prog_seq_cfg_0/1/2().
        // -------------------------------------------------------------------

        /// @brief Page-program command opcode (prog_seq_cfg_0 bits[7:0])
        uint8_t  prog_cmd_val;
        /// @brief Page-program command I/O width (prog_seq_cfg_0 bits[9:8])
        uint8_t  prog_cmd_ios;
        /// @brief Page-program command DDR edge select (prog_seq_cfg_0 bit[11])
        bool     prog_cmd_edge;
        /// @brief Page-program address byte count (prog_seq_cfg_0 bits[14:12])
        uint8_t  prog_addr_cnt;
        /// @brief Page-program address I/O width (prog_seq_cfg_0 bits[17:16])
        uint8_t  prog_addr_ios;
        /// @brief Page-program address DDR edge select (prog_seq_cfg_0 bit[19])
        bool     prog_addr_edge;
        /// @brief Page-program data I/O width (prog_seq_cfg_0 bits[21:20])
        uint8_t  prog_data_ios;
        /// @brief Page-program data DDR edge select (prog_seq_cfg_0 bit[23])
        bool     prog_data_edge;
        /// @brief Dummy cycle count in Profile 1 program (prog_seq_cfg_0 bits[29:24])
        uint8_t  prog_dummy_cnt;
        /// @brief Page-program command extension enable (prog_seq_cfg_1 bit[0])
        bool     prog_cmd_ext_en;
        /// @brief Page-program command extension value (prog_seq_cfg_1 bits[15:8])
        uint8_t  prog_cmd_ext_val;
        /// @brief Profile 2 target space (prog_seq_cfg_2 bit[0])
        bool     prog_p2_target;
        /// @brief Profile 2 burst type (prog_seq_cfg_2 bit[1])
        bool     prog_p2_burst_type;
        /// @brief Profile 2 mask command modifier (prog_seq_cfg_2 bit[2])
        bool     prog_p2_mask_cmd_mod;
        /// @brief Profile 2 latency cycle count (prog_seq_cfg_2 bits[13:8])
        uint8_t  prog_p2_latency_cnt;

        // -------------------------------------------------------------------
        // READ Sequence — Profile 1 / SPI NAND and Profile 2
        // Populated by handle_write_read_seq_cfg_0/1/2().
        // -------------------------------------------------------------------

        /// @brief Read command opcode (read_seq_cfg_0 bits[7:0])
        uint8_t  read_cmd_val;
        /// @brief Read command I/O width (read_seq_cfg_0 bits[9:8])
        uint8_t  read_cmd_ios;
        /// @brief Read command DDR edge select (read_seq_cfg_0 bit[11])
        bool     read_cmd_edge;
        /// @brief Read address byte count (read_seq_cfg_0 bits[14:12])
        uint8_t  read_addr_cnt;
        /// @brief Read address I/O width (read_seq_cfg_0 bits[17:16])
        uint8_t  read_addr_ios;
        /// @brief Read address DDR edge select (read_seq_cfg_0 bit[19])
        bool     read_addr_edge;
        /// @brief Read data I/O width (read_seq_cfg_0 bits[21:20])
        uint8_t  read_data_ios;
        /// @brief Read data DDR edge select (read_seq_cfg_0 bit[23])
        bool     read_data_edge;
        /// @brief Dummy cycle count without mode bytes (read_seq_cfg_0 bits[29:24])
        uint8_t  read_dummy_cnt;
        /// @brief Read command extension enable (read_seq_cfg_1 bit[0])
        bool     read_cmd_ext_en;
        /// @brief Cache random read enable (read_seq_cfg_1 bit[4])
        bool     read_cache_random_en;
        /// @brief Read command extension value (read_seq_cfg_1 bits[15:8])
        uint8_t  read_cmd_ext_val;
        /// @brief Dummy cycles used when mode-bytes enabled (read_seq_cfg_1 bits[29:24])
        uint8_t  read_mb_dummy_cnt;
        /// @brief Mode-byte enable flag (read_seq_cfg_1 bit[31])
        bool     read_mb_en;
        /// @brief Profile 2 target space (read_seq_cfg_2 bit[0])
        bool     read_p2_target;
        /// @brief Profile 2 burst type (read_seq_cfg_2 bit[1])
        bool     read_p2_burst_type;
        /// @brief Profile 2 mask command modifier (read_seq_cfg_2 bit[2])
        bool     read_p2_mask_cmd_mod;
        /// @brief Profile 2 HyperFlash boundary enable (read_seq_cfg_2 bit[3])
        bool     read_p2_hf_bound_en;
        /// @brief Profile 2 latency cycle count (read_seq_cfg_2 bits[13:8])
        uint8_t  read_p2_latency_cnt;

        // -------------------------------------------------------------------
        // WE (Write Enable Latch) Sequence — Profile 1 / SPI NAND
        // Populated by handle_write_we_seq_cfg_0().
        // -------------------------------------------------------------------

        /// @brief WEL command opcode (we_seq_cfg_0 bits[7:0])
        uint8_t  we_cmd_val;
        /// @brief WEL command I/O width (we_seq_cfg_0 bits[9:8])
        uint8_t  we_cmd_ios;
        /// @brief WEL command DDR edge select (we_seq_cfg_0 bit[11])
        bool     we_cmd_edge;
        /// @brief WEL command extension enable (we_seq_cfg_0 bit[15])
        bool     we_cmd_ext_en;
        /// @brief WEL command extension value (we_seq_cfg_0 bits[23:16])
        uint8_t  we_cmd_ext_val;
        /// @brief WEL sequence enable flag (we_seq_cfg_0 bit[24])
        bool     we_en;

        // -------------------------------------------------------------------
        // STATUS Sequence — Profile 1 / SPI NAND / Profile 2 HF
        // Populated by handle_write_stat_seq_cfg_0/1/2/3/4/5/7/8/9/10().
        // -------------------------------------------------------------------

        /// @brief Status command I/O width (stat_seq_cfg_0 bits[1:0])
        uint8_t  stat_cmd_ios;
        /// @brief Status command DDR edge select (stat_seq_cfg_0 bit[4])
        bool     stat_cmd_edge;
        /// @brief Status command extension enable (stat_seq_cfg_0 bit[5])
        bool     stat_cmd_ext_en;
        /// @brief Status address byte count (stat_seq_cfg_0 bits[9:8])
        uint8_t  stat_addr_cnt;
        /// @brief Status address I/O width (stat_seq_cfg_0 bits[11:10])
        uint8_t  stat_addr_ios;
        /// @brief Status address DDR edge select (stat_seq_cfg_0 bit[12])
        bool     stat_addr_edge;
        /// @brief Status data I/O width (stat_seq_cfg_0 bits[21:20])
        uint8_t  stat_data_ios;
        /// @brief Status data DDR edge select (stat_seq_cfg_0 bit[22])
        bool     stat_data_edge;
        /// @brief RDY/BUSY check dummy cycles (stat_seq_cfg_1 bits[5:0])
        uint8_t  stat_dev_rdy_dummy_cnt;
        /// @brief RDY/BUSY check address enable (stat_seq_cfg_1 bit[6])
        bool     stat_dev_rdy_addr_en;
        /// @brief PROG_FAIL check dummy cycles (stat_seq_cfg_1 bits[21:16])
        uint8_t  stat_prog_fail_dummy_cnt;
        /// @brief PROG_FAIL check address enable (stat_seq_cfg_1 bit[22])
        bool     stat_prog_fail_addr_en;
        /// @brief ERS_FAIL check dummy cycles (stat_seq_cfg_1 bits[29:24])
        uint8_t  stat_ers_fail_dummy_cnt;
        /// @brief ERS_FAIL check address enable (stat_seq_cfg_1 bit[30])
        bool     stat_ers_fail_addr_en;
        /// @brief RDY/BUSY status command opcode (stat_seq_cfg_2 bits[7:0])
        uint8_t  stat_dev_rdy_cmd_val;
        /// @brief ERS_FAIL status command opcode (stat_seq_cfg_2 bits[15:8])
        uint8_t  stat_ers_fail_cmd_val;
        /// @brief PROG_FAIL status command opcode (stat_seq_cfg_2 bits[31:24])
        uint8_t  stat_prog_fail_cmd_val;
        /// @brief RDY/BUSY status command extension value (stat_seq_cfg_3 bits[7:0])
        uint8_t  stat_dev_rdy_cmd_ext_val;
        /// @brief ERS_FAIL status command extension value (stat_seq_cfg_3 bits[15:8])
        uint8_t  stat_ers_fail_cmd_ext_val;
        /// @brief PROG_FAIL status command extension value (stat_seq_cfg_3 bits[31:24])
        uint8_t  stat_prog_fail_cmd_ext_val;
        /// @brief Profile 2 HF status latency count (stat_seq_cfg_4 bits[13:8])
        uint8_t  stat_p2_latency_cnt;
        /// @brief Profile 2 HF mask command modifier (stat_seq_cfg_4 bit[2])
        bool     stat_p2_mask_cmd_mod;
        /// @brief RDY/BUSY status bit index (stat_seq_cfg_5 bits[3:0])
        uint8_t  stat_dev_rdy_idx;
        /// @brief RDY ready compare value (stat_seq_cfg_5 bit[4])
        bool     stat_dev_rdy_val;
        /// @brief RDY status word size (stat_seq_cfg_5 bit[5])
        bool     stat_dev_rdy_size;
        /// @brief RDY/BUSY polling enable (stat_seq_cfg_5 bit[6])
        bool     stat_dev_rdy_en;
        /// @brief ERS_FAIL status bit index (stat_seq_cfg_5 bits[11:8])
        uint8_t  stat_ers_fail_idx;
        /// @brief ERS_FAIL compare value (stat_seq_cfg_5 bit[12])
        bool     stat_ers_fail_val;
        /// @brief ERS_FAIL status word size (stat_seq_cfg_5 bit[13])
        bool     stat_ers_fail_size;
        /// @brief ERS_FAIL polling enable (stat_seq_cfg_5 bit[14])
        bool     stat_ers_fail_en;
        /// @brief PROG_FAIL status bit index (stat_seq_cfg_5 bits[27:24])
        uint8_t  stat_prog_fail_idx;
        /// @brief PROG_FAIL compare value (stat_seq_cfg_5 bit[28])
        bool     stat_prog_fail_val;
        /// @brief PROG_FAIL status word size (stat_seq_cfg_5 bit[29])
        bool     stat_prog_fail_size;
        /// @brief PROG_FAIL polling enable (stat_seq_cfg_5 bit[30])
        bool     stat_prog_fail_en;
        /// @brief RDY/BUSY check address value (stat_seq_cfg_7 bits[31:0])
        uint32_t stat_dev_rdy_addr;
        /// @brief PROG_FAIL check address value (stat_seq_cfg_8 bits[31:0])
        uint32_t stat_prog_fail_addr;
        /// @brief ERS_FAIL check address value (stat_seq_cfg_9 bits[31:0])
        uint32_t stat_ers_fail_addr;
        /// @brief ECC fail mask (stat_seq_cfg_10 bits[7:0])
        uint8_t  stat_ecc_fail_mask;
        /// @brief ECC fail value (stat_seq_cfg_10 bits[15:8])
        uint8_t  stat_ecc_fail_val;
        /// @brief ECC correctable error value (stat_seq_cfg_10 bits[23:16])
        uint8_t  stat_ecc_corr_val;
        /// @brief Cache Read Busy bit index (stat_seq_cfg_10 bits[26:24])
        uint8_t  stat_crdy_idx;
        /// @brief Cache Read Busy compare value (stat_seq_cfg_10 bit[27])
        bool     stat_crdy_val;
        /// @brief ECC fail polling enable (stat_seq_cfg_10 bit[31])
        bool     stat_ecc_fail_en;
    };

    /**
     * @brief Sequence configuration shadow instance.
     *
     * Populated by the Group 4 sequence register write callbacks. Consumed by
     * the ACMD, PIO, DIRECT, and XIP operating mode engines. Initialized to
     * register hardware reset values at construction to ensure that mode engines
     * can access valid configuration data before any software register writes.
     *
     * Reset: the struct fields are initialized in the constructor initializer
     * list to match the register reset values documented in the register map.
     * On reset_in the fields are restored by re-applying the register reset
     * values through the same code path as the initial construction.
     */
    seq_config_t seq_cfg;

    /**
     * @brief NAND spare area size shadow (from global_seq_cfg_1.seq_page_size_ext
     *        bits[8:0]).
     *
     * Extends the data-phase byte count for SPI NAND page read/program
     * operations in ACMD and PIO modes. Not used in DIRECT mode.
     * Reset: 0 (register reset value 0x000 for bits[8:0]).
     * Updated by handle_write_global_seq_cfg_1().
     */
    uint16_t nand_spare_area;

    /**
     * @brief READ page size encoded as 2^N (from global_seq_cfg.seq_page_size_rd
     *        bits[3:0]).
     *
     * Determines the page granularity for READ operations. The actual size in
     * bytes is 2^read_page_size. Value 0xF means unlimited (all data in single
     * xSPI command). Reset: 0xF (register reset 0x0000208F bits[3:0] = 0xF).
     * Updated by handle_write_global_seq_cfg().
     */
    uint8_t  read_page_size;

    /**
     * @brief PROGRAM page size encoded as 2^N (from global_seq_cfg.seq_page_size_pgm
     *        bits[7:4]).
     *
     * Determines the page granularity for PROGRAM operations. The actual size
     * in bytes is 2^program_page_size. Reset: 0x8 (256 bytes; register reset
     * 0x0000208F bits[7:4] = 0x8). Updated by handle_write_global_seq_cfg().
     */
    uint8_t  program_page_size;

    // =========================================================================
    // Discovery Control Shadow Struct (FUNC_XSPI_006)
    // =========================================================================

    /**
     * @struct discovery_cfg_t
     * @brief Groups all software-programmed discovery configuration parameters
     *        decoded from the discovery_control register (offset 0x260).
     *
     * All fields are updated by handle_write_discovery_control() and consumed
     * by the SFDP discovery engine (FUNC_XSPI_007) when a software-initiated
     * re-discovery is triggered via discovery_req=1.
     *
     * The read-only hardware-updated fields (discovery_comp at bit[2],
     * discovery_fail at bits[4:3], discovery_inhibit at bit[5]) are NOT
     * represented here; they are maintained exclusively in the hardware
     * register and are protected by write_bit_mask=0x7ffc3.
     *
     * Reset defaults: all fields zero / false (matches discovery_control
     * hardware reset value 0x00000000).
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            registers.discovery_control.fields.*;
     *            docs/xspi_ctrl-detailed-design.md Section 6.3.2.
     */
    struct discovery_cfg_t {
        uint8_t bank;         ///< CS select for SFDP discovery [18:16]; valid range 0-7
        uint8_t num_lines;    ///< I/O width encoding for discovery [15:12]; 0=Auto,1=1S,2=2S,4=4S,8=8S,0xC=HF,0xE=NAND
        uint8_t abnum;        ///< Address bytes [11]; 0=3-byte, 1=4-byte
        uint8_t dummy_cnt;    ///< Dummy cycles selector [10]; 0=8 cycles, 1=20 cycles
        uint8_t cmd_type;     ///< SDR/DDR/DTR encoding [9:8]; 0=SDR,1=DDR,2=DTR
        bool    extop_en;     ///< Extended opcode enable [7]; false=standard READ_SFDP
        uint8_t extop_val;    ///< Extended opcode variant [6]; 0=rep(0x5A5A), 1=neg(0x5AA5)
        uint8_t req_type;     ///< Request type [1]; 0=full SFDP discovery, 1=pre-configured
        bool    req;          ///< Discovery request trigger [0]; write 1 to trigger re-discovery
    };

    /**
     * @brief Discovery configuration shadow instance.
     *
     * Populated by handle_write_discovery_control() from the writable fields
     * of the discovery_control register. Consumed by the SFDP discovery engine
     * (FUNC_XSPI_007). Initialized to all-zero at construction matching the
     * hardware reset value 0x00000000.
     */
    discovery_cfg_t discovery_cfg;

    // =========================================================================
    // PoR Initialization Completion Flag (FUNC_XSPI_007)
    // =========================================================================

    /**
     * @brief Initialization completion flag — set once PoR sequence finishes.
     *
     * Initialized to false at construction and reset to false by reset_handler().
     * Set to true by b_transport_por() after SFDP discovery (and optional boot)
     * completes. While false, all register write callbacks return immediately
     * without modifying register state, implementing the hardware behaviour that
     * register writes before init_comp are silently discarded.
     *
     * The flag mirrors ctrl_status.init_comp (bit 8 in the register), which is
     * also set at the same time, but uses a separate C++ bool for callback-path
     * speed: the bool check avoids a register read on every incoming write.
     *
     * Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_007 (pre-init
     *            write blocking); docs/xspi_ctrl-detailed-design.md Section 8.1.
     */
    bool m_init_comp_done;

    // =========================================================================
    // STIG Engine Decoded Instruction Structure (FUNC_XSPI_009)
    // =========================================================================

    /**
     * @struct stig_instruction
     * @brief Decoded representation of one 128-bit STIG instruction.
     *
     * Populated by decode_instruction() from the raw register snapshot of
     * cmd_reg1–cmd_reg4. Consumed by execute_stig() and its five handlers.
     *
     * UG Table 4.23 (command) and 4.27 (glued). cmd_reg1..4 = [31:0]..[127:96].
     * INSTR[6:0] in cmd_reg1; CMD [87:80] in cmd_reg3[23:16]; not ACMD PIO/ADDR0
     * in reg1[31:24] as opcode. Immediate data length: cmd_reg3[25:24] (0/1/2).
     * Long transfers use a glued (127) second STIG. BANK[2:0] in cmd_reg4[14:12].
     * INSTR_LINK is bit 124 (cmd_reg4[28]).
     */
    struct stig_instruction {
        uint8_t  opcode;           ///< CMD [87:80] — from cmd_reg3[23:16]
        uint8_t  cmd_ext;          ///< CMD_EXT [79:72] from cmd_reg3[15:8]
        uint8_t  opcode_ios;       ///< I/O/edge timing: not used in v1; forced 0
        bool     opcode_edge;      ///< reserved, false
        uint8_t  addr_ios;         ///< reserved, 0
        bool     addr_edge;        ///< reserved, false
        uint8_t  data_ios;         ///< reserved, 0
        bool     data_edge;        ///< reserved, false
        uint8_t  instr_type;       ///< INSTR[6:0] from cmd_reg1[6:0]
        bool     instr_link;        ///< INSTR_LINK bit 124, cmd_reg4[28]
        uint8_t  data_bytes;       ///< DATA/MODE 2b in cmd_reg3[25:24] (0/1/2)
        uint32_t data_bytes_count; ///< Per-type/phase length (see decode_instruction)
        uint8_t  bank_num;         ///< BANK[2:0] from cmd_reg4[14:12]
        uint64_t address;         ///< Table 4.23: ADDR0..5 byte lanes across r1–r3
        uint32_t write_data;       ///< DATA0/DATA1 in r1[15:8] and r1[23:16] (16b)
        uint8_t  glued_dir;        ///< Glued (127) phase: DIR, Table 4.27 (e.g. r4[4])
    };

    // =========================================================================
    // STIG INSTR_LINK Chain State (FUNC_XSPI_002)
    // =========================================================================

    /**
     * @brief STIG INSTR_LINK pending flag.
     *
     * Set to true when the STIG engine processes a first-phase INSTR_LINK
     * instruction (cmd_reg4[28]=1). The engine then holds CS# active and
     * waits for the second cmd_reg0 write (the Glued Data Instruction).
     * Cleared after the second phase completes or on reset.
     */
    bool stig_instr_link_pending;

    /**
     * @brief Saved first-phase STIG instruction for INSTR_LINK two-phase chains.
     *
     * When the STIG engine processes a first-phase INSTR_LINK trigger
     * (inst.instr_link=1), the decoded stig_instruction is saved here before
     * the engine loops back to wait for the second cmd_reg0 trigger.
     * On the second trigger the saved instruction is passed as the data_phase
     * argument to execute_stig() / handle_stig_read().
     * Valid only when stig_instr_link_pending is true.
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            state_machines.STIG.INSTR_LINK;
     *            docs/xspi_ctrl-detailed-design.md Section 7.2.3.
     */
    stig_instruction stig_link_cmd_phase;

    /**
     * @brief STIG command trigger event.
     *
     * Notified by handle_write_cmd_reg0 when ctrl_config.work_mode indicates
     * STIG mode. Consumed by stig_engine_thread SC_THREAD.
     */
    sc_core::sc_event cmd_trigger_event;

    /**
     * @brief Interrupt output update event.
     *
     * Notified by evaluate_interrupt_out() and reset_handler() whenever the
     * interrupt state may have changed. Consumed exclusively by the
     * update_int_out() SC_METHOD, which is the sole writer of int_out.
     *
     * This event is the mechanism that enforces the single-writer rule for
     * int_out: no code path other than update_int_out() may call int_out.write().
     *
     * Reference: docs/xspi_ctrl-detailed-design.md Section 9.1 (interrupt
     *            output architecture); SystemC LRM single-writer rule (E115).
     */
    sc_core::sc_event m_int_update_event;

    // =========================================================================
    // Temporal Decoupling
    // =========================================================================

    /**
     * @brief TLM-2.0 quantum keeper for temporal decoupling.
     *
     * Accumulates local time advances from b_transport handlers and syncs with
     * the SystemC kernel when the quantum is exceeded, improving simulation
     * performance while maintaining LT functional correctness.
     */
    tlm_utils::tlm_quantumkeeper m_qk;

    // =========================================================================
    // Logger
    // =========================================================================

    /// @brief CSML structured logger instance
    mutable CsmlLogger logger;

    // =========================================================================
    // Flash Bus Transaction Helpers (FUNC_XSPI_002)
    // =========================================================================

    /**
     * @brief Construct a cdns_extension and issue a single b_transport on
     *        xspi_bus_socket[bank].
     *
     * Creates a tlm_generic_payload, populates it from the caller-supplied
     * cdns_extension fields, validates the bank index against NUM_TARGETS,
     * attaches the extension, and calls xspi_bus_socket[bank]->b_transport().
     * Returns whether the transport completed with TLM_OK_RESPONSE.
     *
     * Caller responsibilities:
     *  - Populate all cdns_extension fields before calling.
     *  - Set ctrl_status.ctrl_busy before calling; clear it after return.
     *  - This function must not be called from an SC_METHOD (no wait()).
     *
     * @param ext    Fully populated cdns_extension to attach to the payload
     * @param bank   Chip-select / bank index (0 to NUM_TARGETS-1)
     * @param is_read True for TLM_READ_COMMAND (data from flash), false for
     *                TLM_WRITE_COMMAND (data to flash or command-only)
     * @param data_ptr  Pointer to a data buffer for READ responses; may be
     *                  nullptr for command-only operations (no data phase)
     * @param data_len  Number of data bytes; 0 for command-only
     * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
     */
    bool dispatch_flash_transaction(cdns_extension& ext,
                                    unsigned int    bank,
                                    bool            is_read,
                                    uint8_t*        data_ptr,
                                    uint32_t        data_len);

    // =========================================================================
    // AXI Master DMA Transaction Helpers (FUNC_XSPI_004)
    // =========================================================================

    /**
     * @brief Issue an AXI master READ transaction on i_dma_socket.
     *
     * Constructs a TLM-2.0 tlm_generic_payload with TLM_READ_COMMAND,
     * sets the 64-bit address (lower 32 bits always from addr_low; upper 32
     * bits from addr_high when dma_addr_width == 64, zero otherwise), sets
     * the data pointer and length, and calls i_dma_socket->b_transport().
     *
     * On TLM_GENERIC_ERROR_RESPONSE the method:
     *   1. Writes the failing 64-bit address into dma_target_error_l (lower)
     *      and dma_target_error_h (upper) RO registers via direct assignment.
     *   2. Sets intr_status.cdma_terr (bit 17) for descriptor-path errors,
     *      or intr_status.ddma_terr (bit 18) for data-path errors, depending
     *      on the @p is_data_path flag.
     *   3. Calls evaluate_interrupt_out() to propagate the interrupt.
     *   4. Returns false.
     *
     * On TLM_OK_RESPONSE returns true.
     *
     * This function MUST NOT be called from an SC_METHOD (b_transport is
     * blocking and may advance simulation time via the quantum keeper).
     *
     * @param addr_low    Lower 32 bits of the 64-bit system memory address
     * @param addr_high   Upper 32 bits of the 64-bit system memory address
     *                    (ignored when dma_addr_width == 32)
     * @param data_ptr    Pointer to caller-supplied buffer to receive data
     * @param data_len    Number of bytes to read
     * @param is_data_path True → sets ddma_terr on error;
     *                     False → sets cdma_terr on error
     * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
     */
    bool dma_read(uint32_t    addr_low,
                  uint32_t    addr_high,
                  uint8_t*    data_ptr,
                  uint32_t    data_len,
                  bool        is_data_path);

    /**
     * @brief Issue an AXI master WRITE transaction on i_dma_socket.
     *
     * Constructs a TLM-2.0 tlm_generic_payload with TLM_WRITE_COMMAND,
     * sets the 64-bit address (lower 32 bits always from addr_low; upper 32
     * bits from addr_high when dma_addr_width == 64, zero otherwise), sets
     * the data pointer and length, and calls i_dma_socket->b_transport().
     *
     * On TLM_GENERIC_ERROR_RESPONSE the method:
     *   1. Writes the failing 64-bit address into dma_target_error_l (lower)
     *      and dma_target_error_h (upper) RO registers via direct assignment.
     *   2. Sets intr_status.cdma_terr (bit 17) for descriptor-path errors,
     *      or intr_status.ddma_terr (bit 18) for data-path errors, depending
     *      on the @p is_data_path flag.
     *   3. Calls evaluate_interrupt_out() to propagate the interrupt.
     *   4. Returns false.
     *
     * On TLM_OK_RESPONSE returns true.
     *
     * This function MUST NOT be called from an SC_METHOD (b_transport is
     * blocking and may advance simulation time via the quantum keeper).
     *
     * @param addr_low    Lower 32 bits of the 64-bit system memory address
     * @param addr_high   Upper 32 bits of the 64-bit system memory address
     *                    (ignored when dma_addr_width == 32)
     * @param data_ptr    Pointer to caller-supplied buffer containing data to write
     * @param data_len    Number of bytes to write
     * @param is_data_path True → sets ddma_terr on error;
     *                     False → sets cdma_terr on error
     * @return true if b_transport returned TLM_OK_RESPONSE, false otherwise
     */
    bool dma_write(uint32_t    addr_low,
                   uint32_t    addr_high,
                   uint8_t*    data_ptr,
                   uint32_t    data_len,
                   bool        is_data_path);

    // =========================================================================
    // Boot Engine Helper (FUNC_XSPI_010)
    // =========================================================================

    /**
     * @brief Autonomous Boot DMA Engine — FUNC_XSPI_010.
     *
     * Executes the full boot sequence synchronously from within
     * b_transport_por() after SFDP discovery completes, when both
     * conditions hold: boot_en == 1 (caller-supplied) AND
     * ctrl_features_reg.boot_available == 1 (hardware parameter).
     *
     * Sequence:
     *  Step 1 — Configuration Record Read:
     *    Issues a flash READ on xspi_bus_socket[0] at address 0x0 to read
     *    the 32-byte boot configuration record using the seq_cfg parameters
     *    already established by SFDP discovery (or their reset defaults when
     *    discovery_inhibit=1). If the flash transaction returns a non-OK
     *    response, sets boot_status.boot_dqs_err (bit 0) and returns with
     *    boot_error=1.
     *
     *  Step 2 — Parameter Extraction:
     *    Parses the 32-byte record:
     *      - Main Data Size  : bytes[0..3]  (lower 32 bits of 8-byte field)
     *      - Image Offset    : bytes[8..13] (lower 48 bits of 8-byte field)
     *      - Host Address    : bytes[16..23] (full 64 bits)
     *    If Main Data Size == 0, treats as a no-op and sets boot_comp=1.
     *
     *  Step 3 — DMA Transfer (flash to system memory):
     *    Issues READ transactions on xspi_bus_socket[0] in chunks of at most
     *    BOOT_DMA_CHUNK_BYTES bytes from Image_Offset for Main_Data_Size total
     *    bytes. For each chunk:
     *      a. Dispatch flash READ via dispatch_flash_transaction().
     *      b. Write the read data to system memory via dma_write() at
     *         Host_Address + bytes_transferred.
     *    If any flash READ returns non-OK, sets boot_status.boot_dqs_err (bit 0)
     *    and terminates with boot_error=1.
     *    If any dma_write() returns non-OK (AXI bus error), sets
     *    boot_status.boot_bus_err (bit 2) and terminates with boot_error=1.
     *    (boot_status.boot_bus_err may already be set by dma_write via the
     *    dma_target_error_l/h path; the boot engine additionally mirrors it
     *    in boot_status bit 2 for testability.)
     *
     *  Step 4 — Status Output:
     *    On success: sets por_ext->boot_comp = 1.
     *    On error:   sets por_ext->boot_error = 1; leaves boot_comp = 0.
     *    boot_status register bits are written directly (write_bit_mask=0,
     *    direct assignment bypasses write-ignore for hardware-driven writes).
     *
     * Register blocking:
     *    m_init_comp_done remains false throughout this function, ensuring
     *    that software register writes are silently discarded while the boot
     *    engine is executing (TC_XSPI_BOOT_005 requirement).
     *    It is the caller's (b_transport_por) responsibility to set
     *    m_init_comp_done = true BEFORE calling this function so that
     *    TC_XSPI_BOOT_005 style tests can verify the blocking.
     *    Specifically, run_boot_engine() is called while m_init_comp_done
     *    is still false (it is set true by b_transport_por after this
     *    function returns).
     *
     * Architecture references:
     *   docs/xspi_ctrl-architecture-behaviour-map.json operations.boot_sequence
     *   docs/xspi_ctrl-detailed-design.md Section 7.6
     *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_010
     *
     * Single-writer compliance: does not write int_out or any sc_signal
     * directly. boot_status register is written via direct register assignment
     * (hardware-driven path; write_bit_mask=0 on boot_status, contract allows
     * model-internal direct assignment regardless of write_bit_mask).
     *
     * @param por_ext  Pointer to the xspi_PoR_trans extension; must be non-null.
     *                 boot_comp and boot_error output fields are written here.
     */
    void run_boot_engine(xspi_PoR_trans* por_ext);

    /**
     * @brief PIO mode engine — called by handle_write_cmd_reg0 in PIO mode.
     *
     * Implements the full PIO thread dispatch sequence for up to 8 concurrent
     * threads (FUNC_XSPI_011). Executes synchronously within the b_transport
     * call — no SC_THREAD wakeup is required.
     *
     * Sequence (per docs/xspi_ctrl-detailed-design.md Section 7.3):
     *  1. Mask reserved bits [29:27] and [23] from cmd_reg0_val.
     *  2. Decode TRD_NUM[26:24], BANK/CS[22:20], DMA_SEL[19], INT[18],
     *     MB_XIP_DIS[17], MB_XIP_EN[16], CMD_TYPE[15:0].
     *  3. Clamp TRD_NUM to [0, n_threads-1].
     *  4. Check trd_status.trd_busy[TRD_NUM]. If set: silent ignore (no
     *     CMD_IGNORED for PIO per spec) and return.
     *  5. Set trd_status.trd_busy[TRD_NUM] and ctrl_status.ctrl_busy.
     *  6. Snapshot only the cmd_reg fields required by CMD_TYPE:
     *     - RESET_SOFT/JEDEC (0x1100/0x1101): cmd_reg0 only.
     *     - CHIP_ERASE (0x1001):              cmd_reg0 only.
     *     - SECTOR_ERASE (0x1000):            cmd_reg1 (xSPI addr low),
     *                                          cmd_reg4 (SECT_CNT), cmd_reg5 (xSPI addr high).
     *     - READ (0x2200) / PROGRAM (0x2100):  all of cmd_reg1–cmd_reg5.
     *  7. Dispatch flash transaction and DMA transfer per CMD_TYPE:
     *     - READ:         flash READ via xspi_bus_socket[BANK] → DMA WRITE to
     *                     sys_addr via dma_write() (ddma_terr on bus error).
     *                     MB_XIP_EN passed to cdns_extension for XIP entry.
     *     - PROGRAM:      DMA READ from sys_addr via dma_read() → WREN +
     *                     PAGE_PROGRAM via xspi_bus_socket[BANK].
     *     - SECTOR_ERASE: WREN + ERASE_64KB (0xD8) per sector address
     *                     via dispatch_flash_transaction().
     *     - CHIP_ERASE:   WREN + chip erase (0xDC in LT; vendor may use 0xC7/0x60).
     *     - RESET_SOFT:   RESET (0xFF) via dispatch_flash_transaction().
     *     - RESET_JEDEC:  JEDEC RESET (0xF0) via dispatch_flash_transaction().
     *  8. Populate thread_status_[TRD_NUM]: COMPLETE always set; FAIL set on
     *     error; BUS_ERROR set on DMA failure; CMD_ERROR set on unknown CMD_TYPE.
     *  9. Clear trd_status.trd_busy[TRD_NUM] and ctrl_status.ctrl_busy.
     * 10. If INT flag was set: on success set trd_comp_intr_status[TRD_NUM];
     *     on error set trd_error_intr_status[TRD_NUM]; call evaluate_interrupt_out().
     *
     * Single-writer compliance: does not write int_out or any sc_signal directly.
     * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *   state_machines.PIO; docs/xspi_ctrl-detailed-design.md Sections 7.3.1–7.3.5.
     *
     * @param cmd_reg0_val The 32-bit value written to cmd_reg0 in PIO mode
     */
    void pio_handle_trigger(uint32_t cmd_reg0_val);

    /**
     * @brief ACMD/CDMA mode descriptor-based DMA engine (FUNC_XSPI_012).
     *
     * Implements the complete ACMD descriptor chain execution state machine:
     * IDLE → FETCHING_DESCRIPTOR → EXECUTING_COMMAND → UPDATING_STATUS
     * → COMPLETE (or ERROR on any failure).
     *
     * Execution sequence per docs/xspi_ctrl-detailed-design.md Section 7.4:
     *  1.  Decode TRD_NUM from cmd_reg0_val[26:24]; clamp to [0, n_threads-1].
     *  2.  Assemble 64-bit descriptor head address from cmd_reg3:cmd_reg2.
     *      When dma_addr_width == 32, only the lower 32 bits (cmd_reg2) are used.
     *  3.  Busy-thread guard: if trd_status.trd_busy[trd_idx] is already set,
     *      set intr_status.cmd_ignored (W1C, bit 20), call evaluate_interrupt_out()
     *      and return without starting any operation (Section 7.4.5).
     *  4.  Alignment guard (Section 7.4.6): if desc_addr is not 64-byte aligned
     *      (desc_addr & 0x3F != 0), set thread_status_[trd_idx].cmd_error and
     *      .fail, assert trd_error_intr_status[trd_idx], call
     *      evaluate_interrupt_out() and return WITHOUT marking thread busy.
     *  5.  Mark thread BUSY in trd_status; set ctrl_status.ctrl_busy (bit 7)
     *      and ctrl_status.acmd_eng_busy (bit 2).
     *  6.  FETCHING_DESCRIPTOR: issue 64-byte DMA read via dma_read() using
     *      is_data_path=false (cdma_terr on AXI error with address capture).
     *      On AXI error: also set trd_error_intr_status[trd_idx], clear busy
     *      flags, call evaluate_interrupt_out() and return.
     *  7.  Decode 64-byte descriptor fields (little-endian layout, Section 7.4.3):
     *      - bytes  0– 7: next_pointer       (uint64_t LE)
     *      - bytes  8–15: system_mem_pointer (uint64_t LE)
     *      - bytes 16–23: xspi_pointer       (uint64_t LE)
     *      - bytes 24–31: reserved
     *      - bytes 32–33: cmd_type           (uint16_t LE)
     *      - bytes 34–35: cmd_flags          (uint16_t LE)
     *      - bytes 36–37: cmd_counter        (uint16_t LE) — count minus 1
     *      - bytes 38–39: reserved (upper half of Word 4)
     *      - bytes 40–43: status             (uint32_t, writeback slot)
     *      - bytes 44–63: reserved
     *  8.  EXECUTING_COMMAND — descriptor validation (Section 7.4.3):
     *      a. CMD_TYPE must be one of: 0x1000, 0x1001, 0x1100, 0x1101, 0x2100,
     *         0x2200; any other value → DSC_ERROR in descriptor status, transition
     *         to ERROR (set trd_error_intr_status, write back status, return).
     *      b. MB_XIP_EN (cmd_flags bit 6) is valid only on READ (0x2200);
     *         on any other CMD_TYPE → DSC_ERROR, ERROR path as above.
     *      c. next_pointer must be zero or 64-byte aligned; misaligned non-zero
     *         next_pointer → DSC_ERROR, ERROR path.
     *  9.  Dispatch per CMD_TYPE:
     *      - 0x2200 READ:         flash READ via xspi_bus_socket[BANK];
     *                              DMA WRITE result to sys_mem_ptr via dma_write()
     *                              (ddma_terr on data DMA error).
     *                              MB_XIP_EN (bit 6) sets mode_byte on READ ext.
     *      - 0x2100 PROGRAM:       DMA READ from sys_mem_ptr via dma_read();
     *                              WREN + PAGE_PROGRAM on xspi_bus_socket[BANK].
     *      - 0x1000 ERASE_SECTORS: WREN + ERASE_64KB on xspi_bus_socket[BANK];
     *                              cmd_counter+1 sectors; address from xspi_pointer.
     *      - 0x1001 FULL_CHIP_ERASE: WREN + chip erase (0xDC in LT); no address/count.
     *      - 0x1100 DEVICE_RESET:  RESET (0xFF) command; no address/data.
     *      - 0x1101 JEDEC_RESET:   JEDEC RESET (0xF0) command; no address/data.
     * 10.  UPDATING_STATUS (writeback, Section 7.4.3): build 8-byte status word
     *      (uint32_t status at offset +40; upper 4 bytes zero); COMPLETE bit (15)
     *      always set; FAIL bit (14) and error bits set on failure paths. Write
     *      8 bytes to (desc_current_addr + 40) via dma_write() is_data_path=false.
     *      On writeback AXI error: set cdma_terr; continue to interrupt reporting.
     * 11.  Chain evaluation: if CONT (cmd_flags bit 9) is set and next_pointer
     *      is non-zero, update desc_current_addr = next_pointer and loop back to
     *      step 6 (FETCHING_DESCRIPTOR). The INT flag from intermediate chain
     *      members is NOT consumed; only the final descriptor's INT flag matters.
     * 12.  COMPLETE: on success with INT (cmd_flags bit 8) set in the final
     *      descriptor, set trd_comp_intr_status[trd_idx] and call
     *      evaluate_interrupt_out(). On any error: always set
     *      trd_error_intr_status[trd_idx] regardless of INT flag.
     * 13.  Clear trd_status.trd_busy[trd_idx] and ctrl_status.ctrl_busy /
     *      ctrl_status.acmd_eng_busy.
     *
     * BANK selection: cmd_flags bits[2:0] provide the BANK/CS value within
     * the descriptor command flags (flags_bank accessor). This overrides
     * dac_cfg.active_bank for per-descriptor bank targeting.
     *
     * Single-writer compliance: no sc_signal or sc_out written directly.
     * Architecture reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *   state_machines.ACMD; docs/xspi_ctrl-detailed-design.md Section 7.4;
     *   docs/xspi_ctrl-functionality_list.md FUNC_XSPI_012.
     *
     * @param cmd_reg0_val The 32-bit value written to cmd_reg0 in ACMD mode.
     *                     Bits[26:24] = TRD_NUM; bits[31:30] must be 0b00.
     */
    void cdma_handle_trigger(uint32_t cmd_reg0_val);

    // =========================================================================
    // STIG Engine Helper Methods (FUNC_XSPI_009)
    // =========================================================================

    /**
     * @brief Decode the staged cmd_reg1–cmd_reg4 into a stig_instruction struct.
     *
     * UG Profile 1/Variant 1, Table 4.23 (command) and 4.27 (glued second phase)
     * when stig_instr_link_pending is set. 128b layout: reg1=bits[31:0] …
     * reg4=bits[127:96].
     *
     *   INSTR[6:0]      = cmd_reg1[6:0]
     *   DATA0, DATA1    = cmd_reg1[15:8], cmd_reg1[23:16]
     *   ADDR0           = cmd_reg1[31:24] (and ADDR1..5 in reg2, reg3 per Table 4.23)
     *   CMD, CMD_EXT    = cmd_reg3[23:16], cmd_reg3[15:8]
     *   DATA/MODE [1:0] = cmd_reg3[25:24]  (0/1/2 immediate bytes, not ACMD DATA_CNT)
     *   ADDR_NO[2:0]     = cmd_reg3[30:28]  (not expanded in this decode path; byte lanes fixed)
     *   BANK[2:0]       = cmd_reg4[14:12]
     *   INSTR_LINK      = cmd_reg4[28] (bit 124 of 128b word)
     *
     * @return Populated stig_instruction struct.
     * @see stig_v1_extract_address(), stig_glued_extract_nbytes_79_48()
     */
    stig_instruction decode_instruction();

    /**
     * @brief Assert STIG busy status bits in ctrl_status.
     *
     * Sets ctrl_status.ctrl_busy (bit 7) and ctrl_status.gcmd_eng_busy (bit 3).
     * Called at the start of every STIG engine activation from stig_engine_thread().
     * Does NOT write directly to any port (single-writer rule compliance).
     *
     * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.4,
     *       docs/xspi_ctrl-architecture-behaviour-map.json state_machines.STIG.
     */
    void stig_set_busy();

    /**
     * @brief Complete a STIG operation — clear busy bits and set completion status.
     *
     * Performs in order:
     *  1. Clears ctrl_status.ctrl_busy (bit 7) and ctrl_status.gcmd_eng_busy (bit 3).
     *  2. Sets cmd_status.COMPLETE (bit 15).
     *  3. Sets intr_status.stig_done (bit 23).
     *  4. Calls evaluate_interrupt_out() to propagate the interrupt.
     *
     * Called from stig_engine_thread() after the flash transaction completes
     * or after execute_stig() returns (whether with success or error).
     *
     * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.4,
     *       docs/xspi_ctrl-architecture-behaviour-map.json state_machines.STIG.
     */
    void stig_finish();

    /**
     * @brief Record a STIG command error in cmd_status error bits.
     *
     * Sets the BUS_ERROR bit (bit 1) in cmd_status to indicate that the
     * underlying flash bus transaction failed (dispatch_flash_transaction
     * returned false). Does not clear COMPLETE — that is done by stig_finish().
     *
     * @param msg  Descriptive error string for logging purposes.
     *
     * @note Reference: docs/xspi_ctrl-detailed-design.md Section 7.2.5,
     *       docs/xspi_ctrl-architecture-behaviour-map.json registers.cmd_status.
     */
    void stig_set_error(const char* msg);

    /**
     * @brief Dispatch the decoded STIG instruction to the appropriate handler.
     *
     * Reads inst.instr_type and dispatches to one of five handlers:
     *   XSPI_INSTR_READ  (1)   → handle_stig_read()
     *   XSPI_INSTR_WRITE (2)   → handle_stig_write()
     *   XSPI_INSTR_SFDP  (96)  → handle_stig_read_sfdp()
     *   XSPI_INSTR_GLUED (127) → handle_stig_merged_read() / handle_stig_merged_write()
     *   ERASE_SUSPEND    (39)  → handle_stig_suspend_resume() (suspend)
     *   ERASE_RESUME     (40)  → handle_stig_suspend_resume() (resume)
     *   PROG_SUSPEND     (41)  → handle_stig_suspend_resume() (prog suspend)
     *   PROG_RESUME      (42)  → handle_stig_suspend_resume() (prog resume)
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
     */
    bool execute_stig(const stig_instruction& inst,
                      const stig_instruction* data_phase);

    /**
     * @brief STIG READ handler — issue a READ transaction on xspi_bus_socket.
     *
     * Constructs a cdns_extension from inst fields (opcode, address, data_bytes_count,
     * I/O widths, edge modes), sets instr_type=XSPI_INSTR_READ (1), and
     * dispatches via dispatch_flash_transaction() as a TLM_READ_COMMAND.
     * Received data is stored in a local stig_data_buf (not propagated to
     * memory in the LT model; the flash stub returns whatever it generates).
     *
     * @param inst        Decoded STIG READ instruction.
     * @param data_phase  For glued INSTR_LINK chains, the data phase; nullptr
     *                    for normal single-phase READ.
     * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
     *
     * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009
     *       "STIG READ handler"; docs/xspi_ctrl-detailed-design.md Section 7.2.4.
     */
    bool handle_stig_read(const stig_instruction& inst,
                          const stig_instruction* data_phase);

    /**
     * @brief STIG WRITE handler — issue WREN then WRITE transaction on xspi_bus_socket.
     *
     * Per the hardware architecture, a STIG WRITE operation consists of two
     * sequential flash bus transactions:
     *  1. WREN command (opcode = seq_cfg.we_cmd_val, default 0x06, no data).
     *  2. WRITE/PROGRAM command (opcode from inst.opcode, data from inst.write_data
     *     and inst.data_bytes_count) as a TLM_WRITE_COMMAND.
     *
     * Both transactions target the same bank (inst.bank_num / dac_cfg.active_bank).
     *
     * @param inst  Decoded STIG WRITE instruction.
     * @return true if both WREN and WRITE dispatch_flash_transaction() calls
     *         returned TLM_OK_RESPONSE; false if either fails.
     *
     * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009
     *       "STIG WRITE handler"; docs/xspi_ctrl-detailed-design.md Section 7.2.4.
     */
    bool handle_stig_write(const stig_instruction& inst);

    bool handle_stig_merged_read(const stig_instruction& cmd_phase,
                                 const stig_instruction& data_phase);

    bool handle_stig_merged_write(const stig_instruction& cmd_phase,
                                  const stig_instruction& data_phase);

    /**
     * @brief STIG control command handler — issue a command-only or short-read transaction.
     *
     * Issues a single flash transaction using the opcode encoded in inst.opcode.
     * Handles WREN (0x06), WRDI (0x04), and RDSR (0x05) as the three canonical
     * control commands. WREN and WRDI are command-only (no data phase);
     * RDSR is a 1-byte READ with TLM_READ_COMMAND direction.
     *
     * For unrecognised opcodes (e.g., XSPI_INSTR_GENERIC / custom control),
     * the handler uses the direction implied by data_bytes_count: if zero,
     * issues as write (command-only); if non-zero, issues as read.
     *
     * @param inst        Decoded STIG control instruction.
     * @param data_phase  Pointer to glued data phase for INSTR_LINK; nullptr
     *                    for single-phase.
     * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
     *
     * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009
     *       "STIG control command handler"; docs/xspi_ctrl-detailed-design.md
     *       Section 7.2.4.
     */
    bool handle_stig_control_command(const stig_instruction& inst,
                                     const stig_instruction* data_phase);

    /**
     * @brief STIG suspend/resume handler — issue erase/program suspend or resume.
     *
     * Issues a single command-only flash transaction using the opcode already
     * encoded in inst.opcode (placed there by software from SFDP DWORD 13
     * values previously read from stat_seq_cfg_8/9). The handler does not
     * independently look up opcodes from sequence registers; the opcode comes
     * directly from the staged cmd_reg1[31:24] value.
     *
     * The handler issues a TLM_WRITE_COMMAND with data_bytes=0 (command-only).
     *
     * @param inst  Decoded STIG suspend/resume instruction.
     * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
     *
     * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009
     *       "STIG suspend/resume handler"; docs/xspi_ctrl-detailed-design.md
     *       Section 7.2.4.
     */
    bool handle_stig_suspend_resume(const stig_instruction& inst);

    /**
     * @brief STIG READ_SFDP handler — issue READ_SFDP (opcode 0x5A) transaction.
     *
     * Constructs a cdns_extension with opcode=0x5A (READ_SFDP), address from
     * inst.address, data_bytes from inst.data_bytes_count, instr_type set to
     * XSPI_INSTR_SFDP (96), and dispatches via dispatch_flash_transaction()
     * as a TLM_READ_COMMAND on the active flash bank.
     *
     * SFDP data returned from the flash stub is stored in a local buffer.
     * In the LT model, the data is not propagated to registers; the completion
     * status is sufficient for test verification.
     *
     * @param inst        Decoded STIG READ_SFDP instruction.
     * @param data_phase  Pointer to glued data phase; nullptr for single-phase.
     * @return true if dispatch_flash_transaction() returned TLM_OK_RESPONSE.
     *
     * @note Reference: docs/xspi_ctrl-functionality_list.md FUNC_XSPI_009
     *       "STIG READ_SFDP handler"; docs/xspi_ctrl-detailed-design.md
     *       Section 7.2.4.
     */
    bool handle_stig_read_sfdp(const stig_instruction& inst,
                                const stig_instruction* data_phase);

    // =========================================================================
    // SC_THREAD and SC_METHOD Process Declarations
    // =========================================================================

    /**
     * @brief STIG engine SC_THREAD — FUNC_XSPI_009.
     *
     * Permanently running SC_THREAD that blocks on cmd_trigger_event. Woken
     * by handle_write_cmd_reg0 in STIG mode. On each wake:
     *  1. Calls decode_instruction() to assemble stig_instruction from registers.
     *  2. Calls stig_set_busy() to assert ctrl_status.gcmd_eng_busy.
     *  3. Handles INSTR_LINK first-phase (sets gcmd_eng_mc_busy, arms chain).
     *  4. On second phase or single-phase: calls execute_stig() which dispatches
     *     to one of five handlers (READ, WRITE, control, suspend/resume, SFDP).
     *  5. Calls stig_finish() to clear busy bits, set COMPLETE, set stig_done.
     *
     * Sensitivity: cmd_trigger_event.
     * Driven state: ctrl_status (gcmd_eng_busy, gcmd_eng_mc_busy, ctrl_busy),
     *               cmd_status (COMPLETE, BUS_ERROR), intr_status (stig_done).
     */
    void stig_engine_thread();

    /**
     * @brief Sole writer of int_out — SC_METHOD process.
     *
     * Sensitive to m_int_update_event only. This is the ONLY SystemC process
     * permitted to call int_out.write(), enforcing the single-writer rule
     * (SystemC LRM E115).
     *
     * Computes the logical OR of all three interrupt contribution paths and
     * drives int_out to reflect the current masked interrupt state:
     *
     *  Path 1 — General interrupt (intr_status AND intr_enable):
     *    Active when intr_enable.intr_en (global gate) is set AND any
     *    intr_status bit has a corresponding intr_enable bit set.
     *    Only bits within intr_status.write_bit_mask (0x1FF7F000) are
     *    evaluated so reserved and RO fields are excluded.
     *
     *  Path 2a — Thread completion (trd_comp_intr_status):
     *    Active when intr_enable.intr_en (bit 31) is set and
     *    trd_comp_intr_status[7:0] != 0. No trd_comp_intr_en gate (KB).
     *
     *  Path 2b — Thread error (trd_error_intr_status AND trd_error_intr_en):
     *    Active when the global intr_en gate is set and
     *    (trd_error_intr_status[7:0] &
     *     trd_error_intr_en.trd_error_intr_en_field) != 0.
     *
     * Sensitivity: m_int_update_event (dont_initialize).
     * Sole driven output: int_out.
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            events/interrupt_matrix; docs/xspi_ctrl-detailed-design.md
     *            Sections 9.1, 9.3, 11.3.
     */
    void update_int_out();

    /**
     * @brief Reset handler SC_METHOD.
     *
     * Sensitive to the falling edge of reset_in (active-low). Performs the
     * following sequence in order:
     *  1. Clears all internal thread-state variables (current_work_mode,
     *     thrd_status_sel, active_dac_bank, rmp_addr_en, xip_exit_pending,
     *     active_device_profile, tcms_enabled, remap_offset, xip_active_banks).
     *  2. Clears interrupt-related shadow values (long_polling_val,
     *     short_polling_val reset to hardware defaults 1000 / 500).
     *  3. Calls reset_all_registers() to restore all 86 register instances
     *     to their hardware reset values.
     *  4. Notifies m_int_update_event so that update_int_out() SC_METHOD
     *     re-evaluates int_out from the freshly reset register state.
     *     int_out is NOT written directly here — only update_int_out() may
     *     write int_out (single-writer rule compliance).
     *
     * Architecture note: The method fires on the falling edge of reset_in
     * (active-low assertion). De-assertion of reset_in is not handled here.
     *
     * Single-writer compliance: int_out is NEVER written directly in this
     * SC_METHOD. m_int_update_event.notify() delegates all int_out writes
     * to the dedicated update_int_out() SC_METHOD.
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            reset_behavior section; docs/xspi_ctrl-detailed-design.md
     *            Section 4.8 (reset_in) and Section 8.1 (PoR sequence).
     */
    void reset_handler();

    /**
     * @brief Notify m_int_update_event to schedule an int_out re-evaluation.
     *
     * This function does NOT write int_out directly. It notifies
     * m_int_update_event at SC_ZERO_TIME, which causes the update_int_out()
     * SC_METHOD to be scheduled for execution in the next delta cycle.
     * update_int_out() is the sole writer of int_out (single-writer rule).
     *
     * Called by every W1C write callback and every enable register write
     * callback to ensure int_out is re-evaluated whenever interrupt state
     * may have changed. Also called from stig_engine_thread after each
     * interrupt status update.
     *
     * No sensitivity list — called directly from callbacks and threads.
     * Does not write any register or output port directly.
     *
     * Reference: docs/xspi_ctrl-architecture-behaviour-map.json
     *            events/interrupt_matrix; docs/xspi_ctrl-detailed-design.md
     *            Section 9.1 (interrupt paths) and Section 9.3 (W1C clear).
     */
    void evaluate_interrupt_out();

    // =========================================================================
    // TLM b_transport Handlers
    // =========================================================================

    /**
     * @brief b_transport stub for t_axi_slave_socket (AXI slave interface).
     *
     * Handles Direct-mode memory-mapped flash transactions. In the full model,
     * read transactions are translated to flash READ commands on xspi_bus_socket
     * and write transactions to WREN+PAGE_PROGRAM sequences.
     *
     * @param trans TLM generic payload for the AXI slave transaction
     * @param delay Local time offset for temporal decoupling
     */
    void b_transport_axi_slave(tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay);

    /**
     * @brief b_transport handler for PoR_input_signals (power-on reset).
     *
     * Implements FUNC_XSPI_007: processes the xspi_PoR_trans extension at
     * power-on reset. Performs register reset, SFDP discovery (when
     * discovery_inhibit=0), sequence register auto-population, and
     * discovery_control / ctrl_status result reporting. When boot_en=1 and
     * boot_available=true the boot engine is invoked (FUNC_XSPI_010).
     * Output fields boot_comp and boot_error are written back into the
     * extension before b_transport returns.
     *
     * @param trans TLM generic payload carrying xspi_PoR_trans extension
     * @param delay Local time offset for temporal decoupling
     */
    void b_transport_por(tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay);

    // =========================================================================
    // SFDP Discovery Helpers (FUNC_XSPI_007)
    // =========================================================================

    /**
     * @brief Internal SFDP discovery engine.
     *
     * Executes the READ_SFDP_HEADER -> PARSE_PARAM_TABLE -> WRITE_SEQ_REGS ->
     * SUCCESS/FAIL state machine as described in
     * docs/xspi_ctrl-detailed-design.md Section 8.3.
     *
     * @param bank              xspi_bus_socket index for the READ_SFDP command
     * @param num_lines         discovery_num_lines encoding (0=Auto, 1,2,4,8,0xC,0xE)
     * @param abnum             0=3-byte addressing, 1=4-byte addressing
     * @param dummy_cnt         0=8 dummy cycles, 1=20 dummy cycles
     * @param cmd_type          0=SDR, 1=DDR, 2=DTR
     * @param extop_en          Extended opcode enable flag
     * @param extop_val         0=repetition (0x5A 0x5A), 1=negation (0x5A 0xA5)
     * @param crc_en            CRC enable (stored to global_seq_cfg; not computed)
     * @param crc_variant       CRC variant (stored; not computed)
     * @param crc_oe            CRC output enable (stored; not computed)
     * @param crc_chunk_size    CRC chunk size (stored; not computed)
     * @param[out] detected_abnum  Address bytes discovered (updated on success)
     * @param[out] detected_type   Device type found: 0=xSPI/NOR, 1=HyperFlash,
     *                             3=SPI-NAND; 0xFF=failed
     * @return true if a valid SFDP signature was received and table parsed
     */
    bool run_sfdp_discovery(unsigned int bank,
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
                            uint8_t&     detected_type);

    /**
     * @brief Issue a single READ_SFDP transaction on xspi_bus_socket[bank].
     *
     * Constructs a cdns_extension for opcode 0x5A (READ_SFDP) with the
     * supplied addressing and I/O-mode parameters and calls
     * dispatch_flash_transaction(). The received SFDP data is returned via
     * the data buffer pointed to by @p buf.
     *
     * @param bank       xspi_bus_socket index
     * @param address    SFDP ROM address to read from
     * @param buf        Caller-allocated buffer to receive data
     * @param len        Number of bytes to read (typically 8 or 64)
     * @param num_lines  I/O line width encoding
     * @param abnum      0=3-byte, 1=4-byte addressing
     * @param dummy_cnt  0=8 dummy cycles, 1=20 dummy cycles
     * @param cmd_type   0=SDR, 1=DDR, 2=DTR
     * @param extop_en   Extended opcode enable
     * @param extop_val  0=rep (0x5A 0x5A), 1=neg (0x5A 0xA5)
     * @return true if the transport returned TLM_OK_RESPONSE
     */
    bool issue_sfdp_read(unsigned int bank,
                         uint32_t     address,
                         uint8_t*     buf,
                         uint32_t     len,
                         uint8_t      num_lines,
                         uint8_t      abnum,
                         uint8_t      dummy_cnt,
                         uint8_t      cmd_type,
                         uint8_t      extop_en,
                         uint8_t      extop_val);

    /**
     * @brief Auto-configure all 10 sequence register groups from parsed SFDP table.
     *
     * Called after a successful SFDP READ_SFDP_HEADER + PARSE_PARAM_TABLE
     * sequence. Writes the 10 sequence register groups directly to their
     * internal scml2 register storage (bypassing write callbacks to avoid
     * re-entrancy) and updates the seq_cfg shadow struct to keep it consistent.
     *
     * Parsed from the 16-DWORD JESD216A basic flash parameter table:
     *   DWORD 1  bits[18:17]  address mode (0=3-byte, 1=3/4-byte, 2=4-byte)
     *   DWORD 2  bits[15:8]   page size (2^N bytes)
     *   DWORD 7  bits[15:8]   erase type 1 size; bits[23:16] erase type 1 opcode
     *   DWORD 8  bits[7:0]    erase type 2 size; bits[15:8] erase type 2 opcode
     *   DWORD 8  bits[23:16]  erase type 3 size; bits[31:24] erase type 3 opcode
     *   DWORD 9  bits[7:0]    erase type 4 size; bits[15:8] erase type 4 opcode
     *   DWORD 13 bits[7:0]    prog_resume_op; bits[15:8] prog_suspend_op;
     *            bits[23:16]  erase_resume_op; bits[31:24] erase_suspend_op
     *
     * @param param_table  16-element array of uint32_t DWORD values from SFDP ROM
     * @param abnum        Discovered address width (0=3-byte, 1=4-byte)
     * @param num_lines    I/O line encoding of the successful READ_SFDP attempt
     * @param dummy_cnt    Dummy cycle selector used during discovery
     * @param cmd_type     DDR/SDR selector used during discovery
     * @param crc_en       CRC enable stored into global_seq_cfg.seq_crc_en
     * @param crc_variant  CRC variant stored into global_seq_cfg.seq_crc_variant
     * @param crc_oe       CRC output enable stored into global_seq_cfg.seq_crc_oe
     * @param crc_chunk_size CRC chunk size stored into global_seq_cfg
     */
    void configure_registers_from_sfdp(const uint32_t param_table[16],
                                       uint8_t abnum,
                                       uint8_t num_lines,
                                       uint8_t dummy_cnt,
                                       uint8_t cmd_type,
                                       uint8_t crc_en,
                                       uint8_t crc_variant,
                                       uint8_t crc_oe,
                                       uint8_t crc_chunk_size);

    /**
     * @brief b_transport stub for t_reg_socket (register access).
     *
     * Structural stub retained for direct-bind test scenarios. In normal
     * scml2 operation the register file dispatch is managed internally by
     * the scml2 framework through memory.bind_to_socket(target_socket) in
     * xspi_ctrl_base.
     *
     * @param trans TLM generic payload
     * @param delay Local time offset for temporal decoupling
     */
    void b_transport_reg(tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay);

    // =========================================================================
    // Register Callback Declarations — Group 1: Command and Status (0x000)
    // =========================================================================

    /**
     * @brief Write callback for cmd_reg0 — primary dispatch trigger.
     * @param value 32-bit value written to cmd_reg0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_cmd_reg0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for cmd_status_ptr — thread selector update.
     * @param value 32-bit value written to cmd_status_ptr
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_cmd_status_ptr(uint32_t value, uint8_t be) override;

    /**
     * @brief Read callback for cmd_status — volatile thread status read.
     * @param[out] value Reference to receive dynamic register value
     * @return true on successful callback execution
     */
    bool handle_read_cmd_status(uint32_t& value) override;

    /**
     * @brief Write callback for intr_status — W1C interrupt clear.
     * @param value 32-bit mask (set bits clear corresponding interrupt flags)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_intr_status(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for intr_enable — interrupt enable mask update.
     * @param value 32-bit value written to intr_enable
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_intr_enable(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for trd_comp_intr_status — W1C thread completion clear.
     * @param value 32-bit mask (set bits clear thread completion interrupt flags)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_trd_comp_intr_status(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for trd_error_intr_status — W1C thread error clear.
     * @param value 32-bit mask (set bits clear thread error interrupt flags)
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_trd_error_intr_status(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for trd_error_intr_en — thread error enable update.
     * @param value 32-bit value written to trd_error_intr_en
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_trd_error_intr_en(uint32_t value, uint8_t be) override;

    // =========================================================================
    // Register Callback Declarations — Group 2: Controller Config (0x200)
    // =========================================================================

    /**
     * @brief Write callback for long_polling — polling count update.
     * @param value 32-bit value written to long_polling
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_long_polling(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for short_polling — short polling count update.
     * @param value 32-bit value written to short_polling
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_short_polling(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for ctrl_config — operating mode switch.
     * @param value 32-bit value written to ctrl_config
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_ctrl_config(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for dma_settings — DMA parameter update.
     * @param value 32-bit value written to dma_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_dma_settings(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for discovery_control — SFDP discovery parameters.
     * @param value 32-bit value written to discovery_control
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_discovery_control(uint32_t value, uint8_t be) override;

    // =========================================================================
    // Register Callback Declarations — Group 3: Common Sequence Config (0x380)
    // =========================================================================

    /**
     * @brief Write callback for xip_mode_cfg — XIP active banks update.
     * @param value 32-bit value written to xip_mode_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_xip_mode_cfg(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for global_seq_cfg — device profile selection.
     * @param value 32-bit value written to global_seq_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_global_seq_cfg(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for global_seq_cfg_1 — NAND spare area update.
     * @param value 32-bit value written to global_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_global_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for direct_access_cfg — DAC bank and remap update.
     * @param value 32-bit value written to direct_access_cfg
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_direct_access_cfg(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for direct_access_rmp — remap offset[31:0].
     * @param value 32-bit value written to direct_access_rmp
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_direct_access_rmp(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for direct_access_rmp_1 — remap offset[63:32].
     * @param value 32-bit value written to direct_access_rmp_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_direct_access_rmp_1(uint32_t value, uint8_t be) override;

    // =========================================================================
    // Register Callback Declarations — Group 4: Device Sequence Config (0x400)
    // =========================================================================

    /**
     * @brief Write callback for rst_seq_cfg_0 — RESET sequence parameters.
     * @param value 32-bit value written to rst_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_rst_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for rst_seq_cfg_1 — RESET sequence extension.
     * @param value 32-bit value written to rst_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_rst_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for ers_seq_cfg_0 — ERASE sequence parameters.
     * @param value 32-bit value written to ers_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_ers_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for ers_seq_cfg_1 — ERASE sequence timing.
     * @param value 32-bit value written to ers_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_ers_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for ers_seq_cfg_2 — ERASE sequence Profile 2.
     * @param value 32-bit value written to ers_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_ers_seq_cfg_2(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for prog_seq_cfg_0 — PROGRAM sequence parameters.
     * @param value 32-bit value written to prog_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_prog_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for prog_seq_cfg_1 — PROGRAM sequence extension.
     * @param value 32-bit value written to prog_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_prog_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for prog_seq_cfg_2 — PROGRAM sequence Profile 2.
     * @param value 32-bit value written to prog_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_prog_seq_cfg_2(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for read_seq_cfg_0 — READ sequence parameters.
     * @param value 32-bit value written to read_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_read_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for read_seq_cfg_1 — READ sequence extension.
     * @param value 32-bit value written to read_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_read_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for read_seq_cfg_2 — READ sequence Profile 2.
     * @param value 32-bit value written to read_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_read_seq_cfg_2(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for we_seq_cfg_0 — WRITE ENABLE sequence.
     * @param value 32-bit value written to we_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_we_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_0 — STATUS CHECK I/O config.
     * @param value 32-bit value written to stat_seq_cfg_0
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_0(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_1 — STATUS CHECK dummy count.
     * @param value 32-bit value written to stat_seq_cfg_1
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_1(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_2 — STATUS CHECK opcodes.
     * @param value 32-bit value written to stat_seq_cfg_2
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_2(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_3 — STATUS CHECK extension opcodes.
     * @param value 32-bit value written to stat_seq_cfg_3
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_3(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_4 — STATUS CHECK Profile 2 latency.
     * @param value 32-bit value written to stat_seq_cfg_4
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_4(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_5 — STATUS CHECK ready mask.
     * @param value 32-bit value written to stat_seq_cfg_5
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_5(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_7 — STATUS CHECK NAND fail masks.
     * @param value 32-bit value written to stat_seq_cfg_7
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_7(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_8 — STATUS CHECK erase-fail detect.
     * @param value 32-bit value written to stat_seq_cfg_8
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_8(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_9 — STATUS CHECK prog-fail detect.
     * @param value 32-bit value written to stat_seq_cfg_9
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_9(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for stat_seq_cfg_10 — STATUS CHECK NAND ECC detect.
     * @param value 32-bit value written to stat_seq_cfg_10
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_stat_seq_cfg_10(uint32_t value, uint8_t be) override;

    // =========================================================================
    // Register Callback Declarations — Group 5: Mini-Controller (0x1000)
    // =========================================================================

    /**
     * @brief Write callback for wp_settings — Write Protect pin control.
     * @param value 32-bit value written to wp_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_wp_settings(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for reset_pin_settings — hardware reset pin control.
     * @param value 32-bit value written to reset_pin_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_reset_pin_settings(uint32_t value, uint8_t be) override;

    /**
     * @brief Write callback for clock_mode_settings — SPI clock mode selection.
     * @param value 32-bit value written to clock_mode_settings
     * @param be    Byte-enable mask
     * @return true on successful callback execution
     */
    bool handle_write_clock_mode_settings(uint32_t value, uint8_t be) override;
};

/// @brief Convenience typedef so existing code using 'xspi_ctrl' as class name compiles
typedef xspi_ctrl_ip xspi_ctrl_model;
