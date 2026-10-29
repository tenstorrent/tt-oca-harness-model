/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smu-vp-tests/smu-reset-pulse-test/smc_main.c
 *
 * SMC side of the SMU core reset pulse test (runs on smu-vp).
 *
 * Places a counter routine in SMC SRAM for the SEP to start hart 0 at, then
 * tells the SEP it is ready. The routine increments a token 0x1000 past its own
 * address forever; it is position-independent, so it runs where it is copied.
 * The SEP then pulses hart 0 into it, holds hart 0, and releases it, watching
 * the token through its SMC window.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define ROUTINE_ADDR  0xC0070000ULL /* SMC SRAM; the SEP sees it at 0x4007_0000 */
#define TOKEN_ADDR    (ROUTINE_ADDR + 0x1000)
#define READY_ADDR    0xC0061100ULL /* SEP view 0x4006_1100 */
#define MAGIC_READY   0x52454459u   /* "REDY" */

#define REG32(addr)   (*(volatile uint32_t *)(uintptr_t)(addr))

/* t0 = token address; loop { token += 1 } */
/* Uncompressed, so the routine is a whole number of words. */
__asm__(".section .rodata\n"
        ".balign 4\n"
        ".option push\n"
        ".option norvc\n"
        "counter_routine:\n"
        "    auipc t0, 1\n"
        "1:  lw    t1, 0(t0)\n"
        "    addi  t1, t1, 1\n"
        "    sw    t1, 0(t0)\n"
        "    j     1b\n"
        "counter_routine_end:\n"
        ".option pop\n"
        ".text\n");
extern const uint32_t counter_routine[];
extern const uint32_t counter_routine_end[];

int main(void)
{
    printf("\n=== SMU reset pulse test (SMC side) ===\n");

    const unsigned words = (unsigned)(counter_routine_end - counter_routine);
    for (unsigned k = 0; k < words; ++k) {
        REG32(ROUTINE_ADDR + 4 * k) = counter_routine[k];
    }
    REG32(TOKEN_ADDR) = 0;
    __asm__ volatile("fence\n\tfence.i" ::: "memory");
    printf("counter routine (%u words) at 0x%08x, token at 0x%08x\n", words,
           (unsigned)ROUTINE_ADDR, (unsigned)TOKEN_ADDR);

    /* Report before the SEP is told: hart 0 is about to be reset out of main. */
    printf("PASS: SMU reset pulse test (SMC side)\n");
    REG32(READY_ADDR) = MAGIC_READY;
    for (;;) {
    }
}
