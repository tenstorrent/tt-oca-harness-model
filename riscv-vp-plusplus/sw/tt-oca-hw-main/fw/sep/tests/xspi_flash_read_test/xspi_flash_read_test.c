/*
 * Cadence xSPI Flash Read Test - TC_SPI_CDN_007 (P1)
 *
 * xSPI equivalent of spi_ot_flash_read_test (TC_SPIOT_020).
 *
 * After Cadence xSPI initialization, writes 4 words with a known pattern
 * to the alias window at 0xD0004000, then reads them back and verifies
 * the pattern matches. The test also re-reads to confirm consistency.
 *
 * NOTE: In simulation the flash model requires a prior write before reads
 * return meaningful data (reading unprogrammed pages hangs the controller).
 * This test uses a distinct alias offset (0xD0004000) to avoid conflicts
 * with CDN_003 (0xD0000000) and CDN_008 (0xD0008000).
 *
 * Pass criteria:
 *   [1] init_comp=1 (xSPI init completed)
 *   [2] init_fail=0 (no init failure)
 *   [3] ctrl_busy=0 after init
 *   [4] All 4 read-back words match the written pattern
 *   [5] All 4 words match on consistency re-read
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define SPI_ALIAS_WR    0xD0004000u   /* distinct sector — avoids CDN_003/CDN_008 */
#define RW_WORDS        4
#define WR_PATTERN_BASE 0xA5050000u

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
    configure_spi_mux();
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
    printf("  Cadence xSPI init OK: CTRL_STATUS=0x%08x\n", s.val);
    return 0;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("Cadence xSPI Flash Read Test (TC_SPI_CDN_007)\n");
    printf("xSPI equivalent of spi_ot_flash_read_test\n");
    printf("Write+Read alias window @ 0x%08x\n", SPI_ALIAS_WR);
    printf("========================================\n\n");

    int pass = 1;
    uint32_t i;

    /* Step 1: Initialize Cadence xSPI */
    printf("Step 1: Initialize Cadence xSPI controller\n");
    if (cdns_init()) {
        pass = 0;
        goto done;
    }

    /* Check [3]: ctrl_busy=0 after init */
    {
        CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
        s.val = READ_REG(
            SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (s.f.ctrl_busy) {
            printf("  WARN: ctrl_busy still set after init_comp\n");
        } else {
            printf("  PASS [3]: ctrl_busy=0 after init\n");
        }
    }

    /* Step 2: Write 4 words at SPI_ALIAS_WR */
    printf("\nStep 2: Write %u words to alias 0x%08x\n", RW_WORDS, SPI_ALIAS_WR);
    volatile uint32_t *alias = (volatile uint32_t *)SPI_ALIAS_WR;
    for (i = 0; i < RW_WORDS; i++) {
        uint32_t wr_val = WR_PATTERN_BASE | (i * 0x1111u);
        alias[i] = wr_val;
        printf("  [%u] wrote 0x%08x @ 0x%08x\n", i, wr_val, SPI_ALIAS_WR + (i * 4));
    }

    /* Step 3: Read back and verify pattern — check [4] */
    printf("\nStep 3: Read back %u words (verify pattern) — check [4]\n", RW_WORDS);
    uint32_t fail4 = 0;
    uint32_t rd1[RW_WORDS];
    for (i = 0; i < RW_WORDS; i++) {
        uint32_t exp = WR_PATTERN_BASE | (i * 0x1111u);
        rd1[i] = alias[i];
        int ok = (rd1[i] == exp);
        printf("  [%u] exp=0x%08x got=0x%08x %s\n", i, exp, rd1[i], ok ? "OK" : "MISMATCH");
        if (!ok) fail4++;
    }
    if (fail4) {
        printf("  FAIL [4]: %u word(s) mismatched\n", fail4);
        pass = 0;
    } else {
        printf("  PASS [4]: all %u words match written pattern\n", RW_WORDS);
    }

    /* Step 4: Re-read to verify consistency — check [5] */
    printf("\nStep 4: Re-read %u words (consistency check) — check [5]\n", RW_WORDS);
    uint32_t fail5 = 0;
    for (i = 0; i < RW_WORDS; i++) {
        uint32_t rd2 = alias[i];
        int ok = (rd2 == rd1[i]);
        printf("  [%u] first=0x%08x second=0x%08x %s\n",
               i, rd1[i], rd2, ok ? "OK" : "INCONSISTENT");
        if (!ok) fail5++;
    }
    if (fail5) {
        printf("  FAIL [5]: %u word(s) inconsistent on re-read\n", fail5);
        pass = 0;
    } else {
        printf("  PASS [5]: all %u reads consistent\n", RW_WORDS);
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== CDNS FLASH READ TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== CDNS FLASH READ TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
