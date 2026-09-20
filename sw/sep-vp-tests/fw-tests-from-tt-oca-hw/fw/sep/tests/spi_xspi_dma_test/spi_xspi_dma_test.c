// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_SPI_CDN_005 - Cadence xSPI DMA Write/Read Test
 *
 * Verifies that the SEP Secure DMA can transfer data through the Cadence
 * xSPI AXI alias window in both directions:
 *
 *   Phase 1 — DMA TX: SRAM → xSPI alias (0xD000_0000)
 *     The DMA writes a test pattern from SRAM to the xSPI alias window,
 *     which causes the Cadence controller to program the flash.
 *
 *   Phase 2 — DMA RX: xSPI alias (0xD000_0000) → SRAM
 *     The DMA reads back from the same alias window address into a separate
 *     SRAM destination buffer.
 *
 *   Phase 3 — Compare
 *     CPU compares src SRAM buffer vs dst SRAM buffer word-by-word.
 *
 * DMA configuration (unlike OT SPI DMA there is no FIFO watermark trigger):
 *   - ADDR_SPACE_ID = 0x77  (both OtInternal — only functional bus in SEP DMA)
 *   - TRANSFER_WIDTH = 0x2  (32-bit / 4 bytes)
 *   - SRC_CONFIG = 0x1 (increment), DST_CONFIG = 0x1 (increment)
 *   - CHUNK_DATA_SIZE = TOTAL_DATA_SIZE (one-shot, no hardware handshake)
 *   - HANDSHAKE_INTR_ENABLE = 0  (no lsio_trigger; memory-mapped access)
 *   - CONTROL: hardware_handshake_enable=0, initial_transfer=1, go=1
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_xspi_dma_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

/* Physical XIP address — offset 0x4000 is the verified-working flash sector.
 * ROPEN-009: DMA bypasses CPU alias remap; 0xD000_0000 has no crossbar slave.
 * Use physical XIP addr (SEP_AXI_EXTENSION_XIP_REGION_MEM_BASE_ADDR = 0x3000_0000).
 * Offset 0x4000 matches CDN-007 / TC_SPI_004 proven sector. */
#define SPI_PHYS_BASE   (SEP_AXI_EXTENSION_XIP_REGION_MEM_BASE_ADDR + 0x4000U)
#define DMA_SIZE        24U   /* bytes — 6 words; DMA XIP write limit = 24 bytes */
#define DMA_TIMEOUT     500000

/* SRAM buffers — two non-overlapping 32-byte windows */
#define SRAM_SRC_BASE  (SEP_SRAM_MEM_BASE_ADDR + 0x4000U)
#define SRAM_DST_BASE  (SEP_SRAM_MEM_BASE_ADDR + 0x5000U)

/* ------------------------------------------------------------------ */
/* Cadence xSPI controller init (identical to spi_write_read_test)    */
/* ------------------------------------------------------------------ */

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

static int wait_init_comp(void)
{
    CTRL_CMD_STAT_CTRL_STATUS_reg_u s;
    int timeout = 1000000;
    do {
        s.val = READ_REG(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
        if (--timeout <= 0) { printf("ERROR: Timeout waiting for init_comp\n"); return -1; }
    } while (s.f.init_comp == 0);
    printf("  SPI init OK: status=0x%08x\n", s.val);
    if (s.f.init_fail == 1) { printf("ERROR: init_fail=1\n"); return -1; }
    return 0;
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

static int init_spi_controller(void)
{
    configure_spi_mux();
    enable_spi_controller(0);
    reset_spi_controller(0);
    program_spi_clk_div(25, 800);
    set_discovery_8lane_ddr();
    enable_spi_controller(1);
    /* Dummy RMW writes — required before reset deassert for alias-window access */
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    reset_spi_controller(1);
    return wait_init_comp();
}

/* ------------------------------------------------------------------ */
/* Secure DMA helpers                                                  */
/* ------------------------------------------------------------------ */

static void dma_setup_range(void)
{
    /* Allow DMA to access full 32-bit address space via OtInternal bus */
    __asm__ volatile ("csrw 0x7c0, %0" : : "r" (0x8));
    WRITE_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_BASE_REG_ADDR,  0x0U);
    WRITE_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_LIMIT_REG_ADDR, 0xFFFFFFFFU);
    WRITE_REG(SECURE_DMA_RANGE_VALID_REG_ADDR, 0x1U);
}

static void dma_configure(uint32_t src, uint32_t dst, uint32_t size)
{
    WRITE_REG(SECURE_DMA_SRC_ADDR_LO_REG_ADDR, src);
    WRITE_REG(SECURE_DMA_SRC_ADDR_HI_REG_ADDR, 0x0U);
    WRITE_REG(SECURE_DMA_DST_ADDR_LO_REG_ADDR, dst);
    WRITE_REG(SECURE_DMA_DST_ADDR_HI_REG_ADDR, 0x0U);

    WRITE_REG(SECURE_DMA_ADDR_SPACE_ID_REG_ADDR, 0x77U);   /* both OtInternal */
    WRITE_REG(SECURE_DMA_TRANSFER_WIDTH_REG_ADDR, 0x2U);   /* 32-bit words */

    /* Both src and dst increment — memory-to-memory style, no wrap */
    WRITE_REG(SECURE_DMA_SRC_CONFIG_REG_ADDR, 0x1U);
    WRITE_REG(SECURE_DMA_DST_CONFIG_REG_ADDR, 0x1U);

    /* One-shot transfer: chunk == total, no hardware handshake trigger */
    WRITE_REG(SECURE_DMA_TOTAL_DATA_SIZE_REG_ADDR,  size);
    WRITE_REG(SECURE_DMA_CHUNK_DATA_SIZE_REG_ADDR,  size);

    /* No hardware handshake (xSPI has no lsio_trigger, unlike OT SPI) */
    WRITE_REG(SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_ADDR, 0x0U);
}

static int dma_start_and_wait(const char *label)
{
    SECURE_DMA_CONTROL_reg_u ctrl;
    ctrl.val                      = 0;
    ctrl.f.opcode                 = 0;
    ctrl.f.hardware_handshake_enable = 0;
    ctrl.f.initial_transfer       = 1;
    ctrl.f.go                     = 1;

    printf("  [%s] Starting DMA CONTROL=0x%08x\n", label, ctrl.val);
    WRITE_REG(SECURE_DMA_CONTROL_REG_ADDR, ctrl.val);

    SECURE_DMA_STATUS_reg_u st;
    int timeout = DMA_TIMEOUT;
    while (timeout-- > 0) {
        st.val = READ_REG(SECURE_DMA_STATUS_REG_ADDR);
        if (st.f.done) {
            printf("  [%s] DMA done (STATUS=0x%08x)\n", label, st.val);
            return 0;
        }
        if (st.f.error) {
            uint32_t ec = READ_REG(SECURE_DMA_ERROR_CODE_REG_ADDR);
            printf("  [%s] DMA ERROR STATUS=0x%08x EC=0x%08x\n", label, st.val, ec);
            return -1;
        }
    }
    printf("  [%s] DMA TIMEOUT STATUS=0x%08x\n", label, st.val);
    return -2;
}

/* ------------------------------------------------------------------ */
/* OT SPI DMA helpers (only compiled with -DUSE_OT_SPI)               */
/* ------------------------------------------------------------------ */
#ifdef USE_OT_SPI

#define OT_TX_WM       4
#define OT_DMA_CHUNK   16   /* = OT_TX_WM * sizeof(uint32_t) */
#define OT_SPI_CLKDIV  9

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static void init_spi_ot_controller(void)
{
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    ctrl.val = 0;
    ctrl.f.rx_watermark = 1;
    ctrl.f.tx_watermark = OT_TX_WM;
    ctrl.f.spien        = 1;
    ctrl.f.output_en    = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    SPI_CONTROLLER_CONFIGOPTS_reg_u cfg;
    cfg.val = 0;
    cfg.f.clkdiv = OT_SPI_CLKDIV;
    WRITE_REG(SPI_CONTROLLER_CONFIGOPTS_REG_ADDR, cfg.val);

    SPI_CONTROLLER_EVENT_ENABLE_reg_u event_en;
    event_en.val = 0;
    event_en.f.txwm = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_en.val);
}

static void dma_configure_ot_tx(uint32_t src_addr)
{
    dma_setup_range();  /* shared helper — sets DMA address range */
    WRITE_REG(SECURE_DMA_SRC_ADDR_LO_REG_ADDR, src_addr);
    WRITE_REG(SECURE_DMA_SRC_ADDR_HI_REG_ADDR, 0x0U);
    WRITE_REG(SECURE_DMA_DST_ADDR_LO_REG_ADDR, SPI_CONTROLLER_TXDATA_REG_ADDR);
    WRITE_REG(SECURE_DMA_DST_ADDR_HI_REG_ADDR, 0x0U);

    WRITE_REG(SECURE_DMA_ADDR_SPACE_ID_REG_ADDR,   0x77U);  /* both OtInternal */
    WRITE_REG(SECURE_DMA_TRANSFER_WIDTH_REG_ADDR,  0x2U);   /* 32-bit words */

    /* SRC: increment (walk SRAM), DST: wrap (fixed TXDATA register) */
    WRITE_REG(SECURE_DMA_SRC_CONFIG_REG_ADDR, 0x1U);
    WRITE_REG(SECURE_DMA_DST_CONFIG_REG_ADDR, 0x2U);

    WRITE_REG(SECURE_DMA_TOTAL_DATA_SIZE_REG_ADDR, DMA_SIZE);
    WRITE_REG(SECURE_DMA_CHUNK_DATA_SIZE_REG_ADDR, OT_DMA_CHUNK);

    /* Hardware handshake via SPI lsio_trigger (tx_wm level signal, bit 0) */
    WRITE_REG(SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_ADDR, 0x1U);
}

static int dma_start_ot_and_wait(void)
{
    SECURE_DMA_CONTROL_reg_u ctrl;
    ctrl.val                          = 0;
    ctrl.f.opcode                     = 0;
    ctrl.f.hardware_handshake_enable  = 1;
    ctrl.f.initial_transfer           = 1;
    ctrl.f.go                         = 1;

    printf("  [OT TX] Starting DMA CONTROL=0x%08x\n", ctrl.val);
    WRITE_REG(SECURE_DMA_CONTROL_REG_ADDR, ctrl.val);

    SECURE_DMA_STATUS_reg_u st;
    int timeout = DMA_TIMEOUT;
    while (timeout-- > 0) {
        st.val = READ_REG(SECURE_DMA_STATUS_REG_ADDR);
        if (st.f.done) {
            printf("  [OT TX] DMA done (STATUS=0x%08x)\n", st.val);
            return 0;
        }
        if (st.f.error) {
            uint32_t ec = READ_REG(SECURE_DMA_ERROR_CODE_REG_ADDR);
            printf("  [OT TX] DMA ERROR STATUS=0x%08x EC=0x%08x\n", st.val, ec);
            return -1;
        }
    }
    printf("  [OT TX] DMA TIMEOUT STATUS=0x%08x\n", st.val);
    return -2;
}

#endif  /* USE_OT_SPI */

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    // sep_outbound_filter_init();

#ifdef USE_OT_SPI
    /* ==================================================================
     * OT SPI DMA TX Test
     * OT equivalent of the Cadence xSPI DMA test: DMA TX from SRAM to
     * the OT SPI TXDATA FIFO via TX-watermark hardware handshake.
     * ================================================================== */
    int errors = 0;
    uint32_t i;

    printf("\n");
    printf("========================================\n");
    printf("  OT SPI DMA TX Test\n");
    printf("========================================\n\n");

    /* Step 1: Init OT SPI controller */
    printf("=== Step 1: Init OT SPI controller ===\n");
    // configure_spi_mux_ot();
    init_spi_ot_controller();
    printf("  OT SPI ready: CLKDIV=%d TX_WM=%d\n", OT_SPI_CLKDIV, OT_TX_WM);

    /* Step 2: Fill SRAM source buffer */
    printf("=== Step 2: Prepare SRAM source buffer ===\n");
    volatile uint32_t *src_ptr = (volatile uint32_t *)SRAM_SRC_BASE;
    uint32_t num_words = DMA_SIZE / 4U;
    for (i = 0; i < num_words; i++) {
        src_ptr[i] = 0xB5000000U | (i & 0xFFU);
    }
    printf("  src @ 0x%08x (%u words)\n", SRAM_SRC_BASE, num_words);

    /* Step 3: Issue SPI TX command */
    printf("=== Step 3: Issue SPI TX command ===\n");
    {
        SPI_CONTROLLER_COMMAND_reg_u cmd;
        cmd.val         = 0;
        cmd.f.len       = DMA_SIZE - 1;
        cmd.f.direction = 2;    /* TX only */
        cmd.f.speed     = 0;    /* Standard single SPI */
        cmd.f.csaat     = 0;
        WRITE_REG(SPI_CONTROLLER_COMMAND_REG_ADDR, cmd.val);
        printf("  CMD: LEN=%u direction=TX speed=Standard\n", DMA_SIZE - 1);
    }

    /* Step 4: DMA SRAM → TXDATA with HW handshake */
    printf("=== Step 4: DMA TX (SRAM 0x%08x -> TXDATA, %u bytes) ===\n",
           SRAM_SRC_BASE, DMA_SIZE);
    dma_configure_ot_tx(SRAM_SRC_BASE);
    if (dma_start_ot_and_wait() != 0) {
        printf("FAIL: OT DMA TX did not complete\n");
        errors++;
    } else {
        printf("PASS: OT DMA TX completed\n");
    }

    /* Step 5: Verify SPI status — no errors */
    printf("=== Step 5: Check SPI status ===\n");
    {
        SPI_CONTROLLER_STATUS_reg_u st;
        st.val = READ_REG(SPI_CONTROLLER_STATUS_REG_ADDR);
        printf("  SPI STATUS=0x%08x TXQD=%u ACTIVE=%u TXEMPTY=%u\n",
               st.val, st.f.txqd, st.f.active, st.f.txempty);

        SPI_CONTROLLER_ERROR_STATUS_reg_u err;
        err.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
        if (err.val != 0) {
            printf("FAIL: SPI ERROR_STATUS=0x%08x\n", err.val);
            errors++;
        } else {
            printf("PASS: No SPI errors\n");
        }
    }

    /* Cleanup */
    WRITE_REG(SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_ADDR,
              SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_DEFAULT);
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, 0);

    printf("\n========================================\n");
    if (errors == 0) {
        printf("=== OT SPI DMA TX TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== OT SPI DMA TX TEST FAILED (errors=%d) ===\n", errors);
        test_fail(1);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }

#else  /* Cadence xSPI DMA path */

    printf("\n");
    printf("========================================\n");
    printf("  TC_SPI_CDN_005: Cadence xSPI DMA Test\n");
    printf("========================================\n\n");

    int errors = 0;
    uint32_t i;

    /* Step 1: Initialize Cadence xSPI controller */
    printf("=== Step 1: Init Cadence xSPI ===\n");
    if (init_spi_controller() != 0) {
        printf("FAIL: xSPI controller init failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* Step 2: Fill SRAM source buffer with test pattern */
    printf("=== Step 2: Prepare SRAM source buffer ===\n");
    volatile uint32_t *src_ptr = (volatile uint32_t *)SRAM_SRC_BASE;
    volatile uint32_t *dst_ptr = (volatile uint32_t *)SRAM_DST_BASE;
    uint32_t num_words = DMA_SIZE / 4U;

    for (i = 0; i < num_words; i++) {
        src_ptr[i] = 0xA5000000U | (i & 0xFFU);
    }
    for (i = 0; i < num_words; i++) {
        dst_ptr[i] = 0xDEADBEEFU;
    }
    printf("  src @ 0x%08x, dst @ 0x%08x (%u words)\n",
           SRAM_SRC_BASE, SRAM_DST_BASE, num_words);

    /* Step 3: DMA TX — SRAM → xSPI physical window */
    printf("=== Step 3: DMA TX (SRAM 0x%08x -> xSPI phys 0x%08x, %u bytes) ===\n",
           SRAM_SRC_BASE, SPI_PHYS_BASE, DMA_SIZE);

    dma_setup_range();
    dma_configure(SRAM_SRC_BASE, SPI_PHYS_BASE, DMA_SIZE);
    if (dma_start_and_wait("TX") != 0) {
        printf("FAIL: DMA TX did not complete\n");
        errors++;
    } else {
        printf("PASS: DMA TX completed\n");
    }

    /* Step 4: DMA RX — xSPI physical window → SRAM dst buffer */
    printf("=== Step 4: DMA RX (xSPI phys 0x%08x -> SRAM 0x%08x, %u bytes) ===\n",
           SPI_PHYS_BASE, SRAM_DST_BASE, DMA_SIZE);

    dma_configure(SPI_PHYS_BASE, SRAM_DST_BASE, DMA_SIZE);
    if (dma_start_and_wait("RX") != 0) {
        printf("FAIL: DMA RX did not complete\n");
        errors++;
    } else {
        printf("PASS: DMA RX completed\n");
    }

    /* Step 5: Compare src SRAM vs dst SRAM */
    printf("=== Step 5: Compare SRAM src vs dst ===\n");
    int mismatch = 0;
    for (i = 0; i < num_words; i++) {
        if (src_ptr[i] != dst_ptr[i]) {
            printf("  MISMATCH word[%u]: wrote=0x%08x read=0x%08x\n",
                   i, src_ptr[i], dst_ptr[i]);
            mismatch++;
            errors++;
        }
    }
    if (mismatch == 0) {
        printf("  PASS: all %u words match\n", num_words);
    }

    /* Cleanup */
    WRITE_REG(SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_ADDR,
              SECURE_DMA_HANDSHAKE_INTR_ENABLE_REG_DEFAULT);

    printf("\n========================================\n");
    if (errors == 0) {
        printf("=== TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== TEST FAILED (errors=%d) ===\n", errors);
        test_fail(1);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
#endif  /* USE_OT_SPI */
}
