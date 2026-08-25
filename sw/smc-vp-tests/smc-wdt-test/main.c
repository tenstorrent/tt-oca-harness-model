/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smc-vp-tests/smc-wdt-test/main.c
 *
 * Platform-level smoke test for the SiFive TLWDT (stage 1) on the SMC VP
 * front-port window, plus stage-2 WDT_TIMEOUT / WDT_TIMEOUT_RESET CSRs on the
 * cluster cpu_ctrl path (0xC003_9000).
 *
 * Flow:
 *   1. Unlock KEY, program a small CMP, enable ALWAYS
 *   2. Poll until CTRL.IP asserts (count reaches compare)
 *   3. FEED clears COUNT; verify KEY re-locks
 *   4. Stage-2: write/read WDT_TIMEOUT via front-port cpu_ctrl; pulse RESET
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define WDT_BASE   SMC_WDT0_BASE
#define S2_BASE    SMC_CPU_CTRL_FP_BASE

static void wdt_unlock(void)
{
    REG_WRITE(WDT_BASE + WDT_KEY, WDT_KEY_MAGIC);
}

static int poll_wdt_ip(unsigned max_spins)
{
    unsigned i;
    for (i = 0; i < max_spins; ++i) {
        if ((REG_READ(WDT_BASE + WDT_CTRL) & WDT_CTRL_IP) != 0u) {
            return 1;
        }
        /* Burn a few cycles so Whisper/SystemC time can advance ticks. */
        for (volatile int k = 0; k < 64; ++k) {
            __asm__ volatile("nop");
        }
    }
    return 0;
}

int main(void)
{
    int pass = 1;

    printf("\n=== SMC WDT stage-1 / stage-2 platform test ===\n\n");

    /* --- Reset defaults --- */
    if (REG_READ(WDT_BASE + WDT_KEY) != 0u) {
        printf("FAIL: KEY unlocked at reset\n");
        pass = 0;
    }
    if (REG_READ(WDT_BASE + WDT_COUNT) != 0u) {
        printf("FAIL: COUNT not zero at reset\n");
        pass = 0;
    }
    printf("reset defaults OK (KEY=0 COUNT=0)\n");

    /* --- Unlock + program compare / always-on --- */
    wdt_unlock();
    if (REG_READ(WDT_BASE + WDT_KEY) != 1u) {
        printf("FAIL: KEY unlock did not stick\n");
        pass = 0;
    }
    REG_WRITE(WDT_BASE + WDT_CMP, 0x20u);
    wdt_unlock();
    REG_WRITE(WDT_BASE + WDT_CTRL, WDT_CTRL_ALWAYS);
    printf("programmed CMP=0x20 CTRL=ALWAYS\n");

    /* Locked write should be ignored */
    REG_WRITE(WDT_BASE + WDT_CMP, 0xFFFFu);
    if ((REG_READ(WDT_BASE + WDT_CMP) & 0xFFFFu) != 0x20u) {
        printf("FAIL: locked CMP write was not ignored\n");
        pass = 0;
    }

    /* --- Wait for stage-1 IP --- */
    printf("polling for CTRL.IP ...\n");
    if (!poll_wdt_ip(2000000u)) {
        printf("FAIL: timed out waiting for WDT IP (count=0x");
        printf("%x", REG_READ(WDT_BASE + WDT_COUNT));
        printf(")\n");
        pass = 0;
    } else {
        printf("CTRL.IP asserted (count=0x");
        printf("%x", REG_READ(WDT_BASE + WDT_COUNT));
        printf(")\n");
    }

    /* --- Feed clears count and re-locks --- */
    wdt_unlock();
    REG_WRITE(WDT_BASE + WDT_FEED, WDT_FEED_MAGIC);
    if (REG_READ(WDT_BASE + WDT_COUNT) != 0u) {
        printf("FAIL: FEED did not clear COUNT\n");
        pass = 0;
    }
    if (REG_READ(WDT_BASE + WDT_KEY) != 0u) {
        printf("FAIL: FEED did not re-lock KEY\n");
        pass = 0;
    }
    printf("FEED cleared COUNT and re-locked\n");

    /* --- Stage-2 CSRs on cluster front-port cpu_ctrl --- */
    REG_WRITE(S2_BASE + CPU_CTRL_WDT_TIMEOUT, 0x123u);
    {
        uint32_t to = REG_READ(S2_BASE + CPU_CTRL_WDT_TIMEOUT);
        printf("WDT_TIMEOUT readback = 0x");
        printf("%x", to);
        printf("\n");
        if (to != 0x123u) {
            printf("FAIL: WDT_TIMEOUT readback mismatch\n");
            pass = 0;
        }
    }
    REG_WRITE(S2_BASE + CPU_CTRL_WDT_TIMEOUT_RESET, 0x1u);
    if (REG_READ(S2_BASE + CPU_CTRL_WDT_TIMEOUT_RESET) != 0u) {
        printf("FAIL: WDT_TIMEOUT_RESET did not self-clear\n");
        pass = 0;
    } else {
        printf("WDT_TIMEOUT_RESET self-cleared\n");
    }

    if (pass) {
        printf("\nPASS: SMC WDT stage-1 IP/FEED and stage-2 CSR paths work\n\n");
    } else {
        printf("\nFAIL: SMC WDT platform test\n\n");
    }
    return 0;
}
