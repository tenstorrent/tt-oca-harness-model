// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/sep/inc/tcm_dma_policy.h
//
// The VeeR EL2 DMA slave's rule for the core's ICCM and DCCM, from
// el2_dma_ctrl.sv (dma_alignment_error). The SEP DMA reaches the TCMs through
// its AXI-Lite master, so every access is a word; the core refuses a word write
// that does not enable all four byte lanes, and the DMA sees an error response.
// ===========================================================================

#pragma once

#include "xbar_policy.h"

#include <cstdint>

namespace sep_tcm_dma {

// lanes: bit i set when data byte i is enabled.
inline bool write_refused(uint64_t addr, unsigned length, unsigned lanes) {
    if (sep_xbar::classify(addr) != sep_xbar::slave::cpu_tcm)
        return false;
    return length != 4 || (addr & 0x3) != 0 || (lanes & 0xF) != 0xF;
}

}  // namespace sep_tcm_dma
