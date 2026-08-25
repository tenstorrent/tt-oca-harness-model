/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-beu-test/main.c
 *
 * BEU (Bus Error Unit) register smoke test — Phase E1 of the BEU platform
 * integration plan (smc/peripherals/beu/doc/04_BEU_Platform_Integration_Test_Plan.md).
 *
 * This is a minimum-viable platform test: hart 0 drives its own per-core BEU
 * MMIO window (SMC_BEU_BASE_N(0)) through the fabric and confirms:
 *   1. ENABLE resets to 0xE6 (all defined sources {1,2,5,6,7} recording).
 *   2. PLIC_ENABLE / LOCAL_ENABLE reset to 0, and writes are masked to the
 *      valid-source bits (0xE6) even when 0xFF is written.
 *   3. PHYS_ADDR is read-only; writes are silently ignored (WI).
 *   4. CAUSE writes 0 (re-arm) cleanly while already clear (no error latched).
 *   5. Every hart's BEU instance decodes at its own 4 KiB alias (sanity that
 *      beu_router demuxes all NUM_BEU windows, not just hart 0's).
 *
 * No error is injected here — inject_error() is a C++ test-bench back door,
 * not reachable from firmware/MMIO by design (see beu.h). Error-injection /
 * NMI-delivery is exercised by later phases (E2+) via a dedicated hook.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define NUM_BEU_INSTANCES 4u

int main(void)
{
    printf("\n=== SMC BEU (Bus Error Unit) register smoke test ===\n\n");

    int pass = 1;

    /* -------- 1. ENABLE resets to 0xE6 on hart 0's BEU ------------------ */
    uint64_t enable = REG_READ64(SMC_BEU_BASE_N(0) + BEU_ENABLE);
    printf("hart0 BEU ENABLE = 0x%x\n", (uint32_t)enable);
    if (enable != BEU_VALID_MASK) {
        printf("  FAIL: ENABLE reset value 0x%x != 0x%x\n",
               (uint32_t)enable, (uint32_t)BEU_VALID_MASK);
        pass = 0;
    }

    /* -------- 2a. PLIC_ENABLE / LOCAL_ENABLE reset to 0 ------------------ */
    uint64_t plic_en_rst  = REG_READ64(SMC_BEU_BASE_N(0) + BEU_PLIC_ENABLE);
    uint64_t local_en_rst = REG_READ64(SMC_BEU_BASE_N(0) + BEU_LOCAL_ENABLE);
    printf("hart0 BEU PLIC_ENABLE=0x%x LOCAL_ENABLE=0x%x (reset)\n",
           (uint32_t)plic_en_rst, (uint32_t)local_en_rst);
    if (plic_en_rst != 0 || local_en_rst != 0) {
        printf("  FAIL: PLIC_ENABLE/LOCAL_ENABLE did not reset to 0\n");
        pass = 0;
    }

    /* -------- 2b. Write 0xFF, read back masked to 0xE6 ------------------- */
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_PLIC_ENABLE, 0xFFull);
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_LOCAL_ENABLE, 0xFFull);
    uint64_t plic_en  = REG_READ64(SMC_BEU_BASE_N(0) + BEU_PLIC_ENABLE);
    uint64_t local_en = REG_READ64(SMC_BEU_BASE_N(0) + BEU_LOCAL_ENABLE);
    printf("after write 0xff: PLIC_ENABLE=0x%x LOCAL_ENABLE=0x%x\n",
           (uint32_t)plic_en, (uint32_t)local_en);
    if (plic_en != BEU_VALID_MASK || local_en != BEU_VALID_MASK) {
        printf("  FAIL: PLIC_ENABLE/LOCAL_ENABLE not masked to 0x%x\n",
               (uint32_t)BEU_VALID_MASK);
        pass = 0;
    }

    /* -------- 3. PHYS_ADDR is read-only; writes are ignored -------------- */
    uint64_t phys_before = REG_READ64(SMC_BEU_BASE_N(0) + BEU_PHYS_ADDR);
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_PHYS_ADDR, 0xDEADBEEFCAFEULL);
    uint64_t phys_after = REG_READ64(SMC_BEU_BASE_N(0) + BEU_PHYS_ADDR);
    printf("PHYS_ADDR before=0x%x after write-attempt=0x%x\n",
           (uint32_t)phys_before, (uint32_t)phys_after);
    if (phys_after != phys_before) {
        printf("  FAIL: PHYS_ADDR is writable (should be read-only)\n");
        pass = 0;
    }

    /* -------- 4. CAUSE write 0 (re-arm) is a clean no-op while clear ----- */
    uint64_t cause_before = REG_READ64(SMC_BEU_BASE_N(0) + BEU_CAUSE);
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_CAUSE, 0ull);
    uint64_t cause_after = REG_READ64(SMC_BEU_BASE_N(0) + BEU_CAUSE);
    printf("CAUSE before=0x%x after write(0)=0x%x\n",
           (uint32_t)cause_before, (uint32_t)cause_after);
    if (cause_before != 0 || cause_after != 0) {
        printf("  FAIL: CAUSE not clear (before=0x%x after=0x%x)\n",
               (uint32_t)cause_before, (uint32_t)cause_after);
        pass = 0;
    }

    /* -------- 5. Every hart's BEU alias decodes independently ------------ */
    for (unsigned n = 0; n < NUM_BEU_INSTANCES; n++) {
        uint64_t e = REG_READ64(SMC_BEU_BASE_N(n) + BEU_ENABLE);
        printf("BEU%u ENABLE = 0x%x\n", n, (uint32_t)e);
        if (e != BEU_VALID_MASK) {
            printf("  FAIL: BEU%u ENABLE reset value 0x%x != 0x%x\n",
                   n, (uint32_t)e, (uint32_t)BEU_VALID_MASK);
            pass = 0;
        }
    }

    if (pass) {
        printf("\nPASS: BEU register smoke test\n\n");
    } else {
        printf("\nFAIL: BEU register smoke test failed\n\n");
    }
    return pass ? 0 : 1;
}
