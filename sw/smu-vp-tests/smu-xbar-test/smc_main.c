/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * Port of tt-oca-hw fw/smc/tests/smc_sep_xbar + smu_bidirect (SEP_SMU_003).
 *
 * VP-adapted: CLA / fuse-sense / outbound-filter programming is RTL DV
 * machinery and is omitted.  The handshake tokens and the two AXI paths
 * match the hardware protocol header.
 *
 *   SEP -> SMC  dedicated sep_ext_to_smc_axi  (scratchpad mailbox)
 *   SMC -> SEP  SMU xbar catch of the SEP global aperture (SEP SRAM)
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define SMC_SPM_BASE        0xC0060000ULL
#define SEP_DATA_OFF        0x2000u
#define SEP_ACK_OFF         0x2004u

#define SEP_SRAM_SHARED     0x60009000ULL   /* sep_global 0x50000000 + local 0x10009000 */

#define XBAR_DATA_PATTERN       0x13579BDFu
#define XBAR_SMC_TO_SEP_CMD     0xC001CAFEu
#define XBAR_SMC_TO_SEP_DONE    0xD0E0F00Du
#define XBAR_SEP_TO_SMC_ACK     0x5E9ACCE5u

#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU xbar test (SMC side) ===\n");

    uint32_t spins = 0;
    while (REG32(SMC_SPM_BASE + SEP_DATA_OFF) != XBAR_DATA_PATTERN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SEP data pattern\n");
            return 1;
        }
    }
    printf("SEP->SMC dedicated path: pattern 0x%08x\n", XBAR_DATA_PATTERN);

    REG32(SEP_SRAM_SHARED) = XBAR_SMC_TO_SEP_CMD;
    printf("SMC->SEP xbar: CMD 0x%08x\n", XBAR_SMC_TO_SEP_CMD);

    spins = 0;
    while (REG32(SMC_SPM_BASE + SEP_ACK_OFF) != XBAR_SEP_TO_SMC_ACK) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SEP ACK\n");
            return 1;
        }
    }
    printf("SEP ACK received\n");

    REG32(SEP_SRAM_SHARED) = XBAR_SMC_TO_SEP_DONE;
    printf("SMC->SEP xbar: DONE 0x%08x\n", XBAR_SMC_TO_SEP_DONE);

    printf("PASS: SMU xbar test (SMC side)\n");
    return 0;
}
