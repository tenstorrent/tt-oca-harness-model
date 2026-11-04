/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * sw/smu-vp-tests/smu-reset-pulse-test/sep_main.c
 *
 * SEP side of the SMU core reset pulse test (runs on smu-vp).
 *
 * Through its SMC window (SMC-local 0xC00x_xxxx at 0x400x_xxxx), the SEP:
 *   1. waits for the SMC's ready flag;
 *   2. sets RESET_VECTOR[0] to the SMC's counter routine, as two 32-bit halves;
 *   3. pulses hart 0 alone (RESET_CTRL 0x11F) and waits for core_resets_done[0];
 *   4. checks that the token starts counting: hart 0 restarted at the routine;
 *   5. holds hart 0 (RESET_CTRL 0x10E) and checks that the token stops;
 *   6. releases hart 0 (RESET_CTRL 0x10F) and checks that it counts again.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define CPU_CTRL          0x40039000u
#define RESET_VECTOR0_LO  (CPU_CTRL + 0x000)
#define RESET_VECTOR0_HI  (CPU_CTRL + 0x004)
#define RESET_CTRL        (CPU_CTRL + 0x020)
#define PULSE_COUNT_HI    (CPU_CTRL + 0x02C) /* core_resets_done[35:32] in bits [3:0] */

#define ROUTINE_SMC       0xC0070000u
#define TOKEN             0x40071000u
#define READY             0x40061100u
#define MAGIC_READY       0x52454459u

#define SPIN_LIMIT        50000000u
#define SETTLE_SPINS      20000u

#define REG32(addr)       (*(volatile uint32_t *)(uintptr_t)(addr))

static int wait_for(uint32_t addr, uint32_t mask, uint32_t want, const char *what)
{
    for (uint32_t spins = 0; (REG32(addr) & mask) != want; ++spins) {
        if (spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for %s (0x%08x reads 0x%08x)\n", what, addr,
                   REG32(addr));
            return 0;
        }
    }
    return 1;
}

static void settle(void)
{
    for (volatile uint32_t k = 0; k < SETTLE_SPINS; ++k) {
    }
}

static int token_advances(const char *when)
{
    const uint32_t before = REG32(TOKEN);
    settle();
    const uint32_t after = REG32(TOKEN);
    if (after == before) {
        printf("FAIL: token stuck at %u %s\n", before, when);
        return 0;
    }
    printf("token %u -> %u %s\n", before, after, when);
    return 1;
}

int main(void)
{
    printf("\n=== SMU reset pulse test (SEP side) ===\n");

    if (!wait_for(READY, 0xFFFFFFFFu, MAGIC_READY, "the SMC ready flag")) return 1;

    REG32(RESET_VECTOR0_HI) = 0;
    REG32(RESET_VECTOR0_LO) = ROUTINE_SMC;
    if (REG32(RESET_VECTOR0_LO) != ROUTINE_SMC || REG32(RESET_VECTOR0_HI) != 0) {
        printf("FAIL: RESET_VECTOR[0] reads back 0x%08x_%08x\n", REG32(RESET_VECTOR0_HI),
               REG32(RESET_VECTOR0_LO));
        return 1;
    }
    if (REG32(TOKEN) != 0) {
        printf("FAIL: token is %u before the pulse\n", REG32(TOKEN));
        return 1;
    }

    REG32(RESET_CTRL) = 0x11Fu;
    if (REG32(RESET_CTRL) != 0x10Fu) {
        printf("FAIL: RESET_CTRL reads 0x%08x after a pulse (pulse bits are not stored)\n",
               REG32(RESET_CTRL));
        return 1;
    }
    if (!wait_for(PULSE_COUNT_HI, 0x1u, 0x1u, "core_resets_done[0]")) return 1;
    for (uint32_t spins = 0; REG32(TOKEN) == 0; ++spins) {
        if (spins > SPIN_LIMIT) {
            printf("FAIL: token still 0: hart 0 did not restart at RESET_VECTOR[0]\n");
            return 1;
        }
    }
    if (!token_advances("after the pulse: hart 0 runs the routine")) return 1;

    REG32(RESET_CTRL) = 0x10Eu;
    settle();
    const uint32_t held = REG32(TOKEN);
    settle();
    if (REG32(TOKEN) != held) {
        printf("FAIL: token moved %u -> %u while hart 0 is held\n", held, REG32(TOKEN));
        return 1;
    }
    printf("token held at %u while RESET_CTRL holds hart 0\n", held);

    REG32(RESET_CTRL) = 0x10Fu;
    if (!token_advances("after release")) return 1;

    printf("PASS: SMU reset pulse test (SEP side)\n");
    return 0;
}
