// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_filter_ctrl.h
 * @brief Generic SEP filter controller model — single class for both outbound and inbound.
 *
 * Instantiated twice in the VP:
 *   outbound_filter("outbound_filter", InstanceType::OUTBOUND)  CSR=0x10A20000, 32 entries
 *   inbound_filter ("inbound_filter",  InstanceType::INBOUND)   CSR=0x10A21000, 16 entries
 *
 * Sockets:
 *   target_socket   — CPU programs the filter CSR table (inherited from base)
 *   data_socket     — AXI data-path slave (incoming transactions to filter)
 *   filtered_socket — AXI data-path master (forwarded after filter pass)
 *
 * Filter algorithm (entry 0 = highest priority):
 *   for entry 0..num_instances-1:
 *     // traffic_filter.sv's filter_hit term — failing any of these means this
 *     // entry is not the match, so the scan moves on to the next one
 *     if !entry_enabled: skip (disabled)
 *     shift = allow_burst ? 12 : DATA_BUS_WIDTH   // 4KB page vs 8B word granularity
 *     if (addr>>shift) not in [start_addr>>shift, end_addr>>shift]: skip
 *     if src_id != 0 && sep_axi_extension.source_id != src_id: skip
 *     if sep_axi_extension.is_ns != allow_ns: skip
 *     if !allow_burst && data_length > 8B: skip
 *     // permission check at the winning entry — denies, does not defer
 *       READ  && !read_allowed  → TLM_ADDRESS_ERROR_RESPONSE
 *       WRITE && !write_allowed → TLM_ADDRESS_ERROR_RESPONSE
 *       else                    → forward via filtered_socket
 *   no match (BlockByDefault=1, matches RTL axi_filter_wrap for both SEP
 *   outbound_filter and inbound_filter instances) → TLM_ADDRESS_ERROR_RESPONSE,
 *   always — whether zero entries are enabled or N entries are enabled but
 *   none match. There is no "unconfigured = passthrough" state.
 *
 * filter_skip_i defeats all of the above: axi_filter_wrap.sv forces both
 * isolate_write and isolate_read low when it is high, so every transaction is
 * forwarded regardless of matching or permissions. RTL ties the outbound
 * instance to 1'b0 and drives the inbound one from feat_ctrl_o.sep_debug
 * (sep.sv:952). The port is optional — left unbound it ties low, which is the
 * correct constant for the outbound instance and a fail-closed default for the
 * inbound one until the platform wires SEP debug up.
 *
 * Note: the range check is masked to page/word granularity, NOT an exact
 * byte comparison — this matches axi_filter_wrap.sv's hardware behavior of
 * always widening the effective range to the enclosing page (burst-enabled
 * entries) or 8-byte word (non-burst entries). START_ADDR/END_ADDR are also
 * hw=rw in the RDL: whenever start_addr and end_addr fall in the same
 * page/word, hardware snaps the STORED register values to the full
 * page/word boundary (auto_correct_start_end()).
 */

#pragma once
#include "sep_filter_ctrl_base.h"
#include "reg_param.h"
#include "reg_logger.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

class sep_filter_ctrl_ip : public sep_filter_ctrl_base
{
public:
    SC_HAS_PROCESS(sep_filter_ctrl_ip);

    typedef typename regmodel::Reg<64>::DT DT;

    // =========================================================================
    // Instance selector — VP passes this instead of a raw entry count
    // =========================================================================
    enum class InstanceType { OUTBOUND, INBOUND };

    // =========================================================================
    // Hardware constants
    // =========================================================================
    static constexpr uint32_t MAX_INSTANCES          = 32;
    static constexpr uint32_t OUTBOUND_NUM_INSTANCES = 32;
    static constexpr uint32_t INBOUND_NUM_INSTANCES  = 16;
    static constexpr uint64_t ADDR_FIELD_MASK        = 0x00FFFFFFFFFFFFFFULL; // [55:0]
    static constexpr uint8_t  DATA_BUS_WIDTH         = 3; // hw=w, always 3 (64-bit bus)
    static constexpr unsigned SINGLE_BEAT_BYTES      = 8; // one 64-bit AXI beat; larger = burst

    // =========================================================================
    // Sockets
    // =========================================================================
    tlm_utils::simple_target_socket<sep_filter_ctrl_ip>    data_socket;
    tlm_utils::simple_initiator_socket<sep_filter_ctrl_ip> filtered_socket;

    /// Active-low asynchronous reset (matches arst_n in RTL)
    sc_core::sc_in<bool> rst_ni;

    /// Bypass all filter checking (axi_filter_wrap.sv filter_skip_i). Optional —
    /// reads as 0 while unbound, which is what RTL ties the outbound instance to.
    sc_core::sc_in<bool> filter_skip_i;

    regmodel::Param<int> verbosity;
    RegLogger            logger;

    // =========================================================================
    // Constructors
    //
    // Preferred (VP): pass InstanceType — entry count resolved in model.
    //   sep_filter_ctrl_ip("outbound_filter", InstanceType::OUTBOUND)
    //
    // Parametric (testbench / custom): pass explicit entry count.
    //   sep_filter_ctrl_ip("dut", 32)
    // =========================================================================
    sep_filter_ctrl_ip(sc_module_name n, InstanceType type);
    explicit sep_filter_ctrl_ip(sc_module_name n, uint32_t num_instances);

    void reset();

    // =========================================================================
    // Decoded filter entry (testbench backdoor)
    // =========================================================================
    struct FilterEntry {
        bool     read_allowed  = false;
        bool     write_allowed = false;
        bool     entry_enabled = false;
        bool     allow_ns      = false;
        uint8_t  src_id        = 0;
        uint8_t  group_id      = 0;
        bool     allow_burst   = false;
        bool     locked        = false;
        uint64_t start_addr    = 0;
        uint64_t end_addr      = 7;
    };
    FilterEntry get_filter_entry(uint32_t idx) const;

    bool handle_filter_config_write(uint32_t idx, DT value);
    bool handle_filter_config_read(uint32_t idx, DT& value);

private:
    /// Leaving filter_skip_i open is supported two different ways, because the
    /// two callers differ: before_end_of_elaboration() binds the tie-off for
    /// anything that runs the scheduler (sc_start requires every port bound, or
    /// it aborts with E109), while this guard covers the standalone testbenches,
    /// which drive transactions straight from sc_main and so never reach that
    /// callback. Either way an open port reads 0.
    bool filter_skip() const
    {
        return filter_skip_i.get_interface() && filter_skip_i.read();
    }

    void         before_end_of_elaboration() override;
    void         data_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int data_transport_dbg(tlm::tlm_generic_payload& trans);
    bool         check_and_forward(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void         reset_handler();

    /// Index of the first entry whose filter_hit term is satisfied, or -1 for
    /// none. Shared by the functional and debug paths so the two cannot drift.
    int          match_entry(const tlm::tlm_generic_payload& trans) const;

    /// Drives an otherwise unbound filter_skip_i. RTL ties the outbound
    /// instance's input to 1'b0, so this is the correct constant, not a stub.
    sc_core::sc_signal<bool> filter_skip_tie_low_;

    // Mirrors axi_filter_wrap.sv's combinational start/end auto-correction:
    // if start_addr and end_addr fall in the same page (allow_burst) or
    // same 8-byte word (!allow_burst), snap the STORED register values to
    // the full enclosing page/word boundary. Called after every write to
    // START_ADDR, END_ADDR, or FILTER_CONFIG (allow_burst can change which
    // granularity applies).
    void auto_correct_start_end(uint32_t idx);
};
