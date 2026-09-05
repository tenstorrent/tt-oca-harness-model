// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// vp/platform/smc/main.cpp
//
// sc_main for the `smc-vp` executable.  Mirrors vp/platform/sep/main.cpp:
//   smc-vp <cci-ini> <elf> [sim_time_ms] [--uart-live] [--uart-interactive]
//
#include "smc_platform.hpp"
#include "tlm_quantum_policy.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

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
    } else if (section == "double" || section == "float") {
        val = cci::cci_value(std::strtod(v.c_str(), nullptr));
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

// ---------------------------------------------------------------------------
// Live UART0 console: drain TX history to stdout and (optionally) inject
// stdin bytes into RX so a Zephyr shell can be used interactively.
// ---------------------------------------------------------------------------
volatile std::sig_atomic_t g_stop_sim = 0;

void on_sigint(int)
{
    g_stop_sim = 1;
}

class termios_guard
{
public:
    void arm()
    {
        // Always non-blocking: a blocking read() inside the SystemC
        // SC_THREAD freezes the whole simulation (no boot banner, no prompt).
        orig_fl_ = ::fcntl(STDIN_FILENO, F_GETFL, 0);
        if (orig_fl_ >= 0) {
            ::fcntl(STDIN_FILENO, F_SETFL, orig_fl_ | O_NONBLOCK);
            fl_armed_ = true;
        }
        if (::isatty(STDIN_FILENO) && ::tcgetattr(STDIN_FILENO, &orig_) == 0) {
            termios raw = orig_;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN]  = 0;
            raw.c_cc[VTIME] = 0;
            if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
                tty_armed_ = true;
        }
    }
    ~termios_guard()
    {
        if (tty_armed_) ::tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
        if (fl_armed_)  ::fcntl(STDIN_FILENO, F_SETFL, orig_fl_);
    }
private:
    termios orig_{};
    int     orig_fl_   = -1;
    bool    tty_armed_ = false;
    bool    fl_armed_  = false;
};

class uart_console : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(uart_console);
    uart_console(sc_core::sc_module_name name, smc::uart& u, bool interactive)
        : sc_core::sc_module(name), uart_(u), interactive_(interactive)
    {
        SC_THREAD(pump);
    }
private:
    smc::uart& uart_;
    bool       interactive_;
    void pump()
    {
        while (true) {
            uint8_t ch = 0;
            bool any = false;
            while (uart_.dbg_tx_pop(ch)) {
                std::cout.put(static_cast<char>(ch));
                any = true;
            }
            if (any) std::cout.flush();
            if (interactive_) {
                pollfd pfd{STDIN_FILENO, POLLIN, 0};
                if (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
                    char buf[64];
                    const ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
                    for (ssize_t i = 0; i < n; ++i) {
                        uart_.inject_rx_char(static_cast<uint8_t>(buf[i]));
                    }
                }
            }
            if (g_stop_sim) {
                sc_core::sc_stop();
                return;
            }
            wait(sc_core::sc_time(50, sc_core::SC_US));
        }
    }
};

} // namespace

// ===========================================================================
// sc_main
// ===========================================================================
int sc_main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <cci-ini> <elf> [sim_time_ms] [--uart-live] [--uart-interactive]\n";
        return 1;
    }

    const std::string ini_path = argv[1];
    const std::string elf_path = argv[2];
    double sim_time_ms = 50.0;
    bool uart_live = false;
    bool uart_interactive = false;
    bool have_time = false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--uart-live") {
            uart_live = true;
        } else if (a == "--uart-interactive") {
            uart_interactive = true;
            uart_live = true;
        } else if (!have_time && !a.empty()
                   && (std::isdigit(static_cast<unsigned char>(a[0])) || a[0] == '.')) {
            sim_time_ms = std::stod(a);
            have_time = true;
        } else {
            std::cerr << "ERROR: unknown argument '" << a << "'\n";
            return 1;
        }
    }
    if (uart_interactive && !have_time) sim_time_ms = 0.0;

    const std::string top = "dut";

    // ELF entry -> cluster.reset_pc (immutable; must preset before construct).
    uint64_t entry = read_elf64_entry(elf_path);
    if (entry == UINT64_MAX) return 1;
    std::cout << "smc-vp: ELF '" << elf_path << "' entry=0x" << std::hex << entry << std::dec << "\n";

    // Register the global CCI broker BEFORE any cci_param is constructed.
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);

    // Defaults first, then ini overrides.
    apply_default_presets(top, entry);
    parse_ini(ini_path);

    // One process-wide quantum, before any ISS/cci_param construction in dut.
    {
        cci::cci_originator o("smc_vp_cfg");
        auto broker = cci::cci_get_global_broker(o);
        uint64_t qns = simtlm::DEFAULT_GLOBAL_QUANTUM_NS;
        const cci::cci_value v = broker.get_preset_cci_value(top + ".cluster.quantum_ns");
        if (v.is_uint64())
            qns = v.get_uint64();
        simtlm::set_global_quantum_ns(qns);
    }

    // Instantiate the platform (cluster is always wired in here).
    // `dut` is the CCI instance name of the SMC platform, not a second quantum.
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

    termios_guard tty;
    std::unique_ptr<uart_console> console;
    if (uart_live) {
        if (uart_interactive) {
            tty.arm();
            std::signal(SIGINT, on_sigint);
        }
        std::cout << "---- UART0 (live) ----\n" << std::flush;
        console = std::make_unique<uart_console>("uart0_console", dut.uart_[0],
                                                 uart_interactive);
    }

    if (sim_time_ms <= 0.0) {
        std::cout << "smc-vp: running until Ctrl-C...\n";
        sc_start();
    } else {
        std::cout << "smc-vp: running " << sim_time_ms << " ms of simulation...\n";
        sc_start(sc_time(sim_time_ms, SC_MS));
    }
    std::cout << "smc-vp: simulation ended at " << sc_time_stamp() << "\n";

    // Drain whatever is still sitting in UART0's TX history.  In live mode
    // most bytes were already printed; this catches the tail.  In the
    // historical (non-live) mode this is the only console dump.
    if (!uart_live) std::cout << "---- UART0 output ----\n";
    {
        uint8_t ch = 0;
        while (dut.uart_[0].dbg_tx_pop(ch)) std::cout.put((char)ch);
        std::cout << "\n---- end UART0 ----\n";
    }
    return 0;
}

