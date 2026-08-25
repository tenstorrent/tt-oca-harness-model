// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Cadence xSPI Flash SRAM Loopback Test - TC_SPI_CDN_010 (P0)
 *
 * Verifies a CPU-driven flash model loopback path:
 *
 *   Phase 1: CPU seeds a source block in SPI flash through the XIP window.
 *   Phase 2: CPU reads the flash block into SEP scratch SRAM.
 *   Phase 3: CPU writes the SRAM block back out to a different SPI flash block.
 *   Phase 4: CPU reads the destination flash block into another SRAM buffer and compares.
 *
 * This intentionally does not use Secure DMA. The goal is to prove that SEP CPU
 * load/store traffic can move block data through flash -> SRAM -> flash using
 * the flash model as the loopback storage.
 *
 * Addressing note:
 *   CPU 0xF000_xxxx is remapped to physical XIP 0x3000_xxxx by the SEP local
 *   alias window. Do not use CPU 0xD000_xxxx here; that aliases to SRAM.
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
// #include "sep_outbound_filter.h"  /* not modeled in VP */

#define CPU_XIP_BASE       0xF0000000U
#define SRC_FLASH_OFFSET   0x00004000U
#define DST_FLASH_OFFSET   0x00008000U
#define SRC_FLASH_ADDR     (CPU_XIP_BASE + SRC_FLASH_OFFSET)
#define DST_FLASH_ADDR     (CPU_XIP_BASE + DST_FLASH_OFFSET)

#define LOCAL_ALIAS_BASE   0xC0000000ULL
#define LOCAL_ALIAS_SIZE   0x40000000U
#define SEP_LOCAL_BASE_REG SEP_CPU_CTRL_SEP_LOCAL_BASE_ADDR_REG_ADDR
#define SEP_REGION_SIZE_REG SEP_CPU_CTRL_SEP_REGION_SIZE_REG_ADDR

#define LOOPBACK_WORDS     6U
#define LOOPBACK_BYTES     (LOOPBACK_WORDS * 4U)
#define PATTERN_MARKER     0xE1000000U
#define SRAM_BUF_A_BASE    (SEP_SRAM_MEM_BASE_ADDR + 0x9000U)
#define SRAM_BUF_B_BASE    (SEP_SRAM_MEM_BASE_ADDR + 0x9100U)

static void mmio_fence(void)
{
    __asm__ volatile ("fence iorw, iorw" ::: "memory");
}

static uint32_t loopback_pattern(uint32_t idx)
{
    return PATTERN_MARKER | ((idx & 0xFFU) << 8) | ((~idx) & 0xFFU);
}

static void configure_cpu_xip_alias(void)
{
    WRITE_REG64(SEP_LOCAL_BASE_REG, LOCAL_ALIAS_BASE);
    WRITE_REG(SEP_REGION_SIZE_REG, LOCAL_ALIAS_SIZE);
}

static void configure_spi_mux(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u m = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    m.f.spi_sel       = 0;
    m.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, m.val);
}

static void reset_spi_controller(int deassert)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u c = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT
    };
    c.f.spi_reset_n_n0_scan          = deassert;
    c.f.spi_ctrl_reg_reset_n_n0_scan = deassert;
    c.f.spi_phy_reg_reset_n_n0_scan  = deassert;
    c.f.spi_phy_reset_n_n0_scan      = deassert;
    c.f.spi_axi_reset_n_n0_scan      = deassert;
    c.f.spi_reg_reset_n_n0_scan      = deassert;
    c.f.spi_xspi_reg_reset_n_n0_scan = deassert;
    c.f.spi_enable                   = deassert;
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

static void program_spi_clk_div(uint32_t target_mhz, uint32_t sys_mhz)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u k = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT
    };
    k.f.clock_divider_value = (uint8_t)(sys_mhz / target_mhz);
    k.f.clock_dutycycle     = 128;
    k.f.clock_div_enable    = 1;
    k.f.clock_div_set       = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, k.val);
}

static void set_discovery_8lane_ddr(void)
{
    OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_reg_u d = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_DEFAULT
    };
    d.f.discovery_num_lines = 8;
    d.f.discovery_cmd_type  = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, d.val);
}

static int wait_init_comp(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
    int timeout = 1000000;

    do {
        s.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) {
            printf("  ERROR: Timeout waiting for init_comp\n");
            return -1;
        }
    } while (s.f.init_comp == 0);

    if (s.f.init_fail == 1) {
        printf("  ERROR: init_fail=1 (CTRL_STATUS=0x%08x)\n", s.val);
        return -1;
    }

    printf("  xSPI init OK: CTRL_STATUS=0x%08x (init_fail=%u)\n", s.val, s.f.init_fail);
    return 0;
}

static int init_spi_controller(void)
{
    configure_spi_mux();
    enable_spi_controller(0);
    reset_spi_controller(0);
    program_spi_clk_div(25, 800);
    set_discovery_8lane_ddr();
    enable_spi_controller(1);

    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));

    reset_spi_controller(1);
    return wait_init_comp();
}

static void write_flash_block(uint32_t flash_addr, volatile uint32_t *src)
{
    volatile uint32_t *flash = (volatile uint32_t *)flash_addr;

    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        flash[i] = src[i];
        mmio_fence();
    }
}

static void read_flash_block(uint32_t flash_addr, volatile uint32_t *dst)
{
    volatile uint32_t *flash = (volatile uint32_t *)flash_addr;

    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        dst[i] = flash[i];
        mmio_fence();
    }
}

static int compare_block(const char *label, volatile uint32_t *expected, volatile uint32_t *actual)
{
    int mismatches = 0;

    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        if (actual[i] != expected[i]) {
            printf("  %s word[%u] FAIL: exp=0x%08x got=0x%08x\n",
                   label, i, expected[i], actual[i]);
            mismatches++;
        } else {
            printf("  %s word[%u] OK: 0x%08x\n", label, i, actual[i]);
        }
    }

    return mismatches;
}

int main(void)
{
    // sep_outbound_filter_init();  /* not modeled in VP */

    printf("\n");
    printf("========================================\n");
    printf("  TC_SPI_CDN_010: xSPI Flash SRAM Loopback Test\n");
    printf("  CPU path: flash -> SRAM -> flash (%u bytes)\n", LOOPBACK_BYTES);
    printf("========================================\n\n");

    int errors = 0;
    volatile uint32_t *sram_a = (volatile uint32_t *)SRAM_BUF_A_BASE;
    volatile uint32_t *sram_b = (volatile uint32_t *)SRAM_BUF_B_BASE;

    printf("=== Step 1: Configure CPU XIP alias ===\n");
    configure_cpu_xip_alias();
    printf("  CPU XIP alias 0xF000_0000 -> physical XIP 0x3000_0000\n");

    printf("\n=== Step 2: Init Cadence xSPI ===\n");
    if (init_spi_controller() != 0) {
        printf("  CHK[1] FAIL: xSPI init failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }
    printf("  CHK[1] PASS: init_comp=1\n");

    printf("\n=== Step 3: Seed source flash block ===\n");
    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        sram_a[i] = loopback_pattern(i);
        sram_b[i] = 0xDEADBEEFU;
    }
    printf("  src flash @ 0x%08x, dst flash @ 0x%08x\n", SRC_FLASH_ADDR, DST_FLASH_ADDR);
    printf("  SRAM_A @ 0x%08x, SRAM_B @ 0x%08x\n", SRAM_BUF_A_BASE, SRAM_BUF_B_BASE);
    write_flash_block(SRC_FLASH_ADDR, sram_a);
    printf("  CHK[2] PASS: source flash seeded from SRAM_A\n");

    printf("\n=== Step 4: CPU read flash -> SRAM_A ===\n");
    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        sram_a[i] = 0xAAAAAAAAU;
    }
    read_flash_block(SRC_FLASH_ADDR, sram_a);
    for (uint32_t i = 0; i < LOOPBACK_WORDS; i++) {
        uint32_t expected = loopback_pattern(i);
        if (sram_a[i] != expected) {
            printf("  CHK[%u] FAIL: SRAM_A[%u] exp=0x%08x got=0x%08x\n",
                   i + 3, i, expected, sram_a[i]);
            errors++;
        }
    }
    if (errors == 0) {
        printf("  CHK[3..%u] PASS: source flash block copied into SRAM_A\n",
               LOOPBACK_WORDS + 2);
    }

    printf("\n=== Step 5: CPU write SRAM_A -> destination flash ===\n");
    write_flash_block(DST_FLASH_ADDR, sram_a);
    printf("  CHK[%u] PASS: SRAM_A written to destination flash\n", LOOPBACK_WORDS + 3);

    printf("\n=== Step 6: CPU read destination flash -> SRAM_B ===\n");
    read_flash_block(DST_FLASH_ADDR, sram_b);
    int mismatches = compare_block("loopback", sram_a, sram_b);
    if (mismatches != 0) {
        errors += mismatches;
        printf("  CHK[%u] FAIL: %d destination mismatch(es)\n",
               LOOPBACK_WORDS + 4, mismatches);
    } else {
        printf("  CHK[%u] PASS: SRAM_B matches SRAM_A after flash loopback\n",
               LOOPBACK_WORDS + 4);
    }

    printf("\n========================================\n");
    if (errors == 0) {
        printf("=== XSPI FLASH SRAM LOOPBACK TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== XSPI FLASH SRAM LOOPBACK TEST FAILED (errors=%d) ===\n", errors);
        test_fail(1);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return errors ? -1 : 0;
}
