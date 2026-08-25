/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-telemetry-test/main.c
 *
 * Telemetry receiver platform smoke test — register map + optional ATB inject.
 *
 * Exercises the three RTL-aligned instances under SMC_TELEMETRY_BASE
 * (0xC000_9000 + N*0x100):
 *   1. STATUS resets to BUFFER_EMPTY; CTRL / INTR_* reset to 0.
 *   2. INTR_ENABLE masks reserved bits to 0x11.
 *   3. All three instance windows decode independently.
 *   4. When the platform .ini enables tel_inject_*, instance 0 already holds
 *      the injected message (probe_id / counter[0]) before firmware runs.
 *
 * ATB ingress is a C++ back door (push_atb_beats); firmware cannot synthesise
 * beats over MMIO — same pattern as BEU inject_error.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define NUM_TEL_INSTANCES 3u

int main(void)
{
    printf("\n=== SMC Telemetry Receiver platform smoke test ===\n\n");

    int pass = 1;

    /* -------- 1. Reset values on instance 0 ------------------------------ */
    uint32_t status = REG_READ(SMC_TELEMETRY0_BASE + TEL_STATUS);
    uint32_t ctrl   = REG_READ(SMC_TELEMETRY0_BASE + TEL_CTRL);
    uint32_t ien    = REG_READ(SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE);
    uint32_t ist    = REG_READ(SMC_TELEMETRY0_BASE + TEL_INTR_STATUS);
    printf("tel0 STATUS=0x%x CTRL=0x%x INTR_ENABLE=0x%x INTR_STATUS=0x%x\n",
           status, ctrl, ien, ist);

    /* With CCI inject enabled the queue is non-empty; without it STATUS must
     * show BUFFER_EMPTY.  Detect inject by reading PROBE_ID after checking
     * the empty case's INTR/CTRL reset fields. */
    if (ctrl != 0u || ien != 0u) {
        printf("  FAIL: CTRL/INTR_ENABLE did not reset to 0\n");
        pass = 0;
    }

    /* -------- 2. INTR_ENABLE masks reserved bits ------------------------- */
    REG_WRITE(SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE, 0xFFFFFFFFu);
    ien = REG_READ(SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE);
    printf("tel0 INTR_ENABLE after write 0xffffffff = 0x%x\n", ien);
    if (ien != (TEL_INTR_MISSING_LAST | TEL_INTR_BUFFER_THRESHOLD)) {
        printf("  FAIL: INTR_ENABLE not masked to 0x11 (got 0x%x)\n", ien);
        pass = 0;
    }
    REG_WRITE(SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE, 0u);

    /* -------- 3. Every instance window decodes --------------------------- */
    for (unsigned n = 0; n < NUM_TEL_INSTANCES; n++) {
        uintptr_t base = SMC_TELEMETRY_BASE + 0x100ULL * n;
        uint32_t st = REG_READ(base + TEL_STATUS);
        printf("tel%u STATUS = 0x%x\n", n, st);
        /* Bit 0 (EMPTY) or bit 4 (FULL) must be a defined STATUS encoding;
         * a decode miss would typically return poison / all-ones. */
        if (st != TEL_STATUS_BUFFER_EMPTY &&
            st != 0u &&
            st != TEL_STATUS_BUFFER_FULL &&
            st != (TEL_STATUS_BUFFER_EMPTY | TEL_STATUS_BUFFER_FULL)) {
            /* Allow empty (1) or non-empty (0) or full (0x10). */
            if ((st & ~(TEL_STATUS_BUFFER_EMPTY | TEL_STATUS_BUFFER_FULL)) != 0u) {
                printf("  FAIL: tel%u STATUS=0x%x has reserved bits set\n", n, st);
                pass = 0;
            }
        }
    }

    /* -------- 4. Injected message (optional; see smc_telemetry_test.ini) - */
    uint32_t probe = REG_READ(SMC_TELEMETRY0_BASE + TEL_PROBE_ID);
    uint32_t vlds  = REG_READ(SMC_TELEMETRY0_BASE + TEL_COUNTER_VLDS);
    uint32_t c0    = REG_READ(SMC_TELEMETRY0_BASE + TEL_COUNTER(0));
    status = REG_READ(SMC_TELEMETRY0_BASE + TEL_STATUS);
    printf("tel0 after optional inject: STATUS=0x%x PROBE_ID=0x%x "
           "VLDS=0x%x COUNTER0=0x%x\n", status, probe, vlds, c0);

    if ((status & TEL_STATUS_BUFFER_EMPTY) == 0u) {
        /* Inject path: expect probe 0x15, counter0 0xDEADBEEF, vld bit 0. */
        if (probe != 0x15u) {
            printf("  FAIL: expected PROBE_ID=0x15, got 0x%x\n", probe);
            pass = 0;
        }
        if ((vlds & 0x1u) == 0u) {
            printf("  FAIL: COUNTER_VLDS bit0 clear (got 0x%x)\n", vlds);
            pass = 0;
        }
        if (c0 != 0xDEADBEEFu) {
            printf("  FAIL: COUNTER0=0x%x != 0xdeadbeef\n", c0);
            pass = 0;
        }
        /* Drain with a threshold-preserving BUFFER_POP. */
        uint32_t thr = REG_READ(SMC_TELEMETRY0_BASE + TEL_CTRL) & 0x00FFF000u;
        REG_WRITE(SMC_TELEMETRY0_BASE + TEL_CTRL, thr | TEL_CTRL_BUFFER_POP);
        status = REG_READ(SMC_TELEMETRY0_BASE + TEL_STATUS);
        if ((status & TEL_STATUS_BUFFER_EMPTY) == 0u) {
            printf("  FAIL: queue not empty after BUFFER_POP (STATUS=0x%x)\n",
                   status);
            pass = 0;
        }
    } else {
        printf("  (no inject: queue empty — register smoke only)\n");
        if (probe != 0u || vlds != 0u || c0 != 0u) {
            printf("  FAIL: empty view should read zeros "
                   "(probe=0x%x vlds=0x%x c0=0x%x)\n", probe, vlds, c0);
            pass = 0;
        }
    }

    if (pass) {
        printf("\nPASS: Telemetry receiver platform smoke test\n\n");
    } else {
        printf("\nFAIL: Telemetry receiver platform smoke test failed\n\n");
    }
    return pass ? 0 : 1;
}
