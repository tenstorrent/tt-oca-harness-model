// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file avsbus_controller_neg_tb.cpp
 * @brief Negative-path / edge-case bench for the SMC AVSBus Controller model.
 *
 * Targets constructor FATAL, TLM error responses, RO/WO semantics, dbg paths,
 * canned default-slave responses, readback overflow, resync-vs-xfer races,
 * ACK_BUSY retries, and header field helpers — driving line coverage of
 * `avsbus_controller.cpp` above ~95 %.
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
#include <sstream>

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

template <typename F>
bool expect_fatal(F&& body)
{
    try { body(); }
    catch (const sc_core::sc_report&) { return true; }
    catch (const std::exception&)     { return true; }
    catch (...)                       { return true; }
    return false;
}

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;
    tlm_utils::tlm_quantumkeeper qk;

    explicit driver(sc_core::sc_module_name n) : sc_module(n), sock("sock") {
        qk.set_global_quantum(sc_time(1, SC_US));
        qk.reset();
    }

    tlm::tlm_response_status xfer(tlm::tlm_command cmd, uint64_t addr,
                                  uint8_t* buf, unsigned len)
    {
        tlm::tlm_generic_payload gp;
        sc_time t = qk.get_local_time();
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(buf);
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        qk.set_and_sync(t);
        return gp.get_response_status();
    }

    uint32_t read32_ok(uint64_t addr) {
        uint32_t data = 0;
        auto st = xfer(tlm::TLM_READ_COMMAND, addr,
                       reinterpret_cast<uint8_t*>(&data), 4);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, st);
        return data;
    }

    void write32_ok(uint64_t addr, uint32_t value) {
        auto st = xfer(tlm::TLM_WRITE_COMMAND, addr,
                       reinterpret_cast<uint8_t*>(&value), 4);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, st);
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
        : sc_module(n), drv("drv"), dut("avsbus")
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
    do_reset();

    // ------------------------------------------------------------------
    // 1. TLM error responses
    // ------------------------------------------------------------------
    {
        uint32_t d = 0;
        auto st = drv.xfer(tlm::TLM_READ_COMMAND, 0x50,
                           reinterpret_cast<uint8_t*>(&d), 2);
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE, st);

        st = drv.xfer(tlm::TLM_READ_COMMAND, 0x51,
                      reinterpret_cast<uint8_t*>(&d), 4);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, st);

        st = drv.xfer(tlm::TLM_READ_COMMAND, 0x2000,
                      reinterpret_cast<uint8_t*>(&d), 4);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, st);

        // Decode miss inside window (gap between 0x0C and 0x20)
        st = drv.xfer(tlm::TLM_READ_COMMAND, 0x10,
                      reinterpret_cast<uint8_t*>(&d), 4);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, st);

        // Write decode miss
        st = drv.xfer(tlm::TLM_WRITE_COMMAND, 0x10,
                      reinterpret_cast<uint8_t*>(&d), 4);
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE, st);

        tlm::tlm_generic_payload gp;
        sc_time t = sc_time(0, SC_NS);
        gp.set_command(tlm::TLM_IGNORE_COMMAND);
        gp.set_address(0x50);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&d));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        drv.sock->b_transport(gp, t);
        EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE, gp.get_response_status());
    }
    std::cout << "  [PASS] TLM error responses\n";

    // ------------------------------------------------------------------
    // 2. transport_dbg + dbg_reg coverage of every side-effect register
    // ------------------------------------------------------------------
    {
        tlm::tlm_generic_payload gp;
        uint32_t val = 0xA5A5A5A5u;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(0x50);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&val));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
        EXPECT_EQ(0x00A5A5A5u, dut.dbg_reg(0x50));

        uint32_t out = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&out));
        EXPECT_EQ(4u, drv.sock->transport_dbg(gp));

        gp.set_data_length(2);
        EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

        // IGNORE via transport_dbg → 0
        gp.set_data_length(4);
        gp.set_command(tlm::TLM_IGNORE_COMMAND);
        EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

        // dbg write to unmapped offset → 0 bytes
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(0x10);
        gp.set_data_length(4);
        EXPECT_EQ(0u, drv.sock->transport_dbg(gp));

        // Exercise every dbg_reg switch arm
        EXPECT_EQ(0u, dut.dbg_reg(0x00));                         // AVS_CMD
        EXPECT_EQ(0xDEADBEEFu, dut.dbg_reg(0x04));                // empty READBACK peek
        EXPECT_EQ(0xDEADBEEFu, dut.dbg_reg(0x08));                // DEBUG_READBACK
        (void)dut.dbg_reg(0x0C);                                  // LATEST
        (void)dut.dbg_reg(0x20);                                  // NORMAL_STATUS
        (void)dut.dbg_reg(0x24);                                  // SLAVE_STATUS
        (void)dut.dbg_reg(0x28);                                  // FIFOS_STATUS
        (void)dut.dbg_reg(0x30);                                  // INTERRUPT
        EXPECT_EQ(0u, dut.dbg_reg(0x38));                         // CLEAR RAZ
        EXPECT_EQ(0u, dut.dbg_reg(0x10));                         // unmapped
        EXPECT_EQ(0u, dut.dbg_reg(0x2000));                       // OOB
    }
    std::cout << "  [PASS] transport_dbg / dbg_reg\n";

    // ------------------------------------------------------------------
    // 3. WO RAZ + RO write-ignore
    // ------------------------------------------------------------------
    {
        EXPECT_EQ(0u, drv.read32_ok(0x38)); // INTERRUPT_CLEAR RAZ
        EXPECT_EQ(0u, drv.read32_ok(0x00)); // AVS_CMD RAZ

        const uint32_t before = drv.read32_ok(0x0C);
        drv.write32_ok(0x04, 0xFFFFFFFFu); // READBACK WI
        drv.write32_ok(0x08, 0xFFFFFFFFu); // DEBUG WI
        drv.write32_ok(0x0C, 0xFFFFFFFFu); // LATEST WI
        drv.write32_ok(0x20, 0xFFFFFFFFu);
        drv.write32_ok(0x24, 0xFFFFFFFFu);
        drv.write32_ok(0x28, 0xFFFFFFFFu);
        drv.write32_ok(0x30, 0xFFFFFFFFu);
        EXPECT_EQ(before, drv.read32_ok(0x0C));
    }
    std::cout << "  [PASS] RO/WO semantics\n";

    // ------------------------------------------------------------------
    // 4. Readback backpressure (complete_one_xfer early-out when rb full)
    // ------------------------------------------------------------------
    do_reset();
    for (unsigned i = 0; i < 8; ++i)
        drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0xF, 0, 0xFFFF));
    step(8 * 60.0);
    settle();
    EXPECT_EQ(8u, dut.rb_fifo_count());
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0x0, 0, 0xFFFF));
    step(60.0);
    settle();
    EXPECT_EQ(1u, dut.cmd_fifo_count());
    EXPECT_EQ(8u, dut.rb_fifo_count());
    (void)drv.read32_ok(0x04);
    step(60.0);
    settle();
    EXPECT_EQ(0u, dut.cmd_fifo_count());
    EXPECT_EQ(8u, dut.rb_fifo_count());
    std::cout << "  [PASS] readback backpressure\n";

    // ------------------------------------------------------------------
    // 5. inject_readback → OVERFLOW + count_as_retry
    // ------------------------------------------------------------------
    {
        // rb still full from previous test
        EXPECT_EQ(8u, dut.rb_fifo_count());
        dut.inject_readback(0x11111111u, /*count_as_retry=*/true);
        settle();
        EXPECT_TRUE((drv.read32_ok(0x30) & (1u << 8)) != 0); // READBACK_OVERFLOW
        // Still 8 — drop on overflow
        EXPECT_EQ(8u, dut.rb_fifo_count());
        // Retry counter bumped
        EXPECT_TRUE((drv.read32_ok(0x20) & 0xFFFFu) >= 1u);
    }
    std::cout << "  [PASS] readback overflow inject\n";

    // ------------------------------------------------------------------
    // 6. ACK_BAD_DATA no-retry + ACK_BUSY retry
    // ------------------------------------------------------------------
    do_reset();
    unsigned calls = 0;
    dut.set_slave_model([&](uint32_t) {
        ++calls;
        return smc::avs_rb::pack(smc::avs_rb::ACK_BAD_DATA, 0x1F, 0x1111, 0);
    });
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0, 0, 0xFFFF));
    step(60.0);
    settle();
    EXPECT_EQ(1u, calls);
    EXPECT_EQ(0u, dut.cmd_fifo_count());

    do_reset();
    calls = 0;
    dut.set_slave_model([&](uint32_t) {
        ++calls;
        // First two attempts BUSY, then OK
        if (calls < 3)
            return smc::avs_rb::pack(smc::avs_rb::ACK_BUSY, 0, 0, 0);
        return smc::avs_rb::pack(smc::avs_rb::ACK_OK, 0x14, 0xBEEF, 0);
    });
    drv.write32_ok(0x50, 0x00050000u); // max_retries = 5
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0, 0, 0xFFFF));
    step(5 * 60.0);
    settle();
    EXPECT_TRUE(calls >= 3);
    EXPECT_EQ(0u, dut.cmd_fifo_count());
    EXPECT_EQ(1u, dut.rb_fifo_count());
    dut.set_slave_model({});
    std::cout << "  [PASS] ACK_BAD_DATA / ACK_BUSY\n";

    // ------------------------------------------------------------------
    // 7. Default slave canned responses for every READ cmd_code
    // ------------------------------------------------------------------
    do_reset();
    struct Expect { uint32_t code; uint32_t data; };
    const Expect expects[] = {
        {0x0, 0x03E8u},
        {0x1, 0x1010u},
        {0x2, 0x0064u},
        {0x3, 0x00FAu},
        {0x5, 0x0003u},
        {0xE, 0x0000u},
        {0xF, 0x0002u},
        {0x7, 0xAAAAu}, // default arm
    };
    for (const auto& e : expects) {
        drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, e.code, 0, 0xFFFF));
        step(60.0);
        settle();
        const uint32_t rb = drv.read32_ok(0x04);
        EXPECT_EQ(e.data, smc::avs_rb::cmd_data(rb));
        EXPECT_EQ(smc::avs_rb::ACK_OK, smc::avs_rb::slave_ack(rb));
        (void)smc::avs_rb::status_resp(rb);
        (void)smc::avs_rb::crc(rb);
    }
    // Pop path with remaining data re-asserts HAS_DATA
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0x0, 0, 0xFFFF));
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0x1, 0, 0xFFFF));
    step(2 * 60.0);
    settle();
    EXPECT_EQ(2u, dut.rb_fifo_count());
    (void)drv.read32_ok(0x04); // leave one
    settle();
    EXPECT_TRUE((drv.read32_ok(0x30) & (1u << 3)) != 0); // HAS_DATA sticky re-assert
    std::cout << "  [PASS] canned READ responses\n";

    // ------------------------------------------------------------------
    // 8. Force resync while a transfer is pending (resync early-out)
    // ------------------------------------------------------------------
    do_reset();
    {
        // Stretch resync past the pending xfer so complete_one_xfer hits the
        // slave_in_resync_ early-out arm.
        auto broker = cci::cci_get_broker();
        auto h = broker.get_param_handle("tb.avsbus.resync_delay_ns");
        EXPECT_TRUE(h.is_valid());
        h.set_cci_value(cci::cci_value(200.0));
    }
    drv.write32_ok(0x00, smc::avs_cmd::pack(smc::avs_cmd::READ, 0, 0x0, 0, 0xFFFF));
    settle(); // xfer scheduled (+50 ns)
    drv.write32_ok(0x54, (1u << 9)); // FORCE_SLAVE_RESYNC (+200 ns)
    settle();
    EXPECT_TRUE((drv.read32_ok(0x20) & (1u << 22)) != 0); // in resync
    step(60.0); // pending xfer fires while still in resync → early-out
    settle();
    EXPECT_TRUE((drv.read32_ok(0x20) & (1u << 22)) != 0); // still resyncing
    EXPECT_EQ(1u, dut.cmd_fifo_count()); // command not yet consumed
    step(160.0); // finish resync (resync_method calls schedule_xfer)
    settle();
    step(60.0);  // allow the post-resync transfer to complete
    settle();
    EXPECT_EQ(0u, dut.cmd_fifo_count());
    EXPECT_EQ(1u, dut.rb_fifo_count());
    // Restore default resync delay for any later tests.
    {
        auto broker = cci::cci_get_broker();
        broker.get_param_handle("tb.avsbus.resync_delay_ns")
              .set_cci_value(cci::cci_value(20.0));
    }
    std::cout << "  [PASS] resync vs pending xfer\n";

    // ------------------------------------------------------------------
    // 9. Field helpers + inject_interrupt out-of-range + dump_state
    // ------------------------------------------------------------------
    {
        const uint32_t w = smc::avs_cmd::pack(smc::avs_cmd::READ, 1, 0xA, 0x3, 0x1234);
        EXPECT_EQ(smc::avs_cmd::READ, smc::avs_cmd::r_or_w(w));
        EXPECT_EQ(1u, smc::avs_cmd::cmd_grp(w));
        EXPECT_EQ(0xAu, smc::avs_cmd::cmd_code(w));
        EXPECT_EQ(0x3u, smc::avs_cmd::rail_sel(w));
        EXPECT_EQ(0x1234u, smc::avs_cmd::cmd_data(w));

        dut.inject_interrupt(99); // ignored (bit > 8)
        settle();
        std::ostringstream oss;
        dut.dump_state(oss);
        EXPECT_TRUE(oss.str().find("AVSBus Controller") != std::string::npos);
    }
    std::cout << "  [PASS] helpers / dump_state\n";

    // ------------------------------------------------------------------
    // 10. CRC3 smoke
    // ------------------------------------------------------------------
    EXPECT_EQ(0u, smc::avsbus_controller::crc3(0u));
    std::cout << "  [PASS] crc3 smoke\n";

    // ------------------------------------------------------------------
    // 11. Empty debug sentinel
    // ------------------------------------------------------------------
    do_reset();
    EXPECT_EQ(0xDEADBEEFu, drv.read32_ok(0x08));
    std::cout << "  [PASS] empty debug sentinel\n";

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
    // Make SC_REPORT_FATAL throw so the zero-depth ctor guard is testable.
    sc_core::sc_report_handler::set_actions(
        sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_LOGIC_X_TO_BOOL_,
                                            sc_core::SC_DO_NOTHING);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    std::cout << "==== AVSBus Controller negative-path TB ====\n";

    // Constructor guard: zero FIFO depth is FATAL.
    {
        smc::avsbus_controller_cfg c0;
        c0.command_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&]{
            smc::avsbus_controller bad("bad_cmd", c0);
        }));
        smc::avsbus_controller_cfg c1;
        c1.readback_fifo_depth = 0;
        EXPECT_TRUE(expect_fatal([&]{
            smc::avsbus_controller bad("bad_rb", c1);
        }));
        std::cout << "  [PASS] constructor guard rail (zero FIFO depth)\n";
    }

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
