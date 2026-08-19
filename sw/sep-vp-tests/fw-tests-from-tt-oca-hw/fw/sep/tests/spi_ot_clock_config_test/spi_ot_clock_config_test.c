/*
 * SPI OT Clock Config Test - TC_SPIOT_004 (P1)
 *
 * Verifies SPI clock divider (CLKDIV), polarity (CPOL), phase (CPHA),
 * full-cycle mode (FULLCYC), and CS timing (CSNIDLE, CSNLEAD, CSNTRAIL).
 *
 * Test Flow:
 *   1. Configure SPI mux for OpenTitan
 *   2. Test CLKDIV values: 0, 49, 0xFFFF
 *   3. Test all 4 SPI modes (CPOL/CPHA combinations)
 *   4. Test FULLCYC mode
 *   5. Test CS timing fields (CSNIDLE, CSNLEAD, CSNTRAIL)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_clock_config_test STACK=sim
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
    printf("SPI OT Clock Config Test (TC_SPIOT_004)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_CFG_reg_u cfg;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n\n");

    /* Step 1: Verify CFG default */
    printf("Step 1: CFG default check\n");
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CFG default", cfg.val, SPI_CONTROLLER_CFG_REG_DEFAULT)) pass = 0;

    /* Step 2: Test CLKDIV values */
    printf("\nStep 2: CLKDIV values\n");

    cfg.val = 0;
    cfg.f.clkdiv = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CLKDIV=0 (fastest)", cfg.f.clkdiv, 0)) pass = 0;

    cfg.val = 0;
    cfg.f.clkdiv = 49;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CLKDIV=49 (1MHz@50MHz)", cfg.f.clkdiv, 49)) pass = 0;

    cfg.val = 0;
    cfg.f.clkdiv = 0xFFFF;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CLKDIV=0xFFFF (max)", cfg.f.clkdiv, 0xFFFF)) pass = 0;

    /* Step 3: Test all 4 SPI modes */
    printf("\nStep 3: SPI modes (CPOL/CPHA)\n");
    uint32_t cpol, cpha;
    for (cpol = 0; cpol <= 1; cpol++) {
        for (cpha = 0; cpha <= 1; cpha++) {
            cfg.val = 0;
            cfg.f.cpol = cpol;
            cfg.f.cpha = cpha;
            WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
            cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
            printf("  Mode %u (CPOL=%u, CPHA=%u): readback CPOL=%u, CPHA=%u - %s\n",
                   (cpol << 1) | cpha, cpol, cpha,
                   cfg.f.cpol, cfg.f.cpha,
                   (cfg.f.cpol == cpol && cfg.f.cpha == cpha) ? "PASS" : "FAIL");
            if (cfg.f.cpol != cpol || cfg.f.cpha != cpha) pass = 0;
        }
    }

    /* Step 4: Test FULLCYC */
    printf("\nStep 4: FULLCYC mode\n");
    cfg.val = 0;
    cfg.f.fullcyc = 1;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("FULLCYC=1", cfg.f.fullcyc, 1)) pass = 0;

    cfg.f.fullcyc = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("FULLCYC=0", cfg.f.fullcyc, 0)) pass = 0;

    /* Step 5: Test CS timing fields */
    printf("\nStep 5: CS timing fields\n");
    cfg.val = 0;
    cfg.f.csnidle = 0xF;
    cfg.f.csnlead = 0xF;
    cfg.f.csntrail = 0xF;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CSNIDLE=0xF", cfg.f.csnidle, 0xF)) pass = 0;
    if (!check_reg("CSNLEAD=0xF", cfg.f.csnlead, 0xF)) pass = 0;
    if (!check_reg("CSNTRAIL=0xF", cfg.f.csntrail, 0xF)) pass = 0;

    cfg.val = 0;
    cfg.f.csnidle = 0;
    cfg.f.csnlead = 0;
    cfg.f.csntrail = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CS timing all zero", cfg.val & 0x0FFF0000, 0)) pass = 0;

    /* Step 6: Combined configuration */
    printf("\nStep 6: Combined config (CLKDIV=9, Mode3, FULLCYC, timing)\n");
    cfg.val = 0;
    cfg.f.clkdiv = 9;
    cfg.f.cpol = 1;
    cfg.f.cpha = 1;
    cfg.f.fullcyc = 1;
    cfg.f.csnidle = 4;
    cfg.f.csnlead = 2;
    cfg.f.csntrail = 3;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    if (!check_reg("CLKDIV", cfg.f.clkdiv, 9)) pass = 0;
    if (!check_reg("CPOL", cfg.f.cpol, 1)) pass = 0;
    if (!check_reg("CPHA", cfg.f.cpha, 1)) pass = 0;
    if (!check_reg("FULLCYC", cfg.f.fullcyc, 1)) pass = 0;
    if (!check_reg("CSNIDLE", cfg.f.csnidle, 4)) pass = 0;
    if (!check_reg("CSNLEAD", cfg.f.csnlead, 2)) pass = 0;
    if (!check_reg("CSNTRAIL", cfg.f.csntrail, 3)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT CLOCK CONFIG TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT CLOCK CONFIG TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
