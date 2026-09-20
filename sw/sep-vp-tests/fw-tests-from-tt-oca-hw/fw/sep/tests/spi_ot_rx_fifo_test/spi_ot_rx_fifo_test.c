// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT RX FIFO Test - TC_SPIOT_006 (P0)
 *
 * Verifies RX FIFO fill, status monitoring (RXQD, RXEMPTY, RXFULL, RXWM),
 * underflow error detection, and RX watermark behavior.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Verify RXEMPTY=1, RXQD=0 initially
 *   3. Issue 16-byte RX command (MISO=0xFF without flash model)
 *   4. Verify RXQD=4, RXEMPTY=0 after transaction
 *   5. Drain RX FIFO, verify RXQD→0, RXEMPTY=1
 *   6. Test UNDERFLOW (read RXDATA when empty → ERROR_STATUS.underflow=1)
 *   7. Test RXWM: issue RX again with RX_WATERMARK=2, check RXWM=1
 *
 * Note: Without a flash model MISO is 0xFF, so RX words will be 0xFFFFFFFF.
 *   The test verifies FIFO behavior (RXQD, RXEMPTY, RXWM), not data content.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_rx_fifo_test STACK=cgen,sim
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

#define SPI_CLKDIV    spi_clkdiv()
#define TIMEOUT_LIMIT 200000
#define RX_LEN_BYTES  16    /* 4 words */

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static int wait_for_ready(int timeout)
{
    SPI_CONTROLLER_STATUS_reg_u status;
    while (timeout-- > 0) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (status.f.ready) return 0;
    }
    printf("  TIMEOUT waiting for READY\n");
    return -1;
}

static int wait_for_idle(int timeout)
{
    SPI_CONTROLLER_STATUS_reg_u status;
    while (timeout-- > 0) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.active) return 0;
    }
    printf("  TIMEOUT waiting for ACTIVE=0\n");
    return -1;
}

/* Issue an RX-only command of rx_bytes bytes (max 256) */
static void issue_rx_cmd(uint32_t rx_bytes)
{
    SPI_CONTROLLER_COMMAND_reg_u cmd;
    cmd.val = 0;
    cmd.f.len       = rx_bytes - 1;
    cmd.f.csaat     = 0;
    cmd.f.speed     = 0;
    cmd.f.direction = 1;    /* RX only */
    WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT RX FIFO Test (TC_SPIOT_006)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    uint32_t i;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien     = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    /* Configure SPI clock */
    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    cfg.val = 0;
    cfg.f.clkdiv    = SPI_CLKDIV;
    cfg.f.cpol      = 0;
    cfg.f.cpha      = 0;
    cfg.f.csnidle   = 2;
    cfg.f.csnlead   = 2;
    cfg.f.csntrail  = 2;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    printf("SPI controller enabled: CLKDIV=%d\n", SPI_CLKDIV);

    /* -------------------------------------------------------------------
     * Step 1: Verify RX FIFO empty initially
     * ------------------------------------------------------------------- */
    printf("\nStep 1: RX FIFO initial state\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXEMPTY=%u, RXFULL=%u, RXQD=%u\n",
           status.f.rxempty, status.f.rxfull, status.f.rxqd);
    if (status.f.rxempty != 1) {
        printf("  FAIL: RXEMPTY should be 1\n");
        pass = 0;
    } else {
        printf("  PASS: RX FIFO empty\n");
    }
    if (status.f.rxqd != 0) {
        printf("  FAIL: RXQD should be 0\n");
        pass = 0;
    } else {
        printf("  PASS: RXQD=0\n");
    }

    /* -------------------------------------------------------------------
     * Step 2: Issue 16-byte RX command and wait for completion
     * Without a flash model MISO=0xFF → 0xFFFFFFFF per word
     * ------------------------------------------------------------------- */
    printf("\nStep 2: Issue RX %u bytes, wait for idle\n", RX_LEN_BYTES);
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    issue_rx_cmd(RX_LEN_BYTES);
    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  WARN: transaction did not complete (no SPI device?)\n");
    }

    /* -------------------------------------------------------------------
     * Step 3: Verify RXQD and RXEMPTY after transaction
     * ------------------------------------------------------------------- */
    printf("\nStep 3: RX FIFO state after transaction\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXEMPTY=%u, RXQD=%u (expected RXQD=%u)\n",
           status.f.rxempty, status.f.rxqd, RX_LEN_BYTES / 4);
    if (status.f.rxempty != 0) {
        printf("  FAIL: RXEMPTY should be 0 after RX transaction\n");
        pass = 0;
    } else {
        printf("  PASS: RXEMPTY=0 after RX\n");
    }
    if (status.f.rxqd != (RX_LEN_BYTES / 4)) {
        printf("  FAIL: RXQD=%u, expected %u\n", status.f.rxqd, RX_LEN_BYTES / 4);
        pass = 0;
    } else {
        printf("  PASS: RXQD=%u correct\n", status.f.rxqd);
    }

    /* -------------------------------------------------------------------
     * Step 4: Drain RX FIFO, verify RXQD→0 and RXEMPTY→1
     * ------------------------------------------------------------------- */
    printf("\nStep 4: Drain RX FIFO (%u words)\n", RX_LEN_BYTES / 4);
    uint32_t words_read = 0;
    for (i = 0; i < RX_LEN_BYTES / 4; i++) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.rxempty) {
            uint32_t word = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
            printf("  [%u] 0x%08x\n", i, word);
            words_read++;
        } else {
            printf("  [%u] RX FIFO empty (underrun)\n", i);
        }
    }
    printf("  Read %u/%u words\n", words_read, RX_LEN_BYTES / 4);
    if (words_read != RX_LEN_BYTES / 4) {
        printf("  FAIL: Expected %u words, got %u\n", RX_LEN_BYTES / 4, words_read);
        pass = 0;
    }

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  After drain: RXEMPTY=%u, RXQD=%u\n", status.f.rxempty, status.f.rxqd);
    if (status.f.rxempty != 1) {
        printf("  FAIL: RXEMPTY should be 1 after draining all words\n");
        pass = 0;
    } else {
        printf("  PASS: RXEMPTY=1 after drain\n");
    }
    if (status.f.rxqd != 0) {
        printf("  FAIL: RXQD should be 0 after drain\n");
        pass = 0;
    } else {
        printf("  PASS: RXQD=0 after drain\n");
    }

    /* -------------------------------------------------------------------
     * Step 5: UNDERFLOW test (read from empty RX FIFO)
     * ------------------------------------------------------------------- */
    printf("\nStep 5: UNDERFLOW test (read empty RX FIFO)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    uint32_t dummy = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
    (void)dummy;
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x, UNDERFLOW=%u\n", err_status.val, err_status.f.underflow);
    if (err_status.f.underflow) {
        printf("  PASS: UNDERFLOW error detected\n");
    } else {
        printf("  FAIL: UNDERFLOW not detected after reading empty RX FIFO\n");
        pass = 0;
    }
    /* Clear UNDERFLOW */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, err_status.val);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  After W1C clear: ERROR_STATUS=0x%08x\n", err_status.val);
    if (err_status.f.underflow) {
        printf("  FAIL: UNDERFLOW did not clear on W1C write\n");
        pass = 0;
    } else {
        printf("  PASS: UNDERFLOW cleared correctly\n");
    }

    /* -------------------------------------------------------------------
     * Step 6: RXWM test (RX watermark)
     * Set RX_WATERMARK=2, issue 16-byte RX (4 words → RXQD=4 >= WM=2)
     * RXWM should be 1
     * ------------------------------------------------------------------- */
    printf("\nStep 6: RXWM test (RX_WATERMARK=2)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.rx_watermark = 2;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    issue_rx_cmd(RX_LEN_BYTES);
    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  WARN: transaction did not complete\n");
    }

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXQD=%u, RX_WM=2, RXWM=%u\n", status.f.rxqd, status.f.rxwm);
    if (status.f.rxwm != 1) {
        printf("  FAIL: RXWM should be 1 (RXQD=%u >= WM=2)\n", status.f.rxqd);
        pass = 0;
    } else {
        printf("  PASS: RXWM=1 correct\n");
    }

    /* Drain and verify RXWM=0 when RXQD drops below WM */
    printf("  Draining 3 words (RXQD expected to become 1, below WM=2)\n");
    for (i = 0; i < 3; i++) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.rxempty) {
            (void)READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        }
    }
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXQD=%u, RXWM=%u (expected RXWM=0)\n", status.f.rxqd, status.f.rxwm);
    if (status.f.rxwm != 0) {
        printf("  FAIL: RXWM should be 0 when RXQD < WM=2\n");
        pass = 0;
    } else {
        printf("  PASS: RXWM=0 after draining below watermark\n");
    }

    /* Drain remaining */
    for (i = 0; i < 4; i++) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.rxempty) {
            (void)READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        }
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT RX FIFO TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT RX FIFO TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
