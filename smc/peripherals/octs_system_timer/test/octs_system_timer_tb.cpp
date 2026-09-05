// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file octs_system_timer_tb.cpp
 * @brief Self-checking unit testbench for the OCTS System Timer model.
 *
 * ## Topology
 *
 * Three DUT instances share one clock and reset:
 *
 * ```
 *            +--------------+   sync_load   +--------------+
 *   drv_p -->|    dut_p     |-------------->|    dut_s     |<-- drv_s
 *            |  (PRIMARY)   |    credit     | (SECONDARY)  |
 *            +--------------+-------------->+--------------+
 *
 *            +--------------+
 *   drv_x -->|    dut_x     |<-- sync_load / credit driven directly by the TB
 *            | (SECONDARY)  |
 *            +--------------+
 * ```
 *
 * `dut_p` -> `dut_s` is the end-to-end OCTS protocol check.  `dut_x` is a
 * standalone SECONDARY whose sync inputs the TB drives by hand, so credit
 * starvation and the credit re-anchor can be provoked deterministically.
 *
 * ## Cycle alignment
 *
 * The stimulus thread runs on the clock's **falling** edge, i.e. halfway
 * between the rising edges the DUT evaluates on.  So `advance(1)` == one clock
 * cycle, and every sample/drive happens at a point where the DUT's state for
 * the preceding rising edge is settled and unambiguous.  This is what makes
 * the exact cycle counts below meaningful rather than delta-cycle-order luck.
 *
 * Register accesses do not consume simulated time (the annotated `b_transport`
 * delay is checked once, then discarded), so a burst of accesses always lands
 * inside a single half-cycle.
 *
 * ## Coverage
 *
 *  1. Reset defaults of every register
 *  2. STATUS.MODE follows `is_primary_i`
 *  3. RW / RO / reserved-bit contracts, GPIO_ENABLE output
 *  4. Access rules: 32-bit only, alignment, out-of-window, annotated delay
 *  5. PRIMARY: START loads the 64-bit preset, RUNNING asserts, START self-clears
 *  6. PRIMARY: sync_load pulse is exactly PULSE_WIDTH cycles
 *  7. PRIMARY: credit pulse width and CREDIT_VAL period
 *  8. PRIMARY -> SECONDARY: count tracks with the modelled sync latency
 *  9. SECONDARY without sync stays idle
 * 10. SECONDARY: sync_load latency, load value, STEP counting
 * 11. SECONDARY: credit exhaustion halts the count, CREDIT_EXPIRED accumulates
 * 12. SECONDARY: credit pulse replenishes; CREDIT_EXPIRED write-to-clear
 * 13. SECONDARY with STEP=3: credit pulse re-anchors count to
 *     `expected_count + CREDIT_VAL` (pulls an over-run counter back)
 * 14. CTRL constraint warning (CREDIT_VAL <= PULSE_WIDTH)
 * 15. CCI `access_delay_ns` preset, discovery and live mutation
 */

#include "octs_system_timer.h"

#include "sim_log.h"

#include <tlm_utils/simple_initiator_socket.h>

#include <cci/utils/consuming_broker.h>
#include <cci_configuration>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

using namespace sc_core;
using smc::octs_system_timer;
using cfg_t = smc::octs_system_timer_cfg;

// ---------------------------------------------------------------------------
// Tiny check framework
// ---------------------------------------------------------------------------

static unsigned g_checks   = 0;
static unsigned g_failures = 0;

static void report_fail(const char* what, int line, const std::string& detail)
{
    ++g_failures;
    std::cout << "  FAIL [line " << line << "] " << what << ": " << detail
              << "\n";
}

template <typename A, typename B>
static void check_eq_(const char* what, A got, B exp, int line)
{
    ++g_checks;
    const auto g = static_cast<uint64_t>(got);
    const auto e = static_cast<uint64_t>(exp);
    if (g != e) {
        std::ostringstream os;
        os << "got 0x" << std::hex << g << " (" << std::dec << g
           << "), expected 0x" << std::hex << e << " (" << std::dec << e << ")";
        report_fail(what, line, os.str());
    }
}

static void check_true_(const char* what, bool cond, int line)
{
    ++g_checks;
    if (!cond) report_fail(what, line, "condition is false");
}

#define CHECK_EQ(what, got, exp) check_eq_((what), (got), (exp), __LINE__)
#define CHECK_TRUE(what, cond)   check_true_((what), (cond), __LINE__)

static void banner(const std::string& title)
{
    std::cout << "\n--- " << title << " ---\n";
}

// ---------------------------------------------------------------------------
// reg_driver: a minimal TLM initiator used to poke a DUT's register file
// ---------------------------------------------------------------------------

class reg_driver : public sc_module {
public:
    tlm_utils::simple_initiator_socket<reg_driver> sock;

    explicit reg_driver(sc_module_name n) : sc_module(n), sock("sock") {}

    /// Raw access; returns the TLM response so negative tests can inspect it.
    tlm::tlm_response_status access(tlm::tlm_command cmd, uint64_t addr,
                                   void* data, unsigned len,
                                   sc_time* out_delay = nullptr)
    {
        tlm::tlm_generic_payload gp;
        sc_time                  d = SC_ZERO_TIME;

        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(static_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_byte_enable_length(0);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sock->b_transport(gp, d);
        if (out_delay != nullptr) *out_delay = d;
        return gp.get_response_status();
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t v = 0;
        const auto st = access(tlm::TLM_READ_COMMAND, addr, &v, 4);
        if (st != tlm::TLM_OK_RESPONSE) {
            ++g_failures;
            std::cout << "  FAIL: read32 @0x" << std::hex << addr
                      << " returned status " << std::dec << st << "\n";
        }
        return v;
    }

    void write32(uint64_t addr, uint32_t v)
    {
        const auto st = access(tlm::TLM_WRITE_COMMAND, addr, &v, 4);
        if (st != tlm::TLM_OK_RESPONSE) {
            ++g_failures;
            std::cout << "  FAIL: write32 @0x" << std::hex << addr
                      << " returned status " << std::dec << st << "\n";
        }
    }
};

// ---------------------------------------------------------------------------
// Testbench
// ---------------------------------------------------------------------------

class tb : public sc_module {
public:
    SC_HAS_PROCESS(tb);

    sc_clock          clk{"clk", 10, SC_NS};
    sc_signal<bool>   rst_n{"rst_n"};
    sc_signal<bool>   tie_low{"tie_low"};

    // PRIMARY under test, plus the SECONDARY it feeds.
    sc_signal<bool>     is_primary_p{"is_primary_p"};
    sc_signal<bool>     is_primary_s{"is_primary_s"};
    sc_signal<bool>     is_primary_x{"is_primary_x"};

    sc_signal<bool>     sl_p2s{"sl_p2s"};
    sc_signal<bool>     cc_p2s{"cc_p2s"};
    sc_signal<uint64_t> count_p{"count_p"};
    sc_signal<bool>     gpio_p{"gpio_p"};
    sc_signal<uint32_t> cred_p{"cred_p"};
    sc_signal<bool>     credleft_p{"credleft_p"};

    sc_signal<bool>     sl_s_o{"sl_s_o"};
    sc_signal<bool>     cc_s_o{"cc_s_o"};
    sc_signal<uint64_t> count_s{"count_s"};
    sc_signal<bool>     gpio_s{"gpio_s"};
    sc_signal<uint32_t> cred_s{"cred_s"};
    sc_signal<bool>     credleft_s{"credleft_s"};

    // Standalone SECONDARY with TB-driven sync inputs.
    sc_signal<bool>     sl_drv{"sl_drv"};
    sc_signal<bool>     cc_drv{"cc_drv"};
    sc_signal<bool>     sl_x_o{"sl_x_o"};
    sc_signal<bool>     cc_x_o{"cc_x_o"};
    sc_signal<uint64_t> count_x{"count_x"};
    sc_signal<bool>     gpio_x{"gpio_x"};
    sc_signal<uint32_t> cred_x{"cred_x"};
    sc_signal<bool>     credleft_x{"credleft_x"};

    octs_system_timer dut_p{"dut_p"};
    octs_system_timer dut_s{"dut_s"};
    octs_system_timer dut_x{"dut_x"};

    reg_driver drv_p{"drv_p"};
    reg_driver drv_s{"drv_s"};
    reg_driver drv_x{"drv_x"};

    explicit tb(sc_module_name n) : sc_module(n)
    {
        drv_p.sock.bind(dut_p.reg_socket);
        drv_s.sock.bind(dut_s.reg_socket);
        drv_x.sock.bind(dut_x.reg_socket);

        bind_common(dut_p, is_primary_p);
        dut_p.timer_sync_load_i(tie_low);
        dut_p.timer_cnt_credit_i(tie_low);
        dut_p.timer_sync_load_o(sl_p2s);
        dut_p.timer_cnt_credit_o(cc_p2s);
        dut_p.timer_count_o(count_p);
        dut_p.timer_gpio_enable_o(gpio_p);
        dut_p.cur_credits_debug_o(cred_p);
        dut_p.credits_left_debug_o(credleft_p);

        bind_common(dut_s, is_primary_s);
        dut_s.timer_sync_load_i(sl_p2s);
        dut_s.timer_cnt_credit_i(cc_p2s);
        dut_s.timer_sync_load_o(sl_s_o);
        dut_s.timer_cnt_credit_o(cc_s_o);
        dut_s.timer_count_o(count_s);
        dut_s.timer_gpio_enable_o(gpio_s);
        dut_s.cur_credits_debug_o(cred_s);
        dut_s.credits_left_debug_o(credleft_s);

        bind_common(dut_x, is_primary_x);
        dut_x.timer_sync_load_i(sl_drv);
        dut_x.timer_cnt_credit_i(cc_drv);
        dut_x.timer_sync_load_o(sl_x_o);
        dut_x.timer_cnt_credit_o(cc_x_o);
        dut_x.timer_count_o(count_x);
        dut_x.timer_gpio_enable_o(gpio_x);
        dut_x.cur_credits_debug_o(cred_x);
        dut_x.credits_left_debug_o(credleft_x);

        SC_THREAD(run);
    }

private:
    void bind_common(octs_system_timer& d, sc_signal<bool>& mode)
    {
        d.clk_i(clk);
        d.rst_n_i(rst_n);
        d.is_primary_i(mode);
    }

    /// Advance @p n whole clock cycles, resuming on the falling edge.
    void advance(unsigned n = 1)
    {
        for (unsigned i = 0; i < n; ++i) wait(clk.negedge_event());
    }

    /// Drive a @p cycles -long pulse on @p sig, leaving the TB one cycle after
    /// the pulse's trailing edge.
    void pulse(sc_signal<bool>& sig, unsigned cycles)
    {
        sig.write(true);
        advance(cycles);
        sig.write(false);
    }

    void apply_reset()
    {
        rst_n.write(false);
        advance(3);
        rst_n.write(true);
        advance(1);
    }

    void run();

    void test_reset_defaults();
    void test_register_contracts();
    void test_access_rules();
    void test_primary_start_and_pulses();
    void test_primary_credit_period();
    void test_primary_to_secondary_sync();
    void test_secondary_load_and_starvation();
    void test_secondary_replenish_and_clear();
    void test_secondary_reanchor_with_step();
    void test_ctrl_constraint_warning();
    void test_cci_parameter();
    void test_tlm_error_paths();
};

// ---------------------------------------------------------------------------

void tb::run()
{
    std::cout << "\n=== octs_system_timer unit tests ===\n";

    tie_low.write(false);
    sl_drv.write(false);
    cc_drv.write(false);
    is_primary_p.write(true);   // PRIMARY
    is_primary_s.write(false);  // SECONDARY
    is_primary_x.write(false);  // SECONDARY

    apply_reset();

    test_reset_defaults();
    test_register_contracts();
    test_access_rules();
    test_primary_start_and_pulses();
    test_primary_credit_period();
    test_primary_to_secondary_sync();
    test_secondary_load_and_starvation();
    test_secondary_replenish_and_clear();
    test_secondary_reanchor_with_step();
    test_ctrl_constraint_warning();
    test_cci_parameter();
    test_tlm_error_paths();

    std::cout << "\n=== " << (g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << ": " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed ===\n\n";

    sc_stop();
}

// --- 1. Reset defaults -----------------------------------------------------

void tb::test_reset_defaults()
{
    banner("1. Reset defaults");

    CHECK_EQ("CTRL reset", drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);
    CHECK_EQ("TIMER_START reads 0", drv_x.read32(cfg_t::OFF_TIMER_START), 0u);
    CHECK_EQ("TIMER_PRESET_LO reset",
             drv_x.read32(cfg_t::OFF_TIMER_PRESET_LO), 0u);
    CHECK_EQ("TIMER_PRESET_HI reset",
             drv_x.read32(cfg_t::OFF_TIMER_PRESET_HI), 0u);
    CHECK_EQ("TIMER_COUNT_LO reset",
             drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO), 0u);
    CHECK_EQ("TIMER_COUNT_HI reset",
             drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI), 0u);
    CHECK_EQ("CREDIT_EXPIRED reset",
             drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);
    CHECK_EQ("TIMER_GPIO_ENABLE reset",
             drv_x.read32(cfg_t::OFF_TIMER_GPIO_ENABLE), 0u);

    // Decoded CTRL fields must match the RDL defaults.
    CHECK_EQ("CTRL.CREDIT_VAL default",  dut_x.credit_val(),  0x0Au);
    CHECK_EQ("CTRL.PULSE_WIDTH default", dut_x.pulse_width(), 0x02u);
    CHECK_EQ("CTRL.STEP default",        dut_x.step(),        0x01u);

    // --- 2. STATUS.MODE follows is_primary_i ---
    CHECK_EQ("STATUS PRIMARY (MODE=0, not running)",
             drv_p.read32(cfg_t::OFF_STATUS), 0u);
    CHECK_EQ("STATUS SECONDARY (MODE=1, not running)",
             drv_x.read32(cfg_t::OFF_STATUS), cfg_t::STATUS_MODE);

    std::cout << "reset defaults and STATUS.MODE OK\n";
}

// --- 3. Register contracts -------------------------------------------------

void tb::test_register_contracts()
{
    banner("3. RW / RO / reserved-bit contracts");

    // CTRL: [23:0] writable, [31:24] reserved -> read-as-zero / write-ignore.
    drv_x.write32(cfg_t::OFF_CTRL, 0xFF030415u);
    CHECK_EQ("CTRL drops reserved [31:24]", drv_x.read32(cfg_t::OFF_CTRL),
             0x00030415u);
    CHECK_EQ("CTRL.CREDIT_VAL written",  dut_x.credit_val(),  0x15u);
    CHECK_EQ("CTRL.PULSE_WIDTH written", dut_x.pulse_width(), 0x04u);
    CHECK_EQ("CTRL.STEP written",        dut_x.step(),        0x03u);
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    CHECK_EQ("CTRL restored", drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);

    // 64-bit preset is two independent 32-bit halves.
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0xDEADBEEFu);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0x12345678u);
    CHECK_EQ("PRESET_LO RW", drv_x.read32(cfg_t::OFF_TIMER_PRESET_LO),
             0xDEADBEEFu);
    CHECK_EQ("PRESET_HI RW", drv_x.read32(cfg_t::OFF_TIMER_PRESET_HI),
             0x12345678u);

    // GPIO_ENABLE is a single bit and drives an output.
    drv_x.write32(cfg_t::OFF_TIMER_GPIO_ENABLE, 0xFFFFFFFFu);
    advance();
    CHECK_EQ("GPIO_ENABLE keeps bit 0 only",
             drv_x.read32(cfg_t::OFF_TIMER_GPIO_ENABLE), 1u);
    CHECK_TRUE("gpio_enable_o asserted", gpio_x.read());
    drv_x.write32(cfg_t::OFF_TIMER_GPIO_ENABLE, 0u);
    advance();
    CHECK_TRUE("gpio_enable_o deasserted", !gpio_x.read());

    // Read-only registers ignore writes.
    drv_x.write32(cfg_t::OFF_STATUS, 0xFFFFFFFFu);
    CHECK_EQ("STATUS is RO", drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE);
    drv_x.write32(cfg_t::OFF_TIMER_COUNT_LO, 0xFFFFFFFFu);
    drv_x.write32(cfg_t::OFF_TIMER_COUNT_HI, 0xFFFFFFFFu);
    CHECK_EQ("COUNT_LO is RO", drv_x.read32(cfg_t::OFF_TIMER_COUNT_LO), 0u);
    CHECK_EQ("COUNT_HI is RO", drv_x.read32(cfg_t::OFF_TIMER_COUNT_HI), 0u);

    std::cout << "register access contracts OK\n";
}

// --- 4. Access rules -------------------------------------------------------

void tb::test_access_rules()
{
    banner("4. Access rules (32-bit only, aligned, in-window)");

    uint32_t buf = 0;
    sc_time  delay = SC_ZERO_TIME;

    CHECK_EQ("aligned 32-bit access is OK",
             drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::OFF_TIMER_GPIO_ENABLE,
                          &buf, 4, &delay),
             tlm::TLM_OK_RESPONSE);
    CHECK_TRUE("b_transport annotates a delay", delay > SC_ZERO_TIME);

    CHECK_EQ("byte access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x00, &buf, 1),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("halfword access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x00, &buf, 2),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("misaligned 32-bit access rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, 0x02, &buf, 4),
             tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK_EQ("access past the window rejected",
             drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::WINDOW_SIZE, &buf, 4),
             tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK_EQ("last register in window is reachable",
             drv_x.access(tlm::TLM_READ_COMMAND,
                          cfg_t::WINDOW_SIZE - 4, &buf, 4),
             tlm::TLM_OK_RESPONSE);

    std::cout << "access rules OK\n";
}

// --- 5/6. PRIMARY start, 64-bit preset load, sync_load pulse width ---------

void tb::test_primary_start_and_pulses()
{
    banner("5/6. PRIMARY start, 64-bit preset load, sync_load pulse width");

    // Straddle the 32-bit boundary so both preset/count halves are exercised.
    const uint64_t preset = 0x0000'0001'0000'1000ULL;

    // Programming sequence from memmap.adoc: load the *same* preset into every
    // participating timer, then start the PRIMARY.  The SECONDARY latches its
    // own preset when the sync_load pulse arrives, so dut_s must be programmed
    // too -- otherwise it would load 0 and track an offset timeline.
    for (reg_driver* drv : {&drv_p, &drv_s}) {
        drv->write32(cfg_t::OFF_TIMER_PRESET_LO, 0x00001000u);
        drv->write32(cfg_t::OFF_TIMER_PRESET_HI, 0x00000001u);
    }
    drv_p.write32(cfg_t::OFF_TIMER_START, 0x1u);

    advance();  // the rising edge that consumes the START singlepulse

    CHECK_EQ("count loaded from 64-bit preset", count_p.read(), preset);
    CHECK_EQ("COUNT_LO after load", drv_p.read32(cfg_t::OFF_TIMER_COUNT_LO),
             0x00001000u);
    CHECK_EQ("COUNT_HI after load", drv_p.read32(cfg_t::OFF_TIMER_COUNT_HI),
             0x00000001u);
    CHECK_EQ("TIMER_START self-clears", drv_p.read32(cfg_t::OFF_TIMER_START),
             0u);
    CHECK_EQ("STATUS.RUNNING asserted", drv_p.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_RUNNING);
    CHECK_TRUE("sync_load_o asserted on cycle 1", sl_p2s.read());

    // PULSE_WIDTH = 2 -> asserted for exactly two cycles.
    advance();
    CHECK_EQ("count increments by 1 in PRIMARY", count_p.read(), preset + 1);
    CHECK_TRUE("sync_load_o still asserted on cycle 2", sl_p2s.read());
    advance();
    CHECK_EQ("count keeps incrementing", count_p.read(), preset + 2);
    CHECK_TRUE("sync_load_o deasserted after PULSE_WIDTH cycles",
               !sl_p2s.read());

    // A PRIMARY never drives its credit budget or expiry counters.
    CHECK_EQ("PRIMARY reports no expired credits",
             drv_p.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);

    std::cout << "PRIMARY start + sync_load pulse OK (count=0x" << std::hex
              << count_p.read() << std::dec << ")\n";
}

// --- 7. PRIMARY credit pulse width and period ------------------------------

void tb::test_primary_credit_period()
{
    banner("7. PRIMARY credit pulse width and CREDIT_VAL period");

    const unsigned expect_width  = dut_p.eff_pulse_width();  // 2
    const unsigned expect_period = dut_p.credit_val();       // 10
    const unsigned kBound        = 200;

    unsigned n = 0;
    while (!cc_p2s.read() && n < kBound) { advance(); ++n; }
    CHECK_TRUE("credit pulse observed", cc_p2s.read());

    unsigned width = 0;
    while (cc_p2s.read() && width < kBound) { advance(); ++width; }
    CHECK_EQ("credit pulse width == PULSE_WIDTH", width, expect_width);

    unsigned gap = 0;
    while (!cc_p2s.read() && gap < kBound) { advance(); ++gap; }
    CHECK_TRUE("second credit pulse observed", cc_p2s.read());
    CHECK_EQ("credit pulse period == CREDIT_VAL", width + gap, expect_period);

    // sync_load is a one-shot per START; it must not re-fire with credits.
    CHECK_TRUE("sync_load_o stays low after its one-shot", !sl_p2s.read());

    std::cout << "credit pulse width=" << width << " period=" << (width + gap)
              << " OK\n";
}

// --- 8. PRIMARY -> SECONDARY synchronisation --------------------------------

void tb::test_primary_to_secondary_sync()
{
    banner("8. PRIMARY -> SECONDARY count tracking");

    const unsigned kThreePeriods = 3u * dut_p.credit_val();

    // Let a few credit periods elapse so the SECONDARY has been re-anchored
    // by real credit pulses, not just by its initial sync_load.
    advance(kThreePeriods);

    CHECK_EQ("SECONDARY reports MODE=1 and RUNNING",
             drv_s.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE | cfg_t::STATUS_RUNNING);
    CHECK_TRUE("SECONDARY has credits left", credleft_s.read());
    CHECK_EQ("SECONDARY never starved", drv_s.read32(cfg_t::OFF_CREDIT_EXPIRED),
             0u);

    // The SECONDARY trails by exactly the input-synchronizer depth, and that
    // offset is constant: the credit protocol keeps re-anchoring it.
    for (unsigned i = 0; i < kThreePeriods; ++i) {
        const uint64_t p = count_p.read();
        const uint64_t s = count_s.read();
        CHECK_EQ("SECONDARY trails PRIMARY by SYNC_LATENCY_CYCLES", p - s,
                 octs_system_timer::SYNC_LATENCY_CYCLES);
        CHECK_TRUE("SECONDARY within its credit budget",
                   (p - s) <= dut_p.credit_val());
        advance();
    }

    std::cout << "PRIMARY=0x" << std::hex << count_p.read() << " SECONDARY=0x"
              << count_s.read() << std::dec << " (constant lag of "
              << octs_system_timer::SYNC_LATENCY_CYCLES << " cycles) OK\n";
}

// --- 9/10/11. SECONDARY load, STEP counting, credit starvation -------------

void tb::test_secondary_load_and_starvation()
{
    banner("9/10/11. SECONDARY load latency, STEP counting, credit starvation");

    // 9. A SECONDARY that never sees a sync_load never leaves 0.
    CHECK_EQ("idle SECONDARY count stays 0", count_x.read(), 0u);
    CHECK_EQ("idle SECONDARY is not RUNNING", drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE);

    // Faithful to the RTL: a SECONDARY's credit accounting is gated only on
    // `~is_primary_i`, not on the timer being started.  An un-synced SECONDARY
    // therefore drains its budget and piles up a meaningless CREDIT_EXPIRED --
    // which is why memmap.adoc has software clear the register.
    CHECK_TRUE("un-synced SECONDARY drains its credit budget",
               !credleft_x.read());
    CHECK_TRUE("un-synced SECONDARY accumulates a stale CREDIT_EXPIRED",
               drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED) > 0);

    const uint64_t preset = 0x200;
    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);  // CV=10, PW=2, STEP=1
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, static_cast<uint32_t>(preset));
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0u);

    // 10. A sync_load pulse is consumed SYNC_LATENCY_CYCLES rising edges after
    // it is driven (input flop + 2FF synchronizer + edge-detect flop).
    pulse(sl_drv, 2);
    CHECK_EQ("count still 0 before the synchronizer drains", count_x.read(),
             0u);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("SECONDARY loads preset after SYNC_LATENCY_CYCLES",
             count_x.read(), preset);
    CHECK_EQ("SECONDARY is RUNNING after sync_load",
             drv_x.read32(cfg_t::OFF_STATUS),
             cfg_t::STATUS_MODE | cfg_t::STATUS_RUNNING);
    CHECK_EQ("expected_count anchored to preset", dut_x.dbg_expected_count(),
             preset);
    CHECK_EQ("sync_load clears the expiry counter", dut_x.dbg_credit_expired(),
             0u);

    // Drop the stale pre-sync peak so the starvation run below is measured
    // from zero.
    drv_x.write32(cfg_t::OFF_CREDIT_EXPIRED, 0u);

    advance();
    CHECK_EQ("SECONDARY advances by STEP", count_x.read(),
             preset + dut_x.step());

    // 11. With no credit pulses the budget drains and the counter halts once
    // cur_credits reaches CREDIT_VAL.
    const uint64_t cv = dut_x.credit_val();
    advance(static_cast<unsigned>(cv - 1));
    CHECK_EQ("count halts at preset + CREDIT_VAL", count_x.read(), preset + cv);
    CHECK_EQ("credit budget exhausted", dut_x.dbg_cur_credits(), cv);
    CHECK_TRUE("credits_left_debug_o deasserted", !credleft_x.read());

    advance(5);
    CHECK_EQ("count stays halted while starved", count_x.read(), preset + cv);
    const uint32_t expired_a = drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED);
    CHECK_TRUE("CREDIT_EXPIRED accumulates while starved", expired_a > 0);

    advance(4);
    const uint32_t expired_b = drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED);
    CHECK_TRUE("CREDIT_EXPIRED tracks a growing starvation run",
               expired_b > expired_a);

    std::cout << "starvation OK (count halted at 0x" << std::hex
              << count_x.read() << std::dec << ", CREDIT_EXPIRED "
              << expired_a << " -> " << expired_b << ")\n";
}

// --- 12. Credit replenish and CREDIT_EXPIRED write-to-clear ----------------

void tb::test_secondary_replenish_and_clear()
{
    banner("12. Credit replenish and CREDIT_EXPIRED write-to-clear");

    const uint64_t halted = count_x.read();

    pulse(cc_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);

    CHECK_EQ("credit pulse clears the expiry counter",
             dut_x.dbg_credit_expired(), 0u);
    CHECK_TRUE("credits available again", credleft_x.read());

    advance(3);
    CHECK_TRUE("counter resumes after replenish", count_x.read() > halted);

    // "To reset the value of this register, write anything to it."
    drv_x.write32(cfg_t::OFF_CREDIT_EXPIRED, 0xA5A5A5A5u);
    CHECK_EQ("CREDIT_EXPIRED cleared by a write",
             drv_x.read32(cfg_t::OFF_CREDIT_EXPIRED), 0u);

    std::cout << "replenish + CREDIT_EXPIRED clear OK (count=0x" << std::hex
              << count_x.read() << std::dec << ")\n";
}

// --- 13. Credit pulse re-anchors an over-run counter ------------------------

void tb::test_secondary_reanchor_with_step()
{
    banner("13. Credit pulse re-anchors count to expected_count + CREDIT_VAL");

    apply_reset();

    // STEP=3 does not divide CREDIT_VAL=10, so the free-running counter
    // overshoots the PRIMARY's timeline and the credit pulse must pull it back.
    const uint32_t ctrl = (3u << 16) | (2u << 8) | 10u;
    drv_x.write32(cfg_t::OFF_CTRL, ctrl);
    const uint64_t preset = 0x100;
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, static_cast<uint32_t>(preset));
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_HI, 0u);

    pulse(sl_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("loaded preset", count_x.read(), preset);

    // cur_credits: 0 -> 3 -> 6 -> 9 -> 12, so the counter runs to preset+12
    // and then halts one cycle later.
    advance(4);
    CHECK_EQ("counter overshoots to preset + 12", count_x.read(), preset + 12);
    advance();
    CHECK_EQ("counter halts past the budget", count_x.read(), preset + 12);
    CHECK_EQ("cur_credits overshot CREDIT_VAL", dut_x.dbg_cur_credits(), 12u);

    pulse(cc_drv, 2);
    advance(octs_system_timer::SYNC_LATENCY_CYCLES - 2);
    CHECK_EQ("credit pulse re-anchors count to expected + CREDIT_VAL",
             count_x.read(), preset + 10);
    CHECK_EQ("expected_count advances by CREDIT_VAL",
             dut_x.dbg_expected_count(), preset + 10);
    CHECK_EQ("credit budget reset", dut_x.dbg_cur_credits(), 0u);

    std::cout << "re-anchor OK (0x" << std::hex << (preset + 12) << " -> 0x"
              << count_x.read() << std::dec << ")\n";
    dut_x.dump_state();
}

// --- 14. CTRL constraint warning -------------------------------------------

void tb::test_ctrl_constraint_warning()
{
    banner("14. CTRL constraint check (CREDIT_VAL > PULSE_WIDTH)");

    std::cout << "(one SC_WARNING about CREDIT_VAL <= PULSE_WIDTH is expected "
                 "below)\n";

    const int before = sc_report_handler::get_count(SC_WARNING);
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (8u << 8) | 4u);  // CV=4 <= PW=8
    const int after = sc_report_handler::get_count(SC_WARNING);

    CHECK_TRUE("illegal CREDIT_VAL/PULSE_WIDTH combination warns",
               after > before);

    drv_x.write32(cfg_t::OFF_CTRL, cfg_t::CTRL_RESET);
    CHECK_EQ("CTRL restored after the negative test",
             drv_x.read32(cfg_t::OFF_CTRL), cfg_t::CTRL_RESET);

    std::cout << "CTRL constraint check OK\n";
}

// --- 15. CCI parameter discovery and live mutation -------------------------

void tb::test_cci_parameter()
{
    banner("15. CCI access_delay_ns discovery and live mutation");

    auto broker = cci::cci_get_broker();
    auto handle = broker.get_param_handle("tb.dut_x.access_delay_ns");

    CHECK_TRUE("access_delay_ns is discoverable via CCI", handle.is_valid());
    if (!handle.is_valid()) return;

    auto delay_ns_of_a_read = [this]() {
        uint32_t buf   = 0;
        sc_time  delay = SC_ZERO_TIME;
        drv_x.access(tlm::TLM_READ_COMMAND, cfg_t::OFF_CTRL, &buf, 4, &delay);
        return delay.to_seconds() * 1e9;
    };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };

    // sc_main presets this to 5.0 before elaboration.
    CHECK_TRUE("preset applied", near(handle.get_cci_value().get_double(), 5.0));
    CHECK_TRUE("annotated delay matches the preset",
               near(delay_ns_of_a_read(), 5.0));

    handle.set_cci_value(cci::cci_value(3.0));
    CHECK_TRUE("annotated delay follows a run-time override",
               near(delay_ns_of_a_read(), 3.0));

    std::cout << "CCI parameter discovery + mutation OK\n";
}

void tb::test_tlm_error_paths()
{
    banner("16. TLM error responses and dump_state CREDIT branch");

    uint32_t buf = 0;
    tlm::tlm_generic_payload gp;
    sc_time delay = SC_ZERO_TIME;

    gp.set_command(tlm::TLM_READ_COMMAND);
    gp.set_address(cfg_t::OFF_CTRL);
    gp.set_data_ptr(nullptr);
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    gp.set_byte_enable_ptr(nullptr);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("null data_ptr is GENERIC_ERROR", gp.get_response_status(),
             tlm::TLM_GENERIC_ERROR_RESPONSE);

    gp.set_data_ptr(reinterpret_cast<unsigned char*>(&buf));
    gp.set_data_length(0);
    gp.set_streaming_width(0);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("zero length is GENERIC_ERROR", gp.get_response_status(),
             tlm::TLM_GENERIC_ERROR_RESPONSE);

    uint8_t be[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    gp.set_data_length(4);
    gp.set_streaming_width(4);
    gp.set_byte_enable_ptr(be);
    gp.set_byte_enable_length(4);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("byte enables are BYTE_ENABLE_ERROR", gp.get_response_status(),
             tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);

    gp.set_byte_enable_ptr(nullptr);
    gp.set_byte_enable_length(0);
    gp.set_streaming_width(1);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("streaming_width < len is BURST_ERROR", gp.get_response_status(),
             tlm::TLM_BURST_ERROR_RESPONSE);

    gp.set_streaming_width(4);
    gp.set_command(tlm::TLM_IGNORE_COMMAND);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    drv_x.sock->b_transport(gp, delay);
    CHECK_EQ("IGNORE command is COMMAND_ERROR", gp.get_response_status(),
             tlm::TLM_COMMAND_ERROR_RESPONSE);

    // Reserved-hole default in reg_read/reg_write: 0x24 is past the window
    // (ADDRESS_ERROR). There is no aligned hole inside WINDOW_SIZE.

    apply_reset();
    drv_x.write32(cfg_t::OFF_CTRL, (1u << 16) | (2u << 8) | 10u);
    drv_x.write32(cfg_t::OFF_TIMER_PRESET_LO, 0x100u);
    drv_x.write32(cfg_t::OFF_TIMER_START, 0x1u);
    pulse(cc_drv, 2);
    advance(1);
    dut_x.dump_state();

    std::cout << "TLM error paths OK\n";
}

// ---------------------------------------------------------------------------

int sc_main(int, char*[])
{
    simlog::set_level(simlog::level::info);

    // CCI parameters must be constructed against a registered broker.
    static cci_utils::consuming_broker broker("GlobalBroker");
    cci::cci_register_broker(broker);

    cci::cci_originator tb_cfg("tb_cfg");
    auto global_broker = cci::cci_get_global_broker(tb_cfg);
    global_broker.set_preset_cci_value("tb.dut_x.access_delay_ns",
                                       cci::cci_value(5.0));

    tb top("tb");
    sc_start();  // the stimulus thread calls sc_stop() when it is done

    return g_failures == 0 ? 0 : 1;
}
