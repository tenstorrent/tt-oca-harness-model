// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smu/main.cpp
//
// sc_main for the `smu-vp` executable. Mirrors vp/platform/smc/main.cpp and
// vp/platform/sep/main.cpp: parse args, apply CCI presets, construct the
// platform, run, drain UART.
//
//   smu-vp <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]
// ===========================================================================

#include "smu_platform.hpp"
#include "reg_logger.h"
#include "reg_param.h"
#include "tlm_quantum_policy.h"

#include <systemc.h>
#include <cci_configuration>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

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
        if (t[0] == '@') {
            std::istringstream ss(t.substr(1));
            std::string inc; ss >> inc;
            std::string ip = inc;
            if (!ip.empty() && ip[0] != '/') ip = dir + "/" + ip;
            parse_ini(ip);
            continue;
        }
        if (t.front() == '[') {
            auto e = t.find(']');
            section = (e == std::string::npos) ? t.substr(1) : t.substr(1, e - 1);
            continue;
        }
        auto colon = t.find(':');
        if (colon == std::string::npos) continue;
        apply_ini_value(t.substr(0, colon), section, t.substr(colon + 1));
    }
}

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
    set("clint.tick_period_ns",      cci::cci_value(0.0));
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

int sc_main(int argc, char** argv)
{
    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]\n";
        return 1;
    }

    const std::string smc_ini = std::filesystem::absolute(argv[1]).lexically_normal().string();
    const std::string smc_elf = std::filesystem::absolute(argv[2]).lexically_normal().string();
    const std::string sep_ini = std::filesystem::absolute(argv[3]).lexically_normal().string();
    const std::string sep_elf = std::filesystem::absolute(argv[4]).lexically_normal().string();
    const double sim_time_ms  = (argc >= 6) ? std::stod(argv[5]) : 50.0;

    {
        std::filesystem::path cfgPath(sep_ini);
        (void) ::setenv("SEP_VP_INI_DIR", cfgPath.parent_path().string().c_str(), 1);
        std::filesystem::current_path(cfgPath.parent_path());
        regmodel::load_config_file(cfgPath.string().c_str());
    }

    const std::string smc_top = "dut";
    uint64_t entry = read_elf64_entry(smc_elf);
    if (entry == UINT64_MAX) return 1;
    std::cout << "smu-vp: SMC ELF '" << smc_elf << "' entry=0x" << std::hex << entry << std::dec << "\n";
    apply_smc_default_presets(smc_top, entry);

    {
        cci::cci_originator o("smu_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        broker.set_preset_cci_value("smu_xbar.sep_region_size",
                                    cci::cci_value(uint64_t(0x0100'0000ULL)));
        broker.set_preset_cci_value("smu_xbar.smc_region_size",
                                    cci::cci_value(uint64_t(0x0100'0000ULL)));
    }
    parse_ini(smc_ini);

    {
        cci::cci_originator o("smu_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        broker.set_preset_cci_value("och_sep_ss1.smc_global.forward_en", cci::cci_value(true));
        broker.set_preset_cci_value("och_sep_ss1.sep_global_base",
                                    cci::cci_value(uint64_t(0x5000'0000ULL)));
        const cci::cci_value xbar_sep_size =
            broker.get_preset_cci_value("smu_xbar.sep_region_size");
        if (xbar_sep_size.is_uint64() || xbar_sep_size.is_number())
            broker.set_preset_cci_value("och_sep_ss1.sep_region_size", xbar_sep_size);
        broker.set_preset_cci_value("smu_xbar.smc_global_base",
                                    cci::cci_value(uint64_t(0x4000'0000ULL)));
        broker.set_preset_cci_value("och_sep_ss1.smc_global_base",
                                    cci::cci_value(smu::smu_platform::SEP_SMC_REGION_BASE));
        broker.set_preset_cci_value("och_sep_ss1.smc_region_size",
                                    cci::cci_value(smu::smu_platform::SEP_SMC_REGION_SIZE));
        const std::string targets_json = "\"" + sep_elf + "\"";
        broker.set_preset_cci_value("och_sep_ss1.targets",
                                    cci::cci_value::from_json(targets_json));
    }

    RegLogger::setGlobalLogFile("och_sep_ss.log");

    // One process-wide quantum before either ISS is constructed. All
    // platform defaults are simtlm::DEFAULT_GLOBAL_QUANTUM_NS (1 µs).
    {
        cci::cci_originator o("smu_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        auto read_u64 = [&](const std::string& key, uint64_t& out) -> bool {
            const cci::cci_value v = broker.get_preset_cci_value(key);
            if (v.is_uint64()) { out = v.get_uint64(); return true; }
            return false;
        };
        uint64_t qns = simtlm::DEFAULT_GLOBAL_QUANTUM_NS;
        if (!read_u64("global_quantum_ns", qns) &&
            !read_u64("och_sep_ss1.globalQuantumNs", qns) &&
            !read_u64(smc_top + ".cluster.quantum_ns", qns)) {
            qns = simtlm::DEFAULT_GLOBAL_QUANTUM_NS;
        }
        simtlm::set_global_quantum_ns(qns);
        std::cout << "smu-vp: global TLM quantum " << qns << " ns\n";
    }

    // `dut` is the CCI hierarchical name of the SMC platform instance so
    // standalone smc-vp INI keys (`dut.cluster.*`) apply unchanged.
    smu::smu_platform plat{smc_top.c_str(), "och_sep_ss1"};

    if (!plat.dut.cluster.load_elf({smc_elf})) {
        std::cerr << "ERROR: cluster.load_elf failed for '" << smc_elf << "'\n";
        return 1;
    }

    std::cout << "smu-vp: running " << sim_time_ms << " ms of simulation...\n";
    sc_start(sc_time(sim_time_ms, SC_MS));
    std::cout << "smu-vp: simulation ended at " << sc_time_stamp() << "\n";

    std::cout << "---- SMC UART0 output ----\n";
    {
        uint8_t ch = 0;
        while (plat.dut.uart_[0].dbg_tx_pop(ch)) std::cout.put((char)ch);
        std::cout << "\n---- end SMC UART0 ----\n";
    }
    return 0;
}
