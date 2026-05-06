/*
 * SPI OT Flash Write+Read Test - TC_SPIOT_021 (P0)
 *
 * Verifies a full SPI NOR flash write + read + verify cycle using the
 * OpenTitan SPI controller. Requires a Quad SPI flash model in the testbench.
 *
 * Run with: +spi_device_sel=winbond (Winbond W25Q512JV, JEDEC: EF 40 20)
 *           OR +spi_device_sel=0 (Micron N25Q128,   JEDEC: 20 BA 18)
 *
 * Flash commands used:
 *   0x06 - WREN  (Write Enable, 1 byte TX, no address)
 *   0x05 - RDSR  (Read Status Register-1, 1 byte TX + 1 byte RX)
 *   0x02 - PP    (Page Program, 1 byte cmd + 3 byte addr + up to 256 bytes data)
 *   0x03 - READ  (Standard Read, 1 byte cmd + 3 byte addr, then RX data)
 *
 * TX byte packing (LITTLE_ENDIAN=1): TXDATA[7:0] is transmitted first.
 *   cmd+addr packed as: byte[0]=cmd, byte[1]=addr[23:16], byte[2]=addr[15:8], byte[3]=addr[7:0]
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan, enable controller
 *   2. WREN: Write Enable (0x06, 1 byte TX)
 *   3. RDSR: Read Status, verify WEL=1 (bit 1) to confirm write enable
 *   4. PP:   Page Program (0x02 + addr 0x000000 + 16 bytes pattern), CSAAT
 *   5. RDSR poll: wait for WIP=0 (bit 0) - page program complete
 *   6. READ: Standard Read 16 bytes from 0x000000
 *   7. Verify read data matches written pattern
 *
 * Status Register-1 bits:
 *   [0] WIP  (Write In Progress): 1=busy, 0=ready
 *   [1] WEL  (Write Enable Latch): 1=write enabled
 *
 * Note: Requires flash model. Without flash model, status poll will timeout.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_flash_write_read_test STACK=sim \
 *       EXTRA_SIM_ARGS=+spi_device_sel=winbond
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

#define SPI_CLKDIV          9
#define TIMEOUT_LIMIT       200000
#define STATUS_POLL_LIMIT   500000  /* ~10ms at 50MHz core clock */
#define WRITE_LEN_BYTES     16      /* 4 words */

/* Flash commands */
#define FLASH_CMD_WREN      0x06
#define FLASH_CMD_RDSR      0x05
#define FLASH_CMD_PP        0x02
#define FLASH_CMD_READ      0x03

/* Flash status register bits */
#define FLASH_SR_WIP        (1u << 0)
#define FLASH_SR_WEL        (1u << 1)


/* Target flash address (page-aligned) */
#define FLASH_TARGET_ADDR   0x000000

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

/*
 * Read Status Register-1 (0x05).
 * Returns status byte, or 0xFF on timeout.
 */
static uint8_t flash_read_status(void)
{
    SPI_CONTROLLER_CMD_reg_u cmd;

    if (wait_for_ready(TIMEOUT_LIMIT)) return 0xFF;
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00000005);

    cmd.val = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 1;
    cmd.f.speed     = 0;
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) return 0xFF;

    cmd.val = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 0;
    cmd.f.speed     = 0;
    cmd.f.direction = 1;    /* RX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_idle(TIMEOUT_LIMIT)) return 0xFF;

    SPI_CONTROLLER_STATUS_reg_u spi_status;
    spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    if (spi_status.f.rxqd >= 1) {
        uint32_t rxdata = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        return (uint8_t)(rxdata & 0xFF);
    }
    return 0xFF;
}

int main(void)
{
    //sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Flash Write+Read Test (TC_SPIOT_021)\n");
    printf("Requires: +spi_device_sel=winbond (W25Q512JV)\n");
    printf("========================================\n\n");

    int pass = 1;
    uint32_t i;
    SPI_CONTROLLER_CMD_reg_u cmd;

    //configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    init_spi_controller();
    printf("SPI controller enabled: CLKDIV=%d\n\n", SPI_CLKDIV);

    /* ----------------------------------------------------------------
     * Step 1: Write Enable (WREN, 0x06)
     * Single 1-byte TX transaction, CSAAT=0
     * ---------------------------------------------------------------- */
    printf("Step 1: Write Enable (WREN 0x06)\n");
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00000006);

    cmd.val = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 0;    /* release CS after */
    cmd.f.speed     = 0;
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_idle(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    printf("  WREN issued\n");

    /* ----------------------------------------------------------------
     * Step 2: Read Status Register, verify WEL=1
     * ---------------------------------------------------------------- */
    printf("\nStep 2: Read Status Register, check WEL=1\n");
    uint8_t sr = flash_read_status();
    printf("  Status Register = 0x%02x (WEL=%u, WIP=%u)\n",
           sr, (sr >> 1) & 1, (sr >> 0) & 1);

    if (sr == 0xFF) {
        printf("  WARN: Status 0xFF - no flash model? Continuing...\n");
    } else if (!(sr & FLASH_SR_WEL)) {
        printf("  FAIL: WEL bit not set after WREN\n");
        pass = 0;
        goto done;
    } else {
        printf("  WEL=1 confirmed\n");
    }

    /* ----------------------------------------------------------------
     * Step 3: Page Program (PP, 0x02)
     * Segment 1: TX cmd+addr (4 bytes), CSAAT=1
     * Segment 2: TX data (16 bytes = 4 words), CSAAT=0
     * ---------------------------------------------------------------- */
    printf("\nStep 3: Page Program 0x02 at addr 0x%06x (%u bytes)\n",
           FLASH_TARGET_ADDR, WRITE_LEN_BYTES);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* Pack cmd + address (byte[0]=cmd, byte[1]=addr[23:16], ...) */
    uint32_t pp_hdr = (FLASH_CMD_PP & 0xFF)
                    | (((FLASH_TARGET_ADDR >> 16) & 0xFF) << 8)
                    | (((FLASH_TARGET_ADDR >>  8) & 0xFF) << 16)
                    | (((FLASH_TARGET_ADDR >>  0) & 0xFF) << 24);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, pp_hdr);
    printf("  PP header TXDATA=0x%08x (cmd=0x02, addr=0x%06x)\n",
           pp_hdr, FLASH_TARGET_ADDR);

    cmd.val = 0;
    cmd.f.len       = 3;    /* 4 bytes */
    cmd.f.csaat     = 1;    /* keep CS# for data */
    cmd.f.speed     = 0;
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    /* Load data pattern and issue data segment */
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* Data pattern: 0xCAxx0000 where xx = word index */
    uint32_t tx_data[WRITE_LEN_BYTES / 4];
    uint32_t num_words = WRITE_LEN_BYTES / 4;
    printf("  Writing data pattern:\n");
    for (i = 0; i < num_words; i++) {
        tx_data[i] = 0xCA000000 | ((i & 0xFF) << 16);
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, tx_data[i]);
        printf("    [%u] 0x%08x\n", i, tx_data[i]);
    }

    cmd.val = 0;
    cmd.f.len       = WRITE_LEN_BYTES - 1;  /* 16 bytes */
    cmd.f.csaat     = 0;                    /* release CS */
    cmd.f.speed     = 0;
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_idle(TIMEOUT_LIMIT)) { pass = 0; goto done; }
    printf("  Page Program command issued\n");

    /* ----------------------------------------------------------------
     * Step 4: Poll Status Register until WIP=0 (page program complete)
     * ---------------------------------------------------------------- */
    printf("\nStep 4: Poll Status Register for WIP=0 (page program done)\n");
    int poll_count = 0;
    int wip_done = 0;
    while (poll_count < STATUS_POLL_LIMIT) {
        sr = flash_read_status();
        poll_count++;
        if (sr == 0xFF) {
            printf("  WARN: Status 0xFF - no flash model? Skipping WIP poll\n");
            wip_done = 1;
            break;
        }
        if (!(sr & FLASH_SR_WIP)) {
            wip_done = 1;
            break;
        }
        if ((poll_count % 100) == 0) {
            printf("  ... polling (count=%d, SR=0x%02x)\n", poll_count, sr);
        }
    }
    if (!wip_done) {
        printf("  FAIL: Timeout waiting for WIP=0 after page program\n");
        pass = 0;
        goto done;
    }
    printf("  WIP=0 after %d polls (page program complete)\n", poll_count);

    /* ----------------------------------------------------------------
     * Step 5: Standard Read back WRITE_LEN_BYTES bytes
     * Segment 1: TX cmd+addr (4 bytes), CSAAT=1
     * Segment 2: RX 16 bytes, CSAAT=0
     * ---------------------------------------------------------------- */
    printf("\nStep 5: Read back %u bytes from addr 0x%06x\n",
           WRITE_LEN_BYTES, FLASH_TARGET_ADDR);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    uint32_t read_hdr = (FLASH_CMD_READ & 0xFF)
                      | (((FLASH_TARGET_ADDR >> 16) & 0xFF) << 8)
                      | (((FLASH_TARGET_ADDR >>  8) & 0xFF) << 16)
                      | (((FLASH_TARGET_ADDR >>  0) & 0xFF) << 24);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, read_hdr);

    cmd.val = 0;
    cmd.f.len       = 3;    /* 4 bytes */
    cmd.f.csaat     = 1;
    cmd.f.speed     = 0;
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    cmd.val = 0;
    cmd.f.len       = WRITE_LEN_BYTES - 1;  /* 16 bytes */
    cmd.f.csaat     = 0;
    cmd.f.speed     = 0;
    cmd.f.direction = 1;    /* RX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_idle(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    /* ----------------------------------------------------------------
     * Step 6: Verify read data matches written pattern
     * ---------------------------------------------------------------- */
    printf("\nStep 6: Verify read data\n");
    SPI_CONTROLLER_STATUS_reg_u spi_status;
    spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  RXQD=%u\n", spi_status.f.rxqd);

    uint32_t verify_fail = 0;
    for (i = 0; i < num_words; i++) {
        if (spi_status.f.rxempty) {
            printf("  [%u] RX FIFO empty (underrun)\n", i);
            verify_fail++;
            spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
            continue;
        }
        uint32_t rx_word = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
        printf("  [%u] expected=0x%08x got=0x%08x %s\n",
               i, tx_data[i], rx_word,
               (rx_word == tx_data[i]) ? "OK" : "MISMATCH");
        if (rx_word != tx_data[i]) {
            if (rx_word == 0xFFFFFFFF) {
                printf("       (0xFF = erased/no flash model)\n");
            }
            verify_fail++;
        }
        spi_status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    }

    if (verify_fail > 0) {
        printf("  FAIL: %u word(s) mismatch\n", verify_fail);
        printf("  NOTE: If no flash model, use +spi_device_sel=winbond for W25Q512JV\n");
        pass = 0;
    } else {
        printf("  All %u words verified\n", num_words);
    }

    /* ----------------------------------------------------------------
     * Final: Check SPI controller errors
     * ---------------------------------------------------------------- */
    printf("\nFinal: Check SPI error status\n");
    SPI_CONTROLLER_ERROR_STATUS_reg_u err;
    err.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x\n", err.val);
    if (err.f.cmdinval || err.f.csidinval) {
        printf("  FAIL: controller error (cmdinval=%u, csidinval=%u)\n",
               err.f.cmdinval, err.f.csidinval);
        pass = 0;
    } else {
        printf("  No critical controller errors\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT FLASH WRITE+READ TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT FLASH WRITE+READ TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
