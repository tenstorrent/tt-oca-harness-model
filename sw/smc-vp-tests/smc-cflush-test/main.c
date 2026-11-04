/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-cflush-test/main.c
 *
 * The SMC Rocket cores implement the custom CFLUSH.D.L1 instruction, which writes back and
 * invalidates the L1 data cache: the whole cache with rs1 = x0, or the line holding the
 * address in rs1. SMC firmware executes it before handing memory to another agent. Execute
 * both forms in M-mode, where they continue without a trap, then once in U-mode, where Rocket
 * traps it as an illegal instruction. The cores have S- and U-mode, so the start-up code's
 * clearing of the trap delegation registers does not trap either.
 */
#include <stdint.h>

#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define MISA_S          (1ULL << ('S' - 'A'))
#define MISA_U          (1ULL << ('U' - 'A'))
#define MSTATUS_MPP     (3ULL << 11)
#define MCAUSE_ILLEGAL  2ULL
#define NO_TRAP         (~0ULL)

static volatile uint64_t flushed_word = 0x0123456789ABCDEFULL;
static volatile uint64_t trap_cause   = NO_TRAP;

/* Record the cause and resume in M-mode after the 4-byte trapping instruction. */
void trap_handler(void)
{
    uint64_t cause, epc;
    __asm__ volatile("csrr %0, mcause" : "=r"(cause));
    __asm__ volatile("csrr %0, mepc" : "=r"(epc));
    trap_cause = cause;
    __asm__ volatile("csrw mepc, %0" ::"r"(epc + 4));
    __asm__ volatile("csrs mstatus, %0" ::"r"(MSTATUS_MPP));
}

static void cflush_in_user_mode(void)
{
    /* One NAPOT PMP region over the whole address space, so U-mode may fetch. */
    __asm__ volatile("csrw pmpaddr0, %0\n\tcsrw pmpcfg0, %1" ::"r"(~0ULL), "r"(0x1FULL));
    __asm__ volatile("li   t0, %0\n\t"
                     "csrc mstatus, t0\n\t"
                     "la   t0, 1f\n\t"
                     "csrw mepc, t0\n\t"
                     "mret\n"
                     "1:\n\t"
                     ".word 0xfc000073"
                     ::"i"(MSTATUS_MPP) : "t0", "memory");
}

int main(void)
{
    printf("\n=== SMC CFLUSH.D.L1 test ===\n");

    uint64_t misa;
    __asm__ volatile("csrr %0, misa" : "=r"(misa));
    if ((misa & (MISA_S | MISA_U)) != (MISA_S | MISA_U)) {
        printf("FAIL: misa 0x%x lacks S or U\n", (unsigned)misa);
        return 0;
    }
    __asm__ volatile("csrw mideleg, zero\n\tcsrw medeleg, zero");
    if (trap_cause != NO_TRAP) {
        printf("FAIL: clearing mideleg/medeleg trapped, mcause %u\n", (unsigned)trap_cause);
        return 0;
    }

    /* CFLUSH.D.L1 x0: write back and invalidate the whole L1 D-cache. */
    __asm__ volatile("fence\n\t.word 0xfc000073\n\tfence" ::: "memory");

    /* CFLUSH.D.L1 a0: the line holding the address in a0 (rs1 = x10). */
    register uintptr_t a0 __asm__("a0") = (uintptr_t)&flushed_word;
    __asm__ volatile(".word 0xfc050073" ::"r"(a0) : "memory");

    if (flushed_word != 0x0123456789ABCDEFULL || trap_cause != NO_TRAP) {
        printf("FAIL: CFLUSH.D.L1 in M-mode changed data or trapped\n");
        return 0;
    }

    cflush_in_user_mode();
    if (trap_cause != MCAUSE_ILLEGAL) {
        printf("FAIL: CFLUSH.D.L1 in U-mode gave mcause 0x%x, not illegal instruction\n",
               (unsigned)trap_cause);
        return 0;
    }
    printf("PASS: CFLUSH.D.L1 executes in M-mode and traps in U-mode\n");
    return 0;
}
