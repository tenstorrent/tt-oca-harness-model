/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-avsbus-test/main.c
 *
 * AVSBus Controller platform smoke test.
 *
 * Hart 0 drives the AVSBus MMIO window (SMC_AVSBUS_BASE = 0xC000_4000)
 * through the fabric and confirms:
 *   1. CFG_0 / CFG_1 / CONFIG / INTERRUPT_MASK match RDL reset values.
 *   2. CFG_0 is writable (masked to [23:0]).
 *   3. A COMMIT_WRITE command completes via the default slave responder:
 *      NORMAL_STATUS.READBACK_HAS_DATA asserts, DEBUG_READBACK peeks,
 *      AVS_READBACK pops an ACK_OK frame with CMD_DATA=0xFFFF.
 *   4. Unmasked READBACK_HAS_DATA raises PLIC source PLIC_SRC_AVSBUS.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define AVS_RB_ACK_SHIFT   30u
#define AVS_RB_ACK_MASK    0x3u
#define AVS_RB_DATA_SHIFT  8u
#define AVS_RB_DATA_MASK   0xFFFFu
#define AVS_RB_ACK_OK      0x0u

static uint32_t avs_read(uint32_t off)
{
    return REG_READ(SMC_AVSBUS_BASE + off);
}

static void avs_write(uint32_t off, uint32_t val)
{
    REG_WRITE(SMC_AVSBUS_BASE + off, val);
}

/* Spin until readback has data, or give up after many polls (each MMIO
 * access advances Whisper/SystemC time past the model's xfer_delay_ns). */
static int wait_readback(void)
{
    for (unsigned i = 0; i < 100000u; ++i) {
        if ((avs_read(AVS_NORMAL_STATUS) & AVS_STATUS_READBACK_HAS_DATA) != 0)
            return 1;
    }
    return 0;
}

static int plic_pending(unsigned source)
{
    unsigned word = source / 32u;
    unsigned bit  = source % 32u;
    uint32_t pend = REG_READ(SMC_PLIC_BASE + PLIC_PENDING(word));
    return (pend & (1u << bit)) != 0;
}

int main(void)
{
    printf("\n=== SMC AVSBus Controller smoke test ===\n\n");

    int pass = 1;

    /* -------- 1. Reset values ------------------------------------------- */
    uint32_t cfg0  = avs_read(AVS_CFG_0);
    uint32_t cfg1  = avs_read(AVS_CFG_1);
    uint32_t cfg   = avs_read(AVS_CONFIG);
    uint32_t mask  = avs_read(AVS_INTERRUPT_MASK);
    printf("reset CFG_0=0x%x CFG_1=0x%x CONFIG=0x%x MASK=0x%x\n",
           cfg0, cfg1, cfg, mask);
    if (cfg0 != AVS_CFG_0_RESET || cfg1 != AVS_CFG_1_RESET ||
        cfg != AVS_CONFIG_RESET || mask != AVS_IRQ_MASK_RESET) {
        printf("  FAIL: unexpected AVSBus reset values\n");
        pass = 0;
    }

    /* -------- 2. CFG_0 RW ----------------------------------------------- */
    avs_write(AVS_CFG_0, 0x000300FFu);
    uint32_t cfg0_rw = avs_read(AVS_CFG_0);
    printf("CFG_0 after write 0x300ff = 0x%x\n", cfg0_rw);
    if (cfg0_rw != 0x000300FFu) {
        printf("  FAIL: CFG_0 RW mismatch\n");
        pass = 0;
    }
    /* Restore default max-retries / resync for the command path. */
    avs_write(AVS_CFG_0, AVS_CFG_0_RESET);

    /* -------- 3. Command -> default slave response ---------------------- */
    avs_write(AVS_INTERRUPT_MASK, 0); /* unmask all sticky IRQs */

    uint32_t cmd = AVS_CMD_PACK(AVS_CMD_COMMIT_WRITE, 0, AVS_CMD_CODE_VOLTAGE,
                                0, 0x03E8u);
    printf("issue CMD = 0x%x\n", cmd);
    avs_write(AVS_CMD, cmd);

    if (!wait_readback()) {
        printf("  FAIL: timed out waiting for READBACK_HAS_DATA\n");
        pass = 0;
    } else {
        uint32_t st  = avs_read(AVS_NORMAL_STATUS);
        uint32_t irq = avs_read(AVS_INTERRUPT);
        printf("status=0x%x irq=0x%x\n", st, irq);
        if ((irq & AVS_IRQ_READBACK_HAS_DATA) == 0) {
            printf("  FAIL: INTERRUPT.READBACK_HAS_DATA not set\n");
            pass = 0;
        }

        /* -------- 4. PLIC pending (edge-latched while irq_o is high) ---- */
        if (!plic_pending(PLIC_SRC_AVSBUS)) {
            printf("  FAIL: PLIC source %u not pending\n", PLIC_SRC_AVSBUS);
            pass = 0;
        } else {
            printf("PLIC source %u pending (avsbus)\n", PLIC_SRC_AVSBUS);
        }

        uint32_t peek = avs_read(AVS_DEBUG_READBACK);
        uint32_t pop  = avs_read(AVS_READBACK);
        uint32_t ack  = (peek >> AVS_RB_ACK_SHIFT) & AVS_RB_ACK_MASK;
        uint32_t data = (peek >> AVS_RB_DATA_SHIFT) & AVS_RB_DATA_MASK;
        printf("DEBUG_READBACK=0x%x READBACK=0x%x ack=%u data=0x%x\n",
               peek, pop, ack, data);
        if (peek != pop) {
            printf("  FAIL: peek != pop\n");
            pass = 0;
        }
        if (ack != AVS_RB_ACK_OK) {
            printf("  FAIL: SlaveAck != ACK_OK\n");
            pass = 0;
        }
        if (data != 0xFFFFu) {
            printf("  FAIL: write response CMD_DATA != 0xFFFF\n");
            pass = 0;
        }
        uint32_t latest = avs_read(AVS_LATEST_SLAVE_SUBFRAME);
        if (latest != pop) {
            printf("  FAIL: LATEST_SLAVE_SUBFRAME mismatch\n");
            pass = 0;
        }
    }

    if (pass) {
        printf("\nPASS: AVSBus Controller smoke test\n\n");
    } else {
        printf("\nFAIL: AVSBus Controller smoke test failed\n\n");
    }
    return pass ? 0 : 1;
}
