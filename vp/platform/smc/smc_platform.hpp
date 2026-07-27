// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smc/smc_platform.hpp
//
// Top-level SMC SystemC/TLM-2.0 platform.  Instantiates the modeled blocks
// (fabric, reset unit, PLIC, CLINT, boot ROM, scratchpad, cpu_ctrl, I3C, 3x
// I2C, 4x UART, and optionally the Whisper-backed CPU cluster), wires the
// fabric's initiator sockets through address routers to the modeled targets
// and stubs, composes the peripheral interrupt vector into the PLIC, and —
// when the cluster is present — connects CLINT MSIP/MTIP and PLIC MEIP to the
// cluster's per-hart IRQ inputs.
//
// See smc/doc/systemc_tlm2_integration_guide.adoc for the address map and
// binding rationale.
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>

#include "sim_log.h"
#include "smc_axi_extension.h"
#include "smc_fabric.h"

#include "bootrom.h"
#include "clint.h"
#include "cpu_ctrl.h"
#include "i2c_controller.h"
#include "i3c_controller.h"
#include "memory_zeroer.h"
#include "plic.h"
#include "reset_unit.h"
#include "scratchpad_ram.h"
#include "uart.h"

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
    static constexpr unsigned NUM_UART      = 4;
    static constexpr unsigned NUM_I2C       = 3;
    static constexpr unsigned NUM_I3C       = 6;
    static constexpr unsigned NUM_PLIC_SRC   = 336;
    static constexpr unsigned NUM_PLIC_CTX   = 8;
    static constexpr unsigned NUM_SUBSYS     = 32;
    // 13 peripheral IRQ inputs: i3c[0..5], uart[0..3], i2c[0..2].
    static constexpr unsigned NUM_PERIPH_IRQ = NUM_I3C + NUM_UART + NUM_I2C;

    // -----------------------------------------------------------------------
    // External boundary (chiplet-facing).  Inbound masters are forwarded to the
    // fabric; outbound `fabric.output_axi` is bound to the internal
    // `stub_sysmem` target (a platform-level outbound initiator hook is left
    // for a later phase).
    // -----------------------------------------------------------------------
    tlm_utils::simple_target_socket<smc_platform, 64>   sys_axi_in{"sys_axi_in"};
    tlm_utils::simple_target_socket<smc_platform, 64>   jtag_axi_in{"jtag_axi_in"};
    tlm_utils::simple_target_socket<smc_platform, 64>   sep_axi_in{"sep_axi_in"};

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
    memory_zeroer   zeroer{"memory_zeroer"};
    i3c_controller  i3c{"i3c"};
    sc_core::sc_vector<i2c_controller> i2c{"i2c", NUM_I2C};
    sc_core::sc_vector<uart>           uart{"uart", NUM_UART};
#ifdef SMC_PLATFORM_WITH_CLUSTER
    smc_cpu_cluster cluster{"cluster"};
#endif

    // -----------------------------------------------------------------------
    // Routers and aggregator.
    //
    // front_port_router: 64-bit in (fabric.to_front_port + cluster.data) ->
    //   64-bit outputs.  The 32-bit front-port peripherals (boot ROM,
    //   scratchpad, PLIC, CLINT) sit behind `width_adapter<64,32>` instances
    //   because TLM simple sockets refuse cross-width binds.
    //
    // periph_router: 64-bit in (fabric.to_periph) -> 32-bit outputs (every
    //   peripheral on this bus is a 32-bit reg_socket).
    // -----------------------------------------------------------------------
    addr_router<64, 64>         front_port_router{"front_port_router", 7};
    addr_router<64, 32>         periph_router{"periph_router", 11};
    width_adapter<64, 32>       wa_bootrom{"wa_bootrom"};
    width_adapter<64, 32>       wa_scratch{"wa_scratch"};
    width_adapter<64, 32>       wa_plic{"wa_plic"};
    width_adapter<64, 32>       wa_clint{"wa_clint"};
    // memory_zeroer sits behind the fabric's data-accelerator ports:
    //  - CSR: `to_data_accel_ctrl` forwards the absolute 0xC003_82xx window; a
    //    1-entry addr_router rebases it to the IP's 0-based register offsets
    //    (and adapts 64->32).  A plain width_adapter would NOT rebase, so the
    //    IP would see 0xC003_8200 instead of offset 0 and reject every access.
    //  - DMA: the 32-bit initiator drives the 64-bit `data_accel_in` target;
    //    it issues absolute addresses the fabric re-decodes, so a verbatim
    //    width_adapter (no rebase) is correct here.
    addr_router<64, 32>         daccel_router{"daccel_router", 1};
    width_adapter<32, 64>       wa_zeroer_dma{"wa_zeroer_dma"};
    interrupt_aggregator        intagg;

    // -----------------------------------------------------------------------
    // Stubs for unmodeled / RTL-connected blocks
    // -----------------------------------------------------------------------
    stub_target<64> stub_wdt_debug{"stub_wdt_debug"};
    stub_target<64> stub_beu{"stub_beu"};
    stub_target<64> stub_dfd{"stub_dfd"};
    stub_target<64> stub_mbox{"stub_mbox"};
    stub_target<64> stub_dft{"stub_dft"};
    stub_target<64> stub_sysmem{"stub_sysmem"};
    stub_target<64> stub_cpu_ctrl_fab{"stub_cpu_ctrl_fab"};
    stub_target<64> stub_aR{"stub_aR"};
    stub_target<64> stub_mR{"stub_mR"};
    stub_target<64> stub_xR{"stub_xR"};
    stub_target<64> stub_ibf{"stub_ibf"};
    stub_target<64> stub_obf{"stub_obf"};
    stub_target<32> stub_periph_misc{"stub_periph_misc"};
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
    // I2C / PLIC / cluster IRQ signals
    // -----------------------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_signal<bool>> i2c_irq{"i2c_irq", NUM_I2C};
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic_src_sig{"plic_src_sig", NUM_PLIC_SRC};
    // Dummy sinks for every stub_target's irq_o (stubs don't drive the PLIC in
    // Phase 1; their irq_drive_method still requires a bound port).
    sc_core::sc_vector<sc_core::sc_signal<bool>> stub_irq_sig{"stub_irq_sig", 20};
    // memory_zeroer completion IRQ (docs: internal interrupt 3). Bound to a
    // dummy sink for now; can later be routed into the PLIC/aggregator.
    sc_core::sc_signal<bool> sig_zeroer_irq{"sig_zeroer_irq"};
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

    void fwd_sys_axi (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void fwd_jtag_axi(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void fwd_sep_axi (tlm::tlm_generic_payload&, sc_core::sc_time&);
};

}  // namespace smc
