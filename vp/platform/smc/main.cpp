// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// vp/platform/smc/main.cpp
//
// sc_main for the `smc-vp` executable.  Mirrors vp/platform/sep/main.cpp:
//   smc-vp <cci-ini> <elf> [sim_time_ms]
//
#include "smc_platform.hpp"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Read e_entry from a little-endian ELF64 header (offset 24, 8 bytes).
// Used to preset the cluster's immutable reset_pc before construction.
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
        std::cerr << "ERROR: '" << path << "' is not ELF64 (smc-vp runs RV64)\n";
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
// Idle initiator: satisfies the BW port of the platform's external inbound
// target sockets (sys/jtag/sep_axi_in) without issuing any traffic.
// ---------------------------------------------------------------------------
class idle_initiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<idle_initiator, 64> sock{"sock"};
    explicit idle_initiator(sc_core::sc_module_name name) : sc_core::sc_module(name) {}
};

// ---------------------------------------------------------------------------
// Minimal accellera-style ini -> CCI preset parser.
//
// Sections: [bool] [int] [uint] [string].  Lines: `key : value`.
// `#` comments, blank lines skipped.  `@include other.ini` recurses relative
// to the ini's directory.  Keys are full hierarchical CCI names (e.g.
// "dut.cluster.reset_pc").  CSML-free (SMC platform stays off the CSML
// framework, per the workspace register-access rule).
// ---------------------------------------------------------------------------
void parse_ini(const std::string& path);

void apply_ini_value(const std::string& key, const std::string& section,
                     const std::string& raw)
{
    cci::cci_originator o("smc_vp_cfg");
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
        // strip surrounding quotes if present
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
        size_t slash = path.find_last_of('/');
        dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
    }
    std::string line, section;
    while (std::getline(f, line)) {
        auto h = line.find_first_not_of(" \t");
        if (h == std::string::npos) continue;            // blank
        if (line[h] == '#' || line[h] == ';') continue;   // comment
        std::string t = line.substr(h);
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
        std::string key = t.substr(0, colon);
        std::string val = t.substr(colon + 1);
        apply_ini_value(key, section, val);
    }
}

// ---------------------------------------------------------------------------
// Default CCI presets.  Applied before the ini (if any) so ini keys override.
// Mirrors the original smc/platform/test/platform_fw_tb.cpp apply_presets()
// adapted for the VP: cluster fetches from ELF-loaded fast-mem at 0x80000000,
// covers the local alias aperture [0xC000_0000, 0xC100_0000).
// ---------------------------------------------------------------------------
void apply_default_presets(const std::string& top, uint64_t elf_entry)
{
    cci::cci_originator o("smc_vp_cfg");
    auto broker = cci::cci_get_global_broker(o);
    auto set = [&](const std::string& leaf, cci::cci_value v) {
        broker.set_preset_cci_value(top + "." + leaf, std::move(v));
    };

    set("bootrom.size_bytes",        cci::cci_value(uint64_t(0x20000)));
    set("scratchpad_ram.size_bytes", cci::cci_value(uint64_t(0x100000)));
    set("plic.num_sources",          cci::cci_value(336u));
    set("plic.num_contexts",         cci::cci_value(8u));
    set("clint.num_harts",            cci::cci_value(4u));
    set("clint.tick_period_ns",      cci::cci_value(0.0));   // freeze MTIME for determinism
    set("reset_unit.num_subsystems", cci::cci_value(32u));

    // Cluster: 4 harts (must match smc_platform's NUM_HARTS — the platform
    // binds cluster.irq_sw/timer/ext[0..3]), ELF entry as reset PC, fast-mem
    // at 0x80000000, MMIO aperture covering the local alias window.
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
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <cci-ini> <elf> [sim_time_ms]\n";
        return 1;
    }

    const std::string ini_path = argv[1];
    const std::string elf_path = argv[2];
    const double sim_time_ms = (argc >= 4) ? std::stod(argv[3]) : 50.0;

    const std::string top = "dut";

    // ELF entry -> cluster.reset_pc (immutable; must preset before construct).
    uint64_t entry = read_elf64_entry(elf_path);
    if (entry == UINT64_MAX) return 1;
    std::cout << "smc-vp: ELF '" << elf_path << "' entry=0x" << std::hex << entry << std::dec << "\n";

    // Register the global CCI broker BEFORE any cci_param is constructed.
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));

    // Defaults first, then ini overrides.
    apply_default_presets(top, entry);
    parse_ini(ini_path);

    // Instantiate the platform (cluster is always wired in here).
    smc::smc_platform dut{top.c_str()};

    // Bind idle initiators to the external inbound target sockets so their
    // BW ports are satisfied (no external master drives them in a firmware run).
    idle_initiator idle_sys{"idle_sys"}, idle_jtag{"idle_jtag"}, idle_sep{"idle_sep"};
    idle_sys .sock.bind(dut.sys_axi_in);
    idle_jtag.sock.bind(dut.jtag_axi_in);
    idle_sep .sock.bind(dut.sep_axi_in);

    // Terminate the platform's outbound system-NoC port with a stub (the SMU
    // platform binds the SMU crossbar here instead).
    smc::stub_target<64> stub_sysmem{"stub_sysmem"};
    sc_core::sc_signal<bool> stub_sysmem_irq{"stub_sysmem_irq"};
    dut.output_axi.bind(stub_sysmem.reg_socket);
    stub_sysmem.irq_o.bind(stub_sysmem_irq);

    // Standalone smc-vp has no SMU xbar: idle-bind AOU AXI ingress and stub
    // the local AOU master (smu-vp binds these to xbar ext_out / ext_in).
    idle_initiator idle_aou{"idle_aou"};
    idle_aou.sock.bind(dut.aou_axi_s);
    smc::stub_target<64> stub_aou_remote{"stub_aou_remote"};
    sc_core::sc_signal<bool> stub_aou_remote_irq{"stub_aou_remote_irq"};
    dut.aou_axi_m.bind(stub_aou_remote.reg_socket);
    stub_aou_remote.irq_o.bind(stub_aou_remote_irq);

    // Load the firmware into the cluster's Whisper ISS (fast-mem backed).
    if (!dut.cluster.load_elf({elf_path})) {
        std::cerr << "ERROR: cluster.load_elf failed for '" << elf_path << "'\n";
        return 1;
    }

    std::cout << "smc-vp: running " << sim_time_ms << " ms of simulation...\n";
    sc_start(sc_time(sim_time_ms, SC_MS));
    std::cout << "smc-vp: simulation ended at " << sc_time_stamp() << "\n";

    // Drain UART0's TX debug buffer to stdout so the firmware's printf output
    // (which writes to UART0 THR) is visible — mirrors how sep-vp surfaces
    // firmware console output via its stdout_device / scratch_cold SIM_OUT tap.
    // The SMC UART model buffers TX bytes in a debug FIFO rather than emitting
    // them live, so we flush it once after the run.
    std::cout << "---- UART0 output ----\n";
    {
        uint8_t ch = 0;
        while (dut.uart_[0].dbg_tx_pop(ch)) std::cout.put((char)ch);
        std::cout << "\n---- end UART0 ----\n";
    }
    return 0;
}


