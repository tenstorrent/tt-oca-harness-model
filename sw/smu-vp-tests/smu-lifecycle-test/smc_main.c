/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smu-vp-tests/smu-lifecycle-test/smc_main.c
 *
 * SMC side of the SMU lifecycle test (runs on smu-vp). The SMC's CHIP_CONFIG.LC_STATE
 * carries the SEP's lifecycle in its differential encoding {~lc, lc}. With the default fuse
 * image the part is TEST_DEV, so the SMC reads 0xF0 from boot.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define SMC_LC_STATE  0xC000290CULL /* misc_wrap + chip_config 0x100 + LC_STATE 0xC */
#define LC_TEST_DEV   0xF0u

int main(void)
{
    printf("\n=== SMU lifecycle test (SMC side) ===\n");

    const uint32_t raw = REG_READ(SMC_LC_STATE) & 0xFFu;
    printf("CHIP_CONFIG.LC_STATE = 0x%02x\n", raw);
    if (raw != LC_TEST_DEV) {
        printf("FAIL: SMC LC_STATE 0x%02x, expected 0x%02x (TEST_DEV)\n", raw, LC_TEST_DEV);
        return 1;
    }
    printf("PASS: SMC LC_STATE carries the SEP's TEST_DEV lifecycle (SMC side)\n");
    return 0;
}
