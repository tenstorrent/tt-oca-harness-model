// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smc/smc_platform.hpp
//
// Top-level SMC SystemC/TLM-2.0 platform.  Instantiates the modeled blocks
// (fabric, reset unit, PLIC, CLINT, boot ROM, scratchpad, cpu_ctrl, DMA, I3C,
// 3x I2C, 4x UART, 3x telemetry_receiver, AVSBus controller, 4x WDT, 4x per-core BEU, and optionally the
// Whisper-backed CPU cluster), wires the fabric's initiator sockets through
// address routers to the modeled targets and stubs, composes the peripheral
// interrupt vector into the PLIC, and — when the cluster is present — connects
// CLINT MSIP/MTIP and PLIC MEIP to the cluster's per-hart IRQ inputs.
//
// See smc/doc/systemc_tlm2_integration_guide.adoc for the address map and
// binding rationale.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>

#include <cstdint>

#include "sim_log.h"
#include "smc_axi_extension.h"
#include "smc_fabric.h"

#include "bootrom.h"
// Path-qualified ("clint/include/...", "uart/include/...") because the SEP
// platform ships same-named headers (vp/platform/infra/clint.h,
// sep/peripherals/uart_16550/include/uart.h): in the SMU platform TU both
// include sets are visible and the bare names are ambiguous.  Requires
// smc/peripherals on the include path (both smc-vp and smu-vp add it).
#include "clint/include/clint.h"
#include "cpu_ctrl.h"
#include "dma.h"
#include "i2c_controller.h"
#include "i3c_controller.h"
#include "memory_zeroer.h"
#include "octs_system_timer.h"
#include "pll_wrapper.h"
#include "pvt_wrap.h"
#include "plic.h"
#include "reset_unit.h"
#include "scratchpad_ram.h"
#include "uart/include/uart.h"
#include "wdt.h"
#include "beu.h"
#include "telemetry_receiver.h"
#include "avsbus_controller.h"
#include "aou_core.h"

#ifdef SMC_PLATFORM_WITH_CLUSTER
#include "smc_cpu_cluster.h"
#endif

#include "inc/addr_router.h"
#include "inc/interrupt_aggregator.h"
#include "inc/multi_stub_target.h"
#include "inc/stub_target.h"
#include "inc/width_adapter.h"

namespace smc {

class smc_platform : public sc_core::sc_module
{
public:
    static constexpr unsigned NUM_HARTS     = 4;
    static constexpr unsigned NUM_BEU       = NUM_HARTS; // one BEU per core
    static constexpr unsigned NUM_UART      = 4;
    static constexpr unsigned NUM_I2C       = 3;
    static constexpr unsigned NUM_I3C       = 6;
    static constexpr unsigned NUM_TELEMETRY = 3;
    static constexpr unsigned NUM_PLIC_SRC   = 336;
    static constexpr unsigned NUM_PLIC_CTX   = 8;
    static constexpr unsigned NUM_SUBSYS     = 32;
    // SEP mailbox channels feeding peripheral_interrupts_o[7:0] (sep_pkg::NUM_MAILBOXES).
    static constexpr unsigned NUM_SEP_MAILBOX = 8;
    // Peripheral IRQ inputs: SEP mailbox[0..7], telemetry[0..2], i3c[0..5],
    // uart[0..3], avsbus, i2c[0..2], AXI hang, and wdt[0..3]. AOU is deliberately absent:
    // RTL does not assign it a peripheral_interrupts_o slot.
    static constexpr unsigned NUM_PERIPH_IRQ =
        NUM_SEP_MAILBOX + NUM_TELEMETRY + NUM_I3C + NUM_UART + 1 + NUM_I2C
        + 1 + NUM_HARTS;
    // RTL packs peripheral_interrupts_i at cpu_interrupts_o[NUM_EXT_INTERRUPTS+:32]
    // and the PLIC's source ID is that bit index + 1, so peripheral bit b is
    // source 256 + b + 1 in the 4-core configuration.
    static constexpr unsigned NUM_EXT_INTERRUPTS = 256;

    // -----------------------------------------------------------------------
    // Test-only BEU error-injection hook (Phase D1; see
    // smc/peripherals/beu/doc/04_BEU_Platform_Integration_Test_Plan.md §Phase
    // D/E2/E3).  Firmware has no way to synthesize a live cache/TileLink ECC
    // event in the VP, so up to 3 errors can be injected directly into named
    // BEU instances at elaboration (before sc_start(), so the state is
    // already latched when firmware's first register read happens),
    // entirely CCI-driven.  Every slot defaults to enable=false, so plain
    // smc-vp runs (and every other smc-vp-tests/ test) are unaffected; a
    // test's own .ini opts in explicitly (see smc-beu-error-test/).
    // Declared before ports/sized members per the CCI parameter convention.
    // -----------------------------------------------------------------------
    cci::cci_param<bool>     beu_inject0_enable_p_;
    cci::cci_param<unsigned> beu_inject0_core_p_;
    cci::cci_param<unsigned> beu_inject0_src_p_;
    cci::cci_param<uint64_t> beu_inject0_addr_p_;

    cci::cci_param<bool>     beu_inject1_enable_p_;
    cci::cci_param<unsigned> beu_inject1_core_p_;
    cci::cci_param<unsigned> beu_inject1_src_p_;
    cci::cci_param<uint64_t> beu_inject1_addr_p_;

    cci::cci_param<bool>     beu_inject2_enable_p_;
    cci::cci_param<unsigned> beu_inject2_core_p_;
    cci::cci_param<unsigned> beu_inject2_src_p_;
    cci::cci_param<uint64_t> beu_inject2_addr_p_;

    /// Period of the clock feeding octs_system_timer, in nanoseconds.
    /// Immutable: `octs_clk` reads it once in the member-initialiser list.
    cci::cci_param<double, cci::CCI_IMMUTABLE_PARAM> octs_clk_period_ns_p_;

    /// Strap value for octs_system_timer's `is_primary_i`.  True (the silicon
    /// configuration) makes SMC the system timekeeping PRIMARY.  A test can set
    /// it false via its .ini to exercise the SECONDARY branch — with no
    /// sync_load source in the SMC VP the timer then stays idle by design.
    cci::cci_param<bool> octs_is_primary_p_;
    // Test-only telemetry ATB inject (elaboration-time).  Firmware cannot drive
    // the ATB byte stream; one message can be pushed into a named receiver
    // before sc_start() so register-visible probe_id / counters are already
    // latched.  Defaults to enable=false so plain smc-vp runs are unaffected.
    cci::cci_param<bool>     tel_inject_enable_p_;
    cci::cci_param<unsigned> tel_inject_inst_p_;
    cci::cci_param<unsigned> tel_inject_probe_p_;
    cci::cci_param<uint32_t> tel_inject_counter0_p_;

    // -----------------------------------------------------------------------
    // External boundary (chiplet-facing).  Inbound masters are forwarded to
    // the fabric; outbound `fabric.output_axi` is re-exported hierarchically
    // as `output_axi` so the integrating executable decides what terminates
    // it (smc-vp binds a stub; the SMU platform binds the SMU crossbar).
    // `output_axi` / `aou_axi_m` must be plain tlm_initiator_socket: a
    // simple_* socket's internal sc_export is bound to its own fw process at
    // construction, so it can never be the parent of a hierarchical initiator
    // bind (E126).
    //
    // AOU AXI (RTL: AoU sits on SMU `smu_axi_in`/`smu_axi_out`, i.e. the
    // xbar's ext_in/ext_out — see tt-oca-hw doc/architecture.adoc).  Local
    // CSRs stay on the SMC periph bus; the AXI hop is the D2D data path.
    // smc-vp idle/stubs these; smu-vp binds them to the xbar chiplet ports.
    // -----------------------------------------------------------------------
    tlm_utils::simple_target_socket<smc_platform, 64>   sys_axi_in{"sys_axi_in"};
    tlm_utils::simple_target_socket<smc_platform, 64>   jtag_axi_in{"jtag_axi_in"};
    tlm_utils::simple_target_socket<smc_platform, 64>   sep_axi_in{"sep_axi_in"};
    tlm::tlm_initiator_socket<64>                        output_axi{"output_axi"};
    tlm_utils::simple_target_socket<smc_platform, 64>   aou_axi_s{"aou_axi_s"};
    tlm::tlm_initiator_socket<64>                        aou_axi_m{"aou_axi_m"};

    // SEP mailbox interrupts entering the SMC (RTL smc.sv: sep_mailbox_interrupts_i),
    // one per SEP mailbox channel.  smu-vp binds these to the SEP platform's
    // inbound lines; smc-vp leaves them open and before_end_of_elaboration()
    // ties them low, the same way el2_pic handles its undriven sources.
    sc_core::sc_vector<sc_core::sc_in<bool>>
        sep_mailbox_irq_i{"sep_mailbox_irq_i", NUM_SEP_MAILBOX};

    // -----------------------------------------------------------------------
    // Modeled blocks
    // -----------------------------------------------------------------------
    smc_fabric      fabric{"fabric"};
    reset_unit      reset{"reset"};
    plic            plic_{"plic"};
    clint           clint_{"clint"};
    bootrom         bootrom_{"bootrom"};
    scratchpad_ram   scratch{"scratchpad_ram"};
    cpu_ctrl        cpu_ctrl_{"cpu_ctrl"};
    dma             dma_{"dma"};
    memory_zeroer   zeroer{"memory_zeroer"};
    octs_system_timer octs_timer{"octs_system_timer"};
    pll::pll_wrapper pll_wrap{"pll_wrap"};
    pvt_wrap        pvt_wrap_{"pvt_wrap"};
    i3c_controller  i3c{"i3c"};
    sc_core::sc_vector<i2c_controller> i2c{"i2c", NUM_I2C};
    i2c_wrap_ctrl                      i2c_ctrl_{"i2c_ctrl"};
    sc_core::sc_vector<uart>           uart_{"uart", NUM_UART};
    sc_core::sc_vector<uart_wrap>      uart_wrap_{"uart_wrap", NUM_UART};
    straps                             straps_{"straps"};
    // Trailing underscore avoids colliding with class smc::wdt (GCC -fpermissive).
    sc_core::sc_vector<wdt>            wdt_{"wdt", NUM_HARTS};
    sc_core::sc_vector<beu>            beu_{"beu", NUM_BEU};
    sc_core::sc_vector<telemetry_receiver> telemetry_{"telemetry", NUM_TELEMETRY};
    avsbus_controller                  avsbus{"avsbus"};
    // AOU: local + peer LT bridge (FDI abstracted via connect_peer).
    aou::aou_core                      aou_{"aou"};
    aou::aou_core                      aou_peer_{"aou_peer"};
    sc_core::sc_signal<bool>           aou_fdi_active{"aou_fdi_active"};
    sc_core::sc_signal<bool>           aou_peer_fdi_active{"aou_peer_fdi_active"};
    sc_core::sc_signal<bool>           aou_irq{"aou_irq"};
    sc_core::sc_signal<bool>           aou_peer_irq{"aou_peer_irq"};
#ifdef SMC_PLATFORM_WITH_CLUSTER
    smc_cpu_cluster cluster{"cluster"};
#endif

    // -----------------------------------------------------------------------
    // Routers and aggregator.
    //
    // front_port_router: 64-bit in (fabric.to_front_port + cluster.data) ->
    //   64-bit outputs.  The 32-bit front-port peripherals (boot ROM,
    //   scratchpad, PLIC, CLINT) sit behind `width_adapter<64,32>` instances
    //   because TLM simple sockets refuse cross-width binds.  The WDT window
    //   is demuxed by `wdt_demux` (64→32) into four 1 KiB instances.
    //
    // periph_router: 64-bit in (fabric.to_periph) -> 32-bit outputs (every
    //   peripheral on this bus is a 32-bit reg_socket).
    // -----------------------------------------------------------------------
    addr_router<64, 64>         front_port_router{"front_port_router", 7};
    addr_router<64, 32>         wdt_demux{"wdt_demux", NUM_HARTS};
    // Demux the 64 KiB BEU alias window into NUM_BEU per-core 4 KiB targets.
    addr_router<64, 64>         beu_router{"beu_router", NUM_BEU};
    // periph_router outputs: reset, i2c[0..2], telemetry demux, uart[0..3],
    // gpio stub, i3c, pvt_wrap, pll_wrap, avsbus, aou, octs_system_timer,
    // catch-all, misc/NDM/efuse/DTP/DFX stubs, i2c_ctrl, uart_wrap[4], straps.
    addr_router<64, 32>         periph_router{"periph_router", 29};
    // Demux the 0x300 telemetry wrap into NUM_TELEMETRY 0x100 windows.
    // InBus=32: sits behind periph_router's 32-bit initiator outputs.
    addr_router<32, 32>         telemetry_router{"telemetry_router", NUM_TELEMETRY};
    width_adapter<64, 32>       wa_bootrom{"wa_bootrom"};
    width_adapter<64, 32>       wa_scratch{"wa_scratch"};
    width_adapter<64, 32>       wa_plic{"wa_plic"};
    width_adapter<64, 32>       wa_clint{"wa_clint"};
    // DMA and memory_zeroer share the fabric's single (simple, single-bind)
    // `to_data_accel_ctrl` initiator socket -- both live in the RTL's
    // 0xC003_8000..0xC0040000 data-accelerator CSR window (see
    // DACCEL_DMA_ZEROER_BASE/_END in smc_fabric.h) -- so their CSR targets
    // fan out through a shared 2-entry addr_router instead of each binding
    // to_data_accel_ctrl directly (which only the first bind would win):
    //  - out[0] "memory_zeroer": rebases the absolute 0xC003_82xx window to
    //    the IP's 0-based register offsets (and adapts 64->32).  A plain
    //    width_adapter would NOT rebase, so the IP would see 0xC003_8200
    //    instead of offset 0 and reject every access.
    //  - out[1] "dma": rebases the absolute 0xC003_80xx window the same way
    //    (dma::normalize_addr() also tolerates absolute addresses, so this
    //    is belt-and-suspenders, not strictly required); dma_.reg_socket is
    //    already 32-bit, so out[1] binds it directly (no width_adapter).
    addr_router<64, 32>         daccel_router{"daccel_router", 2};
    // Separately, both the DMA master and the memory_zeroer DMA master drive the
    // fabric's multi-bind 64-bit `data_accel_in` target; a width_adapter converts
    // the zeroer's 32-bit initiator to the 64-bit fabric target.  Addresses stay
    // absolute until the fabric re-routes them.
    width_adapter<32, 64>       wa_zeroer_dma{"wa_zeroer_dma"};
    interrupt_aggregator        intagg;

    // -----------------------------------------------------------------------
    // Stubs for unmodeled / RTL-connected blocks
    // -----------------------------------------------------------------------
    // WDT is modeled (wdt_[] + wdt_demux); no stub_wdt_debug.
    stub_target<64> stub_dfd{"stub_dfd"};
    stub_target<64> stub_mbox{"stub_mbox"};
    stub_target<64> stub_dft{"stub_dft"};
    // Remote-die SMN/memory stand-in: aou_peer_.axi_m lands here.
    // store_writes so SMC catch-all traffic through AOU (ext_out) can
    // read back (port of tt-oca-hw smu_sep_ext_axi).
    stub_target<64> stub_sysmem{"stub_sysmem", /*warn=*/true, /*store=*/true};
    stub_target<64> stub_cpu_ctrl_fab{"stub_cpu_ctrl_fab"};
    stub_target<64> stub_aR{"stub_aR"};
    stub_target<64> stub_mR{"stub_mR"};
    stub_target<64> stub_xR{"stub_xR"};
    stub_target<64> stub_ibf{"stub_ibf"};
    stub_target<64> stub_obf{"stub_obf"};
    stub_target<32> stub_periph_misc{"stub_periph_misc"};
    // Named stubs for unmodeled RTL periph slots (Phase-1 map realignment).
    stub_target<32> stub_gpio_intf{"stub_gpio_intf"};
    stub_target<32> stub_misc_wrap{"stub_misc_wrap"};
    stub_target<32> stub_ndm_reset{"stub_ndm_reset", /*warn=*/true, /*store=*/true};
    stub_target<32> stub_efuse_map{"stub_efuse_map"};
    stub_target<32> stub_efuse_ctrl{"stub_efuse_ctrl"};
    stub_target<32> stub_dtp_ctrl{"stub_dtp_ctrl"};
    stub_target<32> stub_dfx_ctrl{"stub_dfx_ctrl"};
    // stub_cluster_ctrl / stub_ifetch / stub_data accept multiple initiator
    // binds (multi_stub_target) so that an always-bound idle initiator and a
    // cluster-only initiator (out[1]/cluster.ifetch/cluster.data) can share
    // the same stub across cluster ON/OFF build modes without #ifdef gymnastics
    // or unbound-socket cascades.
    multi_stub_target<64> stub_cluster_ctrl{"stub_cluster_ctrl"};
    multi_stub_target<64> stub_ifetch{"stub_ifetch"};
    // stub_data accepts up to 3 initiator binds: idle_data_init_ (always),
    // cluster.data (cluster-ON), and idle_mmio_init_ (cluster-ON, where
    // cluster.mmio owns fabric.mmio_in so idle_mmio_init_ needs an alternate
    // target to satisfy the initiator-port binding rule).
    multi_stub_target<64, 3> stub_data{"stub_data"};

    // -----------------------------------------------------------------------
    // Reset / strap signals
    // -----------------------------------------------------------------------
    sc_core::sc_signal<bool>    rst_n_sig{"rst_n_sig"};
    sc_core::sc_signal<bool>    sig_powergood{"sig_powergood"};
    sc_core::sc_signal<bool>    sig_fuse_reset{"sig_fuse_reset"};
    sc_core::sc_signal<bool>    sig_rst_ext_wdt{"sig_rst_ext_wdt"};
    sc_core::sc_signal<bool>    sig_wdt_first{"sig_wdt_first"};
    sc_core::sc_signal<bool>    sig_wdt_second{"sig_wdt_second"};
    sc_core::sc_signal<bool>    sig_rst_cool{"sig_rst_cool"};
    sc_core::sc_signal<bool>    sig_isolate{"sig_isolate"};
    sc_core::sc_signal<bool>    sig_flr{"sig_flr"};
    sc_core::sc_signal<uint32_t> sig_ss_complete{"sig_ss_complete"};
    sc_core::sc_signal<uint64_t> sig_straps{"sig_straps"};

    // reset_unit outputs (bound, unused beyond elaboration)
    sc_core::sc_signal<bool>     sig_powergood_stable{"sig_powergood_stable"};
    sc_core::sc_signal<bool>     sig_rst_cold_stable_ref{"sig_rst_cold_stable_ref"};
    sc_core::sc_signal<bool>     sig_rst_cold_stable_smc{"sig_rst_cold_stable_smc"};
    sc_core::sc_signal<bool>     sig_rst_primary_ref{"sig_rst_primary_ref"};
    sc_core::sc_signal<bool>     sig_rst_primary_smc{"sig_rst_primary_smc"};
    sc_core::sc_signal<bool>     sig_rst_primary_periph{"sig_rst_primary_periph"};
    sc_core::sc_signal<bool>     sig_rst_core_smc{"sig_rst_core_smc"};
    sc_core::sc_signal<bool>     sig_rst_wdt_smc{"sig_rst_wdt_smc"};
    sc_core::sc_signal<bool>     sig_rst_cool_no{"sig_rst_cool_no"};
    sc_core::sc_signal<bool>     sig_skip_mem_repair{"sig_skip_mem_repair"};
    sc_core::sc_signal<bool>     sig_sync_irq{"sig_sync_irq"};
    sc_core::sc_signal<uint32_t> sig_isolate_o{"sig_isolate_o"};
    sc_core::sc_signal<uint32_t> sig_ss_config_o{"sig_ss_config_o"};
    sc_core::sc_vector<sc_core::sc_signal<reset_ctrl_t>> sig_ss_reset{"sig_ss_reset", NUM_SUBSYS};

    // -----------------------------------------------------------------------
    // UART signals
    // -----------------------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_rx{"uart_rx", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_cts{"uart_cts", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_dsr{"uart_dsr", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_ri{"uart_ri", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_dcd{"uart_dcd", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_tx{"uart_tx", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_rts{"uart_rts", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_dtr{"uart_dtr", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_out1{"uart_out1", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_out2{"uart_out2", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_rxrdy{"uart_rxrdy", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_txrdy{"uart_txrdy", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_err{"uart_err", NUM_UART};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_irq{"uart_irq", NUM_UART};

    // -----------------------------------------------------------------------
    // PVT wrapper signals (unused externally, bound for elaboration)
    // -----------------------------------------------------------------------
    sc_core::sc_signal<bool>     pvt_process_clk_obs{"pvt_process_clk_obs"};
    sc_core::sc_signal<bool>     pvt_process_clk_obs_en{"pvt_process_clk_obs_en"};
    sc_core::sc_signal<uint32_t> pvt_voltage_code{"pvt_voltage_code"};
    sc_core::sc_signal<bool>     pvt_temp_interrupt{"pvt_temp_interrupt"};

    // -----------------------------------------------------------------------
    // I3C signals (unused pad lines, bound for elaboration)
    // -----------------------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_irq{"i3c_irq", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_scl{"i3c_scl", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_sda{"i3c_sda", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_scl_oe{"i3c_scl_oe", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_sda_oe{"i3c_sda_oe", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_sel_od_pp{"i3c_sel_od_pp", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_rpa{"i3c_rpa", NUM_I3C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> i3c_ria{"i3c_ria", NUM_I3C};

    // -----------------------------------------------------------------------
    // I2C / WDT / PLIC / cluster IRQ signals
    // -----------------------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_signal<bool>> i2c_irq{"i2c_irq", NUM_I2C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> telemetry_irq{"telemetry_irq", NUM_TELEMETRY};
    sc_core::sc_vector<sc_core::sc_signal<bool>> telemetry_afready{"telemetry_afready", NUM_TELEMETRY};
    sc_core::sc_vector<sc_core::sc_signal<bool>> telemetry_afvalid{"telemetry_afvalid", NUM_TELEMETRY};
    sc_core::sc_vector<sc_core::sc_signal<bool>> telemetry_atready{"telemetry_atready", NUM_TELEMETRY};
    sc_core::sc_vector<sc_core::sc_signal<uint32_t>> telemetry_debug{"telemetry_debug", NUM_TELEMETRY};
    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_irq{"wdt_irq", NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_sticky{"wdt_sticky", NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_core_rst{"wdt_core_rst", NUM_HARTS};
    // Standalone periph-bus cpu_ctrl_ stage-2 ports (not driven by front-port WDTs).
    sc_core::sc_vector<sc_core::sc_signal<bool>> cpu_ctrl_wdt_sticky{
        "cpu_ctrl_wdt_sticky", NUM_HARTS};
    sc_core::sc_signal<bool> cpu_ctrl_wdt_first{"cpu_ctrl_wdt_first"};
    sc_core::sc_signal<bool> cpu_ctrl_wdt_second{"cpu_ctrl_wdt_second"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic_src_sig{"plic_src_sig", NUM_PLIC_SRC};
    // Dummy sinks for every stub_target's irq_o (stubs don't drive the PLIC in
    // Phase 1; their irq_drive_method still requires a bound port).
    sc_core::sc_vector<sc_core::sc_signal<bool>> stub_irq_sig{"stub_irq_sig", 25};
    // BEU IRQ: local (NMI-like) is wired to cluster.beu_nmi_in in Phase C;
    // PLIC outputs stay on sinks — OCA RTL uses the per-tile buserror/NMI path
    // (cpu_interrupts.adoc has no BEU PLIC source ID).
    sc_core::sc_vector<sc_core::sc_signal<bool>> beu_irq_local{"beu_irq_local", NUM_BEU};
    sc_core::sc_vector<sc_core::sc_signal<bool>> beu_irq_plic{"beu_irq_plic", NUM_BEU};
    // AVSBus interrupt (peripheral bit 22) + unused GPIO-enable output sink.
    sc_core::sc_signal<bool> avsbus_irq{"avsbus_irq"};
    sc_core::sc_signal<bool> axi_hang_irq{"axi_hang_irq"};
    sc_core::sc_signal<bool> avsbus_gpio_en{"avsbus_gpio_en"};
// memory_zeroer completion IRQ (docs: internal interrupt 3). Bound to a
// dummy sink for now; can later be routed into the PLIC/aggregator.
sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> sig_zeroer_irq{"sig_zeroer_irq"};

    // -----------------------------------------------------------------------
    // OCTS system timer signals
    //
    // Unlike every other peripheral here, octs_system_timer is a synchronous
    // (clock-driven) model: it advances exactly one RTL cycle per rising edge
    // of clk_i, so the platform has to supply a clock.  `octs_clk_period_ns_p_`
    // sets that period; 10 ns (100 MHz) gives the counter enough resolution to
    // move visibly within a firmware busy-wait, since the LT cluster charges a
    // notional 1 ns per retired instruction.
    //
    // By default (`octs_is_primary_p_`) the SMC instance is the system's
    // timekeeping PRIMARY, so it generates sync_load / cnt_credit for
    // downstream SECONDARY timers in other subsystems rather than consuming
    // them.  Both sync inputs are therefore tied low, and the outputs land on
    // sinks — nothing else in the SMC VP consumes them yet.  A test may strap
    // the timer SECONDARY, in which case it has no pulse source here and stays
    // parked; the real primary→secondary sync path is covered by the IP's own
    // unit testbench, which pairs a primary with a secondary.
    // -----------------------------------------------------------------------
    sc_core::sc_clock            octs_clk;
    sc_core::sc_signal<bool>     sig_octs_is_primary{"sig_octs_is_primary"};
    sc_core::sc_signal<bool>     sig_octs_sync_load_in{"sig_octs_sync_load_in"};
    sc_core::sc_signal<bool>     sig_octs_credit_in{"sig_octs_credit_in"};
    sc_core::sc_signal<bool>     sig_octs_sync_load_out{"sig_octs_sync_load_out"};
    sc_core::sc_signal<bool>     sig_octs_credit_out{"sig_octs_credit_out"};
    sc_core::sc_signal<uint64_t> sig_octs_count{"sig_octs_count"};
    sc_core::sc_signal<bool>     sig_octs_gpio_enable{"sig_octs_gpio_enable"};
    sc_core::sc_signal<uint32_t> sig_octs_cur_credits{"sig_octs_cur_credits"};
    sc_core::sc_signal<bool>     sig_octs_credits_left{"sig_octs_credits_left"};

    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_irq_sw{"sig_irq_sw", NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_irq_timer{"sig_irq_timer", NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> sig_irq_ext{"sig_irq_ext", NUM_PLIC_CTX};

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    SC_HAS_PROCESS(smc_platform);
    explicit smc_platform(sc_core::sc_module_name name);

private:
    // Forward external inbound targets to the fabric's target sockets.
    tlm_utils::simple_initiator_socket<smc_platform, 64> fwd_sys_{"fwd_sys_"};
    tlm_utils::simple_initiator_socket<smc_platform, 64> fwd_jtag_{"fwd_jtag_"};
    tlm_utils::simple_initiator_socket<smc_platform, 64> fwd_sep_{"fwd_sep_"};
    // The fabric's log_in / data_accel_in target sockets require an bound
    // initiator (their internal BW port); nothing drives them in the platform,
    // so bind idle initiators that never issue transactions.
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_log_init_{"idle_log_init_"};
    // In cluster-OFF builds there is no cluster.mmio to bind fabric.mmio_in, so
    // an idle initiator satisfies the fabric's BW port instead.
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_mmio_init_{"idle_mmio_init_"};
    // Each multi_stub_target above is always bound by one of these idle
    // initiators; the cluster-only initiator (out[1]/cluster.ifetch/cluster.data)
    // adds a second bind only in cluster mode.  No socket is ever left unbound
    // in either build mode.
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_ctrl_init_{"idle_ctrl_init_"};
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_ifetch_init_{"idle_ifetch_init_"};
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_data_init_{"idle_data_init_"};
    // Forward aou_axi_s (platform boundary) onto local aou_.axi_s[0].
    tlm_utils::simple_initiator_socket<smc_platform, 64> fwd_aou_{"fwd_aou_"};
    // aou_peer_ models the remote die's AOU core; its axi_s[0] would receive
    // traffic from the remote fabric, and its apb_socket would receive CSR
    // accesses from the remote local CPU — neither is modeled by this
    // single-chip platform. Bind idle initiators so the required ports are
    // satisfied.  Peer axi_m terminates on stub_sysmem (remote SMN stub).
    tlm_utils::simple_initiator_socket<smc_platform, 64> idle_aou_peer_init_{"idle_aou_peer_init_"};
    tlm_utils::simple_initiator_socket<smc_platform, 32> idle_aou_peer_apb_init_{"idle_aou_peer_apb_init_"};
    // Dual periph-bus cpu_ctrl bind removed; keep the IP elaboratable.
    tlm_utils::simple_initiator_socket<smc_platform, 32> idle_cpu_ctrl_init_{"idle_cpu_ctrl_init_"};

    // Tie-off for sep_mailbox_irq_i in integrations without a SEP above us.
    sc_core::sc_vector<sc_core::sc_signal<bool>>
        sep_mailbox_irq_tie_low_{"sep_mailbox_irq_tie_low_", NUM_SEP_MAILBOX};

    void before_end_of_elaboration() override;

    void fwd_sys_axi (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void fwd_jtag_axi(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void fwd_sep_axi (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void fwd_aou_axi (tlm::tlm_generic_payload&, sc_core::sc_time&);
};

}  // namespace smc
