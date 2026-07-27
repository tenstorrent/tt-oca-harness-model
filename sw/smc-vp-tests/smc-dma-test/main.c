/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/smc-dma-test/main.c
 *
 * DMA scratchpad-to-scratchpad copy over the SMC fabric.
 *
 * The firmware writes a pattern to the scratchpad, programs the SMC DMA to copy
 * it to a second scratchpad region, starts the transfer by reading
 * DMA_NEXT_ID_0, and verifies the destination.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define DMA_SRC   (SMC_SCRATCH_BASE)
#define DMA_DST   (SMC_SCRATCH_BASE + 0x1000ULL)
#define DMA_LEN   64

#define DMA_STATUS_BUSY 0x1u

static void dma_write64(uintptr_t off, uint64_t val)
{
    REG_WRITE(SMC_DMA_BASE + off, (uint32_t)(val & 0xFFFFFFFFULL));
    REG_WRITE(SMC_DMA_BASE + off + 4, (uint32_t)(val >> 32));
}

static void dma_wait_idle(void)
{
    while ((REG_READ(SMC_DMA_BASE + DMA_STATUS_0) & DMA_STATUS_BUSY) != 0) { }
}

int main(void)
{
    printf("\n=== SMC DMA scratchpad copy test ===\n\n");

    int pass = 1;

    /* Fill source with 64-bit pattern. */
    volatile uint64_t *src = (volatile uint64_t *)(uintptr_t)DMA_SRC;
    volatile uint64_t *dst = (volatile uint64_t *)(uintptr_t)DMA_DST;
    for (unsigned i = 0; i < DMA_LEN / sizeof(uint64_t); ++i) {
        src[i] = 0xAABBCCDD00000000ULL + i;
        dst[i] = 0ULL;
    }

    /* Verify the DMA register interface is reachable. */
    REG_WRITE(SMC_DMA_BASE + DMA_CONFIG, 0xDEADBEEFu);
    uint32_t config_r = REG_READ(SMC_DMA_BASE + DMA_CONFIG);
    printf("DMA CONFIG write 0xdeadbeef read 0x");
    printf("%x", config_r);
    printf("\n");
    if (config_r != 0xDEADBEEFu) {
        pass = 0;
    }

    /* Program the DMA. */
    dma_write64(DMA_SRC_ADDRESS_LO, DMA_SRC);
    dma_write64(DMA_DST_ADDRESS_LO, DMA_DST);
    dma_write64(DMA_LENGTH_LO, DMA_LEN);
    dma_write64(DMA_SRC_STRIDE_LO, 0);
    dma_write64(DMA_DST_STRIDE_LO, 0);
    dma_write64(DMA_NUM_REPETITIONS_LO, 0);
    REG_WRITE(SMC_DMA_BASE + DMA_CONFIG, 0);

    printf("DMA LENGTH_LO = ");
    printf("%u", REG_READ(SMC_DMA_BASE + DMA_LENGTH_LO));
    printf("\n");

    printf("Starting DMA transfer: 0x");
    printf("%x", (uint32_t)(DMA_SRC >> 32));
    printf("%08x", (uint32_t)DMA_SRC);
    printf(" -> 0x");
    printf("%x", (uint32_t)(DMA_DST >> 32));
    printf("%08x", (uint32_t)DMA_DST);
    printf(" len=");
    printf("%u", DMA_LEN);
    printf("\n");

    uint32_t id = REG_READ(SMC_DMA_BASE + DMA_NEXT_ID_0);
    printf("DMA start id = ");
    printf("%u", id);
    printf("\n");
    if (id == 0) {
        pass = 0;
    }

    dma_wait_idle();

    uint32_t done = REG_READ(SMC_DMA_BASE + DMA_DONE_0);
    printf("DMA done count = ");
    printf("%u", done);
    printf("\n");
    if (done != 1) {
        pass = 0;
    }

    /* Verify destination. */
    for (unsigned i = 0; i < DMA_LEN / sizeof(uint64_t); ++i) {
        uint64_t expected = 0xAABBCCDD00000000ULL + i;
        if (dst[i] != expected) {
            printf("mismatch at word ");
            printf("%u", i);
            printf(": expected 0x");
            printf("%x", (uint32_t)(expected >> 32));
            printf("%08x", (uint32_t)expected);
            printf(" got 0x");
            printf("%x", (uint32_t)(dst[i] >> 32));
            printf("%08x", (uint32_t)dst[i]);
            printf("\n");
            pass = 0;
            break;
        }
    }

    if (pass) {
        printf("\nPASS: DMA scratchpad copy works\n\n");
    } else {
        printf("\nFAIL: DMA scratchpad copy mismatch\n\n");
    }
    return 0;
}
