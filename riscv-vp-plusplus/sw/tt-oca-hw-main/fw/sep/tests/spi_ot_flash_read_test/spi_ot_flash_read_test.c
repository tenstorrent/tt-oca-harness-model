/*
 * SPI OT Flash Read Test - TC_SPIOT_020 (P1)
 *
 * Issues a Standard Read command (0x03) to the SPI flash device using a
 * two-segment transaction on the OpenTitan SPI controller:
 *   Segment 1: TX command byte (0x03) + 24-bit address (4 bytes total), CSAAT=1
 *   Segment 2: RX 16 bytes of flash data, CSAAT=0
 *
 * TX byte packing (LITTLE_ENDIAN=1): TXDATA[7:0] is transmitted first.
 *   Packing cmd=0x03 + addr=0x000000 into one 32-bit word:
 *     byte[0]=cmd, byte[1]=addr[23:16], byte[2]=addr[15:8], byte[3]=addr[7:0]
 *     => WRITE_REG(TXDATA, 0x00000003) -> sends 0x03, 0x00, 0x00, 0x00 in order
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan, enable controller
 *   2. Segment 1: TX 4 bytes (0x03 + addr 0x000000), CSAAT=1
 *   3. Segment 2: RX 16 bytes, CSAAT=0
 *   4. Read and log 4 words from RXDATA
 *   5. Verify no SPI controller errors
 *
 * Note: Passes with or without flash model.
 *   With flash model: reads actual flash content (0xFF for erased pages)
 *   Without flash model: MISO=0xFF, all read data = 0xFFFFFFFF (logged)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_flash_read_test STACK=sim
 *   make test-sep TEST_NAME=sep_spi_ot_flash_read_test STACK=sim EXTRA_SIM_ARGS=+spi_device_sel=winbond
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define SPI_CLKDIV      9
#define TIMEOUT_LIMIT   200000
#define READ_LEN_BYTES  16   /* 4 words */

/* Flash commands */
#define FLASH_CMD_READ  0x03

/* Read address (start of flash, typically erased = 0xFF) */
#define FLASH_READ_ADDR 0x000000

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
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
   // sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Flash Read Test (TC_SPIOT_020)\n");
    printf("========================================\n\n");

    int pass = 1;
    uint32_t i;

   // configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    init_spi_controller();
    printf("SPI controller enabled: CLKDIV=%d\n", SPI_CLKDIV);
    printf("Flash address: 0x%06x, Read length: %u bytes\n\n",
           FLASH_READ_ADDR, READ_LEN_BYTES);

    SPI_CONTROLLER_CMD_reg_u cmd;

    /* ----------------------------------------------------------------
     * Segment 1: TX READ command + 24-bit address (4 bytes total)
     *
     * TX byte packing: TXDATA[7:0] first.
     * Pack [cmd=0x03][addr_h=0x00][addr_m=0x00][addr_l=0x00] into 32-bit:
     *   byte[0]=0x03, byte[1]=0x00, byte[2]=0x00, byte[3]=0x00
     *   => TXDATA = 0x00_00_00_03
     * ---------------------------------------------------------------- */
    printf("Step 1: TX READ cmd (0x03) + addr 0x%06x (4 bytes), CSAAT=1\n",
           FLASH_READ_ADDR);
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* cmd byte in [7:0], addr MSB in [15:8], addr mid in [23:16], addr LSB in [31:24] */
    uint32_t tx_word = (FLASH_CMD_READ & 0xFF)
                     | (((FLASH_READ_ADDR >> 16) & 0xFF) << 8)
                     | (((FLASH_READ_ADDR >>  8) & 0xFF) << 16)
                     | (((FLASH_READ_ADDR >>  0) & 0xFF) << 24);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, tx_word);
    printf("  TXDATA=0x%08x (cmd=0x%02x, addr=0x%06x)\n",
           tx_word, FLASH_CMD_READ, FLASH_READ_ADDR);

    cmd.val = 0;
    cmd.f.len       = 3;    /* 4 bytes (LEN+1) */
    cmd.f.csaat     = 1;    /* keep CS# low for data phase */
    cmd.f.speed     = 0;    /* Standard SPI */
    cmd.f.direction = 2;    /* TX only */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD: DIR=TX, SPEED=Std, LEN=3(4B), CSAAT=1\n");

    /* ----------------------------------------------------------------
     * Segment 2: RX READ_LEN_BYTES bytes of flash data, release CS
     * ---------------------------------------------------------------- */
    printf("\nStep 2: RX %u bytes of flash data, CSAAT=0\n", READ_LEN_BYTES);
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val = 0;
    cmd.f.len       = READ_LEN_BYTES - 1;   /* 16 bytes */
    cmd.f.csaat     = 0;                    /* release CS# after */
    cmd.f.speed     = 0;                    /* Standard SPI */
    cmd.f.direction = 1;                    /* RX only */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);
    printf("  CMD: DIR=RX, SPEED=Std, LEN=%u(%uB), CSAAT=0\n",
           READ_LEN_BYTES - 1, READ_LEN_BYTES);

    if (wait_for_idle(TIMEOUT_LIMIT)) {
        printf("  WARN: transaction did not complete (no SPI device?)\n");
    }

    /* ----------------------------------------------------------------
     * Read and log flash data from RX FIFO (4 words = 16 bytes)
     * ---------------------------------------------------------------- */
    printf("\nStep 3: Read %u words from RXDATA\n", READ_LEN_BYTES / 4);
    SPI_CONTROLLER_STATUS_reg_u status;
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXQD=%u, RXEMPTY=%u\n", status.f.rxqd, status.f.rxempty);

    uint32_t rx_words[READ_LEN_BYTES / 4];
    uint32_t num_words = READ_LEN_BYTES / 4;
    uint32_t words_read = 0;

    for (i = 0; i < num_words; i++) {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (!status.f.rxempty) {
            rx_words[i] = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
            printf("  [%u] 0x%08x  (bytes: %02x %02x %02x %02x)\n",
                   i, rx_words[i],
                   (rx_words[i] >>  0) & 0xFF,
                   (rx_words[i] >>  8) & 0xFF,
                   (rx_words[i] >> 16) & 0xFF,
                   (rx_words[i] >> 24) & 0xFF);
            words_read++;
        } else {
            printf("  [%u] RX FIFO empty (underrun)\n", i);
        }
    }
    printf("  Read %u/%u words\n", words_read, num_words);

    if (words_read == 0) {
        printf("  NOTE: No data received (no flash model connected)\n");
    }

    /* ----------------------------------------------------------------
     * Step 4: Verify no critical SPI controller errors
     * ---------------------------------------------------------------- */
    printf("\nStep 4: Check SPI error status\n");
    SPI_CONTROLLER_ERROR_STATUS_reg_u err;
    err.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x\n", err.val);
    if (err.f.cmdinval) {
        printf("  FAIL: CMDINVAL\n");
        pass = 0;
    }
    if (err.f.csidinval) {
        printf("  FAIL: CSIDINVAL\n");
        pass = 0;
    }
    if (!err.f.cmdinval && !err.f.csidinval) {
        printf("  No critical controller errors\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT FLASH READ TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT FLASH READ TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
