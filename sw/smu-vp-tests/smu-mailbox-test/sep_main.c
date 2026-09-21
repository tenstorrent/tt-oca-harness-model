/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * SEP half of the SEP->SMC mailbox interrupt test.
 *
 * Waits for the SMC to arm channel 0's inbound side, pushes a payload into the
 * outbound FIFO, and then waits for the SMC's reply on the reverse direction.
 * Writing outbound WRITE_DATA is what raises outbound_interrupt_o towards the
 * SMC (sep.sv: smc_mailbox_interrupt_o).
 */
#include <stdint.h>

extern int printf(const char *format, ...);

/* Two-word handshake: the SEP publishes READY first so the SMC cannot set GO
 * before the SEP has initialised the cell it polls. */
#define SEP_SRAM_BASE        0x1000A000u
#define HS_READY             (SEP_SRAM_BASE + 0x0u)
#define HS_GO                (SEP_SRAM_BASE + 0x4u)
#define READY_TOKEN          0x5E9EAD00u
#define GO_TOKEN             0xA11CED00u

#define MBOX_BASE            0x10A00000u
#define MBOX_CHANNEL_STRIDE  0x1000u
#define MBOX_INBOUND_OFFSET  0x800u
#define MBOX_OUTBOUND(ch)    (MBOX_BASE + (ch) * MBOX_CHANNEL_STRIDE)
#define MBOX_INBOUND(ch)     (MBOX_OUTBOUND(ch) + MBOX_INBOUND_OFFSET)

#define MBOX_WRITE_DATA      0x00u
#define MBOX_READ_DATA       0x08u
#define MBOX_STATUS          0x10u

#define MBOX_STATUS_EMPTY    (1u << 0)

#define NWORDS               4u
#define PAYLOAD(i)           (0x5E900000u + (i))
#define REPLY                0x5C000001u
#define SPIN_LIMIT           50000000u

#define REG32(addr)          (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU mailbox interrupt test (SEP side) ===\n");

    REG32(HS_GO) = 0u;
    REG32(HS_READY) = READY_TOKEN;

    uint32_t spins = 0;
    while (REG32(HS_GO) != GO_TOKEN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC to arm the mailbox\n");
            return 1;
        }
    }

    for (uint32_t i = 0; i < NWORDS; ++i)
        REG32(MBOX_OUTBOUND(0) + MBOX_WRITE_DATA) = PAYLOAD(i);
    printf("SEP pushed %u words into outbound channel 0\n", NWORDS);

    /* The SMC replies through the inbound side, which pops out here. */
    spins = 0;
    while ((REG32(MBOX_OUTBOUND(0) + MBOX_STATUS) & MBOX_STATUS_EMPTY) != 0u) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for the SMC reply\n");
            return 1;
        }
    }

    const uint32_t got = REG32(MBOX_OUTBOUND(0) + MBOX_READ_DATA);
    if (got != REPLY) {
        printf("FAIL: reply 0x%08x want 0x%08x\n", got, REPLY);
        return 1;
    }

    printf("SEP received SMC reply 0x%08x\n", got);
    printf("PASS: SMU mailbox interrupt test (SEP side)\n");
    return 0;
}
