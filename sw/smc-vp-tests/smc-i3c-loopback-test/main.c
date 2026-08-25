/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * sw/smc-vp-tests/smc-i3c-loopback-test/main.c
 *
 * I3C0 controller -> echo target over the SMC fabric.
 *
 * The platform attaches a bus model to I3C instance 0 that ACKs dynamic
 * address 0x50 and echoes the last written payload back on a subsequent read.
 * This test writes two bytes, then reads them back through the controller.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define I3C_TARGET_ADDR 0x50u
#define I3C_DEV_INDEX   0u

static void i3c_cmd(uintptr_t base, uint64_t desc)
{
    REG_WRITE(base + I3C_COMMAND_PORT, (uint32_t)(desc & 0xFFFFFFFFu));
    REG_WRITE(base + I3C_COMMAND_PORT, (uint32_t)(desc >> 32u));
}

static void i3c_wait_resp(uintptr_t base)
{
    while ((REG_READ(base + I3C_PIO_INTR_STATUS) & I3C_PIO_INTR_RESP_READY) == 0) { }
}

int main(void)
{
    printf("\n=== SMC I3C0 -> echo target loopback test ===\n\n");

    int pass = 1;

    uintptr_t base = SMC_I3C_BASE;

    /* Reset the controller queues. */
    REG_WRITE(base + I3C_RESET_CONTROL, 0x1Fu); /* all queue resets */

    /* Enable PIO and the bus. */
    REG_WRITE(base + I3C_PIO_CONTROL, I3C_PIO_CONTROL_ENABLE);
    REG_WRITE(base + I3C_HC_CONTROL, I3C_HC_CONTROL_BUS_ENABLE);

    /* DAT[0] dynamic address = 0x50. */
    REG_WRITE(base + I3C_DAT_BASE, (uint32_t)I3C_TARGET_ADDR << 16u);

    /* ---------- Private write: 0xDE 0xAD ---------- */
    uint64_t write_desc =
        ((uint64_t)2u << I3C_CMD_LENGTH_SHIFT) |  /* DATA_LENGTH = 2 */
        ((uint64_t)I3C_DEV_INDEX << I3C_CMD_DEVIDX_SHIFT) |
        ((uint64_t)0u << I3C_CMD_RNW_SHIFT) |     /* write */
        ((uint64_t)1u << I3C_CMD_TID_SHIFT);       /* TID = 1 */

    REG_WRITE(base + I3C_XFER_DATA_PORT, 0x0000ADDEu); /* bytes DE AD 00 00 */
    i3c_cmd(base, write_desc);

    i3c_wait_resp(base);
    uint32_t resp0 = REG_READ(base + I3C_RESPONSE_PORT);
    printf("write response = 0x"); printf("%x", resp0); printf("\n");
    if ((resp0 >> 28u) != 0u) { /* ERROR should be Success */
        pass = 0;
    }

    /* ---------- Private read: 2 bytes ---------- */
    uint64_t read_desc =
        ((uint64_t)2u << I3C_CMD_LENGTH_SHIFT) |
        ((uint64_t)I3C_DEV_INDEX << I3C_CMD_DEVIDX_SHIFT) |
        ((uint64_t)1u << I3C_CMD_RNW_SHIFT) |     /* read */
        ((uint64_t)2u << I3C_CMD_TID_SHIFT);       /* TID = 2 */

    i3c_cmd(base, read_desc);

    i3c_wait_resp(base);
    uint32_t resp1 = REG_READ(base + I3C_RESPONSE_PORT);
    printf("read response = 0x"); printf("%x", resp1); printf("\n");
    if ((resp1 >> 28u) != 0u || ((resp1 >> 16u) & 0xFFFu) != 2u) {
        pass = 0;
    }

    uint32_t rx = REG_READ(base + I3C_XFER_DATA_PORT);
    printf("RX data = 0x"); printf("%x", rx); printf("\n");
    if ((rx & 0xFFu) != 0xDEu || ((rx >> 8u) & 0xFFu) != 0xADu) {
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: I3C0 write/read echo loopback works\n\n");
    } else {
        printf("\nFAIL: I3C0 loopback mismatch\n\n");
    }
    return 0;
}
