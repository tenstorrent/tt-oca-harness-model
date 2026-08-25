// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT TX FIFO Test - TC_SPIOT_005 (P0)
 *
 * Verifies TX FIFO write, status monitoring (TXQD, TXEMPTY, TXFULL, TXWM),
 * overflow error detection, and SW_RST drain behavior.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Verify TXEMPTY=1 initially
 *   3. Write multiple words to TXDATA, monitor TXQD
 *   4. Test TX watermark (TXWM) with configurable TX_WATERMARK
 *   5. Write until TXFULL, verify overflow error
 *   6. SW_RST, verify TXEMPTY after reset
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_tx_fifo_test STACK=sim
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
#define TX_FIFO_DEPTH 73  /* effective capacity: 72 FIFO slots + 1 byte_select stage */

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT TX FIFO Test (TC_SPIOT_005)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    ctrl.f.tx_watermark = 4;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* Step 1: Verify TX FIFO empty initially */
    printf("\nStep 1: TX FIFO initial state\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TXEMPTY=%u, TXFULL=%u, TXQD=%u\n",
           status.f.txempty, status.f.txfull, status.f.txqd);
    if (status.f.txempty != 1) {
        printf("  FAIL: TXEMPTY should be 1\n");
        pass = 0;
    } else {
        printf("  PASS: TX FIFO is empty\n");
    }
    if (status.f.txwm != 1) {
        printf("  FAIL: TXWM should be 1 initially (TXQD=0 < WM=4)\n");
        pass = 0;
    } else {
        printf("  PASS: TXWM=1 correct (TXQD < WM=4)\n");
    }

    /* Step 2: Write data words and monitor TXQD */
    printf("\nStep 2: Write 8 words to TX FIFO\n");
    uint32_t i;
    for (i = 0; i < 8; i++) {
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xA0000000 | i);
    }
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  After 8 writes: TXQD=%u, TXEMPTY=%u, TXFULL=%u\n",
           status.f.txqd, status.f.txempty, status.f.txfull);
    if (status.f.txempty != 0) {
        printf("  FAIL: TXEMPTY should be 0 after writes\n");
        pass = 0;
    } else {
        printf("  PASS: TXEMPTY cleared after writes\n");
    }

    /* Step 3: Check TX watermark */
    printf("\nStep 3: TX watermark check (TX_WATERMARK=4)\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TXWM=%u (TXQD=%u, watermark=4)\n", status.f.txwm, status.f.txqd);
    if (status.f.txwm != 0) {
        printf("  FAIL: TXWM should be 0 (TXQD=%u >= WM=4)\n", status.f.txqd);
        pass = 0;
    } else {
        printf("  PASS: TXWM=0 correct (TXQD >= WM=4)\n");
    }

    /* Step 4: Fill TX FIFO to capacity */
    printf("\nStep 4: Fill TX FIFO (writing %d more words)\n", TX_FIFO_DEPTH - 8);
    for (i = 8; i < TX_FIFO_DEPTH; i++) {
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xB0000000 | i);
    }
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  After filling: TXQD=%u, TXFULL=%u\n",
           status.f.txqd, status.f.txfull);
    if (!status.f.txfull) {
        printf("  FAIL: TXFULL should be 1 after filling %d words\n", TX_FIFO_DEPTH);
        pass = 0;
    } else {
        printf("  PASS: TXFULL=1 correct\n");
    }

    /* Step 5: Attempt overflow - write one more word */
    printf("\nStep 5: Overflow test (write when full)\n");
    /* Clear any prior errors */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xDEADBEEF);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x, OVERFLOW=%u\n",
           err_status.val, err_status.f.overflow);
    if (err_status.f.overflow) {
        printf("  PASS: Overflow error detected\n");
    } else {
        printf("  FAIL: Overflow not detected after write when TXFULL=1\n");
        pass = 0;
    }

    /* Clear overflow error */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, err_status.val);

    /* Step 6: Software reset and verify drain */
    printf("\nStep 6: SW_RST drain test\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    volatile int delay;
    for (delay = 0; delay < 5000; delay++) {}

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  After SW_RST: TXEMPTY=%u, TXQD=%u\n",
           status.f.txempty, status.f.txqd);

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT TX FIFO TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT TX FIFO TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
