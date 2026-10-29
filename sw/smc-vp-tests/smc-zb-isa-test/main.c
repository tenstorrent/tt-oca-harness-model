/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-zb-isa-test/main.c
 *
 * The SMC cores implement the Zba, Zbb and Zbs bit-manipulation extensions, and firmware built
 * for them (the SMC Boot ROM among it) uses their instructions. Execute instructions from each
 * extension and check the results; an unsupported instruction traps instead.
 */
#include <stdint.h>

#include "smc_common.h"

extern int printf(const char *fmt, ...);

static int check(const char *what, uint64_t got, uint64_t want)
{
    if (got == want)
        return 1;
    printf("FAIL: %s = 0x%08x%08x, expected 0x%08x%08x\n", what, (uint32_t)(got >> 32),
           (uint32_t)got, (uint32_t)(want >> 32), (uint32_t)want);
    return 0;
}

int main(void)
{
    uint64_t rd;
    int pass = 1;

    printf("\n=== SMC Zba/Zbb/Zbs ISA test ===\n");

    /* Zba */
    __asm__ volatile("sh1add %0, %1, %2" : "=r"(rd) : "r"(0x10ULL), "r"(3ULL));
    pass &= check("sh1add", rd, 0x23);
    __asm__ volatile("add.uw %0, %1, %2" : "=r"(rd) : "r"(0xFFFFFFFF00000005ULL), "r"(1ULL));
    pass &= check("add.uw", rd, 6);

    /* Zbb */
    __asm__ volatile("clz %0, %1" : "=r"(rd) : "r"(0x1000ULL));
    pass &= check("clz", rd, 51);
    __asm__ volatile("andn %0, %1, %2" : "=r"(rd) : "r"(0xFF00ULL), "r"(0x0F00ULL));
    pass &= check("andn", rd, 0xF000);
    __asm__ volatile("rev8 %0, %1" : "=r"(rd) : "r"(0x0102030405060708ULL));
    pass &= check("rev8", rd, 0x0807060504030201ULL);

    /* Zbs */
    __asm__ volatile("bset %0, %1, %2" : "=r"(rd) : "r"(0ULL), "r"(5ULL));
    pass &= check("bset", rd, 0x20);
    __asm__ volatile("bext %0, %1, %2" : "=r"(rd) : "r"(0x20ULL), "r"(5ULL));
    pass &= check("bext", rd, 1);

    printf(pass ? "PASS: Zba, Zbb and Zbs instructions execute with the expected results\n"
                : "FAIL: SMC Zb ISA test\n");
    return 0;
}
