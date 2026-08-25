// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Sanity Test - TC_SPIOT_002 (P0)
 *
 * Merged sanity test covering SPI mux selection and SPI controller
 * enable/disable behavior.
 *
 * Part 1: SPI Mux Selection
 *   1. Read SPI_MUX_CTRL default (spi_sel=0, cs_force_high=1)
 *   2. Switch to OpenTitan (spi_sel=1), verify readback
 *   3. Clear cs_force_high, verify readback
 *   4. Switch back to Cadence (spi_sel=0), verify readback
 *   5. Verify status bits (ot_busy, cdns_busy) are readable
 *
 * Part 2: Controller Enable/Disable
 *   1. Configure SPI mux for OpenTitan
 *   2. Read STATUS when SPIEN=0, verify READY behavior
 *   3. Enable controller (SPIEN=1), verify STATUS
 *   4. Software reset (SW_RST pulse), verify state clears
 *   5. Disable controller (SPIEN=0)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_sanity_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Sanity Test (TC_SPIOT_002)\n");
    printf("========================================\n\n");

    int pass = 1;
    uint32_t read_val;
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;

    /* ================================================================
     * Part 1: SPI Mux Selection
     * ================================================================ */
    printf("--- Part 1: SPI Mux Selection ---\n\n");

    /* Step 1: Read default values */
    printf("Step 1: Read SPI_MUX_CTRL default\n");
    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel default", spi_mux.f.spi_sel, 0)) pass = 0;
    if (!check_reg("cs_force_high default", spi_mux.f.cs_force_high, 1)) pass = 0;

    /* Step 2: Select OpenTitan (spi_sel=1) */
    printf("\nStep 2: Select OpenTitan SPI Host\n");
    spi_mux.val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT;
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 1;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel after OT select", spi_mux.f.spi_sel, 1)) pass = 0;
    if (!check_reg("cs_force_high still set", spi_mux.f.cs_force_high, 1)) pass = 0;

    /* Step 3: Clear cs_force_high */
    printf("\nStep 3: Clear cs_force_high\n");
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel still OT", spi_mux.f.spi_sel, 1)) pass = 0;
    if (!check_reg("cs_force_high cleared", spi_mux.f.cs_force_high, 0)) pass = 0;

    /* Step 4: Switch back to Cadence */
    printf("\nStep 4: Switch back to Cadence\n");
    spi_mux.f.spi_sel = 0;
    spi_mux.f.cs_force_high = 1;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel back to Cadence", spi_mux.f.spi_sel, 0)) pass = 0;

    /* Step 5: Read status bits */
    printf("\nStep 5: Read status bits\n");
    printf("  ot_busy=%u, cdns_busy=%u (read-only status)\n",
           spi_mux.f.ot_busy, spi_mux.f.cdns_busy);
    printf("  ot_irq=%u, cdns_irq=%u (read-only status)\n",
           spi_mux.f.ot_irq, spi_mux.f.cdns_irq);

    /* ================================================================
     * Part 2: Controller Enable/Disable
     * ================================================================ */
    printf("\n--- Part 2: Controller Enable/Disable ---\n\n");

    /* Configure SPI mux for OpenTitan */
    spi_mux.val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT;
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
    printf("SPI mux configured for OpenTitan\n\n");

    /* Step 1: Read CTRL default */
    printf("Step 1: CTRL default check\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    if (!check_reg("CTRL default", ctrl.val, SPI_CONTROLLER_CTRL_REG_DEFAULT)) pass = 0;
    if (!check_reg("SPIEN default", ctrl.f.spien, 0)) pass = 0;
    if (!check_reg("OUTPUT_EN default", ctrl.f.output_en, 0)) pass = 0;

    /* Step 2: Read STATUS when disabled - READY=1 at reset (CMD FIFO empty, ready to accept) */
    printf("\nStep 2: STATUS when SPIEN=0\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS=0x%08x (READY=%u, ACTIVE=%u, TXEMPTY=%u)\n",
           status.val, status.f.ready, status.f.active, status.f.txempty);
    if (!check_reg("READY=1 when idle (CMD FIFO empty)", status.f.ready, 1)) pass = 0;

    /* Step 3: Enable controller */
    printf("\nStep 3: Enable SPI controller (SPIEN=1)\n");
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    if (!check_reg("SPIEN after enable", ctrl.f.spien, 1)) pass = 0;
    if (!check_reg("OUTPUT_EN after set", ctrl.f.output_en, 1)) pass = 0;

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS after enable: 0x%08x (READY=%u, TXEMPTY=%u)\n",
           status.val, status.f.ready, status.f.txempty);
    if (!check_reg("READY=1 after SPIEN=1", status.f.ready, 1)) pass = 0;

    /* Step 4: Software reset */
    printf("\nStep 4: Software reset (SW_RST)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    volatile int delay;
    for (delay = 0; delay < 1000; delay++) {}

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS after SW_RST: TXEMPTY=%u, RXEMPTY=%u, ACTIVE=%u\n",
           status.f.txempty, status.f.rxempty, status.f.active);

    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    if (!check_reg("SW_RST reads 0 (singlepulse)", ctrl.f.sw_rst, 0)) pass = 0;

    /* Step 5: Disable controller */
    printf("\nStep 5: Disable SPI controller (SPIEN=0)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.spien = 0;
    ctrl.f.output_en = 0;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    if (!check_reg("SPIEN after disable", ctrl.f.spien, 0)) pass = 0;
    if (!check_reg("OUTPUT_EN after clear", ctrl.f.output_en, 0)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT SANITY TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT SANITY TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
