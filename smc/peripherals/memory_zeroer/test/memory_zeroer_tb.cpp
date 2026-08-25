// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// memory_zeroer_tb.cpp -- self-checking test bench for the SMC memory zeroer.
//
// CCI: sc_main registers a global broker before any module is constructed.
// Presets exercise chunk_size and access_delay_ns.
//
// Prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "memory_zeroer.h"

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

#define EXPECT_FALSE(cond)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected FALSE: " #cond "\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Tiny TLM memory target — serves as the zeroer's DMA destination.
// ---------------------------------------------------------------------------
struct simple_mem : sc_core::sc_module {
    tlm_utils::simple_target_socket<simple_mem> sock;
    std::vector<uint8_t>                        mem;

    explicit simple_mem(sc_module_name n, std::size_t bytes)
        : sc_module(n), sock("sock"), mem(bytes, 0xA5)
    {
        sock.register_b_transport(this, &simple_mem::b_transport);
    }

    void fill(uint8_t v) { std::fill(mem.begin(), mem.end(), v); }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        const uint64_t adr = gp.get_address();
        const unsigned len = gp.get_data_length();
        unsigned char* ptr = gp.get_data_ptr();
        if (ptr == nullptr || len == 0 || adr + len > mem.size()) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (gp.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(mem.data() + adr, ptr, len);
        } else if (gp.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(ptr, mem.data() + adr, len);
        } else {
            gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        delay += sc_core::sc_time(1, SC_NS);
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ---------------------------------------------------------------------------
// Register-bus driver
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint64_t read64(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return data;
    }

    void write64(uint64_t addr, uint64_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
    }

    tlm::tlm_response_status try_access(tlm::tlm_command cmd, uint64_t addr,
                                        unsigned len)
    {
        tlm::tlm_generic_payload gp;
        std::vector<uint8_t> buf(len ? len : 1, 0);
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(buf.data());
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }
};

// ---------------------------------------------------------------------------
// Top-level TB
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    // MANY_WRITERS: irq_o is driven both from reset_proc (SC_METHOD) and
    // from nested b_transport running in the TB thread process context.
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> irq{"irq"};

    smc::memory_zeroer zeroer;
    simple_mem         mem;
    driver             drv;

    SC_HAS_PROCESS(tb);

    explicit tb(sc_module_name n)
        : sc_module(n)
        , zeroer("zeroer")
        , mem("mem", 8192)
        , drv("drv")
    {
        zeroer.rst_n_i(rst_n);
        zeroer.irq_o(irq);
        drv.sock.bind(zeroer.reg_socket);
        zeroer.dma_socket.bind(mem.sock);
        SC_THREAD(run);
    }

    void settle()
    {
        for (int i = 0; i < 3; ++i) wait(SC_ZERO_TIME);
    }

    void pulse_reset()
    {
        rst_n.write(false);
        wait(10, SC_NS);
        rst_n.write(true);
        settle();
    }

    void run()
    {
        using cfg = smc::memory_zeroer_cfg;

        std::cout << "==== SMC memory_zeroer TB ====\n";

        // Reset defaults ----------------------------------------------------
        pulse_reset();
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_SIZE));
        EXPECT_EQ(UINT64_C(0), drv.read64(cfg::OFF_CTRL_STATUS));
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] reset clears registers and irq\n";

        // DEST_ADDR / SIZE R/W ---------------------------------------------
        drv.write64(cfg::OFF_DEST_ADDR, 0x100);
        drv.write64(cfg::OFF_SIZE, 0x40);
        EXPECT_EQ(UINT64_C(0x100), drv.read64(cfg::OFF_DEST_ADDR));
        EXPECT_EQ(UINT64_C(0x40), drv.read64(cfg::OFF_SIZE));
        std::cout << "  [PASS] DEST_ADDR / SIZE R/W\n";

        // SIZE=0 write to CTRL_STATUS does not touch memory ----------------
        mem.fill(0xA5);
        drv.write64(cfg::OFF_SIZE, 0);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, drv.read64(cfg::OFF_CTRL_STATUS));
        EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.mem[0x100]));
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] SIZE=0 does not start a job\n";

        // Zero-fill a region (+ interrupt) ---------------------------------
        mem.fill(0xA5);
        drv.write64(cfg::OFF_DEST_ADDR, 0x80);
        drv.write64(cfg::OFF_SIZE, 0x100); // 256 bytes
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();

        // After LT job completes: idle (no busy bit), irq asserted.
        const uint64_t ctrl = drv.read64(cfg::OFF_CTRL_STATUS);
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, ctrl & cfg::CTRL_INT_EN_MASK);
        EXPECT_EQ(UINT64_C(0), ctrl & cfg::CTRL_STATUS_MASK);
        EXPECT_TRUE(irq.read());

        bool zeros_ok = true;
        for (uint64_t i = 0x80; i < 0x180; ++i) {
            if (mem.mem[static_cast<std::size_t>(i)] != 0) {
                zeros_ok = false;
                break;
            }
        }
        EXPECT_TRUE(zeros_ok);
        // Unrelated region untouched.
        EXPECT_EQ(0xA5u, static_cast<unsigned>(mem.mem[0x200]));
        std::cout << "  [PASS] zero-fill + completion irq (int_en=1)\n";

        // Chunked transfer larger than default chunk -----------------------
        // Preset chunk_size=64 in sc_main; zero 500 bytes.
        mem.fill(0x5A);
        drv.write64(cfg::OFF_DEST_ADDR, 0x10);
        drv.write64(cfg::OFF_SIZE, 500);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        bool chunk_ok = true;
        for (uint64_t i = 0x10; i < 0x10 + 500; ++i) {
            if (mem.mem[static_cast<std::size_t>(i)] != 0) {
                chunk_ok = false;
                break;
            }
        }
        EXPECT_TRUE(chunk_ok);
        EXPECT_EQ(0x5Au, static_cast<unsigned>(mem.mem[0x10 + 500]));
        std::cout << "  [PASS] multi-chunk zero-fill\n";

        // int_en=0: no irq on completion -----------------------------------
        pulse_reset();
        mem.fill(0xFF);
        drv.write64(cfg::OFF_DEST_ADDR, 0);
        drv.write64(cfg::OFF_SIZE, 16);
        drv.write64(cfg::OFF_CTRL_STATUS, 0); // start without int_en
        settle();
        EXPECT_FALSE(irq.read());
        for (int i = 0; i < 16; ++i)
            EXPECT_EQ(0u, static_cast<unsigned>(mem.mem[static_cast<std::size_t>(i)]));
        std::cout << "  [PASS] completion with int_en=0 leaves irq low\n";

        // status bit is RO to software -------------------------------------
        drv.write64(cfg::OFF_CTRL_STATUS,
                    cfg::CTRL_INT_EN_MASK | cfg::CTRL_STATUS_MASK);
        settle();
        // SIZE still 16 from above → job runs; busy cleared after.
        const uint64_t after = drv.read64(cfg::OFF_CTRL_STATUS);
        EXPECT_EQ(UINT64_C(0), after & cfg::CTRL_STATUS_MASK);
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK, after & cfg::CTRL_INT_EN_MASK);
        std::cout << "  [PASS] status bit is software-RO (write ignored)\n";

        // Clearing int_en drops irq ----------------------------------------
        EXPECT_TRUE(irq.read());
        drv.write64(cfg::OFF_SIZE, 0); // avoid starting another job
        drv.write64(cfg::OFF_CTRL_STATUS, 0);
        settle();
        EXPECT_FALSE(irq.read());
        std::cout << "  [PASS] clearing int_en deasserts irq\n";

        // Bus-error paths --------------------------------------------------
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x0, 4));
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x18, 8));
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_WRITE_COMMAND, 0x4, 8)); // unaligned
        std::cout << "  [PASS] bus error responses (size / window / align)\n";

        // DMA out-of-range fails without irq -------------------------------
        pulse_reset();
        drv.write64(cfg::OFF_DEST_ADDR, 0x7F00); // past 8 KiB mem
        drv.write64(cfg::OFF_SIZE, 0x200);
        drv.write64(cfg::OFF_CTRL_STATUS, cfg::CTRL_INT_EN_MASK);
        settle();
        EXPECT_FALSE(irq.read());
        EXPECT_EQ(cfg::CTRL_INT_EN_MASK,
                  drv.read64(cfg::OFF_CTRL_STATUS) & cfg::CTRL_INT_EN_MASK);
        std::cout << "  [PASS] failed DMA does not raise irq\n";

        // CCI introspection ------------------------------------------------
        {
            auto broker = cci::cci_get_broker();
            auto h = broker.get_param_handle("tb.zeroer.access_delay_ns");
            EXPECT_TRUE(h.is_valid());
            if (h.is_valid()) {
                // Preset was 5.0 — JSON form is "5.0".
                EXPECT_EQ(std::string("5.0"), h.get_cci_value().to_json());
                h.set_cci_value(cci::cci_value(7.0));
                EXPECT_EQ(std::string("7.0"), h.get_cci_value().to_json());
            }
            auto hc = broker.get_param_handle("tb.zeroer.chunk_size");
            EXPECT_TRUE(hc.is_valid());
            if (hc.is_valid()) {
                // Immutable — write must fail (SC_ERROR demoted in sc_main).
                hc.set_cci_value(cci::cci_value(128u));
                EXPECT_EQ(std::string("64"), hc.get_cci_value().to_json());
            }
            std::cout << "  [PASS] CCI discovery / mutation / immutability\n";
        }

        zeroer.dump_state(std::cout);

        if (g_failures == 0)
            std::cout << "\nALL TESTS PASSED\n";
        else
            std::cout << "\n" << g_failures << " FAILURE(S)\n";

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    // CCI immutable-write attempts raise SC_ERROR; demote so TB can probe.
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
                                            sc_core::SC_DISPLAY);

    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    cci::cci_originator platform_cfg("platform_cfg");
    auto global_broker = cci::cci_get_global_broker(platform_cfg);

    global_broker.set_preset_cci_value("tb.zeroer.chunk_size",
                                       cci::cci_value(64u));
    global_broker.set_preset_cci_value("tb.zeroer.access_delay_ns",
                                       cci::cci_value(5.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
