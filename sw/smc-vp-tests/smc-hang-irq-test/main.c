// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include "smc_common.h"

extern int printf(const char*, ...);

#define CLOCK_GATE_CONTROL 0x18u
#define HANG_SYS_CTRL      0x20u
#define HANG_SYS_THRESHOLD 0x28u
#define HANG_SEP_CTRL      0x30u
#define HANG_SEP_THRESHOLD 0x38u
#define HANG_DATA_CTRL     0x40u
#define HANG_DATA_THRESHOLD 0x48u

#define HANG_ENABLE  (1u << 0)
#define HANG_IRQ_EN  (1u << 4)
#define HANG_IRQ_TEST (1u << 8)
#define HANG_IRQ_STATUS (1u << 12) /* sw=r hw=w: live level of this detector's irq */

int main(void)
{
    int pass = 1;
    const unsigned source = PLIC_SRC_AXI_HANG;
    const unsigned word = source / 32u;
    const uint32_t bit = 1u << (source % 32u);

    printf("\n=== SMC AXI Hang IRQ Firmware Test ===\n");

    if (REG_READ(SMC_BASE_CONFIG_BASE + CLOCK_GATE_CONTROL) != 0x1F000000u)
        pass = 0;
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL) != 0u ||
        REG_READ(SMC_BASE_CONFIG_BASE + HANG_SYS_THRESHOLD) != 0x1000u ||
        REG_READ(SMC_BASE_CONFIG_BASE + HANG_SEP_THRESHOLD) != 0x1000u ||
        REG_READ(SMC_BASE_CONFIG_BASE + HANG_DATA_THRESHOLD) != 0x1000u) {
        printf("FAIL: hang CSR reset values\n");
        pass = 0;
    }

    REG_WRITE(SMC_PLIC_BASE + PLIC_PRIORITY(source), 1u);
    REG_WRITE(SMC_PLIC_BASE + PLIC_ENABLE(0, word), bit);
    REG_WRITE(SMC_PLIC_BASE + PLIC_THRESHOLD(0), 0u);

    /* irq[12] is not writable: a write of only bit 12 reads back 0 and raises nothing. */
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL, HANG_IRQ_STATUS);
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL) != 0u) {
        printf("FAIL: HANG_SYS_CTRL.irq[12] accepted a software write\n");
        pass = 0;
    }

    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL,
              HANG_ENABLE | HANG_IRQ_EN | HANG_IRQ_TEST);

    for (volatile unsigned i = 0; i < 256u; ++i)
        __asm__ volatile("nop");

    /* Status names the detector that is interrupting; the other two stay clear. */
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL) !=
            (HANG_ENABLE | HANG_IRQ_EN | HANG_IRQ_TEST | HANG_IRQ_STATUS) ||
        (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL) & HANG_IRQ_STATUS) != 0u ||
        (REG_READ(SMC_BASE_CONFIG_BASE + HANG_DATA_CTRL) & HANG_IRQ_STATUS) != 0u) {
        printf("FAIL: irq[12] does not identify the SYS detector\n");
        pass = 0;
    }

    if ((REG_READ(SMC_PLIC_BASE + PLIC_PENDING(word)) & bit) == 0u) {
        printf("FAIL: PLIC source 287 did not become pending\n");
        pass = 0;
    }

    uint32_t claim = REG_READ(SMC_PLIC_BASE + PLIC_CLAIM_COMPLETE(0));
    if (claim != source) {
        printf("FAIL: claim=%u expected=%u\n", claim, source);
        pass = 0;
    }

    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL,
              HANG_ENABLE | HANG_IRQ_EN);
    REG_WRITE(SMC_PLIC_BASE + PLIC_CLAIM_COMPLETE(0), source);

    /* A level, not a latch: it drops with irq_test (no real timeout here). */
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SYS_CTRL) != (HANG_ENABLE | HANG_IRQ_EN)) {
        printf("FAIL: irq[12] stayed set after irq_test cleared\n");
        pass = 0;
    }

    if ((REG_READ(SMC_PLIC_BASE + PLIC_PENDING(word)) & bit) != 0u) {
        printf("FAIL: source remained pending after irq_test clear\n");
        pass = 0;
    }

    /* Other detector CSRs are independently writable and mask reserved bits.
     * All-ones sets enable + irq_en + irq_test, so each also reports irq[12]. */
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL, 0xFFFFFFFFu);
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_DATA_CTRL, 0xFFFFFFFFu);
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL) != 0x1111u ||
        REG_READ(SMC_BASE_CONFIG_BASE + HANG_DATA_CTRL) != 0x1111u) {
        printf("FAIL: detector control masks\n");
        pass = 0;
    }
    /* irq_en gates the status the same way it gates the pin. */
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL, HANG_ENABLE | HANG_IRQ_TEST);
    if (REG_READ(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL) != (HANG_ENABLE | HANG_IRQ_TEST)) {
        printf("FAIL: irq[12] set with irq_en clear\n");
        pass = 0;
    }
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_SEP_CTRL, 0u);
    REG_WRITE(SMC_BASE_CONFIG_BASE + HANG_DATA_CTRL, 0u);

    printf(pass ? "PASS: PLIC source 287 and hang CSRs work\n"
                : "FAIL: SMC AXI hang IRQ firmware test\n");
    return 0;
}
