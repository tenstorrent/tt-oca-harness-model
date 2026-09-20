// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI Write/Read Test for OCH SEP
 * Ported from tt_sep/firmware/spi_write_read_test.
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define SPI_ALIAS_ADDRESS_BASE 0xD0000000

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

static int try_init_new(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
    int timeout = 1000000;
    do {
        s.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) { printf("ERROR: Timeout\n"); return 1; }
    } while (s.f.init_comp == 0);
    if (s.f.init_fail == 1) { printf("ERROR: init_fail\n"); return 1; }
    printf("  SPI init OK: 0x%08x\n", s.val);
    return 0;
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

/* ------------------------------------------------------------------ */
/* OT SPI helpers (flash WREN, status read, wait idle/ready)          */
/* ------------------------------------------------------------------ */
#ifdef USE_OT_SPI

#define OT_SPI_TIMEOUT       200000
#define OT_SPI_STATUS_LIMIT  500000

#define FLASH_CMD_WREN  0x06
#define FLASH_CMD_RDSR  0x05
#define FLASH_CMD_PP    0x02
#define FLASH_CMD_READ  0x03
#define FLASH_SR_WIP    (1u << 0)
#define FLASH_SR_WEL    (1u << 1)

static void ot_init_spi(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    cfg.val = 0;
    cfg.f.clkdiv   = 9;
    cfg.f.csnidle  = 2;
    cfg.f.csnlead  = 2;
    cfg.f.csntrail = 2;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);

    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
}

static int ot_wait_ready(void)
{
    SPI_CONTROLLER_STATUS_reg_u s;
    int t = OT_SPI_TIMEOUT;
    while (t-- > 0) {
        s.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (s.f.ready) return 0;
    }
    printf("  TIMEOUT: READY\n");
    return -1;
}

static int ot_wait_idle(void)
{
    SPI_CONTROLLER_STATUS_reg_u s;
    int t = OT_SPI_TIMEOUT;
    while (t-- > 0) {
        s.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!s.f.active) return 0;
    }
    printf("  TIMEOUT: ACTIVE\n");
    return -1;
}

static uint8_t ot_flash_read_status(void)
{
    SPI_CONTROLLER_COMMAND_reg_u cmd;
    SPI_CONTROLLER_STATUS_reg_u s;

    if (ot_wait_ready()) return 0xFF;
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, FLASH_CMD_RDSR);
    cmd.val = 0; cmd.f.len = 0; cmd.f.csaat = 1; cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);

    if (ot_wait_ready()) return 0xFF;
    cmd.val = 0; cmd.f.len = 0; cmd.f.csaat = 0; cmd.f.direction = 1;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);

    if (ot_wait_idle()) return 0xFF;
    s.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    if (s.f.rxqd >= 1)
        return (uint8_t)(READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR) & 0xFF);
    return 0xFF;
}
#endif  /* USE_OT_SPI */

int main(void)
{
    // sep_outbound_filter_init();

#ifdef USE_OT_SPI
    /* ==================================================================
     * OpenTitan SPI Write/Read Test
     * OT equivalent of the Cadence alias-window write/read test:
     * use standard NOR flash commands (WREN + PP + READ) to write and
     * read back 4 words. Requires +spi_device_sel=4 (Winbond W25Q512JV).
     *
     * Test Flow:
     *   1. Configure SPI mux for OT, enable controller
     *   2. WREN (0x06), verify WEL=1
     *   3. Page Program 0x02 at 0x001000 with pattern 0xA5xx_BBxx
     *   4. Poll WIP=0
     *   5. Standard Read (0x03) 16 bytes from 0x001000
     *   6. Verify read data matches written pattern
     * ================================================================== */
    int pass = 1;
    uint32_t i;
    SPI_CONTROLLER_COMMAND_reg_u cmd;

    printf("\n=== OCH SEP OpenTitan SPI Write/Read Test ===\n");
    printf("Requires: +spi_device_sel=4 (Winbond W25Q512JV)\n\n");

    ot_init_spi();
    printf("OT SPI controller enabled: CLKDIV=9\n\n");

#define OT_FLASH_ADDR  0x001000
#define OT_WRITE_WORDS 4

    /* Step 1: Write Enable */
    printf("Step 1: WREN (0x06)\n");
    if (ot_wait_ready()) { pass = 0; goto done; }
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, FLASH_CMD_WREN);
    cmd.val = 0; cmd.f.len = 0; cmd.f.csaat = 0; cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
    if (ot_wait_idle()) { pass = 0; goto done; }

    uint8_t sr = ot_flash_read_status();
    printf("  SR=0x%02x WEL=%u\n", sr, (sr >> 1) & 1);
    if (sr != 0xFF && !(sr & FLASH_SR_WEL)) {
        printf("  FAIL: WEL not set\n");
        pass = 0; goto done;
    }

    /* Step 2: Page Program */
    printf("\nStep 2: Page Program 0x02 at 0x%06x (16 bytes)\n", OT_FLASH_ADDR);
    if (ot_wait_ready()) { pass = 0; goto done; }

    uint32_t pp_hdr = FLASH_CMD_PP
                    | (((OT_FLASH_ADDR >> 16) & 0xFF) << 8)
                    | (((OT_FLASH_ADDR >>  8) & 0xFF) << 16)
                    | (((OT_FLASH_ADDR >>  0) & 0xFF) << 24);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, pp_hdr);
    cmd.val = 0; cmd.f.len = 3; cmd.f.csaat = 1; cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);

    if (ot_wait_ready()) { pass = 0; goto done; }

    /* Pattern: 0xA5xx_00yy where xx=word-idx, yy=~word-idx */
    uint32_t tx_data[OT_WRITE_WORDS];
    for (i = 0; i < OT_WRITE_WORDS; i++) {
        tx_data[i] = (0xA5u << 24) | ((i & 0xFF) << 16) | ((~i & 0xFF));
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, tx_data[i]);
        printf("  TX[%u]=0x%08x\n", i, tx_data[i]);
    }
    cmd.val = 0; cmd.f.len = (OT_WRITE_WORDS * 4) - 1; cmd.f.csaat = 0; cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
    if (ot_wait_idle()) { pass = 0; goto done; }

    /* Step 3: Poll WIP=0 */
    printf("\nStep 3: Poll WIP=0...\n");
    {
        int poll_count = 0;
        int wip_done = 0;
        while (poll_count < OT_SPI_STATUS_LIMIT) {
            sr = ot_flash_read_status();
            poll_count++;
            if (sr == 0xFF) { wip_done = 1; break; }  /* No flash model, skip */
            if (!(sr & FLASH_SR_WIP)) { wip_done = 1; break; }
        }
        if (!wip_done) { printf("  FAIL: WIP timeout\n"); pass = 0; goto done; }
        printf("  WIP=0 after %d polls\n", poll_count);
    }

    /* Step 4: Standard Read */
    printf("\nStep 4: Standard Read 0x03 from 0x%06x (16 bytes)\n", OT_FLASH_ADDR);
    if (ot_wait_ready()) { pass = 0; goto done; }
    uint32_t rd_hdr = FLASH_CMD_READ
                    | (((OT_FLASH_ADDR >> 16) & 0xFF) << 8)
                    | (((OT_FLASH_ADDR >>  8) & 0xFF) << 16)
                    | (((OT_FLASH_ADDR >>  0) & 0xFF) << 24);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, rd_hdr);
    cmd.val = 0; cmd.f.len = 3; cmd.f.csaat = 1; cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
    if (ot_wait_ready()) { pass = 0; goto done; }
    cmd.val = 0; cmd.f.len = (OT_WRITE_WORDS * 4) - 1; cmd.f.csaat = 0; cmd.f.direction = 1;
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
    if (ot_wait_idle()) { pass = 0; goto done; }

    /* Step 5: Verify */
    printf("\nStep 5: Verify read data\n");
    {
        SPI_CONTROLLER_STATUS_reg_u spi_status;
        spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        uint32_t mismatches = 0;
        for (i = 0; i < OT_WRITE_WORDS; i++) {
            if (spi_status.f.rxempty) { printf("  [%u] UNDERFLOW\n", i); mismatches++; break; }
            uint32_t rx = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
            printf("  [%u] exp=0x%08x got=0x%08x %s\n",
                   i, tx_data[i], rx, (rx == tx_data[i]) ? "OK" : "MISMATCH");
            if (rx != tx_data[i]) {
                if (rx == 0xFFFFFFFF) printf("       (0xFF = erased/no flash)\n");
                mismatches++;
            }
            spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        }
        if (mismatches > 0) {
            printf("  FAIL: %u mismatches\n", mismatches);
            pass = 0;
        }
    }

    {
        SPI_CONTROLLER_ERROR_STATUS_reg_u err;
        err.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        if (err.f.cmdinval || err.f.csidinval) {
            printf("  FAIL: ERROR_STATUS=0x%08x\n", err.val);
            pass = 0;
        }
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== OT SPI WRITE/READ TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== OT SPI WRITE/READ TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");
    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;

#else  /* USE_OT_SPI not defined → Cadence xSPI alias window path */

    printf("\n=== OCH SEP SPI Write/Read Test ===\n");

 //   configure_spi_mux();
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

    if (try_init_new()) {
        test_fail(0);
        while (1) { __asm__("wfi"); }
        return -1;
    }

    uint32_t w1 = 0x01234567, w2 = 0x89abcdef;
    printf("  Write w1=0x%08x w2=0x%08x\n", w1, w2);
    WRITE_REG(SPI_ALIAS_ADDRESS_BASE, w1);
    WRITE_REG(SPI_ALIAS_ADDRESS_BASE + 4, w2);

    uint32_t r1 = READ_REG(SPI_ALIAS_ADDRESS_BASE);
    uint32_t r2 = READ_REG(SPI_ALIAS_ADDRESS_BASE + 4);
    printf("  Read r1=0x%08x r2=0x%08x\n", r1, r2);

    int pass = (w1 == r1) && (w2 == r2);
    if (!pass) printf("  MISMATCH!\n");

    if (pass) { printf("=== PASSED ===\n"); test_pass(0); }
    else      { printf("=== FAILED ===\n"); test_fail(0); }

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;

#endif  /* USE_OT_SPI */
}
