// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// clint_tick_tb.cpp — focused test for the CLINT auto-tick engine.
//
// This binary is intentionally separate from clint_tb.cpp because the
// deterministic main TB forces tick_period_ns = 0 for repeatable register
// access tests.  Here we enable the real timer with tick_period_ns = 50 ns
// and exercise the three paths that stay uncovered in the main TB:
//
//   • clint.cpp line 121-122  — constructor schedules first tick_event_
//   • clint.cpp line 151-154  — reset_proc cancels and re-arms the event
//   • clint.cpp line 168-179  — tick_method increments MTIME and re-arms
//
// Convention: prints "ALL TESTS PASSED" on success.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iostream>
#include <string>

#include "clint.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                     \
    } while (0)

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected=" << _e << " actual=" << _a               \
                      << "  (" #expected " == " #actual ")\n";                 \
            ++g_failures;                                                      \
        }                                                                     \
    } while (0)

// ---------------------------------------------------------------------------
// Minimal TLM initiator — only the read/write helpers needed here.
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    void write64(uint64_t addr, uint64_t val) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&val));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
    }
};

constexpr uint64_t mtimecmp_addr(unsigned h) {
    return smc::clint_cfg::MTIMECMP_BASE + smc::clint_cfg::MTIMECMP_STRIDE * h;
}

// ---------------------------------------------------------------------------
// Test bench
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_harts_p_;
    cci::cci_param<double,   cci::CCI_IMMUTABLE_PARAM> tick_period_ns_p_;

    smc::clint dut;
    driver     drv;

    sc_core::sc_signal<bool>                     rst_n;
    sc_core::sc_vector<sc_core::sc_signal<bool>> msip_sig;
    sc_core::sc_vector<sc_core::sc_signal<bool>> mtip_sig;

    explicit tb(sc_module_name n)
        : sc_module(n)
        , num_harts_p_("num_harts", 4u, "TB array size, must match preset.")
        , tick_period_ns_p_("tick_period_ns", 100.0, "Mirror of DUT preset.")
        , dut("clint")
        , drv("drv")
        , rst_n("rst_n")
        , msip_sig("msip_sig", num_harts_p_.get_value())
        , mtip_sig("mtip_sig", num_harts_p_.get_value())
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        for (unsigned h = 0; h < num_harts_p_.get_value(); ++h) {
            dut.msip_o[h](msip_sig[h]);
            dut.mtip_o[h](mtip_sig[h]);
        }
        SC_THREAD(run);
    }

    // One delta cycle settle after a signal write.
    static void settle() { sc_core::wait(SC_ZERO_TIME); }

    // Active-low reset pulse: covers reset_proc cancel+re-arm (lines 151-154).
    void pulse_reset() {
        rst_n.write(false);
        sc_core::wait(10, SC_NS);
        rst_n.write(true);
        sc_core::wait(10, SC_NS);
    }

    void run() {
        const double period_ns = tick_period_ns_p_.get_value();
        std::cout << "==== SMC CLINT Auto-Tick TB ====\n";
        std::cout << "  tick_period_ns=" << period_ns << "\n";

        // ------------------------------------------------------------------
        // TA-1. Constructor schedules first tick (lines 121-122).
        //       After bringing the DUT out of reset, MTIME must advance
        //       by itself within a few tick periods.
        // ------------------------------------------------------------------
        rst_n.write(true);
        sc_core::wait(1, SC_NS);

        // Grab MTIME now (should be 0 — no reset was issued yet, but MTIME
        // defaults to 0 and the constructor tick hasn't fired because rst_n
        // was X / false until above write).  Pulse reset for clean state.
        pulse_reset();
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());

        // Wait for 10 full tick periods from the moment reset de-asserts.
        // Reset re-arms tick_event_ for +period_ns from rst_n-low time.
        // The first tick fires ≈period_ns after the reset assertion (not
        // the de-assertion), so 10 more periods from now yields ≈10 ticks.
        sc_core::wait(sc_time(period_ns * 10 + 5.0, SC_NS));
        const uint64_t mtime_a = dut.dbg_mtime();
        EXPECT_TRUE(mtime_a >= 9);   // at least 9 of the 10 ticks landed
        std::cout << "  [PASS] MTIME advanced to " << mtime_a
                  << " after 10 tick periods (auto-tick + constructor schedule)\n";

        // ------------------------------------------------------------------
        // TA-2. reset_proc cancels and re-arms tick_event_ (lines 151-154).
        //       Asserting reset while a tick is pending must not cause an
        //       extra increment; MTIME returns to 0 and counting restarts.
        // ------------------------------------------------------------------
        pulse_reset();
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());

        // After reset de-asserts the first tick fires in ≈period_ns.
        // Wait half a period — tick must not have fired yet.
        sc_core::wait(sc_time(period_ns * 0.4, SC_NS));
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());

        // Wait the remaining 0.7 periods — first post-reset tick must land.
        sc_core::wait(sc_time(period_ns * 0.7, SC_NS));
        EXPECT_TRUE(dut.dbg_mtime() >= 1);
        std::cout << "  [PASS] reset cancels pending tick; counting restarts cleanly\n";

        // ------------------------------------------------------------------
        // TA-3. tick_method drives MTIP (auto-tick → comparator → output).
        //       Set MTIMECMP[0] = N, wait N+1 tick periods, verify MTIP.
        //       This also exercises tick_method's schedule_recompute call.
        // ------------------------------------------------------------------
        pulse_reset();
        const uint64_t cmp = 5;
        drv.write64(mtimecmp_addr(0), cmp);
        // Wait cmp + 1.5 periods to ensure MTIME has crossed the comparator.
        sc_core::wait(sc_time(period_ns * (static_cast<double>(cmp) + 1.5), SC_NS));
        settle(); // let output_method propagate to sc_signal
        EXPECT_TRUE(dut.dbg_mtime() >= cmp);
        EXPECT_TRUE(dut.dbg_mtip(0));
        EXPECT_TRUE(mtip_sig[0].read());
        std::cout << "  [PASS] MTIP asserted automatically once MTIME >= MTIMECMP\n";

        // TA-4. Hold reset for 3.5 periods. MTIME stays 0; after release
        // the first tick lands one period later (exact, not a lower bound).
        rst_n.write(false);
        sc_core::wait(sc_time(period_ns * 3.5, SC_NS));
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());
        EXPECT_TRUE(!mtip_sig[0].read());
        rst_n.write(true);
        sc_core::wait(sc_time(period_ns * 0.4, SC_NS));
        EXPECT_EQ(uint64_t(0), dut.dbg_mtime());
        sc_core::wait(sc_time(period_ns * 0.7, SC_NS));
        EXPECT_EQ(uint64_t(1), dut.dbg_mtime());
        sc_core::wait(sc_time(period_ns, SC_NS));
        EXPECT_EQ(uint64_t(2), dut.dbg_mtime());
        std::cout << "  [PASS] held-reset restarts tick with exact MTIME\n";

        if (g_failures == 0) {
            std::cout << "\nALL TESTS PASSED\n";
        } else {
            std::cout << "\n" << g_failures << " FAILURE(S)\n";
        }
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    cci::cci_originator cfg("platform_cfg");
    auto broker = cci::cci_get_global_broker(cfg);

    // 1 hart; 50 ns tick period so the test stays fast but exercises the
    // real timer logic.  Access delay is left at the default 2 ns.
    broker.set_preset_cci_value("tb.clint.num_harts",      cci::cci_value(1u));
    broker.set_preset_cci_value("tb.clint.tick_period_ns", cci::cci_value(50.0));

    // TB sizing mirrors
    broker.set_preset_cci_value("tb.num_harts",      cci::cci_value(1u));
    broker.set_preset_cci_value("tb.tick_period_ns", cci::cci_value(50.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
