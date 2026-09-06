// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_output_remap_ctrl.h
 * @brief Generic SEP output remap controller — single model, two instances.
 *
 * Extends sep_output_remap_ctrl_base (regmodel register layer) with:
 *  - data_socket      : AXI data-path slave  — incoming transactions to remap
 *  - remapped_socket  : AXI data-path master — forwarded to fabric after remap
 *  - remap algorithm  : matches output_remap.sv exactly
 *
 * Named sep_output_remap_ctrl_ip to avoid collision with the
 * 'sep_output_remap_ctrl' namespace defined in sep_output_remap_ctrl_register.h
 * (mirrors the mailbox → mailbox_ip naming pattern).
 *
 * Instantiated twice in och_sep_ss.hpp:
 *   sep_output_remap_ctrl_ip ap_output_remap  ("ap_output_remap",   InstanceType::AP);
 *   sep_output_remap_ctrl_ip stee_output_remap("stee_output_remap", InstanceType::STEE);
 *
 * Architecture:
 *
 *   sep_output_remap_ctrl_base
 *       regmodel::Memory<64>  memory
 *       target_socket    ──► SW reads/writes REGION_ATTRS[0..15] offsets
 *       REGION_ATTRS[16] ──► 16-entry 56-bit offset table
 *
 *   sep_output_remap_ctrl_ip  (this class)
 *       data_socket      ──► incoming AXI transactions (AP or STEE window)
 *       remapped_socket  ──► translated AXI to fabric
 *
 * Remap algorithm (from output_remap.sv):
 *   adjusted = addr - REGION_BASE
 *   idx      = adjusted[IDX_START + log2(NUM_REGIONS) - 1 : IDX_START]
 *   remapped = { REGION_ATTRS[idx][55:IDX_START], adjusted[IDX_START-1:0] }
 */

#pragma once
#include "sep_output_remap_ctrl_base.h"
#include "reg_param.h"
#include "reg_logger.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

class sep_output_remap_ctrl_ip : public sep_output_remap_ctrl_base
{
public:
    SC_HAS_PROCESS(sep_output_remap_ctrl_ip);

    typedef typename regmodel::Reg<64>::DT DT;

    // =========================================================================
    // Instance selector — VP passes this instead of raw hardware constants
    // =========================================================================
    enum class InstanceType { AP, STEE };

    // =========================================================================
    // Per-instance hardware constants (fixed in silicon)
    // =========================================================================
    static constexpr uint64_t AP_REGION_BASE   = 0x11000000ULL;
    static constexpr uint32_t AP_NUM_REGIONS   = 16;
    static constexpr uint32_t AP_IDX_START     = 19;

    static constexpr uint64_t STEE_REGION_BASE = 0x11800000ULL;
    static constexpr uint32_t STEE_NUM_REGIONS = 16;
    static constexpr uint32_t STEE_IDX_START   = 19;

    // =========================================================================
    // Fixed hardware limits
    // =========================================================================
    static constexpr uint32_t MAX_REGIONS = 16;
    static constexpr uint32_t MEMORY_SIZE = MAX_REGIONS * 8;  ///< 128 B CSR space

    // =========================================================================
    // Hardware constants that vary per instance — stored from constructor
    // =========================================================================
    const uint64_t REGION_BASE;  ///< Data-path window base (AP=0x11000000, STEE=0x11800000)
    const uint32_t NUM_REGIONS;  ///< Number of remap regions (power-of-2, max 16)
    const uint32_t IDX_START;    ///< Granularity bit (19 → 512 KB)

    // =========================================================================
    // Data-path sockets (not in base — functional layer only)
    // =========================================================================

    /// AXI data-path slave — incoming transactions to be remapped
    tlm_utils::simple_target_socket<sep_output_remap_ctrl_ip> data_socket;

    /// AXI data-path master — remapped transactions forwarded to fabric
    tlm_utils::simple_initiator_socket<sep_output_remap_ctrl_ip> remapped_socket;

    /// Active-low asynchronous reset (matches arst_n in RTL)
    sc_core::sc_in<bool> rst_ni;

    regmodel::Param<int> verbosity;
    RegLogger            logger;

    // =========================================================================
    // Constructors
    //
    // Preferred (VP): pass InstanceType — hardware constants resolved in model.
    //   sep_output_remap_ctrl_ip("ap_output_remap", InstanceType::AP)
    //
    // Parametric (testbench / custom): pass explicit hardware constants.
    //   sep_output_remap_ctrl_ip("dut", 0x11000000UL, 16, 19)
    // =========================================================================
    sep_output_remap_ctrl_ip(sc_module_name n, InstanceType type);

    sep_output_remap_ctrl_ip(sc_module_name n,
                              uint64_t region_base,
                              uint32_t num_regions = 16,
                              uint32_t idx_start   = 19);

    /// Reset all REGION_ATTRS to their RDL reset value (0x0)
    void reset();

private:
    void reset_handler();
    // =========================================================================
    // Derived masks (computed once in constructor)
    // =========================================================================
    uint32_t idx_width_;   ///< log2(NUM_REGIONS)
    uint64_t lower_mask_;  ///< bits [IDX_START-1 : 0]
    uint64_t idx_mask_;    ///< bits [IDX_START + idx_width - 1 : IDX_START]

    // =========================================================================
    // Core remap — implements output_remap.sv algorithm
    // =========================================================================
    uint64_t remap_address(uint64_t addr) const;

    /// Re-tag the payload's source ID to OTHERS_SOURCE_ID for the downstream
    /// forward, returning the caller's value so it can be put back. Returns
    /// false when the payload carries no sep_axi_extension, in which case there
    /// is no source ID to override and restore_source_id() is a no-op.
    bool override_source_id(tlm::tlm_generic_payload& trans, uint8_t& previous) const;
    void restore_source_id(tlm::tlm_generic_payload& trans, uint8_t previous) const;

    // =========================================================================
    // Data-path socket transport (registered in constructor)
    // =========================================================================
    void         data_b_transport(tlm::tlm_generic_payload& trans,
                                   sc_core::sc_time& delay);
    unsigned int data_transport_dbg(tlm::tlm_generic_payload& trans);
};
