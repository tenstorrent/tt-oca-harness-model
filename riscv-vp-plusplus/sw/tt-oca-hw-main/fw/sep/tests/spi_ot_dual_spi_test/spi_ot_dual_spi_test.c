/*
 * SPI OT Dual SPI Test - TC_SPIOT_017 (P1)
 *
 * Verifies Dual SPI (x2) mode transmit, dummy cycles, and receive using
 * CMD.SPEED=1. Also verifies that DIRECTION=3 (bidirectional) at Dual speed
 * triggers CMDINVAL (bidirectional only valid at Standard speed), and that
 * bidirectional works correctly at SPEED=0 (Standard).
 *
 * A typical dual read sequence: TX cmd/addr → Dummy cycles → RX data.
 * Two data lines (SD[1:0]) are active in Dual mode.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller (CLKDIV=9, Mode 0)
 *   2. Dual TX: SPEED=1, DIRECTION=2, LEN=3 (4 bytes), CSAAT=1
 *   3. Dual Dummy: SPEED=1, DIRECTION=0, LEN=7 (8 dummy cycles), CSAAT=1
 *   4. Dual RX: SPEED=1, DIRECTION=1, LEN=3 (4 bytes), CSAAT=0
 *   5. CMDINVAL test: SPEED=1 + DIRECTION=3 (bidirectional) must fail
 *   6. Verify bidirectional (DIRECTION=3) accepted at Standard speed (SPEED=0)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_dual_spi_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define TIMEOUT_LIMIT 100000

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

static int wait_for_ready(int timeout)
{
    SPI_CONTROLLER_STATUS_reg_u status;
    while (timeout > 0) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (status.f.ready) return 0;
        timeout--;
    }
    printf("  ERROR: Timeout waiting for READY\n");
    return 1;
}

static int wait_for_idle(int timeout)
{
    SPI_CONTROLLER_STATUS_reg_u status;
    while (timeout > 0) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.active) return 0;
        timeout--;
    }
    printf("  WARN: Timeout waiting for ACTIVE=0 (no SPI device attached)\n");
    return 1;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Dual SPI Test (TC_SPIOT_017)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_CFG_reg_u cfg;
    SPI_CONTROLLER_CMD_reg_u cmd;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    volatile int delay;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* Configure: CLKDIV=9 (~5MHz from 100MHz), SPI Mode 0, standard CS timing */
    cfg.val = 0;
    cfg.f.clkdiv   = 9;
    cfg.f.cpol     = 0;
    cfg.f.cpha     = 0;
    cfg.f.csnidle  = 2;
    cfg.f.csnlead  = 2;
    cfg.f.csntrail = 2;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* ------------------------------------------------------------------ */
    /* Step 1: Dual TX (SPEED=1, DIRECTION=2)                              */
    /* ------------------------------------------------------------------ */
    printf("Step 1: Dual SPI TX (SPEED=Dual, DIR=TX, LEN=3=4bytes, CSAAT=1)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* Load TX FIFO: dual fast-read command pattern */
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x3B000000);

    cmd.val         = 0;
    cmd.f.len       = 3;    /* 4 bytes (LEN+1 bytes total) */
    cmd.f.csaat     = 1;    /* keep CS# low for next segment */
    cmd.f.speed     = 1;    /* Dual */
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u CSIDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval, err_status.f.csidinval);
    if (err_status.f.cmdinval || err_status.f.csidinval) {
        printf("  FAIL: CMD error for valid Dual TX command\n");
        pass = 0;
    } else {
        printf("  PASS: Dual TX accepted (no CMDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* ------------------------------------------------------------------ */
    /* Step 2: Dual Dummy cycles (SPEED=1, DIRECTION=0)                    */
    /* ------------------------------------------------------------------ */
    printf("\nStep 2: Dual Dummy cycles (SPEED=Dual, DIR=Dummy, LEN=7=8cycles, CSAAT=1)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val         = 0;
    cmd.f.len       = 7;    /* 8 dummy cycles */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 1;    /* Dual */
    cmd.f.direction = 0;    /* Dummy */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval);
    if (err_status.f.cmdinval) {
        printf("  FAIL: CMDINVAL for valid Dual Dummy command\n");
        pass = 0;
    } else {
        printf("  PASS: Dual Dummy accepted (no CMDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* ------------------------------------------------------------------ */
    /* Step 3: Dual RX (SPEED=1, DIRECTION=1)                              */
    /* ------------------------------------------------------------------ */
    printf("\nStep 3: Dual SPI RX (SPEED=Dual, DIR=RX, LEN=3=4bytes, CSAAT=0)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val         = 0;
    cmd.f.len       = 3;    /* 4 bytes */
    cmd.f.csaat     = 0;    /* release CS# after */
    cmd.f.speed     = 1;    /* Dual */
    cmd.f.direction = 1;    /* RX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    wait_for_idle(TIMEOUT_LIMIT);

    status.val     = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  STATUS: RXQD=%u, RXEMPTY=%u, ACTIVE=%u\n",
           status.f.rxqd, status.f.rxempty, status.f.active);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u CSIDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval, err_status.f.csidinval);
    if (err_status.f.cmdinval || err_status.f.csidinval) {
        printf("  FAIL: CMD error for valid Dual RX command\n");
        pass = 0;
    } else {
        printf("  PASS: Dual RX accepted (no CMDINVAL/CSIDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* SW_RST to drain RX FIFO */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 200; delay++) {}

    /* ------------------------------------------------------------------ */
    /* Step 4: CMDINVAL — DIRECTION=3 (bidirectional) at Dual speed        */
    /* ------------------------------------------------------------------ */
    printf("\nStep 4: CMDINVAL test (SPEED=Dual + DIRECTION=Bidirectional)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x12345678);
    cmd.val         = 0;
    cmd.f.len       = 0;
    cmd.f.csaat     = 0;
    cmd.f.speed     = 1;    /* Dual */
    cmd.f.direction = 3;    /* Bidirectional — invalid at Dual speed */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    for (delay = 0; delay < 200; delay++) {}

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    if (!check_reg("CMDINVAL for Bidirectional+Dual", err_status.f.cmdinval, 1))
        pass = 0;
    else
        printf("  PASS: CMDINVAL detected for Bidirectional+Dual (expected)\n");

    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* SW_RST to recover */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 200; delay++) {}

    /* ------------------------------------------------------------------ */
    /* Step 5: Bidirectional accepted at Standard speed (SPEED=0)          */
    /* ------------------------------------------------------------------ */
    printf("\nStep 5: Bidirectional valid at Standard speed (SPEED=0, DIR=3)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xABCD1234);
    cmd.val         = 0;
    cmd.f.len       = 0;
    cmd.f.csaat     = 0;
    cmd.f.speed     = 0;    /* Standard */
    cmd.f.direction = 3;    /* Bidirectional — valid at Standard speed */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval);
    if (err_status.f.cmdinval) {
        printf("  FAIL: CMDINVAL for valid Standard Bidirectional command\n");
        pass = 0;
    } else {
        printf("  PASS: Standard Bidirectional accepted (no CMDINVAL)\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT DUAL SPI TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT DUAL SPI TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
