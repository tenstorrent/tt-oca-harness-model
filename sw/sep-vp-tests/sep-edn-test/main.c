// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * EDN (Entropy Distribution Network) Register Test for SEP Platform
 *
 * Tests basic register access for the EDN module at 0x10915800.
 * Verifies reset values, CTRL write/readback, and SW command interface.
 *
 * Flow:
 *   1. Read and verify INTR_STATE reset value (0x0)
 *   2. Read and verify REGWEN reset value (0x1)
 *   3. Read and verify CTRL reset value (0x9999)
 *   4. Read and verify BOOT_INS_CMD reset value (0x901)
 *   5. Read and verify BOOT_GEN_CMD reset value (0xFFF003)
 *   6. Enable EDN in SW port mode
 *   7. Write an Instantiate command to SW_CMD_REQ
 *   8. Poll SW_CMD_STS for CMD_ACK
 *   9. Disable EDN
 *
 * Output goes to UART console (TCP:8888) via uart_print.
 * Final pass/fail also printed to VP stdout console.
 */

#include <stdint.h>

extern int printf(const char *format, ...);

/* ================================================================== */
/* EDN Base Address (DRBG_EDN_BASE from sep_crypto_pkg.sv)             */
/* ================================================================== */
#define CSRNG_BASE                      0x10915000
#define CSRNG_CTRL                      (CSRNG_BASE + 0x14)

#define EDN_BASE                        0x10915800

/* ================================================================== */
/* EDN Register Map                                                    */
/* ================================================================== */
#define EDN_INTR_STATE                  (EDN_BASE + 0x00)
#define EDN_INTR_ENABLE                 (EDN_BASE + 0x04)
#define EDN_INTR_TEST                   (EDN_BASE + 0x08)
#define EDN_ALERT_TEST                  (EDN_BASE + 0x0C)
#define EDN_REGWEN                      (EDN_BASE + 0x10)
#define EDN_CTRL                        (EDN_BASE + 0x14)
#define EDN_BOOT_INS_CMD                (EDN_BASE + 0x18)
#define EDN_BOOT_GEN_CMD                (EDN_BASE + 0x1C)
#define EDN_SW_CMD_REQ                  (EDN_BASE + 0x20)
#define EDN_SW_CMD_STS                  (EDN_BASE + 0x24)
#define EDN_HW_CMD_STS                  (EDN_BASE + 0x28)
#define EDN_RESEED_CMD                  (EDN_BASE + 0x2C)
#define EDN_GENERATE_CMD                (EDN_BASE + 0x30)
#define EDN_MAX_NUM_REQS_BETWEEN_RESEEDS (EDN_BASE + 0x34)
#define EDN_RECOV_ALERT_STS             (EDN_BASE + 0x38)
#define EDN_ERR_CODE                    (EDN_BASE + 0x3C)
#define EDN_ERR_CODE_TEST               (EDN_BASE + 0x40)
#define EDN_MAIN_SM_STATE               (EDN_BASE + 0x44)

/* ================================================================== */
/* Register access helpers                                             */
/* ================================================================== */
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

/* MuBi4True = 0x6, MuBi4False = 0x9 */
#define MUBI4_TRUE   0x6
#define MUBI4_FALSE  0x9

/* CSRNG command types */
#define CSRNG_CMD_INSTANTIATE   0x1

/* SW_CMD_STS bit positions */
#define STS_CMD_REG_RDY_BIT     (1 << 0)
#define STS_CMD_RDY_BIT         (1 << 1)
#define STS_CMD_ACK_BIT         (1 << 2)

/* Test counters */
static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

/* ------------------------------------------------------------------ */
/* Helper: busy-wait delay                                            */
/* ------------------------------------------------------------------ */
static void delay(int cycles)
{
    for (volatile int i = 0; i < cycles; i++);
}

/* ------------------------------------------------------------------ */
/* Helper: check a condition and print PASS / FAIL to UART            */
/* ------------------------------------------------------------------ */
static void check(const char *desc, uint32_t actual, uint32_t expected)
{
    printf("  ");
    printf(desc);
    printf(": actual=0x");
    printf("%x", actual);
    printf(", expected=0x");
    printf("%x", expected);

    if (actual == expected) {
        printf(" [PASS]\n");
        pass_count++;
    } else {
        printf(" [FAIL]\n");
        fail_count++;
    }
}

/* ================================================================== */
/* test_edn_module: EDN register and basic command test                */
/* ================================================================== */
static void test_edn_module(void)
{
    uint32_t val;

    printf("\n=== EDN Register and Command Test ===\n\n");

    /* ---- Step 1: Verify reset values ---- */
    printf("Step 1: Verify reset values\n");

    val = REG_READ(EDN_INTR_STATE);
    check("INTR_STATE reset", val, 0x00000000);

    val = REG_READ(EDN_REGWEN);
    check("REGWEN reset", val, 0x00000001);

    val = REG_READ(EDN_CTRL);
    check("CTRL reset", val, 0x00009999);

    val = REG_READ(EDN_BOOT_INS_CMD);
    check("BOOT_INS_CMD reset", val, 0x00000901);

    val = REG_READ(EDN_BOOT_GEN_CMD);
    check("BOOT_GEN_CMD reset", val, 0x00FFF003);

    val = REG_READ(EDN_SW_CMD_STS);
    check("SW_CMD_STS reset", val, 0x00000000);

    val = REG_READ(EDN_HW_CMD_STS);
    check("HW_CMD_STS reset", val, 0x00000000);

    val = REG_READ(EDN_ERR_CODE);
    check("ERR_CODE reset", val, 0x00000000);

    /* CSRNG ENABLE must be set before EDN can instantiate over the HW app port. */
    REG_WRITE(CSRNG_CTRL, (MUBI4_TRUE) | (MUBI4_TRUE << 4));
    delay(200);

    /* ---- Step 2: Enable EDN in SW Port Mode ---- */
    printf("\nStep 2: Enable EDN in SW Port Mode\n");

    /* CTRL format: EDN_ENABLE[3:0]=MuBi4True(0x6),
     *              BOOT_REQ_MODE[7:4]=MuBi4False(0x9),
     *              AUTO_REQ_MODE[11:8]=MuBi4False(0x9),
     *              CMD_FIFO_RST[15:12]=MuBi4False(0x9)
     */
    uint32_t ctrl_val = (MUBI4_TRUE << 0) |   /* EDN_ENABLE */
                        (MUBI4_FALSE << 4) |   /* BOOT_REQ_MODE disabled */
                        (MUBI4_FALSE << 8) |   /* AUTO_REQ_MODE disabled */
                        (MUBI4_FALSE << 12);   /* CMD_FIFO_RST disabled */
    REG_WRITE(EDN_CTRL, ctrl_val);
    delay(200);

    val = REG_READ(EDN_CTRL);
    check("CTRL after enable", val, ctrl_val);

    /* ---- Step 3: SW_CMD_STS should show ready ---- */
    printf("\nStep 3: Check SW_CMD_STS after enable\n");
    val = REG_READ(EDN_SW_CMD_STS);
    printf("  SW_CMD_STS = 0x");
    printf("%x", val);
    printf("\n");

    /* CMD_REG_RDY (bit 0) and CMD_RDY (bit 1) should be set */
    if ((val & STS_CMD_REG_RDY_BIT) && (val & STS_CMD_RDY_BIT)) {
        printf("  CMD_REG_RDY and CMD_RDY set [PASS]\n");
        pass_count++;
    } else {
        printf("  CMD_REG_RDY or CMD_RDY not set [FAIL]\n");
        fail_count++;
    }

    /* ---- Step 4: Issue Instantiate command via SW_CMD_REQ ---- */
    printf("\nStep 4: Issue Instantiate command\n");

    /* Build CSRNG command header: acmd=1 (instantiate), clen=0 (no seed words), flag0=0
     * Header format: acmd[3:0] | clen[11:8] | flag0[12] | glen[30:12]
     * With clen=0 the model expects exactly 1 word (header only) and processes immediately.
     * Previously (1<<8) set clen=1, causing the model to wait for a second data word
     * that was never written — stalling CMD_ACK permanently. */
    uint32_t cmd = CSRNG_CMD_INSTANTIATE;  /* acmd=0x1, clen=0, flag0=0 */
    REG_WRITE(EDN_SW_CMD_REQ, cmd);
    delay(500);

    /* ---- Step 5: Poll SW_CMD_STS for CMD_ACK ---- */
    printf("\nStep 5: Poll for command acknowledgment\n");
    int timeout = 1000;
    do {
        delay(50);
        val = REG_READ(EDN_SW_CMD_STS);
        timeout--;
    } while (!(val & STS_CMD_ACK_BIT) && timeout > 0);

    if (val & STS_CMD_ACK_BIT) {
        printf("  CMD_ACK received [PASS]\n");
        pass_count++;
    } else {
        printf("  CMD_ACK timeout [FAIL]\n");
        fail_count++;
    }

    printf("  Final SW_CMD_STS = 0x");
    printf("%x", val);
    printf("\n");

    /* ---- Step 6: Check CMD_STS field for success ---- */
    printf("\nStep 6: Check CMD_STS (should be 0 for success)\n");
    uint32_t cmd_sts = (val >> 3) & 0x7;
    check("CMD_STS", cmd_sts, 0);

    /* ---- Step 7: Disable EDN ---- */
    printf("\nStep 7: Disable EDN\n");
    ctrl_val = (MUBI4_FALSE << 0) |   /* EDN_ENABLE disabled */
               (MUBI4_FALSE << 4) |
               (MUBI4_FALSE << 8) |
               (MUBI4_FALSE << 12);
    REG_WRITE(EDN_CTRL, ctrl_val);
    delay(100);

    val = REG_READ(EDN_CTRL);
    check("CTRL after disable", val, ctrl_val);

    /* ---- Summary ---- */
    printf("\n========================================\n");
    printf("EDN Test Summary\n");
    printf("  Passed: ");
    printf("%u", pass_count);
    printf("\n  Failed: ");
    printf("%u", fail_count);
    printf("\n========================================\n");

    if (fail_count == 0) {
        printf("\n*** All EDN checks PASSED! ***\n\n");
        printf("\n*** All EDN checks PASSED! ***\n\n");
    } else {
        printf("\n*** Some EDN checks FAILED! ***\n\n");
        printf("\n*** Some EDN checks FAILED! ***\n\n");
    }
}

/* ================================================================== */
/* main                                                                */
/* ================================================================== */
int main(void)
{
    test_edn_module();
    return 0;
}
