// SPDX-License-Identifier: Apache-2.0
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
// Address-map constants (absolute local addresses; see integration guide).
// ---------------------------------------------------------------------------
static constexpr uint64_t A_WDT_DEBUG   = 0xC000'0000ULL;
static constexpr uint64_t A_CPU_CTRL_FP  = 0xC003'9000ULL;
static constexpr uint64_t A_BOOTROM      = 0xC004'0000ULL;
static constexpr uint64_t A_SCRATCH      = 0xC006'0000ULL;
static constexpr uint64_t A_PLIC         = 0xC080'0000ULL;
static constexpr uint64_t A_CLINT        = 0xC0C0'0000ULL;
static constexpr uint64_t A_BEU          = 0xC0C1'0000ULL;
static constexpr uint64_t A_DMA          = 0xC003'8000ULL;  // dma_cfg default base_addr

static constexpr uint64_t A_RESET        = 0xC000'2000ULL;
static constexpr uint64_t A_I3C         = 0xC000'5000ULL;
static constexpr uint64_t A_AVSBUS       = 0xC000'8000ULL;
static constexpr uint64_t A_I2C0         = 0xC000'9000ULL;
static constexpr uint64_t A_UART0        = 0xC000'A000ULL;
static constexpr uint64_t A_CPU_CTRL    = 0xC040'0000ULL;
static constexpr uint64_t A_PERIPH_MAIN_LO = 0xC000'2000ULL;
static constexpr uint64_t A_PERIPH_MAIN_HI = 0xC000'E800ULL;
static constexpr uint64_t A_PERIPH_EXT_LO  = 0xC040'0000ULL;
static constexpr uint64_t A_PERIPH_EXT_HI  = 0xC080'0000ULL;

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
    , intagg("intagg", NUM_PERIPH_IRQ, NUM_PLIC_SRC,
             std::vector<unsigned>{
                 // i3c[0..5] -> peripheral bits 17:12
                 12, 13, 14, 15, 16, 17,
                 // uart[0..3] -> peripheral bits 21:18
                 18, 19, 20, 21,
                 // avsbus -> peripheral bit 22
                 22,
                 // i2c[0..2] -> peripheral bits 25:23
                 23, 24, 25})
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
    sig_straps.write(0);

    for (unsigned i = 0; i < NUM_UART; ++i) {
        uart_rx[i].write(true);   // idle high
        uart_cts[i].write(false); // asserted (ready)
        uart_dsr[i].write(true);  // tied high (unused modem inputs)
        uart_ri[i].write(true);
        uart_dcd[i].write(true);
    }

    // -- External inbound targets -> fabric --------------------------------
    fwd_sys_ .bind(fabric.sys_axi_in);
    fwd_jtag_.bind(fabric.jtag_axi_in);
    fwd_sep_ .bind(fabric.sep_axi_in);
    sys_axi_in .register_b_transport(this, &smc_platform::fwd_sys_axi);
    jtag_axi_in.register_b_transport(this, &smc_platform::fwd_jtag_axi);
    sep_axi_in .register_b_transport(this, &smc_platform::fwd_sep_axi);

    // -- Fabric reset -------------------------------------------------------
    fabric.rst_n_i.bind(rst_n_sig);

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
    front_port_router.add_route(0, A_WDT_DEBUG,   0x1000,   "wdt_debug");
    front_port_router.add_route(1, A_CPU_CTRL_FP, 0x1000,   "cpu_ctrl_fp");
    front_port_router.add_route(2, A_BOOTROM,     0x20000,  "bootrom");
    front_port_router.add_route(3, A_SCRATCH,     0x100000, "scratch");
    front_port_router.add_route(4, A_PLIC,        0x400000, "plic");
    front_port_router.add_route(5, A_CLINT,       0x20000,  "clint");
    front_port_router.add_route(6, A_BEU,         0x10000,  "beu");
    front_port_router.out[0].bind(stub_wdt_debug.reg_socket);

    // The three multi_stub_target placeholders are each always bound by their
    // own idle initiator (so they are never unbound).  In cluster mode the
    // cluster-only initiators add a second bind:
    //   out[1]      -> cluster.ctrl (cluster) / stub_cluster_ctrl (no-cluster)
    //   cluster.ifetch -> stub_ifetch   (cluster only)
    //   cluster.data    -> stub_data     (cluster only)
    idle_ctrl_init_  .bind(stub_cluster_ctrl.reg_socket);
    idle_ifetch_init_.bind(stub_ifetch.reg_socket);
    idle_data_init_   .bind(stub_data.reg_socket);
#ifdef SMC_PLATFORM_WITH_CLUSTER
    front_port_router.out[1].bind(cluster.ctrl);
    cluster.ifetch.bind(stub_ifetch.reg_socket);
    cluster.data   .bind(stub_data.reg_socket);
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

    // -- periph_router: fabric.to_periph -----------------------------------
    fabric.to_periph.bind(periph_router.tgt);
    periph_router.add_route(0, A_RESET,     0x200,  "reset");
    periph_router.add_route(1, A_I3C,       0x1E00, "i3c");
    periph_router.add_route(2, A_I2C0,      0x200,  "i2c0");
    periph_router.add_route(3, A_I2C0 + 0x200,  0x200, "i2c1");
    periph_router.add_route(4, A_I2C0 + 0x400, 0x200, "i2c2");
    periph_router.add_route(5, A_UART0,      0x1000, "uart0");
    periph_router.add_route(6, A_UART0 + 0x1000, 0x1000, "uart1");
    periph_router.add_route(7, A_UART0 + 0x2000, 0x1000, "uart2");
    periph_router.add_route(8, A_UART0 + 0x3000, 0x1000, "uart3");
    periph_router.add_route(9, A_CPU_CTRL,  0x2000, "cpu_ctrl");
    // AVSBus: 4 KiB window at 0xC000_8000 (shadows the periph_misc catch-all).
    periph_router.add_route(11, A_AVSBUS,   0x1000, "avsbus");
    // Catch-alls (largest windows, checked last) -> periph_misc stub.
    periph_router.add_route(10, A_PERIPH_MAIN_LO, A_PERIPH_MAIN_HI - A_PERIPH_MAIN_LO, "periph_main_misc");
    periph_router.add_route(10, A_PERIPH_EXT_LO,  A_PERIPH_EXT_HI  - A_PERIPH_EXT_LO,  "periph_ext_misc");
    periph_router.out[0].bind(reset.reg_socket);
    periph_router.out[1].bind(i3c.reg_socket);
    periph_router.out[2].bind(i2c[0].reg_socket);
    periph_router.out[3].bind(i2c[1].reg_socket);
    periph_router.out[4].bind(i2c[2].reg_socket);
    periph_router.out[5].bind(uart_[0].reg_socket);
    periph_router.out[6].bind(uart_[1].reg_socket);
    periph_router.out[7].bind(uart_[2].reg_socket);
    periph_router.out[8].bind(uart_[3].reg_socket);
    periph_router.out[9].bind(cpu_ctrl_.reg_socket);
    periph_router.out[10].bind(stub_periph_misc.reg_socket);
    periph_router.out[11].bind(avsbus.reg_socket);

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
    // DMA path: the zeroer's 32-bit initiator injects zero-fill writes back
    // into the fabric via the 64-bit `data_accel_in` target (width adapter).
    zeroer.dma_socket.bind(wa_zeroer_dma.tgt);
    wa_zeroer_dma.init.bind(fabric.data_accel_in);
    zeroer.rst_n_i.bind(rst_n_sig);
    zeroer.irq_o.bind(sig_zeroer_irq);

    // -- Fabric initiator stubs -------------------------------------------
    fabric.to_dfd_apb              .bind(stub_dfd.reg_socket);
    fabric.to_mailbox              .bind(stub_mbox.reg_socket);
    fabric.to_dft_csr              .bind(stub_dft.reg_socket);
    fabric.output_axi              .bind(stub_sysmem.reg_socket);
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

    // -- plic / clint reset ------------------------------------------------
    plic_.rst_n_i .bind(rst_n_sig);
    clint_.rst_n_i.bind(rst_n_sig);

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
    }

    // -- I3C port binding --------------------------------------------------
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

    // -- AVSBus reset + IRQ + GPIO-enable sink -----------------------------
    avsbus.rst_n_i.bind(rst_n_sig);
    avsbus.irq_o.bind(avsbus_irq);
    avsbus.avs_gpio_enable_o.bind(avsbus_gpio_en);

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
            &stub_wdt_debug, &stub_dfd, &stub_mbox,
            &stub_dft, &stub_sysmem, &stub_cpu_ctrl_fab, &stub_aR, &stub_mR,
            &stub_xR, &stub_ibf, &stub_obf,
        };
        unsigned s = 0;
        for (auto* st : stubs64) st->irq_o.bind(stub_irq_sig[s++]);
        stub_periph_misc.irq_o.bind(stub_irq_sig[s++]);
        stub_cluster_ctrl.irq_o.bind(stub_irq_sig[s++]);
        stub_ifetch.irq_o.bind(stub_irq_sig[s++]);
        stub_data.irq_o.bind(stub_irq_sig[s++]);
    }

    // -- Interrupt aggregator -> PLIC -------------------------------------
    // Inputs: i3c[0..5], uart[0..3], avsbus, i2c[0..2].
    for (unsigned i = 0; i < NUM_I3C; ++i)  intagg.src[i].bind(i3c_irq[i]);
    for (unsigned i = 0; i < NUM_UART; ++i) intagg.src[NUM_I3C + i].bind(uart_irq[i]);
    intagg.src[NUM_I3C + NUM_UART].bind(avsbus_irq);
    for (unsigned i = 0; i < NUM_I2C; ++i)
        intagg.src[NUM_I3C + NUM_UART + 1 + i].bind(i2c_irq[i]);
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
        // Phase C: BEU local (NMI-like) -> cluster beu_nmi_in.
        // irq_plic stays on beu_irq_plic sinks — OCA RTL routes BEU to the
        // per-tile buserror/NMI line, not the PLIC vector (see interrupts.adoc).
        cluster.beu_nmi_in[h].bind(beu_irq_local[h]);
    }
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
             << NUM_I3C << " i3c, " << NUM_BEU << " beu, avsbus, dma, "
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

}  // namespace smc
