// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file cgm.h
 * @brief SystemC/TLM-2.0 register model of the SMC PLL CGM block.
 *
 * Mirrors `cgm.rdl` (Clock Generation Module).  16-bit registers on 4-byte
 * strides spanning 0x00..0x90.  Pure register file (see pll_reg_block.h):
 * software RW/RO enforcement plus self-clearing REG_UPDATE / SAMPLE_STROBE
 * strobes; hardware-driven status (lock_detect, monitors) stays at reset and
 * may be driven via the `poke` back door.
 */

#ifndef SMC_PLL_CGM_H_
#define SMC_PLL_CGM_H_

#include "pll_reg_block.h"

namespace smc {
namespace pll {

struct cgm_cfg {
    /// Register span: last register SSC_HALF_PERIOD @0x90 (+4).
    static constexpr uint64_t WINDOW_SIZE = 0x94ULL;

    uint64_t base_addr       = 0x0ULL;
    double   access_delay_ns = 1.0;
};

/// Clock Generation Module register model (from cgm.rdl).
class cgm : public reg_block {
public:
    // Register offsets (bytes) — subset used by tests / enclosing models.
    static constexpr uint64_t OFF_ENABLES       = 0x00ULL;
    static constexpr uint64_t OFF_FCW_INT        = 0x04ULL;
    static constexpr uint64_t OFF_FCW_FRAC       = 0x08ULL;
    static constexpr uint64_t OFF_PREDIV         = 0x0CULL;
    static constexpr uint64_t OFF_REG_UPDATE     = 0x20ULL;
    static constexpr uint64_t OFF_LOCK_MONITOR   = 0x3CULL;
    static constexpr uint64_t OFF_SAMPLE_STROBE  = 0x40ULL;
    static constexpr uint64_t OFF_CGM_STATUS     = 0x4CULL;
    static constexpr uint64_t OFF_SSC_HALF_PERIOD = 0x90ULL;

    explicit cgm(sc_core::sc_module_name name, cgm_cfg cfg = cgm_cfg{});
};

}  // namespace pll
}  // namespace smc

#endif  // SMC_PLL_CGM_H_
