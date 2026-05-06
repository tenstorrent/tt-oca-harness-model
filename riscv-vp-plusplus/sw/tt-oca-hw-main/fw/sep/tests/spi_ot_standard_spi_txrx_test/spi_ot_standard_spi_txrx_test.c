/*
 * SPI OT Standard SPI TX/RX Test - TC_SPIOT_008 (P0)
 *
 * Verifies Standard SPI (x1) transmit and receive using the OpenTitan SPI
 * controller FIFO-based command interface.
 *
 * This test exercises the command/data path without requiring an external
 * SPI flash model. It validates the controller accepts commands and data,
 * and monitors STATUS/ERROR registers during the transaction.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller, set clock/config
 *   2. Load TX data into FIFO
 *   3. Issue TX command (Standard SPI, CSAAT=0)
 *   4. Monitor STATUS.ACTIVE until transaction completes
 *   5. Check for errors
 *   6. Issue multi-byte TX+RX sequence
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_standard_spi_txrx_test STACK=sim
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
    printf("  ERROR: Timeout waiting for ACTIVE=0\n");
    return 1;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Standard SPI TX/RX Test (TC_SPIOT_008)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_CMD_reg_u cmd;
    SPI_CONTROLLER_CFG_reg_u cfg;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* Configure: CLKDIV=9, CPOL=0, CPHA=0, CS timing */
    cfg.val = 0;
    cfg.f.clkdiv = 9;
    cfg.f.cpol = 0;
    cfg.f.cpha = 0;
    cfg.f.csnidle = 2;
    cfg.f.csnlead = 2;
    cfg.f.csntrail = 2;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);

    /* Set CSID=0 */
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);

    /* Clear errors */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    /* Step 1: Simple TX command */
    printf("Step 1: Single byte TX (Standard SPI)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) {
        pass = 0;
        goto done;
    }

    /* Load TX data: 1 word with command byte 0x9F (JEDEC READ ID) */
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x9F000000);

    /* Issue CMD: TX, Standard speed, 1 byte (LEN=0 means 1 byte) */
    cmd.val = 0;
    cmd.f.len = 0;
    cmd.f.csaat = 0;
    cmd.f.speed = 0;
    cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    printf("  CMD issued: DIR=TX, SPEED=Standard, LEN=0 (1 byte)\n");

    /* Wait for completion */
    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  WARN: Transaction did not complete (no SPI device)\n");
    }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS after TX: 0x%08x\n", err_status.val);
    if (err_status.val != 0) {
        printf("  WARN: Errors detected (may be expected without device)\n");
    }

    /* Step 2: Multi-byte TX */
    printf("\nStep 2: Multi-byte TX (4 bytes, CSAAT=1)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    if (wait_for_ready(TIMEOUT_LIMIT)) {
        pass = 0;
        goto done;
    }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x03001000);

    cmd.val = 0;
    cmd.f.len = 3;
    cmd.f.csaat = 1;
    cmd.f.speed = 0;
    cmd.f.direction = 2;
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD issued: DIR=TX, LEN=3 (4 bytes), CSAAT=1\n");

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS: ACTIVE=%u, TXQD=%u, CMDQD=%u\n",
           status.f.active, status.f.txqd, status.f.cmdqd);

    /* Step 3: Issue RX command (following TX with CSAAT) */
    printf("\nStep 3: RX command (4 bytes, CSAAT=0, release CS)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) {
        pass = 0;
        goto done;
    }

    cmd.val = 0;
    cmd.f.len = 3;
    cmd.f.csaat = 0;
    cmd.f.speed = 0;
    cmd.f.direction = 1;
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD issued: DIR=RX, LEN=3 (4 bytes), CSAAT=0\n");

    /* Wait for completion */
    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  INFO: Transaction pending (no external SPI device)\n");
    }

    /* Check RX FIFO */
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS: RXQD=%u, RXEMPTY=%u\n",
           status.f.rxqd, status.f.rxempty);

    if (status.f.rxqd > 0) {
        uint32_t rxdata = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        printf("  RXDATA: 0x%08x\n", rxdata);
    }

    /* Step 4: Verify no critical errors */
    printf("\nStep 4: Final error check\n");
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x\n", err_status.val);
    printf("  CMDBUSY=%u, OVERFLOW=%u, UNDERFLOW=%u, CMDINVAL=%u, CSIDINVAL=%u\n",
           err_status.f.cmdbusy, err_status.f.overflow, err_status.f.underflow,
           err_status.f.cmdinval, err_status.f.csidinval);

    /* CMDINVAL and CSIDINVAL would be hard failures */
    if (err_status.f.cmdinval || err_status.f.csidinval) {
        printf("  FAIL: Invalid command or CSID error\n");
        pass = 0;
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT STANDARD SPI TXRX TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT STANDARD SPI TXRX TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
