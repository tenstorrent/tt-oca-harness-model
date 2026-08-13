/**
 * @file local_alias_remap.h
 * @brief Functional SystemC TLM model for the local_master_alias_remap_ctrl peripheral.
 *
 * Extends local_alias_remap_base (csml register layer) with:
 *  - data_socket     : AXI data-path slave — incoming transactions to remap
 *  - remapped_socket : AXI data-path master — forwarded to fabric after remap
 *
 * Named local_alias_remap_ip to avoid collision with the
 * 'local_alias_remap' namespace defined in local_alias_remap_register.h.
 *
 * Architecture:
 *
 *   local_alias_remap_base
 *       csml_memory<64>        memory
 *       target_socket     ─► CPU programs region table (inherited from base)
 *       REGION_START[16]  ─┐
 *       REGION_END[16]    ─┤─ backing store (csml register arrays)
 *       REGION_ATTRS[16]  ─┘
 *
 *   local_alias_remap_ip  (this class)
 *       data_socket       ─► incoming AXI from local masters (DMA, SPAcc, PKA)
 *       remapped_socket   ─► translated AXI to SEP system crossbar
 *
 * Remap algorithm (from axi_alias_remap.sv):
 *   for r = 0..15:
 *     if REGION_ATTRS[r].valid && addr >= REGION_START[r] && addr < REGION_END[r]:
 *       upper   = addr[55:12] + REGION_ATTRS[r].offset[55:12]   // additive, overflow discarded
 *       remapped = { upper[43:0], addr[11:0] }                  // lower 12 bits preserved
 *       break   // first match wins (LZC priority)
 *   if no match: passthrough (addr unchanged)
 *
 * On a hit the region's `cacheable` bit also replaces the transaction's cache
 * attribute, mirroring axi_alias_remap.sv:122,147 (`{CacheWidth{cacheable}}`);
 * on a miss the incoming value passes through. Since tlm_generic_payload has no
 * cache field, the bit travels on sep_axi_extension.
 *
 * Hardware constants (SEP local master alias remap):
 *   CSR_BASE     = 0x10A10000   (16 × 0x20 B = 0x200 B total CSR span)
 *   NUM_REGIONS  = 16
 *   IDX_START    = 12           (4 KB page granularity)
 */

#pragma once
#include "local_alias_remap_base.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

class local_alias_remap_ip : public local_alias_remap_base
{
public:
    SC_HAS_PROCESS(local_alias_remap_ip);

    typedef typename csml_reg<64>::DT DT;

    // =========================================================================
    // Hardware constants (SEP-specific, hardcoded)
    // =========================================================================
    static constexpr uint32_t NUM_REGIONS       = 16;            ///< Remap region count
    static constexpr uint32_t IDX_START         = 12;            ///< 4 KB page granularity
    static constexpr uint32_t MEMORY_SIZE       = 0x200; ///< 512 B (16 regions × 32B stride)

    // Field masks
    static constexpr uint64_t ADDR_FIELD_MASK   = 0x00FFFFFFFFFFF000ULL; // [55:12]
    static constexpr uint64_t ATTRS_MASK        = 0xC0FFFFFFFFFFF000ULL; // offset+cacheable+valid

    // =========================================================================
    // Data-path sockets (CSR access via inherited target_socket from base)
    // =========================================================================

    /// AXI data-path slave — incoming transactions from SEP local masters
    tlm_utils::simple_target_socket<local_alias_remap_ip> data_socket;

    /// AXI data-path master — remapped transactions forwarded to SEP system crossbar
    tlm_utils::simple_initiator_socket<local_alias_remap_ip> remapped_socket;

    /// Active-low asynchronous reset — clears all region registers when asserted
    sc_core::sc_in<bool> rst_ni;

    // =========================================================================
    // Constructor
    // Configuration is hardcoded internally (see static constexpr above).
    // @param n   SystemC module hierarchical name
    // =========================================================================
    explicit local_alias_remap_ip(sc_module_name n);

    /// Reset all region registers to 0 (all regions invalid)
    void reset();

    // =========================================================================
    // Testbench backdoor — read a decoded Region struct from csml registers
    // =========================================================================
    struct Region {
        uint64_t start_addr = 0;
        uint64_t end_addr   = 0;
        uint64_t offset     = 0;
        bool     cacheable  = false;
        bool     valid      = false;
    };
    Region get_region(uint32_t idx) const;

private:
    // =========================================================================
    // Derived masks (computed from IDX_START in constructor)
    // =========================================================================
    uint64_t lower_mask_;  ///< bits [IDX_START-1 : 0]   — preserved from input
    uint64_t upper_mask_;  ///< bits [55 : IDX_START]     — where addition happens

    // =========================================================================
    // Data-path transport
    // =========================================================================
    void         data_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int data_transport_dbg(tlm::tlm_generic_payload& trans);
    void         reset_handler();

    // =========================================================================
    // Core remap — implements axi_alias_remap.sv LZC + carry-select adder
    // =========================================================================

    /// Index of the first valid region containing addr, or -1 for none. Match and
    /// translation are separate steps because the cacheable override needs the
    /// winning region, not just the translated address.
    int match_region(uint64_t addr) const;

    /// The address translation for a known-matching region.
    uint64_t apply_offset(uint64_t addr, uint32_t idx) const;

    /// Applies REGION_ATTRS[idx].cacheable to the outgoing transaction, returning
    /// the previous value so the caller can restore it. axi_alias_remap.sv:122,147
    /// drives aw.cache/ar.cache from the region's bit on a hit and passes the
    /// incoming value through on a miss; this is the same override, carried on
    /// sep_axi_extension because tlm_generic_payload has no cache field.
    bool  override_cacheable(tlm::tlm_generic_payload& trans, uint32_t idx) const;
    void  restore_cacheable(tlm::tlm_generic_payload& trans, bool previous) const;
};
