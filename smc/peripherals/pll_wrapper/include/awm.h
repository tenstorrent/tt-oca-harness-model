// SPDX-License-Identifier: Apache-2.0
/**
 * @file awm.h
 * @brief SystemC/TLM-2.0 register model of the SMC AWM block (awm.rdl).
 *
 * `awm.rdl` is a composite address map: a GLOBAL sub-block plus six identical
 * FREQUENCYn sub-blocks and three identical CGMn sub-blocks:
 *
 *   GLOBAL       @0x000
 *   FREQUENCY0   @0x100   FREQUENCY1 @0x140   FREQUENCY2 @0x180
 *   FREQUENCY3   @0x1C0   FREQUENCY4 @0x200   FREQUENCY5 @0x240
 *   CGM0         @0x280   CGM1       @0x380   CGM2       @0x480
 *
 * `awm_wrap.rdl` instantiates a single `awm @0x0` (its paired droop detector is
 * stubbed), so this model represents the whole awm_wrap window.  Pure register
 * file (see pll_reg_block.h) with self-clearing GLOBAL REG_UPDATE strobes.
 */

#ifndef SMC_PLL_AWM_H_
#define SMC_PLL_AWM_H_

#include "pll_reg_block.h"

namespace smc {
namespace pll {

struct awm_cfg {
    /// Full awm_wrap window (awm needs 0x500 of address space per awm_wrap.rdl).
    static constexpr uint64_t WINDOW_SIZE = 0x500ULL;

    uint64_t base_addr       = 0x0ULL;
    double   access_delay_ns = 1.0;
};

/// AWM (Adaptive Waveform Manager) composite register model (from awm.rdl).
class awm : public reg_block {
public:
    // Sub-block base offsets within the awm window.
    static constexpr uint64_t OFF_GLOBAL     = 0x000ULL;
    static constexpr uint64_t OFF_FREQUENCY0 = 0x100ULL;
    static constexpr uint64_t FREQUENCY_STRIDE = 0x040ULL;
    static constexpr uint64_t OFF_CGM0       = 0x280ULL;
    static constexpr uint64_t OFF_CGM1       = 0x380ULL;
    static constexpr uint64_t OFF_CGM2       = 0x480ULL;

    // A few frequently-used register offsets inside GLOBAL.
    static constexpr uint64_t GLOBAL_RESOURCE_CONFIG_ENABLES = 0x000ULL;
    static constexpr uint64_t GLOBAL_FCW_INT_BOUND           = 0x01CULL;
    static constexpr uint64_t GLOBAL_REG_UPDATE              = 0x028ULL;
    static constexpr uint64_t GLOBAL_LOCK_STATUS             = 0x098ULL;

    /// Absolute offset of FREQUENCYn (n in 0..5).
    static constexpr uint64_t frequency_base(unsigned n) {
        return OFF_FREQUENCY0 + FREQUENCY_STRIDE * n;
    }

    explicit awm(sc_core::sc_module_name name, awm_cfg cfg = awm_cfg{});
};

}  // namespace pll
}  // namespace smc

#endif  // SMC_PLL_AWM_H_
