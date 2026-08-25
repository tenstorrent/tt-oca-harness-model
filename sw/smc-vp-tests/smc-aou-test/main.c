/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smc-vp-tests/smc-aou-test/main.c
 *
 * AOU LT smoke test: CSR reset image + activate/deactivate via MMIO.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define AOU_IP_VERSION  0x00u
#define AOU_CON0        0x04u
#define AOU_INIT        0x08u
#define AOU_DEST_RP     0x14u

#define AOU_INIT_ACTIVATE_START   (1u << 0)
#define AOU_INIT_DEACTIVATE_START (1u << 1)
#define AOU_INIT_STATE_ENABLED    (1u << 2)
#define AOU_INIT_STATE_DISABLED   (1u << 3)
#define AOU_INIT_INT_ACTIVATE     (1u << 8)

static uint32_t aou_read(uint32_t off)
{
    return REG_READ(SMC_AOU_BASE + off);
}

static void aou_write(uint32_t off, uint32_t val)
{
    REG_WRITE(SMC_AOU_BASE + off, val);
}

int main(void)
{
    printf("\n=== SMC AOU smoke test ===\n\n");
    int pass = 1;

    uint32_t ver = aou_read(AOU_IP_VERSION);
    uint32_t con0 = aou_read(AOU_CON0);
    uint32_t init = aou_read(AOU_INIT);
    uint32_t dest = aou_read(AOU_DEST_RP);
    printf("reset IP_VERSION=0x%x CON0=0x%x INIT=0x%x DEST_RP=0x%x\n",
           ver, con0, init, dest);
    if (ver != 0x00010000u || con0 != 0x00004008u || dest != 0x00003210u) {
        printf("  FAIL: unexpected AOU reset values\n");
        pass = 0;
    }
    if ((init & AOU_INIT_STATE_DISABLED) == 0) {
        printf("  FAIL: expected DISABLED at reset\n");
        pass = 0;
    }

    aou_write(AOU_INIT, AOU_INIT_ACTIVATE_START);
    init = aou_read(AOU_INIT);
    printf("after activate INIT=0x%x\n", init);
    if ((init & AOU_INIT_STATE_ENABLED) == 0) {
        printf("  FAIL: not ENABLED after activate\n");
        pass = 0;
    }
    if ((init & AOU_INIT_INT_ACTIVATE) == 0) {
        printf("  FAIL: missing INT_ACTIVATE_START\n");
        pass = 0;
    }
    aou_write(AOU_INIT, AOU_INIT_INT_ACTIVATE); /* W1C */

    aou_write(AOU_INIT, AOU_INIT_DEACTIVATE_START);
    init = aou_read(AOU_INIT);
    printf("after deactivate INIT=0x%x\n", init);
    if ((init & AOU_INIT_STATE_DISABLED) == 0) {
        printf("  FAIL: not DISABLED after deactivate\n");
        pass = 0;
    }

    if (pass)
        printf("\nALL TESTS PASSED\n");
    else
        printf("\nTEST FAILED\n");
    return pass ? 0 : 1;
}
