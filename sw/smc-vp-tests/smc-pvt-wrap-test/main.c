/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/smc-pvt-wrap-test/main.c
 *
 * Basic PVT wrapper register access test on the SMC platform.
 *
 * Exercises the modeled PVT wrapper through the SMC fabric: enables the
 * process monitor, programs a short reference-clock period, polls for the
 * clock-count-valid status, and checks the voltage/temperature status fields.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

int main(void)
{
    printf("\n=== SMC PVT wrapper test ===\n\n");

    int pass = 1;

    /* Reset defaults: all control registers should read 0 at reset. */
    if (REG_READ(SMC_PVT_WRAP_BASE + PVT_PROCESS_CTRL) != 0) {
        printf("FAIL: PROCESS_CTRL not 0 after reset\n");
        pass = 0;
    }
    if (REG_READ(SMC_PVT_WRAP_BASE + PVT_VOLTAGE_CTRL) != 0) {
        printf("FAIL: VOLTAGE_CTRL not 0 after reset\n");
        pass = 0;
    }
    if (REG_READ(SMC_PVT_WRAP_BASE + PVT_TEMP_CTRL) != 0) {
        printf("FAIL: TEMP_CTRL not 0 after reset\n");
        pass = 0;
    }

    /* Enable process clock counting with a short period. */
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_REF_CLK_PERIOD_LO, 5u);
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_REF_CLK_PERIOD_HI, 0u);
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_PROCESS_CTRL, PVT_PROCESS_COUNT_EN);

    /* Poll for the process clock count to become valid. */
    int timeout = 1000;
    while ((REG_READ(SMC_PVT_WRAP_BASE + PVT_PROCESS_STATUS) & PVT_STATUS_VALID) == 0) {
        if (--timeout == 0) {
            printf("FAIL: PROCESS_STATUS.valid did not set\n");
            pass = 0;
            break;
        }
    }

    if (timeout != 0) {
        uint32_t count_lo = REG_READ(SMC_PVT_WRAP_BASE + PVT_PROCESS_CLOCK_LO);
        uint32_t count_hi = REG_READ(SMC_PVT_WRAP_BASE + PVT_PROCESS_CLOCK_HI);
        printf("PROCESS_CLOCK_COUNTER = 0x");
        printf("%x", count_hi);
        printf("%08x", count_lo);
        printf("\n");
        if (count_hi != 0 || count_lo < 5u) {
            printf("FAIL: process clock counter too small\n");
            pass = 0;
        }
    }

    /* Enable voltage and temperature sensors; check status fields. */
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_VOLTAGE_CTRL, PVT_VOLTAGE_RESET_N);
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_TEMP_CTRL, PVT_TEMP_EN);

    uint32_t volt_status = REG_READ(SMC_PVT_WRAP_BASE + PVT_VOLTAGE_STATUS);
    printf("VOLTAGE_STATUS = 0x"); printf("%x", volt_status); printf("\n");
    if ((volt_status & PVT_STATUS_VALID) == 0) {
        printf("FAIL: voltage_ready not set\n");
        pass = 0;
    }

    uint32_t temp_status = REG_READ(SMC_PVT_WRAP_BASE + PVT_TEMP_STATUS);
    printf("TEMP_STATUS = 0x"); printf("%x", temp_status); printf("\n");
    if ((temp_status & PVT_STATUS_VALID) == 0) {
        printf("FAIL: temp_ready not set\n");
        pass = 0;
    }

    uint32_t temp_interrupt = REG_READ(SMC_PVT_WRAP_BASE + PVT_TEMP_INTERRUPT);
    printf("TEMP_INTERRUPT = 0x"); printf("%x", temp_interrupt); printf("\n");
    if ((temp_interrupt & PVT_STATUS_VALID) == 0) {
        printf("FAIL: temp_interrupt not set\n");
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: PVT wrapper register access works\n\n");
    } else {
        printf("\nFAIL: PVT wrapper test failed\n\n");
    }
    return 0;
}
