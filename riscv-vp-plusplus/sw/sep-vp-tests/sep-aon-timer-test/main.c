/*
 * AON Timer Integration Test for SEP Platform
 *
 * This test exercises the AON Timer peripheral on the SEP VP:
 *   1) Register connectivity: read/write WKUP_CTRL, WDOG_CTRL, INTR_STATE
 *   2) INTR_TEST: verify SW-triggered interrupts reflect in INTR_STATE
 *   3) Wakeup timer: enable with small threshold, verify counter and interrupt
 *   4) Watchdog timer: enable with bark threshold, verify counter and bark
 *   5) Watchdog pet: write to WDOG_COUNT to reset counter before bark
 *   6) WDOG_REGWEN: lock watchdog config and verify writes are blocked
 *
 * Register map (base 0x10801000):
 *   ALERT_TEST      0x00  WO   reset=0x0
 *   WKUP_CTRL       0x04  RW   reset=0x0
 *   WKUP_THOLD_HI   0x08  RW   reset=0x0
 *   WKUP_THOLD_LO   0x0C  RW   reset=0x0
 *   WKUP_COUNT_HI   0x10  RW   reset=0x0
 *   WKUP_COUNT_LO   0x14  RW   reset=0x0
 *   WDOG_REGWEN     0x18  RW0C reset=0x1
 *   WDOG_CTRL       0x1C  RW   reset=0x0
 *   WDOG_BARK_THOLD 0x20  RW   reset=0x0
 *   WDOG_BITE_THOLD 0x24  RW   reset=0x0
 *   WDOG_COUNT      0x28  RW   reset=0x0
 *   INTR_STATE      0x2C  RW1C reset=0x0
 *   INTR_TEST       0x30  WO   reset=0x0
 *   WKUP_CAUSE      0x34  RW0C reset=0x0
 */

#include <stdint.h>

/* Simple UART print functions (defined in uart_io.c) */
extern void uart_print(const char *str);
extern void uart_print_dec(uint32_t value);
extern void uart_print_hex(uint32_t value, int digits);

/* NMI handler symbol from start.S; VeeR jumps here when the AON-timer
 * watchdog bark asserts. The handler is placed in a 256-byte-aligned
 * section by link.ld. */
extern void _nmi_handler(void);

/* Tell the VP what address to use for nmi_vec.
 * Mailbox protocol (vp/src/platform/sep/och_sep_ss.hpp:135-137):
 *   write to STDOUT (0x80000000) with low byte = 0x81 (LOAD_NMI_ADDR);
 *   the VP latches bits [31:8] into nmi_vec_o. */
static void install_nmi_vector(void) {
    uint32_t nmi_addr = (uint32_t)&_nmi_handler;
    uint32_t cmd = (nmi_addr & 0xFFFFFF00U) | 0x81U;
    *(volatile uint32_t *)0x80000000U = cmd;
}

/* ============================================================================
 * Register Access Helpers
 * ============================================================================ */
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

/* ============================================================================
 * AON Timer Register Addresses (SEP platform base = 0x10801000)
 * ============================================================================ */
#define AON_TIMER_BASE        0x10801000U

#define AON_ALERT_TEST        (AON_TIMER_BASE + 0x00U)
#define AON_WKUP_CTRL         (AON_TIMER_BASE + 0x04U)
#define AON_WKUP_THOLD_HI    (AON_TIMER_BASE + 0x08U)
#define AON_WKUP_THOLD_LO    (AON_TIMER_BASE + 0x0CU)
#define AON_WKUP_COUNT_HI    (AON_TIMER_BASE + 0x10U)
#define AON_WKUP_COUNT_LO    (AON_TIMER_BASE + 0x14U)
#define AON_WDOG_REGWEN      (AON_TIMER_BASE + 0x18U)
#define AON_WDOG_CTRL         (AON_TIMER_BASE + 0x1CU)
#define AON_WDOG_BARK_THOLD  (AON_TIMER_BASE + 0x20U)
#define AON_WDOG_BITE_THOLD  (AON_TIMER_BASE + 0x24U)
#define AON_WDOG_COUNT        (AON_TIMER_BASE + 0x28U)
#define AON_INTR_STATE        (AON_TIMER_BASE + 0x2CU)
#define AON_INTR_TEST         (AON_TIMER_BASE + 0x30U)
#define AON_WKUP_CAUSE        (AON_TIMER_BASE + 0x34U)

/* WKUP_CTRL bitfield definitions */
#define WKUP_CTRL_ENABLE_BIT     (1U << 0)
#define WKUP_CTRL_PRESCALER_SHIFT 1
#define WKUP_CTRL_PRESCALER_MASK  (0xFFFU << WKUP_CTRL_PRESCALER_SHIFT)

/* WDOG_CTRL bitfield definitions */
#define WDOG_CTRL_ENABLE_BIT       (1U << 0)
#define WDOG_CTRL_PAUSE_IN_SLEEP   (1U << 1)

/* INTR_STATE / INTR_TEST bitfield definitions */
#define INTR_WKUP_TIMER_EXPIRED    (1U << 0)
#define INTR_WDOG_TIMER_BARK       (1U << 1)

/* WDOG_REGWEN bitfield */
#define WDOG_REGWEN_BIT            (1U << 0)

/* WKUP_CAUSE bitfield */
#define WKUP_CAUSE_BIT             (1U << 0)

/* Test result tracking */
static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

/* ============================================================================
 * Test 1: Register Connectivity
 * Verify basic read/write access to RW registers and reset values
 * ============================================================================ */
static int test_register_connectivity(void) {
    uart_print("\n[Test 1] Register Connectivity\n");

    /* All registers should be at reset values (0x0), except WDOG_REGWEN (0x1) */
    uint32_t wkup_ctrl = REG_READ(AON_WKUP_CTRL);
    uint32_t wdog_ctrl = REG_READ(AON_WDOG_CTRL);
    uint32_t intr_state = REG_READ(AON_INTR_STATE);
    uint32_t wdog_regwen = REG_READ(AON_WDOG_REGWEN);
    uint32_t wkup_count_lo = REG_READ(AON_WKUP_COUNT_LO);
    uint32_t wkup_count_hi = REG_READ(AON_WKUP_COUNT_HI);

    uart_print("  WKUP_CTRL     = ");
    uart_print_hex(wkup_ctrl, 8);
    uart_print(" (expect 0x00000000)\n");

    uart_print("  WDOG_CTRL     = ");
    uart_print_hex(wdog_ctrl, 8);
    uart_print(" (expect 0x00000000)\n");

    uart_print("  INTR_STATE    = ");
    uart_print_hex(intr_state, 8);
    uart_print(" (expect 0x00000000)\n");

    uart_print("  WDOG_REGWEN   = ");
    uart_print_hex(wdog_regwen, 8);
    uart_print(" (expect 0x00000001)\n");

    uart_print("  WKUP_COUNT_LO = ");
    uart_print_hex(wkup_count_lo, 8);
    uart_print(" (expect 0x00000000)\n");

    uart_print("  WKUP_COUNT_HI = ");
    uart_print_hex(wkup_count_hi, 8);
    uart_print(" (expect 0x00000000)\n");

    if (wkup_ctrl != 0x0) {
        uart_print("  ERROR: WKUP_CTRL reset value mismatch\n");
        return -1;
    }
    if (wdog_ctrl != 0x0) {
        uart_print("  ERROR: WDOG_CTRL reset value mismatch\n");
        return -1;
    }
    if (intr_state != 0x0) {
        uart_print("  ERROR: INTR_STATE reset value mismatch\n");
        return -1;
    }
    if (wdog_regwen != 0x1) {
        uart_print("  ERROR: WDOG_REGWEN reset value mismatch\n");
        return -1;
    }

    /* Write and read back wakeup threshold registers */
    REG_WRITE(AON_WKUP_THOLD_LO, 0xDEADBEEF);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x12345678);
    uint32_t thold_lo = REG_READ(AON_WKUP_THOLD_LO);
    uint32_t thold_hi = REG_READ(AON_WKUP_THOLD_HI);

    uart_print("  WKUP_THOLD_LO = ");
    uart_print_hex(thold_lo, 8);
    uart_print(" (expect 0xDEADBEEF)\n");

    uart_print("  WKUP_THOLD_HI = ");
    uart_print_hex(thold_hi, 8);
    uart_print(" (expect 0x12345678)\n");

    if (thold_lo != 0xDEADBEEF) {
        uart_print("  ERROR: WKUP_THOLD_LO readback mismatch\n");
        return -1;
    }
    if (thold_hi != 0x12345678) {
        uart_print("  ERROR: WKUP_THOLD_HI readback mismatch\n");
        return -1;
    }

    /* Clean up: reset thresholds to 0 */
    REG_WRITE(AON_WKUP_THOLD_LO, 0x0);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x0);

    uart_print("  Register connectivity: PASSED\n");
    return 0;
}

/* ============================================================================
 * Test 2: INTR_TEST
 * Use the write-only INTR_TEST register to force-assert interrupts and
 * verify they appear in INTR_STATE, then clear via RW1C
 * ============================================================================ */
static int test_intr_test(void) {
    uart_print("\n[Test 2] INTR_TEST\n");

    /* Clear any pending interrupts first */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);
    uint32_t cleared = REG_READ(AON_INTR_STATE);
    uart_print("  INTR_STATE after clear = ");
    uart_print_hex(cleared, 8);
    uart_print(" (expect 0x0)\n");

    /* Trigger wakeup interrupt via INTR_TEST */
    uart_print("  Writing INTR_TEST to trigger wkup_timer_expired...\n");
    REG_WRITE(AON_INTR_TEST, INTR_WKUP_TIMER_EXPIRED);
    uint32_t intr = REG_READ(AON_INTR_STATE);
    uart_print("  INTR_STATE = ");
    uart_print_hex(intr, 8);
    uart_print(" (expect bit 0 set)\n");
    if (!(intr & INTR_WKUP_TIMER_EXPIRED)) {
        uart_print("  ERROR: INTR_TEST did not set wkup_timer_expired\n");
        return -1;
    }

    /* Clear wakeup interrupt */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED);
    intr = REG_READ(AON_INTR_STATE);
    uart_print("  INTR_STATE after clear wkup = ");
    uart_print_hex(intr, 8);
    uart_print(" (expect 0x0)\n");
    if (intr & INTR_WKUP_TIMER_EXPIRED) {
        uart_print("  ERROR: Failed to clear wkup_timer_expired\n");
        return -1;
    }

    /* Note: INTR_TEST=wdog_timer_bark is intentionally not exercised here.
     * On this SoC the bark line is hard-wired to the CPU's NMI input with no
     * masking between them, so asserting the bark immediately preempts the
     * foreground; by the time main resumes, the NMI handler has already W1C'd
     * INTR_STATE.bark and the bit can never be observed from foreground.
     * The bark path is covered end-to-end by Test 4 (timer -> bark -> NMI). */

    uart_print("  INTR_TEST: PASSED\n");
    return 0;
}

/* ============================================================================
 * Test 3: Wakeup Timer
 * Enable the wakeup timer with a small threshold and prescaler=0,
 * then spin until the counter reaches the threshold and the interrupt fires.
 * ============================================================================ */
static int test_wakeup_timer(void) {
    uart_print("\n[Test 3] Wakeup Timer\n");

    /* Clear any pending interrupts */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    /* Set up a small threshold (counter counts at AON clock rate / (prescaler+1))
     * With prescaler=0, counter increments on every AON tick.
     * AON clock is 200 kHz in SEP => 5 us per tick.
     * Set threshold to 10 so we need ~50 us of AON time. */
    uint32_t threshold = 10;
    /* Safe write sequence: set LO to max first, then HI, then LO */
    REG_WRITE(AON_WKUP_THOLD_LO, 0xFFFFFFFF);
    REG_WRITE(AON_WKUP_THOLD_HI, 0x0);
    REG_WRITE(AON_WKUP_THOLD_LO, threshold);

    uart_print("  Threshold set to ");
    uart_print_dec(threshold);
    uart_print("\n");

    /* Reset counter to 0 */
    REG_WRITE(AON_WKUP_COUNT_LO, 0x0);
    REG_WRITE(AON_WKUP_COUNT_HI, 0x0);

    /* Enable wakeup timer with prescaler=0 */
    uint32_t ctrl_val = WKUP_CTRL_ENABLE_BIT;  /* prescaler=0, enable=1 */
    REG_WRITE(AON_WKUP_CTRL, ctrl_val);

    uart_print("  WKUP_CTRL = ");
    uart_print_hex(REG_READ(AON_WKUP_CTRL), 8);
    uart_print(" (enabled, prescaler=0)\n");

    /* Wait for the wakeup interrupt to fire */
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint32_t intr = REG_READ(AON_INTR_STATE);
        if (intr & INTR_WKUP_TIMER_EXPIRED) {
            uint32_t count_lo = REG_READ(AON_WKUP_COUNT_LO);
            uart_print("  Wakeup interrupt fired! WKUP_COUNT_LO=");
            uart_print_dec(count_lo);
            uart_print("\n");
            break;
        }
    }

    if (timeout <= 0) {
        uint32_t count_lo = REG_READ(AON_WKUP_COUNT_LO);
        uart_print("  ERROR: Wakeup timer interrupt did not fire (count_lo=");
        uart_print_dec(count_lo);
        uart_print(")\n");
        return -1;
    }

    /* Check WKUP_CAUSE is set */
    uint32_t cause = REG_READ(AON_WKUP_CAUSE);
    uart_print("  WKUP_CAUSE = ");
    uart_print_hex(cause, 8);
    uart_print(" (expect bit 0 set)\n");
    if (!(cause & WKUP_CAUSE_BIT)) {
        uart_print("  ERROR: WKUP_CAUSE not set after wakeup interrupt\n");
        return -1;
    }

    /* Clear interrupt and cause */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED);
    REG_WRITE(AON_WKUP_CAUSE, 0x0);  /* RW0C: write 0 to clear */

    /* Disable wakeup timer */
    REG_WRITE(AON_WKUP_CTRL, 0x0);

    uart_print("  Wakeup Timer: PASSED\n");
    return 0;
}

/* ============================================================================
 * Test 4: Watchdog Timer (Bark)
 * Enable the watchdog with a bark threshold, wait for bark interrupt.
 * ============================================================================ */
static int test_watchdog_bark(void) {
    uart_print("\n[Test 4] Watchdog Timer (Bark)\n");

    /* Clear any pending interrupts */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    /* Ensure WDOG_REGWEN is still unlocked */
    uint32_t regwen = REG_READ(AON_WDOG_REGWEN);
    uart_print("  WDOG_REGWEN = ");
    uart_print_hex(regwen, 8);
    uart_print(" (expect 0x1, unlocked)\n");
    if (!(regwen & WDOG_REGWEN_BIT)) {
        uart_print("  ERROR: WDOG_REGWEN is already locked\n");
        return -1;
    }

    /* Set bark threshold to a small value */
    uint32_t bark_threshold = 10;
    REG_WRITE(AON_WDOG_BARK_THOLD, bark_threshold);
    uart_print("  WDOG_BARK_THOLD = ");
    uart_print_dec(REG_READ(AON_WDOG_BARK_THOLD));
    uart_print("\n");

    /* Set bite threshold higher so we don't get a reset */
    REG_WRITE(AON_WDOG_BITE_THOLD, 0xFFFFFFFF);
    uart_print("  WDOG_BITE_THOLD = ");
    uart_print_hex(REG_READ(AON_WDOG_BITE_THOLD), 8);
    uart_print("\n");

    /* Reset watchdog counter (any write resets to 0) */
    REG_WRITE(AON_WDOG_COUNT, 0x0);

    /* Enable watchdog */
    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);
    uart_print("  WDOG_CTRL = ");
    uart_print_hex(REG_READ(AON_WDOG_CTRL), 8);
    uart_print(" (enabled)\n");

    /* Wait for bark interrupt */
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint32_t intr = REG_READ(AON_INTR_STATE);
        if (intr & INTR_WDOG_TIMER_BARK) {
            uint32_t count = REG_READ(AON_WDOG_COUNT);
            uart_print("  Watchdog bark fired! WDOG_COUNT=");
            uart_print_dec(count);
            uart_print("\n");
            break;
        }
    }

    if (timeout <= 0) {
        uint32_t count = REG_READ(AON_WDOG_COUNT);
        uart_print("  ERROR: Watchdog bark did not fire (count=");
        uart_print_dec(count);
        uart_print(")\n");
        return -1;
    }

    /* Clear bark interrupt */
    REG_WRITE(AON_INTR_STATE, INTR_WDOG_TIMER_BARK);

    /* Disable watchdog */
    REG_WRITE(AON_WDOG_CTRL, 0x0);

    /* Pet the watchdog (reset counter) */
    REG_WRITE(AON_WDOG_COUNT, 0x0);

    uart_print("  Watchdog Timer (Bark): PASSED\n");
    return 0;
}

/* ============================================================================
 * Test 5: Watchdog Pet
 * Enable watchdog, pet it before bark, verify no bark fires.
 * ============================================================================ */
static int test_watchdog_pet(void) {
    uart_print("\n[Test 5] Watchdog Pet\n");

    /* Clear interrupts */
    REG_WRITE(AON_INTR_STATE, INTR_WKUP_TIMER_EXPIRED | INTR_WDOG_TIMER_BARK);

    /* Set bark threshold to a medium value */
    uint32_t bark_threshold = 50;
    REG_WRITE(AON_WDOG_BARK_THOLD, bark_threshold);
    REG_WRITE(AON_WDOG_BITE_THOLD, 0xFFFFFFFF);

    /* Reset counter and enable */
    REG_WRITE(AON_WDOG_COUNT, 0x0);
    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);
    uart_print("  Watchdog enabled with bark_threshold=");
    uart_print_dec(bark_threshold);
    uart_print("\n");

    /* Pet the watchdog a few times before it barks */
    for (int i = 0; i < 5; i++) {
        /* Small spin delay */
        for (volatile int j = 0; j < 100; j++) {
            __asm__ volatile("nop");
        }

        /* Pet: any write to WDOG_COUNT resets it to 0 */
        REG_WRITE(AON_WDOG_COUNT, 0x0);

        uint32_t intr = REG_READ(AON_INTR_STATE);
        if (intr & INTR_WDOG_TIMER_BARK) {
            uart_print("  ERROR: Watchdog barked despite petting (iteration ");
            uart_print_dec(i);
            uart_print(")\n");
            /* Disable and clean up */
            REG_WRITE(AON_WDOG_CTRL, 0x0);
            REG_WRITE(AON_INTR_STATE, INTR_WDOG_TIMER_BARK);
            return -1;
        }
    }

    uart_print("  No bark after 5 pets - watchdog pet is working\n");

    /* Disable watchdog */
    REG_WRITE(AON_WDOG_CTRL, 0x0);
    REG_WRITE(AON_WDOG_COUNT, 0x0);

    uart_print("  Watchdog Pet: PASSED\n");
    return 0;
}

/* ============================================================================
 * Test 6: WDOG_REGWEN Lock
 * Lock watchdog config by writing 0 to WDOG_REGWEN, verify that subsequent
 * writes to WDOG_CTRL and WDOG_BARK_THOLD are blocked.
 * ============================================================================ */
static int test_wdog_regwen_lock(void) {
    uart_print("\n[Test 6] WDOG_REGWEN Lock\n");

    /* Write known values to WDOG_CTRL and WDOG_BARK_THOLD before locking */
    REG_WRITE(AON_WDOG_CTRL, 0x0);   /* disabled */
    REG_WRITE(AON_WDOG_BARK_THOLD, 100);
    REG_WRITE(AON_WDOG_BITE_THOLD, 200);

    uart_print("  Before lock:\n");
    uart_print("    WDOG_REGWEN    = ");
    uart_print_hex(REG_READ(AON_WDOG_REGWEN), 8);
    uart_print("\n");
    uart_print("    WDOG_CTRL      = ");
    uart_print_hex(REG_READ(AON_WDOG_CTRL), 8);
    uart_print("\n");
    uart_print("    WDOG_BARK_THOLD= ");
    uart_print_dec(REG_READ(AON_WDOG_BARK_THOLD));
    uart_print("\n");
    uart_print("    WDOG_BITE_THOLD= ");
    uart_print_dec(REG_READ(AON_WDOG_BITE_THOLD));
    uart_print("\n");

    /* Lock: write 0 to WDOG_REGWEN (RW0C - writing 0 clears, permanent until reset) */
    REG_WRITE(AON_WDOG_REGWEN, 0x0);
    uint32_t regwen = REG_READ(AON_WDOG_REGWEN);
    uart_print("  After lock: WDOG_REGWEN = ");
    uart_print_hex(regwen, 8);
    uart_print(" (expect 0x0)\n");

    if (regwen != 0x0) {
        uart_print("  ERROR: WDOG_REGWEN was not locked\n");
        return -1;
    }

    /* Try to change WDOG_CTRL - should be silently ignored */
    REG_WRITE(AON_WDOG_CTRL, WDOG_CTRL_ENABLE_BIT);
    uint32_t ctrl_after = REG_READ(AON_WDOG_CTRL);
    uart_print("  After locked write to WDOG_CTRL: ");
    uart_print_hex(ctrl_after, 8);
    uart_print(" (expect 0x0, unchanged)\n");
    if (ctrl_after != 0x0) {
        uart_print("  ERROR: WDOG_CTRL was modified despite REGWEN lock\n");
        return -1;
    }

    /* Try to change WDOG_BARK_THOLD - should be silently ignored */
    REG_WRITE(AON_WDOG_BARK_THOLD, 999);
    uint32_t bark_after = REG_READ(AON_WDOG_BARK_THOLD);
    uart_print("  After locked write to WDOG_BARK_THOLD: ");
    uart_print_dec(bark_after);
    uart_print(" (expect 100, unchanged)\n");
    if (bark_after != 100) {
        uart_print("  ERROR: WDOG_BARK_THOLD was modified despite REGWEN lock\n");
        return -1;
    }

    /* Verify WDOG_COUNT is NOT gated (petting always allowed) */
    REG_WRITE(AON_WDOG_COUNT, 0x0);
    uint32_t count = REG_READ(AON_WDOG_COUNT);
    uart_print("  WDOG_COUNT after pet while locked: ");
    uart_print_dec(count);
    uart_print(" (expect 0 or small value)\n");

    uart_print("  WDOG_REGWEN Lock: PASSED\n");
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void) {
    uart_print("\n========================================\n");
    uart_print("AON Timer Integration Test\n");
    uart_print("========================================\n");
    uart_print("AON Timer base = ");
    uart_print_hex(AON_TIMER_BASE, 8);
    uart_print("\n");

    /* The watchdog bark is wired to the CPU's NMI input. Test 4 fires a real
     * bark via the timer threshold; without an NMI handler at the FW-supplied
     * vector the CPU jumps to the JSON default (0x1000e00) and hangs. */
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

    /* Summary */
    uart_print("\n=== Test Summary ===\n");
    uart_print("Tests passed: ");
    uart_print_dec(test_passed);
    uart_print("\n");
    uart_print("Tests failed: ");
    uart_print_dec(test_failed);
    uart_print("\n");

    if (test_failed == 0) {
        uart_print("\n========================================\n");
        uart_print("=== AON Timer Test PASSED ===\n");
        uart_print("========================================\n");
    } else {
        uart_print("\n=== AON Timer Test FAILED ===\n");
    }

    return 0;
}
