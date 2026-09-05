// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// pll_wrapper_tb.cpp -- self-checking test bench for the composed SMC PLL
// wrapper.  Drives the wrapper's single 32-bit register target and exercises
// the composed address map (pll_cntl @0x000, cgm_0 @0x100, cgm_1 @0x200,
// awm_0 @0x400, awm_1 @0xA00), covering reset defaults, RW/RO enforcement,
// self-clearing / single-pulse strobes, per-block address decode + rebasing,
// sub-block independence, hardware-status back door, and CCI introspection.
//
// Prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

#include "pll_wrapper.h"

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
                      << std::hex << "  expected=0x" << (uint64_t)_e           \
                      << " actual=0x" << (uint64_t)_a << std::dec              \
                      << "  (" #expected " == " #actual ")\n";                 \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_STREQ(expected, actual)                                         \
    do {                                                                       \
        const std::string _e = (expected);                                     \
        const std::string _a = (actual);                                       \
        if (_e != _a) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                \
                      << "  expected=\"" << _e << "\" actual=\"" << _a         \
                      << "\"\n";                                               \
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

// ---------------------------------------------------------------------------
// Register-bus driver (32-bit accesses).
// ---------------------------------------------------------------------------
struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    uint32_t read32(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint32_t data = 0;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return data;
    }

    void write32(uint64_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
    }

    // 16-bit access — matches the firmware's read/write_cgm_pll_reg etc.
    uint16_t read16(uint64_t addr)
    {
        tlm::tlm_generic_payload gp;
        uint16_t data = 0;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(2);
        gp.set_streaming_width(2);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
        return data;
    }

    void write16(uint64_t addr, uint16_t data)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        gp.set_data_length(2);
        gp.set_streaming_width(2);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, gp.get_response_status());
    }

    tlm::tlm_response_status try_access(tlm::tlm_command cmd, uint64_t addr,
                                        unsigned len)
    {
        tlm::tlm_generic_payload gp;
        uint8_t buf[8] = {0};
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(buf);
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    // Raw TLM (nullptr data, len=0, IGNORE, custom streaming_width).
    tlm::tlm_response_status raw_xfer(tlm::tlm_command cmd, uint64_t addr,
                                      unsigned len, void* data,
                                      unsigned streaming_width = 0)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width == 0 ? len : streaming_width);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    tlm::tlm_response_status raw_xfer_be(tlm::tlm_command cmd, uint64_t addr,
                                         unsigned len, void* data,
                                         uint8_t* be_ptr, unsigned be_len)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(be_ptr);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }
};

// Standalone pll_cntl that exposes the protected per-register callback hooks
// so the bench can install bespoke read/write behaviour (and drive the
// sub-block socket directly for window-overrun faults).
struct probe_cntl : smc::pll::pll_cntl {
    explicit probe_cntl(sc_module_name n) : pll_cntl(n) {}

    bool install_write(uint64_t off, regmodel::Register32::WriteFn fn)
    {
        return set_write_callback(off, std::move(fn));
    }
    bool install_read(uint64_t off, regmodel::Register32::ReadFn fn)
    {
        return set_read_callback(off, std::move(fn));
    }
};

// ---------------------------------------------------------------------------
// Top-level TB
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    smc::pll::pll_wrapper dut;
    probe_cntl            probe;
    driver                drv;
    driver                probe_drv;

    SC_HAS_PROCESS(tb);

    explicit tb(sc_module_name n)
        : sc_module(n), dut("dut"), probe("probe"), drv("drv"),
          probe_drv("probe_drv")
    {
        dut.rst_n_i(rst_n);
        probe.rst_n_i(rst_n);
        drv.sock.bind(dut.reg_socket);
        probe_drv.sock.bind(probe.reg_socket);
        SC_THREAD(run);
    }

    void settle() { for (int i = 0; i < 3; ++i) wait(SC_ZERO_TIME); }

    void pulse_reset()
    {
        rst_n.write(false);
        wait(10, SC_NS);
        rst_n.write(true);
        settle();
    }

    void run()
    {
        using W  = smc::pll::pll_wrapper;
        using PC = smc::pll::pll_cntl;
        using AW = smc::pll::awm;

        std::cout << "==== SMC pll_wrapper TB ====\n";

        pulse_reset();

        // -- 1. Reset defaults across every sub-block ----------------------
        EXPECT_EQ(0x111u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_AWM_0_CTRL));
        EXPECT_EQ(0x111u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_AWM_1_CTRL));
        EXPECT_EQ(0x1500u,
                  drv.read32(W::OFF_PLL_CNTL + PC::OFF_GPIO_CLK_OBS_CTRL));
        EXPECT_EQ(0x0004u, drv.read32(W::OFF_CGM_0));            // cgm ENABLES
        EXPECT_EQ(0x0004u, drv.read32(W::OFF_CGM_1));
        // awm_0 GLOBAL RESOURCE_CONFIGURATION_ENABLES / FCW_INT_BOUND
        EXPECT_EQ(0x0300u,
                  drv.read32(W::OFF_AWM_0 + AW::GLOBAL_RESOURCE_CONFIG_ENABLES));
        EXPECT_EQ(0x1428u,
                  drv.read32(W::OFF_AWM_0 + AW::GLOBAL_FCW_INT_BOUND));
        // awm_0 FREQUENCY0.FCW_INT0 (=0x1010) and FREQUENCY5.FCW_INT0
        EXPECT_EQ(0x1010u, drv.read32(W::OFF_AWM_0 + AW::frequency_base(0)));
        EXPECT_EQ(0x1010u, drv.read32(W::OFF_AWM_0 + AW::frequency_base(5)));
        // awm_1 mirrors the same reset image (independent instance)
        EXPECT_EQ(0x0300u,
                  drv.read32(W::OFF_AWM_1 + AW::GLOBAL_RESOURCE_CONFIG_ENABLES));
        std::cout << "  [PASS] reset defaults across pll_cntl / cgm / awm\n";

        // -- 2. RW read/write-back with per-register masking ---------------
        // pll_cntl AG_MUX_SELECT: writable 0x3F3FFFFF.
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 0xFFFFFFFFu);
        EXPECT_EQ(0x3F3FFFFFu,
                  drv.read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
        // cgm_0 FCW_FRAC: writable 0x3FFF (16-bit reg in 32-bit word).
        drv.write32(W::OFF_CGM_0 + smc::pll::cgm::OFF_FCW_FRAC, 0xFFFFu);
        EXPECT_EQ(0x3FFFu,
                  drv.read32(W::OFF_CGM_0 + smc::pll::cgm::OFF_FCW_FRAC));
        // awm_0 GLOBAL MEAS_DURATION0: writable 0xFFFF.
        drv.write32(W::OFF_AWM_0 + 0x38, 0x1234ABCDu);
        EXPECT_EQ(0xABCDu, drv.read32(W::OFF_AWM_0 + 0x38));
        std::cout << "  [PASS] RW write-back honours per-field write masks\n";

        // -- 3. RO status registers reject software writes -----------------
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS, 0xFFFFFFFFu);
        EXPECT_EQ(0x0u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS));
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS, 0xFFFFFFFFu);
        EXPECT_EQ(0x0u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS));
        std::cout << "  [PASS] RO status registers are software read-only\n";

        // -- 4a. Single-pulse: GPIO postdiv_update_div never latches -------
        // Write bit16 (pulse) plus a normal RW field; bit16 reads back 0.
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_GPIO_CLK_OBS_CTRL,
                    PC::GPIO_POSTDIV_UPDATE_DIV | 0x5u /*gpio_mux_sel*/);
        {
            const uint32_t v =
                drv.read32(W::OFF_PLL_CNTL + PC::OFF_GPIO_CLK_OBS_CTRL);
            EXPECT_EQ(0u, v & PC::GPIO_POSTDIV_UPDATE_DIV);
            EXPECT_EQ(0x5u, v & 0x7u);
        }
        // -- 4b. Self-clearing awm GLOBAL REG_UPDATE (write-only) ----------
        drv.write32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE, 0x3u);
        EXPECT_EQ(0x0u, drv.read32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE));
        std::cout << "  [PASS] single-pulse + self-clearing strobes\n";

        // -- 5. Sub-block independence (cgm_0 vs cgm_1, awm_0 vs awm_1) -----
        drv.write32(W::OFF_CGM_0 + smc::pll::cgm::OFF_FCW_INT, 0x33u);
        drv.write32(W::OFF_CGM_1 + smc::pll::cgm::OFF_FCW_INT, 0x55u);
        EXPECT_EQ(0x33u, drv.read32(W::OFF_CGM_0 + smc::pll::cgm::OFF_FCW_INT));
        EXPECT_EQ(0x55u, drv.read32(W::OFF_CGM_1 + smc::pll::cgm::OFF_FCW_INT));
        drv.write32(W::OFF_AWM_0 + 0x38, 0x0AAAu);
        drv.write32(W::OFF_AWM_1 + 0x38, 0x0555u);
        EXPECT_EQ(0x0AAAu, drv.read32(W::OFF_AWM_0 + 0x38));
        EXPECT_EQ(0x0555u, drv.read32(W::OFF_AWM_1 + 0x38));
        std::cout << "  [PASS] sub-block instances are independent\n";

        // -- 6. Address decode: reserved gaps + past window fault ----------
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x300, 4)); // cgm gap
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x900, 4)); // awm gap
        EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0xF00, 4)); // past awm_1
        std::cout << "  [PASS] reserved-gap accesses fault (decode)\n";

        // -- 7. Size / alignment errors propagate from the sub-block -------
        // 1/2/4-byte accesses are all supported (firmware uses 16-bit), but
        // must be a supported width, naturally aligned, and not straddle a
        // 32-bit register boundary.
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x010, 3)); // bad width
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x012, 4)); // 4B unaligned
        EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                  drv.try_access(tlm::TLM_READ_COMMAND, 0x011, 2)); // 2B unaligned
        std::cout << "  [PASS] size / alignment errors\n";

        // -- 8. Hardware-status back door (poke) then software read --------
        // Drive awm_0 GLOBAL LOCK_STATUS lock_detect[5:3] via the model back
        // door (RO to software) and confirm the readback path exposes it.
        EXPECT_TRUE(dut.awm_0().poke(AW::GLOBAL_LOCK_STATUS, 0x0038u));
        EXPECT_EQ(0x0038u, drv.read32(W::OFF_AWM_0 + AW::GLOBAL_LOCK_STATUS));
        std::cout << "  [PASS] HW-driven status readback via back door\n";

        // -- 9. Reset restores every register to its RDL default -----------
        pulse_reset();
        EXPECT_EQ(0x111u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_AWM_0_CTRL));
        EXPECT_EQ(0x0u,
                  drv.read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
        EXPECT_EQ(0x0u, drv.read32(W::OFF_AWM_0 + 0x38));
        EXPECT_EQ(0x0u, drv.read32(W::OFF_AWM_0 + AW::GLOBAL_LOCK_STATUS));
        EXPECT_EQ(0x1010u, drv.read32(W::OFF_AWM_1 + AW::frequency_base(0)));
        std::cout << "  [PASS] reset restores RDL defaults\n";

        // -- 10. CCI introspection: each sub-block exposes access_delay_ns --
        {
            auto broker = cci::cci_get_broker();
            auto h = broker.get_param_handle("tb.dut.pll_cntl.access_delay_ns");
            EXPECT_TRUE(h.is_valid());
            if (h.is_valid()) {
                h.set_cci_value(cci::cci_value(3.0));
                EXPECT_STREQ("3.0", h.get_cci_value().to_json());
            }
            EXPECT_TRUE(
                broker.get_param_handle("tb.dut.awm_0.access_delay_ns")
                    .is_valid());
            EXPECT_TRUE(
                broker.get_param_handle("tb.dut.cgm_1.access_delay_ns")
                    .is_valid());
            std::cout << "  [PASS] CCI discovery / mutation per sub-block\n";
        }

        // -- 11. Firmware-style CGM program + lock poll (16-bit access) ----
        // Emulate program_cgm(): the firmware uses 16-bit MMIO, sets
        // cgm_enable + freq_acq_enable, programs the config, strobes
        // REG_UPDATE, then busy-polls pll_cntl.CGM_x_STATUS.lock_detect until
        // it reads 1.  The model must make that loop terminate.
        using CG = smc::pll::cgm;
        pulse_reset();
        {
            // lock not asserted until REG_UPDATE
            EXPECT_EQ(0u,
                      drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);

            drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x3u); // cgm+freq_acq
            drv.write16(W::OFF_CGM_0 + CG::OFF_FCW_INT, 20);
            drv.write16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 0x1u);

            uint32_t lock = 0;
            int spins = 0;
            do {
                lock = drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u;
            } while (lock != 1u && ++spins < 1000);
            EXPECT_EQ(1u, lock);
            // sub-block CGM_STATUS mirrors lock for diagnostic reads
            EXPECT_EQ(0x1u,
                      drv.read16(W::OFF_CGM_0 + CG::OFF_CGM_STATUS) & 0x1u);
            // REG_UPDATE self-clears
            EXPECT_EQ(0u, drv.read16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE));
        }
        std::cout << "  [PASS] firmware CGM program + lock poll (16-bit)\n";

        // -- 12. Disable + REG_UPDATE drops CGM lock -----------------------
        drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x0u);      // cgm_enable=0
        drv.write16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 0x1u);
        EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
        std::cout << "  [PASS] CGM disable + REG_UPDATE clears lock\n";

        // -- 13. AWM REG_UPDATE drives pll_cntl AWM lock status ------------
        // AWM_0 must report lock_detect==7 (all three CGMs), AWM_1==1.  The
        // firmware writes GLOBAL REG_UPDATE with a 32-bit store.
        drv.write32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE, 0x1u);
        EXPECT_EQ(0x7u,
                  drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);
        // sub-block GLOBAL_LOCK_STATUS.lock_detect[5:3] mirrors it
        EXPECT_EQ(0x7u,
                  (drv.read32(W::OFF_AWM_0 + AW::GLOBAL_LOCK_STATUS) >> 3) & 0x7u);
        drv.write32(W::OFF_AWM_1 + AW::GLOBAL_REG_UPDATE, 0x1u);
        EXPECT_EQ(0x1u,
                  drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_1_STATUS) & 0x7u);
        std::cout << "  [PASS] AWM REG_UPDATE drives pll_cntl lock status\n";

        // -- 14. 16-bit RMW must preserve the 32-bit register high half ----
        // AG_MUX_SELECT is a 32-bit register; a 16-bit low-half write must not
        // disturb bits [31:16].
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 0x3F3F0000u);
        drv.write16(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 0xBEEFu);
        EXPECT_EQ(0x3F3FBEEFu,
                  drv.read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
        // 16-bit read of the high half returns bits [31:16]
        EXPECT_EQ(0x3F3Fu,
                  drv.read16(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT + 2));
        std::cout << "  [PASS] 16-bit RMW preserves 32-bit register high half\n";

        // -- 15. 8-bit lane access (firmware may use byte MMIO) ------------
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 0x11223344u);
        {
            uint8_t b = 0xAAu;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.raw_xfer(tlm::TLM_WRITE_COMMAND,
                                   W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT + 1,
                                   1, &b));
            EXPECT_EQ(0x1122AA44u,
                      drv.read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
            uint8_t rb = 0;
            EXPECT_EQ(tlm::TLM_OK_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND,
                                   W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT + 1,
                                   1, &rb));
            EXPECT_EQ(0xAAu, rb);
        }
        std::cout << "  [PASS] 8-bit lane RMW\n";

        // -- 16. CGM_1 lock + REG_UPDATE with bit0 clear is a no-op --------
        {
            using CG = smc::pll::cgm;
            drv.write16(W::OFF_CGM_1 + CG::OFF_ENABLES, 0x3u);
            drv.write16(W::OFF_CGM_1 + CG::OFF_REG_UPDATE, 0x0u); // no strobe
            EXPECT_EQ(0u,
                      drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_1_STATUS) & 0x1u);
            drv.write16(W::OFF_CGM_1 + CG::OFF_REG_UPDATE, 0x1u);
            EXPECT_EQ(1u,
                      drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_1_STATUS) & 0x1u);
            // AWM_0 REG_UPDATE with bit0 clear must not change lock_detect.
            const uint32_t before =
                drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u;
            drv.write32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE, 0x2u);
            EXPECT_EQ(before,
                      drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);
        }
        std::cout << "  [PASS] CGM_1 lock + REG_UPDATE bit0-clear is a no-op\n";

        // -- 17. TLM protocol errors forwarded into the sub-block ----------
        {
            uint32_t scratch = 0;
            uint8_t  be      = 0xFF;
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, nullptr));
            EXPECT_EQ(tlm::TLM_GENERIC_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_WRITE_COMMAND, 0x0, 0, &scratch));
            EXPECT_EQ(tlm::TLM_BURST_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_READ_COMMAND, 0x0, 4, &scratch,
                                   /*streaming_width=*/1));
            EXPECT_EQ(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                      drv.raw_xfer_be(tlm::TLM_READ_COMMAND, 0x0, 4, &scratch,
                                      &be, 1));
            EXPECT_EQ(tlm::TLM_COMMAND_ERROR_RESPONSE,
                      drv.raw_xfer(tlm::TLM_IGNORE_COMMAND, 0x0, 4, &scratch));
        }
        std::cout << "  [PASS] TLM protocol errors (null/len/be/sw/cmd)\n";

        // -- 18. Sub-block window overrun + callback install (probe) -------
        {
            uint32_t scratch = 0;
            EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      probe_drv.raw_xfer(tlm::TLM_READ_COMMAND,
                                         smc::pll::pll_cntl_cfg::WINDOW_SIZE, 4,
                                         &scratch));
            // Unknown offset: callback install fails.
            EXPECT_TRUE(!probe.install_write(
                0x1000, [](uint32_t cur, uint32_t) { return cur; }));
            EXPECT_TRUE(!probe.install_read(
                0x1000, [](uint32_t stored) { return stored; }));
            // Known offset: replace write (keep only low byte) and read.
            EXPECT_TRUE(probe.install_write(
                PC::OFF_AG_MUX_SELECT,
                [](uint32_t, uint32_t in) { return in & 0xFFu; }));
            probe_drv.write32(PC::OFF_AG_MUX_SELECT, 0xFFFFFFFFu);
            EXPECT_EQ(0xFFu, probe_drv.read32(PC::OFF_AG_MUX_SELECT));
            EXPECT_TRUE(probe.install_read(
                PC::OFF_AG_MUX_SELECT,
                [](uint32_t) { return 0xA5A5A5A5u; }));
            EXPECT_EQ(0xA5A5A5A5u, probe_drv.read32(PC::OFF_AG_MUX_SELECT));
        }
        std::cout << "  [PASS] window overrun + set_read/write_callback\n";

        // -- 19. peek/poke miss + dump_state smoke -------------------------
        {
            uint32_t tmp = 0xDEADu;
            EXPECT_TRUE(!dut.cntl().peek(0x1000, tmp));
            EXPECT_TRUE(!dut.cntl().poke(0x1000, 0x1u));
            std::ostringstream oss;
            dut.dump_state(oss);
            const std::string s = oss.str();
            EXPECT_TRUE(s.find("pll_wrapper composed state") != std::string::npos);
            EXPECT_TRUE(s.find("reg_block state") != std::string::npos);
            EXPECT_TRUE(s.find("CGM_0_STATUS") != std::string::npos);
        }
        std::cout << "  [PASS] peek/poke miss + dump_state\n";

        if (g_failures == 0)
            std::cout << "\nALL TESTS PASSED\n";
        else
            std::cout << "\n" << g_failures << " FAILURE(S)\n";

        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
