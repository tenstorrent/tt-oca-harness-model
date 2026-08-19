/*
 * SPI OT CS Timing Test - TC_SPIOT_019 (P1)
 *
 * Verifies CS timing parameter configuration and write-readback correctness
 * for the CFG register fields: CSNIDLE, CSNLEAD, CSNTRAIL.
 *
 * CFG register fields (all 4-bit, range 0–15):
 *   CFG.CSNIDLE  [27:24]: CS idle time between back-to-back transfers
 *   CFG.CSNLEAD  [23:20]: CS setup time before first SCLK edge
 *   CFG.CSNTRAIL [19:16]: CS hold time after last SCLK edge
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Write min values (all 0): readback verify
 *   3. Write max values (all 15): readback verify
 *   4. Write mixed values (CSNIDLE=5, CSNLEAD=10, CSNTRAIL=3): readback verify
 *   5. Restore to working values (CSNIDLE=2, CSNLEAD=2, CSNTRAIL=2)
 *   6. Issue a simple TX command to verify SPI still operates correctly
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_cs_timing_test STACK=sim
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

static int check_timing(const char *label,
                        uint32_t csnidle, uint32_t csnlead, uint32_t csntrail,
                        uint32_t exp_csnidle, uint32_t exp_csnlead, uint32_t exp_csntrail)
{
    int ok = 1;
    printf("  %s:\n", label);
    if (csnidle != exp_csnidle) {
        printf("    CSNIDLE:  %u (expected %u) - FAIL\n", csnidle, exp_csnidle);
        ok = 0;
    } else {
        printf("    CSNIDLE:  %u - PASS\n", csnidle);
    }
    if (csnlead != exp_csnlead) {
        printf("    CSNLEAD:  %u (expected %u) - FAIL\n", csnlead, exp_csnlead);
        ok = 0;
    } else {
        printf("    CSNLEAD:  %u - PASS\n", csnlead);
    }
    if (csntrail != exp_csntrail) {
        printf("    CSNTRAIL: %u (expected %u) - FAIL\n", csntrail, exp_csntrail);
        ok = 0;
    } else {
        printf("    CSNTRAIL: %u - PASS\n", csntrail);
    }
    return ok;
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

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT CS Timing Test (TC_SPIOT_019)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CTRL_reg_u ctrl;
    SPI_CONTROLLER_CFG_reg_u cfg;
    SPI_CONTROLLER_CMD_reg_u cmd;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller */
    ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
    ctrl.f.spien = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    WRITE_REG(SPI_CONTROLLER_CSID_REG_ADDR, 0);

    /* ------------------------------------------------------------------ */
    /* Step 1: Min values (CSNIDLE=0, CSNLEAD=0, CSNTRAIL=0)              */
    /* ------------------------------------------------------------------ */
    printf("Step 1: Min CS timing values (all 0)\n");
    cfg.val = 0;
    cfg.f.clkdiv   = spi_clkdiv();
    cfg.f.cpol     = 0;
    cfg.f.cpha     = 0;
    cfg.f.csnidle  = 0;
    cfg.f.csnlead  = 0;
    cfg.f.csntrail = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_timing("Min values", cfg.f.csnidle, cfg.f.csnlead, cfg.f.csntrail,
                      0, 0, 0))
        pass = 0;

    /* ------------------------------------------------------------------ */
    /* Step 2: Max values (CSNIDLE=15, CSNLEAD=15, CSNTRAIL=15)           */
    /* ------------------------------------------------------------------ */
    printf("\nStep 2: Max CS timing values (all 15)\n");
    cfg.val = 0;
    cfg.f.clkdiv   = spi_clkdiv();
    cfg.f.csnidle  = 15;
    cfg.f.csnlead  = 15;
    cfg.f.csntrail = 15;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_timing("Max values", cfg.f.csnidle, cfg.f.csnlead, cfg.f.csntrail,
                      15, 15, 15))
        pass = 0;

    /* ------------------------------------------------------------------ */
    /* Step 3: Mixed values (CSNIDLE=5, CSNLEAD=10, CSNTRAIL=3)           */
    /* ------------------------------------------------------------------ */
    printf("\nStep 3: Mixed CS timing values (CSNIDLE=5, CSNLEAD=10, CSNTRAIL=3)\n");
    cfg.val = 0;
    cfg.f.clkdiv   = spi_clkdiv();
    cfg.f.csnidle  = 5;
    cfg.f.csnlead  = 10;
    cfg.f.csntrail = 3;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_timing("Mixed values", cfg.f.csnidle, cfg.f.csnlead, cfg.f.csntrail,
                      5, 10, 3))
        pass = 0;

    /* ------------------------------------------------------------------ */
    /* Step 4: Restore working values and issue a TX command               */
    /* ------------------------------------------------------------------ */
    printf("\nStep 4: Restore working values (all=2) and issue TX command\n");
    cfg.val = 0;
    cfg.f.clkdiv   = spi_clkdiv();
    cfg.f.cpol     = 0;
    cfg.f.cpha     = 0;
    cfg.f.csnidle  = 2;
    cfg.f.csnlead  = 2;
    cfg.f.csntrail = 2;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_timing("Restored values", cfg.f.csnidle, cfg.f.csnlead, cfg.f.csntrail,
                      2, 2, 2))
        pass = 0;

    /* Issue a simple 1-byte TX command to verify SPI still works */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);
    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    WRITE_REG(SPI_CONTROLLER_TXDATA_REG_ADDR, 0x9F000000);
    cmd.val         = 0;
    cmd.f.len       = 0;    /* 1 byte */
    cmd.f.csaat     = 0;
    cmd.f.speed     = 0;    /* Standard */
    cmd.f.direction = 2;    /* TX */
    WRITE_REG(SPI_CONTROLLER_CMD_REG_ADDR, cmd.val);

    if (wait_for_ready(TIMEOUT_LIMIT)) { pass = 0; goto done; }

    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    printf("  ERROR_STATUS=0x%08x (CMDINVAL=%u CSIDINVAL=%u)\n",
           err_status.val, err_status.f.cmdinval, err_status.f.csidinval);
    if (err_status.f.cmdinval || err_status.f.csidinval) {
        printf("  FAIL: CMD error after restoring CS timing\n");
        pass = 0;
    } else {
        printf("  PASS: TX command accepted after CS timing restore\n");
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT CS TIMING TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT CS TIMING TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
