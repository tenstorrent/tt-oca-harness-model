/*
 * SPI OT Watermark Test - TC_SPIOT_018 (P1)
 *
 * Verifies TX and RX watermark configuration and status bit transitions.
 *
 * CTRL register watermark fields:
 *   CTRL[7:0]  = RX_WATERMARK (8-bit): RXWM=1 when RXQD > RX_WATERMARK
 *   CTRL[15:8] = TX_WATERMARK (8-bit): TXWM=1 when TXQD < TX_WATERMARK
 *
 * Default: CTRL_REG_DEFAULT=0x7F → RX_WM=0x7F=127, TX_WM=0x00=0
 *   - Default TXWM=0 (TXQD=0 is not < 0)
 *   - Default RXWM=0 (RXQD=0 is not > 127)
 *
 * Test Flow:
 *   1. Verify default watermarks (RX_WM=127, TX_WM=0), TXWM=0, RXWM=0
 *   2. Set TX_WM=1: TXWM=1 (empty FIFO: TXQD=0 < 1)
 *   3. Write 2 words to TX FIFO: TXWM=0 (TXQD=2 >= 1)
 *   4. Set TX_WM=4: TXWM=1 (TXQD=2 < 4)
 *   5. Write 2 more words (TXQD=4): TXWM=0 (TXQD=4 >= 4)
 *   6. Set TX_WM=0: TXWM=0 always (0 < 0 is false)
 *   7. Verify RX_WM write-readback (min=0, max=0xFF, restore default)
 *   8. SW_RST to drain TX FIFO
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_watermark_test STACK=sim
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
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Watermark Test (TC_SPIOT_018)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_STATUS_reg_u status;
    volatile int delay;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller with defaults (TX_WM=0, RX_WM=127) */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* ------------------------------------------------------------------ */
    /* Step 1: Verify default watermarks                                   */
    /* Default CTRL=0x7F: rx_watermark=127, tx_watermark=0                */
    /* ------------------------------------------------------------------ */
    printf("Step 1: Default watermark values\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    printf("  CTRL=0x%08x: TX_WM=%u, RX_WM=%u\n",
           ctrl.val, ctrl.f.tx_watermark, ctrl.f.rx_watermark);
    if (ctrl.f.tx_watermark != 0) {
        printf("  FAIL: Default TX_WM expected 0, got %u\n", ctrl.f.tx_watermark);
        pass = 0;
    } else {
        printf("  PASS: Default TX_WM=0\n");
    }
    if (ctrl.f.rx_watermark != 0x7F) {
        printf("  FAIL: Default RX_WM expected 0x7F=127, got %u\n", ctrl.f.rx_watermark);
        pass = 0;
    } else {
        printf("  PASS: Default RX_WM=0x7F=127\n");
    }

    /* Verify STATUS bits with default watermarks (TX FIFO empty) */
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS: TXWM=%u (expected 0: TXQD=0 not < 0), RXWM=%u (expected 0: RXQD=0 not > 127)\n",
           status.f.txwm, status.f.rxwm);
    if (status.f.txwm != 0) {
        printf("  FAIL: TXWM should be 0 with TX_WM=0\n");
        pass = 0;
    } else {
        printf("  PASS: TXWM=0 correct with TX_WM=0\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 2: Set TX_WM=1: TXWM=1 (TXQD=0 < TX_WM=1)                    */
    /* ------------------------------------------------------------------ */
    printf("\nStep 2: Set TX_WM=1, verify TXWM=1 (TX FIFO below watermark)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.tx_watermark = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TX_WM=1, TXQD=%u → TXWM=%u (expected 1)\n",
           status.f.txqd, status.f.txwm);
    if (status.f.txwm != 1) {
        printf("  FAIL: TXWM should be 1 (TXQD=%u < TX_WM=1)\n", status.f.txqd);
        pass = 0;
    } else {
        printf("  PASS: TXWM=1 correct (TX FIFO needs filling)\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 3: Write 2 words to TX FIFO: TXWM=0 (TXQD=2 >= TX_WM=1)      */
    /* ------------------------------------------------------------------ */
    printf("\nStep 3: Write 2 words to TX FIFO, verify TXWM=0\n");
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xAABBCCDD);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x11223344);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TX_WM=1, TXQD=%u → TXWM=%u (expected 0)\n",
           status.f.txqd, status.f.txwm);
    if (status.f.txwm != 0) {
        printf("  FAIL: TXWM should be 0 (TXQD=%u >= TX_WM=1)\n", status.f.txqd);
        pass = 0;
    } else {
        printf("  PASS: TXWM=0 correct (TX FIFO above watermark)\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 4: Raise TX_WM to 4: TXWM=1 (TXQD=2 < TX_WM=4)              */
    /* ------------------------------------------------------------------ */
    printf("\nStep 4: Set TX_WM=4, verify TXWM=1 (TXQD=2 < 4)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.tx_watermark = 4;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TX_WM=4, TXQD=%u → TXWM=%u (expected 1)\n",
           status.f.txqd, status.f.txwm);
    if (status.f.txwm != 1) {
        printf("  FAIL: TXWM should be 1 (TXQD=%u < TX_WM=4)\n", status.f.txqd);
        pass = 0;
    } else {
        printf("  PASS: TXWM=1 correct (watermark raised above TXQD)\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 5: Write 2 more words (TXQD→4): TXWM=0 (TXQD=4 >= TX_WM=4)  */
    /* ------------------------------------------------------------------ */
    printf("\nStep 5: Write 2 more words (TXQD→4), verify TXWM=0\n");
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x55667788);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x99AABBCC);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TX_WM=4, TXQD=%u → TXWM=%u (expected 0)\n",
           status.f.txqd, status.f.txwm);
    if (status.f.txwm != 0) {
        printf("  FAIL: TXWM should be 0 (TXQD=%u >= TX_WM=4)\n", status.f.txqd);
        pass = 0;
    } else {
        printf("  PASS: TXWM=0 correct (TXQD meets watermark)\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 6: Set TX_WM=0: TXWM=0 always (TXQD < 0 is impossible)       */
    /* ------------------------------------------------------------------ */
    printf("\nStep 6: Set TX_WM=0, verify TXWM=0 (threshold disabled)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.tx_watermark = 0;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TX_WM=0, TXQD=%u → TXWM=%u (expected 0)\n",
           status.f.txqd, status.f.txwm);
    if (status.f.txwm != 0) {
        printf("  FAIL: TXWM should be 0 with TX_WM=0 (no threshold)\n");
        pass = 0;
    } else {
        printf("  PASS: TXWM=0 correct (watermark disabled)\n");
    }

    /* ------------------------------------------------------------------ */
    /* Step 7: RX_WM write-readback verification (min=0, max=0xFF)         */
    /* ------------------------------------------------------------------ */
    printf("\nStep 7: RX_WM write-readback (min=0, max=0xFF, restore)\n");
    /* min: RX_WM=0 */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.rx_watermark = 0;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    printf("  RX_WM=0 readback: %u\n", ctrl.f.rx_watermark);
    if (ctrl.f.rx_watermark != 0) {
        printf("  FAIL: RX_WM=0 readback failed\n");
        pass = 0;
    } else {
        printf("  PASS: RX_WM=0 readback OK\n");
    }

    /* max: RX_WM=0xFF */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.rx_watermark = 0xFF;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    printf("  RX_WM=0xFF readback: 0x%02x\n", ctrl.f.rx_watermark);
    if (ctrl.f.rx_watermark != 0xFF) {
        printf("  FAIL: RX_WM=0xFF readback failed\n");
        pass = 0;
    } else {
        printf("  PASS: RX_WM=0xFF readback OK\n");
    }

    /* Restore default RX_WM=127 */
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.rx_watermark = 0x7F;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    /* ------------------------------------------------------------------ */
    /* Step 8: SW_RST to drain TX FIFO                                     */
    /* ------------------------------------------------------------------ */
    printf("\nStep 8: SW_RST to drain TX FIFO\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CTRL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 1000; delay++) {}

    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  After SW_RST: TXEMPTY=%u, TXQD=%u\n",
           status.f.txempty, status.f.txqd);
    if (!status.f.txempty) {
        printf("  FAIL: TXEMPTY should be 1 after SW_RST\n");
        pass = 0;
    } else {
        printf("  PASS: TX FIFO drained by SW_RST\n");
    }

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT WATERMARK TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT WATERMARK TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
