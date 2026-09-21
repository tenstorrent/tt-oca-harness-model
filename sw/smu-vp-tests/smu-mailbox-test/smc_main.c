/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * SMC half of the SEP->SMC mailbox interrupt test.
 *
 * Covers the path RTL builds as
 *   axil_mailbox.outbound_interrupt_o -> sep.sv smc_mailbox_interrupt_o
 *   -> smu.sv sep_mailbox_interrupts -> smc_peripherals.sv
 *      peripheral_interrupts_o[7:0] -> PLIC source 257 + channel.
 *
 * The SMC arms channel 0's outbound write-threshold (that pin is what now
 * leaves the SEP towards the SMC), tells the SEP to push, then checks that
 * the interrupt becomes pending at the PLIC, that the payload reads back
 * through inbound READ_DATA, and that acknowledging IRQS drops the line.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

/* SEP-local addresses seen through the SMU crossbar's SEP aperture
 * (smu_xbar.sep_global_base = 0x5000_0000). */
#define SEP_APERTURE         0x50000000ULL
#define SEP_SRAM_BASE        (SEP_APERTURE + 0x1000A000ULL)
/* Two-word handshake; the SEP publishes READY before the SMC may set GO. */
#define HS_READY             (SEP_SRAM_BASE + 0x0ULL)
#define HS_GO                (SEP_SRAM_BASE + 0x4ULL)
#define READY_TOKEN          0x5E9EAD00u
#define GO_TOKEN             0xA11CED00u

#define MBOX_BASE            (SEP_APERTURE + 0x10A00000ULL)
#define MBOX_CHANNEL_STRIDE  0x1000ULL
#define MBOX_INBOUND_OFFSET  0x800ULL
#define MBOX_OUTBOUND(ch)    (MBOX_BASE + (ch) * MBOX_CHANNEL_STRIDE)
#define MBOX_INBOUND(ch)     (MBOX_OUTBOUND(ch) + MBOX_INBOUND_OFFSET)

#define MBOX_WRITE_DATA      0x00u
#define MBOX_READ_DATA       0x08u
#define MBOX_STATUS          0x10u
#define MBOX_WIRQT           0x20u
#define MBOX_RIRQT           0x28u
#define MBOX_IRQS            0x30u
#define MBOX_IRQEN           0x38u
#define MBOX_IRQP            0x40u

#define MBOX_IRQ_WRITE_THRESHOLD (1u << 0)
#define MBOX_IRQ_READ_THRESHOLD  (1u << 1)

#define NWORDS               4u
#define PAYLOAD(i)           (0x5E900000u + (i))
#define REPLY                0x5C000001u
#define SPIN_LIMIT           200000u

#define REG32(addr)          (*(volatile uint32_t *)(uintptr_t)(addr))

static int plic_pending(unsigned source)
{
    const unsigned word = source / 32u;
    const unsigned bit  = source % 32u;
    return (REG_READ(SMC_PLIC_BASE + PLIC_PENDING(word)) & (1u << bit)) != 0;
}

static int wait_plic_pending(unsigned source)
{
    for (uint32_t i = 0; i < SPIN_LIMIT; ++i) {
        if (plic_pending(source))
            return 1;
    }
    return 0;
}

int main(void)
{
    printf("\n=== SMU mailbox interrupt test (SMC side) ===\n");

    const uint64_t inbound  = MBOX_INBOUND(0);
    const uint64_t outbound = MBOX_OUTBOUND(0);

    uint32_t spins = 0;
    while (REG32(HS_READY) != READY_TOKEN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for the SEP to come up\n");
            return 1;
        }
    }

    /* Arm channel 0's outbound write-threshold: SEP's outbound push raises
     * outbound_interrupt_o, which sep.sv now routes to the SMC PLIC. Payload
     * still pops from the inbound READ_DATA port. */
    REG32(outbound + MBOX_WIRQT) = 0u;
    REG32(outbound + MBOX_IRQS)  = 0x7u;      /* W1C — start clean */
    REG32(outbound + MBOX_IRQEN) = MBOX_IRQ_WRITE_THRESHOLD;

    if (plic_pending(SEP_MAILBOX_0_INTERRUPT_ID)) {
        printf("FAIL: PLIC source %u pending before the SEP pushed anything\n",
               SEP_MAILBOX_0_INTERRUPT_ID);
        return 1;
    }

    REG32(HS_GO) = GO_TOKEN;
    printf("SMC armed outbound channel 0 and released the SEP\n");

    if (!wait_plic_pending(SEP_MAILBOX_0_INTERRUPT_ID)) {
        printf("FAIL: PLIC source %u never went pending (IRQS=0x%x IRQP=0x%x)\n",
               SEP_MAILBOX_0_INTERRUPT_ID,
               REG32(outbound + MBOX_IRQS), REG32(outbound + MBOX_IRQP));
        return 1;
    }
    printf("PLIC source %u pending from mailbox channel 0\n",
           SEP_MAILBOX_0_INTERRUPT_ID);

    if ((REG32(outbound + MBOX_IRQP) & MBOX_IRQ_WRITE_THRESHOLD) == 0u) {
        printf("FAIL: IRQP write-threshold bit clear while the PLIC sees the line\n");
        return 1;
    }

    for (uint32_t i = 0; i < NWORDS; ++i) {
        const uint32_t got  = REG32(inbound + MBOX_READ_DATA);
        const uint32_t want = PAYLOAD(i);
        if (got != want) {
            printf("FAIL: word %u got 0x%08x want 0x%08x\n", i, got, want);
            return 1;
        }
    }
    printf("SMC drained %u words from inbound channel 0\n", NWORDS);

    /* Draining clears the threshold condition; acknowledge the latched status. */
    REG32(outbound + MBOX_IRQS) = 0x7u;
    if ((REG32(outbound + MBOX_IRQP) & MBOX_IRQ_WRITE_THRESHOLD) != 0u) {
        printf("FAIL: IRQP still asserted after drain and acknowledge\n");
        return 1;
    }
    printf("Mailbox interrupt deasserted after drain + IRQS acknowledge\n");

    /* Reverse direction, so the test also proves SMC writes reach the SEP. */
    REG32(inbound + MBOX_WRITE_DATA) = REPLY;

    printf("PASS: SMU mailbox interrupt test (SMC side)\n");
    return 0;
}
