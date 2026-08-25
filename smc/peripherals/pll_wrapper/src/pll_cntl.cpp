// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file pll_cntl.cpp
 * @brief PLL control register table (from pll_cntl.rdl) + model construction.
 */

#include "pll_cntl.h"

namespace smc {
namespace pll {

namespace {

// Masks derived from the pll_cntl.rdl field definitions.
//   AWM_x_CTRL   : freq_sel_one_hot_clk0[2:0]=1, clk1[5:3]=2, clk2[8:6]=4,
//                  droop[18:16]  -> reset 0x111, bits 0x701FF
//   AWM_x_STATUS : lock_detect[2:0], debug_status[7:4], droop_interrupt[12:8]
//                  -> RO bits 0x1FF7
//   AG_MUX_SELECT: cgm_clkmux_sel[7:0], cgm_ag_mux_sel[15:8],
//                  awm_clkmux_sel[21:16], awm_ag_mux_sel[29:24] -> 0x3F3FFFFF
//   GPIO_CLK_OBS_CTRL: gpio_mux_sel[2:0], postdiv_divider[11:4]=0x50,
//                  postdiv_use_postdiv[12]=1, postdiv_update_div[16] (pulse),
//                  clk_obs_en[20] -> reset 0x1500, bits 0x111FF7, pulse 0x10000
//   CLOCK_COUNTER_{CTRL,STATUS}: cgm_0[3:0], cgm_1[7:4], awm_0[10:8],
//                  awm_1[14:12] -> 0x77FF
//
//                offset  name                       reset       rmask        wmask        sc
constexpr reg_spec kPllCntlRegs[] = {
    {0x00, "CGM_0_STATUS",             0x00000000u, 0x000000F1u, 0x00000000u, 0x00000000u},
    {0x04, "CGM_1_STATUS",             0x00000000u, 0x000000F1u, 0x00000000u, 0x00000000u},
    {0x10, "AWM_0_CTRL",               0x00000111u, 0x000701FFu, 0x000701FFu, 0x00000000u},
    {0x14, "AWM_0_STATUS",             0x00000000u, 0x00001FF7u, 0x00000000u, 0x00000000u},
    {0x18, "AWM_1_CTRL",               0x00000111u, 0x000701FFu, 0x000701FFu, 0x00000000u},
    {0x1C, "AWM_1_STATUS",             0x00000000u, 0x00001FF7u, 0x00000000u, 0x00000000u},
    {0x20, "AG_MUX_SELECT",            0x00000000u, 0x3F3FFFFFu, 0x3F3FFFFFu, 0x00000000u},
    {0x24, "GPIO_CLK_OBS_CTRL",        0x00001500u, 0x00111FF7u, 0x00111FF7u, 0x00010000u},
    {0x30, "CLOCK_COUNTER_CTRL",       0x00000000u, 0x000077FFu, 0x000077FFu, 0x00000000u},
    {0x34, "CLOCK_COUNTER_STATUS",     0x00000000u, 0x000077FFu, 0x00000000u, 0x00000000u},
    {0x38, "REF_CLK_COUNT_PERIOD_LO",  0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u},
    {0x3C, "REF_CLK_COUNT_PERIOD_HI",  0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u},
    {0x40, "CGM_0_CLOCK_0_COUNT_LO",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x44, "CGM_0_CLOCK_0_COUNT_HI",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x50, "CGM_1_CLOCK_0_COUNT_LO",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x54, "CGM_1_CLOCK_0_COUNT_HI",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x60, "AWM_0_CLOCK_0_COUNT_LO",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x64, "AWM_0_CLOCK_0_COUNT_HI",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x70, "AWM_0_CLOCK_2_COUNT_LO",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x74, "AWM_0_CLOCK_2_COUNT_HI",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x80, "AWM_1_CLOCK_0_COUNT_LO",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x84, "AWM_1_CLOCK_0_COUNT_HI",   0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
};

}  // namespace

pll_cntl::pll_cntl(sc_core::sc_module_name name, pll_cntl_cfg cfg)
    : reg_block(name, kPllCntlRegs,
                sizeof(kPllCntlRegs) / sizeof(kPllCntlRegs[0]),
                pll_cntl_cfg::WINDOW_SIZE, cfg.base_addr, cfg.access_delay_ns)
{
}

}  // namespace pll
}  // namespace smc
