/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-straps-test/main.c
 *
 * Firmware-visible captured straps at their mandatory-window address,
 * smc_external + 0x3000 (tt-oca-harness #2922 moved them from + 0x5800).
 *
 * The .ini straps 0xFABCDEF0_12345678. STRAPS_LO is [31:0] and STRAPS_HI is
 * [60:32] (field [28:0]), so the expected words are 0x12345678 / 0x1ABCDEF0.
 * A zero strap word could not tell this window from a read-as-zero hole.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define SMC_STRAPS_BASE     0xC0403000ULL
#define SMC_STRAPS_OLD_BASE 0xC0405800ULL
#define STRAPS_LO           0x0u
#define STRAPS_HI           0x4u

int main(void)
{
    int pass = 1;

    printf("\n=== SMC straps addrmap test ===\n");

    const uint32_t lo = REG_READ(SMC_STRAPS_BASE + STRAPS_LO);
    const uint32_t hi = REG_READ(SMC_STRAPS_BASE + STRAPS_HI);
    printf("STRAPS_LO=0x%08x STRAPS_HI=0x%08x\n", lo, hi);
    if (lo != 0x12345678u) {
        printf("FAIL: STRAPS_LO at smc_external+0x3000\n");
        pass = 0;
    }
    if (hi != 0x1ABCDEF0u) {
        printf("FAIL: STRAPS_HI at smc_external+0x3000 (expect [28:0] only)\n");
        pass = 0;
    }

    /* Read-only: software cannot change the captured word. */
    REG_WRITE(SMC_STRAPS_BASE + STRAPS_LO, 0xDEADBEEFu);
    REG_WRITE(SMC_STRAPS_BASE + STRAPS_HI, 0xFFFFFFFFu);
    if (REG_READ(SMC_STRAPS_BASE + STRAPS_LO) != 0x12345678u ||
        REG_READ(SMC_STRAPS_BASE + STRAPS_HI) != 0x1ABCDEF0u) {
        printf("FAIL: straps accepted a software write\n");
        pass = 0;
    }

    /* The old supplementary slot no longer decodes to the straps. */
    const uint32_t old_lo = REG_READ(SMC_STRAPS_OLD_BASE + STRAPS_LO);
    const uint32_t old_hi = REG_READ(SMC_STRAPS_OLD_BASE + STRAPS_HI);
    if (old_lo == 0x12345678u || old_hi == 0x1ABCDEF0u) {
        printf("FAIL: straps still visible at old smc_external+0x5800\n");
        pass = 0;
    }

    printf(pass ? "PASS: straps decode at smc_external+0x3000 only\n"
                : "FAIL: SMC straps addrmap test\n");
    return 0;
}
