// SPDX-License-Identifier: Apache-2.0
/**
 * @file pll_cntl.h
 * @brief SystemC/TLM-2.0 register model of the SMC PLL control block.
 *
 * Mirrors `pll_cntl.rdl` — the top-level PLL control/status registers (CGM &
 * AWM lock/status, AWM frequency-select control, AG mux selects, GPIO clock
 * observation, and the reference/clock counters).  32-bit registers spanning
 * 0x00..0x84.  Pure register file (see pll_reg_block.h); the GPIO_CLK_OBS_CTRL
 * postdiv_update_div bit is modelled as a single-pulse (self-clearing) field.
 */

#ifndef SMC_PLL_CNTL_H_
#define SMC_PLL_CNTL_H_

#include "pll_reg_block.h"

namespace smc {
namespace pll {

struct pll_cntl_cfg {
    /// Register span: last register AWM_1_CLOCK_0_COUNT_HI @0x84 (+4).
    static constexpr uint64_t WINDOW_SIZE = 0x88ULL;

    uint64_t base_addr       = 0x0ULL;
    double   access_delay_ns = 1.0;
};

/// Top-level PLL control/status register model (from pll_cntl.rdl).
class pll_cntl : public reg_block {
public:
    // Register offsets (bytes) — subset used by tests / enclosing models.
    static constexpr uint64_t OFF_CGM_0_STATUS      = 0x00ULL;
    static constexpr uint64_t OFF_CGM_1_STATUS      = 0x04ULL;
    static constexpr uint64_t OFF_AWM_0_CTRL        = 0x10ULL;
    static constexpr uint64_t OFF_AWM_0_STATUS      = 0x14ULL;
    static constexpr uint64_t OFF_AWM_1_CTRL        = 0x18ULL;
    static constexpr uint64_t OFF_AWM_1_STATUS      = 0x1CULL;
    static constexpr uint64_t OFF_AG_MUX_SELECT     = 0x20ULL;
    static constexpr uint64_t OFF_GPIO_CLK_OBS_CTRL = 0x24ULL;
    static constexpr uint64_t OFF_CLOCK_COUNTER_CTRL   = 0x30ULL;
    static constexpr uint64_t OFF_CLOCK_COUNTER_STATUS = 0x34ULL;
    static constexpr uint64_t OFF_REF_CLK_COUNT_PERIOD_LO = 0x38ULL;
    static constexpr uint64_t OFF_REF_CLK_COUNT_PERIOD_HI = 0x3CULL;

    // Bit positions of note.
    static constexpr uint32_t GPIO_POSTDIV_UPDATE_DIV = (1u << 16);

    explicit pll_cntl(sc_core::sc_module_name name,
                      pll_cntl_cfg cfg = pll_cntl_cfg{});
};

}  // namespace pll
}  // namespace smc

#endif  // SMC_PLL_CNTL_H_
