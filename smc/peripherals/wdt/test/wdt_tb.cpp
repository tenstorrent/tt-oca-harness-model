// SPDX-License-Identifier: Apache-2.0
//
// wdt_tb.cpp -- self-checking test bench for the SMC SiFive TLWDT (stage 1).
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iostream>
#include <string>

#include "wdt.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

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

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected TRUE: " #cond "\n";                       \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEF;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

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

    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t len, void* data) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

struct tb : sc_core::sc_module {
    smc::wdt dut;
    driver   drv;

    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<bool> core_rst{"core_rst"};
    sc_core::sc_signal<bool> irq{"irq"};
    sc_core::sc_signal<bool> sticky{"sticky"};

    SC_HAS_PROCESS(tb);
    tb(sc_module_name n)
        : sc_module(n)
        , dut("wdt", [] {
              smc::wdt_cfg c;
              c.tick_period_ns = 0.0;  // tests advance via dbg_tick
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
        EXPECT_EQ(1u, drv.read32(smc::wdt_cfg::OFF_KEY));
    }

    void feed() {
        unlock();
        drv.write32(smc::wdt_cfg::OFF_FEED, smc::wdt_cfg::FEED_MAGIC);
    }

    void wait_delta() {
        // Two deltas: schedule_recompute → output_method → signal update
        // (same pattern as clint_tb).
        for (int i = 0; i < 2; ++i) sc_core::wait(SC_ZERO_TIME);
    }

    void run() {
        rst_n.write(false);
        core_rst.write(false);
        wait_delta();
        rst_n.write(true);
        wait_delta();

        // 1. Reset defaults
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_CTRL));
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_COUNT));
        EXPECT_EQ(smc::wdt_cfg::CMP_RESET, drv.read32(smc::wdt_cfg::OFF_CMP));
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        EXPECT_FALSE(irq.read());
        EXPECT_FALSE(sticky.read());
        std::cout << "  [PASS] reset defaults\n";

        // 2. KEY unlock / lock
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        drv.write32(smc::wdt_cfg::OFF_KEY, 0xBAD);
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        unlock();
        // Any other write re-locks
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x10);
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        EXPECT_EQ(0x10u, drv.read32(smc::wdt_cfg::OFF_CMP));
        std::cout << "  [PASS] KEY unlock/lock\n";

        // 3. Locked writes ignored
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x20);  // locked
        EXPECT_EQ(0x10u, drv.read32(smc::wdt_cfg::OFF_CMP));
        std::cout << "  [PASS] locked writes ignored\n";

        // 4. Enable always + small CMP, tick to elapsed → IRQ
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x8);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT);  // always on
        dut.dbg_set_count(0);
        wait_delta();
        dut.dbg_tick(8);
        wait_delta();
        EXPECT_TRUE(dut.dbg_elapsed());
        EXPECT_TRUE(dut.dbg_ip());
        EXPECT_TRUE(irq.read());
        EXPECT_FALSE(sticky.read());  // rsten off
        std::cout << "  [PASS] compare/IRQ (always)\n";

        // 5. Clear IP via unlocked CTRL write while not elapsed
        feed();  // clears count
        wait_delta();
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_ALWAYS_BIT);  // ip=0
        wait_delta();
        EXPECT_FALSE(dut.dbg_ip());
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] IP clear\n";

        // 6. wdogrsten → sticky
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x4);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_RSTEN_BIT);
        dut.dbg_set_count(0);
        wait_delta();
        dut.dbg_tick(4);
        wait_delta();
        EXPECT_TRUE(sticky.read());
        EXPECT_TRUE(irq.read());
        std::cout << "  [PASS] sticky rst on rsten\n";

        // 7. Feed clears sticky
        feed();
        wait_delta();
        EXPECT_FALSE(sticky.read());
        EXPECT_EQ(0u, dut.dbg_count());
        std::cout << "  [PASS] feed clears sticky/count\n";

        // 8. Scale: count=16, scale=2 → scaled=4; CMP=4 → elapsed
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x4);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | 0x2u);  // scale=2
        unlock();
        drv.write32(smc::wdt_cfg::OFF_COUNT, 16);
        wait_delta();
        EXPECT_EQ(4u, dut.dbg_scaled());
        EXPECT_TRUE(dut.dbg_elapsed());
        EXPECT_TRUE(irq.read());
        std::cout << "  [PASS] scale\n";

        // 9. zerocmp clears count on elapsed
        feed();
        wait_delta();
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x2);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_ZEROCMP_BIT);
        dut.dbg_set_count(0);
        wait_delta();
        dut.dbg_tick(2);
        wait_delta();
        EXPECT_EQ(0u, dut.dbg_count());
        EXPECT_TRUE(irq.read());
        std::cout << "  [PASS] zerocmp\n";

        // 10. awake vs always: awake only counts when core not in reset
        feed();
        wait_delta();
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x1000);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_AWAKE_BIT);
        dut.dbg_set_count(0);
        core_rst.write(true);
        wait_delta();
        dut.dbg_tick(10);
        EXPECT_EQ(0u, dut.dbg_count());
        core_rst.write(false);
        wait_delta();
        dut.dbg_tick(5);
        EXPECT_EQ(5u, dut.dbg_count());
        // always overrides awake/core_rst
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_AWAKE_BIT);
        core_rst.write(true);
        wait_delta();
        dut.dbg_tick(3);
        EXPECT_EQ(8u, dut.dbg_count());
        core_rst.write(false);
        std::cout << "  [PASS] awake vs always\n";

        // 11. SCALED_COUNT write locks
        unlock();
        EXPECT_EQ(1u, drv.read32(smc::wdt_cfg::OFF_KEY));
        drv.write32(smc::wdt_cfg::OFF_SCALED_COUNT, 0xFFFF);
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        std::cout << "  [PASS] SCALED_COUNT write locks\n";

        // 12. Out-of-window / bad size
        uint32_t scratch = 0;
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x400, 4, &scratch));
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 2, &scratch));
        std::cout << "  [PASS] negative TLM paths\n";

        // 13. Module reset clears sticky/IP
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x1);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_RSTEN_BIT);
        dut.dbg_set_count(1);
        wait_delta();
        EXPECT_TRUE(sticky.read());
        rst_n.write(false);
        wait_delta();
        rst_n.write(true);
        wait_delta();
        EXPECT_FALSE(sticky.read());
        EXPECT_FALSE(irq.read());
        EXPECT_EQ(0u, dut.dbg_count());
        EXPECT_EQ(smc::wdt_cfg::CMP_RESET, drv.read32(smc::wdt_cfg::OFF_CMP));
        std::cout << "  [PASS] module reset\n";

        dut.dump_state(std::cout);

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

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
