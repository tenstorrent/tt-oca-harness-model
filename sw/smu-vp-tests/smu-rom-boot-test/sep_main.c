/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * sw/smu-vp-tests/smu-rom-boot-test/sep_main.c
 *
 * SEP side of the SMU ROM-only boot test (runs on smu-vp). The SMC runs only a boot ROM
 * image; when hart 0 has run it from the ROM base, it leaves a token in SMC SRAM, which the
 * SEP reads through its SMC window.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define ROM_TOKEN     0x40061200u /* SMC SRAM 0xC006_1200 */
#define MAGIC_ROM     0x524F4D31u /* "ROM1" */
#define SPIN_LIMIT    50000000u

#define REG32(addr)   (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU ROM-only boot test (SEP side) ===\n");

    for (uint32_t spins = 0; REG32(ROM_TOKEN) != MAGIC_ROM; ++spins) {
        if (spins > SPIN_LIMIT) {
            printf("FAIL: no token from the SMC boot ROM image (0x%08x reads 0x%08x)\n",
                   ROM_TOKEN, REG32(ROM_TOKEN));
            return 1;
        }
    }
    printf("PASS: SMC ran its boot ROM image with no SMC ELF (SEP side)\n");
    return 0;
}
