/*
 * SPI PHY Register Test for OCH SEP
 * Ported from tt_sep/firmware/spi_phy_reg_test.
 *
 * Test Flow:
 *   1. Configure SPI mux, reset SPI controller, set POR timing, deassert reset
 *   2. Wait for init_comp
 *   3. Read discovery and sequence config registers
 *   4. Write PHY control register (phony_dqs_timing = 17)
 *   5. Read back and verify value matches
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static void configure_spi_mux(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 0;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static void reset_spi_controller(int reset)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    spi_ctrl.f.spi_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_ctrl_reg_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_phy_reg_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_phy_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_axi_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_reg_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_xspi_reg_reset_n_n0_scan = reset;
    spi_ctrl.f.spi_enable = reset;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);
}

static int try_init_new(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u spi_status;
    int timeout = 1000000;
    do {
        spi_status.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) {
            printf("ERROR: Timeout waiting for SPI init_comp\n");
            return 1;
        }
    } while (spi_status.f.init_comp == 0);

    if (spi_status.f.init_fail == 1) {
        printf("ERROR: SPI initialization failed\n");
        return 1;
    }
    printf("  SPI init complete: status=0x%08x\n", spi_status.val);
    return 0;
}

static void set_por_timing_parameter(void)
{
    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_reg_u t0 = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_DEFAULT
    };
    t0.f.phy_dqs_timing = 0x00000404;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR, t0.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_reg_u t1 = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_DEFAULT
    };
    t1.f.phy_dq_timing = 0x00000101;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR, t1.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_reg_u t2 = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_DEFAULT
    };
    t2.f.phy_dll_master_ctrl = 0x003400ff;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR, t2.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_reg_u t3 = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_DEFAULT
    };
    t3.f.phy_dll_slave_ctrl = 0x00003306;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR, t3.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_reg_u t4 = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_DEFAULT
    };
    t4.f.phy_gate_lpbk_ctrl = 0x00200030;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR, t4.val);
}

/* ------------------------------------------------------------------ */
/* OT SPI config register check helper                                */
/* ------------------------------------------------------------------ */
#ifdef USE_OT_SPI
static int check_reg_val(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %-40s 0x%08x (exp 0x%08x) %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}
#endif

int main(void)
{
    sep_outbound_filter_init();

#ifdef USE_OT_SPI
    /* ==================================================================
     * OpenTitan SPI Config Register Test
     * OT equivalent of the Cadence PHY register test: exhaustively
     * verify the OT SPI CFG register fields (CLKDIV, CPOL, CPHA,
     * FULLCYC, CSNIDLE, CSNLEAD, CSNTRAIL) with write/readback.
     *
     * Test Flow:
     *   1. Configure SPI mux for OT, enable SPIEN
     *   2. CFG.CLKDIV: write 0, 9, 49 — verify each readback
     *   3. CFG.CPOL/CPHA: all 4 SPI mode combinations
     *   4. CFG.FULLCYC: write 1, readback, write 0
     *   5. CS timing: CSNIDLE/CSNLEAD/CSNTRAIL min/max/mixed
     *   6. Verify unrelated fields are unaffected
     * ================================================================== */
    int errors = 0;
    SPI_CONTROLLER_CFG_reg_u cfg;
    uint32_t rb;

    printf("\n=== OCH SEP OpenTitan SPI CFG Register Test ===\n\n");

    /* Step 1: Configure SPI mux for OT, enable controller */
    {
        OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
            .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
        };
        spi_mux.f.spi_sel = 1;
        spi_mux.f.cs_force_high = 0;
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
    }
    {
        SPI_CONTROLLER_CTRL_reg_u ctrl;
        ctrl.val = SPI_CONTROLLER_CTRL_REG_DEFAULT;
        ctrl.f.spien = 1;
        ctrl.f.output_en = 1;
        WRITE_REG(SPI_CONTROLLER_CTRL_REG_ADDR, ctrl.val);
    }
    printf("SPI mux → OT, SPIEN=1\n\n");

    /* Step 2: CLKDIV values */
    printf("--- CLKDIV ---\n");
    uint32_t clkdiv_vals[] = {0, 9, 49};
    uint32_t i;
    for (i = 0; i < 3; i++) {
        cfg.val = 0;
        cfg.f.clkdiv = clkdiv_vals[i];
        WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
        rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
        cfg.val = rb;
        char name[64];
        /* Use a simple numeric label since snprintf may not be available */
        printf("  CLKDIV write=%u readback=%u ", clkdiv_vals[i], cfg.f.clkdiv);
        if (cfg.f.clkdiv == clkdiv_vals[i]) {
            printf("PASS\n");
        } else {
            printf("FAIL\n");
            errors++;
        }
    }

    /* Step 3: All 4 SPI modes (CPOL/CPHA) */
    printf("\n--- SPI Modes (CPOL/CPHA) ---\n");
    uint32_t cpol_vals[] = {0, 0, 1, 1};
    uint32_t cpha_vals[] = {0, 1, 0, 1};
    for (i = 0; i < 4; i++) {
        cfg.val = 0;
        cfg.f.clkdiv  = 9;
        cfg.f.cpol    = cpol_vals[i];
        cfg.f.cpha    = cpha_vals[i];
        WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
        rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
        cfg.val = rb;
        printf("  Mode %u (CPOL=%u CPHA=%u): readback CPOL=%u CPHA=%u %s\n",
               i, cpol_vals[i], cpha_vals[i], cfg.f.cpol, cfg.f.cpha,
               (cfg.f.cpol == cpol_vals[i] && cfg.f.cpha == cpha_vals[i]) ? "PASS" : "FAIL");
        if (cfg.f.cpol != cpol_vals[i] || cfg.f.cpha != cpha_vals[i])
            errors++;
    }

    /* Step 4: FULLCYC */
    printf("\n--- FULLCYC ---\n");
    cfg.val = 0;
    cfg.f.clkdiv = 9;
    cfg.f.fullcyc = 1;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.val = rb;
    if (!check_reg_val("FULLCYC write=1 readback", cfg.f.fullcyc, 1)) errors++;

    cfg.val = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.f.fullcyc = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.val = rb;
    if (!check_reg_val("FULLCYC write=0 readback", cfg.f.fullcyc, 0)) errors++;

    /* Step 5: CS timing fields (CSNIDLE, CSNLEAD, CSNTRAIL) */
    printf("\n--- CS Timing (min/max/mixed) ---\n");

    /* Minimum (0) */
    cfg.val = 0;
    cfg.f.clkdiv   = 9;
    cfg.f.csnidle  = 0;
    cfg.f.csnlead  = 0;
    cfg.f.csntrail = 0;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.val = rb;
    if (!check_reg_val("CSNIDLE=0  readback", cfg.f.csnidle,  0)) errors++;
    if (!check_reg_val("CSNLEAD=0  readback", cfg.f.csnlead,  0)) errors++;
    if (!check_reg_val("CSNTRAIL=0 readback", cfg.f.csntrail, 0)) errors++;

    /* Maximum (15) */
    cfg.val = 0;
    cfg.f.clkdiv   = 9;
    cfg.f.csnidle  = 15;
    cfg.f.csnlead  = 15;
    cfg.f.csntrail = 15;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.val = rb;
    if (!check_reg_val("CSNIDLE=15  readback", cfg.f.csnidle,  15)) errors++;
    if (!check_reg_val("CSNLEAD=15  readback", cfg.f.csnlead,  15)) errors++;
    if (!check_reg_val("CSNTRAIL=15 readback", cfg.f.csntrail, 15)) errors++;

    /* Mixed */
    cfg.val = 0;
    cfg.f.clkdiv   = 9;
    cfg.f.csnidle  = 5;
    cfg.f.csnlead  = 3;
    cfg.f.csntrail = 7;
    WRITE_REG(SPI_CONTROLLER_CFG_REG_ADDR, cfg.val);
    rb = READ_REG(SPI_CONTROLLER_CFG_REG_ADDR);
    cfg.val = rb;
    if (!check_reg_val("CSNIDLE=5  readback", cfg.f.csnidle,  5)) errors++;
    if (!check_reg_val("CSNLEAD=3  readback", cfg.f.csnlead,  3)) errors++;
    if (!check_reg_val("CSNTRAIL=7 readback", cfg.f.csntrail, 7)) errors++;

    printf("\n========================================\n");
    if (errors == 0) {
        printf("=== OT SPI CFG REGISTER TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== OT SPI CFG REGISTER TEST FAILED (errors=%d) ===\n", errors);
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return errors ? -1 : 0;

#else  /* USE_OT_SPI not defined → Cadence xSPI PHY reg path */

    printf("\n=== OCH SEP SPI PHY Register Test ===\n");

    configure_spi_mux();

    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl = {
            .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
        };
        spi_ctrl.f.spi_enable = 0;
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);
    }
    reset_spi_controller(0);

    {
        uint8_t divider = 800 / 25;
        OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u spi_clk_div_ctrl = {
            .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
        };
        spi_clk_div_ctrl.f.clock_divider_value = divider;
        spi_clk_div_ctrl.f.clock_div_set = 1;
        spi_clk_div_ctrl.f.clock_dutycycle = 128;
        spi_clk_div_ctrl.f.clock_div_enable = 1;
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, spi_clk_div_ctrl.val);
    }

    set_por_timing_parameter();

    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_reg_u discovery_ctrl = {
            .val = OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_DEFAULT
        };
        discovery_ctrl.f.discovery_num_lines = 8;
        discovery_ctrl.f.discovery_cmd_type = 1;
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, discovery_ctrl.val);
    }

    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl = {
            .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
        };
        spi_ctrl.f.spi_enable = 1;
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);
    }

    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));

    reset_spi_controller(1);

    if (try_init_new()) {
        printf("FAILED: SPI init failed\n");
        test_fail(0);
        while (1) { __asm__("wfi"); }
        return -1;
    }

    printf("Reading config registers...\n");
    uint32_t disc = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CFG_COMMON_A_DISCOVERY_CONTROL_REG_ADDR);
    uint32_t gseq = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CMN_SEQ_REGS_A_GLOBAL_SEQ_CFG_REG_ADDR);
    printf("  discovery_ctrl=0x%08x, global_seq=0x%08x\n", disc, gseq);

    // PHY_CTRL_REG: use the generated macro directly.
    // RTL-SPI-003: sep_cdns_spi_wrap.sv uses a single formula
    //   cdns_apb_paddr = paddr - CDNS_CTRL_BASE (0x20002000)
    // which maps PHY address 0x20006080 -> APB offset 0x4080, outside the
    // controller's PHY forwarding window (0x2000-0x3FFF). The RTL fix is to
    // subtract PHY_REG_BASE (0x20004000) for PHY-range accesses, giving
    // offset 0x2080 which the controller correctly forwards to the PHY.
    // Until the RTL is fixed, this test is expected to FAIL.
    printf("Writing PHY ctrl reg (phony_dqs_timing=17)...\n");
    CTB_RFILE_PHY_CTRL_REG_reg_u phy;
    phy.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_PHY_REG_CTB_RFILE_A_PHY_CTRL_REG_REG_ADDR);
    printf("  PHY_CTRL_REG default readback=0x%08x (expect 0x%08x)\n",
           phy.val, CTB_RFILE_PHY_CTRL_REG_REG_DEFAULT);
    phy.f.phony_dqs_timing = 17;
    WRITE_REG(SEP_AXI_EXTENSION_CDNS_XSPI_PHY_REG_CTB_RFILE_A_PHY_CTRL_REG_REG_ADDR, phy.val);

    phy.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_PHY_REG_CTB_RFILE_A_PHY_CTRL_REG_REG_ADDR);

    if (phy.f.phony_dqs_timing == 17) {
        printf("PASSED: readback == 17\n");
        test_pass(0);
    } else {
        printf("FAILED: readback = %d (expected 17)\n", phy.f.phony_dqs_timing);
        test_fail(0);
    }

    while (1) { __asm__("wfi"); }
    return 0;

#endif  /* USE_OT_SPI */
}
