// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// reset_unit_tb.cpp -- self-checking test bench for the SMC Reset Unit
// (CCI-compliant).
//
// Coverage:
//   • register read/write (RW registers + read-only registers)
//   • SS_CONFIG_LOCK / SS_COLD_RESET_LOCK woset + write-protect semantics
//   • cold/primary/core/WDT reset derivation from input pins
//   • WDT second-timeout forcing core/WDT reset
//   • per-subsystem reset-control bundle (SS_COLD/WARM/HOLD/FORCE) → ss_reset_ctrl_o
//   • JTAG overrides (cold reset + per-subsystem warm reset)
//   • FLR cool-reset flow: isolate_req_smc set, skip_mem_repair, rst_cool_no pulse
//   • isolate-request logic (SW reg + pin-enable + smc-enable)
//   • STRAPS_LO / STRAPS_HI reflect captured straps
//   • register reset domains: cold reset clears all; cool reset retains FLR regs
//   • transport_dbg back-door read/write
//   • CCI introspection / mutation / immutability
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
#include <sstream>
#include <string>

#include "reset_unit.h"

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

using smc::reset_unit_cfg;

// ---------------------------------------------------------------------------
// Test bench root
// ---------------------------------------------------------------------------
struct tb : sc_core::sc_module {
    SC_HAS_PROCESS(tb);

    smc::reset_unit dut;
    driver          drv;

    // Input signals.
    sc_core::sc_signal<bool>     powergood{"powergood"};
    sc_core::sc_signal<bool>     rst_cold_n{"rst_cold_n"};
    sc_core::sc_signal<bool>     fuse_reset_n{"fuse_reset_n"};
    sc_core::sc_signal<bool>     rst_ext_wdt_n{"rst_ext_wdt_n"};
    sc_core::sc_signal<bool>     wdt_first{"wdt_first"};
    sc_core::sc_signal<bool>     wdt_second{"wdt_second"};
    sc_core::sc_signal<bool>     rst_cool_n{"rst_cool_n"};
    sc_core::sc_signal<bool>     isolate_pin{"isolate_pin"};
    sc_core::sc_signal<bool>     cfg_flr{"cfg_flr"};
    sc_core::sc_signal<uint32_t> ss_reset_complete{"ss_reset_complete"};
    sc_core::sc_signal<uint64_t> straps{"straps"};

    // Output signals.
    sc_core::sc_signal<bool>     powergood_stable{"powergood_stable"};
    sc_core::sc_signal<bool>     cold_stable_ref{"cold_stable_ref"};
    sc_core::sc_signal<bool>     cold_stable_smc{"cold_stable_smc"};
    sc_core::sc_signal<bool>     primary_ref{"primary_ref"};
    sc_core::sc_signal<bool>     primary_smc{"primary_smc"};
    sc_core::sc_signal<bool>     primary_periph{"primary_periph"};
    sc_core::sc_signal<bool>     core_smc{"core_smc"};
    sc_core::sc_signal<bool>     wdt_smc{"wdt_smc"};
    sc_core::sc_signal<bool>     cool_no{"cool_no"};
    sc_core::sc_signal<bool>     skip_mem_repair{"skip_mem_repair"};
    sc_core::sc_signal<bool>     sync_irq{"sync_irq"};
    sc_core::sc_signal<uint32_t> isolate_req{"isolate_req"};
    sc_core::sc_signal<uint32_t> ss_config{"ss_config"};
    sc_core::sc_vector<sc_core::sc_signal<smc::reset_ctrl_t>> ss_ctrl{"ss_ctrl", 32};

    explicit tb(sc_module_name n)
        : sc_module(n), dut("reset_unit"), drv("drv")
    {
        drv.sock.bind(dut.reg_socket);

        dut.powergood_i(powergood);
        dut.rst_cold_ni(rst_cold_n);
        dut.fuse_reset_ni(fuse_reset_n);
        dut.rst_ext_wdt_ni(rst_ext_wdt_n);
        dut.smc_wdt_first_timeout_i(wdt_first);
        dut.smc_wdt_second_timeout_i(wdt_second);
        dut.rst_cool_ni(rst_cool_n);
        dut.isolate_req_pin_i(isolate_pin);
        dut.cfg_flr_pf_active_i(cfg_flr);
        dut.ss_reset_complete_i(ss_reset_complete);
        dut.captured_straps_i(straps);

        dut.powergood_stable_o(powergood_stable);
        dut.rst_cold_stable_ref_clk_no(cold_stable_ref);
        dut.rst_cold_stable_smc_clk_no(cold_stable_smc);
        dut.rst_primary_ref_clk_no(primary_ref);
        dut.rst_primary_smc_clk_no(primary_smc);
        dut.rst_primary_periph_clk_no(primary_periph);
        dut.rst_core_smc_clk_no(core_smc);
        dut.rst_wdt_smc_clk_no(wdt_smc);
        dut.rst_cool_no(cool_no);
        dut.skip_mem_repair_o(skip_mem_repair);
        dut.sync_irq_o(sync_irq);
        dut.isolate_req_o(isolate_req);
        dut.ss_config_o(ss_config);
        for (unsigned i = 0; i < 32; ++i) dut.ss_reset_ctrl_o[i](ss_ctrl[i]);

        SC_THREAD(run);
    }

    // Drive a known "out of reset" pin state and settle.
    void release_resets() {
        powergood.write(true);
        rst_cold_n.write(true);
        fuse_reset_n.write(true);
        rst_ext_wdt_n.write(true);
        wdt_first.write(false);
        wdt_second.write(false);
        rst_cool_n.write(true);
        isolate_pin.write(false);
        cfg_flr.write(false);
        settle();
    }

    void settle() { sc_core::wait(2, SC_NS); }

    void run() {
        std::cout << "==== SMC Reset Unit TB (CCI-compliant) ====\n"
                  << "  num_subsystems = " << dut.num_subsystems() << "\n";

        release_resets();

        // ------------------------------------------------------------------
        // 1. Out-of-reset derivation: all resets de-asserted (high).
        // ------------------------------------------------------------------
        EXPECT_TRUE(powergood_stable.read());
        EXPECT_TRUE(cold_stable_smc.read());
        EXPECT_TRUE(primary_smc.read());
        EXPECT_TRUE(core_smc.read());
        EXPECT_TRUE(wdt_smc.read());
        EXPECT_TRUE(cool_no.read());
        std::cout << "  [PASS] out-of-reset: all derived resets de-asserted\n";

        // ------------------------------------------------------------------
        // 2. Basic RW register access + reset defaults.
        // ------------------------------------------------------------------
        EXPECT_EQ(uint32_t(0xFFFFFFFFu), drv.read(reset_unit_cfg::SS_WARM_RESET_N));
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::SS_COLD_RESET_N));
        drv.write(reset_unit_cfg::SS_WARM_RESET_N, 0x12345678u);
        EXPECT_EQ(uint32_t(0x12345678u), drv.read(reset_unit_cfg::SS_WARM_RESET_N));
        drv.write(reset_unit_cfg::SS_CONFIG_HOLD, 0x0000FFFFu);
        EXPECT_EQ(uint32_t(0x0000FFFFu), drv.read(reset_unit_cfg::SS_CONFIG_HOLD));
        drv.write(reset_unit_cfg::SS_DEBUG_HOLD, 0xA5A5A5A5u);
        EXPECT_EQ(uint32_t(0xA5A5A5A5u), drv.read(reset_unit_cfg::SS_DEBUG_HOLD));
        std::cout << "  [PASS] RW register access + reset defaults\n";

        // ------------------------------------------------------------------
        // 3. Lock semantics (woset + write-protect).
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SS_CONFIG, 0xFFFFFFFFu);
        EXPECT_EQ(uint32_t(0xFFFFFFFFu), drv.read(reset_unit_cfg::SS_CONFIG));
        // Lock the low byte (woset: write-1-to-set).
        drv.write(reset_unit_cfg::SS_CONFIG_LOCK, 0x000000FFu);
        EXPECT_EQ(uint32_t(0x000000FFu), drv.read(reset_unit_cfg::SS_CONFIG_LOCK));
        // woset is sticky: writing 0s does not clear it; new bits accumulate.
        drv.write(reset_unit_cfg::SS_CONFIG_LOCK, 0x0000FF00u);
        EXPECT_EQ(uint32_t(0x0000FFFFu), drv.read(reset_unit_cfg::SS_CONFIG_LOCK));
        // Now attempt to clear all SS_CONFIG bits: locked [15:0] survive.
        drv.write(reset_unit_cfg::SS_CONFIG, 0x00000000u);
        EXPECT_EQ(uint32_t(0x0000FFFFu), drv.read(reset_unit_cfg::SS_CONFIG));
        std::cout << "  [PASS] lock registers: woset + write-protect\n";

        // ------------------------------------------------------------------
        // 4. Per-subsystem reset-control bundle.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SS_COLD_RESET_N, 0x00000005u); // SS0, SS2 cold de-asserted
        drv.write(reset_unit_cfg::SS_WARM_RESET_N, 0x00000002u); // SS1 warm de-asserted
        drv.write(reset_unit_cfg::SS_SRAM_HOLD,    0x00000004u); // SS2 sram hold
        drv.write(reset_unit_cfg::SS_FORCE_TO_REF_CLK, 0x00000001u); // SS0 force-ref
        settle();
        {
            auto s0 = dut.dbg_ss_reset_ctrl(0);
            auto s1 = dut.dbg_ss_reset_ctrl(1);
            auto s2 = dut.dbg_ss_reset_ctrl(2);
            EXPECT_TRUE(s0.cold_reset_n);             // bit0 of 0x5
            EXPECT_TRUE(!s1.cold_reset_n);            // bit1 clear
            EXPECT_TRUE(s2.cold_reset_n);             // bit2 of 0x5
            EXPECT_TRUE(s1.warm_reset_n);             // bit1 of 0x2
            EXPECT_TRUE(!s0.warm_reset_n);            // bit0 clear
            EXPECT_TRUE(s2.sram_hold);                // bit2 sram hold
            EXPECT_TRUE(s0.force_to_ref_clk_n);       // bit0 force-ref
            // Output signal mirrors the back door.
            EXPECT_TRUE(ss_ctrl[0].read().cold_reset_n);
            EXPECT_TRUE(ss_ctrl[2].read().sram_hold);
        }
        std::cout << "  [PASS] per-subsystem reset-control bundle\n";

        // ------------------------------------------------------------------
        // 5. SS_CONFIG drives ss_config_o.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SS_CONFIG_LOCK, 0); // (lock is sticky; ignored)
        // Unlocked bits [31:16] are writable; write a recognisable pattern.
        drv.write(reset_unit_cfg::SS_CONFIG, 0xBEEF0000u);
        settle();
        EXPECT_EQ(uint32_t(0xBEEF0000u | (drv.read(reset_unit_cfg::SS_CONFIG) & 0xFFFFu)),
                  ss_config.read());
        std::cout << "  [PASS] SS_CONFIG drives ss_config_o\n";

        // ------------------------------------------------------------------
        // 6. STRAPS reflect captured straps input.
        // ------------------------------------------------------------------
        straps.write(0xCAFEF00D'12345678ull);
        settle();
        EXPECT_EQ(uint32_t(0x12345678u), drv.read(reset_unit_cfg::STRAPS_LO));
        EXPECT_EQ(uint32_t(0xCAFEF00Du), drv.read(reset_unit_cfg::STRAPS_HI));
        std::cout << "  [PASS] STRAPS_LO/HI reflect captured straps\n";

        // ------------------------------------------------------------------
        // 7. Cold reset asserts the whole tree and clears all registers.
        // ------------------------------------------------------------------
        rst_cold_n.write(false);
        settle();
        EXPECT_TRUE(!cold_stable_smc.read());
        EXPECT_TRUE(!primary_smc.read());
        EXPECT_TRUE(!core_smc.read());
        rst_cold_n.write(true);
        settle();
        // Registers back at reset defaults.
        EXPECT_EQ(uint32_t(0xFFFFFFFFu), drv.read(reset_unit_cfg::SS_WARM_RESET_N));
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::SS_CONFIG));
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::SS_CONFIG_LOCK));
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::SS_DEBUG_HOLD));
        EXPECT_TRUE(primary_smc.read());
        std::cout << "  [PASS] cold reset clears registers, then releases\n";

        // ------------------------------------------------------------------
        // 8. WDT second-timeout forces core + WDT reset only (primary stays up).
        // ------------------------------------------------------------------
        wdt_second.write(true);
        settle();
        EXPECT_TRUE(!core_smc.read());      // core reset asserted
        EXPECT_TRUE(!wdt_smc.read());       // wdt reset asserted
        EXPECT_TRUE(primary_smc.read());    // primary unaffected
        wdt_second.write(false);
        settle();
        EXPECT_TRUE(core_smc.read());
        std::cout << "  [PASS] WDT second-timeout forces core/WDT reset\n";

        // ------------------------------------------------------------------
        // 9. Fuse reset gates the core reset only.
        // ------------------------------------------------------------------
        fuse_reset_n.write(false);
        settle();
        EXPECT_TRUE(!core_smc.read());
        EXPECT_TRUE(primary_smc.read());
        fuse_reset_n.write(true);
        settle();
        EXPECT_TRUE(core_smc.read());
        std::cout << "  [PASS] fuse reset gates core reset\n";

        // ------------------------------------------------------------------
        // 10. JTAG overrides.
        // ------------------------------------------------------------------
        {
            smc::jtag_reset_ctrl j;
            j.cold_reset_n_ovrd = true;
            j.cold_reset_n_val  = false;  // force cold reset asserted
            dut.set_jtag_ctrl(j);
            settle();
            EXPECT_TRUE(!cold_stable_smc.read());
            EXPECT_TRUE(!primary_smc.read());

            // Per-subsystem warm-reset override: force SS3 warm reset low.
            smc::jtag_reset_ctrl j2;
            j2.ss_warm_reset_n_ovrd = (1u << 3);
            j2.ss_warm_reset_n_val  = 0; // bit3 forced to 0
            dut.set_jtag_ctrl(j2);
            settle();
            EXPECT_TRUE(!dut.dbg_ss_reset_ctrl(3).warm_reset_n);

            dut.set_jtag_ctrl(smc::jtag_reset_ctrl{}); // clear all overrides
            settle();
            EXPECT_TRUE(cold_stable_smc.read());
        }
        std::cout << "  [PASS] JTAG cold + per-subsystem overrides\n";

        // ------------------------------------------------------------------
        // 11. Isolate-request logic (SW reg + pin-enable + smc-enable).
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::ISOLATE_REQ_REG,       0x00000001u); // SW SS0
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0x00000002u); // SS1 via pin
        settle();
        EXPECT_EQ(uint32_t(0x00000001u), isolate_req.read()); // pin low ⇒ only SW
        isolate_pin.write(true);
        settle();
        EXPECT_EQ(uint32_t(0x00000003u), isolate_req.read()); // SW | (pinen & pin)
        EXPECT_TRUE(skip_mem_repair.read());                  // pin high ⇒ skip repair
        EXPECT_EQ(uint32_t(1), drv.read(reset_unit_cfg::ISOLATE_REQ_VIS) & 1u);
        isolate_pin.write(false);
        drv.write(reset_unit_cfg::ISOLATE_REQ_REG, 0);
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0);
        settle();
        std::cout << "  [PASS] isolate-request logic (SW + pin-enable)\n";

        // ------------------------------------------------------------------
        // 12. FLR cool-reset flow + smc-enable isolate.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::ISOLATE_REQ_SMCEN_REG, 0x00000010u); // SS4 via FLR
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE, 2);       // delay 2 cycles
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE, 4); // hold 4 cycles
        settle();
        // Rising edge of FLR active.
        cfg_flr.write(true);
        settle();
        EXPECT_EQ(uint32_t(1), drv.read(reset_unit_cfg::ISOLATE_REQ_SMC_REG) & 1u);
        EXPECT_EQ(uint32_t(0x00000010u), isolate_req.read()); // smcen & smc latch
        EXPECT_TRUE(skip_mem_repair.read());
        EXPECT_TRUE(cool_no.read());  // not yet asserted (delay = 2 × 10 ns)
        // After the delay the cool reset asserts (low).
        sc_core::wait(25, SC_NS);
        EXPECT_TRUE(!cool_no.read());
        // After the hold window it releases (high) again.
        sc_core::wait(50, SC_NS);
        EXPECT_TRUE(cool_no.read());
        // Software clears the isolate-smc latch by writing the register.
        drv.write(reset_unit_cfg::ISOLATE_REQ_SMC_REG, 0x1u);
        settle();
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::ISOLATE_REQ_SMC_REG) & 1u);
        EXPECT_EQ(uint32_t(0), isolate_req.read());
        cfg_flr.write(false);
        settle();
        std::cout << "  [PASS] FLR cool-reset flow + isolate latch\n";

        // ------------------------------------------------------------------
        // 13. Cool reset (from pin) retains FLR registers but clears SS regs.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SS_WARM_RESET_N, 0x0000ABCDu);   // primary domain
        drv.write(reset_unit_cfg::ISOLATE_REQ_REG, 0x00ABCD00u);   // cold domain
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE, 7); // cold domain
        settle();
        rst_cool_n.write(false);   // assert primary (cool-from-pin), cold stays up
        settle();
        EXPECT_TRUE(!primary_smc.read());
        EXPECT_TRUE(cold_stable_smc.read()); // cold domain not asserted
        rst_cool_n.write(true);
        settle();
        // Primary-domain register cleared ...
        EXPECT_EQ(uint32_t(0xFFFFFFFFu), drv.read(reset_unit_cfg::SS_WARM_RESET_N));
        // ... cold-domain (FLR/isolate) registers retained.
        EXPECT_EQ(uint32_t(0x00ABCD00u), drv.read(reset_unit_cfg::ISOLATE_REQ_REG));
        EXPECT_EQ(uint32_t(7), drv.read(reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE));
        std::cout << "  [PASS] cool reset clears SS regs, retains FLR regs\n";

        // ------------------------------------------------------------------
        // 14. SYNC_REG drives sync_irq_o.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SYNC_REG, 0x1u);
        settle();
        EXPECT_TRUE(sync_irq.read());
        drv.write(reset_unit_cfg::SYNC_REG, 0x0u);
        settle();
        EXPECT_TRUE(!sync_irq.read());
        std::cout << "  [PASS] SYNC_REG drives sync_irq_o\n";

        // ------------------------------------------------------------------
        // 15. transport_dbg back-door read/write (no FLR side effects).
        // ------------------------------------------------------------------
        {
            tlm::tlm_generic_payload gp;
            uint32_t data = 0xDEADBEEFu;
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(reset_unit_cfg::SS_SRAM_HOLD);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
            gp.set_data_length(4);
            gp.set_streaming_width(4);
            gp.set_byte_enable_ptr(nullptr);
            EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(uint32_t(0xDEADBEEFu), dut.dbg_read(reset_unit_cfg::SS_SRAM_HOLD));

            uint32_t rb = 0;
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_data_ptr(reinterpret_cast<uint8_t*>(&rb));
            EXPECT_EQ(4u, drv.sock->transport_dbg(gp));
            EXPECT_EQ(uint32_t(0xDEADBEEFu), rb);
        }
        std::cout << "  [PASS] transport_dbg back-door read/write\n";

        // ------------------------------------------------------------------
        // 16. CCI introspection / mutation / immutability.
        // ------------------------------------------------------------------
        {
            auto broker  = cci::cci_get_broker();
            auto h_ns    = broker.get_param_handle("tb.reset_unit.num_subsystems");
            auto h_ref   = broker.get_param_handle("tb.reset_unit.ref_clk_period_ns");
            auto h_delay = broker.get_param_handle("tb.reset_unit.access_delay_ns");
            EXPECT_TRUE(h_ns.is_valid());
            EXPECT_TRUE(h_ref.is_valid());
            EXPECT_TRUE(h_delay.is_valid());
            EXPECT_EQ(uint64_t(dut.num_subsystems()),
                      uint64_t(h_ns.get_cci_value().get_uint()));

            const double old_delay = h_delay.get_cci_value().get_double();
            h_delay.set_cci_value(cci::cci_value(old_delay * 2.0));
            (void)drv.read(reset_unit_cfg::SS_WARM_RESET_N);
            EXPECT_EQ(old_delay * 2.0, h_delay.get_cci_value().get_double());

            const uint64_t old_ns = h_ns.get_cci_value().get_uint();
            try {
                h_ns.set_cci_value(cci::cci_value(uint64_t(old_ns + 1)));
            } catch (...) { /* immutable write may throw; acceptable */ }
            EXPECT_EQ(old_ns, uint64_t(h_ns.get_cci_value().get_uint()));
        }
        std::cout << "  [PASS] CCI: discovery, mutation, immutability\n";

        // ------------------------------------------------------------------
        // 17. dump_state smoke test.
        // ------------------------------------------------------------------
        {
            std::ostringstream oss;
            dut.dump_state(oss);
            const std::string s = oss.str();
            EXPECT_TRUE(s.find("reset_unit state") != std::string::npos);
            EXPECT_TRUE(s.find("primary_n")        != std::string::npos);
        }
        std::cout << "  [PASS] dump_state contains expected fields\n";

        // ------------------------------------------------------------------
        // 18. Full register read/write sweep (region/branch coverage):
        //     exercise every reg_read / reg_write case not hit above.
        // ------------------------------------------------------------------
        // SS_CRITICAL_HOLD: RW round-trip.
        drv.write(reset_unit_cfg::SS_CRITICAL_HOLD, 0x0F0F0F0Fu);
        EXPECT_EQ(uint32_t(0x0F0F0F0Fu), drv.read(reset_unit_cfg::SS_CRITICAL_HOLD));
        // SS_FORCE_TO_REF_CLK: read back (was written in §4).
        drv.write(reset_unit_cfg::SS_FORCE_TO_REF_CLK, 0x00000003u);
        EXPECT_EQ(uint32_t(0x00000003u), drv.read(reset_unit_cfg::SS_FORCE_TO_REF_CLK));
        // SYNC_REG read-back path.
        drv.write(reset_unit_cfg::SYNC_REG, 0x1u);
        EXPECT_EQ(uint32_t(0x1u), drv.read(reset_unit_cfg::SYNC_REG));
        drv.write(reset_unit_cfg::SYNC_REG, 0x0u);
        // ISOLATE_REQ_PINEN_REG / ISOLATE_REQ_SMCEN_REG read-back.
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0x00000055u);
        EXPECT_EQ(uint32_t(0x00000055u), drv.read(reset_unit_cfg::ISOLATE_REQ_PINEN_REG));
        drv.write(reset_unit_cfg::ISOLATE_REQ_SMCEN_REG, 0x000000AAu);
        EXPECT_EQ(uint32_t(0x000000AAu), drv.read(reset_unit_cfg::ISOLATE_REQ_SMCEN_REG));
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0);
        drv.write(reset_unit_cfg::ISOLATE_REQ_SMCEN_REG, 0);
        // ISOLATE_REQ_FLR_RESET_COUNTER_VALUE read-back.
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE, 9);
        EXPECT_EQ(uint32_t(9), drv.read(reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE));
        // SS_RESET_COMPLETE: HW-driven, reflects the input port.
        ss_reset_complete.write(0x000000A5u);
        settle();
        EXPECT_EQ(uint32_t(0x000000A5u), drv.read(reset_unit_cfg::SS_RESET_COMPLETE));
        std::cout << "  [PASS] register read/write sweep\n";

        // ------------------------------------------------------------------
        // 19. SS_COLD_RESET_LOCK (woset) write-protects SS_COLD_RESET_N;
        //     read-only registers ignore writes (no-op write paths).
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::SS_COLD_RESET_N, 0x00000000u);
        drv.write(reset_unit_cfg::SS_COLD_RESET_LOCK, 0x0000000Fu); // woset low nibble
        EXPECT_EQ(uint32_t(0x0000000Fu), drv.read(reset_unit_cfg::SS_COLD_RESET_LOCK));
        drv.write(reset_unit_cfg::SS_COLD_RESET_N, 0xFFFFFFFFu);    // [3:0] protected
        EXPECT_EQ(uint32_t(0xFFFFFFF0u), drv.read(reset_unit_cfg::SS_COLD_RESET_N));
        // Read-only registers: writes are silently ignored.
        drv.write(reset_unit_cfg::SS_RESET_COMPLETE, 0xFFFFFFFFu);
        EXPECT_EQ(uint32_t(0x000000A5u), drv.read(reset_unit_cfg::SS_RESET_COMPLETE));
        drv.write(reset_unit_cfg::STRAPS_LO, 0xDEADBEEFu);
        EXPECT_EQ(uint32_t(0x12345678u), drv.read(reset_unit_cfg::STRAPS_LO));
        drv.write(reset_unit_cfg::STRAPS_HI, 0xDEADBEEFu);
        EXPECT_EQ(uint32_t(0xCAFEF00Du), drv.read(reset_unit_cfg::STRAPS_HI));
        drv.write(reset_unit_cfg::ISOLATE_REQ_VIS, 0xFFFFFFFFu); // RO no-op
        std::cout << "  [PASS] SS_COLD_RESET_LOCK woset + read-only write no-ops\n";

        // ------------------------------------------------------------------
        // 20. JTAG override plane: fuse / cool / core / per-subsystem cold.
        //     Exercises the override ternaries in derive() / output_method.
        // ------------------------------------------------------------------
        {
            smc::jtag_reset_ctrl jall;
            jall.fuse_reset_n_ovrd = true; jall.fuse_reset_n_val = false; // force fuse
            jall.cool_reset_n_ovrd = true; jall.cool_reset_n_val = false; // force cool
            jall.core_reset_n_ovrd = true; jall.core_reset_n_val = false; // force core
            jall.ss_cold_reset_n_ovrd = 0x0000000Fu;
            jall.ss_cold_reset_n_val  = 0x00000005u; // SS0,SS2 high; SS1,SS3 low
            dut.set_jtag_ctrl(jall);
            settle();
            EXPECT_TRUE(dut.jtag_ctrl().cool_reset_n_ovrd); // back-door accessor
            EXPECT_EQ(uint32_t(0x0000000Fu), dut.jtag_ctrl().ss_cold_reset_n_ovrd);
            EXPECT_TRUE(!core_smc.read());            // core forced asserted
            EXPECT_TRUE(!cool_no.read());             // cool forced asserted (jtag)
            EXPECT_TRUE(!primary_smc.read());         // cool override gates primary
            EXPECT_TRUE(dut.dbg_ss_reset_ctrl(0).cold_reset_n);   // bit0 of 0x5
            EXPECT_TRUE(!dut.dbg_ss_reset_ctrl(1).cold_reset_n);  // bit1 forced low
            EXPECT_TRUE(dut.dbg_ss_reset_ctrl(2).cold_reset_n);   // bit2 of 0x5
            // ISOLATE_REQ_VIS cool_n_out (bit8) reflects the forced-low cool reset.
            EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::ISOLATE_REQ_VIS) & (1u << 8));
            dut.set_jtag_ctrl(smc::jtag_reset_ctrl{}); // clear overrides
            settle();
            EXPECT_TRUE(cool_no.read());
        }
        std::cout << "  [PASS] JTAG fuse/cool/core/ss-cold override plane\n";

        // ------------------------------------------------------------------
        // 21. clear_cold_regs with the isolate pin HIGH: PINEN reset is
        //     qualified, so ISOLATE_REQ_PINEN_REG is retained across cold reset.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0x000000FFu);
        drv.write(reset_unit_cfg::ISOLATE_REQ_REG,       0x0000AB00u);
        isolate_pin.write(true);
        settle();
        rst_cold_n.write(false);   // cold reset asserted while isolate pin high
        settle();
        rst_cold_n.write(true);
        isolate_pin.write(false);
        settle();
        // PINEN retained (pin was high); other cold-domain regs cleared.
        EXPECT_EQ(uint32_t(0x000000FFu), drv.read(reset_unit_cfg::ISOLATE_REQ_PINEN_REG));
        EXPECT_EQ(uint32_t(0), drv.read(reset_unit_cfg::ISOLATE_REQ_REG));
        drv.write(reset_unit_cfg::ISOLATE_REQ_PINEN_REG, 0);
        settle();
        std::cout << "  [PASS] cold reset retains PINEN when isolate pin high\n";

        // ------------------------------------------------------------------
        // 22. FLR with reset-duration counter == 0: flr_kick returns early,
        //     no rst_cool_no pulse is generated.
        // ------------------------------------------------------------------
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE, 0);
        drv.write(reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE, 3);
        settle();
        EXPECT_TRUE(cool_no.read());
        cfg_flr.write(true);       // rising edge → flr_kick (early return, no pulse)
        settle();
        EXPECT_EQ(uint32_t(1), drv.read(reset_unit_cfg::ISOLATE_REQ_SMC_REG) & 1u);
        sc_core::wait(60, SC_NS);
        EXPECT_TRUE(cool_no.read()); // still de-asserted: no pulse was scheduled
        cfg_flr.write(false);
        drv.write(reset_unit_cfg::ISOLATE_REQ_SMC_REG, 0x1u); // clear latch
        settle();
        std::cout << "  [PASS] FLR with reset-counter 0 suppresses cool pulse\n";

        // ------------------------------------------------------------------
        // 23. reset_ctrl_t stream + sc_trace overloads (header coverage).
        // ------------------------------------------------------------------
        {
            std::ostringstream os;
            os << dut.dbg_ss_reset_ctrl(2);
            EXPECT_TRUE(os.str().find("cold_n=") != std::string::npos);

            auto* tf = sc_core::sc_create_vcd_trace_file("/tmp/reset_unit_cov_trace");
            smc::reset_ctrl_t rc = dut.dbg_ss_reset_ctrl(2);
            smc::sc_trace(tf, rc, "rc");
            sc_core::sc_close_vcd_trace_file(tf);
        }
        std::cout << "  [PASS] reset_ctrl_t operator<< + sc_trace\n";

        // ------------------------------------------------------------------
        // 24. Back-door API bounds guards (out-of-window / out-of-range).
        // ------------------------------------------------------------------
        EXPECT_EQ(uint32_t(0), dut.dbg_read(reset_unit_cfg::WINDOW_SIZE));      // off >= window ⇒ RAZ
        EXPECT_EQ(uint32_t(0), dut.dbg_read(reset_unit_cfg::WINDOW_SIZE + 0x40));
        {
            // Out-of-range subsystem index returns a default-constructed bundle.
            const smc::reset_ctrl_t oob = dut.dbg_ss_reset_ctrl(dut.num_subsystems());
            EXPECT_TRUE(oob.warm_reset_n);      // default reset value
            EXPECT_TRUE(!oob.cold_reset_n);
        }
        std::cout << "  [PASS] back-door bounds guards\n";

        if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
        else                 std::cout << "\n" << g_failures << " FAILURE(S)\n";
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY);

    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");

    cci::cci_register_broker(cci_global_broker);
    cci::cci_originator origin("platform_cfg");
    auto broker = cci::cci_get_global_broker(origin);
    broker.set_preset_cci_value("tb.reset_unit.ref_clk_period_ns", cci::cci_value(10.0));
    broker.set_preset_cci_value("tb.reset_unit.access_delay_ns",   cci::cci_value(2.0));

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
