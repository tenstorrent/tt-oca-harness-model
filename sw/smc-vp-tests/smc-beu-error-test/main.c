/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * sw/smc-vp-tests/smc-beu-error-test/main.c
 *
 * BEU (Bus Error Unit) error-injection + accrual + SW-ack test — Phases E2
 * ("First-error + accrued + SW ack") and E3 ("Per-core isolation") of the BEU
 * platform integration test plan
 * (smc/peripherals/beu/doc/04_BEU_Platform_Integration_Test_Plan.md).
 *
 * Firmware has no way to synthesize a live cache/TileLink ECC event in the
 * VP, so this test relies on the platform's test-only, CCI-driven
 * error-injection hook (Phase D1): three errors are injected directly into
 * named BEU instances at elaboration (before sc_start()), configured by
 * smc_beu_error_test.ini (see that file for the exact core/src/addr triples):
 *
 *   #0: BEU0 <- DCACHE_UNCORRECTABLE (src=7) @ 0x1000_2000
 *   #1: BEU0 <- DCACHE_TLBUS         (src=5) @ 0x3000_4000
 *   #2: BEU1 <- ICACHE_TLBUS         (src=1) @ 0x5000_6000
 *
 * By the time this firmware's first BEU read executes, all three injections
 * have already happened, so the test simply checks the resulting state:
 *
 *   E2 (BEU0, two injections on the same core):
 *     1. ACCRUED has both {DCACHE_UNCORRECTABLE, DCACHE_TLBUS} bits set.
 *     2. CAUSE == DCACHE_UNCORRECTABLE (7) -- the FIRST injected+enabled
 *        error wins; the second injection only accrues, it does not
 *        overwrite CAUSE/PHYS_ADDR.
 *     3. PHYS_ADDR == the first injection's address (0x1000_2000).
 *     4. SW ack: write ACCRUED_ENABLE=0 and CAUSE=0 (ISR epilogue) and
 *        confirm both read back clear -- re-arms the latch for a future
 *        error.
 *
 *   E3 (per-core isolation):
 *     5. BEU1 shows exactly its own injected error (ICACHE_TLBUS / CAUSE=1 /
 *        matching PHYS_ADDR) and nothing from BEU0's injections.
 *     6. BEU2 and BEU3 (no injection at all) read back fully clear --
 *        confirms the beu_router demuxes all NUM_BEU windows independently
 *        and that injecting into one core's model never leaks into another.
 *
 * NMI/PLIC delivery is intentionally NOT exercised here: LOCAL_ENABLE /
 * PLIC_ENABLE are left at their reset value (0) throughout, so the injected,
 * already-accrued errors never assert an interrupt line into the running
 * hart (irq_local_o / irq_plic_o are level signals gated by
 * ACCRUED & {LOCAL,PLIC}_ENABLE -- see beu.h).  Proving end-to-end NMI
 * control transfer (trap vector, mnstatus/mnepc save-restore, and safely
 * resuming this firmware afterwards) needs a dedicated NMI-vector handshake
 * with the cluster and is tracked as a follow-up phase.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

int main(void)
{
    printf("\n=== SMC BEU error-injection + accrual + isolation test ===\n\n");

    int pass = 1;

    /* -------- E2.1/E2.2/E2.3: BEU0 accrual + first-error latch ----------- */
    uint64_t accrued0 = REG_READ64(SMC_BEU_BASE_N(0) + BEU_ACCRUED_ENABLE);
    uint64_t cause0   = REG_READ64(SMC_BEU_BASE_N(0) + BEU_CAUSE);
    uint64_t addr0    = REG_READ64(SMC_BEU_BASE_N(0) + BEU_PHYS_ADDR);
    uint64_t exp_accrued0 = (1ull << BEU_SRC_DCACHE_UNCORRECTABLE) |
                            (1ull << BEU_SRC_DCACHE_TLBUS);

    printf("BEU0: ACCRUED=0x%x CAUSE=0x%x PHYS_ADDR=0x%x\n",
           (uint32_t)accrued0, (uint32_t)cause0, (uint32_t)addr0);

    if (accrued0 != exp_accrued0) {
        printf("  FAIL: BEU0 ACCRUED=0x%x != expected 0x%x\n",
               (uint32_t)accrued0, (uint32_t)exp_accrued0);
        pass = 0;
    }
    if (cause0 != BEU_SRC_DCACHE_UNCORRECTABLE) {
        printf("  FAIL: BEU0 CAUSE=0x%x != expected 0x%x (first injected "
               "error must win)\n",
               (uint32_t)cause0, (uint32_t)BEU_SRC_DCACHE_UNCORRECTABLE);
        pass = 0;
    }
    if (addr0 != 0x10002000ull) {
        printf("  FAIL: BEU0 PHYS_ADDR=0x%x != expected 0x10002000 (must "
               "hold the FIRST injection's address)\n",
               (uint32_t)addr0);
        pass = 0;
    }

    /* -------- E2.4: SW ack (ISR epilogue) clears ACCRUED + re-arms CAUSE -- */
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_ACCRUED_ENABLE, 0ull);
    REG_WRITE64(SMC_BEU_BASE_N(0) + BEU_CAUSE, 0ull);
    uint64_t accrued0_ack = REG_READ64(SMC_BEU_BASE_N(0) + BEU_ACCRUED_ENABLE);
    uint64_t cause0_ack   = REG_READ64(SMC_BEU_BASE_N(0) + BEU_CAUSE);
    printf("BEU0 after ack: ACCRUED=0x%x CAUSE=0x%x\n",
           (uint32_t)accrued0_ack, (uint32_t)cause0_ack);
    if (accrued0_ack != 0 || cause0_ack != 0) {
        printf("  FAIL: BEU0 did not clear after SW ack (ACCRUED=0x%x "
               "CAUSE=0x%x)\n",
               (uint32_t)accrued0_ack, (uint32_t)cause0_ack);
        pass = 0;
    }

    /* -------- E3.1: BEU1 shows exactly its own injected error ------------- */
    uint64_t accrued1 = REG_READ64(SMC_BEU_BASE_N(1) + BEU_ACCRUED_ENABLE);
    uint64_t cause1   = REG_READ64(SMC_BEU_BASE_N(1) + BEU_CAUSE);
    uint64_t addr1    = REG_READ64(SMC_BEU_BASE_N(1) + BEU_PHYS_ADDR);
    printf("BEU1: ACCRUED=0x%x CAUSE=0x%x PHYS_ADDR=0x%x\n",
           (uint32_t)accrued1, (uint32_t)cause1, (uint32_t)addr1);
    if (accrued1 != (1ull << BEU_SRC_ICACHE_TLBUS)) {
        printf("  FAIL: BEU1 ACCRUED=0x%x != expected 0x%x\n",
               (uint32_t)accrued1, (uint32_t)(1u << BEU_SRC_ICACHE_TLBUS));
        pass = 0;
    }
    if (cause1 != BEU_SRC_ICACHE_TLBUS) {
        printf("  FAIL: BEU1 CAUSE=0x%x != expected 0x%x\n",
               (uint32_t)cause1, (uint32_t)BEU_SRC_ICACHE_TLBUS);
        pass = 0;
    }
    if (addr1 != 0x50006000ull) {
        printf("  FAIL: BEU1 PHYS_ADDR=0x%x != expected 0x50006000\n",
               (uint32_t)addr1);
        pass = 0;
    }

    /* -------- E3.2: BEU2/BEU3 got no injection -- fully clear ------------- */
    for (unsigned n = 2; n < 4; n++) {
        uint64_t a = REG_READ64(SMC_BEU_BASE_N(n) + BEU_ACCRUED_ENABLE);
        uint64_t c = REG_READ64(SMC_BEU_BASE_N(n) + BEU_CAUSE);
        uint64_t p = REG_READ64(SMC_BEU_BASE_N(n) + BEU_PHYS_ADDR);
        printf("BEU%u: ACCRUED=0x%x CAUSE=0x%x PHYS_ADDR=0x%x (expect all 0)\n",
               n, (uint32_t)a, (uint32_t)c, (uint32_t)p);
        if (a != 0 || c != 0 || p != 0) {
            printf("  FAIL: BEU%u is not isolated (ACCRUED=0x%x CAUSE=0x%x "
                   "PHYS_ADDR=0x%x)\n",
                   n, (uint32_t)a, (uint32_t)c, (uint32_t)p);
            pass = 0;
        }
    }

    if (pass) {
        printf("\nPASS: BEU error-injection + accrual + isolation test\n\n");
    } else {
        printf("\nFAIL: BEU error-injection + accrual + isolation test "
               "failed\n\n");
    }
    return pass ? 0 : 1;
}
