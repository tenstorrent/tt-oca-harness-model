/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * Port of tt-oca-hw fw/sep/tests/sep_smu_bidirect + smc_sep_xbar SEP half.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define SEP_SRAM_SHARED     0x10009000u
#define SMC_SCRATCH_DATA    0x40062000u   /* SMC scratchpad 0xC0062000 */
#define SMC_SCRATCH_ACK     0x40062004u

#define XBAR_DATA_PATTERN       0x13579BDFu
#define XBAR_SMC_TO_SEP_CMD     0xC001CAFEu
#define XBAR_SMC_TO_SEP_DONE    0xD0E0F00Du
#define XBAR_SEP_TO_SMC_ACK     0x5E9ACCE5u

#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU xbar test (SEP side) ===\n");

    REG32(SEP_SRAM_SHARED) = 0u;
    REG32(SMC_SCRATCH_ACK) = 0u;
    REG32(SMC_SCRATCH_DATA) = XBAR_DATA_PATTERN;
    printf("SEP->SMC dedicated path: pattern 0x%08x written\n", XBAR_DATA_PATTERN);

    uint32_t spins = 0;
    while (REG32(SEP_SRAM_SHARED) != XBAR_SMC_TO_SEP_CMD) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC CMD\n");
            return 1;
        }
    }
    printf("SMC->SEP xbar: CMD received\n");

    REG32(SMC_SCRATCH_ACK) = XBAR_SEP_TO_SMC_ACK;

    spins = 0;
    while (REG32(SEP_SRAM_SHARED) != XBAR_SMC_TO_SEP_DONE) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC DONE\n");
            return 1;
        }
    }
    printf("SMC->SEP xbar: DONE received\n");

    printf("PASS: SMU xbar test (SEP side)\n");
    return 0;
}
