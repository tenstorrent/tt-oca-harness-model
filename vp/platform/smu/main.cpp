// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smu/main.cpp
//
// sc_main for the `smu-vp` executable: the SMU on-die integration of the SMC
// and SEP virtual platforms (see doc/smc-sep-d2d-interconnect.adoc).
//
//   smu-vp <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]
//
// Topology (RTL: hw/smu/rtl/smu.sv):
//
//   SEP (och_sep_ss1)                       SMC (dut)
//   sep_ext_to_smc_axi --[axi_window_remap]--> sep_axi_in      (dedicated path)
//   sep_smn_inbound_axi <---[smu_axi_xbar]----- output_axi     (crossbar path)
//                        ---[smu_axi_xbar]----> sys_axi_in
//
//   The chiplet-facing xbar ports (ext_in / ext_out == RTL smu_axi_in/out)
//   bind through the local AOU core (AXI-over-UCIe LT stub).  The peer AOU
//   inside smc_platform is the remote-die stand-in (tt-oca-hw
//   doc/architecture.adoc: AoU sits between SMU/SMN and the UCIe PHY).
//   Firmware must write aou_init.activate_start before D2D catch-all
//   traffic forwards (both cores ENABLED); until then axi_s rejects with
//   TLM_COMMAND_ERROR_RESPONSE.
//
// Both subsystems keep their standalone CCI namespaces: the SMC platform is
// "dut" (smc-vp .ini files apply unchanged) and the SEP platform is
// "och_sep_ss1" (sep-vp .ini files apply unchanged, including the `targets`
// ELF selection).  The SEP's smc_global window is switched to forwarding
// mode via the `och_sep_ss1.smc_global.forward_en` preset applied here.
// ===========================================================================

#include "smc_platform.hpp"
#include "och_sep_ss.hpp"

#include "inc/smu_axi_xbar.h"
#include "inc/axi_window_remap.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cci_configuration>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// RTL SEP_SMC_REGION_BASE / _SIZE: the SEP's dedicated view of the SMC. Used
// both to size the dedicated-path remap and to drive the SEP's
// smc_global_base_addr_i/smc_region_size_i equivalents, so there is one
// definition of the window rather than a constant repeated per consumer.
constexpr uint64_t SEP_SMC_REGION_BASE = 0x4000'0000ULL;
constexpr uint64_t SEP_SMC_REGION_SIZE = 0x4000'0000ULL;  // 1 GiB

// ---------------------------------------------------------------------------
// Read e_entry from a little-endian ELF64 header (offset 24, 8 bytes).
// Used to preset the SMC cluster's immutable reset_pc before construction.
// (Same helper as vp/platform/smc/main.cpp.)
// ---------------------------------------------------------------------------
uint64_t read_elf64_entry(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "ERROR: cannot open ELF '" << path << "'\n";
        return UINT64_MAX;
    }
    unsigned char ident[16];
    f.read(reinterpret_cast<char*>(ident), 16);
    if (!f || ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F') {
        std::cerr << "ERROR: '" << path << "' is not an ELF file\n";
        return UINT64_MAX;
    }
    if (ident[4] != 2) {  // ELFCLASS64
        std::cerr << "ERROR: '" << path << "' is not ELF64 (smu-vp runs RV64 SMC)\n";
        return UINT64_MAX;
    }
    f.seekg(24, std::ios::beg);
    uint64_t entry = 0;
    f.read(reinterpret_cast<char*>(&entry), 8);
    if (!f) {
        std::cerr << "ERROR: short read on ELF header of '" << path << "'\n";
        return UINT64_MAX;
    }
    return entry;
}

// ---------------------------------------------------------------------------
// Idle initiator: satisfies the BW port of a target socket without issuing
// any traffic (unused inbound ports in this integration).
// ---------------------------------------------------------------------------
class idle_initiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<idle_initiator, 64> sock{"sock"};
    explicit idle_initiator(sc_core::sc_module_name name) : sc_core::sc_module(name) {}
};

// ---------------------------------------------------------------------------
// Minimal accellera-style ini -> CCI preset parser (same as smc-vp's; the
// global broker is registered by the SEP-side load_config_file below).
// ---------------------------------------------------------------------------
void parse_ini(const std::string& path);

void apply_ini_value(const std::string& key, const std::string& section,
                     const std::string& raw)
{
    cci::cci_originator o("smu_vp_cfg");
    auto broker = cci::cci_get_global_broker(o);

    auto trim = [](std::string s) {
        auto a = s.find_first_not_of(" \t");
        auto b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string{};
        return s.substr(a, b - a + 1);
    };
    std::string keyname = trim(key);
    std::string v = trim(raw);

    cci::cci_value val;
    if (section == "bool") {
        val = cci::cci_value((v == "true" || v == "1"));
    } else if (section == "int") {
        val = cci::cci_value(static_cast<int64_t>(std::strtoll(v.c_str(), nullptr, 0)));
    } else if (section == "uint") {
        val = cci::cci_value(static_cast<uint64_t>(std::strtoull(v.c_str(), nullptr, 0)));
    } else if (section == "string") {
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        val = cci::cci_value(v);
    } else {
        return;
    }
    broker.set_preset_cci_value(keyname, val);
}

void parse_ini(const std::string& path)
{
    std::ifstream f(path);
    if (!f) {
        std::cerr << "WARNING: cannot open ini '" << path << "' (skipping)\n";
        return;
    }
    std::string dir;
    {
        auto slash = path.find_last_of('/');
        dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
    }
    std::string line, section;
    while (std::getline(f, line)) {
        auto t0 = line.find_first_not_of(" \t");
        if (t0 == std::string::npos) continue;
        std::string t = line.substr(t0);
        if (t.empty() || t[0] == '#') continue;
        if (t[0] == '@') {                                // @include
            std::istringstream ss(t.substr(1));
            std::string inc; ss >> inc;
            std::string ip = inc;
            if (!ip.empty() && ip[0] != '/') ip = dir + "/" + ip;
            parse_ini(ip);
            continue;
        }
        if (t.front() == '[') {                            // section header
            auto e = t.find(']');
            section = (e == std::string::npos) ? t.substr(1) : t.substr(1, e - 1);
            continue;
        }
        auto colon = t.find(':');
        if (colon == std::string::npos) continue;
        apply_ini_value(t.substr(0, colon), section, t.substr(colon + 1));
    }
}

// ---------------------------------------------------------------------------
// SMC default CCI presets (identical to smc-vp's apply_default_presets()).
// ---------------------------------------------------------------------------
void apply_smc_default_presets(const std::string& top, uint64_t elf_entry)
{
    cci::cci_originator o("smu_vp_cfg");
    auto broker = cci::cci_get_global_broker(o);
    auto set = [&](const std::string& leaf, cci::cci_value v) {
        broker.set_preset_cci_value(top + "." + leaf, std::move(v));
    };

    set("bootrom.size_bytes",        cci::cci_value(uint64_t(0x20000)));
    set("scratchpad_ram.size_bytes", cci::cci_value(uint64_t(0x100000)));
    set("plic.num_sources",          cci::cci_value(336u));
    set("plic.num_contexts",         cci::cci_value(8u));
    set("clint.num_harts",           cci::cci_value(4u));
    set("clint.tick_period_ns",      cci::cci_value(0.0));   // freeze MTIME for determinism
    set("reset_unit.num_subsystems", cci::cci_value(32u));

    set("cluster.num_harts",    cci::cci_value(4u));
    set("cluster.reset_pc",     cci::cci_value(elf_entry));
    set("cluster.fast_mem_lo",  cci::cci_value(uint64_t(0x80000000)));
    set("cluster.fast_mem_hi",  cci::cci_value(uint64_t(0x90000000)));
    set("cluster.mmio_lo",      cci::cci_value(uint64_t(0xC000'0000ULL)));
    set("cluster.mmio_hi",      cci::cci_value(uint64_t(0xC100'0000ULL)));
    set("cluster.source_id",    cci::cci_value(uint16_t(0x10)));
}

} // namespace

// ===========================================================================
// sc_main
// ===========================================================================
int sc_main(int argc, char** argv)
{
    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]\n";
        return 1;
    }

    // Resolve every path to absolute BEFORE the SEP-side chdir below.
    const std::string smc_ini = std::filesystem::absolute(argv[1]).lexically_normal().string();
    const std::string smc_elf = std::filesystem::absolute(argv[2]).lexically_normal().string();
    const std::string sep_ini = std::filesystem::absolute(argv[3]).lexically_normal().string();
    const std::string sep_elf = std::filesystem::absolute(argv[4]).lexically_normal().string();
    const double sim_time_ms  = (argc >= 6) ? std::stod(argv[5]) : 50.0;

    // ------------------------------------------------------------------
    // SEP-side configuration: registers the global CCI broker and applies
    // the sep-vp ini (including the `targets` ELF selection).  Mirrors
    // vp/platform/sep/main.cpp (SEP_VP_INI_DIR + chdir for data/ files).
    // ------------------------------------------------------------------
    {
        std::filesystem::path cfgPath(sep_ini);
        (void) ::setenv("SEP_VP_INI_DIR", cfgPath.parent_path().string().c_str(), 1);
        std::filesystem::current_path(cfgPath.parent_path());
        load_config_file(cfgPath.string().c_str());
    }

    // ------------------------------------------------------------------
    // SMC-side configuration: default presets, then the smc-vp ini.
    // ------------------------------------------------------------------
    const std::string smc_top = "dut";
    uint64_t entry = read_elf64_entry(smc_elf);
    if (entry == UINT64_MAX) return 1;
    std::cout << "smu-vp: SMC ELF '" << smc_elf << "' entry=0x" << std::hex << entry << std::dec << "\n";
    apply_smc_default_presets(smc_top, entry);

    // Aperture-size defaults come BEFORE the ini so an ini may enlarge/shrink
    // them (e.g. to cover the SEP SRAM alias for the SMU link test).  The bases
    // stay forced after the ini (see below): they are the integration contract
    // with the SEP-side inbound remap and must not be tunable independently.
    // `smu_xbar.sep_region_size` stays the knob the ini turns even though the
    // crossbar now takes its live SEP aperture from the SEP's CSRs, because the
    // value is forwarded into those CSRs below.
    {
        cci::cci_originator o("smu_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        broker.set_preset_cci_value("smu_xbar.sep_region_size",
                                    cci::cci_value(uint64_t(0x0100'0000ULL)));
        broker.set_preset_cci_value("smu_xbar.smc_region_size",
                                    cci::cci_value(uint64_t(0x0100'0000ULL)));
    }
    parse_ini(smc_ini);

    // ------------------------------------------------------------------
    // SMU integration presets.
    //
    // The SEP's smc_global window forwards to the real SMC (dedicated
    // sep_ext_to_smc_axi path).  The SMC aperture follows the SMC's GLOBAL_BASE
    // reset.
    // ------------------------------------------------------------------
    {
        cci::cci_originator o("smu_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        broker.set_preset_cci_value("och_sep_ss1.smc_global.forward_en", cci::cci_value(true));

        // Seed the SEP's inbound-window CSRs.  Standing in for the firmware that
        // programs them on silicon, these are the single definition of the
        // window: the crossbar reads it back over sep_global_base_addr_i /
        // sep_region_size_i (bound below) rather than being configured to match.
        // Presetting smu_xbar.sep_global_base as well is what previously let the
        // two drift, and a crossbar aperture wider than the CSR window forwards
        // inbound hits the SEP's internal bus cannot decode.
        broker.set_preset_cci_value("och_sep_ss1.sep_global_base",
                                    cci::cci_value(uint64_t(0x5000'0000ULL)));
        const cci::cci_value xbar_sep_size =
            broker.get_preset_cci_value("smu_xbar.sep_region_size");
        if (xbar_sep_size.is_uint64() || xbar_sep_size.is_number())
            broker.set_preset_cci_value("och_sep_ss1.sep_region_size", xbar_sep_size);
        broker.set_preset_cci_value("smu_xbar.smc_global_base",
                                    cci::cci_value(uint64_t(0x4000'0000ULL)));

        // RTL's smc_global_base_addr_i/smc_region_size_i. The SEP's own default
        // is the narrower window it used when nothing published these, which
        // would gate off most of the SMC reachable over the dedicated path.
        broker.set_preset_cci_value("och_sep_ss1.smc_global_base",
                                    cci::cci_value(SEP_SMC_REGION_BASE));
        broker.set_preset_cci_value("och_sep_ss1.smc_region_size",
                                    cci::cci_value(SEP_SMC_REGION_SIZE));

        // SEP ELF selection (overrides the sep ini's `targets` key; same
        // JSON-quoted string convention as sep-vp's targets override).
        const std::string targets_json = "\"" + sep_elf + "\"";
        broker.set_preset_cci_value("och_sep_ss1.targets",
                                    cci::cci_value::from_json(targets_json));
    }

    // ------------------------------------------------------------------
    // Construct the two subsystem platforms and the SMU on-die interconnect.
    // ------------------------------------------------------------------
    CsmlLogger::setGlobalLogFile("och_sep_ss.log");
    smc::smc_platform dut{smc_top.c_str()};
    och_sep_ss sep{"och_sep_ss1"};
    smu::smu_axi_xbar xbar{"smu_xbar"};
    // Dedicated-path local-alias remap (RTL SEP_SMC_REGION_BASE/_SIZE):
    // SEP SMC window [SEP_SMC_REGION_BASE, +1 GiB) -> alias base 0x0.  The same
    // two constants are presented to the SEP as smc_global_base/smc_region_size
    // above (RTL's smc_global_base_addr_i/smc_region_size_i), so the window the
    // SEP will emit into and the window this path accepts are one definition.
    smu::axi_window_remap<64, 64> sep2smc_remap{"sep2smc_remap",
                                                SEP_SMC_REGION_BASE, SEP_SMC_REGION_SIZE, 0x0ULL};

    // Dedicated SEP -> SMC path.
    sep.sep_ext_to_smc_axi.bind(sep2smc_remap.tgt);
    sep2smc_remap.init.bind(dut.sep_axi_in);

    // Crossbar paths.
    dut.output_axi.bind(xbar.smc_out);          // SMC outbound -> xbar
    xbar.smc_in.bind(dut.sys_axi_in);           // xbar -> SMC inbound
    xbar.sep_in.bind(sep.sep_smn_inbound_axi);  // xbar -> SEP inbound

    // The crossbar sizes its SEP aperture from the SEP's own window CSRs
    // (sep.sv: sep_global_base_addr_o / sep_region_size_o), so a firmware
    // reprogram moves both sides together.
    xbar.sep_global_base_addr_i(sep.sep_global_base_addr_signal);
    xbar.sep_region_size_i(sep.sep_region_size_signal);

    // SEP outbound (RTL smn_outbound_axi): everything the SEP's outbound filter
    // passes leaves here, so the crossbar sees the SEP as a master.
    sep.sep_smn_outbound_axi.bind(xbar.sep_out);

    // Unused SMC inbound ports.
    idle_initiator idle_jtag{"idle_jtag"};
    idle_jtag.sock.bind(dut.jtag_axi_in);

    // ------------------------------------------------------------------
    // Chiplet-facing boundary (RTL smu_axi_in/out).  AoU is the AXI-over-
    // UCIe stub: SMU catch-all outbound enters local AOU TX, local AOU RX
    // (traffic from the remote-die peer) re-enters the xbar as ext_in.
    // The peer AOU's master already terminates on smc_platform's remote
    // SMN stub; its slave is idle (no remote SMU in this single-die VP).
    // ------------------------------------------------------------------
    xbar.ext_out.bind(dut.aou_axi_s);
    dut.aou_axi_m.bind(xbar.ext_in);

    // Load the SMC firmware into the cluster's Whisper ISS (fast-mem backed).
    if (!dut.cluster.load_elf({smc_elf})) {
        std::cerr << "ERROR: cluster.load_elf failed for '" << smc_elf << "'\n";
        return 1;
    }

    std::cout << "smu-vp: running " << sim_time_ms << " ms of simulation...\n";
    sc_start(sc_time(sim_time_ms, SC_MS));
    std::cout << "smu-vp: simulation ended at " << sc_time_stamp() << "\n";

    // Drain SMC UART0's TX debug buffer (same as smc-vp).
    std::cout << "---- SMC UART0 output ----\n";
    {
        uint8_t ch = 0;
        while (dut.uart_[0].dbg_tx_pop(ch)) std::cout.put((char)ch);
        std::cout << "\n---- end SMC UART0 ----\n";
    }
    return 0;
}
