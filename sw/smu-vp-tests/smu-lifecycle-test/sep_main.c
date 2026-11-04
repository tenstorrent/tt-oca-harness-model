/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * sw/smu-vp-tests/smu-lifecycle-test/sep_main.c
 *
 * SEP side of the SMU lifecycle test (runs on smu-vp). The SEP drives its eFuse lifecycle,
 * {~lc, lc}, to the SMC, where CHIP_CONFIG.LC_STATE shows it. The SEP compares the SMC's
 * register, through its SMC window, with its own fuse: first in TEST_DEV, the default fuse
 * image, then after it moves the part to PROD, which the SMC must follow.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define SEP_EFUSE_LC_STATE  0x1093000Cu
#define SMC_LC_STATE        0x4000290Cu /* SMC-local 0xC000_290C */
#define LC_TEST_DEV         0xF0u
#define LC_PROD             0xE1u

#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

static int expect_lc(uint32_t want, const char *state)
{
    const uint32_t fuse = REG32(SEP_EFUSE_LC_STATE) & 0xFFu;
    const uint32_t smc  = REG32(SMC_LC_STATE) & 0xFFu;
    if (fuse != want || smc != want) {
        printf("FAIL: %s: SEP fuse LC_STATE 0x%02x, SMC LC_STATE 0x%02x, expected 0x%02x\n",
               state, fuse, smc, want);
        return 0;
    }
    printf("%s: SEP fuse and SMC LC_STATE both 0x%02x\n", state, smc);
    return 1;
}

int main(void)
{
    printf("\n=== SMU lifecycle test (SEP side) ===\n");

    if (!expect_lc(LC_TEST_DEV, "TEST_DEV")) return 1;

    REG32(SEP_EFUSE_LC_STATE) = 0x1u; /* TEST_DEV -> PROD */
    if (!expect_lc(LC_PROD, "PROD")) return 1;

    printf("PASS: SMC LC_STATE follows the SEP fuse lifecycle (SEP side)\n");
    return 0;
}
