// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// wdt_tick_tb.cpp — focused test for the WDT auto-tick engine.
//
// Separate from wdt_tb.cpp (which forces tick_period_ns = 0).  Here
// tick_period_ns = 50 ns so we cover:
//   • constructor schedules first tick_event_
//   • reset_proc cancels and re-arms the event
//   • tick_method increments count and re-arms

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <iostream>

#include "wdt.h"

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
        }                                                                      \
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
        }                                                                      \
    } while (0)

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    void write32(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }
};

struct tb : sc_core::sc_module {
    smc::wdt dut;
    driver   drv;

    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> core_rst{"core_rst"};
    sc_core::sc_signal<bool> irq{"irq"};
    sc_core::sc_signal<bool> sticky{"sticky"};

    static constexpr double kTickNs = 50.0;

    SC_HAS_PROCESS(tb);
    tb(sc_module_name n)
        : sc_module(n)
        , dut("wdt", [] {
              smc::wdt_cfg c;
              c.tick_period_ns = kTickNs;
              return c;
          }())
        , drv("drv")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rst_n);
        dut.core_rst_i(core_rst);
        dut.irq_o(irq);
        dut.rst_sticky_o(sticky);
        SC_THREAD(run);
    }

    void unlock() {
        drv.write32(smc::wdt_cfg::OFF_KEY, smc::wdt_cfg::KEY_MAGIC);
    }

    void run() {
        std::cout << "==== WDT auto-tick TB ====\n";
        std::cout << "  tick_period_ns=" << kTickNs << "\n";

        rst_n.write(false);
        core_rst.write(false);
        wait(SC_ZERO_TIME);
        rst_n.write(true);
        wait(SC_ZERO_TIME);

        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x1000);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_ALWAYS_BIT);
        dut.dbg_set_count(0);
        wait(SC_ZERO_TIME);

        // Five auto-ticks should advance count by 5.
        wait(sc_time(kTickNs * 5, SC_NS));
        wait(SC_ZERO_TIME);
        EXPECT_EQ(5u, dut.dbg_count());
        std::cout << "  [PASS] auto-tick advances COUNT\n";

        // Module reset re-arms tick; counting resumes from 0 after release.
        rst_n.write(false);
        wait(SC_ZERO_TIME);
        EXPECT_EQ(0u, dut.dbg_count());
        rst_n.write(true);
        wait(SC_ZERO_TIME);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_ALWAYS_BIT);
        wait(sc_time(kTickNs * 3, SC_NS));
        wait(SC_ZERO_TIME);
        EXPECT_EQ(3u, dut.dbg_count());
        std::cout << "  [PASS] reset cancels/re-arms tick\n";

        // While held in reset, tick_method early-returns (no count advance).
        const uint32_t held = dut.dbg_count();
        rst_n.write(false);
        wait(SC_ZERO_TIME);
        wait(sc_time(kTickNs * 4, SC_NS));
        EXPECT_EQ(0u, dut.dbg_count());  // reset clears; stays cleared while low
        (void)held;
        rst_n.write(true);
        std::cout << "  [PASS] tick ignored while rst_n low\n";

        if (g_failures == 0) {
            std::cout << "ALL TESTS PASSED\n";
        } else {
            std::cout << g_failures << " FAILURE(S)\n";
        }
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("/Accellera/CCI/",
                                            sc_core::SC_DISPLAY);
    static cci_utils::consuming_broker broker("GlobalBroker");
    cci::cci_register_broker(broker);
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
