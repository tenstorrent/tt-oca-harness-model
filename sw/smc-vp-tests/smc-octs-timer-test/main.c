/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smc-vp-tests/smc-octs-timer-test/main.c
 *
 * octs_system_timer (OCTS System Timer) integration test over the SMC fabric.
 *
 * Mirrors the RTL firmware OCTS tests in tt-oca-hw-main/fw/smc/tests -- mainly
 * octs_p0_primary_test (CTRL/GPIO init, preset readback, start, counter
 * monotonicity) and the PRIMARY-mode half of octs_p0_credit_test
 * (CREDIT_EXPIRED must stay 0 on a primary) -- adapted to the smc-vp harness:
 * printf over UART0 instead of simputs/write_scratch, and one `pass` flag
 * instead of hanging in a wfi loop on the first error, so a single run reports
 * every failure.
 *
 * The SMC's timer is strapped as the system timekeeping PRIMARY (is_primary_i
 * tied high in smc_platform.cpp), so this covers the primary datapath only.
 * SECONDARY behaviour needs a sync_load/credit source, which no other block in
 * the SMC VP drives; it is covered by the IP's unit testbench, which pairs a
 * primary with a secondary
 * (smc/peripherals/octs_system_timer/test/octs_system_timer_tb.cpp).
 *
 * Counter reads have to be spaced out in time: the timer advances one tick per
 * rising edge of a 10 ns clock (octs_clk_period_ns), while the LT cluster
 * charges only ~1 ns per retired instruction, so two back-to-back loads can
 * easily land inside the same timer cycle and read the same value.  spin()
 * burns enough instructions to guarantee the count moves.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

#define TIMER(off) (SMC_OCTS_TIMER_BASE + (off))

/* Enough retired instructions to cover many 10 ns timer cycles. */
#define SPIN_ITERS 200u

static void spin(unsigned iters)
{
    for (volatile unsigned i = 0; i < iters; i++) {
        __asm__ volatile("nop");
    }
}

static uint64_t timer_count(void)
{
    /* Non-atomic LO-then-HI read, exactly as the RTL firmware does it.  HI
     * stays 0 for the whole test (a 32-bit rollover needs ~43 s at 10 ns), so
     * no LO-wrap re-read handshake is needed. */
    uint32_t lo = REG_READ(TIMER(OCTS_TIMER_COUNT_LO));
    uint32_t hi = REG_READ(TIMER(OCTS_TIMER_COUNT_HI));
    return ((uint64_t)hi << 32) | lo;
}

static void timer_set_preset(uint64_t preset)
{
    REG_WRITE(TIMER(OCTS_TIMER_PRESET_LO), (uint32_t)(preset & 0xFFFFFFFFu));
    REG_WRITE(TIMER(OCTS_TIMER_PRESET_HI), (uint32_t)(preset >> 32));
}

static uint64_t timer_get_preset(void)
{
    uint32_t lo = REG_READ(TIMER(OCTS_TIMER_PRESET_LO));
    uint32_t hi = REG_READ(TIMER(OCTS_TIMER_PRESET_HI));
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    printf("\n=== SMC octs_system_timer test ===\n\n");

    int pass = 1;

    /* -------- 1. Reset defaults ------------------------------------------- */
    uint32_t ctrl   = REG_READ(TIMER(OCTS_CTRL));
    uint32_t status = REG_READ(TIMER(OCTS_STATUS));
    uint32_t credit = REG_READ(TIMER(OCTS_CREDIT_EXPIRED));
    uint32_t gpio   = REG_READ(TIMER(OCTS_TIMER_GPIO_ENABLE));

    printf("reset state: CTRL=0x%08x STATUS=0x%08x CREDIT_EXPIRED=0x%08x "
           "GPIO_ENABLE=0x%08x\n", ctrl, status, credit, gpio);

    if (ctrl != OCTS_CTRL_RESET) {
        printf("  FAIL: CTRL reset value 0x%08x, expected 0x%08x\n",
               ctrl, OCTS_CTRL_RESET);
        pass = 0;
    }
    /* The SMC instance is the timekeeping PRIMARY, and nothing has started it
     * yet, so both MODE and RUNNING must be clear. */
    if ((status & OCTS_STATUS_MODE) != 0) {
        printf("  FAIL: STATUS.MODE set (SECONDARY); expected PRIMARY\n");
        pass = 0;
    }
    if ((status & OCTS_STATUS_RUNNING) != 0) {
        printf("  FAIL: STATUS.RUNNING set before START\n");
        pass = 0;
    }
    if (credit != 0u || gpio != 0u || timer_get_preset() != 0ull ||
        timer_count() != 0ull) {
        printf("  FAIL: non-zero reset state in CREDIT_EXPIRED / GPIO_ENABLE / "
               "PRESET / COUNT\n");
        pass = 0;
    }

    /* -------- 2. CTRL is RW; [31:24] reserved (RAZ/WI) -------------------- */
    REG_WRITE(TIMER(OCTS_CTRL), 0xDEADBEEFu);
    ctrl = REG_READ(TIMER(OCTS_CTRL));
    printf("CTRL after writing 0xDEADBEEF: 0x%08x\n", ctrl);
    if (ctrl != (0xDEADBEEFu & OCTS_CTRL_WMASK)) {
        printf("  FAIL: CTRL should hold 0x%08x (reserved [31:24] RAZ/WI)\n",
               0xDEADBEEFu & OCTS_CTRL_WMASK);
        pass = 0;
    }

    /* Restore the reset programming the way timer_init() does in the RTL
     * firmware tests: credit_val=0x0A, pulse_width=0x02, step=0x01. */
    REG_WRITE(TIMER(OCTS_CTRL), OCTS_CTRL_RESET);
    ctrl = REG_READ(TIMER(OCTS_CTRL));
    if (ctrl != OCTS_CTRL_RESET) {
        printf("  FAIL: CTRL not restored to 0x%08x, got 0x%08x\n",
               OCTS_CTRL_RESET, ctrl);
        pass = 0;
    }

    /* -------- 3. TIMER_GPIO_ENABLE ---------------------------------------- */
    /* The RTL tests set this to keep the GPIO pad interface from X-propagating
     * out of reset; here it just has to read back and drive gpio_enable_o. */
    REG_WRITE(TIMER(OCTS_TIMER_GPIO_ENABLE), 1u);
    gpio = REG_READ(TIMER(OCTS_TIMER_GPIO_ENABLE));
    printf("GPIO_ENABLE after write 1: 0x%08x\n", gpio);
    if ((gpio & 0x1u) != 1u) {
        printf("  FAIL: GPIO_ENABLE did not latch\n");
        pass = 0;
    }

    /* -------- 4. PRESET is a plain 64-bit RW pair ------------------------- */
    const uint64_t preset_pat = 0x00000012ABCD0000ull;
    timer_set_preset(preset_pat);
    uint64_t preset_rb = timer_get_preset();
    printf("PRESET readback: 0x%08x%08x\n",
           (uint32_t)(preset_rb >> 32), (uint32_t)preset_rb);
    if (preset_rb != preset_pat) {
        printf("  FAIL: PRESET readback mismatch (expected 0x%08x%08x)\n",
               (uint32_t)(preset_pat >> 32), (uint32_t)preset_pat);
        pass = 0;
    }

    /* -------- 5. TIMER_START is a singlepulse ----------------------------- */
    /* Reads as 0 even right after writing 1 -- the write arms a one-cycle
     * pulse rather than latching a bit. */
    const uint64_t preset = 0x1000ull;
    timer_set_preset(preset);
    REG_WRITE(TIMER(OCTS_TIMER_START), 1u);
    uint32_t start_rb = REG_READ(TIMER(OCTS_TIMER_START));
    if (start_rb != 0u) {
        printf("  FAIL: TIMER_START reads back 0x%08x, expected 0 "
               "(singlepulse)\n", start_rb);
        pass = 0;
    }

    /* -------- 6/7. START loads the preset, sets RUNNING, count climbs ----- */
    /* Sampled in one uninterrupted run: printf() drives the UART model and
     * costs far more simulated time than these spin loops, so any print
     * between two samples would swamp the interval comparison below. */
    spin(SPIN_ITERS);
    status = REG_READ(TIMER(OCTS_STATUS));
    uint64_t c0 = timer_count();
    spin(SPIN_ITERS);
    uint64_t c1 = timer_count();
    spin(SPIN_ITERS * 4u);
    uint64_t c2 = timer_count();

    printf("after START: STATUS=0x%08x COUNT=0x%08x%08x (preset 0x%x)\n",
           status, (uint32_t)(c0 >> 32), (uint32_t)c0, (uint32_t)preset);

    if ((status & OCTS_STATUS_RUNNING) == 0) {
        printf("  FAIL: STATUS.RUNNING clear after START\n");
        pass = 0;
    }
    if (c0 < preset) {
        printf("  FAIL: COUNT 0x%08x below the loaded preset 0x%08x\n",
               (uint32_t)c0, (uint32_t)preset);
        pass = 0;
    }

    /* Same shape as test_counter_monotonicity() in octs_p0_primary_test. */
    printf("monotonicity samples: 0x%x -> 0x%x -> 0x%x\n",
           (uint32_t)c0, (uint32_t)c1, (uint32_t)c2);
    if (c1 <= c0 || c2 <= c1) {
        printf("  FAIL: COUNT not strictly increasing\n");
        pass = 0;
    }
    /* A PRIMARY increments by 1 per clock, so the 4x-longer window must advance
     * the count further than the short one.  Compared as a relative ordering
     * rather than an absolute cycle count so it stays valid if the clock period
     * or the cluster's per-instruction cost changes.  Both intervals also carry
     * the fixed cost of a timer_count() pair, which is why the spin ratio is
     * 4x rather than 2x -- that overhead is comparable to spin(SPIN_ITERS). */
    if ((c2 - c1) <= (c1 - c0)) {
        printf("  FAIL: COUNT advanced 0x%x over the 4x-longer window vs 0x%x "
               "over the short one\n",
               (uint32_t)(c2 - c1), (uint32_t)(c1 - c0));
        pass = 0;
    }

    /* -------- 8. Read-only registers ignore writes ------------------------ */
    /* STATUS and the live COUNT are RO/WI: the writes below must not stick,
     * and must not fault the bus. */
    uint64_t before = timer_count();
    REG_WRITE(TIMER(OCTS_STATUS), 0xFFFFFFFFu);
    REG_WRITE(TIMER(OCTS_TIMER_COUNT_LO), 0u);
    REG_WRITE(TIMER(OCTS_TIMER_COUNT_HI), 0xFFFFFFFFu);

    status = REG_READ(TIMER(OCTS_STATUS));
    uint64_t after = timer_count();
    if ((status & ~(OCTS_STATUS_MODE | OCTS_STATUS_RUNNING)) != 0u) {
        printf("  FAIL: STATUS accepted a write (0x%08x)\n", status);
        pass = 0;
    }
    if (after < before) {
        printf("  FAIL: COUNT was clobbered by a write (0x%x -> 0x%x)\n",
               (uint32_t)before, (uint32_t)after);
        pass = 0;
    }
    printf("RO writes ignored: STATUS=0x%08x COUNT kept climbing to 0x%x\n",
           status, (uint32_t)after);

    /* -------- 9. CREDIT_EXPIRED stays 0 on a PRIMARY ---------------------- */
    /* octs_p0_credit_test watches this register across several credit periods.
     * A primary is never credit-starved (it owns the timeline and only emits
     * credits), so the peak must remain 0 no matter how long it runs. */
    spin(SPIN_ITERS * 4u);
    credit = REG_READ(TIMER(OCTS_CREDIT_EXPIRED));
    printf("CREDIT_EXPIRED after several credit periods: 0x%08x\n", credit);
    if (credit != 0u) {
        printf("  FAIL: CREDIT_EXPIRED 0x%08x on a PRIMARY, expected 0\n",
               credit);
        pass = 0;
    }
    /* Writing any value clears it -- harmless here, but exercises the W0 path. */
    REG_WRITE(TIMER(OCTS_CREDIT_EXPIRED), 0x5A5A5A5Au);
    if (REG_READ(TIMER(OCTS_CREDIT_EXPIRED)) != 0u) {
        printf("  FAIL: writing CREDIT_EXPIRED did not clear it\n");
        pass = 0;
    }

    /* -------- 10. Re-START reloads the preset ----------------------------- */
    /* Reloading a preset well below the live count must pull the count back
     * down -- proof that START re-loads rather than resuming. */
    const uint64_t preset2 = 0x100ull;
    uint64_t pre_restart = timer_count();
    timer_set_preset(preset2);
    REG_WRITE(TIMER(OCTS_TIMER_START), 1u);
    spin(SPIN_ITERS);
    uint64_t c3 = timer_count();
    printf("re-START with preset 0x%x: COUNT 0x%x -> 0x%x\n",
           (uint32_t)preset2, (uint32_t)pre_restart, (uint32_t)c3);
    if (pre_restart <= preset2) {
        printf("  FAIL: COUNT 0x%x never passed the new preset 0x%x, so the "
               "reload check proves nothing\n",
               (uint32_t)pre_restart, (uint32_t)preset2);
        pass = 0;
    } else if (c3 >= pre_restart) {
        printf("  FAIL: COUNT did not reload from preset on re-START\n");
        pass = 0;
    }
    if (c3 < preset2) {
        printf("  FAIL: COUNT 0x%x below the reloaded preset 0x%x\n",
               (uint32_t)c3, (uint32_t)preset2);
        pass = 0;
    }

    if (pass) {
        printf("\nPASS: octs_system_timer register map + PRIMARY counter work\n\n");
    } else {
        printf("\nFAIL: octs_system_timer test failed\n\n");
    }
    return pass ? 0 : 1;
}
