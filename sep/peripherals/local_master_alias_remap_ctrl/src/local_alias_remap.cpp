// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file local_alias_remap.cpp
 * @brief Implementation of local_alias_remap_ip functional model.
 *
 * Implements:
 *  - Constructor: mask derivation, socket registration
 *  - reset()              : resets base registers via reset_all_registers()
 *  - match_region()       : region lookup from axi_alias_remap.sv (LZC priority)
 *  - apply_offset()       : address translation for a matched region
 *  - override_cacheable() : region cacheable bit onto the outgoing transaction
 *  - data_b_transport()   : intercepts data-path AXI, remaps addr, forwards
 *  - data_transport_dbg() : GDB debug transport with same remap
 *  - get_region()         : testbench backdoor to read decoded region structs
 */

#include "local_alias_remap.h"

#include "sep_axi_extension.h"

// =============================================================================
// Constructor
// =============================================================================
local_alias_remap_ip::local_alias_remap_ip(sc_module_name n)
    : local_alias_remap_base(n, "local_alias_remap", MEMORY_SIZE)
    , data_socket("data_socket")
    , remapped_socket("remapped_socket")
    , rst_ni("rst_ni")
{
    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();
    // -----------------------------------------------------------------
    // Derive address masks from IDX_START
    // -----------------------------------------------------------------
    // lower_mask: bits [IDX_START-1 : 0] — preserved from input address
    lower_mask_ = (IDX_START > 0) ? ((1ULL << IDX_START) - 1ULL) : 0ULL;

    // upper_mask: bits [55 : IDX_START] — where the addition takes place
    upper_mask_ = ADDR_FIELD_MASK & ~lower_mask_;

    // -----------------------------------------------------------------
    // Register data-path socket callbacks
    // -----------------------------------------------------------------
    data_socket.register_b_transport(this,
        &local_alias_remap_ip::data_b_transport);
    data_socket.register_transport_dbg(this,
        &local_alias_remap_ip::data_transport_dbg);
}

// =============================================================================
// reset()
// =============================================================================
void local_alias_remap_ip::reset()
{
    reset_all_registers();
}

// =============================================================================
// get_region() — Testbench backdoor
// =============================================================================
local_alias_remap_ip::Region local_alias_remap_ip::get_region(uint32_t idx) const
{
    if (idx >= NUM_REGIONS)
        return Region{};  // return default-constructed (all zeros)

    Region r;
    // Extract fields from csml register field accessors
    // Note: CSML fields store [55:12] bits, need to shift left by 12 to get full address
    r.start_addr = static_cast<uint64_t>(REGION_START[idx].start_addr) << 12;
    r.end_addr   = static_cast<uint64_t>(REGION_END[idx].end_addr) << 12;
    r.offset     = static_cast<uint64_t>(REGION_ATTRS[idx].offset) << 12;
    r.cacheable  = static_cast<bool>(REGION_ATTRS[idx].cacheable);
    r.valid      = static_cast<bool>(REGION_ATTRS[idx].valid);

    return r;
}

// =============================================================================
// match_region() / apply_offset()
//
// Implements the axi_alias_remap.sv algorithm exactly:
//
//   for r = 0..15:
//     if valid[r] && addr >= region_start[r] && addr < region_end[r]:
//       upper   = addr[55:12] + offset[r][55:12]   // additive, overflow discarded
//       remapped = { upper[43:0], addr[11:0] }     // lower 12 bits preserved
//       break   // first match wins (LZC priority)
//   if no match: passthrough (addr unchanged)
// =============================================================================
int local_alias_remap_ip::match_region(uint64_t addr) const
{
    // Find first valid region where addr falls in [start, end). This implements
    // the LZC (leading-zero-count / priority encoder) behavior from RTL.
    for (uint32_t r = 0; r < NUM_REGIONS; ++r) {
        // Extract fields directly from csml field accessors
        // Note: CSML fields store [55:12] bits, need to shift left by 12 to get full address
        const uint64_t start_addr = static_cast<uint64_t>(REGION_START[r].start_addr) << 12;
        const uint64_t end_addr   = static_cast<uint64_t>(REGION_END[r].end_addr) << 12;
        const bool     valid      = static_cast<bool>(REGION_ATTRS[r].valid);

        if (!valid)
            continue;

        if (addr >= start_addr && addr < end_addr)
            return static_cast<int>(r);
    }

    // No match — passthrough (no_write_hit / no_read_hit path in RTL)
    return -1;
}

uint64_t local_alias_remap_ip::apply_offset(uint64_t addr, uint32_t idx) const
{
    const uint64_t offset = static_cast<uint64_t>(REGION_ATTRS[idx].offset) << 12;

    // Add offset upper bits to address upper bits.
    // RTL: prim_carry_select_adder on addr[55:12] + offset[55:12]
    // Overflow is discarded (sum is truncated)
    const uint64_t addr_upper   = (addr   & upper_mask_) >> IDX_START;
    const uint64_t offset_upper = (offset & upper_mask_) >> IDX_START;
    const uint64_t sum_upper    = (addr_upper + offset_upper)
                                & (upper_mask_ >> IDX_START); // discard overflow

    // Recombine — upper from addition, lower from original address
    return (sum_upper << IDX_START) | (addr & lower_mask_);
}

// =============================================================================
// cacheable override — see header. Saved and restored around the forward for the
// same reason the address is: the payload belongs to the initiator, and in RTL
// these are separate downstream wires rather than a mutation visible upstream.
// =============================================================================
bool local_alias_remap_ip::override_cacheable(tlm::tlm_generic_payload& trans,
                                               uint32_t idx) const
{
    sep::sep_axi_extension* ext = trans.get_extension<sep::sep_axi_extension>();
    if (!ext)
        return false;   // nothing to override; unextended payloads carry no cache attribute

    const bool previous = ext->cacheable;
    ext->cacheable = static_cast<bool>(REGION_ATTRS[idx].cacheable);
    return previous;
}

void local_alias_remap_ip::restore_cacheable(tlm::tlm_generic_payload& trans,
                                              bool previous) const
{
    if (sep::sep_axi_extension* ext = trans.get_extension<sep::sep_axi_extension>())
        ext->cacheable = previous;
}

// =============================================================================
// data_b_transport()
//
// Saves the original address and cache attribute, applies the region's
// translation and cacheable override, forwards the payload to remapped_socket,
// then restores both on the caller's payload.
// =============================================================================
void local_alias_remap_ip::data_b_transport(tlm::tlm_generic_payload& trans,
                                             sc_core::sc_time& delay)
{
    const uint64_t orig_addr = trans.get_address();
    const int      idx       = match_region(orig_addr);

    if (idx < 0) {                  // miss: address and cache attribute both pass through
        remapped_socket->b_transport(trans, delay);
        return;
    }

    const bool orig_cacheable = override_cacheable(trans, static_cast<uint32_t>(idx));
    trans.set_address(apply_offset(orig_addr, static_cast<uint32_t>(idx)));

    remapped_socket->b_transport(trans, delay);

    trans.set_address(orig_addr);   // restore
    restore_cacheable(trans, orig_cacheable);
}

// =============================================================================
// data_transport_dbg()
//
// Debug transport (GDB / memory inspector) — applies the same remap and
// forwards via remapped_socket->transport_dbg().
// =============================================================================
unsigned int local_alias_remap_ip::data_transport_dbg(tlm::tlm_generic_payload& trans)
{
    const uint64_t orig_addr = trans.get_address();
    const int      idx       = match_region(orig_addr);

    if (idx < 0)
        return remapped_socket->transport_dbg(trans);

    const bool orig_cacheable = override_cacheable(trans, static_cast<uint32_t>(idx));
    trans.set_address(apply_offset(orig_addr, static_cast<uint32_t>(idx)));

    unsigned int ret = remapped_socket->transport_dbg(trans);

    trans.set_address(orig_addr);   // restore
    restore_cacheable(trans, orig_cacheable);

    return ret;
}

// =============================================================================
// reset_handler — fires on any rst_ni edge; clears all regions when low
// =============================================================================
void local_alias_remap_ip::reset_handler()
{
    if (!rst_ni.read())
        reset();
}