// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// smc/platform/src/smc_platform.cpp
// ===========================================================================

#include "smc_platform.h"

#include <systemc>
#include <tlm.h>

#include <cstdint>
#include <sstream>
#include <vector>

namespace smc {

// ---------------------------------------------------------------------------
// Address-map constants (absolute local addresses; see integration guide).
// ---------------------------------------------------------------------------
static constexpr uint64_t A_WDT_DEBUG   = 0xC000'0000ULL;
static constexpr uint64_t A_CPU_CTRL_FP  = 0xC003'9000ULL;
static constexpr uint64_t A_BOOTROM      = 0xC004'0000ULL;
static constexpr uint64_t A_SCRATCH      = 0xC006'0000ULL;
static constexpr uint64_t A_PLIC         = 0xC400'0000ULL;
static constexpr uint64_t A_CLINT        = 0xC800'0000ULL;
static constexpr uint64_t A_BEU          = 0xC801'0000ULL;

static constexpr uint64_t A_RESET        = 0xC000'2000ULL;
static constexpr uint64_t A_I3C         = 0xC000'5000ULL;
static constexpr uint64_t A_I2C0         = 0xC000'9000ULL;
static constexpr uint64_t A_UART0        = 0xC000'A000ULL;
static constexpr uint64_t A_CPU_CTRL    = 0xC001'0000ULL;
static constexpr uint64_t A_PERIPH_MAIN_LO = 0xC000'2000ULL;
static constexpr uint64_t A_PERIPH_MAIN_HI = 0xC000'E800ULL;
static constexpr uint64_t A_PERIPH_EXT_LO  = 0xC040'0000ULL;
static constexpr uint64_t A_PERIPH_EXT_HI  = 0xC080'0000ULL;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
smc_platform::smc_platform(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , intagg("intagg", NUM_PERIPH_IRQ, NUM_PLIC_SRC,
             std::vector<unsigned>{
                 // i3c[0..5] -> peripheral bits 17:12
                 12, 13, 14, 15, 16, 17,
                 // uart[0..3] -> peripheral bits 21:18
                 18, 19, 20, 21,
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

    // Idle initiators satisfy the fabric's log_in / data_accel_in BW ports.
    idle_log_init_   .bind(fabric.log_in);
    idle_daccel_init_.bind(fabric.data_accel_in);

    // -- front_port_router: fabric.to_front_port (cluster.data is routed to a
    //    dedicated multi_stub_target below, not through the shared router, so
    //    the router target only ever takes the one fabric initiator). --------
    fabric.to_front_port.bind(front_port_router.tgt);
    front_port_router.add_route(0, A_WDT_DEBUG,   0x1000,   "wdt_debug");
    front_port_router.add_route(1, A_CPU_CTRL_FP, 0x1000,   "cpu_ctrl_fp");
    front_port_router.add_route(2, A_BOOTROM,     0x20000,  "bootrom");
    front_port_router.add_route(3, A_SCRATCH,     0x100000, "scratch");
    front_port_router.add_route(4, A_PLIC,        0x4000000,"plic");
    front_port_router.add_route(5, A_CLINT,       0xC000,   "clint");
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
    front_port_router.out[6].bind(stub_beu.reg_socket);

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
    // Catch-alls (largest windows, checked last) -> periph_misc stub.
    periph_router.add_route(10, A_PERIPH_MAIN_LO, A_PERIPH_MAIN_HI - A_PERIPH_MAIN_LO, "periph_main_misc");
    periph_router.add_route(10, A_PERIPH_EXT_LO,  A_PERIPH_EXT_HI  - A_PERIPH_EXT_LO,  "periph_ext_misc");
    periph_router.out[0].bind(reset.reg_socket);
    periph_router.out[1].bind(i3c.reg_socket);
    periph_router.out[2].bind(i2c[0].reg_socket);
    periph_router.out[3].bind(i2c[1].reg_socket);
    periph_router.out[4].bind(i2c[2].reg_socket);
    periph_router.out[5].bind(uart[0].reg_socket);
    periph_router.out[6].bind(uart[1].reg_socket);
    periph_router.out[7].bind(uart[2].reg_socket);
    periph_router.out[8].bind(uart[3].reg_socket);
    periph_router.out[9].bind(cpu_ctrl_.reg_socket);
    periph_router.out[10].bind(stub_periph_misc.reg_socket);

    // -- Fabric initiator stubs -------------------------------------------
    fabric.to_data_accel_ctrl     .bind(stub_dma.reg_socket);
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
        uart[i].rst_n_i.bind(rst_n_sig);
        uart[i].rx_i  .bind(uart_rx[i]);
        uart[i].cts_ni.bind(uart_cts[i]);
        uart[i].dsr_ni.bind(uart_dsr[i]);
        uart[i].ri_ni .bind(uart_ri[i]);
        uart[i].dcd_ni.bind(uart_dcd[i]);
        uart[i].tx_o   .bind(uart_tx[i]);
        uart[i].rts_no .bind(uart_rts[i]);
        uart[i].dtr_no .bind(uart_dtr[i]);
        uart[i].out1_no.bind(uart_out1[i]);
        uart[i].out2_no.bind(uart_out2[i]);
        uart[i].rxrdy_o.bind(uart_rxrdy[i]);
        uart[i].txrdy_o.bind(uart_txrdy[i]);
        uart[i].err_o   .bind(uart_err[i]);
        uart[i].irq_o   .bind(uart_irq[i]);
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

    // -- Bind every stub_target's irq_o to a dummy sink --------------------
    {
        stub_target<64>* stubs64[] = {
            &stub_wdt_debug, &stub_beu, &stub_dma, &stub_dfd, &stub_mbox,
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
    // Inputs: i3c[0..5], uart[0..3], i2c[0..2].
    for (unsigned i = 0; i < NUM_I3C; ++i)  intagg.src[i].bind(i3c_irq[i]);
    for (unsigned i = 0; i < NUM_UART; ++i) intagg.src[NUM_I3C + i].bind(uart_irq[i]);
    for (unsigned i = 0; i < NUM_I2C; ++i)  intagg.src[NUM_I3C + NUM_UART + i].bind(i2c_irq[i]);
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
             << NUM_I3C << " i3c, " << NUM_PLIC_SRC << " plic sources";
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
