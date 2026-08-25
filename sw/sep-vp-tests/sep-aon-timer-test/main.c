// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// // /*
// //  * AON Timer Integration Test for SEP Platform
// //  *
// //  * This test exercises the AON Timer peripheral on the SEP VP:
// //  *   1) Register connectivity: read/write WKUP_CTRL, WDOG_CTRL, INTR_STATE
// //  *   2) INTR_TEST: verify SW-triggered interrupts reflect in INTR_STATE
// //  *   3) Wakeup timer: enable with small threshold, verify counter and interrupt
// //  *   4) Watchdog timer: enable with bark threshold, verify counter and bark
// //  *   5) Watchdog pet: write to WDOG_COUNT to reset counter before bark
// //  *   6) WDOG_REGWEN: lock watchdog config and verify writes are blocked
// //  *
// //  * Register map (base 0x10801000):
// //  *   ALERT_TEST      0x00  WO   reset=0x0
// //  *   WKUP_CTRL       0x04  RW   reset=0x0
// //  *   WKUP_THOLD_HI   0x08  RW   reset=0x0
// //  *   WKUP_THOLD_LO   0x0C  RW   reset=0x0
// //  *   WKUP_COUNT_HI   0x10  RW   reset=0x0
// //  *   WKUP_COUNT_LO   0x14  RW   reset=0x0
// //  *   WDOG_REGWEN     0x18  RW0C reset=0x1
// //  *   WDOG_CTRL       0x1C  RW   reset=0x0
// //  *   WDOG_BARK_THOLD 0x20  RW   reset=0x0
// //  *   WDOG_BITE_THOLD 0x24  RW   reset=0x0
// //  *   WDOG_COUNT      0x28  RW   reset=0x0
// //  *   INTR_STATE      0x2C  RW1C reset=0x0
// //  *   INTR_TEST       0x30  WO   reset=0x0
// //  *   WKUP_CAUSE      0x34  RW0C reset=0x0
// //  */

#include <stdint.h>

extern int printf(const char *format, ...);

/* NMI handler symbol from start.S */
extern void _nmi_handler(void);

static void install_nmi_vector(void)
{
    uint32_t nmi_addr = (uint32_t)&_nmi_handler;
    uint32_t cmd = (nmi_addr & 0xFFFFFF00U) | 0x81U;
    *(volatile uint32_t *)0x80000000U = cmd;
}

#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

#define AON_TIMER_BASE        0x10801000U

#define AON_ALERT_TEST        (AON_TIMER_BASE + 0x00U)
#define AON_WKUP_CTRL         (AON_TIMER_BASE + 0x04U)
#define AON_WKUP_THOLD_HI     (AON_TIMER_BASE + 0x08U)
#define AON_WKUP_THOLD_LO     (AON_TIMER_BASE + 0x0CU)
#define AON_WKUP_COUNT_HI     (AON_TIMER_BASE + 0x10U)
#define AON_WKUP_COUNT_LO     (AON_TIMER_BASE + 0x14U)
#define AON_WDOG_REGWEN       (AON_TIMER_BASE + 0x18U)
#define AON_WDOG_CTRL         (AON_TIMER_BASE + 0x1CU)
#define AON_WDOG_BARK_THOLD   (AON_TIMER_BASE + 0x20U)
#define AON_WDOG_BITE_THOLD   (AON_TIMER_BASE + 0x24U)
#define AON_WDOG_COUNT        (AON_TIMER_BASE + 0x28U)
#define AON_INTR_STATE        (AON_TIMER_BASE + 0x2CU)
#define AON_INTR_TEST         (AON_TIMER_BASE + 0x30U)
#define AON_WKUP_CAUSE        (AON_TIMER_BASE + 0x34U)

#define WKUP_CTRL_ENABLE_BIT      (1U << 0)

#define WDOG_CTRL_ENABLE_BIT      (1U << 0)

#define INTR_WKUP_TIMER_EXPIRED   (1U << 0)
#define INTR_WDOG_TIMER_BARK      (1U << 1)

#define WDOG_REGWEN_BIT           (1U << 0)

#define WKUP_CAUSE_BIT            (1U << 0)

static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

static int test_register_connectivity(void)
{
    printf("\n[Test 1] Register Connectivity\n");

    uint32_t wkup_ctrl = REG_READ(AON_WKUP_CTRL);
    uint32_t wdog_ctrl = REG_READ(AON_WDOG_CTRL);
    uint32_t intr_state = REG_READ(AON_INTR_STATE);
    uint32_t wdog_regwen = REG_READ(AON_WDOG_REGWEN);
    uint32_t wkup_count_lo = REG_READ(AON_WKUP_COUNT_LO);
    uint32_t wkup_count_hi = REG_READ(AON_WKUP_COUNT_HI);

    printf("  WKUP_CTRL     = 0x%x (expect 0x00000000)\n", wkup_ctrl);
    printf("  WDOG_CTRL     = 0x%x (expect 0x00000000)\n", wdog_ctrl);
    printf("  INTR_STATE    = 0x%x (expect 0x00000000)\n", intr_state);
    printf("  WDOG_REGWEN   = 0x%x (expect 0x00000001)\n", wdog_regwen);
    printf("  WKUP_COUNT_LO = 0x%x (expect 0x00000000)\n", wkup_count_lo);
    printf("  WKUP_COUNT_HI = 0x%x (expect 0x00000000)\n", wkup_count_hi);

    if (wkup_ctrl != 0x0) {
        printf("  ERROR: WKUP_CTRL reset value mismatch\n");
        return -1;
    }

    if (wdog_ctrl != 0x0) {
        printf("  ERROR: WDOG_CTRL reset value mismatch\n");
        return -1;
    }

    if (intr_state != 0x0) {
        printf("  ERROR: INTR_STATE reset value mismatch\n");
        return -1;
    }

    if (wdog_regwen != 0x1) {
        printf("  ERROR: WDOG_REGWEN reset value mismatch\n");
        return -1;
    }

    REG_WRITE(AON_WKUP_THOLD_LO, 0xDEADBEEF);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x12345678);

    uint32_t thold_lo = REG_READ(AON_WKUP_THOLD_LO);
    uint32_t thold_hi = REG_READ(AON_WKUP_THOLD_HI);

    printf("  WKUP_THOLD_LO = 0x%x (expect 0xDEADBEEF)\n", thold_lo);
    printf("  WKUP_THOLD_HI = 0x%x (expect 0x12345678)\n", thold_hi);

    if (thold_lo != 0xDEADBEEF) {
        printf("  ERROR: WKUP_THOLD_LO readback mismatch\n");
        return -1;
    }

    if (thold_hi != 0x12345678) {
        printf("  ERROR: WKUP_THOLD_HI readback mismatch\n");
        return -1;
    }

    REG_WRITE(AON_WKUP_THOLD_LO, 0x0);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x0);

    printf("  Register connectivity: PASSED\n");

    return 0;
}

static int test_intr_test(void)
{
    printf("\n[Test 2] INTR_TEST\n");

    REG_WRITE(AON_INTR_STATE,
              INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    uint32_t cleared = REG_READ(AON_INTR_STATE);

    printf("  INTR_STATE after clear = 0x%x (expect 0x0)\n", cleared);

    printf("  Writing INTR_TEST to trigger wkup_timer_expired...\n");

    REG_WRITE(AON_INTR_TEST, INTR_WKUP_TIMER_EXPIRED);

    uint32_t intr = REG_READ(AON_INTR_STATE);

    printf("  INTR_STATE = 0x%x (expect bit 0 set)\n", intr);

    if (!(intr & INTR_WKUP_TIMER_EXPIRED)) {
        printf("  ERROR: INTR_TEST did not set wkup_timer_expired\n");
        return -1;
    }

    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED);

    intr = REG_READ(AON_INTR_STATE);

    printf("  INTR_STATE after clear wkup = 0x%x (expect 0x0)\n", intr);

    if (intr & INTR_WKUP_TIMER_EXPIRED) {
        printf("  ERROR: Failed to clear wkup_timer_expired\n");
        return -1;
    }

    printf("  INTR_TEST: PASSED\n");

    return 0;
}

static int test_wakeup_timer(void)
{
    printf("\n[Test 3] Wakeup Timer\n");

    REG_WRITE(AON_INTR_STATE,
              INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    uint32_t threshold = 10;

    REG_WRITE(AON_WKUP_THOLD_LO, 0xFFFFFFFF);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x0);
    REG_WRITE(AON_WKUP_THOLD_LO, threshold);

    printf("  Threshold set to %u\n", threshold);

    REG_WRITE(AON_WKUP_COUNT_LO, 0x0);
    REG_WRITE(AON_WKUP_COUNT_HI, 0x0);

    uint32_t ctrl_val = WKUP_CTRL_ENABLE_BIT;

    REG_WRITE(AON_WKUP_CTRL, ctrl_val);

    printf("  WKUP_CTRL = 0x%x (enabled, prescaler=0)\n",
           REG_READ(AON_WKUP_CTRL));

    int timeout = 1000000;

    while (timeout-- > 0) {
        uint32_t intr = REG_READ(AON_INTR_STATE);

        if (intr & INTR_WKUP_TIMER_EXPIRED) {
            uint32_t count_lo = REG_READ(AON_WKUP_COUNT_LO);

            printf("  Wakeup interrupt fired! WKUP_COUNT_LO=%u\n",
                   count_lo);

            break;
        }
    }

    if (timeout <= 0) {
        uint32_t count_lo = REG_READ(AON_WKUP_COUNT_LO);

        printf("  ERROR: Wakeup timer interrupt did not fire "
               "(count_lo=%u)\n",
               count_lo);

        return -1;
    }

    uint32_t cause = REG_READ(AON_WKUP_CAUSE);

    printf("  WKUP_CAUSE = 0x%x (expect bit 0 set)\n", cause);

    if (!(cause & WKUP_CAUSE_BIT)) {
        printf("  ERROR: WKUP_CAUSE not set after wakeup interrupt\n");
        return -1;
    }

    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED);
    REG_WRITE(AON_WKUP_CAUSE, 0x0);

    REG_WRITE(AON_WKUP_CTRL, 0x0);

    printf("  Wakeup Timer: PASSED\n");

    return 0;
}

static int test_watchdog_bark(void)
{
    printf("\n[Test 4] Watchdog Timer (Bark)\n");

    REG_WRITE(AON_INTR_STATE,
              INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    uint32_t regwen = REG_READ(AON_WDOG_REGWEN);

    printf("  WDOG_REGWEN = 0x%x (expect 0x1, unlocked)\n",
           regwen);

    if (!(regwen & WDOG_REGWEN_BIT)) {
        printf("  ERROR: WDOG_REGWEN is already locked\n");
        return -1;
    }

    uint32_t bark_threshold = 10;

    REG_WRITE(AON_WDOG_BARK_THOLD, bark_threshold);

    printf("  WDOG_BARK_THOLD = %u\n",
           REG_READ(AON_WDOG_BARK_THOLD));

    REG_WRITE(AON_WDOG_BITE_THOLD, 0xFFFFFFFF);

    printf("  WDOG_BITE_THOLD = 0x%x\n",
           REG_READ(AON_WDOG_BITE_THOLD));

    REG_WRITE(AON_WDOG_COUNT, 0x0);

    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);

    printf("  WDOG_CTRL = 0x%x (enabled)\n",
           REG_READ(AON_WDOG_CTRL));

    int timeout = 1000000;

    while (timeout-- > 0) {
        uint32_t intr = REG_READ(AON_INTR_STATE);

        if (intr & INTR_WDOG_TIMER_BARK) {
            uint32_t count = REG_READ(AON_WDOG_COUNT);

            printf("  Watchdog bark fired! WDOG_COUNT=%u\n", count);

            break;
        }
    }

    if (timeout <= 0) {
        uint32_t count = REG_READ(AON_WDOG_COUNT);

        printf("  ERROR: Watchdog bark did not fire (count=%u)\n",
               count);

        return -1;
    }

    REG_WRITE(AON_INTR_STATE, INTR_WDOG_TIMER_BARK);

    REG_WRITE(AON_WDOG_CTRL, 0x0);

    REG_WRITE(AON_WDOG_COUNT, 0x0);

    printf("  Watchdog Timer (Bark): PASSED\n");

    return 0;
}

static int test_watchdog_pet(void)
{
    printf("\n[Test 5] Watchdog Pet\n");

    REG_WRITE(AON_INTR_STATE,
              INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    uint32_t bark_threshold = 50;

    REG_WRITE(AON_WDOG_BARK_THOLD, bark_threshold);
    REG_WRITE(AON_WDOG_BITE_THOLD, 0xFFFFFFFF);

    REG_WRITE(AON_WDOG_COUNT, 0x0);
    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);

    printf("  Watchdog enabled with bark_threshold=%u\n",
           bark_threshold);

    for (int i = 0; i < 5; i++) {

        for (volatile int j = 0; j < 100; j++) {
            __asm__ volatile("nop");
        }

        REG_WRITE(AON_WDOG_COUNT, 0x0);

        uint32_t intr = REG_READ(AON_INTR_STATE);

        if (intr & INTR_WDOG_TIMER_BARK) {
            printf("  ERROR: Watchdog barked despite petting "
                   "(iteration %d)\n",
                   i);

            REG_WRITE(AON_WDOG_CTRL, 0x0);
            REG_WRITE(AON_INTR_STATE, INTR_WDOG_TIMER_BARK);

            return -1;
        }
    }

    printf("  No bark after 5 pets - watchdog pet is working\n");

    REG_WRITE(AON_WDOG_CTRL, 0x0);
    REG_WRITE(AON_WDOG_COUNT, 0x0);

    printf("  Watchdog Pet: PASSED\n");

    return 0;
}

static int test_wdog_regwen_lock(void)
{
    printf("\n[Test 6] WDOG_REGWEN Lock\n");

    REG_WRITE(AON_WDOG_CTRL, 0x0);
    REG_WRITE(AON_WDOG_BARK_THOLD, 100);
    REG_WRITE(AON_WDOG_BITE_THOLD, 200);

    printf("  Before lock:\n");
    printf("    WDOG_REGWEN     = 0x%x\n",
           REG_READ(AON_WDOG_REGWEN));
    printf("    WDOG_CTRL       = 0x%x\n",
           REG_READ(AON_WDOG_CTRL));
    printf("    WDOG_BARK_THOLD = %u\n",
           REG_READ(AON_WDOG_BARK_THOLD));
    printf("    WDOG_BITE_THOLD = %u\n",
           REG_READ(AON_WDOG_BITE_THOLD));

    REG_WRITE(AON_WDOG_REGWEN, 0x0);

    uint32_t regwen = REG_READ(AON_WDOG_REGWEN);

    printf("  After lock: WDOG_REGWEN = 0x%x (expect 0x0)\n",
           regwen);

    if (regwen != 0x0) {
        printf("  ERROR: WDOG_REGWEN was not locked\n");
        return -1;
    }

    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);

    uint32_t ctrl_after = REG_READ(AON_WDOG_CTRL);

    printf("  After locked write to WDOG_CTRL: 0x%x "
           "(expect 0x0, unchanged)\n",
           ctrl_after);

    if (ctrl_after != 0x0) {
        printf("  ERROR: WDOG_CTRL was modified despite "
               "REGWEN lock\n");
        return -1;
    }

    REG_WRITE(AON_WDOG_BARK_THOLD, 999);

    uint32_t bark_after = REG_READ(AON_WDOG_BARK_THOLD);

    printf("  After locked write to WDOG_BARK_THOLD: %u "
           "(expect 100, unchanged)\n",
           bark_after);

    if (bark_after != 100) {
        printf("  ERROR: WDOG_BARK_THOLD was modified despite "
               "REGWEN lock\n");
        return -1;
    }

    REG_WRITE(AON_WDOG_COUNT, 0x0);

    uint32_t count = REG_READ(AON_WDOG_COUNT);

    printf("  WDOG_COUNT after pet while locked: %u "
           "(expect 0 or small value)\n",
           count);

    printf("  WDOG_REGWEN Lock: PASSED\n");

    return 0;
}

int main(void)
{
    printf("\n========================================\n");
    printf("AON Timer Integration Test\n");
    printf("========================================\n");
    printf("AON Timer base = 0x%x\n", AON_TIMER_BASE);

    install_nmi_vector();

    if (test_register_connectivity() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    if (test_intr_test() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    if (test_wakeup_timer() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    if (test_watchdog_bark() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    if (test_watchdog_pet() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    if (test_wdog_regwen_lock() == 0) {
        test_passed++;
    } else {
        test_failed++;
    }

    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %u\n", test_passed);
    printf("Tests failed: %u\n", test_failed);

    if (test_failed == 0) {
        printf("\n========================================\n");
        printf("=== AON Timer Test PASSED ===\n");
        printf("========================================\n");
    } else {
        printf("\n=== AON Timer Test FAILED ===\n");
    }

    return 0;
}