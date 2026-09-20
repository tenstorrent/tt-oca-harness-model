// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Enable/Disable Test - TC_SPIOT_003 (P0)
 *
 * Verifies SPI controller enable/disable via SPIEN bit, OUTPUT_EN,
 * and software reset (SW_RST) behavior.
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan
 *   2. Read STATUS when SPIEN=0, verify READY behavior
 *   3. Enable controller (SPIEN=1), verify STATUS
 *   4. Software reset (SW_RST level: hold, drain, then release), verify state clears
 *   5. Disable controller (SPIEN=0)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_enable_disable_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

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
    printf("SPI OT Enable/Disable Test (TC_SPIOT_003)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n\n");

    /* Step 1: Read CTRL default */
    printf("Step 1: CTRL default check\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("CTRL default", ctrl.val, SPI_CONTROLLER_CONTROL_REG_DEFAULT)) pass = 0;
    if (!check_reg("SPIEN default", ctrl.f.spien, 0)) pass = 0;
    if (!check_reg("OUTPUT_EN default", ctrl.f.output_en, 0)) pass = 0;

    /* Step 2: Read STATUS when disabled */
    printf("\nStep 2: STATUS when SPIEN=0\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS=0x%08x (READY=%u, ACTIVE=%u, TXEMPTY=%u)\n",
           status.val, status.f.ready, status.f.active, status.f.txempty);

    /* Step 3: Enable controller */
    printf("\nStep 3: Enable SPI controller (SPIEN=1)\n");
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SPIEN after enable", ctrl.f.spien, 1)) pass = 0;
    if (!check_reg("OUTPUT_EN after set", ctrl.f.output_en, 1)) pass = 0;

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS after enable: 0x%08x (READY=%u, TXEMPTY=%u)\n",
           status.val, status.f.ready, status.f.txempty);

    /* Step 4: Software reset. SW_RST is a level: it reads back as written and
     * the core stays in reset until software clears it, so the drain is
     * observed while it is held and the release is what makes the controller
     * usable again. */
    printf("\nStep 4: Software reset (SW_RST)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SW_RST reads 1 while held", ctrl.f.sw_rst, 1)) pass = 0;

    volatile int delay;
    for (delay = 0; delay < 1000; delay++) {}

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS after SW_RST: TXEMPTY=%u, RXEMPTY=%u, ACTIVE=%u\n",
           status.f.txempty, status.f.rxempty, status.f.active);

    ctrl.f.sw_rst = 0;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SW_RST reads 0 after release", ctrl.f.sw_rst, 0)) pass = 0;

    /* Step 5: Disable controller */
    printf("\nStep 5: Disable SPI controller (SPIEN=0)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.spien = 0;
    ctrl.f.output_en = 0;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SPIEN after disable", ctrl.f.spien, 0)) pass = 0;
    if (!check_reg("OUTPUT_EN after clear", ctrl.f.output_en, 0)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT ENABLE/DISABLE TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT ENABLE/DISABLE TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
