// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/sep/inc/xbar_policy.h
//
// Connectivity matrix for the SEP local AXI crossbar, transcribed from
// tt-oca-hw meta/crossbars/configs/sep_local_axi_xbar.yaml.
//
// The RTL crossbar is not fully connected: each master reaches only a listed
// subset of subordinates, and an access outside that subset is a decode error
// rather than a permitted transfer.  Several of those denials are load-bearing
// for security — the external master cannot reach the core's TCMs or the reset
// controller, and the DMA cannot reach its own CSRs — so a fully-connected
// model lets VP traffic through that silicon would refuse.
//
// The peripherals block's own crossbar sits in front of this one for inbound
// external traffic; see inner_peripherals_claim() below.
//
// The matrix is keyed on the *subordinate*, resolved from the address, rather
// than on the VP's bus port index.  That is how the RTL decodes it, it survives
// bus ports being added or reordered, and it keeps this table diffable against
// the yaml.
//
// Two deliberate departures from the yaml, both because the VP's master set is
// coarser than the RTL's:
//
//   * The RTL splits the core into `ifu_sram`, `lsu` and `dbg`; the VP has one
//     initiator socket for all three, so `cpu` carries the union of their rows.
//     None of the three may reach `cpu_tcm` in the yaml because the core gets
//     at its TCMs internally, but the VP routes TCM accesses across this same
//     bus, so `cpu` must be allowed there.
//
//   * The local-alias remapper re-injects remapped traffic as its own bus
//     initiator, losing the identity of whoever originated it.  It is fed from
//     the core's alias window, so it inherits the core's rights; treating it as
//     a distinct, more restricted master would deny legitimate CPU traffic.
//
// Addresses that fall outside every RTL subordinate window are permitted for
// everyone.  Those are VP-only conveniences with no silicon counterpart (the
// boot ROM alias at 0x10040000, the console window, the upper reach of the
// alias data window), and denying them would break the platform without
// modelling anything real.
// ===========================================================================

#pragma once

#include <cstdint>

namespace sep_xbar {

// Subordinates, named as in the yaml's `outputs`.
enum class slave {
    cpu_tcm,
    sram,
    dma_csr,
    sep_wdt,
    sep_reset_ctrl,
    sep_crypto,
    sep_system_peripherals,
    sep_io,
    entropy_fifo,
    sep_external,
    unclassified,  // no RTL counterpart; see header comment
};

// Masters as the VP presents them; see the header for how these fold the
// yaml's five `inputs` down to four.
enum class master {
    cpu,          // yaml ifu_sram + lsu + dbg, plus the VP's TCM path
    dma,          // yaml dma (all three of the VP's DMA legs)
    local_alias,  // re-injected local-alias traffic; inherits cpu's rights
    ext,          // yaml ext — the SMN/SMC/AP inbound path
};

inline bool in_window(uint64_t addr, uint64_t base, uint64_t size) {
    return addr >= base && addr < base + size;
}

// Address -> subordinate, transcribed from the yaml's `outputs` address_ranges.
inline slave classify(uint64_t addr) {
    // iccm { 0xC0000000, 0x40000 } + dccm { 0xC0040000, 0x20000 }
    if (in_window(addr, 0xC0000000ULL, 0x60000ULL))    return slave::cpu_tcm;
    if (in_window(addr, 0x10000000ULL, 0x40000ULL))    return slave::sram;
    if (in_window(addr, 0x10800000ULL, 0x1000ULL))     return slave::dma_csr;
    if (in_window(addr, 0x10801000ULL, 0x1000ULL))     return slave::sep_wdt;
    if (in_window(addr, 0x10803000ULL, 0x8ULL))        return slave::sep_reset_ctrl;
    if (in_window(addr, 0x10900000ULL, 0x50000ULL))    return slave::sep_crypto;
    if (in_window(addr, 0x10950000ULL, 0x10000ULL))    return slave::entropy_fifo;
    // 0xFFFFF, not 0x100000: that is literally what the yaml says, so the last
    // byte of the megabyte (0x10BFFFFF) belongs to no subordinate at all. Almost
    // certainly a typo upstream, but this table's job is to mirror the yaml, and
    // "fixing" it here would hide the discrepancy rather than resolve it. Nothing
    // is mapped near the top of that range in the VP anyway, so the address fails
    // decode before the policy is ever consulted.
    if (in_window(addr, 0x10B00000ULL, 0xFFFFFULL))    return slave::sep_io;
    if (in_window(addr, 0x20000000ULL, 0x20000000ULL)) return slave::sep_external;
    // sep_system_peripherals claims five disjoint ranges.
    if (in_window(addr, 0x10802000ULL, 0x100ULL)       // scratch_region
     || in_window(addr, 0x10A00000ULL, 0x60000ULL)     // csr_region
     || in_window(addr, 0x11000000ULL, 0x1000000ULL)   // remap_region (AP + STEE)
     || in_window(addr, 0x00000000ULL, 0x10000000ULL)  // external_chiplet
     || in_window(addr, 0x40000000ULL, 0x80000000ULL)) // external_smu
        return slave::sep_system_peripherals;
    return slave::unclassified;
}

// The yaml's `connectivity` block.
inline bool permits(master m, slave s) {
    if (s == slave::unclassified) return true;

    switch (m) {
    case master::cpu:
    case master::local_alias:
        // Union of ifu_sram/lsu/dbg, plus cpu_tcm for the VP's TCM path.
        return true;

    case master::dma:
        // dma: [cpu_tcm, sram, sep_wdt, sep_reset_ctrl, sep_crypto,
        //       sep_system_peripherals, sep_io, sep_external]
        // Notably not dma_csr — the DMA cannot reprogram itself.
        return s != slave::dma_csr && s != slave::entropy_fifo;

    case master::ext:
        // ext: [sram, dma_csr, sep_wdt, sep_crypto, sep_io, entropy_fifo,
        //       sep_external].  The inbound filter is block-by-default on top
        //       of this; the crossbar denial is the backstop underneath it.
        return s != slave::cpu_tcm
            && s != slave::sep_reset_ctrl
            && s != slave::sep_system_peripherals;
    }
    return true;
}

// The peripherals block has a crossbar of its own in front of the local one
// (sep_system_peripherals_xbar_pkg.sv: inputs sep_local_from_remap and
// smn_inbound, outputs mailbox / system_csr / smn_inbound_from_xbar, with every
// input reaching every output).  RTL routes inbound SMN traffic into that
// crossbar first and forwards only what it does not claim to the local
// crossbar's ext port:
//
//   sep.sv  smn_inbound_axi_req_i -> u_sep_system_peripherals
//                                 -> smn_inbound_to_sep_axi -> ext_axi_req_i
//
// which is why the local matrix below denies ext -> sep_system_peripherals: by
// the time traffic reaches it, the peripherals have already had their turn.
// The VP flattens both crossbars onto one bus, so that first stage has to be
// modeled here, otherwise the SMC can never reach the SEP mailbox.
inline bool inner_peripherals_claim(uint64_t addr) {
    return in_window(addr, 0x10A00000ULL, 0x10000ULL)   // mailbox
        || in_window(addr, 0x10A10000ULL, 0x40000ULL)   // system_csr main
        || in_window(addr, 0x10802000ULL, 0x100ULL);    // system_csr scratch_region
}

// Both crossbar stages for one master and address, in RTL order.
inline bool permits(master m, uint64_t addr) {
    if (inner_peripherals_claim(addr))
        return true;
    return permits(m, classify(addr));
}

inline const char* name(slave s) {
    switch (s) {
    case slave::cpu_tcm:                return "cpu_tcm";
    case slave::sram:                   return "sram";
    case slave::dma_csr:                return "dma_csr";
    case slave::sep_wdt:                return "sep_wdt";
    case slave::sep_reset_ctrl:         return "sep_reset_ctrl";
    case slave::sep_crypto:             return "sep_crypto";
    case slave::sep_system_peripherals: return "sep_system_peripherals";
    case slave::sep_io:                 return "sep_io";
    case slave::entropy_fifo:           return "entropy_fifo";
    case slave::sep_external:           return "sep_external";
    case slave::unclassified:           return "unclassified";
    }
    return "?";
}

inline const char* name(master m) {
    switch (m) {
    case master::cpu:         return "cpu";
    case master::dma:         return "dma";
    case master::local_alias: return "local_alias";
    case master::ext:         return "ext";
    }
    return "?";
}

}  // namespace sep_xbar
