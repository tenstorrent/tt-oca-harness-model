// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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
                                      uint32_t len, void* data,
                                      unsigned char* be = nullptr,
                                      unsigned streaming = 0) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming ? streaming : len);
        gp.set_byte_enable_ptr(be);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        return gp.get_response_status();
    }

    uint64_t read64(uint64_t addr) {
        tlm::tlm_generic_payload gp;
        uint64_t data = 0;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return data;
    }

    void write64(uint64_t addr, uint64_t value) {
        tlm::tlm_generic_payload gp;
        sc_time t = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        gp.set_data_length(8);
        gp.set_streaming_width(8);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, t);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
    }

    unsigned dbg_xfer(tlm::tlm_command cmd, uint64_t addr, unsigned len,
                      void* data) {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<uint8_t*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return sock->transport_dbg(gp);
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

        // 14. CTRL readback reflects programmed fields + IP
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0x1000);
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_AWAKE_BIT |
                        smc::wdt_cfg::CTRL_RSTEN_BIT |
                        smc::wdt_cfg::CTRL_ZEROCMP_BIT | 0x3u);
        uint32_t ctrl = drv.read32(smc::wdt_cfg::OFF_CTRL);
        EXPECT_EQ(0x3u, ctrl & smc::wdt_cfg::CTRL_SCALE_MASK);
        EXPECT_TRUE((ctrl & smc::wdt_cfg::CTRL_RSTEN_BIT) != 0);
        EXPECT_TRUE((ctrl & smc::wdt_cfg::CTRL_ZEROCMP_BIT) != 0);
        EXPECT_TRUE((ctrl & smc::wdt_cfg::CTRL_ALWAYS_BIT) != 0);
        EXPECT_TRUE((ctrl & smc::wdt_cfg::CTRL_AWAKE_BIT) != 0);
        // Force IP via CTRL write of ip bit
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_IP_BIT);
        wait_delta();
        EXPECT_TRUE(dut.dbg_ip());
        EXPECT_TRUE((drv.read32(smc::wdt_cfg::OFF_CTRL) &
                     smc::wdt_cfg::CTRL_IP_BIT) != 0);
        std::cout << "  [PASS] CTRL readback / IP set\n";

        // 15. SCALED_COUNT + FEED reads; hole RAZ/WI
        EXPECT_EQ(dut.dbg_scaled(),
                  drv.read32(smc::wdt_cfg::OFF_SCALED_COUNT) & 0xFFFFu);
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_FEED));
        EXPECT_EQ(0u, drv.read32(0x04));  // hole between CTRL and COUNT
        unlock();
        drv.write32(0x04, 0x12345678u);  // hole WI (no unlock consume on unknown?)
        // Hole write does not change KEY lock state in our model when unlocked
        // was already consumed only on known regs — still unlocked until known write.
        // Actually default case returns without clearing unlock. Force a known write.
        drv.write32(smc::wdt_cfg::OFF_SCALED_COUNT, 0);
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        std::cout << "  [PASS] SCALED_COUNT/FEED/hole reads\n";

        // 16. Locked FEED / CTRL / COUNT / CMP ignored; bad FEED magic re-locks
        drv.write32(smc::wdt_cfg::OFF_FEED, smc::wdt_cfg::FEED_MAGIC);  // locked
        unlock();
        drv.write32(smc::wdt_cfg::OFF_FEED, 0xBADF00Du);  // wrong magic
        EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
        unlock();
        const uint32_t before_cnt = dut.dbg_count();
        const uint16_t before_cmp = dut.dbg_cmp();
        drv.write32(smc::wdt_cfg::OFF_CTRL, 0);  // consume unlock
        // locked CTRL / COUNT / CMP writes
        drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_RSTEN_BIT);
        EXPECT_FALSE((drv.read32(smc::wdt_cfg::OFF_CTRL) &
                      smc::wdt_cfg::CTRL_RSTEN_BIT) != 0);
        drv.write32(smc::wdt_cfg::OFF_COUNT, 0x55AAu);
        EXPECT_EQ(before_cnt, dut.dbg_count());
        drv.write32(smc::wdt_cfg::OFF_CMP, 0xBEEFu);
        EXPECT_EQ(before_cmp, dut.dbg_cmp());
        std::cout << "  [PASS] locked FEED/CTRL/COUNT + bad FEED\n";

        // 17. Feed while elapsed + zerocmp (covers was_elapsed && zerocmp path)
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CMP, 0);  // always elapsed
        unlock();
        drv.write32(smc::wdt_cfg::OFF_CTRL,
                    smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_ZEROCMP_BIT);
        wait_delta();
        EXPECT_TRUE(dut.dbg_elapsed());
        feed();
        wait_delta();
        EXPECT_EQ(0u, dut.dbg_count());
        EXPECT_TRUE(dut.dbg_ip());
        std::cout << "  [PASS] feed while elapsed+zerocmp\n";

        // 18. 64-bit MMIO: read CTRL+pad, write FEED+KEY combo, generic 64-bit write
        {
            const uint64_t ctrl64 = drv.read64(smc::wdt_cfg::OFF_CTRL);
            EXPECT_EQ(drv.read32(smc::wdt_cfg::OFF_CTRL),
                      static_cast<uint32_t>(ctrl64 & 0xFFFFFFFFu));
            // FEED (lo) + KEY (hi) in one beat at 0x18
            const uint64_t feed_key =
                (static_cast<uint64_t>(smc::wdt_cfg::KEY_MAGIC) << 32) |
                smc::wdt_cfg::FEED_MAGIC;
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CMP, 0x1000);
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_ALWAYS_BIT);
            dut.dbg_set_count(42);
            drv.write64(smc::wdt_cfg::OFF_FEED, feed_key);
            wait_delta();
            EXPECT_EQ(42u, dut.dbg_count());  // locked beat cannot feed
            EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
            // Already unlocked: the low half feeds and the KEY half of the
            // same beat cannot leave the lock set.
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CMP, 1);
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CTRL,
                        smc::wdt_cfg::CTRL_ALWAYS_BIT | smc::wdt_cfg::CTRL_RSTEN_BIT);
            dut.dbg_set_count(1);
            wait_delta();
            EXPECT_TRUE(sticky.read());
            EXPECT_EQ(1u, dut.dbg_count());
            unlock();
            drv.write64(smc::wdt_cfg::OFF_FEED, feed_key);
            wait_delta();
            EXPECT_EQ(0u, dut.dbg_count());
            EXPECT_EQ(0u, drv.read32(smc::wdt_cfg::OFF_KEY));
            EXPECT_FALSE(sticky.read());
            // IP survives the feed, on the debug flag and on the pin.
            EXPECT_TRUE(dut.dbg_ip());
            EXPECT_TRUE(irq.read());
            EXPECT_EQ(smc::wdt_cfg::CTRL_IP_BIT,
                      drv.read32(smc::wdt_cfg::OFF_CTRL) & smc::wdt_cfg::CTRL_IP_BIT);
            // Generic 64-bit write at COUNT (0x08): lo=COUNT, hi=hole @0x0C
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CMP, 0x1000);  // avoid zerocmp clear
            unlock();
            drv.write32(smc::wdt_cfg::OFF_CTRL, smc::wdt_cfg::CTRL_ALWAYS_BIT);
            unlock();
            const uint64_t count_pair = 0x0000'0000'0000'0077ull;
            drv.write64(smc::wdt_cfg::OFF_COUNT, count_pair);
            EXPECT_EQ(0x77u, dut.dbg_count());
        }
        std::cout << "  [PASS] 64-bit TLM paths\n";

        // 19. More negative TLM: misaligned, BE present, streaming_width, OOB, null
        {
            uint64_t scratch = 0;
            unsigned char be = 0xF;
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x2, 4, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, &scratch, &be));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, &scratch, nullptr,
                                   /*streaming=*/2));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, nullptr));
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_WRITE_COMMAND, 0x400, 4, &scratch));
            // Unknown command
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0x0, 4, &scratch));
            // FEED+KEY with bad KEY: unlock fails, feed skipped
            dut.dbg_set_count(99);
            const uint64_t bad_key_feed =
                (static_cast<uint64_t>(0xDEADBEEFu) << 32) | smc::wdt_cfg::FEED_MAGIC;
            drv.write64(smc::wdt_cfg::OFF_FEED, bad_key_feed);
            EXPECT_EQ(99u, dut.dbg_count());
        }
        std::cout << "  [PASS] extended negative TLM paths\n";

        // 20. transport_dbg read/write + rejection paths
        {
            uint32_t d32 = 0;
            uint64_t d64 = 0;
            EXPECT_EQ(4u, drv.dbg_xfer(tlm::TLM_READ_COMMAND,
                                       smc::wdt_cfg::OFF_CMP, 4, &d32));
            EXPECT_EQ(dut.dbg_cmp(), d32);
            EXPECT_EQ(8u, drv.dbg_xfer(tlm::TLM_READ_COMMAND,
                                       smc::wdt_cfg::OFF_CTRL, 8, &d64));
            d32 = 0xABCDu;
            unlock();
            EXPECT_EQ(4u, drv.dbg_xfer(tlm::TLM_WRITE_COMMAND,
                                       smc::wdt_cfg::OFF_CMP, 4, &d32));
            EXPECT_EQ(0xABCDu, dut.dbg_cmp());
            // Reject: bad length / OOB / null / unknown cmd
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x0, 2, &d32));
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x400, 4, &d32));
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, nullptr));
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_IGNORE_COMMAND, 0x0, 4, &d32));
            // Failed debug write (OOB) returns 0
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_WRITE_COMMAND, 0x400, 4, &d32));
            // Misaligned, and a 64-bit access whose high half leaves the window.
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x2, 4, &d32));
            EXPECT_EQ(0u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x3FC, 8, &d64));
            // Last legal 64-bit beat: both halves are inside the window.
            EXPECT_EQ(8u, drv.dbg_xfer(tlm::TLM_READ_COMMAND, 0x3F8, 8, &d64));
        }
        std::cout << "  [PASS] transport_dbg paths\n";

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

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);

    // tick_period_ns < 0 is fatal. Throw on SC_FATAL so the guard can be
    // observed, then put the previous action back before the rest of the bench.
    //
    // Only the rejecting values can be probed this way. wdt's guard fires ahead
    // of its socket and SC_METHOD registrations, so an aborted construction
    // leaves nothing behind; a module that constructs successfully and is then
    // destroyed would leave the kernel holding processes and a scheduled
    // tick_event_ on freed memory. The accepting side of the contract is
    // covered by tb itself, which builds the DUT with tick_period_ns = 0.0.
    {
        const sc_core::sc_actions prev_fatal =
            sc_core::sc_report_handler::set_actions(
                sc_core::SC_FATAL, sc_core::SC_DISPLAY | sc_core::SC_THROW);

        auto rejects = [](const char* probe_name, double tick_period_ns) {
            smc::wdt_cfg cfg;
            cfg.tick_period_ns = tick_period_ns;
            try {
                smc::wdt probe(probe_name, cfg);
            } catch (const sc_core::sc_report&) {
                return true;
            }
            return false;
        };

        EXPECT_TRUE(rejects("bad_tick_negative", -1.0));
        EXPECT_TRUE(rejects("bad_tick_tiny_negative", -0.001));

        sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL, prev_fatal);
    }
    std::cout << "  [PASS] tick_period_ns construction guard\n";

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
