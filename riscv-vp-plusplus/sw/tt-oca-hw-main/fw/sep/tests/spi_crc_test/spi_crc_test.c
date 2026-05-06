/*
 * TC_SPI_CDN_004 - Cadence xSPI CRC Accumulation Test
 *
 * STATUS: BLOCKED by RTL-SPI-005 (sep_ip_integration.sv XIP demux bug).
 *         Phase A (register R/W) runs and passes. Phase C reads return
 *         non-zero SRAM data but CRC stays 0. Phase D fails with CRC = 0.
 *
 * Confirmed root cause (2026-03-31): TWO compounding issues prevent CRC
 * accumulation:
 *
 *   Issue 1 — Wrong alias address (FW, not RTL):
 *     CPU alias 0xD000_xxxx → axi_local_alias_remap → 0x1000_xxxx.
 *     sep_local_axi_xbar: 0x1000_xxxx is in SRAM range [0x1000_0000, 0x1004_0000)
 *     → routes to SRAM (idx=1), NOT to axi_extension (idx=7 at [0x2000_0000, 0x4000_0000)).
 *     Confirmed by local_alias_sanity.c: 0xD000_0000 is the SRAM alias base.
 *     To reach XIP the CPU must use 0xF000_xxxx → remap → 0x3000_xxxx → axi_extension.
 *
 *   Issue 2 — RTL-SPI-005: sep_ip_integration.sv XIP demux unreachable:
 *     The AW/AR select for axi_extension (hw/sep/sep_ip_integration.sv) uses:
 *       if (addr >= SPI_CTRL_BASE && addr < XIP_BASE + XIP_SIZE)  // [0x2001_1000, 0x4000_0000)
 *           → AXI_EXT_SLAVE_CDNS_SPI_CTRL   ← covers entire axi_extension range
 *       else
 *           → AXI_EXT_SLAVE_XIP             ← UNREACHABLE from crossbar
 *     XIP_BASE+XIP_SIZE = 0x3000_0000+0x1000_0000 = 0x4000_0000 equals the
 *     axi_extension upper bound, so all addresses route to CDNS_SPI_CTRL register
 *     block. spi_data_axi_req_i never receives a request; spi_rvalid_from_cdns
 *     never fires; CRC accumulates nothing.
 *     Required RTL fix: change upper bound to XIP_BASE (0x3000_0000).
 *
 *   Combined result: Phase B writes succeed to SRAM. Phase C reads return
 *     correct non-zero SRAM data. CRC stays 0 because spi_data_axi_req_i is idle.
 *     Phase D fails: "HW CRC is 0 (BLOCKED RTL-SPI-005)".
 *
 *   History: v1 used XIP physical 0x3004_0000 (uninitialized flash = all-zero,
 *     CRC(0) vacuously matches). v2/v3 used alias, incorrectly believed 0xD000_xxxx
 *     routed to XIP — confirmed wrong: 0x1000_xxxx is in the SRAM xbar window.
 *
 * CRC hardware (sep_cdns_spi_wrap.sv:971):
 *   nxt_spi_crc = crc(spi_crc, spi_rdata_from_cdns)  // 64-bit polynomial
 *   Triggered when: spi_rvalid_from_cdns && r_ready && spi_enable && crc_enable
 *   Polynomial: x^64 + x^4 + x^3 + x + 1 (right-shift, little-endian, seed=0)
 *
 * Flash sector layout:
 *   Sector C (alias 0xD000_C000 → SRAM 0x1000_C000; XIP post-fix: 0xF000_C000):
 *   distinct from CDN_003 (0xD000_0000), CDN_005 (0xD000_4000), CDN_008 (0xD000_8000).
 *
 * Test phases (current state — blocked by RTL-SPI-005):
 *   Phase A  SPI init + SPI_CRC_CTRL register R/W verification   (PASSES)
 *   Phase B  Write 4 known words via alias 0xD000_C000 → SRAM    (PASSES)
 *   Phase C  Enable CRC, read alias+0 and alias+8 → SRAM reads   (non-zero data)
 *   Phase D  FAIL: HW CRC == 0 (spi_data_axi_req_i never fired)
 *   Phase E  crc_clear: CRC stays 0 (trivially passes)
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

/* Flash alias base — routes to SRAM, NOT XIP (see Issue 1 in file header).
 * CPU 0xD000_xxxx → remap → 0x1000_xxxx → SRAM ([0x1000_0000, 0x1004_0000)).
 * Kept as-is; XIP accumulation blocked by RTL-SPI-005 until demux fix merged. */
#define SPI_ALIAS_BASE   0xD0000000U
/* Sector C offset — distinct from CDN_003/005/008 sectors */
#define FLASH_OFFSET     0x0000C000U
#define FLASH_ALIAS_ADDR (SPI_ALIAS_BASE + FLASH_OFFSET)

/* Known non-zero pattern written to flash for CRC verification.
 * Two 64-bit double-words: dw0 = {word1, word0}, dw1 = {word3, word2} */
#define CRC_WORD0   0xC0C00001U
#define CRC_WORD1   0xC0C00002U
#define CRC_WORD2   0xC0C00003U
#define CRC_WORD3   0xC0C00004U

/* ------------------------------------------------------------------ */
/* SPI controller init (identical to spi_write_read_test)             */
/* ------------------------------------------------------------------ */

static void configure_spi_mux(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u m = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    m.f.spi_sel      = 0;
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

static int try_init_new(void)
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

static void set_discovery_param_8lane_ddr(void)
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
    set_discovery_param_8lane_ddr();
    enable_spi_controller(1);
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR,
              READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));
    reset_spi_controller(1);
    return try_init_new();
}

/* ------------------------------------------------------------------ */
/* SW CRC implementation — mirrors sep_cdns_spi_wrap.sv:988-1057      */
/* Polynomial: x^64 + x^4 + x^3 + x + 1 (0xD800000000000000)        */
/* Width: 64 bits, right-shift, little-endian, input: 64-bit word     */
/* ------------------------------------------------------------------ */

/* Extract bit i from 64-bit value as int (0 or 1) */
#define CBIT(x, i) ((int)(((x) >> (i)) & 1ULL))

static uint64_t xspi_crc_step(uint64_t crc_in, uint64_t data) {
    uint64_t result = 0;
    int i;

    /* Bits 0..55: crc[i] = crcIn[i]^crcIn[i+1]^crcIn[i+3]^crcIn[i+4]
     *                     ^ data[i]^data[i+1]^data[i+3]^data[i+4]   */
    for (i = 0; i <= 55; i++) {
        int bit = CBIT(crc_in, i)   ^ CBIT(crc_in, i+1) ^
                  CBIT(crc_in, i+3) ^ CBIT(crc_in, i+4) ^
                  CBIT(data,   i)   ^ CBIT(data,   i+1) ^
                  CBIT(data,   i+3) ^ CBIT(data,   i+4);
        result |= ((uint64_t)(bit & 1)) << i;
    }

    /* Bits 56..63: feedback wraps through low bits of crcIn/data
     * (transcribed verbatim from RTL) */
    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,56)^CBIT(crc_in,57)^CBIT(crc_in,59)^CBIT(crc_in,60)^
        CBIT(data,0)^CBIT(data,56)^CBIT(data,57)^CBIT(data,59)^CBIT(data,60)) & 1) << 56;

    result |= ((uint64_t)(
        CBIT(crc_in,1)^CBIT(crc_in,57)^CBIT(crc_in,58)^CBIT(crc_in,60)^CBIT(crc_in,61)^
        CBIT(data,1)^CBIT(data,57)^CBIT(data,58)^CBIT(data,60)^CBIT(data,61)) & 1) << 57;

    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,2)^CBIT(crc_in,58)^CBIT(crc_in,59)^
        CBIT(crc_in,61)^CBIT(crc_in,62)^
        CBIT(data,0)^CBIT(data,2)^CBIT(data,58)^CBIT(data,59)^
        CBIT(data,61)^CBIT(data,62)) & 1) << 58;

    result |= ((uint64_t)(
        CBIT(crc_in,1)^CBIT(crc_in,3)^CBIT(crc_in,59)^CBIT(crc_in,60)^
        CBIT(crc_in,62)^CBIT(crc_in,63)^
        CBIT(data,1)^CBIT(data,3)^CBIT(data,59)^CBIT(data,60)^
        CBIT(data,62)^CBIT(data,63)) & 1) << 59;

    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,1)^CBIT(crc_in,2)^CBIT(crc_in,3)^
        CBIT(crc_in,60)^CBIT(crc_in,61)^CBIT(crc_in,63)^
        CBIT(data,0)^CBIT(data,1)^CBIT(data,2)^CBIT(data,3)^
        CBIT(data,60)^CBIT(data,61)^CBIT(data,63)) & 1) << 60;

    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,2)^CBIT(crc_in,61)^CBIT(crc_in,62)^
        CBIT(data,0)^CBIT(data,2)^CBIT(data,61)^CBIT(data,62)) & 1) << 61;

    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,1)^CBIT(crc_in,3)^CBIT(crc_in,62)^CBIT(crc_in,63)^
        CBIT(data,0)^CBIT(data,1)^CBIT(data,3)^CBIT(data,62)^CBIT(data,63)) & 1) << 62;

    result |= ((uint64_t)(
        CBIT(crc_in,0)^CBIT(crc_in,2)^CBIT(crc_in,3)^CBIT(crc_in,63)^
        CBIT(data,0)^CBIT(data,2)^CBIT(data,3)^CBIT(data,63)) & 1) << 63;

    return result;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    sep_outbound_filter_init();

#ifdef USE_OT_SPI
    /* ==================================================================
     * OpenTitan SPI Control Register Test
     * OT equivalent: verify OT SPI control registers (INTR_ENABLE,
     * ERROR_ENABLE, EVENT_ENABLE) with write/readback. Also reads the
     * shared SPI_MUX_CTRL CRC_LOW/HIGH registers.
     * ================================================================== */
    int errors = 0;

    printf("\n");
    printf("========================================\n");
    printf("  OT SPI Control Register Test\n");
    printf("========================================\n\n");

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

    printf("=== Step 2: INTR_ENABLE R/W ===\n");
    {
        uint32_t rb;
        rb = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
        printf("  INTR_ENABLE default: 0x%08x\n", rb);
        WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, 0x11);
        rb = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
        printf("  write=0x11 readback=0x%08x %s\n", rb, (rb == 0x11) ? "PASS" : "FAIL");
        if (rb != 0x11) errors++;
        WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, 0x0);
        rb = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
        printf("  write=0x0 readback=0x%08x %s\n", rb, (rb == 0x0) ? "PASS" : "FAIL");
        if (rb != 0x0) errors++;
    }

    printf("\n=== Step 3: ERROR_ENABLE R/W ===\n");
    {
        uint32_t rb;
        rb = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
        printf("  default: 0x%08x (expect 0x11111)\n", rb);
        WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, 0x11111);
        rb = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
        printf("  write=0x11111 readback=0x%08x %s\n", rb, (rb == 0x11111) ? "PASS" : "FAIL");
        if (rb != 0x11111) errors++;
        WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, 0x0);
        rb = READ_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR);
        printf("  write=0x0 readback=0x%08x %s\n", rb, (rb == 0x0) ? "PASS" : "FAIL");
        if (rb != 0x0) errors++;
        WRITE_REG(SPI_CONTROLLER_ERROR_ENABLE_REG_ADDR, 0x11111);
    }

    printf("\n=== Step 4: EVENT_ENABLE R/W ===\n");
    {
        uint32_t rb;
        uint32_t all_events = (1u<<0)|(1u<<4)|(1u<<8)|(1u<<12)|(1u<<16)|(1u<<20);
        rb = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
        printf("  default: 0x%08x\n", rb);
        WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, all_events);
        rb = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
        printf("  write=0x%08x readback=0x%08x %s\n",
               all_events, rb, (rb == all_events) ? "PASS" : "FAIL");
        if (rb != all_events) errors++;
        WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, 0x0);
        rb = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
        printf("  write=0x0 readback=0x%08x %s\n", rb, (rb == 0x0) ? "PASS" : "FAIL");
        if (rb != 0x0) errors++;
    }

    printf("\n=== Step 5: Mux CRC registers (informational) ===\n");
    {
        uint32_t mux_lo = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_CRC_LOW_REG_ADDR);
        uint32_t mux_hi = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_CRC_HIGH_REG_ADDR);
        printf("  mux CRC_LOW=0x%08x CRC_HIGH=0x%08x\n", mux_lo, mux_hi);
    }

    printf("\n========================================\n");
    if (errors == 0) {
        printf("=== OT SPI CTRL REG TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== OT SPI CTRL REG TEST FAILED (errors=%d) ===\n", errors);
        test_fail(1);
    }
    printf("========================================\n");
    while (1) { __asm__("wfi"); }

#else  /* USE_OT_SPI not defined → Cadence xSPI CRC path */

    printf("\n");
    printf("========================================\n");
    printf("  TC_SPI_CDN_004: Cadence xSPI CRC Test\n");
    printf("========================================\n\n");

    int errors = 0;

    /* ================================================================
     * Phase A: SPI controller init
     * ================================================================ */
    printf("=== Phase A: Init SPI controller ===\n");
    if (init_spi_controller() != 0) {
        printf("FAIL: SPI controller init failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* ================================================================
     * Phase A (cont): SPI_CRC_CTRL register R/W verification
     * ================================================================ */
    printf("\n=== Phase A: SPI_CRC_CTRL default ===\n");
    OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_reg_u crc_ctrl;
    crc_ctrl.val = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR);
    printf("  default readback: 0x%08x\n", crc_ctrl.val);
    if (crc_ctrl.f.crc_enable != 0 || crc_ctrl.f.crc_clear != 0) {
        printf("  FAIL: expected 0, got crc_enable=%u crc_clear=%u\n",
               crc_ctrl.f.crc_enable, crc_ctrl.f.crc_clear);
        errors++;
    } else {
        printf("  PASS: SPI_CRC_CTRL default = 0\n");
    }

    printf("\n=== Phase A: crc_enable=1 write/readback ===\n");
    crc_ctrl.val          = 0;
    crc_ctrl.f.crc_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_reg_u rb;
        rb.val = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR);
        printf("  readback: 0x%08x (crc_enable=%u)\n", rb.val, rb.f.crc_enable);
        if (rb.f.crc_enable != 1) { printf("  FAIL\n"); errors++; }
        else printf("  PASS\n");
    }

    printf("\n=== Phase A: crc_clear pulse ===\n");
    crc_ctrl.val         = 0;
    crc_ctrl.f.crc_enable = 1;
    crc_ctrl.f.crc_clear  = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    crc_ctrl.f.crc_clear  = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_reg_u rb;
        rb.val = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR);
        if (rb.f.crc_clear != 0) {
            printf("  FAIL: crc_clear still set (rb=0x%08x)\n", rb.val);
            errors++;
        } else if (rb.f.crc_enable != 1) {
            printf("  FAIL: crc_enable cleared unexpectedly\n");
            errors++;
        } else {
            printf("  PASS: crc_clear deasserted, crc_enable preserved\n");
        }
    }

    printf("\n=== Phase A: crc_enable=0 write/readback ===\n");
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, 0);
    {
        OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_reg_u rb;
        rb.val = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR);
        if (rb.f.crc_enable != 0) {
            printf("  FAIL: crc_enable readback=%u\n", rb.f.crc_enable);
            errors++;
        } else {
            printf("  PASS: crc_enable=0\n");
        }
    }

    /* ================================================================
     * Phase B: Write known non-zero pattern to flash sector C.
     * Writes 4 words at FLASH_ADDR via the Cadence alias window.
     * The Cadence xSPI controller handles write-enable + page-program
     * automatically. These words form two 64-bit double-words:
     *   dw0 (alias+0): r.data = {CRC_WORD1, CRC_WORD0}
     *   dw1 (alias+8): r.data = {CRC_WORD3, CRC_WORD2}
     * ================================================================ */
    printf("\n=== Phase B: Write known pattern to flash sector C ===\n");
    printf("  Flash alias addr: 0x%08x\n", FLASH_ALIAS_ADDR);
    WRITE_REG(FLASH_ALIAS_ADDR + 0x0U, CRC_WORD0);
    WRITE_REG(FLASH_ALIAS_ADDR + 0x4U, CRC_WORD1);
    WRITE_REG(FLASH_ALIAS_ADDR + 0x8U, CRC_WORD2);
    WRITE_REG(FLASH_ALIAS_ADDR + 0xCU, CRC_WORD3);
    printf("  word[0]=0x%08x word[1]=0x%08x\n", CRC_WORD0, CRC_WORD1);
    printf("  word[2]=0x%08x word[3]=0x%08x\n", CRC_WORD2, CRC_WORD3);

    /* Read back via alias to verify write succeeded */
    {
        uint32_t rb0 = READ_REG(FLASH_ALIAS_ADDR + 0x0U);
        uint32_t rb1 = READ_REG(FLASH_ALIAS_ADDR + 0x4U);
        uint32_t rb2 = READ_REG(FLASH_ALIAS_ADDR + 0x8U);
        uint32_t rb3 = READ_REG(FLASH_ALIAS_ADDR + 0xCU);
        printf("  readback: 0x%08x 0x%08x 0x%08x 0x%08x\n", rb0, rb1, rb2, rb3);
        if (rb0 != CRC_WORD0 || rb1 != CRC_WORD1 ||
            rb2 != CRC_WORD2 || rb3 != CRC_WORD3) {
            printf("  FAIL: flash write verification failed — CRC test would be unreliable\n");
            errors++;
        } else {
            printf("  PASS: flash write verified\n");
        }
    }

    /* ================================================================
     * Phase C: Enable CRC, read alias+0 and alias+8.
     * Each READ_REG at an 8-byte-aligned alias address generates one
     * 64-bit AXI read on the XIP path; spi_rvalid_from_cdns fires and
     * the CRC accumulates the full 64-bit r.data.
     *   read at alias+0: r.data = {CRC_WORD1, CRC_WORD0} = dw0
     *   read at alias+8: r.data = {CRC_WORD3, CRC_WORD2} = dw1
     * ================================================================ */
    printf("\n=== Phase C: Enable CRC, read 2 DWs via alias ===\n");

    crc_ctrl.val          = 0;
    crc_ctrl.f.crc_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    printf("  CRC enabled\n");

    /* These reads go to SRAM (0xD000_C000 → remap → 0x1000_C000 → SRAM), not XIP.
     * spi_data_axi_req_i stays idle; spi_rvalid_from_cdns never asserts; CRC = 0.
     * Blocked by RTL-SPI-005 — see file header Issue 2. */
    uint32_t rd0 = READ_REG(FLASH_ALIAS_ADDR + 0x0U);   /* dw0 lo-word, expect CRC_WORD0 */
    uint32_t rd1 = READ_REG(FLASH_ALIAS_ADDR + 0x8U);   /* dw1 lo-word, expect CRC_WORD2 */
    printf("  Phase C reads: 0x%08x @ +0, 0x%08x @ +8 (confirm data != 0)\n", rd0, rd1);

    uint32_t crc_lo_c = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_LOW_REG_ADDR);
    uint32_t crc_hi_c = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_HIGH_REG_ADDR);
    printf("  HW CRC: 0x%08x_%08x\n", crc_hi_c, crc_lo_c);

    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, 0);

    /* ================================================================
     * Phase D: SW CRC reference and comparison.
     * dw0 = {CRC_WORD1, CRC_WORD0}, dw1 = {CRC_WORD3, CRC_WORD2}
     * SW computes xspi_crc_step(xspi_crc_step(0, dw0), dw1).
     * ================================================================ */
    printf("\n=== Phase D: SW CRC verification ===\n");

    uint64_t dw0 = ((uint64_t)CRC_WORD1 << 32) | (uint64_t)CRC_WORD0;
    uint64_t dw1 = ((uint64_t)CRC_WORD3 << 32) | (uint64_t)CRC_WORD2;
    uint64_t sw_crc = xspi_crc_step(xspi_crc_step(0ULL, dw0), dw1);
    uint32_t sw_crc_lo = (uint32_t)(sw_crc & 0xFFFFFFFFU);
    uint32_t sw_crc_hi = (uint32_t)(sw_crc >> 32);

    printf("  dw0=0x%08x_%08x\n", CRC_WORD1, CRC_WORD0);
    printf("  dw1=0x%08x_%08x\n", CRC_WORD3, CRC_WORD2);
    printf("  SW CRC: 0x%08x_%08x\n", sw_crc_hi, sw_crc_lo);
    printf("  HW CRC: 0x%08x_%08x\n", crc_hi_c, crc_lo_c);

    if (crc_lo_c == 0 && crc_hi_c == 0) {
        printf("  FAIL: HW CRC is 0 (BLOCKED RTL-SPI-005: spi_data_axi_req_i unreachable, reads hit SRAM not XIP)\n");
        errors++;
    } else if (sw_crc_lo == crc_lo_c && sw_crc_hi == crc_hi_c) {
        printf("  PASS: HW CRC matches SW reference\n");
    } else {
        printf("  FAIL: CRC mismatch — polynomial or data mismatch\n");
        errors++;
    }

    /* ================================================================
     * Phase E: crc_clear — verify CRC resets to 0.
     * ================================================================ */
    printf("\n=== Phase E: crc_clear resets CRC to 0 ===\n");

    crc_ctrl.val          = 0;
    crc_ctrl.f.crc_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    (void)READ_REG(FLASH_ALIAS_ADDR + 0x0U);   /* load CRC with non-zero value */

    crc_ctrl.f.crc_clear = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    crc_ctrl.f.crc_clear = 0;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);

    {
        uint32_t lo = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_LOW_REG_ADDR);
        uint32_t hi = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_HIGH_REG_ADDR);
        if (lo == 0 && hi == 0) {
            printf("  PASS: CRC = 0 after crc_clear\n");
        } else {
            printf("  FAIL: CRC = 0x%08x_%08x after crc_clear (expected 0)\n", hi, lo);
            errors++;
        }
    }
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, 0);

    /* ================================================================
     * Phase F: Determinism — same reads must produce same CRC.
     * ================================================================ */
    printf("\n=== Phase F: Determinism (same reads → same CRC) ===\n");

    crc_ctrl.val          = 0;
    crc_ctrl.f.crc_enable = 1;
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, crc_ctrl.val);
    (void)READ_REG(FLASH_ALIAS_ADDR + 0x0U);
    (void)READ_REG(FLASH_ALIAS_ADDR + 0x8U);

    uint32_t crc_lo_f = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_LOW_REG_ADDR);
    uint32_t crc_hi_f = READ_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_HIGH_REG_ADDR);
    printf("  Phase C CRC: 0x%08x_%08x\n", crc_hi_c, crc_lo_c);
    printf("  Phase F CRC: 0x%08x_%08x\n", crc_hi_f, crc_lo_f);

    if (crc_lo_f == crc_lo_c && crc_hi_f == crc_hi_c) {
        printf("  PASS: CRC is deterministic\n");
    } else {
        printf("  FAIL: CRC not deterministic\n");
        errors++;
    }
    WRITE_REG(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CRC_CTRL_REG_ADDR, 0);

    /* ================================================================
     * Result
     * ================================================================ */
    printf("\n========================================\n");
    if (errors == 0) {
        printf("  RESULT: TEST PASSED\n");
        test_pass(0);
    } else {
        printf("  RESULT: TEST FAILED (errors=%d)\n", errors);
        test_fail(1);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
#endif  /* USE_OT_SPI */
}
