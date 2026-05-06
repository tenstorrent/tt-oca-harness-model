/*
 * SPI OT Quad SPI Test - TC_SPIOT_016 (P1)
 *
 * Verifies Quad SPI (x4) mode transmit, dummy cycles, and receive using
 * CMD.SPEED=2. Also verifies that DIRECTION=3 (bidirectional) at Quad speed
 * triggers CMDINVAL (bidirectional only valid at Standard speed).
 *
 * A typical quad read sequence: TX cmd/addr → Dummy cycles → RX data.
 * All 4 data lines (SD[3:0]) are active in Quad mode.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller (CLKDIV=9, Mode 0)
 *   2. Quad TX: SPEED=2, DIRECTION=2, LEN=3 (4 bytes), CSAAT=1
 *   3. Quad Dummy: SPEED=2, DIRECTION=0, LEN=7 (8 dummy cycles), CSAAT=1
 *   4. Quad RX: SPEED=2, DIRECTION=1, LEN=3 (4 bytes), CSAAT=0
 *   5. CMDINVAL test: SPEED=2 + DIRECTION=3 (bidirectional) must fail
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_quad_spi_test STACK=sim
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
    printf("SPI OT Quad SPI Test (TC_SPIOT_016)\n");
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
    /* Step 1: Quad TX (SPEED=2, DIRECTION=2)                              */
    /* ------------------------------------------------------------------ */
    printf("Step 1: Quad SPI TX (SPEED=Quad, DIR=TX, LEN=3=4bytes, CSAAT=1)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* Load TX FIFO: 4 bytes = 1 word (Quad fast-read command pattern) */
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xEB000000);

    cmd.val         = 0;
    cmd.f.len       = 3;    /* 4 bytes (LEN+1 bytes total) */
    cmd.f.csaat     = 1;    /* keep CS# low for next segment */
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u CSIDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval, err_status.f.csidinval);
    if (err_status.f.cmdinval || err_status.f.csidinval) {
        printf("  FAIL: CMD error for valid Quad TX command\n");
        pass = 0;
    } else {
        printf("  PASS: Quad TX accepted (no CMDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* ------------------------------------------------------------------ */
    /* Step 2: Quad Dummy cycles (SPEED=2, DIRECTION=0)                    */
    /* ------------------------------------------------------------------ */
    printf("\nStep 2: Quad Dummy cycles (SPEED=Quad, DIR=Dummy, LEN=7=8cycles, CSAAT=1)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val         = 0;
    cmd.f.len       = 7;    /* 8 dummy cycles */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 0;    /* Dummy */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval);
    if (err_status.f.cmdinval) {
        printf("  FAIL: CMDINVAL for valid Quad Dummy command\n");
        pass = 0;
    } else {
        printf("  PASS: Quad Dummy accepted (no CMDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* ------------------------------------------------------------------ */
    /* Step 3: Quad RX (SPEED=2, DIRECTION=1)                              */
    /* ------------------------------------------------------------------ */
    printf("\nStep 3: Quad SPI RX (SPEED=Quad, DIR=RX, LEN=3=4bytes, CSAAT=0)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val         = 0;
    cmd.f.len       = 3;    /* 4 bytes */
    cmd.f.csaat     = 0;    /* release CS# after */
    cmd.f.speed     = 2;    /* Quad */
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
        printf("  FAIL: CMD error for valid Quad RX command\n");
        pass = 0;
    } else {
        printf("  PASS: Quad RX accepted (no CMDINVAL/CSIDINVAL)\n");
    }
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* SW_RST to drain RX FIFO before CMDINVAL test */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 200; delay++) {}

    /* ------------------------------------------------------------------ */
    /* Step 4: CMDINVAL — DIRECTION=3 (bidirectional) at Quad speed        */
    /* Bidirectional is only valid at Standard (SPEED=0) speed.            */
    /* ------------------------------------------------------------------ */
    printf("\nStep 4: CMDINVAL test (SPEED=Quad + DIRECTION=Bidirectional)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x12345678);
    cmd.val         = 0;
    cmd.f.len       = 0;
    cmd.f.csaat     = 0;
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 3;    /* Bidirectional — invalid at Quad speed */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    for (delay = 0; delay < 200; delay++) {}

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    if (!check_reg("CMDINVAL for Bidirectional+Quad", err_status.f.cmdinval, 1))
        pass = 0;
    else
        printf("  PASS: CMDINVAL detected for Bidirectional+Quad (expected)\n");

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT QUAD SPI TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT QUAD SPI TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
