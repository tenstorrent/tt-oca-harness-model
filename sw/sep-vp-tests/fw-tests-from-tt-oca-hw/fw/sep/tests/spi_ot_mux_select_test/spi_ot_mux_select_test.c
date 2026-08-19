/*
 * SPI OT Mux Select Test - TC_SPIOT_002 (P0)
 *
 * Verifies SPI mux controller selection between Cadence xSPI and OpenTitan
 * SPI Host, including cs_force_high behavior and status bit readback.
 *
 * Test Flow:
 *   1. Read SPI_MUX_CTRL default (spi_sel=0, cs_force_high=1)
 *   2. Switch to OpenTitan (spi_sel=1), verify readback
 *   3. Clear cs_force_high, verify readback
 *   4. Switch back to Cadence (spi_sel=0), verify readback
 *   5. Verify status bits (ot_busy, cdns_busy) are readable
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_mux_select_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

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
    printf("SPI OT Mux Select Test (TC_SPIOT_002)\n");
    printf("========================================\n\n");

    int pass = 1;
    uint32_t read_val;
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux;

    /* Step 1: Read default values */
    printf("Step 1: Read SPI_MUX_CTRL default\n");
    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel default", spi_mux.f.spi_sel, 0)) pass = 0;
    if (!check_reg("cs_force_high default", spi_mux.f.cs_force_high, 1)) pass = 0;

    /* Step 2: Select OpenTitan (spi_sel=1) */
    printf("\nStep 2: Select OpenTitan SPI Host\n");
    spi_mux.val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT;
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 1;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel after OT select", spi_mux.f.spi_sel, 1)) pass = 0;
    if (!check_reg("cs_force_high still set", spi_mux.f.cs_force_high, 1)) pass = 0;

    /* Step 3: Clear cs_force_high */
    printf("\nStep 3: Clear cs_force_high\n");
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel still OT", spi_mux.f.spi_sel, 1)) pass = 0;
    if (!check_reg("cs_force_high cleared", spi_mux.f.cs_force_high, 0)) pass = 0;

    /* Step 4: Switch back to Cadence */
    printf("\nStep 4: Switch back to Cadence\n");
    spi_mux.f.spi_sel = 0;
    spi_mux.f.cs_force_high = 1;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);

    read_val = READ_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR);
    spi_mux.val = read_val;
    if (!check_reg("spi_sel back to Cadence", spi_mux.f.spi_sel, 0)) pass = 0;

    /* Step 5: Read status bits */
    printf("\nStep 5: Read status bits\n");
    printf("  ot_busy=%u, cdns_busy=%u (read-only status)\n",
           spi_mux.f.ot_busy, spi_mux.f.cdns_busy);
    printf("  ot_irq=%u, cdns_irq=%u (read-only status)\n",
           spi_mux.f.ot_irq, spi_mux.f.cdns_irq);

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT MUX SELECT TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT MUX SELECT TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
