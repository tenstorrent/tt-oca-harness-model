// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI clock helpers — frequency-robust SCLK.
 *
 * The OT spi_host generates SCLK = core_clk / (2 * (clkdiv + 1)), and its clk_i
 * is the SEP core clock. A hardcoded clkdiv therefore makes SCLK scale with the
 * core clock (e.g. clkdiv=9 gave 40 MHz @800 but only 5 MHz @100).
 *
 * The Cadence xSPI controller uses its own divider register. Both controllers
 * share the same source of truth for the active DV core clock: directed eFuse
 * preload content in SYSCLK_FREQ_MHZ.sysclk_freq_mhz.
 *
 * Target = 25 MHz. The divider is coarse at a 100 MHz core (only clkdiv=0 -> 50
 * MHz or clkdiv=1 -> 25 MHz are reachable, nothing between), and 50 MHz proved
 * too fast @800: spi_ot_dual_spi read the RX FIFO while rdata_o was still X
 * (prim_fifo_sync DataKnown_A). 25 MHz is the highest CONSTANT SCLK that is safe
 * for all OT tests at both frequencies: clkdiv=15 -> 800/32 = 25 MHz @800,
 * clkdiv=1 -> 100/4 = 25 MHz @100. Well within the modeled flash devices'
 * rating (S25FL064L 108 MHz, W25Q128JV 104 MHz).
 *
 * SPI tests should use eFuse/shadow preloads whose sysclk_freq_mhz value matches
 * the simulated core clock. If unset (0), helpers fall back to the 100 MHz
 * reference clock.
 */
#ifndef SPI_CLK_H
#define SPI_CLK_H

#include <stdint.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"

#define SPI_TARGET_SCLK_MHZ  25u

/* Real core clock (MHz) from the sensed eFuse sysclk_freq_mhz field; 0 -> 100
 * (reference-clock fallback, matching ROM pll_init). */
static inline uint32_t spi_core_mhz(void)
{
    SEP_EFUSE_MAP_SYSCLK_FREQ_MHZ_reg_u ef;
    ef.val = READ_REG(SEP_EFUSE_MAP_SYSCLK_FREQ_MHZ_REG_ADDR);
    uint32_t f = (uint32_t)ef.f.sysclk_freq_mhz;
    return f ? f : 100u;
}

/* OT spi_host clkdiv for a target SCLK:
 *   SCLK = core / (2*(clkdiv+1))
 *   => clkdiv = core/(2*target) - 1, clamped to [0, 0xFFFF]. */
static inline uint16_t spi_clkdiv_for(uint32_t target_mhz)
{
    uint32_t core = spi_core_mhz();
    uint32_t d2   = 2u * target_mhz;
    uint32_t div  = (core > d2) ? (core / d2 - 1u) : 0u;
    if (div > 0xFFFFu) div = 0xFFFFu;
    return (uint16_t)div;
}

/* clkdiv for the default 25 MHz target (15 @800, 1 @100). */
static inline uint16_t spi_clkdiv(void)
{
    return spi_clkdiv_for(SPI_TARGET_SCLK_MHZ);
}

/* Cadence xSPI divider value for a target SCLK. Existing Cadence tests program
 * clock_divider_value = core / target. Clamp to the 8-bit register field. */
static inline uint8_t spi_cdns_clkdiv_for(uint32_t target_mhz)
{
    uint32_t target = target_mhz ? target_mhz : SPI_TARGET_SCLK_MHZ;
    uint32_t div = spi_core_mhz() / target;
    if (div > 0xFFu) div = 0xFFu;
    return (uint8_t)div;
}

static inline void spi_cdns_program_clk_div(uint32_t target_mhz)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u clk_div = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
    };
    clk_div.f.clock_divider_value = spi_cdns_clkdiv_for(target_mhz);
    clk_div.f.clock_div_set       = 1;
    clk_div.f.clock_dutycycle     = 128;
    clk_div.f.clock_div_enable    = 1;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR,
              clk_div.val);
}

#endif /* SPI_CLK_H */
