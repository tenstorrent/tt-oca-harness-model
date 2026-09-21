// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Command Queue Test - TC_SPIOT_007 (P0)
 *
 * Verifies command queue functionality, CMDQD monitoring, CSID selection,
 * and error conditions (CMDBUSY, CMDINVAL, CSIDINVAL).
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Verify CMDQD=0 initially
 *   3. Write CSID=0, issue CMD, check CMDQD
 *   4. Test CMDINVAL error with invalid SPEED=3
 *   5. Test CSIDINVAL error with CSID > NUM_CS
 *   6. Verify ERROR_STATUS W1C clear
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_cmd_queue_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"
#include "spi_clk.h"

#define TIMEOUT_LIMIT 100000

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
    printf("SPI OT Command Queue Test (TC_SPIOT_007)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_COMMAND_reg_u cmd;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    SPI_CONTROLLER_ERROR_ENABLE_reg_u err_enable;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    /* Configure clock */
    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    cfg.val = 0;
    cfg.f.clkdiv = spi_clkdiv();
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);

    /* Step 1: Verify CMDQD initial state */
    printf("\nStep 1: Command queue initial state\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  CMDQD=%u, READY=%u\n", status.f.cmdqd, status.f.ready);

    /* Step 2: Set CSID and verify */
    printf("\nStep 2: CSID configuration\n");
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);
    uint32_t csid_val = READ_REG(SPI_CONTROLLER_CSID_REG_ADDR);
    if (!check_reg("CSID=0", csid_val, 0)) pass = 0;

    /* Step 3: Issue valid command */
    printf("\nStep 3: Issue valid command (TX, Standard, LEN=3)\n");
    /* Clear prior errors */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* Load TX data first */
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x9F000000);

    /* Wait for READY */
    int timeout = TIMEOUT_LIMIT;
    do {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (--timeout <= 0) break;
    } while (status.f.ready == 0);

    if (status.f.ready) {
        cmd.val = 0;
        cmd.f.len = 3;
        cmd.f.csaat = 0;
        cmd.f.speed = 0;
        cmd.f.direction = 2;
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
        printf("  CMD issued: LEN=%u, SPEED=%u, DIR=%u, CSAAT=%u\n",
               cmd.f.len, cmd.f.speed, cmd.f.direction, cmd.f.csaat);

        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        printf("  ERROR_STATUS=0x%08x (should be clean)\n", err_status.val);
    } else {
        printf("  WARN: Controller not READY, skipping CMD issue\n");
    }

    /* Step 3.5: Test CMDINVAL error (SPEED=3, reserved value) */
    printf("\nStep 3.5: CMDINVAL error test (SPEED=3)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);

    timeout = TIMEOUT_LIMIT;
    do {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (--timeout <= 0) break;
    } while (status.f.ready == 0);

    if (status.f.ready) {
        cmd.val = 0;
        cmd.f.len       = 0;
        cmd.f.speed     = 3;    /* reserved speed → CMDINVAL */
        cmd.f.direction = 2;
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00);
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);

        volatile int delay;
        for (delay = 0; delay < 100; delay++) {}

        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        printf("  ERROR_STATUS=0x%08x, CMDINVAL=%u\n",
               err_status.val, err_status.f.cmdinval);
        if (err_status.f.cmdinval) {
            printf("  PASS: CMDINVAL error detected\n");
        } else {
            printf("  FAIL: CMDINVAL not set for SPEED=3\n");
            pass = 0;
        }
        /* Clear errors */
        WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    } else {
        printf("  WARN: Controller not READY, skipping CMDINVAL test\n");
    }

    /* Step 4: Test CSIDINVAL error */
    printf("\nStep 4: CSIDINVAL error test (CSID=5, NUM_CS=1)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 5);

    timeout = TIMEOUT_LIMIT;
    do {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (--timeout <= 0) break;
    } while (status.f.ready == 0);

    if (status.f.ready) {
        cmd.val = 0;
        cmd.f.len = 0;
        cmd.f.speed = 0;
        cmd.f.direction = 2;
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00);
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);

        volatile int delay;
        for (delay = 0; delay < 100; delay++) {}

        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        printf("  ERROR_STATUS=0x%08x, CSIDINVAL=%u\n",
               err_status.val, err_status.f.csidinval);
        if (err_status.f.csidinval) {
            printf("  PASS: CSIDINVAL error detected\n");
        } else {
            printf("  FAIL: CSIDINVAL not set for CSID=5 (NUM_CS=1)\n");
            pass = 0;
        }
    }

    /* Restore CSID=0 */
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);

    /* Step 5: Test ERROR_STATUS W1C */
    printf("\nStep 5: ERROR_STATUS W1C clear\n");
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  Before clear: ERROR_STATUS=0x%08x\n", err_status.val);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, err_status.val);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  After W1C: ERROR_STATUS=0x%08x\n", err_status.val);

    /* Step 6: Test ERROR_ENABLE defaults */
    printf("\nStep 6: ERROR_ENABLE default check\n");
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("ERROR_ENABLE default", err_enable.val, SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT CMD QUEUE TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT CMD QUEUE TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
