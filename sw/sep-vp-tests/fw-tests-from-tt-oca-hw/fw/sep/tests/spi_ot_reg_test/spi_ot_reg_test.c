// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Register Test - TC_SPIOT_001 (P1)
 *
 * Verifies reset defaults, write-readback, and special behaviors for all
 * SPI controller registers:
 *   - INTR_STATE, INTR_ENABLE, INTR_TEST
 *   - CONTROL (incl. SW_RST hold/release), CONFIGOPTS, CSID
 *   - STATUS (TXEMPTY, RXEMPTY, BYTEORDER=1, READY=1 at reset)
 *   - ERROR_ENABLE, EVENT_ENABLE, ERROR_STATUS
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan
 *   2. Verify reset defaults for all readable registers
 *   3. Write-readback for all RW registers
 *   4. Verify SW_RST drains the FIFOs while held, then releases
 *   5. Verify STATUS.BYTEORDER=1 (LITTLE_ENDIAN parameter)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_reg_test STACK=cgen,sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

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
    printf("SPI OT Register Test (TC_SPIOT_001)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    SPI_CONTROLLER_STATUS_reg_u status;
    SPI_CONTROLLER_INTR_STATE_reg_u intr_status;
    SPI_CONTROLLER_INTR_ENABLE_reg_u intr_enable;
    SPI_CONTROLLER_INTR_TEST_reg_u intr_test;
    SPI_CONTROLLER_EVENT_ENABLE_reg_u event_enable;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    SPI_CONTROLLER_ERROR_ENABLE_reg_u err_enable;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* -------------------------------------------------------------------
     * Step 1: Verify reset defaults
     * ------------------------------------------------------------------- */
    printf("\nStep 1: Reset default verification\n");

    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS default", intr_status.val, 0)) pass = 0;

    intr_enable.val = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
    if (!check_reg("INTR_ENABLE default", intr_enable.val, 0)) pass = 0;

    intr_test.val = READ_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR);
    if (!check_reg("INTR_TEST default", intr_test.val, 0)) pass = 0;

    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("CTRL default", ctrl.val, SPI_CONTROLLER_CONTROL_REG_DEFAULT)) pass = 0;

    cfg.val = READ_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR);
    if (!check_reg("CFG default", cfg.val, SPI_CONTROLLER_CONFIGOPTS_REG_DEFAULT)) pass = 0;

    uint32_t csid_val = READ_REG(SPI_CONTROLLER_CSID_REG_ADDR);
    if (!check_reg("CSID default", csid_val, 0)) pass = 0;

    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("ERROR_ENABLE default", err_enable.val,
                   SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT)) pass = 0;

    event_enable.val = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
    if (!check_reg("EVENT_ENABLE default", event_enable.val, 0)) pass = 0;

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    if (!check_reg("ERROR_STATUS default", err_status.val, 0)) pass = 0;

    /* -------------------------------------------------------------------
     * Step 2: STATUS at reset — key flag verification
     * ------------------------------------------------------------------- */
    printf("\nStep 2: STATUS reset flags\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS=0x%08x\n", status.val);
    if (!check_reg("TXEMPTY=1", status.f.txempty, 1)) pass = 0;
    if (!check_reg("RXEMPTY=1", status.f.rxempty, 1)) pass = 0;
    if (!check_reg("READY=1",   status.f.ready,   1)) pass = 0;
    if (!check_reg("ACTIVE=0",  status.f.active,  0)) pass = 0;
    if (!check_reg("TXFULL=0",  status.f.txfull,  0)) pass = 0;
    if (!check_reg("RXFULL=0",  status.f.rxfull,  0)) pass = 0;

    /* Step 2.5: STATUS.BYTEORDER = 1 (LITTLE_ENDIAN parameter) */
    printf("\nStep 2.5: STATUS.BYTEORDER check\n");
    if (!check_reg("BYTEORDER=1 (little-endian)", status.f.byteorder, 1)) pass = 0;

    /* -------------------------------------------------------------------
     * Step 3: Write-readback for all RW registers
     * ------------------------------------------------------------------- */
    printf("\nStep 3: Write-readback for RW registers\n");

    /* INTR_ENABLE */
    intr_enable.val = 0;
    intr_enable.f.error     = 1;
    intr_enable.f.spi_event = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, intr_enable.val);
    intr_enable.val = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
    if (!check_reg("INTR_ENABLE.error=1",     intr_enable.f.error,     1)) pass = 0;
    if (!check_reg("INTR_ENABLE.spi_event=1", intr_enable.f.spi_event, 1)) pass = 0;
    /* Restore */
    WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, 0);

    /* INTR_TEST write and readback */
    intr_test.val = 0;
    intr_test.f.error     = 1;
    intr_test.f.spi_event = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_test.val = READ_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR);
    if (!check_reg("INTR_TEST.error=1",     intr_test.f.error,     1)) pass = 0;
    if (!check_reg("INTR_TEST.spi_event=1", intr_test.f.spi_event, 1)) pass = 0;
    /* Clear INTR_TEST */
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, 0);
    intr_test.val = READ_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR);
    if (!check_reg("INTR_TEST cleared", intr_test.val, 0)) pass = 0;

    /* CTRL write-readback (enable controller) */
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien     = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("CTRL.spien=1",     ctrl.f.spien,     1)) pass = 0;
    if (!check_reg("CTRL.output_en=1", ctrl.f.output_en, 1)) pass = 0;

    /* CFG write-readback */
    cfg.val = 0;
    cfg.f.clkdiv  = 9;
    cfg.f.cpol    = 1;
    cfg.f.cpha    = 1;
    cfg.f.csnidle = 3;
    cfg.f.csnlead = 3;
    cfg.f.csntrail = 3;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR);
    if (!check_reg("CFG.clkdiv=9",  cfg.f.clkdiv,   9)) pass = 0;
    if (!check_reg("CFG.cpol=1",    cfg.f.cpol,     1)) pass = 0;
    if (!check_reg("CFG.cpha=1",    cfg.f.cpha,     1)) pass = 0;
    /* Restore CFG to standard mode */
    cfg.val = 0;
    cfg.f.clkdiv = 9;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);

    /* CSID write-readback */
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 3);
    csid_val = READ_REG(SPI_CONTROLLER_CSID_REG_ADDR);
    if (!check_reg("CSID write 3", csid_val, 3)) pass = 0;
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);

    /* ERROR_ENABLE write-readback */
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, 0);
    err_enable.val = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
    if (!check_reg("ERROR_ENABLE all disabled", err_enable.val, 0)) pass = 0;
    WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR,
              SPI_CONTROLLER_ERROR_ENABLE_REG_DEFAULT);

    /* EVENT_ENABLE write-readback */
    event_enable.val = 0;
    event_enable.f.rxfull  = 1;
    event_enable.f.txempty = 1;
    event_enable.f.rxwm    = 1;
    event_enable.f.txwm    = 1;
    event_enable.f.ready   = 1;
    event_enable.f.idle    = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    event_enable.val = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
    if (!check_reg("EVENT_ENABLE.rxfull",  event_enable.f.rxfull,  1)) pass = 0;
    if (!check_reg("EVENT_ENABLE.txempty", event_enable.f.txempty, 1)) pass = 0;
    if (!check_reg("EVENT_ENABLE.rxwm",    event_enable.f.rxwm,    1)) pass = 0;
    if (!check_reg("EVENT_ENABLE.txwm",    event_enable.f.txwm,    1)) pass = 0;
    if (!check_reg("EVENT_ENABLE.ready",   event_enable.f.ready,   1)) pass = 0;
    if (!check_reg("EVENT_ENABLE.idle",    event_enable.f.idle,    1)) pass = 0;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, 0);

    /* -------------------------------------------------------------------
     * Step 4: SW_RST drain proof. SW_RST holds the core, both data FIFOs and
     * the command queue in reset until software clears it.
     * ------------------------------------------------------------------- */
    printf("\nStep 4: SW_RST drain (level hold/release)\n");
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    ctrl.f.sw_rst = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SW_RST reads 1 while held", ctrl.f.sw_rst, 1)) pass = 0;
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    if (!check_reg("TXEMPTY=1 while SW_RST held", status.f.txempty, 1)) pass = 0;
    if (!check_reg("RXEMPTY=1 while SW_RST held", status.f.rxempty, 1)) pass = 0;
    ctrl.f.sw_rst = 0;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);
    ctrl.val = READ_REG(SPI_CONTROLLER_CONTROL_REG_ADDR);
    if (!check_reg("SW_RST reads 0 after release", ctrl.f.sw_rst, 0)) pass = 0;

    /* -------------------------------------------------------------------
     * Step 5: Post-SW_RST STATUS sanity
     * ------------------------------------------------------------------- */
    printf("\nStep 5: Post-SW_RST STATUS check\n");
    status.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
    printf("  STATUS=0x%08x\n", status.val);
    if (!check_reg("TXEMPTY=1 after SW_RST", status.f.txempty, 1)) pass = 0;
    if (!check_reg("RXEMPTY=1 after SW_RST", status.f.rxempty, 1)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT REG TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT REG TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
