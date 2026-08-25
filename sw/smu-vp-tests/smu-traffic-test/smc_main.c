/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * Port of tt-oca-hw dv/smu smc_cpu_traffic_sep_axi_test (50-transaction
 * SMC CPU traffic into the SEP aperture).  VP: 50 write+readback beats
 * through output_axi -> smu_axi_xbar -> sep_in -> SEP SRAM.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define SEP_SRAM_BASE       0x6000A000ULL
#define NBEATS              50u
#define ARM_TOKEN           0xA11CED00u
#define PATTERN(i)          (0xA5000000u + (i))
#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU traffic test (SMC side) ===\n");

    /* Wait until SEP has armed the mailbox (SRAM may already be zero). */
    uint32_t spins = 0;
    while (REG32(SEP_SRAM_BASE) != ARM_TOKEN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SEP to arm mailbox\n");
            return 1;
        }
    }

    for (uint32_t i = 0; i < NBEATS; ++i) {
        const uint32_t want = PATTERN(i);
        REG32(SEP_SRAM_BASE + 4u * i) = want;
        const uint32_t got = REG32(SEP_SRAM_BASE + 4u * i);
        if (got != want) {
            printf("FAIL: beat %u readback 0x%08x want 0x%08x\n", i, got, want);
            return 1;
        }
    }
    /* Doorbell: last cell already holds PATTERN(NBEATS-1); SEP polls beat 0. */
    printf("SMC->SEP xbar: %u beats written and read back\n", NBEATS);
    printf("PASS: SMU traffic test (SMC side)\n");
    return 0;
}
