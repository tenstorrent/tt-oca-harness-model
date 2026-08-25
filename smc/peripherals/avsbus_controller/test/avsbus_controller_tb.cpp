// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file avsbus_controller_tb.cpp
 * @brief Primary self-checking test bench for the SMC AVSBus Controller model.
 *
 * Covers: reset values, register RW/RO/W1C, command/readback FIFOs, default
 * slave response path, IRQ mask/clear aggregation, overflow/underflow,
 * retry-to-max, forced resync, debug peek vs pop, CCI introspection.
 */

#include "avsbus_controller.h"

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cstdint>
#include <cstring>
#include <iostream>

using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {

int g_failures = 0;

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        auto _e = (expected);                                                  \
        auto _a = (actual);                                                    \
        if (_e != _a) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  " #actual " expected=0x" << std::hex               \
                      << static_cast<uint64_t>(_e) << " got=0x"                \
                      << static_cast<uint64_t>(_a) << std::dec << "\n";        \
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

constexpr uint64_t AVS_CMD                   = 0x00;
constexpr uint64_t AVS_READBACK              = 0x04;
constexpr uint64_t AVS_DEBUG_READBACK        = 0x08;
constexpr uint64_t AVS_LATEST_SLAVE_SUBFRAME = 0x0C;
constexpr uint64_t AVS_NORMAL_STATUS         = 0x20;
constexpr uint64_t AVS_SLAVE_STATUS          = 0x24;
constexpr uint64_t AVS_FIFOS_STATUS          = 0x28;
constexpr uint64_t AVS_INTERRUPT             = 0x30;
constexpr uint64_t AVS_INTERRUPT_MASK        = 0x34;
constexpr uint64_t AVS_INTERRUPT_CLEAR       = 0x38;
constexpr uint64_t AVS_CFG_0                 = 0x50;
constexpr uint64_t AVS_CFG_1                 = 0x54;
constexpr uint64_t AVS_CONFIG                = 0x58;

constexpr uint32_t I_SLAVE          = 1u << 0;
constexpr uint32_t I_RB_FULL        = 1u << 2;
constexpr uint32_t I_RB_HAS_DATA    = 1u << 3;
constexpr uint32_t I_MAX_RETRIES    = 1u << 5;
constexpr uint32_t I_CMD_OVERFLOW   = 1u << 6;
constexpr uint32_t I_RB_UNDERFLOW   = 1u << 7;

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    tlm_utils::tlm_quantumkeeper qk;

    explicit driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    uint32_t read32(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0xDEADBEEFu;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL read32(0x" << std::hex << addr << ") rsp="
                      << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
        return data;
    }

    void write32(uint64_t addr, uint32_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = qk.get_local_time();
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "FAIL write32(0x" << std::hex << addr << ", 0x" << value
                      << ") rsp=" << gp.get_response_string() << std::dec << "\n";
            ++g_failures;
        }
    }
};

struct tb : sc_core::sc_module {
    driver drv;
    smc::avsbus_controller dut;

    sc_core::sc_signal<bool> rstn{"rstn"};
    sc_core::sc_signal<bool> irq{"irq"};
    sc_core::sc_signal<bool> gpio_en{"gpio_en"};

    SC_HAS_PROCESS(tb);

    explicit tb(sc_core::sc_module_name n)
        : sc_module(n)
        , drv("drv")
        , dut("avsbus")
    {
        drv.sock.bind(dut.reg_socket);
        dut.rst_n_i(rstn);
        dut.irq_o(irq);
        dut.avs_gpio_enable_o(gpio_en);
        SC_THREAD(run);
    }

    void settle() { sc_core::wait(sc_time(1, SC_NS)); }
    void step(double ns) { sc_core::wait(sc_time(ns, SC_NS)); }

    void do_reset() {
        // Ensure a falling edge even if the signal already reads low.
        rstn.write(true);
        settle();
        rstn.write(false);
        settle();
        rstn.write(true);
        settle();
    }

    void run();
};

void tb::run()
{
    // ------------------------------------------------------------------
    // 1. Reset values
    // ------------------------------------------------------------------
    do_reset();
    EXPECT_EQ(0x00051000u, drv.read32(AVS_CFG_0));
    EXPECT_EQ(0x00000003u, drv.read32(AVS_CFG_1));
    EXPECT_EQ(0x1u,        drv.read32(AVS_CONFIG));
    EXPECT_EQ(0x1FFu,      drv.read32(AVS_INTERRUPT_MASK));
    EXPECT_EQ(0u,          drv.read32(AVS_INTERRUPT));
    EXPECT_EQ(0x0000FFFFu, drv.read32(AVS_LATEST_SLAVE_SUBFRAME));
    EXPECT_EQ(0x08000800u, drv.read32(AVS_FIFOS_STATUS)); // 8 vacant each
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 21)) != 0); // idle
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 17)) != 0); // cmd empty
    EXPECT_TRUE(!irq.read());
    EXPECT_TRUE(gpio_en.read());
    std::cout << "  [PASS] reset values\n";

    // ------------------------------------------------------------------
    // 2. Register RW / RO / W1C
    // ------------------------------------------------------------------
    drv.write32(AVS_CFG_0, 0x000300FFu); // max_retries=3, resync=0xFF
    EXPECT_EQ(0x000300FFu, drv.read32(AVS_CFG_0));

    drv.write32(AVS_INTERRUPT_MASK, 0x0); // unmask all
    EXPECT_EQ(0x0u, drv.read32(AVS_INTERRUPT_MASK));

    drv.write32(AVS_CONFIG, 0x0);
    settle();
    EXPECT_EQ(0x0u, drv.read32(AVS_CONFIG));
    EXPECT_TRUE(!gpio_en.read());
    drv.write32(AVS_CONFIG, 0x1);
    settle();
    EXPECT_TRUE(gpio_en.read());

    // RO writes ignored
    drv.write32(AVS_NORMAL_STATUS, 0xFFFFFFFFu);
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 21)) != 0);

    // WO AVS_CMD reads as 0
    EXPECT_EQ(0u, drv.read32(AVS_CMD));
    std::cout << "  [PASS] register RW/RO\n";

    // ------------------------------------------------------------------
    // 3. Command → default response path
    // ------------------------------------------------------------------
    do_reset();
    drv.write32(AVS_INTERRUPT_MASK, 0); // unmask
    const uint32_t cmd_v = smc::avs_cmd::pack(
        smc::avs_cmd::COMMIT_WRITE, 0, /*voltage*/0x0, /*rail*/0x0, 0x03E8u);
    drv.write32(AVS_CMD, cmd_v);
    settle();
    EXPECT_EQ(1u, dut.cmd_fifo_count());
    step(60.0); // > xfer_delay_ns default 50
    settle();
    EXPECT_EQ(0u, dut.cmd_fifo_count());
    EXPECT_EQ(1u, dut.rb_fifo_count());
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_RB_HAS_DATA) != 0);
    EXPECT_TRUE(irq.read());

    const uint32_t peek = drv.read32(AVS_DEBUG_READBACK);
    EXPECT_EQ(1u, dut.rb_fifo_count()); // peek does not pop
    const uint32_t pop  = drv.read32(AVS_READBACK);
    EXPECT_EQ(peek, pop);
    EXPECT_EQ(0u, dut.rb_fifo_count());
    EXPECT_EQ(smc::avs_rb::ACK_OK, smc::avs_rb::slave_ack(pop));
    EXPECT_EQ(0xFFFFu, smc::avs_rb::cmd_data(pop)); // write → 0xFFFF
    EXPECT_EQ(pop, drv.read32(AVS_LATEST_SLAVE_SUBFRAME));
    std::cout << "  [PASS] command/response path\n";

    // ------------------------------------------------------------------
    // 4. IRQ clear / mask
    // ------------------------------------------------------------------
    drv.write32(AVS_INTERRUPT_CLEAR, I_RB_HAS_DATA);
    settle();
    // HAS_DATA sticky cleared; FIFO empty so no re-assert
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_RB_HAS_DATA) == 0);
    EXPECT_TRUE(!irq.read());

    dut.inject_interrupt(smc::avs_irq::SLAVE_ISSUED);
    settle();
    EXPECT_TRUE(irq.read());
    drv.write32(AVS_INTERRUPT_MASK, I_SLAVE); // disable slave IRQ
    settle();
    EXPECT_TRUE(!irq.read());
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_SLAVE) != 0); // status sticky
    drv.write32(AVS_INTERRUPT_CLEAR, I_SLAVE);
    settle();
    EXPECT_EQ(0u, drv.read32(AVS_INTERRUPT) & I_SLAVE);
    std::cout << "  [PASS] IRQ mask/clear\n";

    // ------------------------------------------------------------------
    // 5. Cmd FIFO fill + overflow
    // ------------------------------------------------------------------
    do_reset();
    // Push 8 cmds before the timed xfer fires (b_transport never waits),
    // then the 9th must overflow.
    for (unsigned i = 0; i < 8; ++i) {
        drv.write32(AVS_CMD, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0xF, 0, 0xFFFF));
    }
    settle();
    EXPECT_EQ(8u, dut.cmd_fifo_count());
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 18)) != 0); // cmd full
    drv.write32(AVS_CMD, 0x12345678u); // overflow
    settle();
    EXPECT_EQ(8u, dut.cmd_fifo_count());
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_CMD_OVERFLOW) != 0);
    std::cout << "  [PASS] cmd FIFO overflow\n";

    // Drain
    step(8 * 60.0);
    settle();
    EXPECT_TRUE(dut.cmd_fifo_count() == 0);
    EXPECT_TRUE(dut.rb_fifo_count() == 8);
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_RB_FULL) != 0);
    std::cout << "  [PASS] readback FIFO full after burst\n";

    // ------------------------------------------------------------------
    // 6. Readback underflow
    // ------------------------------------------------------------------
    do_reset();
    const uint32_t empty_rb = drv.read32(AVS_READBACK);
    EXPECT_EQ(0u, empty_rb);
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_RB_UNDERFLOW) != 0);
    std::cout << "  [PASS] readback underflow\n";

    // ------------------------------------------------------------------
    // 7. Retry until max_retries
    // ------------------------------------------------------------------
    do_reset();
    drv.write32(AVS_CFG_0, 0x00020000u); // max_retries = 2
    drv.write32(AVS_INTERRUPT_MASK, 0);
    unsigned calls = 0;
    dut.set_slave_model([&](uint32_t) {
        ++calls;
        // Always return BAD_CRC → retry
        return smc::avs_rb::pack(smc::avs_rb::ACK_BAD_CRC, 0, 0, 0);
    });
    drv.write32(AVS_CMD, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0x0, 0, 0xFFFF));
    step(5 * 60.0);
    settle();
    EXPECT_TRUE((drv.read32(AVS_INTERRUPT) & I_MAX_RETRIES) != 0);
    EXPECT_TRUE(calls >= 3); // initial + 2 retries, then exhaust (≥3 attempts)
    EXPECT_EQ(0u, dut.cmd_fifo_count());
    EXPECT_EQ(1u, dut.rb_fifo_count()); // failure response pushed
    dut.set_slave_model({}); // restore default
    std::cout << "  [PASS] max retries\n";

    // ------------------------------------------------------------------
    // 8. Forced resync
    // ------------------------------------------------------------------
    do_reset();
    drv.write32(AVS_CFG_1, (1u << 9)); // FORCE_SLAVE_RESYNC
    settle();
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 22)) != 0);
    step(25.0); // > resync_delay_ns
    settle();
    EXPECT_TRUE((drv.read32(AVS_NORMAL_STATUS) & (1u << 22)) == 0);
    std::cout << "  [PASS] forced resync\n";

    // ------------------------------------------------------------------
    // 9. Read command canned data + slave status
    // ------------------------------------------------------------------
    do_reset();
    drv.write32(AVS_CMD, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, /*voltage*/0x0, 0, 0xFFFF));
    step(60.0);
    settle();
    const uint32_t rb = drv.read32(AVS_READBACK);
    EXPECT_EQ(0x03E8u, smc::avs_rb::cmd_data(rb));
    const uint32_t ss = drv.read32(AVS_SLAVE_STATUS);
    EXPECT_EQ(smc::avs_rb::ACK_OK, (ss >> 16) & 0x3u);
    std::cout << "  [PASS] read voltage response\n";

    // ------------------------------------------------------------------
    // 10. CCI introspection + dump_state / dbg_reg
    // ------------------------------------------------------------------
    {
        auto broker = cci::cci_get_broker();
        auto p = broker.get_param_handle("tb.avsbus.command_fifo_depth");
        EXPECT_TRUE(p.is_valid());
        EXPECT_EQ(8u, p.get_cci_value().get_uint());
    }
    EXPECT_EQ(0x00051000u, dut.dbg_reg(AVS_CFG_0));
    dut.dump_state(std::cout);
    std::cout << "  [PASS] CCI / dbg\n";

    // ------------------------------------------------------------------
    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << g_failures << " FAILURE(S)\n";
    }
    sc_core::sc_stop();
}

} // namespace

int sc_main(int, char*[])
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);
    global_broker.set_preset_cci_value("tb.avsbus.xfer_delay_ns",
                                       cci::cci_value(50.0));
    global_broker.set_preset_cci_value("tb.avsbus.resync_delay_ns",
                                       cci::cci_value(20.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
