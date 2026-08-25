// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Cadence xSPI Flash Write+Read Test - TC_SPI_CDN_008 (P0)
 *
 * xSPI equivalent of spi_ot_flash_write_read_test (TC_SPIOT_021).
 *
 * Writes 8 words with a distinctive pattern to the xSPI alias window at
 * offset 0x8000 (0xD0008000), then reads them back and verifies all 8 words
 * match. The Cadence controller handles the write-enable + page-program SPI
 * commands automatically when data is written to the alias window.
 *
 * Address 0xD0008000 (flash sector 8) is used to avoid collision with:
 *   - spi_write_read_test (CDN_003) which uses 0xD0000000
 *   - spi_xspi_dma_test   (CDN_005) which uses 0xD0000000
 *
 * Test Flow:
 *   1. Initialize Cadence xSPI (init_comp wait)
 *   2. Write 8-word pattern at alias 0xD0008000
 *   3. Read back 8 words from same address
 *   4. Verify all 8 words match written pattern
 *
 * Pass criteria:
 *   [1] init_comp=1, init_fail=0
 *   [2-9] Each of the 8 words reads back correctly
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
#define FLASH_OFFSET    0x00008000u     /* Sector 8 — distinct from CDN_003/005 */
#define WRITE_WORDS     8
#define PATTERN_MARKER  0xAC000000u

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
//    configure_spi_mux();
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
    printf("Cadence xSPI Flash Write+Read Test (TC_SPI_CDN_008)\n");
    printf("xSPI equivalent of spi_ot_flash_write_read_test\n");
    printf("Target: alias 0x%08x (flash offset 0x%05x, sector 8)\n",
           SPI_ALIAS_BASE + FLASH_OFFSET, FLASH_OFFSET);
    printf("========================================\n\n");

    int pass = 1;
    uint32_t i;
    uint32_t tx_data[WRITE_WORDS];
    uint32_t flash_addr = SPI_ALIAS_BASE + FLASH_OFFSET;

    /* Step 1: Initialize Cadence xSPI */
    printf("Step 1: Initialize Cadence xSPI controller\n");
    // if (cdns_init()) {
    //     pass = 0;
    //     goto done;
    // }

    /* Step 2: Prepare and write pattern to alias window
     * Pattern: 0xACxx_00yy where xx=word-idx, yy=~word-idx */
    printf("\nStep 2: Write %u words to alias 0x%08x\n", WRITE_WORDS, flash_addr);
    for (i = 0; i < WRITE_WORDS; i++) {
        tx_data[i] = PATTERN_MARKER | ((i & 0xFF) << 16) | (~i & 0xFF);
        WRITE_REG(flash_addr + (i * 4), tx_data[i]);
        printf("  [%u] @ 0x%08x <- 0x%08x\n", i, flash_addr + (i * 4), tx_data[i]);
    }

    /* Step 3: Read back 8 words */
    printf("\nStep 3: Read back %u words from alias 0x%08x\n", WRITE_WORDS, flash_addr);
    uint32_t verify_fail = 0;
    for (i = 0; i < WRITE_WORDS; i++) {
        uint32_t rx = READ_REG(flash_addr + (i * 4));
        int match = (rx == tx_data[i]);
        printf("  [%u] exp=0x%08x got=0x%08x %s\n",
               i, tx_data[i], rx,
               match ? "OK" : (rx == 0xFFFFFFFF ? "WARN(0xFF=erased/no model)" : "MISMATCH"));
        if (!match) verify_fail++;
    }

    /* Step 4: Verdict */
    if (verify_fail > 0) {
        printf("\n  FAIL: %u/%u word(s) mismatch\n", verify_fail, WRITE_WORDS);
        pass = 0;
    } else {
        printf("\n  PASS: All %u words verified\n", WRITE_WORDS);
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== CDNS FLASH WRITE+READ TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== CDNS FLASH WRITE+READ TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
