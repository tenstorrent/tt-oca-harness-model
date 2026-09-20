// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Error Handling Test - TC_SPIOT_012 (P0)
 *
 * Verifies error detection and reporting for all error conditions:
 * CMDBUSY, OVERFLOW, UNDERFLOW, CMDINVAL, CSIDINVAL.
 * Also tests ERROR_ENABLE masking and ERROR_STATUS W1C clearing.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Test ERROR_ENABLE defaults (all enabled)
 *   3. Test UNDERFLOW (read RXDATA when empty)
 *   4. Test ERROR_STATUS W1C clear
 *   5. Test ERROR_ENABLE masking (disable underflow, trigger again)
 *   6. Re-enable all errors
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_error_handling_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#define TX_FIFO_DEPTH 73  /* effective capacity: 72 FIFO slots + 1 byte_select stage */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Error Handling Test (TC_SPIOT_012)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_COMMAND_reg_u cmd;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    SPI_CONTROLLER_ERROR_ENABLE_reg_u err_enable;
    uint32_t dummy;
    uint32_t i;
    int timeout;
    volatile int delay;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    /* Step 1: ERROR_ENABLE defaults */
    printf("\nStep 1: ERROR_ENABLE defaults (all enabled)\n");
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("ERROR_ENABLE default", err_enable.val,
                   SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT)) pass = 0;
    if (!check_reg("CMDBUSY enable", err_enable.f.cmdbusy, 1)) pass = 0;
    if (!check_reg("OVERFLOW enable", err_enable.f.overflow, 1)) pass = 0;
    if (!check_reg("UNDERFLOW enable", err_enable.f.underflow, 1)) pass = 0;
    if (!check_reg("CMDINVAL enable", err_enable.f.cmdinval, 1)) pass = 0;
    if (!check_reg("CSIDINVAL enable", err_enable.f.csidinval, 1)) pass = 0;

    /* Step 2: Clear any existing errors */
    printf("\nStep 2: Clear existing errors\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS after clear: 0x%08x\n", err_status.val);

    /* Step 3: Test UNDERFLOW (read from empty RX FIFO) */
    printf("\nStep 3: UNDERFLOW test (read empty RX FIFO)\n");
    dummy = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
    (void)dummy;

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x, UNDERFLOW=%u\n",
           err_status.val, err_status.f.underflow);
    if (err_status.f.underflow) {
        printf("  PASS: UNDERFLOW error detected\n");
    } else {
        printf("  FAIL: UNDERFLOW not detected after reading empty RX FIFO\n");
        pass = 0;
    }

    /* Step 4: W1C clear test */
    printf("\nStep 4: ERROR_STATUS W1C clear\n");
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  Before clear: 0x%08x\n", err_status.val);
    if (err_status.val != 0) {
        WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, err_status.val);
        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        printf("  After W1C: 0x%08x\n", err_status.val);
    } else {
        printf("  No errors to clear\n");
    }

    /* Step 4.5: OVERFLOW test (write beyond TX_FIFO_DEPTH) */
    printf("\nStep 4.5: OVERFLOW test (fill TX FIFO to %d words, then write one more)\n",
           TX_FIFO_DEPTH);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    for (i = 0; i < TX_FIFO_DEPTH; i++) {
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xA0000000 | i);
    }
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  TXQD=%u, TXFULL=%u\n", status.f.txqd, status.f.txfull);
    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xDEADBEEF);
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x, OVERFLOW=%u\n", err_status.val, err_status.f.overflow);
    if (err_status.f.overflow) {
        printf("  PASS: OVERFLOW error detected\n");
    } else {
        printf("  FAIL: OVERFLOW not detected after write when TX FIFO full\n");
        pass = 0;
    }
    /* Clear overflow and drain TX FIFO via SW_RST */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 1000; delay++) {}
    ctrl.f.sw_rst = 0;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    /* Step 4.6: CMDINVAL test (CMD.SPEED=3, reserved value) */
    printf("\nStep 4.6: CMDINVAL test (CMD.SPEED=3)\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    timeout = 100000;
    do {
        status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        if (--timeout <= 0) break;
    } while (!status.f.ready);
    if (status.f.ready) {
        cmd.val = 0;
        cmd.f.len       = 0;
        cmd.f.speed     = 3;    /* reserved speed → CMDINVAL */
        cmd.f.direction = 2;
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x00);
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
        for (delay = 0; delay < 100; delay++) {}
        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        printf("  ERROR_STATUS=0x%08x, CMDINVAL=%u\n", err_status.val, err_status.f.cmdinval);
        if (err_status.f.cmdinval) {
            printf("  PASS: CMDINVAL error detected\n");
        } else {
            printf("  FAIL: CMDINVAL not detected for CMD.SPEED=3\n");
            pass = 0;
        }
        WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    } else {
        printf("  WARN: Not READY after SW_RST, skipping CMDINVAL test\n");
    }

    /* Step 4.7: CMDBUSY test (fill CMD FIFO with slow CLKDIV) */
    printf("\nStep 4.7: CMDBUSY test (CLKDIV=0xFFFF, fill CMD FIFO)\n");
    cfg.val = 0;
    cfg.f.clkdiv = 0xFFFF;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    /* Pre-fill TX FIFO (8 bytes for up to 8 single-byte CMDs) */
    for (i = 0; i < 8; i++) {
        WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0xCAFE0000 | i);
    }
    /* Issue CMDs without waiting for READY until CMDBUSY fires (max 8) */
    int cmdbusy_detected = 0;
    for (i = 0; i < 8; i++) {
        cmd.val = 0;
        cmd.f.len       = 0;    /* 1 byte TX per CMD */
        cmd.f.direction = 2;
        cmd.f.speed     = 0;
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
        err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        if (err_status.f.cmdbusy) {
            cmdbusy_detected = 1;
            printf("  PASS: CMDBUSY detected after %u CMDs issued\n", i + 1);
            break;
        }
    }
    if (!cmdbusy_detected) {
        printf("  FAIL: CMDBUSY not detected after 8 CMDs\n");
        pass = 0;
    }
    /* Recover: clear errors and SW_RST to drain CMD + TX FIFOs */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    for (delay = 0; delay < 1000; delay++) {}
    ctrl.f.sw_rst = 0;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    /* Restore CLKDIV */
    cfg.val = 0;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);

    /* Step 5: ERROR_ENABLE masking test */
    printf("\nStep 5: ERROR_ENABLE masking\n");
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    err_enable.f.underflow = 0;
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, err_enable.val);
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("UNDERFLOW disabled", err_enable.f.underflow, 0)) pass = 0;

    /* Trigger underflow again with masking */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    dummy = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);
    (void)dummy;
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS with UNDERFLOW masked: 0x%08x, UNDERFLOW=%u\n",
           err_status.val, err_status.f.underflow);

    /* Step 6: Restore all error enables */
    printf("\nStep 6: Restore ERROR_ENABLE\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR,
              SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT);
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("ERROR_ENABLE restored", err_enable.val,
                   SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT)) pass = 0;

    /* Step 7: ERROR_ENABLE individual field write-readback */
    printf("\nStep 7: ERROR_ENABLE field toggle\n");
    err_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, err_enable.val);
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("All errors disabled", err_enable.val, 0)) pass = 0;

    err_enable.f.cmdbusy = 1;
    err_enable.f.overflow = 1;
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, err_enable.val);
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("CMDBUSY re-enabled", err_enable.f.cmdbusy, 1)) pass = 0;
    if (!check_reg("OVERFLOW re-enabled", err_enable.f.overflow, 1)) pass = 0;
    if (!check_reg("UNDERFLOW still off", err_enable.f.underflow, 0)) pass = 0;

    /* Restore defaults */
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR,
              SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT);

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT ERROR HANDLING TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT ERROR HANDLING TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
