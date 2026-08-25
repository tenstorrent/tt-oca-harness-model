// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// pvt_wrap_tb.cpp -- self-checking test bench for the SMC PVT Wrapper.
//
// Coverage:
//   - reset defaults
//   - RW register read/write (PROCESS_CTRL, REF_CLK_COUNT_PERIOD, VOLTAGE_CTRL, TEMP_CTRL)
//   - RO register read (PROCESS_STATUS, PROCESS_CLOCK_COUNTER, VOLTAGE_STATUS,
//                      TEMP_STATUS, TEMP_INTERRUPT)
//   - process clock counter enable / valid flow
//   - voltage status reflects voltage_reset_n
//   - temperature status / interrupt reflects temp_en and process valid
//   - decode miss for out-of-window offsets
//   - reset clears all state
//   - transport_dbg back-door access
//   - CCI introspection of parameters
//
// Convention: prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>

#include "pvt_wrap.h"

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
                      << "  expected=" << +_e << " actual=" << +_a             \
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

#define EXPECT_FALSE(cond)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected FALSE: " #cond "\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Tiny TLM driver -- 32-bit AXI-Lite-style register access.
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
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
            std::cerr << "FAIL read(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write(uint64_t addr, uint32_t value) {
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
            std::cerr << "FAIL write(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }

    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      uint32_t len, void* data, uint32_t sw = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(sw == 0 ? len : sw);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }
};

using smc::pvt_wrap_cfg;

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::pvt_wrap dut;
    driver          drv;

    sc_core::sc_signal<bool> rst_n{"rst_n"};

    // Output sinks.
    sc_core::sc_signal<bool>     process_clk_obs{"process_clk_obs"};
    sc_core::sc_signal<bool>     process_clk_obs_en{"process_clk_obs_en"};
    sc_core::sc_signal<uint32_t> voltage_code{"voltage_code"};
    sc_core::sc_signal<bool>     temp_interrupt{"temp_interrupt"};

    explicit tb(sc_module_name n)
        : sc_module(n), dut("pvt_wrap"), drv("drv")
    {
        drv.sock.bind(dut.reg_socket);

        dut.rst_n_i(rst_n);
        dut.process_clk_obs_o(process_clk_obs);
        dut.process_clk_obs_en_o(process_clk_obs_en);
        dut.voltage_code_o(voltage_code);
        dut.temp_interrupt_o(temp_interrupt);

        SC_THREAD(run);
    }

    void release_reset() {
        rst_n.write(true);
        settle();
    }

    void assert_reset() {
        rst_n.write(false);
        settle();
    }

    void settle() { sc_core::wait(2, SC_NS); }

    void run() {
        std::cout << "==== SMC PVT Wrapper TB ====\n";

        assert_reset();
        release_reset();

        // ------------------------------------------------------------------
        // 1. Reset defaults.
        // ------------------------------------------------------------------
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_STATUS_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_LO_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::VOLTAGE_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::VOLTAGE_STATUS_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_STATUS_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_INTERRUPT_OFF));
        std::cout << "  [PASS] reset defaults\n";

        // ------------------------------------------------------------------
        // 2. RW register read/write.
        // ------------------------------------------------------------------
        drv.write(pvt_wrap_cfg::PROCESS_CTRL_OFF, 0x0000'0111u);
        EXPECT_EQ(0x0000'0111u, drv.read(pvt_wrap_cfg::PROCESS_CTRL_OFF));

        drv.write(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF, 0x1234'5678u);
        EXPECT_EQ(0x1234'5678u, drv.read(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF));

        drv.write(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF, 0x9ABC'DEF0u);
        EXPECT_EQ(0x9ABC'DEF0u, drv.read(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF));

        drv.write(pvt_wrap_cfg::VOLTAGE_CTRL_OFF, 0x0000'0001u);
        EXPECT_EQ(0x0000'0001u, drv.read(pvt_wrap_cfg::VOLTAGE_CTRL_OFF));

        drv.write(pvt_wrap_cfg::TEMP_CTRL_OFF, 0x0000'0001u);
        EXPECT_EQ(0x0000'0001u, drv.read(pvt_wrap_cfg::TEMP_CTRL_OFF));
        std::cout << "  [PASS] RW register access\n";

        // ------------------------------------------------------------------
        // 3. RO registers reflect status; voltage and temperature ready.
        // ------------------------------------------------------------------
        EXPECT_EQ(0x0004'0001u, drv.read(pvt_wrap_cfg::VOLTAGE_STATUS_OFF)); // voltage_ready + code=4
        EXPECT_EQ(0xABCD'0001u, drv.read(pvt_wrap_cfg::TEMP_STATUS_OFF)); // temp_ready + output=0xABCD
        std::cout << "  [PASS] voltage / temperature status\n";

        // ------------------------------------------------------------------
        // 4. Process clock counter: enable counting, program a short period,
        //    wait for valid, then read counter.
        // ------------------------------------------------------------------
        drv.write(pvt_wrap_cfg::PROCESS_CTRL_OFF, 0x0000'0100u); // count_en only
        drv.write(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF, 5u);
        drv.write(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF, 0u);

        // Wait for the counter to reach the period and set valid.
        sc_core::wait(200, SC_NS);

        EXPECT_EQ(1u, drv.read(pvt_wrap_cfg::PROCESS_STATUS_OFF));

        uint32_t counter_lo = drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_LO_OFF);
        uint32_t counter_hi = drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF);
        EXPECT_EQ(0u, counter_hi);
        EXPECT_TRUE(counter_lo >= 5u);
        std::cout << "  [PASS] process clock counter valid (counter=" << counter_lo << ")\n";

        // ------------------------------------------------------------------
        // 5. Temperature interrupt is asserted once process is valid and temp_en.
        // ------------------------------------------------------------------
        EXPECT_TRUE(temp_interrupt.read());
        EXPECT_EQ(1u, drv.read(pvt_wrap_cfg::TEMP_INTERRUPT_OFF));
        std::cout << "  [PASS] temperature interrupt\n";

        // ------------------------------------------------------------------
        // 6. Process clock observation outputs.
        // ------------------------------------------------------------------
        drv.write(pvt_wrap_cfg::PROCESS_CTRL_OFF, 0x0000'0111u); // enable + obs_en + count_en
        sc_core::wait(2, SC_NS);
        EXPECT_TRUE(process_clk_obs_en.read());
        EXPECT_TRUE(process_clk_obs.read());
        std::cout << "  [PASS] process clock observation outputs\n";

        // ------------------------------------------------------------------
        // 7. Voltage droop code output.
        // ------------------------------------------------------------------
        EXPECT_EQ(0x4u, voltage_code.read());
        std::cout << "  [PASS] voltage droop code output\n";

        // ------------------------------------------------------------------
        // 8. Disabling count_en clears valid and de-asserts temp interrupt.
        // ------------------------------------------------------------------
        drv.write(pvt_wrap_cfg::PROCESS_CTRL_OFF, 0x0000'0001u); // enable only
        sc_core::wait(2, SC_NS);
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_STATUS_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_INTERRUPT_OFF));
        EXPECT_FALSE(temp_interrupt.read());
        std::cout << "  [PASS] count_en disable clears valid\n";

        // ------------------------------------------------------------------
        // 9. Decode miss for out-of-window and in-window unmapped offsets.
        // ------------------------------------------------------------------
        {
            uint32_t dummy = 0;
            tlm::tlm_response_status rsp =
                drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x1004, 4, &dummy);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, rsp);
        }
        {
            uint32_t dummy = 0;
            tlm::tlm_response_status rsp =
                drv.raw_xfer(tlm::TLM_WRITE_COMMAND, 0x1004, 4, &dummy);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, rsp);
        }
        {
            uint32_t dummy = 0;
            tlm::tlm_response_status rsp =
                drv.raw_xfer(tlm::TLM_WRITE_COMMAND, 0xFF0, 4, &dummy);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, rsp);
        }
        std::cout << "  [PASS] decode miss (out-of-window and unmapped)\n";

        // ------------------------------------------------------------------
        // 10. transport_dbg back-door access (read, write, and invalid).
        // ------------------------------------------------------------------
        {
            uint32_t write_val = 0xCAFEBABEu;
            tlm::tlm_generic_payload gp;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&write_val));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_byte_enable_ptr(nullptr);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            unsigned int dbg_len = drv.sock->transport_dbg(gp);
            EXPECT_EQ(4u, dbg_len);
            EXPECT_EQ(write_val, drv.read(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF));

            uint32_t read_data = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&read_data));
            gp.set_data_length(4);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            dbg_len = drv.sock->transport_dbg(gp);
            EXPECT_EQ(4u, dbg_len);
            EXPECT_EQ(write_val, read_data);

            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(0xFF0);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&read_data));
            gp.set_data_length(4);
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            dbg_len = drv.sock->transport_dbg(gp);
            EXPECT_EQ(0u, dbg_len);
        }
        std::cout << "  [PASS] transport_dbg back-door\n";

        // ------------------------------------------------------------------
        // 11. Read-only registers ignore writes.
        // ------------------------------------------------------------------
        {
            drv.write(pvt_wrap_cfg::PROCESS_STATUS_OFF, 0xFFFFFFFFu);
            EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_STATUS_OFF));

            drv.write(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF, 0xDEADBEEFu);
            EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF));

            drv.write(pvt_wrap_cfg::VOLTAGE_STATUS_OFF, 0xFFFFFFFFu);
            EXPECT_EQ(0x0004'0001u, drv.read(pvt_wrap_cfg::VOLTAGE_STATUS_OFF));
        }
        std::cout << "  [PASS] read-only registers ignore writes\n";

        // ------------------------------------------------------------------
        // 12. Malformed TLM transactions.
        // ------------------------------------------------------------------
        {
            uint32_t value = 0x55u;
            tlm::tlm_response_status rsp =
                drv.raw_xfer(tlm::TLM_WRITE_COMMAND, pvt_wrap_cfg::PROCESS_CTRL_OFF,
                             2, &value);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, rsp);
        }
        std::cout << "  [PASS] malformed write length rejected\n";

        // ------------------------------------------------------------------
        // 13. Unknown TLM command is rejected.
        // ------------------------------------------------------------------
        {
            uint32_t value = 0;
            tlm::tlm_response_status rsp =
                drv.raw_xfer(static_cast<tlm::tlm_command>(2),
                             pvt_wrap_cfg::PROCESS_CTRL_OFF, 4, &value);
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, rsp);
        }
        std::cout << "  [PASS] unknown TLM command rejected\n";

        // ------------------------------------------------------------------
        // 14. Reset during counting cancels the tick and clears state.
        // ------------------------------------------------------------------
        {
            drv.write(pvt_wrap_cfg::PROCESS_CTRL_OFF, 0x0000'0100u);
            drv.write(pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF, 1000u);
            sc_core::wait(10, SC_NS);
            assert_reset();
            release_reset();
            EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_STATUS_OFF));
            EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_LO_OFF));
        }
        std::cout << "  [PASS] reset during counting\n";

        // ------------------------------------------------------------------
        // 15. Reset clears all state.
        // ------------------------------------------------------------------
        assert_reset();
        release_reset();
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::PROCESS_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::VOLTAGE_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_CTRL_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::VOLTAGE_STATUS_OFF));
        EXPECT_EQ(0u, drv.read(pvt_wrap_cfg::TEMP_STATUS_OFF));
        EXPECT_FALSE(temp_interrupt.read());
        std::cout << "  [PASS] reset clears state\n";

        // ------------------------------------------------------------------
        // 16. CCI introspection.
        // ------------------------------------------------------------------
        {
            auto broker = cci::cci_get_broker();
            auto h_delay = broker.get_param_handle("tb.pvt_wrap.access_delay_ns");
            auto h_tick  = broker.get_param_handle("tb.pvt_wrap.tick_period_ns");
            EXPECT_TRUE(h_delay.is_valid());
            EXPECT_TRUE(h_tick.is_valid());
            if (h_delay.is_valid()) {
                EXPECT_EQ(2.0, h_delay.get_cci_value().get_double());
            }
            if (h_tick.is_valid()) {
                EXPECT_EQ(5.0, h_tick.get_cci_value().get_double());
            }
        }
        std::cout << "  [PASS] CCI introspection\n";

        if (g_failures == 0) {
            std::cout << "\nALL TESTS PASSED\n";
        } else {
            std::cout << "\nFAILURES: " << g_failures << "\n";
        }
        sc_core::sc_stop();
    }
};

} // anonymous namespace

int sc_main(int, char**)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    cci::cci_originator originator("pvt_wrap_tb");
    auto broker = cci::cci_get_global_broker(originator);
    broker.set_preset_cci_value("tb.pvt_wrap.access_delay_ns", cci::cci_value(2.0));
    broker.set_preset_cci_value("tb.pvt_wrap.tick_period_ns",  cci::cci_value(5.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
