/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-cflush-test/main.c
 *
 * The SMC Rocket cores implement the custom CFLUSH.D.L1 instruction, which writes back and
 * invalidates the L1 data cache: the whole cache with rs1 = x0, or the line holding the
 * address in rs1. SMC firmware executes it before handing memory to another agent. Execute
 * both forms; the test passes when execution continues past them.
 */
#include <stdint.h>

#include "smc_common.h"

extern int printf(const char *fmt, ...);

static volatile uint64_t flushed_word = 0x0123456789ABCDEFULL;

int main(void)
{
    printf("\n=== SMC CFLUSH.D.L1 test ===\n");

    /* CFLUSH.D.L1 x0: write back and invalidate the whole L1 D-cache. */
    __asm__ volatile("fence\n\t.word 0xfc000073\n\tfence" ::: "memory");

    /* CFLUSH.D.L1 a0: the line holding the address in a0 (rs1 = x10). */
    register uintptr_t a0 __asm__("a0") = (uintptr_t)&flushed_word;
    __asm__ volatile(".word 0xfc050073" ::"r"(a0) : "memory");

    if (flushed_word != 0x0123456789ABCDEFULL) {
        printf("FAIL: data changed across CFLUSH.D.L1\n");
        return 0;
    }
    printf("PASS: CFLUSH.D.L1 with rs1 = x0 and rs1 = a0 executes without a trap\n");
    return 0;
}
