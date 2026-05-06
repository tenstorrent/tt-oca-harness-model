/*
 * SPI Sanity Test for OCH SEP - Cadence xSPI Controller
 *
 * Tests SPI controller initialization at both 25 MHz (low-speed) and
 * 100 MHz (high-speed) with Octal SPI flash in DDR mode.
 *
 * Ported from tt_sep/firmware/spi_sanity.
 *
 * Test Flow:
 *   1. Configure SPI mux (select Cadence, clear cs_force_high)
 *   2. Low-speed init: disable/reset, program 25 MHz clk div, set PHY timing,
 *      set discovery (8-lane DDR), enable, deassert reset, wait init_comp
 *   3. If low-speed passes: high-speed init: reset, program 100 MHz clk div,
 *      set high-speed PHY timing, re-enable, wait init_comp
 *   4. Report PASS if high-speed init succeeds (init_fail=0, init_comp=1)
 *
 * System Clock: 800 MHz (testbench, matching tt_sep)
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
    printf("SPI mux configured: spi_sel=0 (Cadence), cs_force_high=0\n");
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

static void enable_spi_controller(int enable)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    spi_ctrl.f.spi_enable = enable;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);
}

static void program_spi_clk_div(uint32_t target_freq, uint32_t sys_freq)
{
    uint8_t divider = sys_freq / target_freq;
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u spi_clk_div_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
    };
    spi_clk_div_ctrl.f.clock_divider_value = divider;
    spi_clk_div_ctrl.f.clock_div_set = 1;
    spi_clk_div_ctrl.f.clock_dutycycle = 128;
    spi_clk_div_ctrl.f.clock_div_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, spi_clk_div_ctrl.val);
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
        printf("ERROR: SPI initialization failed (init_fail=1)\n");
        return 1;
    }
    printf("  SPI init complete: status=0x%08x\n", spi_status.val);
    return 0;
}

static void set_por_timing_parameter_lowspeed(void)
{
    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_reg_u init_phy_dqs_timing = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_DEFAULT
    };
    init_phy_dqs_timing.f.phy_dqs_timing = 0x00000404;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR, init_phy_dqs_timing.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_reg_u init_phy_dq_timing = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_DEFAULT
    };
    init_phy_dq_timing.f.phy_dq_timing = 0x00000101;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR, init_phy_dq_timing.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_reg_u init_phy_dll_master = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_DEFAULT
    };
    init_phy_dll_master.f.phy_dll_master_ctrl = 0x003400ff;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR, init_phy_dll_master.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_reg_u init_phy_dll_slave = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_DEFAULT
    };
    init_phy_dll_slave.f.phy_dll_slave_ctrl = 0x00003306;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR, init_phy_dll_slave.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_reg_u init_phy_gate_lpbk_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_DEFAULT
    };
    init_phy_gate_lpbk_ctrl.f.phy_gate_lpbk_ctrl = 0x00200030;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR, init_phy_gate_lpbk_ctrl.val);
}

static void set_por_timing_parameter_highspeed(void)
{
    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_reg_u init_phy_dqs_timing = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_DEFAULT
    };
    init_phy_dqs_timing.f.phy_dqs_timing = 0x00000404;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR, init_phy_dqs_timing.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_reg_u init_phy_dq_timing = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_DEFAULT
    };
    init_phy_dq_timing.f.phy_dq_timing = 0x00000101;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR, init_phy_dq_timing.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_reg_u init_phy_dll_master = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_DEFAULT
    };
    init_phy_dll_master.f.phy_dll_master_ctrl = 0x00340058;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR, init_phy_dll_master.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_reg_u init_phy_dll_slave = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_DEFAULT
    };
    init_phy_dll_slave.f.phy_dll_slave_ctrl = 0x00003311;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR, init_phy_dll_slave.val);

    OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_reg_u init_phy_gate_lpbk_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_DEFAULT
    };
    init_phy_gate_lpbk_ctrl.f.phy_gate_lpbk_ctrl = 0x00280030;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR, init_phy_gate_lpbk_ctrl.val);
}

static void set_discovery_param(int num_dataline, int ddr, int full_discovery)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_reg_u discovery_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_DEFAULT
    };
    if (full_discovery == 1) {
        discovery_ctrl.f.discovery_num_lines = 0;
    } else {
        discovery_ctrl.f.discovery_num_lines = num_dataline;
        discovery_ctrl.f.discovery_cmd_type = ddr;
    }
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, discovery_ctrl.val);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("OCH SEP SPI Sanity Test (Low+High Speed)\n");
    printf("========================================\n");

    uint32_t sys_freq = 800;
    int pass = 1;

    configure_spi_mux();

    /* === 25 MHz Low-Speed Init === */
    printf("\n--- Phase 1: Low-speed init (25 MHz) ---\n");
    enable_spi_controller(0);
    reset_spi_controller(0);
    program_spi_clk_div(25, sys_freq);
    set_por_timing_parameter_lowspeed();
    set_discovery_param(8, 1, 0);
    enable_spi_controller(1);

    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));

    reset_spi_controller(1);

    if (try_init_new()) {
        printf("  FAILED: Low-speed init\n");
        pass = 0;
    } else {
        printf("  PASSED: Low-speed init\n");

        /* === 100 MHz High-Speed Init === */
        printf("\n--- Phase 2: High-speed init (100 MHz) ---\n");
        reset_spi_controller(0);
        enable_spi_controller(0);
        set_por_timing_parameter_highspeed();
        program_spi_clk_div(100, sys_freq);
        set_discovery_param(8, 1, 0);
        enable_spi_controller(1);

        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
                  READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
        WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
                  READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));

        reset_spi_controller(1);

        if (try_init_new()) {
            printf("  FAILED: High-speed init\n");
            pass = 0;
        } else {
            printf("  PASSED: High-speed init\n");
        }
    }

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI SANITY TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI SANITY TEST FAILED ===\n");
        test_fail(0);
    }

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
