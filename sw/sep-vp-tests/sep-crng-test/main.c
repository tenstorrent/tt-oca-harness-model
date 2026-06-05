// /*
//  * CSRNG Complete Command Flow Test for SEP Platform
//  *
//  * Single test exercising: INSTANTIATE, GENERATE, RESEED, UPDATE, UNINSTANTIATE
//  * Also verifies reseed counter behavior and genbits non-zero across commands.
//  *
//  * Flow:
//  *   1. Enable CSRNG, set RESEED_INTERVAL = 5
//  *   2. INSTANTIATE  (reseed_counter -> 0, verify twice)
//  *   3. GENERATE x5  (reseed_counter 1->2->3->4->5, check genbits non-zero)
//  *   4. RESEED       (reseed_counter -> 0)
//  *   5. Set RESEED_INTERVAL = 3
//  *   6. GENERATE x3  (reseed_counter 1->2->3, check genbits non-zero)
//  *   7. UPDATE       (reseed_counter stays 3, NOT reset)
//  *   8. UNINSTANTIATE
//  *
//  * Final pass/fail also printed to VP stdout console.
//  */

#include <stdint.h>

extern int printf(const char *format, ...);

/* ================================================================== */
/* CSRNG Base Address                                                 */
/* ================================================================== */
#define CSRNG_BASE                      0x10915000

/* ================================================================== */
/* Full CSRNG Register Map                                            */
/* ================================================================== */
#define CSRNG_INTR_STATE                (CSRNG_BASE + 0x00)
#define CSRNG_INTR_ENABLE               (CSRNG_BASE + 0x04)
#define CSRNG_INTR_TEST                 (CSRNG_BASE + 0x08)
#define CSRNG_ALERT_TEST                (CSRNG_BASE + 0x0C)
#define CSRNG_REGWEN                    (CSRNG_BASE + 0x10)
#define CSRNG_CTRL                      (CSRNG_BASE + 0x14)
#define CSRNG_CMD_REQ                   (CSRNG_BASE + 0x18)
#define CSRNG_RESEED_INTERVAL           (CSRNG_BASE + 0x1C)
#define CSRNG_RESEED_COUNTER_0          (CSRNG_BASE + 0x20)
#define CSRNG_RESEED_COUNTER_1          (CSRNG_BASE + 0x24)
#define CSRNG_RESEED_COUNTER_2          (CSRNG_BASE + 0x28)
#define CSRNG_SW_CMD_STS                (CSRNG_BASE + 0x2C)
#define CSRNG_GENBITS_VLD               (CSRNG_BASE + 0x30)
#define CSRNG_GENBITS                   (CSRNG_BASE + 0x34)
#define CSRNG_INT_STATE_READ_ENABLE     (CSRNG_BASE + 0x38)
#define CSRNG_INT_STATE_READ_ENABLE_REGWEN (CSRNG_BASE + 0x3C)
#define CSRNG_INT_STATE_NUM             (CSRNG_BASE + 0x40)
#define CSRNG_INT_STATE_VAL             (CSRNG_BASE + 0x44)
#define CSRNG_FIPS_FORCE                (CSRNG_BASE + 0x48)
#define CSRNG_HW_EXC_STS                (CSRNG_BASE + 0x4C)
#define CSRNG_RECOV_ALERT_STS           (CSRNG_BASE + 0x50)
#define CSRNG_ERR_CODE                  (CSRNG_BASE + 0x54)
#define CSRNG_ERR_CODE_TEST             (CSRNG_BASE + 0x58)
#define CSRNG_MAIN_SM_STATE             (CSRNG_BASE + 0x5C)

/* ================================================================== */
/* Register access helpers                                            */
/* ================================================================== */
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

/* ================================================================== */
/* Command types                                                      */
/* ================================================================== */
#define CMD_INSTANTIATE         0x1
#define CMD_RESEED              0x2
#define CMD_GENERATE            0x3
#define CMD_UPDATE              0x4
#define CMD_UNINSTANTIATE       0x5

#define CMD_HDR(acmd, clen, flag0, glen) \
    (((acmd) & 0xF) | (((clen) & 0xF) << 4) | \
    (((flag0) & 0xF) << 8) | (((glen) & 0xFFF) << 12))

#define MUBI4_TRUE   0x6
#define MUBI4_FALSE  0x9

#define STS_CMD_RDY_BIT         (1 << 1)
#define STS_CMD_ACK_BIT         (1 << 2)

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
/* Helper: wait until CMD_RDY=1                                       */
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
        printf("  [WARN] Timeout waiting for CMD_RDY\n");
    }
}

/* ------------------------------------------------------------------ */
/* Helper: check PASS / FAIL                                          */
/* ------------------------------------------------------------------ */
static void check(const char *desc, uint32_t actual, uint32_t expected)
{
    printf("  %s: actual=%u, expected=%u",
           desc,
           actual,
           expected);

    if (actual == expected) {
        printf(" [PASS]\n");
        pass_count++;
    } else {
        printf(" [FAIL]\n");
        fail_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Helper: check non-zero                                             */
/* ------------------------------------------------------------------ */
static void check_nonzero(const char *desc, uint32_t value)
{
    printf("  %s: value=0x%08X",
           desc,
           value);

    if (value != 0) {
        printf(" [PASS]\n");
        pass_count++;
    } else {
        printf(" (zero!) [FAIL]\n");
        fail_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Helper: read 128-bit GENBITS                                       */
/* ------------------------------------------------------------------ */
static int read_genbits(void)
{
    uint32_t vld = REG_READ(CSRNG_GENBITS_VLD);

    if (!(vld & 0x1)) {
        printf("  GENBITS not valid\n");
        return 0;
    }

    uint32_t w[4];
    uint32_t any_nonzero = 0;

    printf("  GENBITS: ");

    for (int i = 0; i < 4; i++) {
        w[i] = REG_READ(CSRNG_GENBITS);

        printf("0x%08X ", w[i]);

        if (w[i] != 0) {
            any_nonzero = 1;
        }
    }

    printf("\n");

    return any_nonzero;
}

/* ================================================================== */
/* test_crng_module                                                   */
/* ================================================================== */
static void test_crng_module(void)
{
    uint32_t rc;
    int genbits_ok;
    int i;

    printf("\n=== CSRNG Complete Command Flow Test ===\n\n");

    /* ---- Step 0 ---- */
    printf("Step 0: Enable CSRNG module\n");

    REG_WRITE(CSRNG_CTRL,
              (MUBI4_TRUE) | (MUBI4_TRUE << 4));

    delay(200);

    wait_cmd_ready();

    rc = REG_READ(CSRNG_SW_CMD_STS);

    printf("  SW_CMD_STS after enable: 0x%08X\n", rc);

    /* ---- Step 1 ---- */
    printf("\nStep 1: Set RESEED_INTERVAL = 5\n");

    REG_WRITE(CSRNG_RESEED_INTERVAL, 5);

    delay(50);

    rc = REG_READ(CSRNG_RESEED_INTERVAL);

    check("RESEED_INTERVAL readback", rc, 5);

    /* ---- Step 2 ---- */
    printf("\nStep 2: INSTANTIATE\n");

    wait_cmd_ready();

    REG_WRITE(CSRNG_CMD_REQ,
              CMD_HDR(CMD_INSTANTIATE, 0, MUBI4_TRUE, 0));

    delay(500);

    wait_cmd_ready();

    rc = REG_READ(CSRNG_RESEED_COUNTER_0);

    check("Reseed counter after INSTANTIATE (1st read)", rc, 0);

    delay(50);

    rc = REG_READ(CSRNG_RESEED_COUNTER_0);

    check("Reseed counter after INSTANTIATE (2nd read)", rc, 0);

    /* ---- Step 3 ---- */
    printf("\nStep 3: GENERATE x5 (reseed_interval=5)\n");

    for (i = 1; i <= 5; i++) {

        printf("  GENERATE #%d of 5\n", i);

        wait_cmd_ready();

        REG_WRITE(CSRNG_CMD_REQ,
                  CMD_HDR(CMD_GENERATE, 0, 0, 1));

        delay(500);

        wait_cmd_ready();

        genbits_ok = read_genbits();

        check_nonzero("  GENBITS non-zero check",
                      (uint32_t)genbits_ok);

        rc = REG_READ(CSRNG_RESEED_COUNTER_0);

        printf("  Reseed counter check (expect %d):\n", i);

        check("reseed_counter", rc, i);
    }

    /* ---- Step 4 ---- */
    printf("\nStep 4: RESEED\n");

    wait_cmd_ready();

    REG_WRITE(CSRNG_CMD_REQ,
              CMD_HDR(CMD_RESEED, 0, MUBI4_TRUE, 0));

    delay(500);

    wait_cmd_ready();

    rc = REG_READ(CSRNG_RESEED_COUNTER_0);

    check("Reseed counter after RESEED", rc, 0);

    /* ---- Step 5 ---- */
    printf("\nStep 5: Set RESEED_INTERVAL = 3\n");

    REG_WRITE(CSRNG_RESEED_INTERVAL, 3);

    delay(50);

    rc = REG_READ(CSRNG_RESEED_INTERVAL);

    check("RESEED_INTERVAL readback", rc, 3);

    /* ---- Step 6 ---- */
    printf("\nStep 6: GENERATE x3 (reseed_interval=3)\n");

    for (i = 1; i <= 3; i++) {

        printf("  GENERATE #%d of 3\n", i);

        wait_cmd_ready();

        REG_WRITE(CSRNG_CMD_REQ,
                  CMD_HDR(CMD_GENERATE, 0, 0, 1));

        delay(500);

        wait_cmd_ready();

        genbits_ok = read_genbits();

        check_nonzero("  GENBITS non-zero check",
                      (uint32_t)genbits_ok);

        rc = REG_READ(CSRNG_RESEED_COUNTER_0);

        printf("  Reseed counter check (expect %d):\n", i);

        check("reseed_counter", rc, i);
    }

    /* ---- Step 7 ---- */
    printf("\nStep 7: UPDATE (clen=1)\n");

    wait_cmd_ready();

    REG_WRITE(CSRNG_CMD_REQ,
              CMD_HDR(CMD_UPDATE, 1, 0, 0));

    delay(50);

    REG_WRITE(CSRNG_CMD_REQ, 0xDEADBEEF);

    delay(500);

    wait_cmd_ready();

    rc = REG_READ(CSRNG_RESEED_COUNTER_0);

    check("Reseed counter after UPDATE (unchanged)", rc, 3);

    /* ---- Step 8 ---- */
    printf("\nStep 8: UNINSTANTIATE\n");

    wait_cmd_ready();

    REG_WRITE(CSRNG_CMD_REQ,
              CMD_HDR(CMD_UNINSTANTIATE, 0, 0, 0));

    delay(500);

    wait_cmd_ready();

    rc = REG_READ(CSRNG_RESEED_COUNTER_0);

    check("Reseed counter after UNINSTANTIATE", rc, 0);

    /* ---- Summary ---- */

    if (fail_count == 0) {

        printf("\n*** All checks PASSED! ***\n\n");

    } else {

        printf("\n*** Some checks FAILED! ***\n\n");
    }
}

/* ================================================================== */
/* main                                                               */
/* ================================================================== */
int main(void)
{
    test_crng_module();
    return 0;
}