/*
 * CSRNG Complete Command Flow Test for SEP Platform
 *
 * Single test exercising: INSTANTIATE, GENERATE, RESEED, UPDATE, UNINSTANTIATE
 * Also verifies reseed counter behavior and genbits non-zero across commands.
 *
 * Flow:
 *   1. Enable CSRNG, set RESEED_INTERVAL = 5
 *   2. INSTANTIATE  (reseed_counter -> 0, verify twice)
 *   3. GENERATE x5  (reseed_counter 1->2->3->4->5, check genbits non-zero)
 *   4. RESEED       (reseed_counter -> 0)
 *   5. Set RESEED_INTERVAL = 3
 *   6. GENERATE x3  (reseed_counter 1->2->3, check genbits non-zero)
 *   7. UPDATE       (reseed_counter stays 3, NOT reset)
 *   8. UNINSTANTIATE
 *
 * Output goes to UART console (TCP:8888) via uart_print.
 * Final pass/fail also printed to VP stdout console.
 */

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* UART print functions (output to TCP:8888 - uart_terminal_client)   */
/* Implemented in uart_io.c                                           */
/* ------------------------------------------------------------------ */
extern void uart_print(const char *str);
extern void uart_print_dec(uint32_t value);
extern void uart_print_hex(uint32_t value, int digits);

/* ------------------------------------------------------------------ */
/* stdout device at 0x80000000 - prints directly to VP console        */
/* Used only for final pass/fail summary so it also appears on stdout */
/* ------------------------------------------------------------------ */
#define STDOUT_ADDR  0x80000000
static void console_putc(char c)
{
    *((volatile uint32_t *)STDOUT_ADDR) = (uint32_t)c;
}

static void console_print(const char *str)
{
    while (*str) {
        console_putc(*str++);
    }
}


/* ================================================================== */
/* CSRNG Base Address                                                  */
/* ================================================================== */
#define CSRNG_BASE                      0x10915000

/* ================================================================== */
/* Full CSRNG Register Map (24 registers)                              */
/* ================================================================== */
/* Offset 0x00 - Interrupt State Register */
#define CSRNG_INTR_STATE                (CSRNG_BASE + 0x00)
/* Offset 0x04 - Interrupt Enable Register */
#define CSRNG_INTR_ENABLE               (CSRNG_BASE + 0x04)
/* Offset 0x08 - Interrupt Test Register */
#define CSRNG_INTR_TEST                 (CSRNG_BASE + 0x08)
/* Offset 0x0C - Alert Test Register */
#define CSRNG_ALERT_TEST                (CSRNG_BASE + 0x0C)
/* Offset 0x10 - Register Write Enable */
#define CSRNG_REGWEN                    (CSRNG_BASE + 0x10)
/* Offset 0x14 - Control Register (ENABLE, SW_APP_ENABLE, READ_INT_STATE, FIPS_FORCE_ENABLE) */
#define CSRNG_CTRL                      (CSRNG_BASE + 0x14)
/* Offset 0x18 - Command Request Register */
#define CSRNG_CMD_REQ                   (CSRNG_BASE + 0x18)
/* Offset 0x1C - Reseed Interval Register */
#define CSRNG_RESEED_INTERVAL           (CSRNG_BASE + 0x1C)
/* Offset 0x20 - Reseed Counter 0 (SW app instance) */
#define CSRNG_RESEED_COUNTER_0          (CSRNG_BASE + 0x20)
/* Offset 0x24 - Reseed Counter 1 (HW instance 0) */
#define CSRNG_RESEED_COUNTER_1          (CSRNG_BASE + 0x24)
/* Offset 0x28 - Reseed Counter 2 (HW instance 1) */
#define CSRNG_RESEED_COUNTER_2          (CSRNG_BASE + 0x28)
/* Offset 0x2C - SW Command Status Register (CMD_RDY, CMD_ACK, CMD_STS) */
#define CSRNG_SW_CMD_STS                (CSRNG_BASE + 0x2C)
/* Offset 0x30 - Generate Bits Valid Register (GENBITS_VLD, GENBITS_FIPS) */
#define CSRNG_GENBITS_VLD               (CSRNG_BASE + 0x30)
/* Offset 0x34 - Generate Bits Data Register (128-bit, read 4 times) */
#define CSRNG_GENBITS                   (CSRNG_BASE + 0x34)
/* Offset 0x38 - Internal State Read Enable */
#define CSRNG_INT_STATE_READ_ENABLE     (CSRNG_BASE + 0x38)
/* Offset 0x3C - Internal State Read Enable REGWEN */
#define CSRNG_INT_STATE_READ_ENABLE_REGWEN (CSRNG_BASE + 0x3C)
/* Offset 0x40 - Internal State Number Register */
#define CSRNG_INT_STATE_NUM             (CSRNG_BASE + 0x40)
/* Offset 0x44 - Internal State Value Register */
#define CSRNG_INT_STATE_VAL             (CSRNG_BASE + 0x44)
/* Offset 0x48 - FIPS Force Register */
#define CSRNG_FIPS_FORCE                (CSRNG_BASE + 0x48)
/* Offset 0x4C - HW Exception Status Register */
#define CSRNG_HW_EXC_STS               (CSRNG_BASE + 0x4C)
/* Offset 0x50 - Recoverable Alert Status Register */
#define CSRNG_RECOV_ALERT_STS           (CSRNG_BASE + 0x50)
/* Offset 0x54 - Error Code Register */
#define CSRNG_ERR_CODE                  (CSRNG_BASE + 0x54)
/* Offset 0x58 - Error Code Test Register */
#define CSRNG_ERR_CODE_TEST             (CSRNG_BASE + 0x58)
/* Offset 0x5C - Main SM State Register (reset value 0x4E) */
#define CSRNG_MAIN_SM_STATE             (CSRNG_BASE + 0x5C)

/* ================================================================== */
/* Register access helpers                                             */
/* ================================================================== */
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

/* ================================================================== */
/* Command types                                                       */
/* ================================================================== */
#define CMD_INSTANTIATE         0x1
#define CMD_RESEED              0x2
#define CMD_GENERATE            0x3
#define CMD_UPDATE              0x4
#define CMD_UNINSTANTIATE       0x5

/* Build command header: acmd[3:0], clen[7:4], flag0[11:8], glen[23:12] */
#define CMD_HDR(acmd, clen, flag0, glen) \
    (((acmd) & 0xF) | (((clen) & 0xF) << 4) | (((flag0) & 0xF) << 8) | (((glen) & 0xFFF) << 12))

/* MuBi4True = 0x6, MuBi4False = 0x9 */
#define MUBI4_TRUE   0x6
#define MUBI4_FALSE  0x9

/* SW_CMD_STS bit positions */
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
/* Helper: wait until CMD_RDY=1 in SW_CMD_STS (bit 1)                */
/* ------------------------------------------------------------------ */
static void wait_cmd_ready(void)
{
    uint32_t sts;
    int timeout = 10000;
    do {
        delay(50);
        sts = REG_READ(CSRNG_SW_CMD_STS);
        timeout--;
    } while (!(sts & STS_CMD_RDY_BIT) && timeout > 0);

    if (timeout <= 0) {
        uart_print("  [WARN] Timeout waiting for CMD_RDY\n");
    }
}

/* ------------------------------------------------------------------ */
/* Helper: check a condition and print PASS / FAIL to UART            */
/* ------------------------------------------------------------------ */
static void check(const char *desc, uint32_t actual, uint32_t expected)
{
    uart_print("  ");
    uart_print(desc);
    uart_print(": actual=");
    uart_print_dec(actual);
    uart_print(", expected=");
    uart_print_dec(expected);

    if (actual == expected) {
        uart_print(" [PASS]\n");
        pass_count++;
    } else {
        uart_print(" [FAIL]\n");
        fail_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Helper: check non-zero condition and print PASS / FAIL to UART     */
/* ------------------------------------------------------------------ */
static void check_nonzero(const char *desc, uint32_t value)
{
    uart_print("  ");
    uart_print(desc);
    uart_print(": value=");
    uart_print_hex(value, 8);

    if (value != 0) {
        uart_print(" [PASS]\n");
        pass_count++;
    } else {
        uart_print(" (zero!) [FAIL]\n");
        fail_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Helper: read 128-bit GENBITS (4 x 32-bit words)                   */
/* Returns 1 if at least one word is non-zero, 0 otherwise            */
/* ------------------------------------------------------------------ */
static int read_genbits(void)
{
    uint32_t vld = REG_READ(CSRNG_GENBITS_VLD);
    if (!(vld & 0x1)) {
        uart_print("  GENBITS not valid\n");
        return 0;
    }

    uint32_t w[4];
    uint32_t any_nonzero = 0;

    uart_print("  GENBITS: ");
    for (int i = 0; i < 4; i++) {
        w[i] = REG_READ(CSRNG_GENBITS);
        uart_print_hex(w[i], 8);
        uart_print(" ");
        if (w[i] != 0) {
            any_nonzero = 1;
        }
    }
    uart_print("\n");

    return any_nonzero;
}

/* ================================================================== */
/* test_crng_module: complete CSRNG command flow test                  */
/* ================================================================== */
static void test_crng_module(void)
{
    uint32_t rc;
    int genbits_ok;
    int i;

    uart_print("\n=== CSRNG Complete Command Flow Test ===\n\n");

    /* ---- Step 0: Enable CSRNG (ENABLE=0x6, SW_APP_ENABLE=0x6) ---- */
    uart_print("Step 0: Enable CSRNG module\n");
    REG_WRITE(CSRNG_CTRL, (MUBI4_TRUE) | (MUBI4_TRUE << 4));
    delay(200);
    wait_cmd_ready();

    rc = REG_READ(CSRNG_SW_CMD_STS);
    uart_print("  SW_CMD_STS after enable: ");
    uart_print_hex(rc, 8);
    uart_print("\n");

    /* ---- Step 1: Set RESEED_INTERVAL = 5 ---- */
    uart_print("\nStep 1: Set RESEED_INTERVAL = 5\n");
    REG_WRITE(CSRNG_RESEED_INTERVAL, 5);
    delay(50);

    rc = REG_READ(CSRNG_RESEED_INTERVAL);
    check("RESEED_INTERVAL readback", rc, 5);

    /* ---- Step 2: INSTANTIATE (flag0=0x6 entropy mode, clen=0) ---- */
    uart_print("\nStep 2: INSTANTIATE\n");
    wait_cmd_ready();
    REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_INSTANTIATE, 0, MUBI4_TRUE, 0));
    delay(500);
    wait_cmd_ready();

    /* Verify reseed counter = 0 after instantiate (first read) */
    rc = REG_READ(CSRNG_RESEED_COUNTER_0);
    check("Reseed counter after INSTANTIATE (1st read)", rc, 0);

    /* Verify reseed counter = 0 after instantiate (second read to confirm) */
    delay(50);
    rc = REG_READ(CSRNG_RESEED_COUNTER_0);
    check("Reseed counter after INSTANTIATE (2nd read)", rc, 0);

    /* ---- Step 3: GENERATE 5 times (reseed_interval=5) ---- */
    uart_print("\nStep 3: GENERATE x5 (reseed_interval=5)\n");
    for (i = 1; i <= 5; i++) {
        uart_print("  GENERATE #");
        uart_print_dec(i);
        uart_print(" of 5\n");

        wait_cmd_ready();
        REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_GENERATE, 0, 0, 1));
        delay(500);
        wait_cmd_ready();

        /* Read generated data and verify non-zero */
        genbits_ok = read_genbits();
        check_nonzero("  GENBITS non-zero check", (uint32_t)genbits_ok);

        /* Verify reseed counter increments: expected = i */
        rc = REG_READ(CSRNG_RESEED_COUNTER_0);
        uart_print("  Reseed counter check (expect ");
        uart_print_dec(i);
        uart_print("): ");
        check("reseed_counter", rc, i);
    }

    /* ---- Step 4: RESEED (flag0=0x6 entropy mode, clen=0) ---- */
    uart_print("\nStep 4: RESEED\n");
    wait_cmd_ready();
    REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_RESEED, 0, MUBI4_TRUE, 0));
    delay(500);
    wait_cmd_ready();

    /* Verify reseed counter = 0 after reseed (counter is reset) */
    rc = REG_READ(CSRNG_RESEED_COUNTER_0);
    check("Reseed counter after RESEED", rc, 0);

    /* ---- Step 5: Set RESEED_INTERVAL = 3 ---- */
    uart_print("\nStep 5: Set RESEED_INTERVAL = 3\n");
    REG_WRITE(CSRNG_RESEED_INTERVAL, 3);
    delay(50);

    rc = REG_READ(CSRNG_RESEED_INTERVAL);
    check("RESEED_INTERVAL readback", rc, 3);

    /* ---- Step 6: GENERATE 3 times (reseed_interval=3) ---- */
    uart_print("\nStep 6: GENERATE x3 (reseed_interval=3)\n");
    for (i = 1; i <= 3; i++) {
        uart_print("  GENERATE #");
        uart_print_dec(i);
        uart_print(" of 3\n");

        wait_cmd_ready();
        REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_GENERATE, 0, 0, 1));
        delay(500);
        wait_cmd_ready();

        /* Read generated data and verify non-zero */
        genbits_ok = read_genbits();
        check_nonzero("  GENBITS non-zero check", (uint32_t)genbits_ok);

        /* Verify reseed counter increments: expected = i */
        rc = REG_READ(CSRNG_RESEED_COUNTER_0);
        uart_print("  Reseed counter check (expect ");
        uart_print_dec(i);
        uart_print("): ");
        check("reseed_counter", rc, i);
    }

    /* ---- Step 7: UPDATE (clen=1, provide 1 word of additional data) ---- */
    uart_print("\nStep 7: UPDATE (clen=1)\n");
    wait_cmd_ready();
    REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_UPDATE, 1, 0, 0));   /* header with clen=1 */
    delay(50);
    REG_WRITE(CSRNG_CMD_REQ, 0xDEADBEEF);                     /* additional data word */
    delay(500);
    wait_cmd_ready();

    /* Verify reseed counter is still 3 (UPDATE does NOT reset counter) */
    rc = REG_READ(CSRNG_RESEED_COUNTER_0);
    check("Reseed counter after UPDATE (unchanged)", rc, 3);

    /* ---- Step 8: UNINSTANTIATE ---- */
    uart_print("\nStep 8: UNINSTANTIATE\n");
    wait_cmd_ready();
    REG_WRITE(CSRNG_CMD_REQ, CMD_HDR(CMD_UNINSTANTIATE, 0, 0, 0));
    delay(500);
    wait_cmd_ready();

    /* Verify reseed counter = 0 after uninstantiate */
    rc = REG_READ(CSRNG_RESEED_COUNTER_0);
    check("Reseed counter after UNINSTANTIATE", rc, 0);

    /* ---- Summary (print to BOTH UART console and VP stdout) ---- */

    if (fail_count == 0) {
        uart_print("\n*** All checks PASSED! ***\n\n");
        console_print("\n*** All checks PASSED! ***\n\n");
    } else {
        uart_print("\n*** Some checks FAILED! ***\n\n");
        console_print("\n*** Some checks FAILED! ***\n\n");
    }
}

/* ================================================================== */
/* main                                                                */
/* ================================================================== */
int main(void)
{
    test_crng_module();
    return 0;
}

