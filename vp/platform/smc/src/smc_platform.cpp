// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smc/src/smc_platform.cpp
// ===========================================================================

#include "smc_platform.hpp"

#include <systemc>
#include <tlm.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace smc {

// ---------------------------------------------------------------------------
// Address-map constants (absolute local addresses; Phase-1 RTL realignment).
// ---------------------------------------------------------------------------
static constexpr uint64_t A_WDT_DEBUG   = 0xC000'0000ULL;
static constexpr uint64_t A_CPU_CTRL_FP  = 0xC003'9000ULL;  // cluster.ctrl (cpu_ctrl.rdl)
static constexpr uint64_t A_BOOTROM      = 0xC004'0000ULL;
static constexpr uint64_t A_SCRATCH      = 0xC006'0000ULL;
// Cluster-internal interrupt/error blocks (smc_addrmap_pkg SMC_TOP_SMC_CLUSTER_*).
// These sit above the fabric's 16 MB local-alias window, so the harts reach them
// straight off cluster.data rather than through fabric.mmio_in -- see the
// cluster.data bind below.
static constexpr uint64_t A_PLIC         = 0xC400'0000ULL;
static constexpr uint64_t A_CLINT        = 0xC800'0000ULL;
static constexpr uint64_t A_BEU          = 0xC801'0000ULL;
static constexpr uint64_t A_DMA          = 0xC003'8000ULL;  // dma_cfg default base_addr

// Phase-1 RTL-aligned peripheral bases (tt-oca-harness smc.rdl / smc_top).
static constexpr uint64_t A_RESET        = 0xC000'2000ULL;
static constexpr uint64_t A_MISC_WRAP    = 0xC000'2800ULL;  // named stub
static constexpr uint64_t A_GPIO_INTF    = 0xC000'3000ULL;  // named stub (was pll)
static constexpr uint64_t A_AVSBUS       = 0xC000'4000ULL;
static constexpr uint64_t A_I2C0         = 0xC000'5000ULL;  // RTL smc_i2c_wrap
static constexpr uint64_t A_I2C_CTRL     = A_I2C0 + 0xE00ULL; // i2c_ctrl.rdl
// uart_wrap @ 0xC000_6000; 16550 at +0x100, wrap CSRs at +0x0 / +0x200.
static constexpr uint64_t A_UART_WRAP    = 0xC000'6000ULL;
static constexpr uint64_t A_UART0_16550  = 0xC000'6100ULL;
static constexpr uint64_t A_UART_STRIDE  = 0x400ULL;
static constexpr uint64_t A_UART_WIN     = 0x100ULL;        // uart::WINDOW_SIZE
static constexpr uint64_t A_STRAPS       = 0xC040'3000ULL;  // straps.rdl
static constexpr uint64_t A_EFUSE_MAP    = 0xC000'7000ULL;  // named stub
static constexpr uint64_t A_EFUSE_CTRL   = 0xC000'8000ULL;  // named stub
static constexpr uint64_t A_TELEMETRY    = 0xC000'9000ULL;  // RTL telemetry_receiver_wrap
static constexpr uint64_t A_SYSTEM_TIMER_OCTS = 0xC000'A000ULL;
static constexpr uint64_t A_DTP_CTRL     = 0xC000'B000ULL;  // named stub
// AOU CSRs: D1=A VP-only park (not in smc_top); window 0x80.
static constexpr uint64_t A_AOU          = 0xC000'C000ULL;
static constexpr uint64_t A_I3C          = 0xC003'A000ULL;  // RTL oca_i3c_wrap_0
static constexpr uint64_t A_PLL_WRAP     = 0xC040'2000ULL;  // smc_external + 0x2000
static constexpr uint64_t A_PVT_WRAP     = 0xC040'5C00ULL;  // smc_external + 0x5C00
static constexpr uint64_t A_PERIPH_MAIN_LO = 0xC000'2000ULL;
static constexpr uint64_t A_PERIPH_MAIN_HI = 0xC000'E800ULL;  // Phase 2 → 0xB800
static constexpr uint64_t A_PERIPH_EXT_LO  = 0xC040'0000ULL;
static constexpr uint64_t A_PERIPH_EXT_HI  = 0xC080'0000ULL;
static constexpr uint64_t A_NDM_RESET      = 0xC000'2A00ULL;
static constexpr uint64_t A_DFX_CTRL       = 0xC000'B800ULL;

// Stub identity tokens at offset 0 (map-coherence / decode checks).
static constexpr uint32_t STUB_MAGIC_GPIO      = 0x4750494Fu;
static constexpr uint32_t STUB_MAGIC_MISC      = 0x4D495343u;
static constexpr uint32_t STUB_MAGIC_EFUSE_MAP = 0xEF05E000u;
static constexpr uint32_t STUB_MAGIC_EFUSE_CTL = 0xEF05E001u;
static constexpr uint32_t STUB_MAGIC_DTP       = 0x44545000u;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
smc_platform::smc_platform(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    // Test-only BEU error-injection hook (Phase D1); see the member
    // declarations in smc_platform.hpp for the full rationale.
    , beu_inject0_enable_p_("beu_inject0_enable", false,
          "Test-only: if true, inject BEU error #0 at elaboration.")
    , beu_inject0_core_p_("beu_inject0_core", 0u,
          "Target BEU instance (0..NUM_BEU-1) for injection #0.")
    , beu_inject0_src_p_("beu_inject0_src", 7u,
          "beu_src enum value (bit position) for injection #0; default 7 = "
          "DCACHE_UNCORRECTABLE.")
    , beu_inject0_addr_p_("beu_inject0_addr", uint64_t{0},
          "Physical address recorded by injection #0 (masked to 56 bits).")
    , beu_inject1_enable_p_("beu_inject1_enable", false,
          "Test-only: if true, inject BEU error #1 at elaboration.")
    , beu_inject1_core_p_("beu_inject1_core", 0u,
          "Target BEU instance (0..NUM_BEU-1) for injection #1.")
    , beu_inject1_src_p_("beu_inject1_src", 5u,
          "beu_src enum value (bit position) for injection #1; default 5 = "
          "DCACHE_TLBUS.")
    , beu_inject1_addr_p_("beu_inject1_addr", uint64_t{0},
          "Physical address recorded by injection #1 (masked to 56 bits).")
    , beu_inject2_enable_p_("beu_inject2_enable", false,
          "Test-only: if true, inject BEU error #2 at elaboration.")
    , beu_inject2_core_p_("beu_inject2_core", 1u,
          "Target BEU instance (0..NUM_BEU-1) for injection #2.")
    , beu_inject2_src_p_("beu_inject2_src", 1u,
          "beu_src enum value (bit position) for injection #2; default 1 = "
          "ICACHE_TLBUS.")
    , beu_inject2_addr_p_("beu_inject2_addr", uint64_t{0},
          "Physical address recorded by injection #2 (masked to 56 bits).")
    , octs_clk_period_ns_p_("octs_clk_period_ns", 10.0,
          "Period of the clock driving octs_system_timer, in ns (10 ns = "
          "100 MHz). One timer tick per rising edge.")
    , octs_is_primary_p_("octs_is_primary", true,
          "Strap for octs_system_timer.is_primary_i: true = SMC is the system "
          "timekeeping PRIMARY (silicon default), false = SECONDARY.")
    , captured_straps_p_("captured_straps", uint64_t{0},
          "Captured GPIO strap word driven into reset_unit and the straps "
          "addrmap at smc_external + 0x3000.")
    , tel_inject_enable_p_("tel_inject_enable", false,
          "Test-only: if true, push one ATB message into a telemetry receiver "
          "at elaboration.")
    , tel_inject_inst_p_("tel_inject_inst", 0u,
          "Target telemetry_receiver instance (0..NUM_TELEMETRY-1).")
    , tel_inject_probe_p_("tel_inject_probe", 0x15u,
          "Probe ID encoded into the injected ATB message (low 5 bits).")
    , tel_inject_counter0_p_("tel_inject_counter0", uint32_t{0xDEADBEEFu},
          "Counter[0] value encoded into the injected ATB message.")
    , intagg("intagg", NUM_PERIPH_IRQ, NUM_PLIC_SRC,
             std::vector<unsigned>{
                 // SEP mailbox[0..7] -> peripheral bits 7:0 -> PLIC source IDs
                 // 257..264, matching SEP_MAILBOX_n_INTERRUPT_ID in the SMC
                 // firmware's tt_smc_interrupts.h.
                 NUM_EXT_INTERRUPTS + 0, NUM_EXT_INTERRUPTS + 1,
                 NUM_EXT_INTERRUPTS + 2, NUM_EXT_INTERRUPTS + 3,
                 NUM_EXT_INTERRUPTS + 4, NUM_EXT_INTERRUPTS + 5,
                 NUM_EXT_INTERRUPTS + 6, NUM_EXT_INTERRUPTS + 7,
                 // telemetry[0..2] -> peripheral bits 10:8
                 NUM_EXT_INTERRUPTS + 8, NUM_EXT_INTERRUPTS + 9,
                 NUM_EXT_INTERRUPTS + 10,
                 // i3c[0..5] -> peripheral bits 17:12
                 NUM_EXT_INTERRUPTS + 12, NUM_EXT_INTERRUPTS + 13,
                 NUM_EXT_INTERRUPTS + 14, NUM_EXT_INTERRUPTS + 15,
                 NUM_EXT_INTERRUPTS + 16, NUM_EXT_INTERRUPTS + 17,
                 // uart[0..3] -> peripheral bits 21:18
                 NUM_EXT_INTERRUPTS + 18, NUM_EXT_INTERRUPTS + 19,
                 NUM_EXT_INTERRUPTS + 20, NUM_EXT_INTERRUPTS + 21,
                 // avsbus -> peripheral bit 22
                 NUM_EXT_INTERRUPTS + 22,
                 // i2c[0..2] -> peripheral bits 25:23
                 NUM_EXT_INTERRUPTS + 23, NUM_EXT_INTERRUPTS + 24,
                 NUM_EXT_INTERRUPTS + 25,
                 // combined AXI hang detector -> PLIC source ID 287 (bit 286)
                 286,
                 // GPIO half-ORs -> peripheral bits 28 and 29
                 NUM_EXT_INTERRUPTS + 28, NUM_EXT_INTERRUPTS + 29,
                 // wdt[0..3] -> PLIC source IDs 329..332 (bits 328..331)
                 328, 329, 330, 331})
    , octs_clk("octs_clk",
               sc_core::sc_time(octs_clk_period_ns_p_.get_value(),
                                sc_core::SC_NS))
{
    // -- Drive input signals to safe, post-reset defaults -------------------
    rst_n_sig.write(true);
    sig_powergood.write(true);
    sig_fuse_reset.write(true);
    sig_rst_ext_wdt.write(true);
    sig_wdt_first.write(false);
    sig_wdt_second.write(false);
    sig_rst_cool.write(true);
    sig_isolate.write(false);
    sig_flr.write(false);
    sig_ss_complete.write(0xFFFFFFFFu);
    sig_straps.write(captured_straps_p_.get_value());

    // As a PRIMARY (the default strap) the OCTS timer sources sync_load /
    // cnt_credit instead of consuming them, so its inputs stay idle.  Strapped
    // SECONDARY it has no pulse source in this platform and stays idle too.
    sig_octs_is_primary.write(octs_is_primary_p_.get_value());
    sig_octs_sync_load_in.write(false);
    sig_octs_credit_in.write(false);

    for (unsigned i = 0; i < NUM_UART; ++i) {
        uart_rx[i].write(true);   // idle high
        uart_cts[i].write(false); // asserted (ready)
        uart_dsr[i].write(true);  // tied high (unused modem inputs)
        uart_ri[i].write(true);
        uart_dcd[i].write(true);
    }

    for (unsigned i = 0; i < NUM_HARTS; ++i) {
        wdt_core_rst[i].write(false);          // cores not in reset
        wdt_sticky[i].write(false);
        cpu_ctrl_wdt_sticky[i].write(false);
    }
    cpu_ctrl_wdt_first.write(false);
    cpu_ctrl_wdt_second.write(false);

    // -- External inbound targets -> fabric --------------------------------
    fwd_sys_ .bind(fabric.sys_axi_in);
    fwd_jtag_.bind(fabric.jtag_axi_in);
    fwd_sep_ .bind(fabric.sep_axi_in);
    fwd_aou_ .bind(aou_.axi_s[0]);
    sys_axi_in .register_b_transport(this, &smc_platform::fwd_sys_axi);
    jtag_axi_in.register_b_transport(this, &smc_platform::fwd_jtag_axi);
    sep_axi_in .register_b_transport(this, &smc_platform::fwd_sep_axi);
    aou_axi_s  .register_b_transport(this, &smc_platform::fwd_aou_axi);

    // -- Fabric reset -------------------------------------------------------
    fabric.rst_n_i.bind(rst_n_sig);
    fabric.axi_hang_irq_o.bind(axi_hang_irq);

    // Idle initiator satisfies the fabric's log_in BW port (nothing drives
    // log_in in this platform).
    //
    // fabric.data_accel_in is a tlm_utils::multi_passthrough_target_socket
    // (multi-bind) specifically because two independent masters both write
    // results back into local memory through it: smc_dma's own data-mover
    // (dma_.mst_socket, bound here) and the memory_zeroer's DMA master
    // (wa_zeroer_dma.init, bound in the peripheral section below). See
    // smc_fabric.h's data_accel_in declaration for the multi-bind rationale.
    idle_log_init_ .bind(fabric.log_in);
    dma_.mst_socket.bind(fabric.data_accel_in);

    // -- front_port_router: fabric.to_front_port (cluster.data is routed to a
    //    dedicated multi_stub_target below, not through the shared router, so
    //    the router target only ever takes the one fabric initiator). --------
    fabric.to_front_port.bind(front_port_router.tgt);
    front_port_router.add_route(0, A_WDT_DEBUG,   0x1000,   "wdt");
    front_port_router.add_route(1, A_CPU_CTRL_FP, 0x1000,   "cpu_ctrl_fp");
    front_port_router.add_route(2, A_BOOTROM,     0x20000,  "bootrom");
    front_port_router.add_route(3, A_SCRATCH,     0x100000, "scratch");
    front_port_router.add_route(4, A_PLIC,        0x400000, "plic");
    front_port_router.add_route(5, A_CLINT,       0x20000,  "clint");
    front_port_router.add_route(6, A_BEU,         0x10000,  "beu");

    // Nested demux: fabric subtracts 0xC000_0000, then each 1 KiB window
    // maps to one SiFive TLWDT instance (address relative to instance base).
    front_port_router.out[0].bind(wdt_demux.tgt);
    for (unsigned i = 0; i < NUM_HARTS; ++i) {
        wdt_demux.add_route(i, static_cast<uint64_t>(i) * 0x400ULL, 0x400ULL,
                            std::string("wdt") + std::to_string(i));
        wdt_demux.out[i].bind(wdt_[i].reg_socket);
    }

    // The three multi_stub_target placeholders are each always bound by their
    // own idle initiator (so they are never unbound).  In cluster mode the
    // cluster-only initiators add a second bind:
    //   out[1]      -> cluster.ctrl (cluster) / stub_cluster_ctrl (no-cluster)
    //   cluster.ifetch -> stub_ifetch   (cluster only)
    idle_ctrl_init_  .bind(stub_cluster_ctrl.reg_socket);
    idle_ifetch_init_.bind(stub_ifetch.reg_socket);
    idle_data_init_   .bind(stub_data.reg_socket);
#ifdef SMC_PLATFORM_WITH_CLUSTER
    front_port_router.out[1].bind(cluster.ctrl);
    cluster.ifetch.bind(stub_ifetch.reg_socket);
    // cluster.data carries everything outside the [mmio_lo, mmio_hi) carve-out,
    // which after the Phase-3 move is exactly the cluster-internal PLIC / CLINT
    // / BEU block.  In RTL those sit on the cluster's own periphery bus and the
    // harts never route to them through smc_input_fabric, so bind straight to
    // the front-port router instead of back through fabric.mmio_in (whose 16 MB
    // local-alias demux would push 0xC400_0000 / 0xC800_0000 outbound).
    cluster.data   .bind(front_port_router.tgt);
#else
    front_port_router.out[1].bind(stub_cluster_ctrl.reg_socket);
#endif
    front_port_router.out[2].bind(wa_bootrom.tgt);
    wa_bootrom.init.bind(bootrom_.reg_socket);
    front_port_router.out[3].bind(wa_scratch.tgt);
    wa_scratch.init.bind(scratch.reg_socket);
    front_port_router.out[4].bind(wa_plic.tgt);
    wa_plic.init.bind(plic_.reg_socket);
    front_port_router.out[5].bind(wa_clint.tgt);
    wa_clint.init.bind(clint_.reg_socket);
    // BEU: front_port out[6] -> beu_router -> beu[N] (4 KiB each).
    // front_port_router already rebases A_BEU away, so beu_router sees
    // offsets in [0, 0x10000) and further rebases each 4 KiB window to 0.
    front_port_router.out[6].bind(beu_router.tgt);
    for (unsigned n = 0; n < NUM_BEU; ++n) {
        beu_router.add_route(n, n * 0x1000ULL, 0x1000,
                             std::string("beu") + std::to_string(n));
        beu_router.out[n].bind(beu_[n].reg_socket);
        beu_[n].rst_n_i.bind(rst_n_sig);
        beu_[n].irq_local_o.bind(beu_irq_local[n]);
        beu_[n].irq_plic_o.bind(beu_irq_plic[n]);
    }

    // -- Test-only BEU error injection (Phase D1) --------------------------
    // Runs at elaboration (before sc_start()), so any enabled injection is
    // already latched into ACCRUED/CAUSE/PHYS_ADDR by the time firmware's
    // first register read happens.  See the CCI param declarations in
    // smc_platform.hpp for the full rationale; every slot is disabled
    // (enable=false) unless a test's .ini opts in.
    auto inject_beu_test_error = [this](bool enable, unsigned core,
                                        unsigned src, uint64_t addr) {
        if (!enable) return;
        if (core >= NUM_BEU) {
            SIM_LOG_WARN(this, "beu test-inject: core index " << core
                              << " out of range (NUM_BEU=" << NUM_BEU
                              << "); injection ignored");
            return;
        }
        SIM_LOG_INFO(this, "beu test-inject: core=" << core
                          << " src=" << src
                          << " addr=0x" << std::hex << addr << std::dec);
        beu_[core].inject_error(static_cast<beu_src>(src), addr);
    };
    inject_beu_test_error(beu_inject0_enable_p_.get_value(),
                          beu_inject0_core_p_.get_value(),
                          beu_inject0_src_p_.get_value(),
                          beu_inject0_addr_p_.get_value());
    inject_beu_test_error(beu_inject1_enable_p_.get_value(),
                          beu_inject1_core_p_.get_value(),
                          beu_inject1_src_p_.get_value(),
                          beu_inject1_addr_p_.get_value());
    inject_beu_test_error(beu_inject2_enable_p_.get_value(),
                          beu_inject2_core_p_.get_value(),
                          beu_inject2_src_p_.get_value(),
                          beu_inject2_addr_p_.get_value());

    // -- periph_router: fabric.to_periph (Phase-1 RTL-aligned map) ---------
    fabric.to_periph.bind(periph_router.tgt);
    periph_router.add_route(0, A_RESET,     0x200,  "reset");
    periph_router.add_route(1, A_I2C0,      0x200,  "i2c0");
    periph_router.add_route(2, A_I2C0 + 0x200,  0x200, "i2c1");
    periph_router.add_route(3, A_I2C0 + 0x400, 0x200, "i2c2");
    periph_router.add_route(21, A_I2C_CTRL, 0x10, "i2c_ctrl");
    periph_router.add_route(4, A_TELEMETRY, 0x300,  "telemetry");
    // 16550 at +0x100 is the narrower window and wins over the 0x400 wrap.
    for (unsigned u = 0; u < NUM_UART; ++u) {
        periph_router.add_route(5 + u, A_UART0_16550 + u * A_UART_STRIDE,
                                A_UART_WIN,
                                std::string("uart") + std::to_string(u));
        periph_router.add_route(22 + u, A_UART_WRAP + u * A_UART_STRIDE,
                                A_UART_STRIDE,
                                std::string("uart_wrap") + std::to_string(u));
    }
    // Named stubs for unmodeled RTL slots (map-coherence identity tokens).
    periph_router.add_route(9,  A_GPIO_INTF,  0x1000, "gpio_intf_stub");
    // i3c_controller packs NUM_I3C instances at INSTANCE_SPACING=0x1000.
    periph_router.add_route(10, A_I3C, 0x6000, "i3c");
    periph_router.add_route(26, A_STRAPS, 0x8, "straps");
    periph_router.add_route(11, A_PVT_WRAP, 0x1000, "pvt_wrap");
    periph_router.add_route(12, A_PLL_WRAP, 0x1000, "pll_wrap");
    periph_router.add_route(13, A_AVSBUS,   0x1000, "avsbus");
    // AOU CSRs: D1=A VP-only park at 0xC000_C000 (window 0x80).
    periph_router.add_route(14, A_AOU,      0x80,   "aou");
    periph_router.add_route(15, A_SYSTEM_TIMER_OCTS, 0x1000, "octs_system_timer");
    periph_router.add_route(27, A_NDM_RESET, 0x0C, "ndm_reset_stub");
    periph_router.add_route(17, A_MISC_WRAP,  0x800,  "misc_wrap_stub");
    periph_router.add_route(18, A_EFUSE_MAP,  0x1000, "efuse_map_stub");
    periph_router.add_route(19, A_EFUSE_CTRL, 0x1000, "efuse_ctrl_stub");
    periph_router.add_route(20, A_DTP_CTRL,   0x800,  "dtp_ctrl_stub");
    periph_router.add_route(28, A_DFX_CTRL,   0x18,   "dfx_ctrl_stub");
    // Catch-alls (largest windows, checked last) -> periph_misc stub.
    periph_router.add_route(16, A_PERIPH_MAIN_LO, A_PERIPH_MAIN_HI - A_PERIPH_MAIN_LO, "periph_main_misc");
    periph_router.add_route(16, A_PERIPH_EXT_LO,  A_PERIPH_EXT_HI  - A_PERIPH_EXT_LO,  "periph_ext_misc");
    periph_router.out[0].bind(reset.reg_socket);
    periph_router.out[1].bind(i2c[0].reg_socket);
    periph_router.out[2].bind(i2c[1].reg_socket);
    periph_router.out[3].bind(i2c[2].reg_socket);
    periph_router.out[21].bind(i2c_ctrl_.reg_socket);
    periph_router.out[4].bind(telemetry_router.tgt);
    for (unsigned n = 0; n < NUM_TELEMETRY; ++n) {
        telemetry_router.add_route(n, n * 0x100ULL, 0x100,
                                   std::string("tel") + std::to_string(n));
        telemetry_router.out[n].bind(telemetry_[n].reg_socket);
        telemetry_[n].rst_n_i.bind(rst_n_sig);
        telemetry_[n].afready_i.bind(telemetry_afready[n]);
        telemetry_[n].irq_o.bind(telemetry_irq[n]);
        telemetry_[n].afvalid_o.bind(telemetry_afvalid[n]);
        telemetry_[n].atready_o.bind(telemetry_atready[n]);
        telemetry_[n].atvalid_i.bind(telemetry_atvalid[n]);
        telemetry_[n].atdata_i.bind(telemetry_atdata[n]);
        telemetry_[n].debug_o.bind(telemetry_debug[n]);
        telemetry_afready[n].write(false);
        telemetry_atvalid[n].write(false);
        telemetry_atdata[n].write(0);
    }
    periph_router.out[5].bind(uart_[0].reg_socket);
    periph_router.out[6].bind(uart_[1].reg_socket);
    periph_router.out[7].bind(uart_[2].reg_socket);
    periph_router.out[8].bind(uart_[3].reg_socket);
    periph_router.out[22].bind(uart_wrap_[0].reg_socket);
    periph_router.out[23].bind(uart_wrap_[1].reg_socket);
    periph_router.out[24].bind(uart_wrap_[2].reg_socket);
    periph_router.out[25].bind(uart_wrap_[3].reg_socket);
    periph_router.out[26].bind(straps_.reg_socket);
    // Dual periph cpu_ctrl @ 0xC040_0000 dropped; live path is cluster.ctrl
    // @ A_CPU_CTRL_FP. Keep the modeled IP elaboratable via idle initiator.
    idle_cpu_ctrl_init_.bind(cpu_ctrl_.reg_socket);
    periph_router.out[9].bind(stub_gpio_intf.reg_socket);
    stub_gpio_intf.set_reset_value(0, STUB_MAGIC_GPIO);
    periph_router.out[10].bind(i3c.reg_socket);
    periph_router.out[11].bind(pvt_wrap_.reg_socket);
    periph_router.out[12].bind(pll_wrap.reg_socket);
    pll_wrap.rst_n_i.bind(rst_n_sig);
    periph_router.out[13].bind(avsbus.reg_socket);
    periph_router.out[14].bind(aou_.apb_socket);
    periph_router.out[15].bind(octs_timer.reg_socket);
    periph_router.out[16].bind(stub_periph_misc.reg_socket);
    periph_router.out[17].bind(stub_misc_wrap.reg_socket);
    stub_misc_wrap.set_reset_value(0, STUB_MAGIC_MISC);
    periph_router.out[27].bind(stub_ndm_reset.reg_socket);
    periph_router.out[18].bind(stub_efuse_map.reg_socket);
    stub_efuse_map.set_reset_value(0, STUB_MAGIC_EFUSE_MAP);
    periph_router.out[19].bind(stub_efuse_ctrl.reg_socket);
    stub_efuse_ctrl.set_reset_value(0, STUB_MAGIC_EFUSE_CTL);
    periph_router.out[20].bind(stub_dtp_ctrl.reg_socket);
    stub_dtp_ctrl.set_reset_value(0, STUB_MAGIC_DTP);
    periph_router.out[28].bind(stub_dfx_ctrl.reg_socket);
    stub_dfx_ctrl.set_reset_value(0, 0x00000113u);

    // AOU: CSRs on the SMC periph bus; AXI hop is the D2D data path
    // (tt-oca-hw: AoU on SMU smu_axi_in/out == xbar ext_in/ext_out).
    // Local axi_s/axi_m are re-exported at the platform boundary
    // (aou_axi_s / aou_axi_m); smc-vp idle/stubs them, smu-vp binds the
    // xbar chiplet ports.  Peer models the remote die: axi_s idle, axi_m
    // on stub_sysmem (remote SMN).  aou_irq -> intagg -> PLIC (local core
    // only — the peer stub's irq_o has no local-firmware-visible sink).
    aou_.fdi_active_i.bind(aou_fdi_active);
    aou_peer_.fdi_active_i.bind(aou_peer_fdi_active);
    aou_.irq_o.bind(aou_irq);
    aou_peer_.irq_o.bind(aou_peer_irq);
    aou_fdi_active.write(true);
    aou_peer_fdi_active.write(true);
    aou_.connect_peer(&aou_peer_);
    aou_peer_.connect_peer(&aou_);
    aou_.axi_m[0].bind(aou_axi_m);
    idle_aou_peer_init_.bind(aou_peer_.axi_s[0]);
    aou_peer_.axi_m[0].bind(stub_sysmem.reg_socket);
    idle_aou_peer_apb_init_.bind(aou_peer_.apb_socket);

    // -- octs_system_timer signals -----------------------------------------
    octs_timer.clk_i              .bind(octs_clk);
    octs_timer.rst_n_i            .bind(rst_n_sig);
    octs_timer.is_primary_i       .bind(sig_octs_is_primary);
    octs_timer.timer_sync_load_i  .bind(sig_octs_sync_load_in);
    octs_timer.timer_cnt_credit_i .bind(sig_octs_credit_in);
    octs_timer.timer_sync_load_o  .bind(sig_octs_sync_load_out);
    octs_timer.timer_cnt_credit_o .bind(sig_octs_credit_out);
    octs_timer.timer_count_o      .bind(sig_octs_count);
    octs_timer.timer_gpio_enable_o.bind(sig_octs_gpio_enable);
    octs_timer.cur_credits_debug_o.bind(sig_octs_cur_credits);
    octs_timer.credits_left_debug_o.bind(sig_octs_credits_left);

    // -- Test-only telemetry ATB inject ------------------------------------
    if (tel_inject_enable_p_.get_value()) {
        const unsigned inst = tel_inject_inst_p_.get_value();
        if (inst >= NUM_TELEMETRY) {
            SIM_LOG_WARN(this, "tel test-inject: inst " << inst
                              << " out of range (NUM_TELEMETRY=" << NUM_TELEMETRY
                              << "); injection ignored");
        } else {
            const uint8_t  probe = static_cast<uint8_t>(
                tel_inject_probe_p_.get_value() & 0x1Fu);
            const uint32_t ctr0  = tel_inject_counter0_p_.get_value();
            SIM_LOG_INFO(this, "tel test-inject: inst=" << inst
                              << " probe=0x" << std::hex << unsigned(probe)
                              << " counter0=0x" << ctr0 << std::dec);
            std::vector<telemetry_counter_value> counters{
                {true, ctr0}};
            // Default max_counters_per_message is 4; pad with invalid entries
            // so the encoder fills a full multi-packet message.
            while (counters.size() < 4)
                counters.push_back({false, 0u});
            const auto beats = telemetry_encode_message(probe, counters, 4);
            const unsigned accepted = telemetry_[inst].push_atb_beats(beats);
            if (accepted != beats.size()) {
                SIM_LOG_WARN(this, "tel test-inject: only accepted "
                                  << accepted << " of " << beats.size()
                                  << " beats");
            }
        }
    }

    // -- DMA + memory_zeroer (data-accelerator CSR + DMA paths) ------------
    // CSR path: fabric's 64-bit `to_data_accel_ctrl` initiator is a single-
    // bind socket, but both DMA and memory_zeroer live in the RTL's shared
    // 0xC003_8000..0xC0040000 data-accelerator CSR window, so they fan out
    // through a shared 2-entry addr_router that rebases each absolute window
    // down to the IP's 0-based register offsets (and adapts 64->32).
    fabric.to_data_accel_ctrl.bind(daccel_router.tgt);
    daccel_router.add_route(0, memory_zeroer_cfg::DEFAULT_BASE_ADDR,
                            memory_zeroer_cfg::WINDOW_SIZE, "memory_zeroer");
    daccel_router.out[0].bind(zeroer.reg_socket);
    daccel_router.add_route(1, A_DMA, dma_cfg::WINDOW_SIZE, "dma");
    daccel_router.out[1].bind(dma_.reg_socket);
    // DMA path: both smc_dma and the memory_zeroer DMA master drive the fabric's
    // multi-bind 64-bit `data_accel_in` target with absolute addresses the fabric
    // re-decodes; the width adapter is verbatim (no rebase) because addresses
    // stay absolute until the fabric re-routes them.
    zeroer.dma_socket.bind(wa_zeroer_dma.tgt);
    wa_zeroer_dma.init.bind(fabric.data_accel_in);
    zeroer.rst_n_i.bind(rst_n_sig);
    zeroer.irq_o.bind(sig_zeroer_irq);

    // -- Fabric initiator stubs -------------------------------------------
    fabric.to_dfd_apb              .bind(stub_dfd.reg_socket);
    fabric.to_mailbox              .bind(stub_mbox.reg_socket);
    fabric.to_dft_csr              .bind(stub_dft.reg_socket);
    // Outbound system-NoC traffic is re-exported at the platform boundary
    // (hierarchical initiator bind); smc-vp stubs it, smu-vp binds the xbar.
    fabric.output_axi              .bind(output_axi);
    fabric.to_cpu_ctrl             .bind(stub_cpu_ctrl_fab.reg_socket);
    fabric.to_aR_ctrl              .bind(stub_aR.reg_socket);
    fabric.to_mR_ctrl              .bind(stub_mR.reg_socket);
    fabric.to_xR_ctrl              .bind(stub_xR.reg_socket);
    fabric.to_inbound_filter_ctrl  .bind(stub_ibf.reg_socket);
    fabric.to_outbound_filter_ctrl .bind(stub_obf.reg_socket);

    // -- reset_unit signals ------------------------------------------------
    reset.powergood_i            .bind(sig_powergood);
    reset.rst_cold_ni            .bind(rst_n_sig);
    reset.fuse_reset_ni          .bind(sig_fuse_reset);
    reset.rst_ext_wdt_ni         .bind(sig_rst_ext_wdt);
    reset.smc_wdt_first_timeout_i.bind(sig_wdt_first);
    reset.smc_wdt_second_timeout_i.bind(sig_wdt_second);
    reset.rst_cool_ni            .bind(sig_rst_cool);
    reset.isolate_req_pin_i      .bind(sig_isolate);
    reset.cfg_flr_pf_active_i    .bind(sig_flr);
    reset.ss_reset_complete_i    .bind(sig_ss_complete);
    reset.captured_straps_i      .bind(sig_straps);
    straps_.captured_straps_i    .bind(sig_straps);

    reset.powergood_stable_o           .bind(sig_powergood_stable);
    reset.rst_cold_stable_ref_clk_no    .bind(sig_rst_cold_stable_ref);
    reset.rst_cold_stable_smc_clk_no    .bind(sig_rst_cold_stable_smc);
    reset.rst_primary_ref_clk_no         .bind(sig_rst_primary_ref);
    reset.rst_primary_smc_clk_no         .bind(sig_rst_primary_smc);
    reset.rst_primary_periph_clk_no      .bind(sig_rst_primary_periph);
    reset.rst_core_smc_clk_no             .bind(sig_rst_core_smc);
    reset.rst_wdt_smc_clk_no              .bind(sig_rst_wdt_smc);
    reset.rst_cool_no                     .bind(sig_rst_cool_no);
    reset.skip_mem_repair_o               .bind(sig_skip_mem_repair);
    reset.sync_irq_o                      .bind(sig_sync_irq);
    reset.isolate_req_o                   .bind(sig_isolate_o);
    reset.ss_config_o                     .bind(sig_ss_config_o);
    for (unsigned i = 0; i < NUM_SUBSYS; ++i)
        reset.ss_reset_ctrl_o[i].bind(sig_ss_reset[i]);

    // -- plic / clint / pvt_wrap reset -------------------------------------
    plic_.rst_n_i .bind(rst_n_sig);
    clint_.rst_n_i.bind(rst_n_sig);
    pvt_wrap_.rst_n_i.bind(rst_n_sig);

    // -- pvt_wrap outputs (unused externally, bound to dummy signals) -------
    pvt_wrap_.process_clk_obs_o.bind(pvt_process_clk_obs);
    pvt_wrap_.process_clk_obs_en_o.bind(pvt_process_clk_obs_en);
    pvt_wrap_.voltage_code_o.bind(pvt_voltage_code);
    pvt_wrap_.temp_interrupt_o.bind(pvt_temp_interrupt);

    // -- bootrom / scratchpad reset ---------------------------------------
    bootrom_.rst_n_i.bind(rst_n_sig);
    scratch.rst_n_i  .bind(rst_n_sig);

    // -- UART port binding -------------------------------------------------
    for (unsigned i = 0; i < NUM_UART; ++i) {
        uart_[i].rst_n_i.bind(rst_n_sig);
        uart_[i].rx_i  .bind(uart_rx[i]);
        uart_[i].cts_ni.bind(uart_cts[i]);
        uart_[i].dsr_ni.bind(uart_dsr[i]);
        uart_[i].ri_ni .bind(uart_ri[i]);
        uart_[i].dcd_ni.bind(uart_dcd[i]);
        uart_[i].tx_o   .bind(uart_tx[i]);
        uart_[i].rts_no .bind(uart_rts[i]);
        uart_[i].dtr_no .bind(uart_dtr[i]);
        uart_[i].out1_no.bind(uart_out1[i]);
        uart_[i].out2_no.bind(uart_out2[i]);
        uart_[i].rxrdy_o.bind(uart_rxrdy[i]);
        uart_[i].txrdy_o.bind(uart_txrdy[i]);
        uart_[i].err_o   .bind(uart_err[i]);
        uart_[i].irq_o   .bind(uart_irq[i]);
        uart_wrap_[i].rst_n_i.bind(rst_n_sig);
    }

    // -- I3C port binding --------------------------------------------------
    i3c.rst_n_i.bind(rst_n_sig);
    for (unsigned i = 0; i < NUM_I3C; ++i) {
        i3c.irq_o[i].bind(i3c_irq[i]);
        i3c.scl_o[i].bind(i3c_scl[i]);
        i3c.sda_o[i].bind(i3c_sda[i]);
        i3c.scl_oe_o[i].bind(i3c_scl_oe[i]);
        i3c.sda_oe_o[i].bind(i3c_sda_oe[i]);
        i3c.sel_od_pp_o[i].bind(i3c_sel_od_pp[i]);
        i3c.recovery_payload_available_o[i].bind(i3c_rpa[i]);
        i3c.recovery_image_activated_o[i].bind(i3c_ria[i]);
    }

    // -- I2C reset + IRQ ---------------------------------------------------
    for (unsigned i = 0; i < NUM_I2C; ++i) {
        i2c[i].rst_n_i.bind(rst_n_sig);
        i2c[i].irq_o.bind(i2c_irq[i]);
    }
    i2c_ctrl_.rst_n_i.bind(rst_n_sig);

    // -- AVSBus reset + IRQ + GPIO-enable sink -----------------------------
    avsbus.rst_n_i.bind(rst_n_sig);
    avsbus.irq_o.bind(avsbus_irq);
    avsbus.avs_gpio_enable_o.bind(avsbus_gpio_en);

    // -- WDT (stage-1 SiFive TLWDT) ----------------------------------------
    for (unsigned i = 0; i < NUM_HARTS; ++i) {
        wdt_[i].rst_n_i.bind(rst_n_sig);
        wdt_[i].core_rst_i.bind(wdt_core_rst[i]);
        wdt_[i].irq_o.bind(wdt_irq[i]);
        wdt_[i].rst_sticky_o.bind(wdt_sticky[i]);
    }

    // Standalone periph-bus cpu_ctrl_ stage-2 ports (elaboration only; sticky
    // from front-port WDTs feeds the cluster's embedded cpu_ctrl path).
    for (unsigned i = 0; i < NUM_HARTS; ++i)
        cpu_ctrl_.wdt_timeout_cluster_i[i].bind(cpu_ctrl_wdt_sticky[i]);
    cpu_ctrl_.rst_primary_n_i.bind(rst_n_sig);
    cpu_ctrl_.wdt_first_timeout_o.bind(cpu_ctrl_wdt_first);
    cpu_ctrl_.wdt_second_timeout_o.bind(cpu_ctrl_wdt_second);

    // -- I2C0 controller -> I2C1 target loopback (for smc-i2c-loopback-test)
    //    I2C0's bus model forwards controller segments to I2C1's target back door.
    i2c[0].set_bus_model([this](smc::i2c_xfer& x) {
        if (x.dir == smc::i2c_dir::Write) {
            x.ack = i2c[1].target_write(x.addr, x.write_data);
        } else {
            std::vector<uint8_t> out;
            x.ack = i2c[1].target_read(x.addr, x.read_len, out);
            x.read_data = std::move(out);
        }
    });

    // -- I3C0 controller -> echo target (for smc-i3c-loopback-test)
    //    The bus model ACKs dynamic address 0x50 and echoes the last written
    //    bytes back on a subsequent read.
    {
        auto i3c_echo = std::make_shared<std::vector<uint8_t>>();
        i3c.set_bus_model(0, [i3c_echo](smc::i3c_xfer& x) {
            if (x.dynamic_addr != 0x50u) {
                x.ack = false;
                return;
            }
            x.ack = true;
            x.error = smc::i3c_err::Success;
            if (x.kind == smc::i3c_xfer_kind::PrivateWrite) {
                *i3c_echo = std::move(x.write_data);
            } else if (x.kind == smc::i3c_xfer_kind::PrivateRead) {
                x.read_data.assign(i3c_echo->begin(), i3c_echo->end());
                if (x.read_data.size() > x.data_length)
                    x.read_data.resize(x.data_length);
            }
        });
    }

    // -- Bind every stub_target's irq_o to a dummy sink --------------------
    {
        stub_target<64>* stubs64[] = {
            &stub_dfd, &stub_mbox,
            &stub_dft, &stub_sysmem, &stub_cpu_ctrl_fab, &stub_aR, &stub_mR,
            &stub_xR, &stub_ibf, &stub_obf,
        };
        unsigned s = 0;
        for (auto* st : stubs64) st->irq_o.bind(stub_irq_sig[s++]);
        stub_periph_misc.irq_o.bind(stub_irq_sig[s++]);
        stub_gpio_intf.irq_o.bind(stub_irq_sig[s++]);
        stub_misc_wrap.irq_o.bind(stub_irq_sig[s++]);
        stub_ndm_reset.irq_o.bind(stub_irq_sig[s++]);
        stub_efuse_map.irq_o.bind(stub_irq_sig[s++]);
        stub_efuse_ctrl.irq_o.bind(stub_irq_sig[s++]);
        stub_dtp_ctrl.irq_o.bind(stub_irq_sig[s++]);
        stub_dfx_ctrl.irq_o.bind(stub_irq_sig[s++]);
        stub_cluster_ctrl.irq_o.bind(stub_irq_sig[s++]);
        stub_ifetch.irq_o.bind(stub_irq_sig[s++]);
        stub_data.irq_o.bind(stub_irq_sig[s++]);
    }

    // -- Interrupt aggregator -> PLIC -------------------------------------
    // Inputs: sep_mailbox[0..7], telemetry[0..2], i3c[0..5], uart[0..3],
    // avsbus, i2c[0..2], AXI hang, wdt[0..3], aou (local core only — the peer stub
    // models the remote die and its irq_o is not observable by local
    // firmware).  Order must match the plic_bits list passed above.
    {
        unsigned s = 0;
        for (unsigned i = 0; i < NUM_SEP_MAILBOX; ++i)
            intagg.src[s++].bind(sep_mailbox_irq_i[i]);
        for (unsigned i = 0; i < NUM_TELEMETRY; ++i)
            intagg.src[s++].bind(telemetry_irq[i]);
        for (unsigned i = 0; i < NUM_I3C; ++i)
            intagg.src[s++].bind(i3c_irq[i]);
        for (unsigned i = 0; i < NUM_UART; ++i)
            intagg.src[s++].bind(uart_irq[i]);
        intagg.src[s++].bind(avsbus_irq);
        for (unsigned i = 0; i < NUM_I2C; ++i)
            intagg.src[s++].bind(i2c_irq[i]);
        intagg.src[s++].bind(axi_hang_irq);
        for (unsigned i = 0; i < NUM_HARTS; ++i)
            intagg.src[s++].bind(wdt_irq[i]);
        intagg.src[s++].bind(gpio_irq_lower);
        intagg.src[s++].bind(gpio_irq_upper);
    }
    for (unsigned i = 0; i < kNumGpioWraps; ++i)
        gpio_irqs.wrap_irq[i].bind(gpio_wrap_irq[i]);
    gpio_irqs.lower_o.bind(gpio_irq_lower);
    gpio_irqs.upper_o.bind(gpio_irq_upper);
    for (unsigned i = 0; i < NUM_PLIC_SRC; ++i) {
        plic_.src_in[i].bind(plic_src_sig[i]);
        intagg.plic_src[i].bind(plic_src_sig[i]);
    }

    // -- CLINT -> (cluster IRQ when present, else dummy sinks) -------------
    for (unsigned h = 0; h < NUM_HARTS; ++h) {
        clint_.msip_o[h].bind(sig_irq_sw[h]);
        clint_.mtip_o[h].bind(sig_irq_timer[h]);
    }

    // -- PLIC -> cluster IRQ (all 8 contexts bound; M-mode feeds cluster) --
    for (unsigned c = 0; c < NUM_PLIC_CTX; ++c)
        plic_.ctx_out[c].bind(sig_irq_ext[c]);

#ifdef SMC_PLATFORM_WITH_CLUSTER
    for (unsigned h = 0; h < NUM_HARTS; ++h) {
        cluster.irq_sw[h]   .bind(sig_irq_sw[h]);
        cluster.irq_timer[h].bind(sig_irq_timer[h]);
        cluster.irq_ext[h]  .bind(sig_irq_ext[h]);
        cluster.wdt_timeout_cluster_i[h].bind(wdt_sticky[h]);
        // Phase C: BEU local (NMI-like) -> cluster beu_nmi_in.
        // irq_plic stays on beu_irq_plic sinks — OCA RTL routes BEU to the
        // per-tile buserror/NMI line, not the PLIC vector (see interrupts.adoc).
        cluster.beu_nmi_in[h].bind(beu_irq_local[h]);
    }
    cluster.rst_primary_n_i.bind(sig_rst_primary_smc);
    cluster.wdt_first_timeout_o.bind(sig_wdt_first);
    cluster.wdt_second_timeout_o.bind(sig_wdt_second);
    // Cluster MMIO enters the fabric; fetches go via cluster.data (already
    // bound to front_port_router for ROM/scratch/PLIC/CLINT).
    cluster.mmio.bind(fabric.mmio_in);
    // idle_mmio_init_ is unbound in this mode (cluster.mmio owns fabric.mmio_in),
    // so bind it to stub_data (3rd bind) so the initiator port is satisfied.
    idle_mmio_init_.bind(stub_data.reg_socket);
#else
    // No cluster -> bind an idle initiator to fabric.mmio_in so its BW port
    // is satisfied (the fabric requires exactly one initiator bound).
    idle_mmio_init_.bind(fabric.mmio_in);
#endif

    {
        std::ostringstream _oss;
        _oss << "smc_platform elaborated: "
             << NUM_UART << " uart, " << NUM_I2C << " i2c, "
             << NUM_I3C << " i3c, " << NUM_TELEMETRY << " telemetry, avsbus, " << NUM_HARTS << " wdt, "
             << NUM_BEU << " beu, pvt_wrap, pll_wrap, dma, "
             << NUM_PLIC_SRC << " plic sources";
#ifdef SMC_PLATFORM_WITH_CLUSTER
        _oss << ", cluster ON";
#else
        _oss << ", cluster OFF";
#endif
        SIM_LOG_INFO(this, _oss.str());
    }
}

// ---------------------------------------------------------------------------
// Late elaboration
// ---------------------------------------------------------------------------
// smc-vp has no SEP above it to drive sep_mailbox_irq_i, and SystemC requires
// every sc_in to be bound, so any channel the integrator left open is tied low
// once binding is otherwise complete.  A parent's bind always wins; this only
// fills gaps.
void smc_platform::before_end_of_elaboration()
{
    for (unsigned i = 0; i < NUM_SEP_MAILBOX; ++i) {
        if (!sep_mailbox_irq_i[i].get_interface()) {
            sep_mailbox_irq_tie_low_[i].write(false);
            sep_mailbox_irq_i[i](sep_mailbox_irq_tie_low_[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// Inbound forwarding
// ---------------------------------------------------------------------------
void smc_platform::fwd_sys_axi(tlm::tlm_generic_payload& gp, sc_core::sc_time& t)
{
    fwd_sys_->b_transport(gp, t);
}
void smc_platform::fwd_jtag_axi(tlm::tlm_generic_payload& gp, sc_core::sc_time& t)
{
    fwd_jtag_->b_transport(gp, t);
}
void smc_platform::fwd_sep_axi(tlm::tlm_generic_payload& gp, sc_core::sc_time& t)
{
    fwd_sep_->b_transport(gp, t);
}
void smc_platform::fwd_aou_axi(tlm::tlm_generic_payload& gp, sc_core::sc_time& t)
{
    fwd_aou_->b_transport(gp, t);
}

}  // namespace smc
