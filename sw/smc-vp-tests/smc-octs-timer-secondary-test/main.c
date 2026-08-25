/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-octs-timer-secondary-test/main.c
 *
 * octs_system_timer SECONDARY-mode test over the SMC fabric.
 *
 * Mirrors octs_p0_sec_test from tt-oca-hw-main/fw/smc/tests: read STATUS to
 * discover the mode, program CTRL / GPIO_ENABLE / PRESET, write TIMER_START,
 * then poll STATUS.RUNNING waiting for the PRIMARY's sync_load pulse.
 *
 * The difference is the expected outcome.  The RTL test runs against a padring
 * model that generates sync_load pulses, so its timer eventually starts; in the
 * SMC VP nothing drives timer_sync_load_i (the SMC instance is the system
 * PRIMARY, and no other subsystem is modelled), so a timer strapped SECONDARY
 * has no pulse source.  This test therefore pins the *contract* that a
 * SECONDARY stays parked until it is synced:
 *
 *   - STATUS.MODE reads 1 (SECONDARY)
 *   - the register file is fully accessible and behaves exactly as on a primary
 *   - TIMER_START alone does NOT start the counter: RUNNING stays clear and
 *     COUNT stays 0 for as long as we poll
 *
 * That is the same negative result octs_p0_sec_test reports when sync_load
 * never arrives ("Timer never started (sync_load pulse not received)"), except
 * here it is the expected behaviour rather than a diagnostic.
 *
 * Requires dut.octs_is_primary = false, set in
 * smc_octs_timer_secondary_test.ini.  The end-to-end primary -> secondary sync
 * path (sync_load, credit budget, starvation, re-anchor) needs two timers and
 * is covered by the IP's unit testbench,
 * smc/peripherals/octs_system_timer/test/octs_system_timer_tb.cpp.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define TIMER(off) (SMC_OCTS_TIMER_BASE + (off))

/* Poll budget while waiting for a sync_load that never comes.  Mirrors
 * MAX_WAIT_CYCLES / the wait loop in octs_p0_sec_test. */
#define POLL_ATTEMPTS 64u
#define POLL_SPIN     50u

static void spin(unsigned iters)
{
    for (volatile unsigned i = 0; i < iters; i++) {
        __asm__ volatile("nop");
    }
}

static uint64_t timer_count(void)
{
    uint32_t lo = REG_READ(TIMER(OCTS_TIMER_COUNT_LO));
    uint32_t hi = REG_READ(TIMER(OCTS_TIMER_COUNT_HI));
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    printf("\n=== SMC octs_system_timer SECONDARY test ===\n\n");

    int pass = 1;

    /* -------- 1. Mode discovery ------------------------------------------- */
    uint32_t status = REG_READ(TIMER(OCTS_STATUS));
    printf("STATUS=0x%08x (MODE=%s RUNNING=%u)\n", status,
           (status & OCTS_STATUS_MODE) ? "SECONDARY" : "PRIMARY",
           (unsigned)((status & OCTS_STATUS_RUNNING) != 0));

    if ((status & OCTS_STATUS_MODE) == 0) {
        printf("  FAIL: timer reports PRIMARY; this test needs "
               "dut.octs_is_primary=false in its .ini\n");
        pass = 0;
    }
    if ((status & OCTS_STATUS_RUNNING) != 0) {
        printf("  FAIL: STATUS.RUNNING set out of reset\n");
        pass = 0;
    }

    /* -------- 2. Register file works the same as on a primary ------------- */
    /* timer_init() from the RTL test: CTRL then GPIO_ENABLE, both read back. */
    REG_WRITE(TIMER(OCTS_CTRL), OCTS_CTRL_RESET);
    uint32_t ctrl = REG_READ(TIMER(OCTS_CTRL));
    if (ctrl != OCTS_CTRL_RESET) {
        printf("  FAIL: CTRL readback 0x%08x, expected 0x%08x\n",
               ctrl, OCTS_CTRL_RESET);
        pass = 0;
    }

    REG_WRITE(TIMER(OCTS_TIMER_GPIO_ENABLE), 1u);
    uint32_t gpio = REG_READ(TIMER(OCTS_TIMER_GPIO_ENABLE));
    if ((gpio & 0x1u) != 1u) {
        printf("  FAIL: GPIO_ENABLE did not latch (0x%08x)\n", gpio);
        pass = 0;
    }
    printf("CTRL=0x%08x GPIO_ENABLE=0x%08x\n", ctrl, gpio);

    /* octs_p0_sec_test uses preset 10. */
    const uint32_t preset = 10u;
    REG_WRITE(TIMER(OCTS_TIMER_PRESET_LO), preset);
    REG_WRITE(TIMER(OCTS_TIMER_PRESET_HI), 0u);
    if (REG_READ(TIMER(OCTS_TIMER_PRESET_LO)) != preset ||
        REG_READ(TIMER(OCTS_TIMER_PRESET_HI)) != 0u) {
        printf("  FAIL: PRESET readback mismatch\n");
        pass = 0;
    }

    /* -------- 3. START does not start an unsynced SECONDARY --------------- */
    REG_WRITE(TIMER(OCTS_TIMER_START), 1u);

    unsigned attempts = 0;
    uint32_t seen_running = 0;
    while (attempts < POLL_ATTEMPTS) {
        status = REG_READ(TIMER(OCTS_STATUS));
        if ((status & OCTS_STATUS_RUNNING) != 0) {
            seen_running = 1;
            break;
        }
        spin(POLL_SPIN);
        attempts++;
    }

    uint64_t count = timer_count();
    printf("after START + %u polls: STATUS=0x%08x COUNT=0x%08x%08x\n",
           attempts, status, (uint32_t)(count >> 32), (uint32_t)count);

    if (seen_running) {
        printf("  FAIL: SECONDARY started without a sync_load pulse\n");
        pass = 0;
    }
    if (count != 0ull) {
        printf("  FAIL: COUNT advanced to 0x%08x while unsynced; a SECONDARY "
               "must stay parked at 0\n", (uint32_t)count);
        pass = 0;
    }

    /* -------- 4. Sync outputs are a primary-only feature ------------------ */
    /* Nothing observable from firmware drives sync_load_o / cnt_credit_o on a
     * secondary, but CREDIT_EXPIRED is: the credit budget of an idle, unsynced
     * secondary drains, so the peak starved-cycle counter is allowed to be
     * non-zero here (it faithfully reproduces the RTL's accounting on an idle
     * secondary).  Only the clear path is contractual. */
    uint32_t credit = REG_READ(TIMER(OCTS_CREDIT_EXPIRED));
    printf("CREDIT_EXPIRED while idle/unsynced: 0x%08x (informational)\n",
           credit);
    REG_WRITE(TIMER(OCTS_CREDIT_EXPIRED), 0u);
    credit = REG_READ(TIMER(OCTS_CREDIT_EXPIRED));
    if (credit != 0u) {
        printf("  FAIL: writing CREDIT_EXPIRED did not clear it (0x%08x)\n",
               credit);
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: SECONDARY stays parked until synced; registers OK\n\n");
    } else {
        printf("\nFAIL: octs_system_timer SECONDARY test failed\n\n");
    }
    return pass ? 0 : 1;
}
