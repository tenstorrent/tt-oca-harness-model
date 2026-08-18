// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/sep/sep_platform.hpp
//
// Top-level SEP SystemC/TLM-2.0 platform. Instantiates the modeled SEP
// blocks, the internal SimpleBus, and the chiplet-boundary AXI sockets.
// Implementation lives in src/sep_platform.cpp (same split as
// smc_platform.hpp / src/smc_platform.cpp).
//
// The SystemC module type remains `och_sep_ss` so existing instance names
// (`och_sep_ss1`) and CCI preset keys stay unchanged.
// ===========================================================================

#pragma once

#include <systemc.h>
#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <tlm_utils/simple_target_socket.h>
#include <tlm.h>
#include "sep_memory.h"
#include "sep_scratch_cold.h"
#include "sep_scratch_warm.h"
#include "sep_output_remap_ctrl.h"
#include "sep_filter_ctrl.h"
#include "sep_reset_ctrl.h"
#include "sep_cpu_ctrl.h"
#include "local_alias_remap.h"
#include "secure_dma.h"
#include "hmac.h"
#include "kmac.h"
#include "otbn.h"
#include "otbn_interfaces.h"
#include "spi_controller.h"
#include "spi_controller_interface.h"
#include "spi_flash.h"
#include "csrng.h"
#include "key_manager.h"
#include <openssl/rand.h>
#include <cstring>
#include "aes.h"
#include "mailbox.h"
#include "adapters.h"
#include "smc_global_port.h"
#include "stubs.h"
#include "xbar_policy.h"
#include "vp_devices.h"
#include "aon_timer.h"
#include "efuse.h"
#include "lifecycle_ctrl.h"
#include "entropy_src.h"
#include "edn.h"
#include "rv32/elf_loader.h"
#include "options.h"
#include "bus.h"
#include "HartConfig.hpp"
#include "VeeR-ISSTlm.hpp"
#include "Args.hpp"
#include "irq_map.h"

class och_sep_ss : public sc_module {

public:
    // VP bus topology
    // Initiators: riscv, dma_ot, dma_ctn, dma_sys_adapter, local_alias_remap_out,
    //             smn_remap_out (the SMU on-die inbound path re-enters the bus
    //             here once an external driver feeds sep_smn_inbound_axi)
    static constexpr unsigned int INIT_COUNT = 6;
    // Targets: sram, rom, dma, hmac, otbn, itcm, dtcm,
    //          stdout, spi, kmac, csrng, aes, mailbox, aon_timer, keymgr_mb,
    //          keymgr_kpvlp, efuse, lc_ctrl, entropy_src, edn,
    //          scratch_cold, scratch_warm,
    //          local_alias_remap_csr, local_alias_remap_data,
    //          ap_remap_csr, ap_remap_data, stee_remap_csr, stee_remap_data,
    //          outbound_filter_csr,
    //          inbound_filter_csr,
    //          smc_global, smu (-> outbound_filter_mux), spi_mux, reset_ctrl, cpu_ctrl
    //          (PIC is internal to VeeRISSTlm; SEP has no external PLIC/CLINT
    //          or SEP-side GPIO/AVBbus in real silicon — none of these were
    //          ever part of the register map, they were VP-only scaffolding)
    // Note: outbound_filter's data path has no standalone bus window — it is
    // fed by outbound_filter_mux (ap_output_remap + stee_output_remap +
    // SMU-window remapped_socket outputs), mirroring sep_system_peripherals.sv's
    // u_outbound_filter_mux (3 slv_reqs_i: AP-remapped, STEE-remapped, and the
    // raw SEP_EXT_TO_SMU leg) -> axi_filter_wrap chain (BlockByDefault=1).
    // inbound_filter's data path likewise has no standalone bus window — it
    // is fed by sep_smn_inbound_axi (the external-facing boundary port).
    // smc_global is a placeholder stub for u_axi_demux's SEP_EXT_TO_SMC leg
    // (sep_system_peripherals.sv) — no SMC model exists in this VP yet. The
    // SMU window has no stub of its own — it forwards straight into
    // outbound_filter_mux, since real RTL merges it with AP/STEE before the
    // Outbound Filter rather than terminating it locally.
    static constexpr unsigned int TARG_COUNT = 35;

    SC_HAS_PROCESS(och_sep_ss);

    // ------------------------------------------------------------------
    // Chiplet boundary (SMU on-die path; see doc/smc-sep-d2d-interconnect).
    //
    // sep_ext_to_smc_axi — SEP outbound master port for the SMC global
    //   window (RTL: sep_ext_to_smc_axi).  Carries global addresses; the
    //   SMU platform routes it through the local-alias window remap to the
    //   SMC's dedicated sep_axi_in.  Standalone sep-vp binds a stub here.
    //
    // sep_smn_inbound_axi — inbound target port from the SMU crossbar
    //   (RTL: smn_inbound_axi).  Feeds the RTL-ordered inbound chain
    //   (inbound_filter -> global->local window remap -> internal bus); see
    //   smn_inbound_b_transport.  Standalone sep-vp binds an idle initiator
    //   here.
    //
    // sep_smn_outbound_axi — general outbound master port (RTL:
    //   smn_outbound_axi).  Carries everything the outbound filter passes:
    //   AP/STEE remapped traffic merged by outbound_filter_mux.  Egress is
    //   therefore observable at the boundary rather than disappearing into a
    //   sink, which is what lets the SMU crossbar see SEP as a master.  The
    //   internal chain is 32-bit and the boundary is 64-bit, so
    //   outbound_filter_to_smn relays between them (mirror of the inbound
    //   relay below).  Standalone sep-vp binds a sink here.
    //
    // sep_ext_to_smc_axi must be a plain tlm_initiator_socket: it is the
    // parent of a hierarchical initiator bind (smc_global->init64), and a
    // simple_* socket's internal sc_export is already bound to its own fw
    // process at construction, so a simple_* parent fails elaboration (E126).
    // sep_smn_outbound_axi is driven by och_sep_ss itself, not hierarchically
    // bound, so it can be a simple_* socket.
    // ------------------------------------------------------------------
    tlm::tlm_initiator_socket<64>                     sep_ext_to_smc_axi{"sep_ext_to_smc_axi"};
    tlm_utils::simple_target_socket<och_sep_ss, 64>    sep_smn_inbound_axi{"sep_smn_inbound_axi"};
    tlm_utils::simple_initiator_socket<och_sep_ss, 64> sep_smn_outbound_axi{"sep_smn_outbound_axi"};

    // Inbound-window exports (sep.sv: sep_global_base_addr_o / sep_region_size_o),
    // driven by cpu_ctrl straight from its CSRs.  An enclosing platform sizes its
    // SEP aperture from these rather than from a preset of its own, so the two
    // cannot disagree about where the window is.  Plain signals rather than
    // sc_out because standalone sep-vp has nothing above it to bind to and an
    // unbound sc_out fails elaboration.  Single-writer on purpose: cpu_ctrl's
    // publish process is the only driver, so a second one is caught here.
    sc_signal<uint64_t> sep_global_base_addr_signal{"sep_global_base_addr_signal"};
    sc_signal<uint64_t> sep_region_size_signal{"sep_region_size_signal"};

    och_sep_ss(sc_module_name name, BasicOptions& opt_in);
    explicit och_sep_ss(sc_module_name name);
    ~och_sep_ss();

private:
    BasicOptions opt;

    // =========================================================================
    // Peripheral models
    // =========================================================================
    SEPMemory*                        sram               = nullptr;
    SEPMemory*                        rom                = nullptr;
    SEPMemory*                        itcm               = nullptr;
    SEPMemory*                        dtcm               = nullptr;
    sep_scratch_cold_ip*              scratch_cold         = nullptr;
    sep_scratch_warm_ip*              scratch_warm         = nullptr;
    sep_output_remap_ctrl_ip*         ap_output_remap      = nullptr;
    sep_output_remap_ctrl_ip*         stee_output_remap    = nullptr;
    outbound_filter_mux*              outbound_mux         = nullptr;
    sep_filter_ctrl_ip*               outbound_filter      = nullptr;
    sep_filter_ctrl_ip*               inbound_filter       = nullptr;
    smn_inbound_remap_adapter*        smn_remap            = nullptr;
    // SMC window: RW fallback stub in standalone sep-vp, or forwarder to the
    // real SMC when `smc_global.forward_en` is set (see inc/smc_global_port.h).
    sep_smc_global_port*              smc_global           = nullptr;
    SEPMemory*                        spi_mux              = nullptr;  // functional stub (RW) — spi_mux_ctrl
    sep_reset_ctrl_ip*                reset_ctrl           = nullptr;
    sep_cpu_ctrl_ip*                  cpu_ctrl             = nullptr;
    local_alias_remap_ip*             local_alias_remap    = nullptr;
    local_alias_remap_adapter*        local_alias_fixed_adapter = nullptr;
    stdout_device*                    stdout_dev         = nullptr;
    secure_dma_model*                 dma                = nullptr;
    dma_sys_bus_adapter*              dma_sys_adapter    = nullptr;
    hmac_ip*                          hmac               = nullptr;
    kmac_ip*                          kmac               = nullptr;
    otbn_ip*                          otbn               = nullptr;
    otp_key_req_stub*                 otp_key_req_stub_inst = nullptr;
    csrng_model*                      csrng              = nullptr;
    aes_model*                        aes                = nullptr;
    mailbox_ip*                       mailbox            = nullptr;
    mailbox_host_stub*                mbox_host          = nullptr;
    MailboxBridge*                    mbox_bridge        = nullptr;
    aon_timer_ip*                     aon_timer          = nullptr;
    efuse_model*                      sep_efuse          = nullptr;
    lifecycle_ctrl_model*             lc_ctrl            = nullptr;
    entropy_src_ip*                   entropy_src        = nullptr;
    edn_ip*                           edn                = nullptr;
    spi_flash*                        spi_device         = nullptr;
    spi_controller_ip*                spi_controller     = nullptr;
    key_manager_model*                keymgr             = nullptr;

    // =========================================================================
    // Platform infrastructure
    // =========================================================================
    VeeRISSTlm*                                riscv     = nullptr;  // created after ELF load
    SimpleBus<INIT_COUNT, TARG_COUNT>*         bus       = nullptr;
    reset_generation_unit*                     rsu_module = nullptr;

    // =========================================================================
    // Signals — reset + per-IP clocks, interrupts, alerts
    // =========================================================================
    sc_signal<bool, SC_MANY_WRITERS> reset_signal;

    // Per-IP software reset signals driven by sep_reset_ctrl_ip
    // (AND of global reset_signal with SW_RESET_N register bits)
    sc_signal<bool> km_sw_rst_n_signal;
    sc_signal<bool> otbn_sw_rst_n_signal;
    sc_signal<bool> aes_sw_rst_n_signal;
    sc_signal<bool> hmac_sw_rst_n_signal;
    sc_signal<bool> kmac_sw_rst_n_signal;

    // SPI
    sc_signal<bool, SC_MANY_WRITERS> spi_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_error_irq_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_event_irq_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_dma_trigger_signal;

    // HMAC
    sc_signal<double, SC_MANY_WRITERS> hmac_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_fifo_empty_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_err_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_alert_signal;

    // KMAC
    sc_signal<bool, SC_MANY_WRITERS> kmac_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_idle_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_intr_signal;

    // OTBN
    sc_signal<double, SC_MANY_WRITERS> otbn_clk_core_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_intr_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_alert_fatal_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_alert_recov_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_escalate_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_escalate_rsp_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_rma_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_rma_rsp_signal;

    // CSRNG
    sc_signal<bool, SC_MANY_WRITERS> csrng_clk_signal;
    sc_signal<uint8_t>               csrng_otp_en_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_cmd_req_done_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_entropy_req_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_hw_inst_exc_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_fatal_err_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_recov_alert_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_fatal_alert_signal;

    // AES
    sc_signal<bool, SC_MANY_WRITERS> aes_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_idle_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_alert_recov_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_alert_fatal_signal;

    // DMA
    sc_signal<bool, SC_MANY_WRITERS>   dma_done_intr_sig;
    sc_signal<bool, SC_MANY_WRITERS>   dma_chunk_done_intr_sig;
    sc_signal<bool, SC_MANY_WRITERS>   dma_error_intr_sig;
    sc_signal<sc_core::sc_time>        dma_clk_signal;
    sc_signal<bool>                    dma_lsio_trigger[11];
    sc_signal<bool>                    dma_alert_fatal_sig;

    // Mailbox
    sc_signal<double, SC_MANY_WRITERS> mbox_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   mbox_irq0_signal;
    sc_signal<bool, SC_MANY_WRITERS>   mbox_irq1_signal;

    // AON Timer
    sc_signal<double, SC_MANY_WRITERS> aon_clk_aon_freq_signal;
    sc_signal<double, SC_MANY_WRITERS> aon_clk_sys_freq_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_sleep_mode_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_intr_wkup_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_intr_bark_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_nmi_bark_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_wkup_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_rst_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_fatal_fault_signal;
    sc_signal<uint32_t>                aon_racl_policies_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_racl_error_signal;
    // NMI address — written by stdout_device (LOAD_NMI_ADDR) or sep_cpu_ctrl
    sc_signal<uint32_t, SC_MANY_WRITERS> nmi_vec_signal;

    // KeyMgr
    sc_signal<bool, SC_MANY_WRITERS> keymgr_wipe_ni_signal;
    sc_signal<bool, SC_MANY_WRITERS> keymgr_irq_signal;

    // Entropy Source
    sc_signal<bool, SC_MANY_WRITERS> entropy_src_irq_signal;

    // EDN
    sc_signal<double, SC_MANY_WRITERS> edn_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_cmd_req_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_fatal_err_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_recov_alert_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_fatal_alert_signal;

    // PIC input table — unused slots tied to unused_irq_signal
    sc_signal<bool, SC_MANY_WRITERS>               unused_irq_signal;
    std::vector<sc_signal<bool, SC_MANY_WRITERS>*> pic_inputs;

    // =========================================================================
    // Config / parameters
    // =========================================================================
    WdRiscv::HartConfig    config;
    ArgsCSML*              argsCSML      = nullptr;
    csml_param<uint64_t>   globalQuantumNs;

    // Boot straps latched by the (emulated) SMC reset unit, exposed as CCI params
    // so the ROM boot mode is selectable at invocation (accellera_config.ini) with
    // no rebuild. The constructor composes them into the STRAPS_LO/STRAPS_HI
    // words (sep_smc_interface.h). Defaults: Secondary chiplet, status reporting
    // enabled, refclk.
    csml_param<bool>       strap_primary_chiplet;       // STRAPS_LO[25]
    csml_param<bool>       strap_boot_recovery;         // STRAPS_LO[19]
    csml_param<bool>       strap_rotate_update;         // STRAPS_HI[26]
    csml_param<bool>       strap_status_report_disable; // STRAPS_LO[21]
    csml_param<bool>       strap_bl0_pll_clk;           // STRAPS_LO[20]

    // Optional SPI flash preload: path to a Verilog $readmemh-style hex file
    // (e.g. fw/sep/bootcode/prebuilt/non_secure_boot.spi_preload) — "@addr" lines
    // set the write cursor, subsequent lines are whitespace-separated hex byte
    // pairs. Parsed directly into spi_flash's backing memory in
    // start_of_simulation via spi_flash_model::write_byte(); no conversion to a
    // raw .bin is needed. Empty (default) falls back to the existing
    // data/flash_memory.bin raw-binary backdoor load.
    csml_param<std::string> spiPreload;
    std::string              spiPreloadPath;

    // Relays sep_smn_inbound_axi (external-facing, 64-bit) into the 32-bit
    // internal inbound chain at inbound_filter->data_socket.
    tlm_utils::simple_initiator_socket<och_sep_ss> smn_inbound_to_filter;

    // Mirror of the above for egress: receives the 32-bit outbound-filter
    // output and relays it out on the 64-bit sep_smn_outbound_axi boundary.
    tlm_utils::simple_target_socket<och_sep_ss> outbound_filter_to_smn;

    // Value seeded into CPU_CTRL.SEP_GLOBAL_BASE_ADDR, which the inbound window
    // remap reads (see smn_inbound_remap_adapter).  On silicon the CSR resets to
    // 0 and SEP firmware programs it, which is what the default preserves;
    // platforms that drive inbound traffic without running that firmware
    // (smu-vp) preset this to match the SMU crossbar's own sep_global_base.
    csml_param<uint64_t>   sep_global_base;

    // Value seeded into CPU_CTRL.SEP_REGION_SIZE, the other half of the inbound
    // window the remap checks. The CSR resets to 16 MiB, so an inbound aperture
    // wider than that (smu-vp routes 512 MiB) needs the CSR widened to match, or
    // hits above the reset window would pass through unremapped and land on the
    // internal bus as a global address. 0 keeps the reset value.
    csml_param<uint64_t>   sep_region_size;

    // RTL's smc_global_base_addr_i / smc_region_size_i: the SEP_EXT_TO_SMC demux
    // window, defined by the SMC and not by anything inside SEP.  Modelled as
    // config rather than signals because no VP SMC model publishes them yet;
    // the defaults reproduce the window the static Args constants described.
    // An enclosing platform must keep these in step with whatever it puts on
    // sep_ext_to_smc_axi, or SEP will gate off part of the SMC it can reach.
    csml_param<uint64_t>   smc_global_base;
    csml_param<uint64_t>   smc_region_size;

    // feat_ctrl_o.sep_debug (sep.sv:952) — drives the inbound filter's
    // filter_skip_i, bypassing all match/permission checking. The inbound filter
    // is BlockByDefault, so a platform whose masters issue inbound traffic
    // without programming the filter tables must raise this, exactly as SEP
    // debug mode does on silicon. Defaults false (fail-closed, as on reset).
    csml_param<bool>                 sep_debug;
    sc_signal<bool, SC_MANY_WRITERS> sep_debug_signal;

    // =========================================================================
    // Methods
    // =========================================================================
    void create_modules();
    void module_bind();
    void start_of_simulation() override;
    void seed_inbound_window();
    void smn_inbound_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int smn_inbound_transport_dbg(tlm::tlm_generic_payload& trans);
    void smn_outbound_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int smn_outbound_transport_dbg(tlm::tlm_generic_payload& trans);
};

