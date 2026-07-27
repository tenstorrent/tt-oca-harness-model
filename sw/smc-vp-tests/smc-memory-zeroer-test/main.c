/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/smc-memory-zeroer-test/main.c
 *
 * memory_zeroer (AXI zeroer) integration test over the SMC fabric.
 *
 * The CPU programs the zeroer's CSRs (routed through the fabric's
 * `to_data_accel_ctrl` port), triggers a DMA zero-fill, and the zeroer's
 * DMA master writes zeros back into the scratchpad RAM through the fabric's
 * `data_accel_in` port.  The CPU then reads the region back to confirm it was
 * cleared.
 *
 * Requires chunk_size = 8 (see smc_memory_zeroer_test.ini) so each DMA burst
 * is a naturally aligned 8-byte write, which the scratchpad RAM model accepts
 * (it supports only 1/2/4/8-byte aligned accesses).
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

/* Region of scratchpad the zeroer will clear (8 x 64-bit words = 64 bytes). */
#define ZERO_TARGET  SMC_SCRATCH_BASE
#define ZERO_WORDS   8u
#define ZERO_BYTES   (ZERO_WORDS * 8u)
#define FILL_PATTERN 0xA5A5A5A5DEADBEEFull

int main(void)
{
    printf("\n=== SMC memory_zeroer test ===\n\n");

    int pass = 1;

    /* -------- 1. CSR read/write-back (DEST_ADDR, SIZE are plain RW) -------- */
    REG_WRITE64(SMC_ZEROER_BASE + ZEROER_DEST_ADDR, 0x00000000CAFEF00Dull);
    REG_WRITE64(SMC_ZEROER_BASE + ZEROER_SIZE,      0x0000000000001234ull);

    uint64_t rb_dest = REG_READ64(SMC_ZEROER_BASE + ZEROER_DEST_ADDR);
    uint64_t rb_size = REG_READ64(SMC_ZEROER_BASE + ZEROER_SIZE);
    printf("CSR readback: DEST_ADDR=0x%x SIZE=0x%x\n",
           (uint32_t)rb_dest, (uint32_t)rb_size);
    if (rb_dest != 0x00000000CAFEF00Dull || rb_size != 0x1234ull) {
        printf("  FAIL: CSR read/write-back mismatch\n");
        pass = 0;
    }

    /* -------- 2. Seed the target region with a non-zero pattern ----------- */
    for (unsigned i = 0; i < ZERO_WORDS; i++) {
        REG_WRITE64(ZERO_TARGET + 8u * i, FILL_PATTERN);
    }
    /* Confirm the seed actually landed (sanity on the readback path). */
    for (unsigned i = 0; i < ZERO_WORDS; i++) {
        if (REG_READ64(ZERO_TARGET + 8u * i) != FILL_PATTERN) {
            printf("  FAIL: seed word %u did not stick\n", i);
            pass = 0;
        }
    }
    printf("seeded 0x%x bytes at 0x%x with pattern 0x%08x%08x\n",
           ZERO_BYTES, (uint32_t)ZERO_TARGET,
           (uint32_t)(FILL_PATTERN >> 32), (uint32_t)FILL_PATTERN);

    /* -------- 3. Program + trigger the zero-fill job ---------------------- */
    REG_WRITE64(SMC_ZEROER_BASE + ZEROER_DEST_ADDR, (uint64_t)ZERO_TARGET);
    REG_WRITE64(SMC_ZEROER_BASE + ZEROER_SIZE,      (uint64_t)ZERO_BYTES);
    /* Writing CTRL_STATUS with int_en set kicks the job.  The model is
     * loosely-timed and blocking, so the store returns only after the DMA
     * has finished writing every chunk. */
    REG_WRITE64(SMC_ZEROER_BASE + ZEROER_CTRL_STATUS, ZEROER_CTRL_INT_EN);

    /* -------- 4. Verify the region is now zero ---------------------------- */
    for (unsigned i = 0; i < ZERO_WORDS; i++) {
        uint64_t v = REG_READ64(ZERO_TARGET + 8u * i);
        if (v != 0ull) {
            printf("  FAIL: word %u = 0x%08x%08x (expected 0)\n",
                   i, (uint32_t)(v >> 32), (uint32_t)v);
            pass = 0;
        }
    }
    printf("post-zero check: read back 0x%x bytes\n", ZERO_BYTES);

    /* -------- 5. CTRL_STATUS: busy cleared, int_en latched ---------------- */
    uint64_t ctrl = REG_READ64(SMC_ZEROER_BASE + ZEROER_CTRL_STATUS);
    printf("CTRL_STATUS = 0x%08x%08x (int_en=%u busy=%u)\n",
           (uint32_t)(ctrl >> 32), (uint32_t)ctrl,
           (unsigned)((ctrl & ZEROER_CTRL_INT_EN) != 0),
           (unsigned)((ctrl & ZEROER_CTRL_BUSY) != 0));
    if ((ctrl & ZEROER_CTRL_BUSY) != 0) {
        printf("  FAIL: busy still set after completion\n");
        pass = 0;
    }
    if ((ctrl & ZEROER_CTRL_INT_EN) == 0) {
        printf("  FAIL: int_en did not latch\n");
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: memory_zeroer CSR + DMA zero-fill work\n\n");
    } else {
        printf("\nFAIL: memory_zeroer test failed\n\n");
    }
    return 0;
}
