/*
 * Cadence xSPI Flash Discovery Info Test - TC_SPI_CDN_006 (P1)
 *
 * xSPI equivalent of spi_ot_flash_jedec_id_test (TC_SPIOT_019).
 *
 * After Cadence xSPI initialization (init_comp=1), reads the flash discovery
 * configuration registers to verify the controller successfully identified
 * the flash device via SFDP. The Cadence controller performs automatic SFDP
 * discovery during init and stores the results in sequence config registers.
 *
 * Registers checked:
 *   CTRL_STATUS       (0x20002100): init_comp=1, init_fail=0
 *   DISCOVERY_CONTROL (0x20002260): discovery params (expect 0x8900 for 8-lane)
 *   GLOBAL_SEQ_CFG    (0x20002390): global sequence config (0x8F = Octal DDR)
 *   GLOBAL_SEQ_CFG_1  (0x20002394): extended global sequence config
 *   READ_SEQ_CFG_0    (0x20002430): read sequence config
 *
 * Pass criteria:
 *   [1] init_comp=1 (SFDP discovery completed)
 *   [2] init_fail=0 (no initialization failure)
 *   [3] DISCOVERY_CONTROL != 0 (discovery control was set, not default)
 *   [4] GLOBAL_SEQ_CFG != 0 (flash protocol detected and configured)
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

/* Cadence xSPI sequence config register addresses */
#define CDNS_DISCOVERY_CONTROL_ADDR  \
    SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CFG_COMMON_A_DISCOVERY_CONTROL_REG_ADDR
#define CDNS_GLOBAL_SEQ_CFG_ADDR     \
    SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CMN_SEQ_REGS_A_GLOBAL_SEQ_CFG_REG_ADDR
#define CDNS_GLOBAL_SEQ_CFG_1_ADDR   \
    SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CMN_SEQ_REGS_A_GLOBAL_SEQ_CFG_1_REG_ADDR
#define CDNS_READ_SEQ_CFG_0_ADDR     \
    SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_DEV_SEQ_REGS_A_READ_SEQ_CFG_0_REG_ADDR
#define CDNS_PROG_SEQ_CFG_0_ADDR     \
    SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_DEV_SEQ_REGS_A_PROG_SEQ_CFG_0_REG_ADDR

static void configure_spi_mux(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u m = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    m.f.spi_sel = 0;
    m.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, m.val);
}

static void reset_spi_controller(int reset)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u c = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    c.f.spi_reset_n_n0_scan = reset;
    c.f.spi_ctrl_reg_reset_n_n0_scan = reset;
    c.f.spi_phy_reg_reset_n_n0_scan = reset;
    c.f.spi_phy_reset_n_n0_scan = reset;
    c.f.spi_axi_reset_n_n0_scan = reset;
    c.f.spi_reg_reset_n_n0_scan = reset;
    c.f.spi_xspi_reg_reset_n_n0_scan = reset;
    c.f.spi_enable = reset;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, c.val);
}

static void enable_spi_controller(int enable)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u c = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    c.f.spi_enable = enable;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, c.val);
}

static void program_spi_clk_div(uint32_t target, uint32_t sys)
{
    uint8_t div = sys / target;
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u k = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
    };
    k.f.clock_divider_value = div;
    k.f.clock_div_set = 1;
    k.f.clock_dutycycle = 128;
    k.f.clock_div_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, k.val);
}

static void set_discovery_param(int lines, int ddr, int full)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_reg_u d = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_DEFAULT
    };
    if (full) { d.f.discovery_num_lines = 0; }
    else { d.f.discovery_num_lines = lines; d.f.discovery_cmd_type = ddr; }
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, d.val);
}

static int cdns_init(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
    int timeout = 1000000;
    configure_spi_mux();
    enable_spi_controller(0);
    reset_spi_controller(0);
    program_spi_clk_div(25, 800);
    set_discovery_param(8, 1, 0);
    enable_spi_controller(1);
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    reset_spi_controller(1);
    do {
        s.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) { printf("  ERROR: Timeout waiting for init_comp\n"); return 1; }
    } while (s.f.init_comp == 0);
    if (s.f.init_fail == 1) { printf("  ERROR: init_fail=1\n"); return 1; }
    printf("  Cadence xSPI init OK: CTRL_STATUS=0x%08x\n", s.val);
    return 0;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("Cadence xSPI Flash Discovery Info Test (TC_SPI_CDN_006)\n");
    printf("xSPI equivalent of spi_ot_flash_jedec_id_test\n");
    printf("========================================\n\n");

    int pass = 1;

    /* Step 1: Initialize Cadence xSPI controller */
    printf("Step 1: Initialize Cadence xSPI controller\n");
    if (cdns_init()) {
        pass = 0;
        goto done;
    }

    /* Step 2: Read flash discovery result registers
     * These registers are populated by the Cadence controller after SFDP
     * discovery. Their values confirm what flash protocol was detected.
     * Equivalent to reading JEDEC ID in OT: confirms flash was identified. */
    printf("\nStep 2: Read SFDP discovery result registers\n");
    {
        uint32_t disc_ctrl   = READ_REG(CDNS_DISCOVERY_CONTROL_ADDR);
        uint32_t global_seq  = READ_REG(CDNS_GLOBAL_SEQ_CFG_ADDR);
        uint32_t global_seq1 = READ_REG(CDNS_GLOBAL_SEQ_CFG_1_ADDR);
        uint32_t read_seq0   = READ_REG(CDNS_READ_SEQ_CFG_0_ADDR);
        uint32_t prog_seq0   = READ_REG(CDNS_PROG_SEQ_CFG_0_ADDR);

        printf("  DISCOVERY_CONTROL (0x%08x) = 0x%08x\n",
               CDNS_DISCOVERY_CONTROL_ADDR, disc_ctrl);
        printf("  GLOBAL_SEQ_CFG    (0x%08x) = 0x%08x",
               CDNS_GLOBAL_SEQ_CFG_ADDR, global_seq);
        if ((global_seq & 0xFF) == 0x8F)
            printf(" [Octal DDR mode detected]");
        printf("\n");
        printf("  GLOBAL_SEQ_CFG_1  (0x%08x) = 0x%08x\n",
               CDNS_GLOBAL_SEQ_CFG_1_ADDR, global_seq1);
        printf("  READ_SEQ_CFG_0    (0x%08x) = 0x%08x\n",
               CDNS_READ_SEQ_CFG_0_ADDR, read_seq0);
        printf("  PROG_SEQ_CFG_0    (0x%08x) = 0x%08x\n",
               CDNS_PROG_SEQ_CFG_0_ADDR, prog_seq0);

        /* Check [3]: DISCOVERY_CONTROL non-zero */
        if (disc_ctrl == 0) {
            printf("  FAIL [3]: DISCOVERY_CONTROL=0 (no discovery params set)\n");
            pass = 0;
        } else {
            printf("  PASS [3]: DISCOVERY_CONTROL non-zero\n");
        }

        /* Check [4]: GLOBAL_SEQ_CFG non-zero */
        if (global_seq == 0) {
            printf("  FAIL [4]: GLOBAL_SEQ_CFG=0 (flash protocol not detected)\n");
            pass = 0;
        } else {
            printf("  PASS [4]: GLOBAL_SEQ_CFG non-zero (flash protocol detected)\n");
        }
    }

done:
    printf("\n========================================\n");
    if (pass) {
        printf("=== CDNS FLASH JEDEC ID TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== CDNS FLASH JEDEC ID TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
