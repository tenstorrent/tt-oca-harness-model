/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * Port of tt-oca-hw fw/smc/tests/smu_sep_ext_axi (SEP_SMU_016).  Catch-all
 * address 0xA0001000 misses both xbar apertures and exits
 * smu_axi_xbar.ext_out through the local AOU (RTL: smu_axi_out -> AoU).
 *
 * Firmware activates AOU (peer auto-acks), write/readbacks the remote SMN
 * stub behind aou_peer_.axi_m, then doorbells SEP so it can run the
 * smn_outbound -> ext_out -> AOU leg.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define AOU_INIT                0x08u
#define AOU_INIT_ACTIVATE_START (1u << 0)
#define AOU_INIT_STATE_ENABLED  (1u << 2)

/* Hardware uses 0x80001000 (smu_sep_ext_axi).  On the VP that address sits
 * inside cluster fast-mem [0x8000_0000, 0x9000_0000), so the store would
 * never leave the ISS.  0xA0001000 is in the MMIO window, misses both SMU
 * apertures, and is the VP catch-all equivalent. */
#define EXTAXI_SMC_OUT_ADDR     0xA0001000ULL
#define EXTAXI_SMC_OUT_DATA0    0x5A5AA5A5u
#define EXTAXI_SMC_OUT_DATA1    0xC001CAFEu

#define SMC_SPM_BASE            0xC0060000ULL
#define SEP_DONE_OFF            0x3000u
#define DONE_TOKEN              0xA0A00001u

#define REG32(addr)             (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU AOU ext-axi test (SMC side) ===\n");

    REG_WRITE(SMC_AOU_BASE + AOU_INIT, AOU_INIT_ACTIVATE_START);
    const uint32_t init = REG_READ(SMC_AOU_BASE + AOU_INIT);
    if ((init & AOU_INIT_STATE_ENABLED) == 0) {
        printf("FAIL: AOU not ENABLED after activate (INIT=0x%x)\n", init);
        return 1;
    }
    printf("AOU ENABLED (INIT=0x%x)\n", init);

    REG32(EXTAXI_SMC_OUT_ADDR) = EXTAXI_SMC_OUT_DATA0;
    const uint32_t g0 = REG32(EXTAXI_SMC_OUT_ADDR);
    if (g0 != EXTAXI_SMC_OUT_DATA0) {
        printf("FAIL: ext_out beat0 readback 0x%08x want 0x%08x\n",
               g0, EXTAXI_SMC_OUT_DATA0);
        return 1;
    }
    REG32(EXTAXI_SMC_OUT_ADDR + 4) = EXTAXI_SMC_OUT_DATA1;
    const uint32_t g1 = REG32(EXTAXI_SMC_OUT_ADDR + 4);
    if (g1 != EXTAXI_SMC_OUT_DATA1) {
        printf("FAIL: ext_out beat1 readback 0x%08x want 0x%08x\n",
               g1, EXTAXI_SMC_OUT_DATA1);
        return 1;
    }
    printf("SMC->ext_out via AOU: 0x%08x 0x%08x\n", g0, g1);

    REG32(SMC_SPM_BASE + SEP_DONE_OFF) = DONE_TOKEN;
    printf("PASS: SMU AOU ext-axi test (SMC side)\n");
    return 0;
}
