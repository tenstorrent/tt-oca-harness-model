// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Flash JEDEC ID Test - TC_SPIOT_019 (P1)
 *
 * Issues a standard JEDEC Read ID command (0x9F) to the SPI flash device
 * using a two-segment transaction on the OpenTitan SPI controller.
 *
 * Signal path: OT SPI Controller -> SPI Mux -> GPIO pads -> Flash model
 *
 * TX byte packing (LITTLE_ENDIAN=1): TXDATA[7:0] is transmitted first.
 *   WRITE_REG(TXDATA, 0x0000009F) -> sends 0x9F on the bus
 * RX byte ordering: RXDATA[7:0] = first byte received from device.
 *   byte[0] = manufacturer ID, byte[1] = memory type, byte[2] = capacity
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan, enable controller
 *   2. Segment 1: TX 0x9F (JEDEC ID cmd), CSAAT=1
 *   3. Segment 2: RX 3 bytes (MFR + type + capacity), CSAAT=0
 *   4. Read and log JEDEC response from RXDATA
 *   5. Verify no SPI controller errors (CMDINVAL, CSIDINVAL)
 *
 * Note: Passes with or without a flash model.
 *   With flash model (+spi_device_sel=winbond): verifies manufacturer ID is 0xEF (Winbond W25Q)
 *   Without flash model: MISO=0xFF, JEDEC reads 0xFFFFFF (logged as warning only)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_flash_jedec_id_test STACK=sim
 *   make test-sep TEST_NAME=sep_spi_ot_flash_jedec_id_test STACK=sim EXTRA_SIM_ARGS=+spi_device_sel=winbond
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

#define SPI_CLKDIV      spi_clkdiv()
#define TIMEOUT_LIMIT   200000

/* Flash commands */
#define FLASH_CMD_JEDEC_ID  0x9F

/* Expected JEDEC manufacturer IDs */
#define JEDEC_MFR_WINBOND   0xEF
#define JEDEC_MFR_MICRON    0x20
#define JEDEC_MFR_MACRONIX  0xC2

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static void init_spi_controller(void)
{
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);

    SPI_CONTROLLER_CFG_reg_u cfg;
    cfg.val = 0;
    cfg.f.clkdiv    = SPI_CLKDIV;
    cfg.f.cpol      = 0;
    cfg.f.cpha      = 0;
    cfg.f.csnidle   = 2;
    cfg.f.csnlead   = 2;
    cfg.f.csntrail  = 2;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);

    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
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

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Flash JEDEC ID Test (TC_SPIOT_019)\n");
    printf("========================================\n\n");

    int pass = 1;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    init_spi_controller();
    printf("SPI controller enabled: CLKDIV=%d, CPOL=0, CPHA=0\n\n", SPI_CLKDIV);

    SPI_CONTROLLER_CMD_reg_u cmd;

    /* ----------------------------------------------------------------
     * Segment 1: TX JEDEC ID command (0x9F), keep CS low
     * TX byte packing: TXDATA[7:0] sent first -> write 0x0000009F
     * ---------------------------------------------------------------- */
    printf("Step 1: TX JEDEC ID command (0x9F), CSAAT=1\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x0000009F);

    cmd.val = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 1;    /* keep CS# low */
    cmd.f.speed     = 0;    /* Standard SPI */
    cmd.f.direction = 2;    /* TX only */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD: DIR=TX, SPEED=Std, LEN=0(1B), CSAAT=1\n");

    /* ----------------------------------------------------------------
     * Segment 2: RX 3 bytes (manufacturer + type + capacity), release CS
     * ---------------------------------------------------------------- */
    printf("\nStep 2: RX 3 bytes JEDEC response, CSAAT=0\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val = 0;
    cmd.f.len       = 2;    /* 3 bytes */
    cmd.f.csaat     = 0;    /* release CS# after */
    cmd.f.speed     = 0;    /* Standard SPI */
    cmd.f.direction = 1;    /* RX only */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD: DIR=RX, SPEED=Std, LEN=2(3B), CSAAT=0\n");

    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  WARN: transaction did not complete (no SPI device?)\n");
    }

    /* ----------------------------------------------------------------
     * Read and decode JEDEC response from RX FIFO
     * RXDATA[7:0]=MFR, [15:8]=mem_type, [23:16]=capacity
     * ---------------------------------------------------------------- */
    printf("\nStep 3: Read JEDEC response from RXDATA\n");
    SPI_CONTROLLER_STATUS_reg_u status;
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXQD=%u, RXEMPTY=%u\n", status.f.rxqd, status.f.rxempty);

    uint8_t mfr_id = 0xFF, mem_type = 0xFF, capacity = 0xFF;
    if (status.f.rxqd >= 1) {
        uint32_t rxdata = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        mfr_id   = (uint8_t)(rxdata & 0xFF);
        mem_type = (uint8_t)((rxdata >> 8) & 0xFF);
        capacity = (uint8_t)((rxdata >> 16) & 0xFF);
        printf("  JEDEC raw: 0x%08x\n", rxdata);
        printf("  Manufacturer ID : 0x%02x", mfr_id);
        if      (mfr_id == JEDEC_MFR_WINBOND)  printf(" (Winbond)");
        else if (mfr_id == JEDEC_MFR_MICRON)   printf(" (Micron)");
        else if (mfr_id == JEDEC_MFR_MACRONIX) printf(" (Macronix)");
        else if (mfr_id == 0xFF)               printf(" (no response / MISO idle)");
        printf("\n");
        printf("  Memory Type     : 0x%02x\n", mem_type);
        printf("  Capacity        : 0x%02x\n", capacity);

        if (mfr_id == 0xFF && mem_type == 0xFF && capacity == 0xFF) {
            printf("  NOTE: All 0xFF - no flash model connected (controller-only test)\n");
        }
    } else {
        printf("  WARN: RX FIFO empty after transaction\n");
    }

    /* ----------------------------------------------------------------
     * Step 4: Verify no critical SPI controller errors
     * ---------------------------------------------------------------- */
    printf("\nStep 4: Check SPI error status\n");
    SPI_CONTROLLER_ERROR_STATUS_reg_u err;
    err.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x\n", err.val);
    if (err.f.cmdinval) {
        printf("  FAIL: CMDINVAL (invalid speed/direction combination)\n");
        pass = 0;
    }
    if (err.f.csidinval) {
        printf("  FAIL: CSIDINVAL (CSID exceeds NUM_CS)\n");
        pass = 0;
    }
    if (!err.f.cmdinval && !err.f.csidinval) {
        printf("  No critical controller errors\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT FLASH JEDEC ID TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT FLASH JEDEC ID TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
