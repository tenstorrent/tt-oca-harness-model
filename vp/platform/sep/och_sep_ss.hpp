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

namespace {
    // Default options for och_sep_ss(sc_module_name) — delegates to two-arg ctor.
    inline BasicOptions& och_sep_ss_default_options_ref() {
        static BasicOptions o;
        return o;
    }

    // SEP status ring buffer placement in the smc_global window, used by the
    // SMC-handshake seed block. The bus strips the 0x40000000 base, so these are
    // window-local offsets. Layout: { u32 head, tail, num_entries; u32 entries[] }.
    constexpr uint32_t SMC_SRAM_WINDOW_OFF  = 0x60000;  // SMC_SRAM_OFFSET (window-local)
    constexpr uint32_t STATUS_RING_SRAM_OFF = 0x1000;   // SRAM-relative; published in scratch[11]
    constexpr uint32_t STATUS_RING_ENTRIES  = 512;      // capacity (real SMC_RING_BUFFER_SIZE)
    constexpr uint64_t STATUS_RING_LOCAL    = static_cast<uint64_t>(SMC_SRAM_WINDOW_OFF) +
                                              STATUS_RING_SRAM_OFF;            // 0x61000 (header)
} // namespace

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
    // sep_ext_to_smc_axi must be a plain tlm_initiator_socket: it is the
    // parent of a hierarchical initiator bind (smc_global->init64), and a
    // simple_* socket's internal sc_export is already bound to its own fw
    // process at construction, so a simple_* parent fails elaboration (E126).
    // ------------------------------------------------------------------
    tlm::tlm_initiator_socket<64>                     sep_ext_to_smc_axi{"sep_ext_to_smc_axi"};
    tlm_utils::simple_target_socket<och_sep_ss, 64>    sep_smn_inbound_axi{"sep_smn_inbound_axi"};

    och_sep_ss(sc_module_name name, BasicOptions& opt_in);
    och_sep_ss(sc_module_name name)
        : och_sep_ss(name, och_sep_ss_default_options_ref()) {}

    ~och_sep_ss() {
        // riscv first — ISS may have threads; stop before freeing other resources
        delete riscv;
        delete sram;
        delete rom;
        delete itcm;
        delete dtcm;
        delete stdout_dev;
        delete dma;
        delete dma_sys_adapter;
        delete hmac;
        delete kmac;
        delete otbn;
        delete otp_key_req_stub_inst;
        delete csrng;
        delete aes;
        delete mailbox;
        delete mbox_host;
        delete mbox_bridge;
        delete aon_timer;
        delete sep_efuse;
        delete lc_ctrl;
        delete entropy_src;
        delete edn;
        delete scratch_cold;
        delete scratch_warm;
        delete ap_output_remap;
        delete stee_output_remap;
        delete outbound_mux;
        delete outbound_filter;
        delete inbound_filter;
        delete smn_remap;
        delete outbound_filter_stub;
        delete smc_global;
        delete spi_mux;
        delete reset_ctrl;
        delete cpu_ctrl;
        delete local_alias_remap;
        delete local_alias_fixed_adapter;
        delete spi_device;
        delete spi_controller;
        delete keymgr;
        delete rsu_module;
        if (bus) {
            for (auto* p : bus->ports)
                delete p;
            delete bus;
        }
        delete argsCSML;
    }

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
    filter_output_stub*               outbound_filter_stub = nullptr;
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
};

// -----------------------------------------------------------------------------
// Constructor implementation
// -----------------------------------------------------------------------------
inline och_sep_ss::och_sep_ss(sc_module_name name, BasicOptions& opt_in)
    : sc_module(name)
    , opt(opt_in)
    , unused_irq_signal("unused_irq_signal")
    , pic_inputs(PIC_NUM_INTERRUPTS, &unused_irq_signal)
    , globalQuantumNs("globalQuantumNs", 10)
    , strap_primary_chiplet("smc.primary_chiplet", false)
    , strap_boot_recovery("smc.boot_recovery", false)
    , strap_rotate_update("smc.rotate_update", false)
    , strap_status_report_disable("smc.status_report_disable", false)
    , strap_bl0_pll_clk("smc.bl0_pll_clk", false)
    , spiPreload("spiPreload", "")
    , smn_inbound_to_filter("smn_inbound_to_filter")
    , sep_global_base("sep_global_base", 0x0ULL)
    , sep_region_size("sep_region_size", 0x0ULL)
    , sep_debug("sep_debug", false)
    , sep_debug_signal("sep_debug_signal")
{
    sep_smn_inbound_axi.register_b_transport(this, &och_sep_ss::smn_inbound_b_transport);
    sep_smn_inbound_axi.register_transport_dbg(this, &och_sep_ss::smn_inbound_transport_dbg);

    argsCSML = new ArgsCSML(opt);
    Args& args = opt;

    if (not parseArgs(args))
        return;
    if (args.help or args.version)
        return;

    args.expandTargets();

    if (not args.configFile.empty()) {
        std::filesystem::path configPath(args.configFile);
        if (configPath.is_relative()) {
            if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                configPath = (std::filesystem::path(baseDir) / configPath).lexically_normal();
            else
                configPath = std::filesystem::absolute(configPath).lexically_normal();
            args.configFile = configPath.string();
        }
        std::cout << "Veer ISS --configFile option set to:" << args.configFile << std::endl;
        if (not config.loadConfigFile(args.configFile))
            return;
    }

    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(static_cast<double>(globalQuantumNs.get_param_value()),
                         sc_core::sc_time_unit::SC_NS));

    create_modules();

    // Load ELF — riscv is created after because it needs the entry point.
    auto& target = args.expandedTargets;
    if (args.expandedTargets.empty())
        throw std::runtime_error("No ELF file specified");

    auto elfFile = target.front().front();
    {
        std::filesystem::path elfPath(elfFile);
        if (elfPath.is_relative()) {
            if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                elfPath = (std::filesystem::path(baseDir) / elfPath).lexically_normal();
            else
                elfPath = std::filesystem::absolute(elfPath).lexically_normal();
            elfFile = elfPath.string();
            target.front().front() = elfFile;
        }
    }

    spiPreloadPath = spiPreload.get_param_value();
    if (not spiPreloadPath.empty()) {
        std::filesystem::path preloadPath(spiPreloadPath);
        if (preloadPath.is_relative()) {
            if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                preloadPath = (std::filesystem::path(baseDir) / preloadPath).lexically_normal();
            else
                preloadPath = std::filesystem::absolute(preloadPath).lexically_normal();
            spiPreloadPath = preloadPath.string();
        }
    }
    if (args.verbose)
        std::cerr << "Loading ELF file " << elfFile << '\n';

    if (elfFile.find("rom_sanity_test") != std::string::npos) {
        // Pre-initialise ROM with a deterministic pattern expected by rom_sanity_test.c.
        // Bus strips base address before calling SEPMemory, so preload at offset 0.
        constexpr uint64_t ROM_PATTERN_MAGIC = 0xDEAD0000ULL;
        constexpr uint32_t TEST_ENTRIES = 64;
        std::vector<uint64_t> rom_init(TEST_ENTRIES);
        for (uint32_t i = 0; i < TEST_ENTRIES; i++)
            rom_init[i] = (static_cast<uint64_t>(i) << 32) | ROM_PATTERN_MAGIC | (i & 0xFFFFU);
        rom->load_data(reinterpret_cast<const char*>(rom_init.data()), 0, rom_init.size() * sizeof(uint64_t));
    }

    rv32::ELFLoader loader(elfFile.c_str());
    size_t entry_point = loader.get_entrypoint();

    riscv = new VeeRISSTlm("rv31imc", args, config, entry_point);

    if (opt.entry_point.available)
        entry_point = opt.entry_point.value;
    try {
        // Boot ROM: code (.text/.metadata) is linked at 0x10040000. The ISS fetches
        // all instructions via TLM callbacks, so populating the read-only `rom` model
        // here makes boot-from-ROM work. load_executable_image filters PT_LOAD segments
        // by address range, so only ROM-resident segments land here; load_data bypasses
        // the read-only flag for one-time init. No-op for ELFs without a ROM segment.
        //
        // Load the ROM by *physical* address (use_vaddr=false): the self-contained boot
        // ROM image links .data into DCCM (VMA) but stores its init image at an LMA in ROM
        // (rom.ld `AT> rom`), and vector.S copies ROM->DCCM at boot. Selecting segments by
        // LMA places that .data init image at its ROM load address so the copy reads real
        // data; a VMA-based load would leave it in DCCM only and the ROM copy-source blank,
        // zeroing g_data_init and tripping the rom_main runtime-init check (0xB001).
        loader.load_executable_image(*rom,  opt.rom_size,   opt.rom_start_addr, /*use_vaddr=*/false);
        loader.load_executable_image(*itcm, 0x20000, opt.itcm_start_addr);
        loader.load_executable_image(*dtcm, 0x10000, opt.dtcm_start_addr);
        loader.load_executable_image(*sram, opt.sram_size, opt.sram_start_addr);
    } catch (rv32::ELFLoader::load_executable_exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Memory map: " << std::endl;
        return;
    }

    // -------------------------------------------------------------------------
    // Seed the SMC->SEP boot handshake into the smc_global stub.
    //
    // The boot ROM coordinates with the SMC through the CPU_CTRL scratch
    // registers and a status-report ring buffer in SMC SRAM. On real silicon the
    // SMC firmware stages these before releasing SEP from reset; the VP models no
    // SMC, so we present the same contract here. Without it the ROM hangs in
    // init_status_reporting() spinning on STATUS_TO_SEP.BUFFER_READY
    // (fw/sep/bootcode/src/status_ring.c) and never reaches manifest load.
    //
    // Offsets below are smc_global-local (the bus strips the 0x40000000 base).
    // Layout mirrors fw/sep/bootcode/include/sep_smc_interface.h:
    //   scratch[i] @ SMC_SCRATCH_BASE_OFFSET(0x10100) + (i << 3)
    //   SMC SRAM   @ SMC_SRAM_OFFSET(0x60000), 1 MiB; scratch offsets are SRAM-relative
    //
    // In forward mode (SMU platform) the window is backed by the real SMC, and
    // the SMC firmware owns staging this handshake (the silicon contract); the
    // VP does not second-guess it here.
    if (smc_global->forwarding()) {
        std::cout << "och_sep_ss: smc_global window forwards to the SMC (SMU mode); "
                     "boot-handshake seeding is owned by the SMC firmware" << std::endl;
    } else {
        constexpr uint64_t SMC_SCRATCH_BASE = 0x10100;   // SMC_SCRATCH_BASE_OFFSET
        auto scratch_local = [](unsigned idx) -> uint64_t {
            return SMC_SCRATCH_BASE + (static_cast<uint64_t>(idx) << 3);
        };

        // SMC SRAM regions staged by the (emulated) SMC, as SRAM-relative offsets. The
        // status ring placement is the shared STATUS_RING_* constants (see top-of-file).
        const uint32_t status_ring_off = STATUS_RING_SRAM_OFF;  // -> scratch[11] STATUS_BUFFER_ADDR
        const uint32_t manifest_off    = 0x2000;                // -> scratch[8]  MANIFEST_ADDR

        // scratch[9] STATUS_TO_SEP: all ready flags
        // (SRAM_INIT|MANIFEST_READY|BUFFER_READY|SRAM_PROTECTED = 0xF). BUFFER_READY
        // unblocks init_status_reporting(); MANIFEST_READY unblocks the SMC
        // coordination probe and manifest-load retry loop.
        const uint32_t status_to_sep = 0xFu;
        smc_global->load_data(reinterpret_cast<const char*>(&status_to_sep),
                              scratch_local(9),  sizeof(status_to_sep));
        smc_global->load_data(reinterpret_cast<const char*>(&status_ring_off),
                              scratch_local(11), sizeof(status_ring_off));
        smc_global->load_data(reinterpret_cast<const char*>(&manifest_off),
                              scratch_local(8),  sizeof(manifest_off));

        // Valid (empty) status_ring_buffer at the shared STATUS_RING_LOCAL offset:
        //   struct { uint32_t head, tail, num_entries; uint32_t entries[]; }
        // head=tail=0; num_entries=STATUS_RING_ENTRIES (512 — the real SMC_RING_BUFFER_SIZE,
        // large enough that a default-config boot's full status stream is captured without
        // the firmware substituting overflow markers). entries[] read back as 0 (paged mem).
        const uint32_t ring_hdr[3] = { 0u, 0u, STATUS_RING_ENTRIES };  // head, tail, num_entries
        smc_global->load_data(reinterpret_cast<const char*>(ring_hdr),
                              STATUS_RING_LOCAL, sizeof(ring_hdr));

        // DFT_CTRL_STATUS_SMU (SMC reg @ 0xF800): memory-repair / MBIST status. The ROM's
        // dft_mem_repair_gate() (rom_main.c) halts with ROM_ERR_DFT_GATE_BLOCKED unless
        // MEM_REPAIR_SUCCESS (bit 1) is set. Present REPAIR_DONE|REPAIR_SUCCESS (0x3) — the
        // (emulated) SMC reports a clean memory-repair pass.
        constexpr uint64_t SMC_DFT_CTRL_STATUS = 0xF800;  // SMC_DFT_CTRL_STATUS_SMU_OFFSET
        const uint32_t dft_status = 0x3u;  // MEM_REPAIR_DONE | MEM_REPAIR_SUCCESS
        smc_global->load_data(reinterpret_cast<const char*>(&dft_status),
                              SMC_DFT_CTRL_STATUS, sizeof(dft_status));

        // Reset-unit latched boot straps (STRAPS_LO/HI), composed from CCI params so
        // the ROM boot mode is invocation-selectable. init_straps() reads these
        // (fw/sep/bootcode/src/boot_straps.c): STRAPS_LO[25]=primary_chiplet selects
        // Primary (SPI boot) vs Secondary (wait for SMC manifest). Bit positions per
        // sep_smc_interface.h; HI bits are relative to the STRAPS_HI word.
        constexpr uint64_t SMC_STRAPS_LO = 0x2090;  // SMC_STRAPS_LO_OFFSET
        constexpr uint64_t SMC_STRAPS_HI = 0x2094;  // SMC_STRAPS_HI_OFFSET
        uint32_t straps_lo = 0u;
        uint32_t straps_hi = 0u;
        // Bit positions per fw/sep/bootcode/include/sep_smc_interface.h. The SEP
        // GPIO/strap reorg moved boot_recovery (HI[23]->LO[19]), bl0_pll_clk
        // (HI[24]->LO[20]) and rotate_update (HI[29]->HI[26]); keep these in lockstep
        // with the SMC strap IDs the bootcode reads.
        if (strap_primary_chiplet.get_param_value())       straps_lo |= (1u << 25);
        if (strap_status_report_disable.get_param_value()) straps_lo |= (1u << 21);
        if (strap_boot_recovery.get_param_value())         straps_lo |= (1u << 19);
        if (strap_bl0_pll_clk.get_param_value())           straps_lo |= (1u << 20);
        if (strap_rotate_update.get_param_value())         straps_hi |= (1u << 26);
        smc_global->load_data(reinterpret_cast<const char*>(&straps_lo),
                              SMC_STRAPS_LO, sizeof(straps_lo));
        smc_global->load_data(reinterpret_cast<const char*>(&straps_hi),
                              SMC_STRAPS_HI, sizeof(straps_hi));
    }  // end smc_global seed (fallback mode only)

    // Seed the SPI mux control register reset default. OCH_SEP_SPI_MUX_CTRL resets with
    // cs_force_high=1 (bit 1), spi_sel=0 (bit 0) -> 0x00000002. The offset is window-local
    // (the bus strips the 0x20001000 base). The driver overwrites this before relying on it;
    // seeding just makes the pre-write read-back match silicon reset. load_data writes the
    // backing store directly (not via b_transport), so no observation tap is involved.
    {
        const uint32_t spi_mux_reset = 0x00000002u;
        spi_mux->load_data(reinterpret_cast<const char*>(&spi_mux_reset),
                           0x0, sizeof(spi_mux_reset));
    }

    unused_irq_signal.write(false);
    module_bind();

    SC_THREAD(seed_inbound_window);
}

// -----------------------------------------------------------------------------
// seed_inbound_window — apply the inbound-window presets to the live CSRs
//
// The inbound window remap reads CPU_CTRL.SEP_GLOBAL_BASE_ADDR (resets to 0) and
// CPU_CTRL.SEP_REGION_SIZE (resets to 16 MiB); SEP firmware programs both on
// silicon. Platforms driving inbound traffic without that firmware preset them
// instead, and both must agree with the aperture the upstream interconnect
// routes here. The writes must land after cpu_ctrl's reset_handler has made its
// initialization pass, or reset_all_registers() would clear them again — hence
// the zero-time wait rather than a poke from the constructor or
// start_of_simulation.
// -----------------------------------------------------------------------------
inline void och_sep_ss::seed_inbound_window() {
    sc_core::wait(sc_core::SC_ZERO_TIME);
    const uint64_t base = sep_global_base.get_param_value();
    if (base != 0) {
        cpu_ctrl->SEP_GLOBAL_BASE_ADDR = base;
        std::cout << "och_sep_ss: SEP_GLOBAL_BASE_ADDR seeded to 0x" << std::hex
                  << base << std::dec << " from the sep_global_base preset"
                  << std::endl;
    }
    const uint64_t size = sep_region_size.get_param_value();
    if (size != 0) {
        cpu_ctrl->SEP_REGION_SIZE = size;
        std::cout << "och_sep_ss: SEP_REGION_SIZE seeded to 0x" << std::hex
                  << size << std::dec << " from the sep_region_size preset"
                  << std::endl;
    }
}

// -----------------------------------------------------------------------------
// create_modules — instantiate all peripheral and infrastructure modules
// -----------------------------------------------------------------------------
inline void och_sep_ss::create_modules() {
    sram            = new SEPMemory("sram", false);
    rom             = new SEPMemory("rom", true);
    itcm            = new SEPMemory("itcm", false);
    dtcm            = new SEPMemory("dtcm", false);
    scratch_cold         = new sep_scratch_cold_ip("scratch_cold");
    scratch_warm         = new sep_scratch_warm_ip("scratch_warm");
    // AP and STEE output remap — same model, instance type carries hardware constants
    ap_output_remap      = new sep_output_remap_ctrl_ip("ap_output_remap",   sep_output_remap_ctrl_ip::InstanceType::AP);
    stee_output_remap    = new sep_output_remap_ctrl_ip("stee_output_remap", sep_output_remap_ctrl_ip::InstanceType::STEE);
    // Merges AP/STEE remapped output onto one wire into outbound_filter,
    // mirroring sep_system_peripherals.sv's u_outbound_filter_mux (axi_mux).
    outbound_mux         = new outbound_filter_mux("outbound_filter_mux");
    // Outbound filter (32 entries) + inbound filter (16 entries)
    outbound_filter      = new sep_filter_ctrl_ip("outbound_filter", sep_filter_ctrl_ip::InstanceType::OUTBOUND);
    inbound_filter       = new sep_filter_ctrl_ip("inbound_filter",  sep_filter_ctrl_ip::InstanceType::INBOUND);
    outbound_filter_stub = new filter_output_stub("outbound_filter_stub");
    reset_ctrl           = new sep_reset_ctrl_ip("reset_ctrl");
    cpu_ctrl             = new sep_cpu_ctrl_ip("cpu_ctrl");
    local_alias_remap    = new local_alias_remap_ip("local_alias_remap");
    // Fixed remapper (axi_local_alias_remap.sv) — reads cpu_ctrl's live
    // SEP_LOCAL_BASE_ADDR/SEP_REGION_SIZE so firmware changes to those CSRs
    // take effect immediately, unlike the bus's static PortMapping subtraction.
    local_alias_fixed_adapter = new local_alias_remap_adapter(
        "local_alias_fixed_adapter", cpu_ctrl, opt.local_alias_remap_data_start_addr);
    // SMN inbound remapper (u_inbound_global_to_local_addr_remap) — reads
    // cpu_ctrl's live SEP_GLOBAL_BASE_ADDR + shared SEP_REGION_SIZE.
    // Downstream of inbound_filter; feeds back into the SEP bus as a new
    // initiator once an external (SMC/AP) driver binds sep_smn_inbound_axi.
    smn_remap = new smn_inbound_remap_adapter("smn_remap", cpu_ctrl);
    // SMC global window (SEP<->SMC dedicated AXI path): RW fallback stub
    // pre-seeded with the boot handshake the real SMC firmware stages before
    // releasing SEP from reset, or a forwarder to the SMU platform when
    // `smc_global.forward_en` is set (see inc/smc_global_port.h).
    smc_global      = new sep_smc_global_port("smc_global");
    // SPI mux control register stub (OCH_SEP_SPI_MUX_CTRL); RW backing store, no mux behavior.
    spi_mux         = new SEPMemory("spi_mux", false);
    stdout_dev      = new stdout_device("stdout");
    dma             = new secure_dma_model("dma");
    dma_sys_adapter = new dma_sys_bus_adapter("dma_sys_adapter");
    hmac            = new hmac_ip("hmac");
    kmac            = new kmac_ip("kmac");
    otbn            = new otbn_ip("otbn", 0x10000);
    // otp_key_rsp stub not needed — sc_export bound internally by OTBN model
    otp_key_req_stub_inst = new otp_key_req_stub("otp_key_req_stub");
    csrng           = new csrng_model("csrng");
    aes             = new aes_model("aes");
    mailbox         = new mailbox_ip("mailbox_ip");
    mbox_host       = new mailbox_host_stub("mailbox_host_stub");
    mbox_bridge     = new MailboxBridge("mailbox_bridge");
    aon_timer       = new aon_timer_ip("aon_timer");
    sep_efuse       = new efuse_model("sep_efuse");
    lc_ctrl         = new lifecycle_ctrl_model("lc_ctrl");
    entropy_src     = new entropy_src_ip("entropy_src");
    edn             = new edn_ip("edn");
    spi_device      = new spi_flash("spi_flash");
    spi_controller  = new spi_controller_ip("spi_controller");
    keymgr          = new key_manager_model("keymgr_tt");
    // Demotion-state callback reads live DEMOTE_1/2 values at KDF invocation time.
    keymgr->set_demote_callback([this]() { return lc_ctrl->get_demote_state(); });
    bus               = new SimpleBus<INIT_COUNT, TARG_COUNT>("bus", false);
    rsu_module        = new reset_generation_unit("rsu");
}

// -----------------------------------------------------------------------------
// module_bind — wire all signals, sockets, and bus port mappings
// -----------------------------------------------------------------------------
inline void och_sep_ss::module_bind() {
    rsu_module->rst_ni(reset_signal);
    rsu_module->reset_req_i(aon_rst_req_signal);
    stdout_dev->nmi_vec_o(nmi_vec_signal);
    cpu_ctrl->rst_ni(reset_signal);
    cpu_ctrl->nmi_vec_o(nmi_vec_signal);
    cpu_ctrl->hwif_in.smc_fuse_sense_done = true;
    cpu_ctrl->hwif_in.sep_fuse_sense_done = true;

    // Bus port mappings (address → target module)
    {
        unsigned it = 0;
        bus->ports[it++] = new PortMapping(opt.sram_start_addr,        opt.sram_end_addr,        *sram);
        bus->ports[it++] = new PortMapping(opt.rom_start_addr,         opt.rom_end_addr,         *rom);
        bus->ports[it++] = new PortMapping(opt.dma_start_addr,         opt.dma_end_addr,         *dma);
        bus->ports[it++] = new PortMapping(opt.hmac_start_addr,        opt.hmac_end_addr,        *hmac);
        bus->ports[it++] = new PortMapping(opt.otbn_start_addr,        opt.otbn_end_addr,        *otbn);
        bus->ports[it++] = new PortMapping(opt.itcm_start_addr,        opt.itcm_end_addr,        *itcm);
        bus->ports[it++] = new PortMapping(opt.dtcm_start_addr,        opt.dtcm_end_addr,        *dtcm);
        bus->ports[it++] = new PortMapping(opt.stdout_start_addr,      opt.stdout_end_addr,      *stdout_dev);
        bus->ports[it++] = new PortMapping(opt.spi_start_addr,         opt.spi_end_addr,         *spi_controller);
        bus->ports[it++] = new PortMapping(opt.kmac_start_addr,        opt.kmac_end_addr,        *kmac);
        bus->ports[it++] = new PortMapping(opt.csrng_start_addr,       opt.csrng_end_addr,       *csrng);
        bus->ports[it++] = new PortMapping(opt.aes_start_addr,         opt.aes_end_addr,         *aes);
        bus->ports[it++] = new PortMapping(opt.mbox_start_addr,        opt.mbox_end_addr,        *mbox_bridge);
        bus->ports[it++] = new PortMapping(opt.aon_timer_start_addr,   opt.aon_timer_end_addr,   *aon_timer);
        bus->ports[it++] = new PortMapping(opt.keymgr_mb_start_addr,   opt.keymgr_mb_end_addr,   *keymgr);
        bus->ports[it++] = new PortMapping(opt.keymgr_kpvlp_start_addr,opt.keymgr_kpvlp_end_addr,*keymgr);
        bus->ports[it++] = new PortMapping(opt.sep_efuse_start_addr,   opt.sep_efuse_end_addr,   *sep_efuse);
        bus->ports[it++] = new PortMapping(opt.lc_ctrl_start_addr,     opt.lc_ctrl_end_addr,     *lc_ctrl);
        bus->ports[it++] = new PortMapping(opt.entropy_src_start_addr, opt.entropy_src_end_addr, *entropy_src);
        bus->ports[it++] = new PortMapping(opt.edn_start_addr,         opt.edn_end_addr,         *edn);
        // Scratch cold (0x10802000–0x1080203F)
        bus->ports[it++] = new PortMapping(opt.scratch_cold_start_addr, opt.scratch_cold_end_addr, *scratch_cold);
        // Scratch warm (0x10802080–0x108020BF)
        bus->ports[it++] = new PortMapping(opt.scratch_warm_start_addr, opt.scratch_warm_end_addr, *scratch_warm);
        // Local master alias remap CSR (0x10A10000–0x10A101FF)
        bus->ports[it++] = new PortMapping(opt.local_alias_remap_csr_start_addr,  opt.local_alias_remap_csr_end_addr,  *local_alias_remap);
        // Local master alias remap data-path window (0xC0000000–0xFFFFFFFF)
        bus->ports[it++] = new PortMapping(opt.local_alias_remap_data_start_addr, opt.local_alias_remap_data_end_addr, *local_alias_remap);
        // AP output remap CSR (16 × 8 B = 0x80 B at 0x10A10200)
        bus->ports[it++] = new PortMapping(opt.ap_remap_csr_start_addr,    opt.ap_remap_csr_end_addr,    *ap_output_remap);
        // AP output remap data-path window (8 MB at 0x11000000)
        bus->ports[it++] = new PortMapping(opt.ap_remap_data_start_addr,   opt.ap_remap_data_end_addr,   *ap_output_remap);
        // STEE output remap CSR (16 × 8 B = 0x80 B at 0x10A10300)
        bus->ports[it++] = new PortMapping(opt.stee_remap_csr_start_addr,  opt.stee_remap_csr_end_addr,  *stee_output_remap);
        // STEE output remap data-path window (8 MB at 0x11800000)
        bus->ports[it++] = new PortMapping(opt.stee_remap_data_start_addr, opt.stee_remap_data_end_addr, *stee_output_remap);
        // Outbound filter CSR (32 × 0x20 B = 0x400 B at 0x10A20000)
        bus->ports[it++] = new PortMapping(opt.outbound_filter_csr_start_addr,  opt.outbound_filter_csr_end_addr,  *outbound_filter);
        // Outbound filter data path has no standalone bus window — it is fed
        // by outbound_filter_mux (ap_output_remap + stee_output_remap remapped
        // outputs), matching sep_system_peripherals.sv's remap -> axi_mux ->
        // outbound filter chain.
        // Inbound filter CSR (16 × 0x20 B = 0x200 B at 0x10A21000)
        bus->ports[it++] = new PortMapping(opt.inbound_filter_csr_start_addr,   opt.inbound_filter_csr_end_addr,   *inbound_filter);
        // Inbound filter data path has no standalone bus window — fed by
        // sep_smn_inbound_axi (external-facing), matching sep_system_peripherals.sv's
        // smn_inbound_axi_req_i -> inbound_filter direct connection.
        bus->ports[it++] = new PortMapping(opt.smc_global_start_addr,      opt.smc_global_end_addr,      *smc_global);
        // SMU window (0x80000000–0xBFFFFFFF): SEP_EXT_TO_SMU leg of
        // u_axi_demux — merges into outbound_filter_mux alongside AP/STEE
        // remap output (sep_system_peripherals.sv's u_outbound_filter_mux
        // takes exactly these three slv_reqs_i), so it is subject to the
        // same Outbound Filter policy, not a standalone stub. Registered
        // after stdout's port above, so stdout's 256-byte console window at
        // the same base address continues to win by first-match priority.
        bus->ports[it++] = new PortMapping(opt.smu_global_start_addr,      opt.smu_global_end_addr,      *outbound_mux);
        // SPI mux ctrl (0x20001000–0x2000100B)
        bus->ports[it++] = new PortMapping(opt.spi_mux_ctrl_start_addr,    opt.spi_mux_ctrl_end_addr,    *spi_mux);
        // SEP software reset controller (0x10803000–0x10803007)
        bus->ports[it++] = new PortMapping(opt.reset_ctrl_start_addr,      opt.reset_ctrl_end_addr,      *reset_ctrl);
        // SEP CPU control (0x10A30000–0x10A31007)
        bus->ports[it++] = new PortMapping(opt.cpu_ctrl_start_addr,        opt.cpu_ctrl_end_addr,        *cpu_ctrl);
    }
    bus->mapping_complete();

    // Initiator sockets → bus target sockets
    {
        unsigned it = 0;
        riscv->setMasterId(it);
        bus->tsocks[it++].bind(riscv->initiator_socket);
        bus->tsocks[it++].bind(dma->ot_initiator_socket);
        bus->tsocks[it++].bind(dma->ctn_initiator_socket);
        bus->tsocks[it++].bind(dma_sys_adapter->ini);
        // local_alias_remap forwards remapped transactions back into the same bus
        bus->tsocks[it++].bind(local_alias_remap->remapped_socket);
        // smn_remap forwards inbound (external) transactions back into the same
        // bus, once an SMC/AP model binds sep_smn_inbound_axi
        bus->tsocks[it++].bind(smn_remap->ini);
        dma->sys_initiator_socket.bind(dma_sys_adapter->tgt);
    }

    // Fixed local-alias remapper chains into the programmable 16-region table
    local_alias_fixed_adapter->ini.bind(local_alias_remap->data_socket);

    // SMN inbound chain: external socket -> inbound_filter -> smn_remap -> bus
    smn_inbound_to_filter.bind(inbound_filter->data_socket);

    // Bus initiator sockets → target module sockets
    {
        unsigned it = 0;
        bus->isocks[it++].bind(sram->tsock);
        bus->isocks[it++].bind(rom->tsock);
        bus->isocks[it++].bind(dma->target_socket);
        bus->isocks[it++].bind(hmac->target_socket);
        bus->isocks[it++].bind(otbn->target_socket);
        bus->isocks[it++].bind(itcm->tsock);
        bus->isocks[it++].bind(dtcm->tsock);
        bus->isocks[it++].bind(stdout_dev->sock);
        bus->isocks[it++].bind(spi_controller->target_socket);
        bus->isocks[it++].bind(kmac->target_socket);
        bus->isocks[it++].bind(csrng->target_socket);
        bus->isocks[it++].bind(aes->target_socket);
        bus->isocks[it++].bind(mbox_bridge->tsock);
        bus->isocks[it++].bind(aon_timer->target_socket);
        bus->isocks[it++].bind(keymgr->mailbox_socket);
        bus->isocks[it++].bind(keymgr->kpvlp_socket);
        bus->isocks[it++].bind(sep_efuse->target_socket);
        bus->isocks[it++].bind(lc_ctrl->target_socket);
        bus->isocks[it++].bind(entropy_src->target_socket);
        bus->isocks[it++].bind(edn->target_socket);
        bus->isocks[it++].bind(scratch_cold->target_socket);
        bus->isocks[it++].bind(scratch_warm->target_socket);
        bus->isocks[it++].bind(local_alias_remap->target_socket);
        // Data-path window routes through the fixed remapper first (reads live
        // SEP_LOCAL_BASE_ADDR/SEP_REGION_SIZE), then into the programmable
        // 16-region table (local_alias_remap->data_socket).
        bus->isocks[it++].bind(local_alias_fixed_adapter->tgt);
        // AP remap: CSR socket first (bus port order matches port mapping order)
        bus->isocks[it++].bind(ap_output_remap->target_socket);
        bus->isocks[it++].bind(ap_output_remap->data_socket);
        // STEE remap
        bus->isocks[it++].bind(stee_output_remap->target_socket);
        bus->isocks[it++].bind(stee_output_remap->data_socket);
        // Outbound filter: CSR only (data path fed by outbound_filter_mux, not the bus)
        bus->isocks[it++].bind(outbound_filter->target_socket);
        // Inbound filter: CSR only (data path fed by sep_smn_inbound_axi, not the bus)
        bus->isocks[it++].bind(inbound_filter->target_socket);
        bus->isocks[it++].bind(smc_global->tgt32);
        bus->isocks[it++].bind(outbound_mux->smu_tgt);
        bus->isocks[it++].bind(spi_mux->tsock);
        bus->isocks[it++].bind(reset_ctrl->target_socket);
        bus->isocks[it++].bind(cpu_ctrl->target_socket);

        // SMU on-die dedicated path: export the SMC window master at the
        // platform boundary (hierarchical initiator bind).
        smc_global->init64.bind(sep_ext_to_smc_axi);

        // The SIM_OUT and SEP_STATUS consoles now live inside sep_scratch_cold,
        // tapping COLD_SCRATCH[2] and COLD_SCRATCH[1] — the registers the ROM's
        // simput*() and STATUS_OUT() actually write (fw .../include/errors.h).
        // No platform-level tap is needed here.
    }

    // DMA
    dma->clk_i(dma_clk_signal);
    dma->rst_ni(reset_signal);
    dma_clk_signal.write(sc_core::sc_time(10, sc_core::SC_NS));
    dma->alert_fatal_fault(dma_alert_fatal_sig);
    dma_alert_fatal_sig.write(false);
    for (int i = 0; i < 11; i++) {
        dma_lsio_trigger[i].write(false);
        dma->lsio_trigger[i](dma_lsio_trigger[i]);
    }
    dma->dma_done_intr(dma_done_intr_sig);
    dma->dma_chunk_done_intr(dma_chunk_done_intr_sig);
    dma->dma_error_intr(dma_error_intr_sig);

    // HMAC
    hmac->clk_i(hmac_clk_signal);
    hmac->rst_ni(hmac_sw_rst_n_signal);
    hmac->intr_hmac_done(hmac_done_signal);
    hmac->intr_fifo_empty(hmac_fifo_empty_signal);
    hmac->intr_hmac_err(hmac_err_signal);
    hmac->alert_fatal_fault(hmac_alert_signal);
    hmac_clk_signal.write(50000000.0);  // 50 MHz
    keymgr->hmac_key_socket.bind(hmac->keymgr_tl_socket);

    // KMAC
    kmac->clk_i(kmac_clk_signal);
    kmac->rst_ni(kmac_sw_rst_n_signal);
    kmac->lc_escalate_en_i(kmac_lc_escalate_signal);
    kmac->idle_o(kmac_idle_signal);
    kmac->intr_o(kmac_intr_signal);
    keymgr->kmac_key_socket.bind(kmac->keymgr_tl_socket);
    kmac_clk_signal.write(true);
    kmac_lc_escalate_signal.write(false);

    // OTBN
    otbn->clk_core(otbn_clk_core_signal);
    otbn->rst_n(otbn_sw_rst_n_signal);
    otbn->intr_done(otbn_intr_done_signal);
    otbn->alert_fatal(otbn_alert_fatal_signal);
    otbn->alert_recov(otbn_alert_recov_signal);
    otbn->lc_escalate_req(otbn_lc_escalate_req_signal);
    otbn->lc_escalate_rsp(otbn_lc_escalate_rsp_signal);
    otbn->lc_rma_req(otbn_lc_rma_req_signal);
    otbn->lc_rma_rsp(otbn_lc_rma_rsp_signal);
    otbn->otp_key_req.bind(*otp_key_req_stub_inst);  // otp_key_rsp is sc_export, bound internally
    keymgr->otbn_key_socket.bind(otbn->keymgr_tl_socket);
    otbn_clk_core_signal.write(50000000.0);   // 50 MHz
    otbn_lc_escalate_req_signal.write(false);
    otbn_lc_rma_req_signal.write(false);

    // SPI
    spi_controller->clk_i(spi_clk_signal);
    spi_controller->rst_ni(reset_signal);
    spi_controller->error_irq(spi_error_irq_signal);
    spi_controller->spi_event_irq(spi_event_irq_signal);
    spi_controller->dma_trigger(dma_lsio_trigger[0]);  // SPI TX DMA handshake
    spi_device->rst_ni(reset_signal);
    spi_controller->spi_master(spi_device->spi_target);
    spi_clk_signal.write(true);

    // Mailbox
    mbox_bridge->isock.bind(mailbox->socket0);
    mbox_host->isock.bind(mailbox->socket1);
    mailbox->clk_i(mbox_clk_signal);
    mailbox->rst_ni(reset_signal);
    mailbox->irq_o[0](mbox_irq0_signal);
    mailbox->irq_o[1](mbox_irq1_signal);
    mbox_clk_signal.write(50000000.0);  // 50 MHz

    // AON Timer
    aon_timer->clk_aon_freq(aon_clk_aon_freq_signal);
    aon_timer->clk_sys_freq(aon_clk_sys_freq_signal);
    aon_timer->rst_n(reset_signal);
    aon_timer->rst_aon_n(reset_signal);
    aon_timer->sleep_mode(aon_sleep_mode_signal);
    aon_timer->lc_escalate_en(aon_lc_escalate_signal);
    aon_timer->intr_wkup_timer_expired(aon_intr_wkup_signal);
    aon_timer->intr_wdog_timer_bark(aon_intr_bark_signal);
    aon_timer->nmi_wdog_timer_bark(aon_nmi_bark_signal);
    aon_timer->wkup_req(aon_wkup_req_signal);
    aon_timer->aon_timer_rst_req(aon_rst_req_signal);
    aon_timer->fatal_fault(aon_fatal_fault_signal);
    aon_timer->racl_policies(aon_racl_policies_signal);
    aon_timer->racl_error(aon_racl_error_signal);
    aon_clk_aon_freq_signal.write(200000.0);    // 200 kHz
    aon_clk_sys_freq_signal.write(100000000.0); // 100 MHz
    aon_sleep_mode_signal.write(false);
    aon_lc_escalate_signal.write(false);
    aon_racl_policies_signal.write(0);

    // PIC interrupt routing
    // Note: aon_intr_wkup_signal, aon_intr_bark_signal, and
    // spi_error_irq_signal have no silicon interrupt source (not part of
    // sep_internal_interrupts[]) and are intentionally NOT routed to the PIC.
    // Each is still driven by its peripheral model but has no reader.
    pic_inputs[KEYMGR_IRQ]          = &keymgr_irq_signal;
    pic_inputs[SPI_EVENT_IRQ]       = &spi_event_irq_signal;
    pic_inputs[HMAC_DONE_IRQ]       = &hmac_done_signal;
    pic_inputs[HMAC_FIFO_EMPTY_IRQ] = &hmac_fifo_empty_signal;
    pic_inputs[HMAC_HMAC_ERR_IRQ]   = &hmac_err_signal;
    pic_inputs[KMAC_IRQ]            = &kmac_intr_signal;
    pic_inputs[OTBN_IRQ]            = &otbn_intr_done_signal;
    pic_inputs[DMA_DONE_IRQ]        = &dma_done_intr_sig;
    pic_inputs[DMA_CHUNK_DONE_IRQ]  = &dma_chunk_done_intr_sig;
    pic_inputs[DMA_ERROR_IRQ]       = &dma_error_intr_sig;
    pic_inputs[MAILBOX_IRQ0]        = &mbox_irq0_signal;
    pic_inputs[MAILBOX_IRQ1]        = &mbox_irq1_signal;
    pic_inputs[CS_CMD_REQ_DONE]     = &csrng_cs_cmd_req_done_signal;
    pic_inputs[CS_ENTROPY_REQ]      = &csrng_cs_entropy_req_signal;
    pic_inputs[CS_FATAL_ERR]        = &csrng_cs_fatal_err_signal;
    pic_inputs[CS_HW_INST_EXC]      = &csrng_cs_hw_inst_exc_signal;
    pic_inputs[ENTROPY_SRC_IRQ]     = &entropy_src_irq_signal;
    pic_inputs[EDN_CMD_REQ_DONE]    = &edn_cmd_req_done_signal;
    pic_inputs[EDN_FATAL_ERR]       = &edn_fatal_err_signal;

    // CSRNG
    csrng->clk_i(csrng_clk_signal);
    csrng->rst_ni(reset_signal);
    csrng->otp_en_csrng_sw_app_read(csrng_otp_en_signal);
    csrng->cs_cmd_req_done(csrng_cs_cmd_req_done_signal);
    csrng->cs_entropy_req(csrng_cs_entropy_req_signal);
    csrng->cs_hw_inst_exc(csrng_cs_hw_inst_exc_signal);
    csrng->cs_fatal_err(csrng_cs_fatal_err_signal);
    csrng->recov_alert_o(csrng_recov_alert_signal);
    csrng->fatal_alert_o(csrng_fatal_alert_signal);
    csrng_clk_signal.write(true);
    csrng_otp_en_signal.write(0x6);  // MuBi4True

    // AES
    aes->clk_i(aes_clk_signal);
    aes->rst_ni(aes_sw_rst_n_signal);
    aes->idle_o(aes_idle_signal);
    aes->lc_escalate_en(aes_lc_escalate_signal);
    aes->alert_recov_ctrl_update_err(aes_alert_recov_signal);
    aes->alert_fatal_fault(aes_alert_fatal_signal);
    keymgr->aes_key_socket.bind(aes->keymgr_tl_socket);
    aes_clk_signal.write(true);
    aes_lc_escalate_signal.write(false);

    // Entropy Source
    entropy_src->rst_ni(reset_signal);
    entropy_src->irq_o(entropy_src_irq_signal);

    // EDN
    edn->clk_i(edn_clk_signal);
    edn->rst_ni(reset_signal);
    edn->intr_edn_cmd_req_done(edn_cmd_req_done_signal);
    edn->intr_edn_fatal_err(edn_fatal_err_signal);
    edn->alert_recov_alert(edn_recov_alert_signal);
    edn->alert_fatal_alert(edn_fatal_alert_signal);
    edn_clk_signal.write(100.0);  // 100 MHz

    // KeyMgr
    keymgr->rst_ni(km_sw_rst_n_signal);
    keymgr->wipe_ni(keymgr_wipe_ni_signal);
    keymgr->irq(keymgr_irq_signal);
    keymgr_wipe_ni_signal.write(true);  // active-low, inactive at startup

    // SEP reset controller — drives per-IP SW reset signals
    reset_ctrl->global_rst_ni(reset_signal);
    reset_ctrl->km_rst_ni(km_sw_rst_n_signal);
    reset_ctrl->otbn_rst_n(otbn_sw_rst_n_signal);
    reset_ctrl->aes_rst_ni(aes_sw_rst_n_signal);
    reset_ctrl->hmac_rst_ni(hmac_sw_rst_n_signal);
    reset_ctrl->kmac_rst_ni(kmac_sw_rst_n_signal);

    // New CSML peripherals — reset
    scratch_warm->rst_ni(reset_signal);
    local_alias_remap->rst_ni(reset_signal);
    ap_output_remap->rst_ni(reset_signal);
    stee_output_remap->rst_ni(reset_signal);
    outbound_filter->rst_ni(reset_signal);
    inbound_filter->rst_ni(reset_signal);
    // feat_ctrl_o.sep_debug drives only the inbound instance's filter_skip_i;
    // RTL ties the outbound one to 1'b0, which is what leaving it unbound does.
    sep_debug_signal.write(sep_debug.get_param_value());
    inbound_filter->filter_skip_i(sep_debug_signal);

    // Remap/filter output stubs (remapped/filtered destinations are external to VP)
    // Serial chain (mirrors sep_system_peripherals.sv: remap -> axi_mux -> filter):
    //   ap_output_remap  ─┐
    //                      ├─► outbound_filter_mux ─► outbound_filter ─► outbound_filter_stub
    //   stee_output_remap ─┘
    ap_output_remap->remapped_socket.bind(outbound_mux->ap_tgt);
    stee_output_remap->remapped_socket.bind(outbound_mux->stee_tgt);
    outbound_mux->ini.bind(outbound_filter->data_socket);
    outbound_filter->filtered_socket.bind(outbound_filter_stub->socket);
    // Inbound chain (mirrors sep_system_peripherals.sv: filter -> remap):
    //   sep_smn_inbound_axi ─► inbound_filter ─► smn_remap ─► SEP bus
    inbound_filter->filtered_socket.bind(smn_remap->tgt);

    // EL2 PIC — embedded in VeeR core, not bus-mapped. The core binds the PIC's
    // clk_i/rst_ni itself (it lives inside the core and resets with it), so the
    // platform only sources the interrupt inputs.
    for (unsigned int i = 0; i < PIC_NUM_INTERRUPTS; i++)
        riscv->get_pic().irq_in[i](*pic_inputs[i]);

    // CPU core
    riscv->rst_ni(reset_signal);
    riscv->nmi_i(aon_nmi_bark_signal);
    riscv->nmi_vec_i(nmi_vec_signal);
}

// -----------------------------------------------------------------------------
// sep_smn_inbound_axi relay — forwards the external-facing boundary socket
// directly into inbound_filter->data_socket. Mirrors smn_inbound_axi_req_i's
// direct connection to u_inbound_filter in sep_system_peripherals.sv.
// -----------------------------------------------------------------------------
inline void och_sep_ss::smn_inbound_b_transport(tlm::tlm_generic_payload& trans,
                                                 sc_core::sc_time& delay) {
    smn_inbound_to_filter->b_transport(trans, delay);
}

inline unsigned int och_sep_ss::smn_inbound_transport_dbg(tlm::tlm_generic_payload& trans) {
    return smn_inbound_to_filter->transport_dbg(trans);
}

// -----------------------------------------------------------------------------
// start_of_simulation — wire OTP data from efuse into key manager
// -----------------------------------------------------------------------------
inline void och_sep_ss::start_of_simulation() {
    // efuse_model::end_of_elaboration() has already run load_fuses() by this
    // point, so lc_state and chiplet_uid are populated from the config file.
    keymgr_tt::km_firmware_handler::km_otp_data_t otp;
    otp.lc_state = sep_efuse->get_lc_state();
    std::copy(sep_efuse->get_chiplet_uid(),
              sep_efuse->get_chiplet_uid() + 8,
              otp.chiplet_uid);
    keymgr->set_otp_data(otp);

    // Backdoor-load the staged SPI flash image so the controller's command/FIFO reads see the
    // real manifest+payload images instead of erased 0xFF. start_of_simulation runs after
    // elaboration/binding and before the first transaction, so the first read sees populated flash.
    if (not spiPreloadPath.empty()) {
        // spiPreload points at a Verilog $readmemh-style hex file (e.g. a
        // fw/sep/bootcode/*.spi_preload image) — "@addr" lines set the write
        // cursor, other lines are whitespace-separated hex byte pairs. Poke
        // straight into the flash model's backing store via write_byte(); the
        // model has no notion of file format, so no .bin conversion is needed.
        std::ifstream vmf(spiPreloadPath);
        if (!vmf.is_open()) {
            std::cerr << "[spi_flash] spiPreload: cannot open " << spiPreloadPath << '\n';
        } else {
            spi_flash_model* flash = spi_device->get_model();
            uint32_t addr = 0;
            std::string line;
            while (std::getline(vmf, line)) {
                if (line.empty() || line[0] == '#') continue;
                if (line[0] == '@') { addr = static_cast<uint32_t>(std::stoul(line.substr(1), nullptr, 16)); continue; }
                std::istringstream ss(line);
                std::string tok;
                while (ss >> tok) {
                    if (addr < flash->size())
                        flash->write_byte(addr++, static_cast<uint8_t>(std::stoul(tok, nullptr, 16)));
                }
            }
            std::cout << "[spi_flash] spiPreload: loaded into flash from " << spiPreloadPath << '\n';
        }
    } else {
        // No spiPreload configured — fall back to the raw-binary backdoor file.
        // The model opens data/flash_memory.bin relative to the working
        // directory (the directory of the .ini passed to sep-vp; sep-vp chdir's
        // there at startup). When nothing is staged this is a no-op that logs
        // "... not found" and leaves the flash erased, so existing tests are
        // unaffected.
        spi_device->get_model()->load_memory_from_file();
    }
}
