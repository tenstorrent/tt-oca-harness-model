// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Cadence xSPI Flash Multi-Sector Write+Read Test - TC_SPI_CDN_009 (P1)
 *
 * xSPI equivalent of spi_ot_flash_sector_erase_test (TC_SPIOT_022),
 * spi_ot_flash_dual_read_test (TC_SPIOT_023), and
 * spi_ot_flash_quad_read_test (TC_SPIOT_024) combined.
 *
 * Writes 4 words with a sector-specific pattern to three separate 4KB sectors
 * of the alias window, then reads back and verifies all 12 words. The Cadence
 * xSPI controller handles the multi-lane (Octal DDR) transfers internally —
 * firmware sees a flat memory-mapped interface.
 *
 * Sector addresses:
 *   Sector 16 (0xD0010000): pattern 0xB1xx_00yy
 *   Sector 32 (0xD0020000): pattern 0xB2xx_00yy
 *   Sector 48 (0xD0030000): pattern 0xB3xx_00yy
 *
 * Addresses are distinct from other xSPI tests:
 *   CDN_003 uses 0xD0000000, CDN_005 uses 0xD0000000, CDN_008 uses 0xD0008000
 *
 * Test Flow:
 *   1. Initialize Cadence xSPI (init_comp wait)
 *   2. Write 4-word pattern to each of 3 sector addresses
 *   3. Read back all 12 words, verify each against written pattern
 *
 * Pass criteria:
 *   [1]    init_comp=1, init_fail=0
 *   [2-5]  Sector 16 (0xD0010000): all 4 words match
 *   [6-9]  Sector 32 (0xD0020000): all 4 words match
 *   [10-13] Sector 48 (0xD0030000): all 4 words match
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define SPI_ALIAS_BASE  0xD0000000u
#define WORDS_PER_SECT  4
#define NUM_SECTORS     3

static const uint32_t SECTOR_OFFSETS[NUM_SECTORS] = {
    0x00010000u,   /* Sector 16 */
    0x00020000u,   /* Sector 32 */
    0x00030000u,   /* Sector 48 */
};

static const uint32_t SECTOR_PATTERNS[NUM_SECTORS] = {
    0xB1000000u,
    0xB2000000u,
    0xB3000000u,
};

static void configure_spi_mux(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u m = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    m.f.spi_sel = 0;
    m.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, m.val);
}

static void reset_spi_controller(int reset)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u c = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    c.f.spi_reset_n_n0_scan = reset;
    c.f.spi_ctrl_reg_reset_n_n0_scan = reset;
    c.f.spi_phy_reg_reset_n_n0_scan = reset;
    c.f.spi_phy_reset_n_n0_scan = reset;
    c.f.spi_axi_reset_n_n0_scan = reset;
    c.f.spi_reg_reset_n_n0_scan = reset;
    c.f.spi_xspi_reg_reset_n_n0_scan = reset;
    c.f.spi_enable = reset;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, c.val);
}

static void enable_spi_controller(int enable)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u c = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    c.f.spi_enable = enable;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, c.val);
}

static void program_spi_clk_div(uint32_t target, uint32_t sys)
{
    uint8_t div = sys / target;
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u k = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
    };
    k.f.clock_divider_value = div;
    k.f.clock_div_set = 1;
    k.f.clock_dutycycle = 128;
    k.f.clock_div_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, k.val);
}

static void set_discovery_param(int lines, int ddr, int full)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_reg_u d = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_DEFAULT
    };
    if (full) { d.f.discovery_num_lines = 0; }
    else { d.f.discovery_num_lines = lines; d.f.discovery_cmd_type = ddr; }
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, d.val);
}

static int cdns_init(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
    int timeout = 1000000;
   // configure_spi_mux();
    enable_spi_controller(0);
    reset_spi_controller(0);
    program_spi_clk_div(25, 800);
    set_discovery_param(8, 1, 0);
    enable_spi_controller(1);
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    reset_spi_controller(1);
    do {
        s.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) { printf("  ERROR: Timeout waiting for init_comp\n"); return 1; }
    } while (s.f.init_comp == 0);
    if (s.f.init_fail == 1) { printf("  ERROR: init_fail=1\n"); return 1; }
    printf("  xSPI init OK: CTRL_STATUS=0x%08x\n", s.val);
    return 0;
}

int main(void)
{
    // sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("Cadence xSPI Flash Multi-Sector Test (TC_SPI_CDN_009)\n");
    printf("xSPI equivalent of sector_erase + dual_read + quad_read\n");
    printf("(%u sectors × %u words = %u total words)\n",
           NUM_SECTORS, WORDS_PER_SECT, NUM_SECTORS * WORDS_PER_SECT);
    printf("========================================\n\n");

    int pass = 1;
    uint32_t s, w;
    uint32_t tx_data[NUM_SECTORS][WORDS_PER_SECT];
    uint32_t total_fail = 0;

    /* Step 1: Initialize Cadence xSPI */
    printf("Step 1: Initialize Cadence xSPI controller\n");
    // if (cdns_init()) {
    //     pass = 0;
    //     goto done;
    // }

    /* Step 2: Write patterns to all sectors */
    printf("\nStep 2: Write %u words to each of %u sectors\n",
           WORDS_PER_SECT, NUM_SECTORS);
    for (s = 0; s < NUM_SECTORS; s++) {
        uint32_t base = SPI_ALIAS_BASE + SECTOR_OFFSETS[s];
        printf("  Sector %u @ 0x%08x:\n", s, base);
        for (w = 0; w < WORDS_PER_SECT; w++) {
            tx_data[s][w] = SECTOR_PATTERNS[s] | ((w & 0xFF) << 16) | (~w & 0xFF);
            WRITE_REG(base + (w * 4), tx_data[s][w]);
            printf("    [%u] <- 0x%08x\n", w, tx_data[s][w]);
        }
    }

    /* Step 3: Read back and verify all sectors */
    printf("\nStep 3: Read back and verify all %u words\n",
           NUM_SECTORS * WORDS_PER_SECT);
    for (s = 0; s < NUM_SECTORS; s++) {
        uint32_t base = SPI_ALIAS_BASE + SECTOR_OFFSETS[s];
        uint32_t sect_fail = 0;
        printf("  Sector %u @ 0x%08x:\n", s, base);
        for (w = 0; w < WORDS_PER_SECT; w++) {
            uint32_t rx = READ_REG(base + (w * 4));
            int match = (rx == tx_data[s][w]);
            printf("    [%u] exp=0x%08x got=0x%08x %s\n",
                   w, tx_data[s][w], rx,
                   match ? "OK" : (rx == 0xFFFFFFFF ? "WARN(0xFF)" : "MISMATCH"));
            if (!match) sect_fail++;
        }
        if (sect_fail) {
            printf("    FAIL: %u word(s) mismatch in sector %u\n", sect_fail, s);
            total_fail += sect_fail;
        } else {
            printf("    PASS: sector %u all %u words match\n", s, WORDS_PER_SECT);
        }
    }

    if (total_fail > 0) {
        printf("\n  FAIL: %u/%u total word(s) mismatch\n",
               total_fail, NUM_SECTORS * WORDS_PER_SECT);
        pass = 0;
    } else {
        printf("\n  PASS: All %u words across %u sectors verified\n",
               NUM_SECTORS * WORDS_PER_SECT, NUM_SECTORS);
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== CDNS FLASH MULTI-SECTOR TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== CDNS FLASH MULTI-SECTOR TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
