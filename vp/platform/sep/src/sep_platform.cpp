// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/sep/src/sep_platform.cpp
// ===========================================================================

#include "sep_platform.hpp"
#include "tlm_quantum_policy.h"

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

och_sep_ss::och_sep_ss(sc_module_name name)
    : och_sep_ss(name, och_sep_ss_default_options_ref()) {}

och_sep_ss::~och_sep_ss() {
    // riscv first — ISS may have threads; stop before freeing other resources
    delete riscv;
    delete sram;
    delete rom;
    delete itcm;
    delete dtcm;
    delete stdout_dev;
    delete dma;
    delete dma_sys_adapter;
    delete dma_alias_remap;
    delete dma_ctn_deadend;
    delete dma_sys_deadend;
    delete hmac;
    delete kmac;
    delete abr;
    delete otbn;
    delete otp_key_req_stub_inst;
    delete csrng;
    delete aes;
    delete mailbox;
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
    delete argsReg;
}

// -----------------------------------------------------------------------------
// Constructor implementation
// -----------------------------------------------------------------------------
och_sep_ss::och_sep_ss(sc_module_name name, BasicOptions& opt_in)
    : sc_module(name)
    , opt(opt_in)
    , unused_irq_signal("unused_irq_signal")
    , pic_inputs(PIC_NUM_INTERRUPTS, &unused_irq_signal)
    , globalQuantumNs("globalQuantumNs", simtlm::DEFAULT_GLOBAL_QUANTUM_NS)
    , strap_primary_chiplet("smc.primary_chiplet", false)
    , strap_boot_recovery("smc.boot_recovery", false)
    , strap_rotate_update("smc.rotate_update", false)
    , strap_status_report_disable("smc.status_report_disable", false)
    , strap_bl0_pll_clk("smc.bl0_pll_clk", false)
    , spiPreload("spiPreload", "")
    , spiBackdoorFile("spiBackdoorFile", "")
    , smcSramBackdoorFile("smcSramBackdoorFile", "")
    , smcSramBackdoorOffset("smcSramBackdoorOffset", 0x2000u)
    , smn_inbound_to_filter("smn_inbound_to_filter")
    , outbound_filter_to_smn("outbound_filter_to_smn")
    , sep_global_base("sep_global_base", 0x0ULL)
    , sep_region_size("sep_region_size", 0x0ULL)
    , smc_global_base("smc_global_base", opt_in.smc_global_start_addr)
    , smc_region_size("smc_region_size",
                      opt_in.smc_global_end_addr - opt_in.smc_global_start_addr + 1)
    , sep_debug("sep_debug", false)
    , sep_debug_signal("sep_debug_signal")
{
    sep_smn_inbound_axi.register_b_transport(this, &och_sep_ss::smn_inbound_b_transport);
    sep_smn_inbound_axi.register_transport_dbg(this, &och_sep_ss::smn_inbound_transport_dbg);
    outbound_filter_to_smn.register_b_transport(this, &och_sep_ss::smn_outbound_b_transport);
    outbound_filter_to_smn.register_transport_dbg(this, &och_sep_ss::smn_outbound_transport_dbg);

    argsReg = new ArgsReg(opt);
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

    // Process-wide quantum is owned by sc_main on composed platforms
    // (smu-vp). Standalone sep-vp / unit tests still get this default.
    simtlm::install_global_quantum_ns_if_unset(globalQuantumNs.get_param_value());

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

    // SPI image keys and other backdoor/preload paths are resolved relative to the .ini that set them.
    auto resolveAgainstIniDir = [](std::string path) {
        if (path.empty())
            return path;
        std::filesystem::path p(path);
        if (p.is_relative()) {
            if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                p = (std::filesystem::path(baseDir) / p).lexically_normal();
            else
                p = std::filesystem::absolute(p).lexically_normal();
            return p.string();
        }
        return path;
    };

    spiPreloadPath  = resolveAgainstIniDir(spiPreload.get_param_value());
    spiBackdoorPath = resolveAgainstIniDir(spiBackdoorFile.get_param_value());
    smcSramBackdoorPath = resolveAgainstIniDir(smcSramBackdoorFile.get_param_value());
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

        // Optional manifest staged into SMC SRAM, for the recovery / secondary
        // boot path. On silicon the SMC firmware puts it there (having fetched
        // it over I3C); the VP models no SMC, so a caller supplies the bytes and
        // this presents the same contract the ROM sees.
        //
        // The offset must agree with the MANIFEST_ADDR published in scratch[8]
        // above -- the ROM reads the manifest from smc_sram_base + that value,
        // so staging it anywhere else is invisible to the ROM. Defaulting the
        // param to the same 0x2000 keeps the two from drifting silently.
        if (!smcSramBackdoorPath.empty()) {
            std::ifstream f(smcSramBackdoorPath, std::ios::binary | std::ios::ate);
            if (!f.is_open()) {
                std::cerr << "smc_global: smcSramBackdoorFile: cannot open "
                          << smcSramBackdoorPath << '\n';
            } else {
                const uint32_t off = smcSramBackdoorOffset.get_param_value();
                const std::streamsize len = f.tellg();
                if (len <= 0) {
                    std::cerr << "smc_global: smcSramBackdoorFile: empty or unreadable file\n";
                } else if (static_cast<uint64_t>(off) + static_cast<uint64_t>(len) > 0x100000ULL) {
                    std::cerr << "smc_global: smcSramBackdoorFile: image ("
                              << len << " bytes) at offset 0x" << std::hex << off << std::dec
                              << " exceeds 1MiB SMC SRAM window\n";
                } else {
                    f.seekg(0, std::ios::beg);
                    std::vector<char> buf(static_cast<size_t>(len));
                    if (f.read(buf.data(), len)) {
                        // Keep MANIFEST_ADDR (scratch[8]) in sync with the staged offset.
                        smc_global->load_data(reinterpret_cast<const char*>(&off),
                                              scratch_local(8), sizeof(off));
                        smc_global->load_data(buf.data(),
                                              static_cast<uint64_t>(SMC_SRAM_WINDOW_OFF) + off,
                                              buf.size());
                        std::cout << "smc_global: staged " << buf.size()
                                  << " bytes from " << smcSramBackdoorPath
                                  << " at SMC SRAM offset 0x" << std::hex << off << std::dec
                                  << std::endl;
                    } else {
                        std::cerr << "smc_global: smcSramBackdoorFile: read failed\n";
                    }
                }
            }
        }

        // DFT_CTRL_STATUS_SMU (SMC reg @ 0xF800): memory-repair / MBIST status. The ROM's
        // dft_mem_repair_gate() (rom_main.c) halts with ROM_ERR_DFT_GATE_BLOCKED unless
        // MEM_REPAIR_SUCCESS (bit 1) is set. Present REPAIR_DONE|REPAIR_SUCCESS (0x3) — the
        // (emulated) SMC reports a clean memory-repair pass.
        constexpr uint64_t SMC_DFT_CTRL_STATUS = 0xF800;  // SMC_DFT_CTRL_STATUS_SMU_OFFSET
        const uint32_t dft_status = 0x3u;  // MEM_REPAIR_DONE | MEM_REPAIR_SUCCESS
        smc_global->load_data(reinterpret_cast<const char*>(&dft_status),
                              SMC_DFT_CTRL_STATUS, sizeof(dft_status));

        // SMC_EXTERNAL straps (STRAPS_LO/HI @ +0x5800), composed from CCI params so
        // the ROM boot mode is invocation-selectable. init_straps() reads these
        // (fw/sep/bootcode/src/boot_straps.c): STRAPS_LO[25]=primary_chiplet selects
        // Primary (SPI boot) vs Secondary (wait for SMC manifest). Bit positions per
        // sep_smc_interface.h; HI bits are relative to the STRAPS_HI word.
        constexpr uint64_t SMC_STRAPS_LO = 0x405800;  // SMC_STRAPS_LO_OFFSET
        constexpr uint64_t SMC_STRAPS_HI = 0x405804;  // SMC_STRAPS_HI_OFFSET
        uint32_t straps_lo = 0u;
        uint32_t straps_hi = 0u;
        // Bit positions per the open-tree SEP↔SMC interface contract:
        // sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/include/sep_smc_interface.h.
        // NOTE: If running under tt-oca-harness with a different ROM/contract, keep these
        // bit positions in lockstep with that ROM’s sep_smc_interface.h.
        if (strap_primary_chiplet.get_param_value())       straps_lo |= (1u << 25);
        if (strap_status_report_disable.get_param_value()) straps_lo |= (1u << 21);
        if (strap_boot_recovery.get_param_value())         straps_hi |= (1u << 23);
        if (strap_bl0_pll_clk.get_param_value())           straps_hi |= (1u << 24);
        if (strap_rotate_update.get_param_value())         straps_hi |= (1u << 29);
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
void och_sep_ss::seed_inbound_window() {
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
    // Assigning the registers bypasses their write callbacks, so the exports
    // still carry the reset values at this point. Republish so anything sizing
    // its aperture from sep_global_base_addr_o/sep_region_size_o sees the
    // seeded window instead of [0,+16 MiB).
    cpu_ctrl->publish_inbound_window();
}

// -----------------------------------------------------------------------------
// create_modules — instantiate all peripheral and infrastructure modules
// -----------------------------------------------------------------------------
void och_sep_ss::create_modules() {
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
    reset_ctrl           = new sep_reset_ctrl_ip("reset_ctrl");
    cpu_ctrl             = new sep_cpu_ctrl_ip("cpu_ctrl");
    // Re-add the live SMU window base on the mux SMU leg (SimpleBus strips it).
    outbound_mux->smu_window_base_fn = [this]() {
        return static_cast<uint64_t>(cpu_ctrl->SMU_GLOBAL_BASE_ADDR) & 0x00FF'FFFF'FFFF'FFFFULL;
    };
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
    dma_alias_remap = new dma_alias_remap_adapter("dma_alias_remap", cpu_ctrl);
    dma_ctn_deadend = new dead_manager_port_stub("dma_ctn_deadend");
    dma_sys_deadend = new dead_manager_port_stub("dma_sys_deadend");
    hmac            = new hmac_ip("hmac");
    kmac            = new kmac_ip("kmac");
    abr             = new abr_ip("abr");
    otbn            = new otbn_ip("otbn", 0x10000);
    // otp_key_rsp stub not needed — sc_export bound internally by OTBN model
    otp_key_req_stub_inst = new otp_key_req_stub("otp_key_req_stub");
    csrng           = new csrng_model("csrng");
    aes             = new aes_model("aes");
    mailbox         = new mailbox_unit("mailbox_unit");
    mbox_bridge     = new MailboxBridge("mailbox_bridge");
    aon_timer       = new aon_timer_ip("aon_timer");
    sep_efuse       = new efuse_model("sep_efuse");
    sep_efuse->locked_field_access_irq_o(efuse_locked_field_irq_sig);
    efuse_locked_field_irq_sig.write(false);
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
void och_sep_ss::module_bind() {
    rsu_module->rst_ni(reset_signal);
    rsu_module->reset_req_i(aon_rst_req_signal);
    stdout_dev->nmi_vec_o(nmi_vec_signal);
    cpu_ctrl->rst_ni(reset_signal);
    cpu_ctrl->nmi_vec_o(nmi_vec_signal);
    cpu_ctrl->sep_global_base_addr_o(sep_global_base_addr_signal);
    cpu_ctrl->sep_region_size_o(sep_region_size_signal);
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
        bus->ports[it++] = new PortMapping(opt.abr_start_addr,         opt.abr_end_addr,         *abr);
        bus->ports[it++] = new PortMapping(opt.csrng_start_addr,       opt.csrng_end_addr,       *csrng);
        bus->ports[it++] = new PortMapping(opt.aes_start_addr,         opt.aes_end_addr,         *aes);
        bus->ports[it++] = new PortMapping(opt.mbox_start_addr,        opt.mbox_end_addr,        *mbox_bridge);
        bus->ports[it++] = new PortMapping(opt.aon_timer_start_addr,   opt.aon_timer_end_addr,   *aon_timer);
        bus->ports[it++] = new PortMapping(opt.keymgr_mb_start_addr,   opt.keymgr_mb_end_addr,   *keymgr);
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
        // SMC window (SEP_EXT_TO_SMC leg of u_axi_demux).  RTL gates it with
        // smc_global_base_addr_i/smc_region_size_i, driven by the SMC rather
        // than by anything inside SEP, so it is modelled as a pair of inputs
        // (see smc_global_base/smc_region_size) and read per transaction.
        bus->ports[it++] = new PortMapping(
            [this]() {
                const uint64_t base = smc_global_base.get_param_value();
                const uint64_t size = smc_region_size.get_param_value();
                // hi < lo is how an empty window (size 0) is expressed.
                return size ? std::make_pair(base, base + size - 1)
                            : std::make_pair(uint64_t(1), uint64_t(0));
            }, *smc_global);
        // SMU window (0x80000000–0xBFFFFFFF): SEP_EXT_TO_SMU leg of
        // u_axi_demux — merges into outbound_filter_mux alongside AP/STEE
        // remap output (sep_system_peripherals.sv's u_outbound_filter_mux
        // takes exactly these three slv_reqs_i), so it is subject to the
        // same Outbound Filter policy, not a standalone stub. Registered
        // after stdout's port above, so stdout's 256-byte console window at
        // the same base address continues to win by first-match priority.
        // Driven by cpu_ctrl's SMU_GLOBAL_BASE_ADDR/SMU_REGION_SIZE, which reset
        // to 0x80000000/0x40000000 — the same window the static Args constants
        // described, except firmware reprogramming it now takes effect.
        bus->ports[it++] = new PortMapping(
            [this]() {
                const uint64_t base =
                    static_cast<uint64_t>(cpu_ctrl->SMU_GLOBAL_BASE_ADDR) & 0x00FF'FFFF'FFFF'FFFFULL;
                const uint64_t size =
                    static_cast<uint64_t>(cpu_ctrl->SMU_REGION_SIZE) & 0xFFFF'FFFFULL;
                return size ? std::make_pair(base, base + size - 1)
                            : std::make_pair(uint64_t(1), uint64_t(0));
            }, *outbound_mux);
        // eFuse shim CSR (0x20000000–0x20000043), the eFuse model's second window
        bus->ports[it++] = new PortMapping(opt.efuse_shim_ctrl_start_addr, opt.efuse_shim_ctrl_end_addr, *sep_efuse);
        // SPI mux ctrl (0x20001000–0x2000100B)
        bus->ports[it++] = new PortMapping(opt.spi_mux_ctrl_start_addr,    opt.spi_mux_ctrl_end_addr,    *spi_mux);
        // SEP software reset controller (0x10803000–0x10803007)
        bus->ports[it++] = new PortMapping(opt.reset_ctrl_start_addr,      opt.reset_ctrl_end_addr,      *reset_ctrl);
        // SEP CPU control (0x10A30000–0x10A31007)
        bus->ports[it++] = new PortMapping(opt.cpu_ctrl_start_addr,        opt.cpu_ctrl_end_addr,        *cpu_ctrl);
    }
    bus->mapping_complete();

    // Initiator sockets → bus target sockets.  The index order here is what
    // BUS_MASTER below maps onto the crossbar's connectivity matrix, so the two
    // must be kept in step.
    {
        unsigned it = 0;
        riscv->setMasterId(it);
        bus->registerObserver(riscv);
        bus->tsocks[it++].bind(riscv->initiator_socket);
        // Every DMA address passes through the alias window remap before it
        // reaches the crossbar, exactly as u_dma_local_alias_remap does.
        bus->tsocks[it++].bind(dma_alias_remap->ini);
        dma->ot_initiator_socket.bind(dma_alias_remap->tgt);
        // local_alias_remap forwards remapped transactions back into the same bus
        bus->tsocks[it++].bind(local_alias_remap->remapped_socket);
        // smn_remap forwards inbound (external) transactions back into the same
        // bus, once an SMC/AP model binds sep_smn_inbound_axi
        bus->tsocks[it++].bind(smn_remap->ini);

        // sep_dma_wrap grounds .sys_i and leaves the CTN response channel
        // stubbed, so neither port reaches the fabric. Both go to a dead end
        // rather than the bus, which turns a transfer programmed with ASID 0x9
        // or 0xA into ERROR_CODE.bus_error instead of a silent success.
        dma->ctn_initiator_socket.bind(dma_ctn_deadend->socket);
        dma->sys_initiator_socket.bind(dma_sys_adapter->tgt);
        dma_sys_adapter->ini.bind(dma_sys_deadend->socket);
    }

    // Crossbar connectivity matrix (sep_local_axi_xbar.yaml; see
    // inc/xbar_policy.h).  Without this every initiator reaches every target,
    // which lets inbound external traffic touch the TCMs, the reset controller
    // and the system-peripheral CSRs — all of which silicon refuses.
    {
        static constexpr sep_xbar::master BUS_MASTER[INIT_COUNT] = {
            sep_xbar::master::cpu,          // 0: riscv (ifu + lsu + dbg)
            sep_xbar::master::dma,          // 1: dma OT leg, post alias remap
            sep_xbar::master::local_alias,  // 2: local-alias re-injection
            sep_xbar::master::ext,          // 3: SMN inbound after remap
        };
        bus->access_policy = [](unsigned initiator_id, uint64_t addr) {
            if (initiator_id >= INIT_COUNT) return true;
            return sep_xbar::permits(BUS_MASTER[initiator_id], addr);
        };
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
        bus->isocks[it++].bind(abr->target_socket);
        bus->isocks[it++].bind(csrng->target_socket);
        bus->isocks[it++].bind(aes->target_socket);
        bus->isocks[it++].bind(mbox_bridge->tsock);
        bus->isocks[it++].bind(aon_timer->target_socket);
        bus->isocks[it++].bind(keymgr->mailbox_socket);
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
        bus->isocks[it++].bind(sep_efuse->shim_target_socket);
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
    kmac->intr_kmac_done(kmac_done_signal);
    kmac->intr_fifo_empty(kmac_fifo_empty_signal);
    kmac->intr_kmac_err(kmac_err_signal);
    kmac->alert_recov_operation_err(kmac_alert_recov_signal);
    kmac->alert_fatal_fault(kmac_alert_fatal_signal);
    keymgr->kmac_key_socket.bind(kmac->keymgr_tl_socket);
    kmac_clk_signal.write(true);
    kmac_lc_escalate_signal.write(false);

    // Adams Bridge (SEP crypto aperture 0x1094_0000). SW_RESET_N.abr_sw_rst_n.
    abr->clk_i(abr_clk_signal);
    abr->rst_ni(abr_sw_rst_n_signal);
    abr->intr_abr_error(abr_error_signal);
    abr->intr_abr_notif(abr_notif_signal);
    abr_clk_signal.write(100000000.0);  // 100 MHz
    keymgr->abr_mldsa_seed_socket.bind(abr->keymgr_mldsa_seed_socket);
    keymgr->abr_mlkem_d_socket.bind(abr->keymgr_mlkem_d_socket);
    keymgr->abr_mlkem_z_socket.bind(abr->keymgr_mlkem_z_socket);
    keymgr->abr_mlkem_msg_socket.bind(abr->keymgr_mlkem_msg_socket);

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
    spi_controller->irq_o(spi_irq_signal);
    spi_controller->error_irq(spi_error_irq_signal);
    spi_controller->spi_event_irq(spi_event_irq_signal);
    spi_controller->dma_trigger(dma_lsio_trigger[0]);  // SPI TX DMA handshake
    spi_device->rst_ni(reset_signal);
    spi_controller->spi_master(spi_device->spi_target);
    spi_clk_signal.write(true);

    // Mailbox
    mbox_bridge->isock.bind(mailbox->target_socket);
    mailbox->clk_i(mbox_clk_signal);
    mailbox->rst_ni(reset_signal);
    for (unsigned int m = 0; m < mailbox_unit::NUM_CHANNELS; ++m) {
        mailbox->outbound_irq_o[m](mbox_outbound_irq_signal[m]);
        mailbox->inbound_irq_o[m](mbox_inbound_irq_signal[m]);
    }
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
    // Note: aon_intr_wkup_signal and aon_intr_bark_signal have no silicon
    // interrupt source (not part of sep_internal_interrupts[]) and are
    // intentionally NOT routed to the PIC. Each is still driven by its
    // peripheral model but has no reader.
    //
    // SPI has one interrupt line in silicon, spi_irq_i = error || event, so the
    // slot is driven from the model's combined irq_o rather than either class
    // signal; spi_error_irq_signal and spi_event_irq_signal remain for tracing.
    pic_inputs[KEYMGR_IRQ]          = &keymgr_irq_signal;
    pic_inputs[SPI_EVENT_IRQ]       = &spi_irq_signal;
    pic_inputs[HMAC_DONE_IRQ]       = &hmac_done_signal;
    pic_inputs[HMAC_FIFO_EMPTY_IRQ] = &hmac_fifo_empty_signal;
    pic_inputs[HMAC_HMAC_ERR_IRQ]   = &hmac_err_signal;
    pic_inputs[KMAC_DONE_IRQ]       = &kmac_done_signal;
    pic_inputs[KMAC_FIFO_EMPTY_IRQ] = &kmac_fifo_empty_signal;
    pic_inputs[KMAC_ERR_IRQ]        = &kmac_err_signal;
    pic_inputs[ABR_ERROR_IRQ]       = &abr_error_signal;
    pic_inputs[ABR_NOTIF_IRQ]       = &abr_notif_signal;
    pic_inputs[OTBN_IRQ]            = &otbn_intr_done_signal;
    pic_inputs[DMA_DONE_IRQ]        = &dma_done_intr_sig;
    pic_inputs[DMA_CHUNK_DONE_IRQ]  = &dma_chunk_done_intr_sig;
    pic_inputs[DMA_ERROR_IRQ]       = &dma_error_intr_sig;
    pic_inputs[DMA_ALERT_IRQ]       = &dma_alert_fatal_sig;
    pic_inputs[WDT_ALERT_IRQ]       = &aon_fatal_fault_signal;
    // A pulse, not a level: configure the gateway edge-triggered to catch it, as the
    // RTL's one-cycle assertion requires there too.
    pic_inputs[LOCKED_FIELD_ACCESS_IRQ] = &efuse_locked_field_irq_sig;
    // sep.sv routes inbound mailbox interrupts to the SEP PIC (one source per
    // channel); outbound interrupts go out to the SMC via smc_mailbox_interrupt_o.
    for (unsigned int m = 0; m < mailbox_unit::NUM_CHANNELS; ++m) {
        pic_inputs[MAILBOX_IRQ0 + m] = &mbox_inbound_irq_signal[m];
    }
    pic_inputs[CS_CMD_REQ_DONE]     = &csrng_cs_cmd_req_done_signal;
    pic_inputs[CS_ENTROPY_REQ]      = &csrng_cs_entropy_req_signal;
    pic_inputs[CS_FATAL_ERR]        = &csrng_cs_fatal_err_signal;
    pic_inputs[CS_HW_INST_EXC]      = &csrng_cs_hw_inst_exc_signal;
    pic_inputs[ENTROPY_SRC_IRQ]     = &entropy_src_irq_signal;
    pic_inputs[EDN_CMD_REQ_DONE]    = &edn_cmd_req_done_signal;
    pic_inputs[EDN_FATAL_ERR]       = &edn_fatal_err_signal;
    pic_inputs[CRYPTO_ALERT_IRQ]    = &crypto_alert_signal;

    // CSRNG
    csrng->clk_i(csrng_clk_signal);
    csrng->rst_ni(trng_sw_rst_n_signal);
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
    entropy_src->rst_ni(trng_sw_rst_n_signal);
    entropy_src->irq_o(entropy_src_irq_signal);

    // EDN
    edn->clk_i(edn_clk_signal);
    edn->rst_ni(trng_sw_rst_n_signal);
    edn->intr_edn_cmd_req_done(edn_cmd_req_done_signal);
    edn->intr_edn_fatal_err(edn_fatal_err_signal);
    edn->alert_recov_alert(edn_recov_alert_signal);
    edn->alert_fatal_alert(edn_fatal_alert_signal);
    edn_clk_signal.write(100.0);  // 100 MHz

    // Collapse the crypto blocks' alerts onto the single PIC line sep.sv gives
    // them. All eleven of sep_crypto.sv's channels are represented.
    SC_METHOD(update_crypto_alert);
    sensitive << hmac_alert_signal
              << otbn_alert_fatal_signal << otbn_alert_recov_signal
              << aes_alert_recov_signal << aes_alert_fatal_signal
              << kmac_alert_recov_signal << kmac_alert_fatal_signal
              << csrng_recov_alert_signal << csrng_fatal_alert_signal
              << edn_recov_alert_signal << edn_fatal_alert_signal;
    dont_initialize();

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
    reset_ctrl->trng_rst_ni(trng_sw_rst_n_signal);
    reset_ctrl->abr_rst_ni(abr_sw_rst_n_signal);

    // Remaining peripherals — reset
    scratch_warm->rst_ni(reset_signal);
    local_alias_remap->rst_ni(reset_signal);
    ap_output_remap->rst_ni(reset_signal);
    stee_output_remap->rst_ni(reset_signal);
    outbound_filter->rst_ni(reset_signal);
    inbound_filter->rst_ni(reset_signal);
    // feat_ctrl_o.sep_debug drives only the inbound instance's filter_skip_i;
    // RTL ties the outbound one to 1'b0, which is what leaving it unbound does.
    // The value comes from lc_ctrl at start_of_simulation, once the eFuse has been
    // sensed and the feature vector computed; this is only the pre-elaboration default.
    sep_debug_signal.write(sep_debug.get_param_value());
    inbound_filter->filter_skip_i(sep_debug_signal);

    // Outbound chain (mirrors sep_system_peripherals.sv: remap -> axi_mux ->
    // filter), terminating at the chiplet boundary rather than in a sink:
    //   ap_output_remap  ─┐
    //                      ├─► outbound_filter_mux ─► outbound_filter ─► sep_smn_outbound_axi
    //   stee_output_remap ─┘
    ap_output_remap->remapped_socket.bind(outbound_mux->ap_tgt);
    stee_output_remap->remapped_socket.bind(outbound_mux->stee_tgt);
    outbound_mux->ini.bind(outbound_filter->data_socket);
    outbound_filter->filtered_socket.bind(outbound_filter_to_smn);
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
// crypto_alert aggregation — sep_crypto.sv gives AES, HMAC, OTBN, KMAC, CSRNG and
// EDN an alert receiver each and ORs every channel into crypto_alert_o, which
// sep.sv drives onto sep_internal_interrupts[32]. None of these blocks has a
// dedicated alert interrupt, so without this reduction a fatal fault is only
// visible by polling the owning block's STATUS register.
// -----------------------------------------------------------------------------
void och_sep_ss::update_crypto_alert() {
    const bool any_alert = hmac_alert_signal.read()
                        || otbn_alert_fatal_signal.read()
                        || otbn_alert_recov_signal.read()
                        || aes_alert_recov_signal.read()
                        || aes_alert_fatal_signal.read()
                        || kmac_alert_recov_signal.read()
                        || kmac_alert_fatal_signal.read()
                        || csrng_recov_alert_signal.read()
                        || csrng_fatal_alert_signal.read()
                        || edn_recov_alert_signal.read()
                        || edn_fatal_alert_signal.read();

    crypto_alert_signal.write(any_alert);
}

// -----------------------------------------------------------------------------
// sep_smn_inbound_axi relay — forwards the external-facing boundary socket
// directly into inbound_filter->data_socket. Mirrors smn_inbound_axi_req_i's
// direct connection to u_inbound_filter in sep_system_peripherals.sv.
// -----------------------------------------------------------------------------
void och_sep_ss::smn_inbound_b_transport(tlm::tlm_generic_payload& trans,
                                                 sc_core::sc_time& delay) {
    smn_inbound_to_filter->b_transport(trans, delay);
}

unsigned int och_sep_ss::smn_inbound_transport_dbg(tlm::tlm_generic_payload& trans) {
    return smn_inbound_to_filter->transport_dbg(trans);
}

// -----------------------------------------------------------------------------
// smn_outbound relay — forwards outbound-filter output to the boundary socket
//
// Egress mirror of the inbound relay: outbound_filter->filtered_socket is 32-bit
// and sep_smn_outbound_axi is 64-bit, so the two cannot bind directly.
// Addresses pass through untouched — AP/STEE remapping has already happened
// upstream in the output remappers, and the boundary carries global addresses.
// -----------------------------------------------------------------------------
void och_sep_ss::smn_outbound_b_transport(tlm::tlm_generic_payload& trans,
                                                 sc_core::sc_time& delay) {
    sep_smn_outbound_axi->b_transport(trans, delay);
}

unsigned int och_sep_ss::smn_outbound_transport_dbg(tlm::tlm_generic_payload& trans) {
    return sep_smn_outbound_axi->transport_dbg(trans);
}

// -----------------------------------------------------------------------------
// start_of_simulation — wire OTP data from efuse into key manager
// -----------------------------------------------------------------------------
void och_sep_ss::start_of_simulation() {
    // efuse_model::end_of_elaboration() has already run load_fuses() by this
    // point, so lc_state and chiplet_uid are populated from the config file.
    keymgr_tt::km_firmware_handler::km_otp_data_t otp;
    otp.lc_state = sep_efuse->get_lc_state();
    std::copy(sep_efuse->get_chiplet_uid(),
              sep_efuse->get_chiplet_uid() + 8,
              otp.chiplet_uid);
    keymgr->set_otp_data(otp);

    // The lifecycle controller has no state of its own in RTL: sep_lifecycle_ctrl.sv
    // takes shadow_regs_i, security_disable_i and secure_tm_i from the eFuse wrapper and
    // is combinational on all three. Driving them from the same place keeps FEAT_CTRL
    // consistent with the fuse instead of relying on two configs agreeing by hand.
    //
    // Re-running on every shadow change is the part that matters. SiP_DIS and SYS_DIS are
    // software-writable woset fields, so firmware disabling a feature through the eFuse
    // has to move FEAT_CTRL; sampling once here would freeze it at the boot value.
    //
    // secure_tm comes from the eFuse too, though it is not a fuse: it is the test_en
    // strap, which sep_efuse_wrapper.sv latches when fuse sense completes and fans out
    // to lc_ctrl alongside the shadow registers. Taking it from there rather than from
    // lc_ctrl's own parameter is what makes it one strap — the same assertion that
    // blocks fuse commands and zeroes the key manager's secrets is the one that opens
    // FEAT_CTRL's test group. lc_ctrl.secure_tm keeps its meaning only standalone; on
    // a platform, och_sep_ss1.sep_efuse.secure_tm is the knob.
    auto refresh_lc_inputs = [this]() {
        lifecycle_ctrl_model::lc_inputs in;
        in.lc_state_code    = sep_efuse->get_lc_state_code();
        in.sip_dis          = sep_efuse->get_sip_dis();
        in.sys_dis          = sep_efuse->get_sys_dis();
        in.security_disable = sep_efuse->get_security_disable();
        in.secure_tm        = sep_efuse->get_secure_tm();
        lc_ctrl->set_inputs(in);
    };

    // sep.sv:952 gives feat_ctrl_o.sep_debug to the inbound filter's filter_skip_i, and
    // it is the only consumer of the feature vector inside SEP. Registering before the
    // first refresh means the signal is driven by the recompute that refresh triggers,
    // and again on every later one — a demote, or a woset write to SiP_DIS/SYS_DIS, can
    // change whether inbound traffic is filtered at all.
    lc_ctrl->set_feat_ctrl_change_callback([this]() {
        sep_debug_signal.write(lc_ctrl->get_sep_debug() || sep_debug.get_param_value());
        // sep_crypto.sv:786 closes the other half of the loop, prod_dbg_active_o back
        // into the eFuse, where it freezes LC_STATE. Storing a flag rather than
        // notifying, so this does not bounce back through the shadow-change callback.
        sep_efuse->set_prod_dbg_active(lc_ctrl->get_prod_dbg_active());
    });
    refresh_lc_inputs();
    sep_efuse->set_shadow_change_callback(refresh_lc_inputs);

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
    } else if (not spiBackdoorPath.empty()) {
        // spiBackdoorFile names a raw binary image, for fixtures that are not in
        // the hex format above. This is deliberately opt-in: a run that names
        // neither key starts from erased flash regardless of what happens to be
        // lying in the working directory.
        if (not spi_device->get_model()->load_memory_from_file(spiBackdoorPath)) {
            std::cerr << "[spi_flash] spiBackdoorFile: cannot load " << spiBackdoorPath
                      << " — flash left erased\n";
        }
    }
}
