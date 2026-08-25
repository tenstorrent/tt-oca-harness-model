/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smc-vp-tests/smc-pll-wrapper-test/main.c
 *
 * pll_wrapper integration test over the SMC fabric.
 *
 * Mirrors the SMC firmware PLL bring-up
 * (fw/smc/test_sequences/pll_ag_sanity_sequence.h and program_cgm /
 * program_awm*_functional in fw/smc/common/smc_defines.h): program a CGM,
 * strobe REG_UPDATE, then busy-poll pll_cntl.CGM_x_STATUS.lock_detect until
 * the PLL reports lock -- then check the AWM lock aggregation the same way.
 *
 * The model asserts lock synchronously on the REG_UPDATE strobe, so the poll
 * loops terminate immediately.  Firmware accesses cgm/awm and the pll_cntl
 * status registers with 16-bit MMIO and the wide pll_cntl registers with
 * 32-bit; this test does the same (REG_READ16 / REG_WRITE16 vs REG_*).
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

/* Bounded so a broken model fails fast instead of relying on the VP's
 * simulation-time watchdog to end an otherwise-infinite firmware loop. */
#define POLL_LIMIT 100000u

/* Program CGM<id> like the firmware, strobe REG_UPDATE, and poll pll_cntl
 * until lock_detect asserts.  Returns 1 on success, 0 on failure. */
static int cgm_program_and_lock(unsigned id)
{
    const uint64_t cgm    = PLL_CGM_BASE(id);
    const uint64_t status = (id == 0)
                              ? (PLL_CNTL_BASE + PLL_CNTL_CGM_0_STATUS)
                              : (PLL_CNTL_BASE + PLL_CNTL_CGM_1_STATUS);

    if (REG_READ16(status) & PLL_LOCK_DETECT_BIT) {
        printf("  FAIL: CGM%u reports lock before programming\n", id);
        return 0;
    }

    /* enable CGM + frequency acquisition; integer FCW = refclk * 20 */
    REG_WRITE16(cgm + CGM_ENABLES, CGM_ENABLE_BIT | CGM_FREQ_ACQ_BIT);
    REG_WRITE16(cgm + CGM_FCW_INT, 20);
    REG_WRITE16(cgm + CGM_FCW_FRAC, 0);
    REG_WRITE16(cgm + CGM_PREDIV, 0);

    /* push shadow config -- this triggers frequency lock */
    REG_WRITE16(cgm + CGM_REG_UPDATE, 0x1u);

    /* poll for lock (firmware's do/while on lock_detect) */
    unsigned spins = 0;
    while (!(REG_READ16(status) & PLL_LOCK_DETECT_BIT)) {
        if (++spins >= POLL_LIMIT) {
            printf("  FAIL: CGM%u lock_detect never asserted\n", id);
            return 0;
        }
    }

    if (REG_READ16(cgm + CGM_REG_UPDATE) != 0) {
        printf("  FAIL: CGM%u REG_UPDATE did not self-clear\n", id);
        return 0;
    }
    if (!(REG_READ16(cgm + CGM_CGM_STATUS) & PLL_LOCK_DETECT_BIT)) {
        printf("  FAIL: CGM%u sub-block CGM_STATUS not locked\n", id);
        return 0;
    }

    printf("CGM%u locked after %u poll(s) (pll_cntl status=0x%x)\n",
           id, spins + 1u, (unsigned)REG_READ16(status));
    return 1;
}

int main(void)
{
    printf("\n=== SMC pll_wrapper test ===\n\n");

    int pass = 1;

    /* -------- 1. Program + lock both CGMs -------------------------------- */
    pass &= cgm_program_and_lock(0);
    pass &= cgm_program_and_lock(1);

    /* -------- 2. Disable + REG_UPDATE drops CGM0 lock ------------------- */
    REG_WRITE16(PLL_CGM_BASE(0) + CGM_ENABLES, 0x0u);
    REG_WRITE16(PLL_CGM_BASE(0) + CGM_REG_UPDATE, 0x1u);
    if (REG_READ16(PLL_CNTL_BASE + PLL_CNTL_CGM_0_STATUS) & PLL_LOCK_DETECT_BIT) {
        printf("  FAIL: CGM0 still locked after disable\n");
        pass = 0;
    } else {
        printf("CGM0 lock cleared after disable + REG_UPDATE\n");
    }

    /* -------- 3. AWM lock aggregation on pll_cntl ----------------------- */
    /* Firmware commits AWM config with a 32-bit GLOBAL REG_UPDATE store and
     * polls AWM_0_STATUS.lock_detect == 7 (all three CGMs), AWM_1 == 1. */
    REG_WRITE(PLL_AWM_BASE(0) + AWM_GLOBAL_REG_UPDATE, 0x1u);
    unsigned awm0 = REG_READ16(PLL_CNTL_BASE + PLL_CNTL_AWM_0_STATUS) & 0x7u;
    if (awm0 != 0x7u) {
        printf("  FAIL: AWM0 lock_detect=0x%x (expected 0x7)\n", awm0);
        pass = 0;
    }

    REG_WRITE(PLL_AWM_BASE(1) + AWM_GLOBAL_REG_UPDATE, 0x1u);
    unsigned awm1 = REG_READ16(PLL_CNTL_BASE + PLL_CNTL_AWM_1_STATUS) & 0x7u;
    if (awm1 != 0x1u) {
        printf("  FAIL: AWM1 lock_detect=0x%x (expected 0x1)\n", awm1);
        pass = 0;
    }
    if (awm0 == 0x7u && awm1 == 0x1u) {
        printf("AWM lock aggregation OK (AWM0=0x%x, AWM1=0x%x)\n", awm0, awm1);
    }

    /* -------- 4. 32-bit RW register masking (AG_MUX_SELECT) ------------- */
    REG_WRITE(PLL_CNTL_BASE + PLL_CNTL_AG_MUX_SELECT, 0xFFFFFFFFu);
    uint32_t agmux = REG_READ(PLL_CNTL_BASE + PLL_CNTL_AG_MUX_SELECT);
    if (agmux != 0x3F3FFFFFu) {
        printf("  FAIL: AG_MUX_SELECT=0x%x (expected 0x3F3FFFFF)\n", agmux);
        pass = 0;
    } else {
        printf("AG_MUX_SELECT masked write-back OK (0x%x)\n", agmux);
    }

    if (pass) {
        printf("\nPASS: pll_wrapper CGM/AWM lock + register access work\n\n");
    } else {
        printf("\nFAIL: pll_wrapper test failed\n\n");
    }
    return pass ? 0 : 1;
}
