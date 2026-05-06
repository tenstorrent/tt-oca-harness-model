/*
 * SPI OT Multi-Segment Test - TC_SPIOT_020 (P1)
 *
 * Verifies that multiple SPI command segments can be chained using CSAAT=1
 * to keep CS# asserted across segments (typical flash/QSPI protocol sequence).
 *
 * Segment chain (mimics a quad fast-read sequence):
 *   Seg 1 (TX 1B, CSAAT=1):  Command byte   (0xEB)
 *   Seg 2 (TX 3B, CSAAT=1):  24-bit Address (0x00_1234)
 *   Seg 3 (Dum 2cy,CSAAT=1): Mode/dummy cycles
 *   Seg 4 (RX 4B, CSAAT=0):  Data read, release CS#
 *
 * Checks:
 *   - No CMDINVAL or CSIDINVAL error throughout the chain
 *   - Controller returns READY between segments (CMDQD drains)
 *   - Final CSAAT=0 command completes cleanly
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_multi_segment_test STACK=sim
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
    printf("  WARN: Timeout waiting for ACTIVE=0\n");
    return 1;
}

static int check_no_errors(const char *seg_name)
{
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    if (err_status.f.cmdinval || err_status.f.csidinval || err_status.f.cmdbusy) {
        printf("  FAIL %s: ERROR_STATUS=0x%08x (CMDINVAL=%u CSIDINVAL=%u CMDBUSY=%u)\n",
               seg_name, err_status.val,
               err_status.f.cmdinval, err_status.f.csidinval, err_status.f.cmdbusy);
        WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
        return 0;
    }
    printf("  PASS %s: no CMD errors\n", seg_name);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    return 1;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Multi-Segment Test (TC_SPIOT_020)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_CFG_reg_u cfg;
    SPI_CONTROLLER_CMD_reg_u cmd;
    SPI_CONTROLLER_STATUS_reg_u status;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* Configure: CLKDIV=9, Mode 0, standard CS timing */
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
    /* Segment 1: TX 1 byte command (0xEB = Quad Fast Read), CSAAT=1      */
    /* ------------------------------------------------------------------ */
    printf("Segment 1: TX cmd byte (0xEB), Standard, CSAAT=1\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  Pre-seg1: CMDQD=%u READY=%u\n", status.f.cmdqd, status.f.ready);

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xEB000000);
    cmd.val         = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 0;    /* Standard for command byte */
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    if (!check_no_errors("Seg1")) pass = 0;

    /* ------------------------------------------------------------------ */
    /* Segment 2: TX 3 bytes address (0x001234), Quad, CSAAT=1            */
    /* ------------------------------------------------------------------ */
    printf("\nSegment 2: TX 3-byte address (0x001234), Quad, CSAAT=1\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00123400);
    cmd.val         = 0;
    cmd.f.len       = 2;    /* 3 bytes */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    if (!check_no_errors("Seg2")) pass = 0;

    /* ------------------------------------------------------------------ */
    /* Segment 3: Dummy 2 cycles, Quad, CSAAT=1                           */
    /* ------------------------------------------------------------------ */
    printf("\nSegment 3: Dummy 2 cycles, Quad, CSAAT=1\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val         = 0;
    cmd.f.len       = 1;    /* 2 dummy cycles */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 0;    /* Dummy */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    if (!check_no_errors("Seg3")) pass = 0;

    /* ------------------------------------------------------------------ */
    /* Segment 4: RX 4 bytes data, Quad, CSAAT=0 (final segment)          */
    /* ------------------------------------------------------------------ */
    printf("\nSegment 4: RX 4 bytes, Quad, CSAAT=0 (final — releases CS#)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  Pre-seg4: CMDQD=%u READY=%u\n", status.f.cmdqd, status.f.ready);

    cmd.val         = 0;
    cmd.f.len       = 3;    /* 4 bytes */
    cmd.f.csaat     = 0;    /* release CS# after */
    cmd.f.speed     = 2;    /* Quad */
    cmd.f.direction = 1;    /* RX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    wait_for_idle(TIMEOUT_LIMIT);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  Post-seg4: CMDQD=%u ACTIVE=%u RXQD=%u\n",
           status.f.cmdqd, status.f.active, status.f.rxqd);
    if (!check_no_errors("Seg4")) pass = 0;

    /* Verify CMDQD returned to 0 after full chain completes */
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("\nFinal state: CMDQD=%u (expected 0)\n", status.f.cmdqd);
    if (status.f.cmdqd != 0) {
        printf("  FAIL: CMDQD should be 0 after all segments complete\n");
        pass = 0;
    } else {
        printf("  PASS: CMDQD=0 (all segments executed)\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT MULTI-SEGMENT TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT MULTI-SEGMENT TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
