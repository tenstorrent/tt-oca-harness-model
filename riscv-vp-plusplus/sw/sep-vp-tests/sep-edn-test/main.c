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
/* EDN Base Address (DRBG_EDN_BASE from sep_crypto_pkg.sv)             */
/* ================================================================== */
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
    uart_print("  ");
    uart_print(desc);
    uart_print(": actual=0x");
    uart_print_hex(actual, 8);
    uart_print(", expected=0x");
    uart_print_hex(expected, 8);

    if (actual == expected) {
        uart_print(" [PASS]\n");
        pass_count++;
    } else {
        uart_print(" [FAIL]\n");
        fail_count++;
    }
}

/* ================================================================== */
/* test_edn_module: EDN register and basic command test                */
/* ================================================================== */
static void test_edn_module(void)
{
    uint32_t val;

    uart_print("\n=== EDN Register and Command Test ===\n\n");

    /* ---- Step 1: Verify reset values ---- */
    uart_print("Step 1: Verify reset values\n");

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

    /* ---- Step 2: Enable EDN in SW Port Mode ---- */
    uart_print("\nStep 2: Enable EDN in SW Port Mode\n");

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
    uart_print("\nStep 3: Check SW_CMD_STS after enable\n");
    val = REG_READ(EDN_SW_CMD_STS);
    uart_print("  SW_CMD_STS = 0x");
    uart_print_hex(val, 8);
    uart_print("\n");

    /* CMD_REG_RDY (bit 0) and CMD_RDY (bit 1) should be set */
    if ((val & STS_CMD_REG_RDY_BIT) && (val & STS_CMD_RDY_BIT)) {
        uart_print("  CMD_REG_RDY and CMD_RDY set [PASS]\n");
        pass_count++;
    } else {
        uart_print("  CMD_REG_RDY or CMD_RDY not set [FAIL]\n");
        fail_count++;
    }

    /* ---- Step 4: Issue Instantiate command via SW_CMD_REQ ---- */
    uart_print("\nStep 4: Issue Instantiate command\n");

    /* Build CSRNG command header: acmd=1 (instantiate), clen=0, flag0=1 */
    uint32_t cmd = CSRNG_CMD_INSTANTIATE | (0 << 4) | (1 << 8);
    REG_WRITE(EDN_SW_CMD_REQ, cmd);
    delay(500);

    /* ---- Step 5: Poll SW_CMD_STS for CMD_ACK ---- */
    uart_print("\nStep 5: Poll for command acknowledgment\n");
    int timeout = 1000;
    do {
        delay(50);
        val = REG_READ(EDN_SW_CMD_STS);
        timeout--;
    } while (!(val & STS_CMD_ACK_BIT) && timeout > 0);

    if (val & STS_CMD_ACK_BIT) {
        uart_print("  CMD_ACK received [PASS]\n");
        pass_count++;
    } else {
        uart_print("  CMD_ACK timeout [FAIL]\n");
        fail_count++;
    }

    uart_print("  Final SW_CMD_STS = 0x");
    uart_print_hex(val, 8);
    uart_print("\n");

    /* ---- Step 6: Check CMD_STS field for success ---- */
    uart_print("\nStep 6: Check CMD_STS (should be 0 for success)\n");
    uint32_t cmd_sts = (val >> 3) & 0x7;
    check("CMD_STS", cmd_sts, 0);

    /* ---- Step 7: Disable EDN ---- */
    uart_print("\nStep 7: Disable EDN\n");
    ctrl_val = (MUBI4_FALSE << 0) |   /* EDN_ENABLE disabled */
               (MUBI4_FALSE << 4) |
               (MUBI4_FALSE << 8) |
               (MUBI4_FALSE << 12);
    REG_WRITE(EDN_CTRL, ctrl_val);
    delay(100);

    val = REG_READ(EDN_CTRL);
    check("CTRL after disable", val, ctrl_val);

    /* ---- Summary ---- */
    uart_print("\n========================================\n");
    uart_print("EDN Test Summary\n");
    uart_print("  Passed: ");
    uart_print_dec(pass_count);
    uart_print("\n  Failed: ");
    uart_print_dec(fail_count);
    uart_print("\n========================================\n");

    if (fail_count == 0) {
        uart_print("\n*** All EDN checks PASSED! ***\n\n");
        console_print("\n*** All EDN checks PASSED! ***\n\n");
    } else {
        uart_print("\n*** Some EDN checks FAILED! ***\n\n");
        console_print("\n*** Some EDN checks FAILED! ***\n\n");
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
