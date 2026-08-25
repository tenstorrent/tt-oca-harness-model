/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * SEP half of smc_cpu_traffic_sep_axi_test: arm SRAM mailbox, then verify
 * 50 beats written by the SMC through the SMU xbar.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define SEP_SRAM_BASE       0x1000A000u
#define NBEATS              50u
#define ARM_TOKEN           0xA11CED00u
#define PATTERN(i)          (0xA5000000u + (i))
#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU traffic test (SEP side) ===\n");

    for (uint32_t i = 0; i < NBEATS; ++i)
        REG32(SEP_SRAM_BASE + 4u * i) = 0u;
    REG32(SEP_SRAM_BASE) = ARM_TOKEN;

    uint32_t spins = 0;
    while (REG32(SEP_SRAM_BASE) != PATTERN(0)) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC beat 0\n");
            return 1;
        }
    }

    for (uint32_t i = 0; i < NBEATS; ++i) {
        const uint32_t got = REG32(SEP_SRAM_BASE + 4u * i);
        const uint32_t want = PATTERN(i);
        if (got != want) {
            printf("FAIL: beat %u got 0x%08x want 0x%08x\n", i, got, want);
            return 1;
        }
    }
    printf("SMC->SEP xbar: %u beats verified in SEP SRAM\n", NBEATS);
    printf("PASS: SMU traffic test (SEP side)\n");
    return 0;
}
