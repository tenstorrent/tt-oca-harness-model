/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/smc-i2c-loopback-test/main.c
 *
 * I2C0 (controller) -> I2C1 (target) loopback over the SMC fabric.
 *
 * The platform wires I2C0's bus model to I2C1's target back door, so a
 * controller transaction from I2C0 is delivered to I2C1's target FIFOs.
 * This test exercises a controller write (data shows up in I2C1 ACQ FIFO)
 * and a controller read (data sourced from I2C1 target TX FIFO).
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define I2C_TARGET_ADDR 0x50u

static void i2c_wait_idle(uintptr_t base)
{
    /* Poll until the host controller is idle. */
    while ((REG_READ(base + I2C_STATUS) & I2C_STATUS_HOSTIDLE) == 0) { }
}

int main(void)
{
    printf("\n=== SMC I2C0 -> I2C1 loopback test ===\n\n");

    int pass = 1;

    /* Reset FIFOs on both cores. */
    REG_WRITE(SMC_I2C0_BASE + I2C_FIFO_CTRL,
              I2C_FIFO_CTRL_RXRST | I2C_FIFO_CTRL_FMTRST);
    REG_WRITE(SMC_I2C1_BASE + I2C_FIFO_CTRL,
              I2C_FIFO_CTRL_ACQRST | I2C_FIFO_CTRL_TXRST);

    /* Configure I2C1 as target at I2C_TARGET_ADDR. */
    REG_WRITE(SMC_I2C1_BASE + I2C_TARGET_ID,
              (I2C_TARGET_ADDR << 0u) | (0x7Fu << 7u)); /* ADDRESS0 + MASK0 */
    REG_WRITE(SMC_I2C1_BASE + I2C_CTRL, I2C_CTRL_ENABLETARGET);

    /* Configure I2C0 as controller. */
    REG_WRITE(SMC_I2C0_BASE + I2C_CTRL, I2C_CTRL_ENABLEHOST);

    /* ---------- Controller write: I2C0 -> I2C1 target ---------- */
    printf("I2C0 controller write 0xDE 0xAD to target 0x");
    printf("%x", I2C_TARGET_ADDR); printf("\n");

    REG_WRITE(SMC_I2C0_BASE + I2C_FDATA,
              ((I2C_TARGET_ADDR << 1u) | 0u) | I2C_FDATA_START);
    REG_WRITE(SMC_I2C0_BASE + I2C_FDATA, 0xDEu);
    REG_WRITE(SMC_I2C0_BASE + I2C_FDATA, 0xADu | I2C_FDATA_STOP);

    i2c_wait_idle(SMC_I2C0_BASE);

    /* The target ACQ FIFO should contain: Start, 0xDE, 0xAD, Stop. */
    uint32_t acq0 = REG_READ(SMC_I2C1_BASE + I2C_ACQDATA);
    printf("I2C1 ACQ[0] = 0x"); printf("%x", acq0); printf("\n");
    if ((acq0 & 0xFFu) != ((I2C_TARGET_ADDR << 1u) | 0u) ||
        ((acq0 >> 8u) & 0x7u) != 0x1u) { /* SIGNAL = Start */
        pass = 0;
    }

    uint32_t acq1 = REG_READ(SMC_I2C1_BASE + I2C_ACQDATA);
    printf("I2C1 ACQ[1] = 0x"); printf("%x", acq1); printf("\n");
    if ((acq1 & 0xFFu) != 0xDEu) {
        pass = 0;
    }

    uint32_t acq2 = REG_READ(SMC_I2C1_BASE + I2C_ACQDATA);
    printf("I2C1 ACQ[2] = 0x"); printf("%x", acq2); printf("\n");
    if ((acq2 & 0xFFu) != 0xADu) {
        pass = 0;
    }

    uint32_t acq3 = REG_READ(SMC_I2C1_BASE + I2C_ACQDATA);
    printf("I2C1 ACQ[3] = 0x"); printf("%x", acq3); printf("\n");
    if (((acq3 >> 8u) & 0x7u) != 0x2u) { /* SIGNAL = Stop */
        pass = 0;
    }

    /* ---------- Controller read: I2C0 <- I2C1 target ---------- */
    printf("I2C0 controller read 2 bytes from target 0x");
    printf("%x", I2C_TARGET_ADDR); printf("\n");

    /* Load I2C1 target TX FIFO with the bytes to return. */
    REG_WRITE(SMC_I2C1_BASE + I2C_TXDATA, 0xCAu);
    REG_WRITE(SMC_I2C1_BASE + I2C_TXDATA, 0xFEu);

    /* Let loosely-timed host FMT drain observe the TX FIFO fill.  Without a
     * short pause, Whisper can issue the read FMT in the same quantum and
     * target_read() sees an empty TX FIFO (RDATA=0). */
    for (volatile unsigned i = 0; i < 64u; ++i) {
        __asm__ volatile("nop");
    }

    REG_WRITE(SMC_I2C0_BASE + I2C_FDATA,
              ((I2C_TARGET_ADDR << 1u) | 1u) | I2C_FDATA_START);
    REG_WRITE(SMC_I2C0_BASE + I2C_FDATA,
              2u | I2C_FDATA_READB | I2C_FDATA_STOP);

    i2c_wait_idle(SMC_I2C0_BASE);

    uint32_t rdata0 = REG_READ(SMC_I2C0_BASE + I2C_RDATA);
    printf("I2C0 RDATA[0] = 0x"); printf("%x", rdata0); printf("\n");
    if ((rdata0 & 0xFFu) != 0xCAu) {
        pass = 0;
    }

    uint32_t rdata1 = REG_READ(SMC_I2C0_BASE + I2C_RDATA);
    printf("I2C0 RDATA[1] = 0x"); printf("%x", rdata1); printf("\n");
    if ((rdata1 & 0xFFu) != 0xFEu) {
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: I2C0 -> I2C1 loopback write and read work\n\n");
    } else {
        printf("\nFAIL: I2C0 -> I2C1 loopback mismatch\n\n");
    }
    return 0;
}
