// ===========================================================================
// test/include/smc_test_utils.h
//
// Common test helpers per 03_SMC_Test_Plan.pdf §A.3.
//
// Provides:
//
//   smc_test::TlmRamStub          -- minimal TLM-2.0 target that NACKs nothing
//                                    and returns zeroes on reads.
//
//   smc_test::CapturingRamStub    -- TLM-2.0 target that records every byte
//                                    written into an address-indexed map and
//                                    serves reads from the same map; lets
//                                    tests reconstruct multi-byte writes
//                                    irrespective of TLM granularity.
//
//   smc_test::smc_master           -- §A.3 register-access helper.  Drives
//                                    32/64-bit reads/writes against the
//                                    cluster via the iss_hart debug API
//                                    (which goes through Whisper's
//                                    MEM_CALLBACKS, so it traverses the
//                                    fast-mem path for in-range addresses
//                                    and the TLM ibus path for MMIO).
//
//   smc_test::expect_irq           -- §A.3 helper.  Polls an sc_in<bool> port
//                                    for a rising edge within a deadline.
//
//   smc_test::Watchdog             -- §A.5 watchdog.  An SC_THREAD that fires
//                                    SC_REPORT_FATAL after a timeout unless
//                                    cancelled.  Use one per fixture that
//                                    calls sc_start.
//
//   smc_test::make_default_cluster_cfg(num_harts)
//                                  -- 1..4 hart RV64GC cluster with a 64 KiB
//                                    fast-mem window at [0, 0x10000) and an
//                                    MMIO half-space starting at 0x80000000.
// ===========================================================================

#pragma once

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <string>
#include <unistd.h>

#include "smc_cpu_cluster.h"
#include "iss_hart.h"
#include "smc_axi_extension.h"

namespace smc_test {

// ---------------------------------------------------------------------------
// stderr_guard: temporarily redirect stderr (e.g. Whisper load_elf errors
// on intentional negative-path tests).
// ---------------------------------------------------------------------------
class stderr_guard
{
public:
    explicit stderr_guard(const char* sink = "/dev/null")
    {
        saved_fd_ = dup(STDERR_FILENO);
        if (saved_fd_ >= 0) {
            const int null_fd = open(sink, O_WRONLY);
            if (null_fd >= 0) {
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
        }
    }

    ~stderr_guard()
    {
        if (saved_fd_ >= 0) {
            dup2(saved_fd_, STDERR_FILENO);
            close(saved_fd_);
        }
    }

    stderr_guard(const stderr_guard&)            = delete;
    stderr_guard& operator=(const stderr_guard&) = delete;

private:
    int saved_fd_ = -1;
};

// ---------------------------------------------------------------------------
// CSR numbers used across tests (RISC-V Privileged Spec).
// ---------------------------------------------------------------------------
inline constexpr uint32_t CSR_MSTATUS = 0x300;
inline constexpr uint32_t CSR_MIE     = 0x304;
inline constexpr uint32_t CSR_MTVEC   = 0x305;
inline constexpr uint32_t CSR_MEPC    = 0x341;
inline constexpr uint32_t CSR_MCAUSE  = 0x342;
inline constexpr uint32_t CSR_MIP     = 0x344;
inline constexpr uint32_t CSR_MISA    = 0x301;
inline constexpr uint32_t CSR_MHARTID = 0xF14;

// MIP / MIE bits.
inline constexpr uint64_t MIP_MSIP    = (1ULL << 3);
inline constexpr uint64_t MIP_MTIP    = (1ULL << 7);
inline constexpr uint64_t MIP_MEIP    = (1ULL << 11);

// Common RV64 opcodes (32-bit, little-endian as stored in memory).
inline constexpr uint32_t OP_NOP             = 0x00000013u; // addi x0, x0, 0
inline constexpr uint32_t OP_WFI             = 0x10500073u; // wfi
inline constexpr uint32_t OP_CSRW_MSTATUS_0  = 0x30001073u; // csrrw x0, mstatus, x0
inline constexpr uint32_t OP_CSRW_MIE_0      = 0x30401073u; // csrrw x0, mie, x0
inline constexpr uint32_t OP_ADDI_X1_X0_5    = 0x00500093u; // addi x1, x0, 5
inline constexpr uint32_t OP_ADDI_X2_X0_10   = 0x00a00113u; // addi x2, x0, 10
inline constexpr uint32_t OP_J_SELF          = 0x0000006Fu; // jal x0, 0  (j .)
inline constexpr uint32_t OP_LR_D             = 0x1000202Fu; // lr.d x0, (x0)
inline constexpr uint32_t OP_SC_D             = 0x1800202Fu; // sc.d x0, x0, (x0)

// ---------------------------------------------------------------------------
// TlmRamStub: TLM target that responds OK to anything; zero-fills reads.
// ---------------------------------------------------------------------------
class TlmRamStub : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<TlmRamStub, 64> socket{"socket"};
    bool fail_next = false;

    SC_HAS_PROCESS(TlmRamStub);
    explicit TlmRamStub(sc_core::sc_module_name n) : sc_module(n)
    {
        socket.register_b_transport(this, &TlmRamStub::b_transport);
    }
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        if (fail_next) {
            fail_next = false;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memset(trans.get_data_ptr(), 0, trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ---------------------------------------------------------------------------
// CapturingRamStub: address-indexed byte map; lets the test reconstruct any
// multi-byte access regardless of the granularity at which Whisper hands the
// transaction down (it fans 1-byte writes but does N-byte reads).
// ---------------------------------------------------------------------------
class CapturingRamStub : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<CapturingRamStub, 64> socket{"socket"};

    std::map<uint64_t, uint8_t> mem;
    unsigned write_calls = 0;
    unsigned read_calls  = 0;

    tlm::tlm_command last_cmd  = tlm::TLM_IGNORE_COMMAND;
    uint64_t         last_addr = 0;
    unsigned         last_len  = 0;

    SC_HAS_PROCESS(CapturingRamStub);
    explicit CapturingRamStub(sc_core::sc_module_name n) : sc_module(n)
    {
        socket.register_b_transport(this, &CapturingRamStub::b_transport);
    }

    // Pre-seed `size` little-endian bytes of `value` at `addr`.
    void seed(uint64_t addr, uint64_t value, unsigned size)
    {
        for (unsigned i = 0; i < size; ++i) {
            mem[addr + i] = uint8_t((value >> (8 * i)) & 0xFF);
        }
    }

    // Reconstruct a little-endian word of `size` bytes at `addr`.
    uint64_t read_le(uint64_t addr, unsigned size) const
    {
        uint64_t v = 0;
        for (unsigned i = 0; i < size; ++i) {
            auto it = mem.find(addr + i);
            uint8_t b = (it != mem.end()) ? it->second : 0;
            v |= (uint64_t(b) << (8 * i));
        }
        return v;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        last_cmd  = trans.get_command();
        last_addr = trans.get_address();
        last_len  = trans.get_data_length();

        if (last_cmd == tlm::TLM_WRITE_COMMAND) {
            ++write_calls;
            for (unsigned i = 0; i < last_len; ++i) {
                mem[last_addr + i] = trans.get_data_ptr()[i];
            }
        } else if (last_cmd == tlm::TLM_READ_COMMAND) {
            ++read_calls;
            for (unsigned i = 0; i < last_len; ++i) {
                auto it = mem.find(last_addr + i);
                trans.get_data_ptr()[i] = (it != mem.end()) ? it->second : 0;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// ---------------------------------------------------------------------------
// smc_master: §A.3 register-access helper.
//
// In §A.2 the test bench drives the DUT with a TLM initiator (master).  For
// the CPU-cluster wrapper, the closest analogue today is the iss_hart debug
// API (`mem_read` / `mem_write`).  Those calls go through Whisper's
// MEM_CALLBACKS, which is the same path the running harts use, so the master
// hits the fast-mem buffer for in-range addresses and the cluster's `ibus`
// for everything else.
//
// When a real "debug master" socket is added to the wrapper later, only this
// helper changes; every test calling smc_master::write32/read32 keeps
// working unchanged.
// ---------------------------------------------------------------------------
class smc_master
{
public:
    explicit smc_master(smc::iss_hart& h) : h_(h) {}

    bool write8 (uint64_t addr, uint8_t  v) { return h_.mem_write(addr, 1, v); }
    bool write16(uint64_t addr, uint16_t v) { return h_.mem_write(addr, 2, v); }
    bool write32(uint64_t addr, uint32_t v) { return h_.mem_write(addr, 4, v); }
    bool write64(uint64_t addr, uint64_t v) { return h_.mem_write(addr, 8, v); }

    uint8_t  read8 (uint64_t addr) const { uint64_t d=0; h_.mem_read(addr, 1, d); return uint8_t(d);  }
    uint16_t read16(uint64_t addr) const { uint64_t d=0; h_.mem_read(addr, 2, d); return uint16_t(d); }
    uint32_t read32(uint64_t addr) const { uint64_t d=0; h_.mem_read(addr, 4, d); return uint32_t(d); }
    uint64_t read64(uint64_t addr) const { uint64_t d=0; h_.mem_read(addr, 8, d); return d;          }

private:
    smc::iss_hart& h_;
};

// ---------------------------------------------------------------------------
// expect_irq: poll an sc_in<bool> for a rising edge within `deadline`.
//
// Returns true if the port reads true at any time before the deadline,
// false otherwise.  Time advances via sc_start in 100 ns slices.  Works for
// the cluster's own irq inputs (all of which are sc_in<bool>) when the test
// drives them from sc_signal<bool> upstream; in that case the port reads
// the signal value after binding.
// ---------------------------------------------------------------------------
inline bool expect_irq(sc_core::sc_in<bool>& port, sc_core::sc_time deadline)
{
    sc_core::sc_time slice(100, sc_core::SC_NS);
    sc_core::sc_time spent(0, sc_core::SC_NS);
    while (spent < deadline) {
        if (port.read()) return true;
        sc_core::sc_start(slice);
        spent += slice;
    }
    return port.read();
}

// ---------------------------------------------------------------------------
// Watchdog: §A.5 mandates a SystemC-side timeout that fires SC_REPORT_FATAL
// if a test fails to reach an expected end state.  Construct one in a test
// fixture; call cancel() before the fixture tears down if the test
// completed normally.
// ---------------------------------------------------------------------------
class Watchdog : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(Watchdog);
    Watchdog(sc_core::sc_module_name name,
             sc_core::sc_time        timeout,
             std::string             tag = "watchdog")
        : sc_module(name)
        , timeout_(timeout)
        , tag_(std::move(tag))
    {
        SC_THREAD(run);
    }

    void cancel()
    {
        cancelled_ = true;
        cancel_ev_.notify(sc_core::SC_ZERO_TIME);
    }

private:
    void run()
    {
        sc_core::wait(timeout_, cancel_ev_);
        if (!cancelled_) {
            SC_REPORT_FATAL(tag_.c_str(),
                            "watchdog timeout - test exceeded its budget");
        }
    }

    sc_core::sc_time  timeout_;
    sc_core::sc_event cancel_ev_;
    bool              cancelled_ = false;
    std::string       tag_;
};

// ---------------------------------------------------------------------------
// make_default_cluster_cfg: a one-liner the tests share.
// ---------------------------------------------------------------------------
inline smc::smc_cpu_cluster::config make_default_cluster_cfg(unsigned num_harts = 1,
                                                             uint64_t reset_pc = 0x80)
{
    smc::smc_cpu_cluster::config cfg;
    cfg.num_harts    = num_harts;
    cfg.hart_id_base = 0;
    cfg.reset_pc     = reset_pc;
    cfg.mem_size     = 1ull << 32;
    cfg.fast_mem_lo  = 0x0;
    cfg.fast_mem_hi  = 0x10000;          // 64 KiB
    cfg.mmio_lo      = 0x80000000ULL;
    cfg.mmio_hi      = 0x90000000ULL;
    cfg.isa          = "rv64imafdc";
    cfg.quantum_ns   = 1000;
    cfg.quantum_insts = 16;              // small K so unit tests are responsive
    cfg.wdt_stage2_tick_ns = 0.0;        // tests drive stage-2 via dbg_wdt_stage2_tick
    return cfg;
}

// ---------------------------------------------------------------------------
// ctrl_initiator: a thin TLM-2.0 initiator the testbench uses to talk to
// the cluster's `ctrl` target socket (§3.8 register file).  Provides 32-bit
// and 64-bit accesses with little-endian byte ordering.
// ---------------------------------------------------------------------------
class ctrl_initiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<ctrl_initiator, 64> socket{"socket"};

    SC_HAS_PROCESS(ctrl_initiator);
    explicit ctrl_initiator(sc_core::sc_module_name n)
        : sc_module(n) {}

    uint32_t read32(uint64_t addr)        { uint32_t v=0; rw(tlm::TLM_READ_COMMAND,  addr, &v, 4); return v; }
    void     write32(uint64_t addr, uint32_t v) { rw(tlm::TLM_WRITE_COMMAND, addr, &v, 4); }
    uint64_t read64(uint64_t addr)        { uint64_t v=0; rw(tlm::TLM_READ_COMMAND,  addr, &v, 8); return v; }
    void     write64(uint64_t addr, uint64_t v) { rw(tlm::TLM_WRITE_COMMAND, addr, &v, 8); }
    void     read_bytes(uint64_t addr, unsigned len, void* data)
        { rw(tlm::TLM_READ_COMMAND, addr, data, len); }
    void     write_bytes(uint64_t addr, unsigned len, const void* data)
        { rw(tlm::TLM_WRITE_COMMAND, addr, const_cast<void*>(data), len); }

    tlm::tlm_response_status last_response() const { return last_response_; }

private:
    tlm::tlm_response_status last_response_ = tlm::TLM_OK_RESPONSE;

    void rw(tlm::tlm_command cmd, uint64_t addr, void* data, unsigned len)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(static_cast<uint8_t*>(data));
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay(0, sc_core::SC_NS);
        socket->b_transport(trans, delay);
        last_response_ = trans.get_response_status();
    }
};

// ---------------------------------------------------------------------------
// default_buses: three TlmRamStubs bound to data / mmio / ifetch, plus an
// idle ctrl_initiator bound to the cluster's ctrl target socket so SystemC
// elaboration completes (sc_start would otherwise fail with "port not
// bound").  Use when the test doesn't care about ctrl-register traffic.
// ---------------------------------------------------------------------------
struct default_buses
{
    TlmRamStub      bus_data  {"bus_data"};
    TlmRamStub      bus_mmio  {"bus_mmio"};
    TlmRamStub      bus_ifetch{"bus_ifetch"};
    ctrl_initiator  ctrl      {"ctrl_init"};

    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_sticky{"wdt_sticky"};
    sc_core::sc_signal<bool> rst_primary_n{"rst_primary_n"};
    sc_core::sc_signal<bool> wdt_first{"wdt_first"};
    sc_core::sc_signal<bool> wdt_second{"wdt_second"};

    void bind(smc::smc_cpu_cluster& cluster)
    {
        cluster.data  .bind(bus_data  .socket);
        cluster.mmio  .bind(bus_mmio  .socket);
        cluster.ifetch.bind(bus_ifetch.socket);
        ctrl.socket.bind(cluster.ctrl);
        bind_wdt_stage2(cluster);
    }

    void bind_wdt_stage2(smc::smc_cpu_cluster& cluster)
    {
        const unsigned nh = cluster.num_harts();
        if (wdt_sticky.size() != nh) {
            wdt_sticky.init(nh);
        }
        rst_primary_n.write(true);
        for (unsigned i = 0; i < nh; ++i) {
            wdt_sticky[i].write(false);
            cluster.wdt_timeout_cluster_i[i].bind(wdt_sticky[i]);
        }
        cluster.rst_primary_n_i.bind(rst_primary_n);
        cluster.wdt_first_timeout_o.bind(wdt_first);
        cluster.wdt_second_timeout_o.bind(wdt_second);
    }
};

// ---------------------------------------------------------------------------
// CapturingRamStub variant of `default_buses` so a test can put a recorder
// on the mmio socket only, while data / ifetch get plain stubs.  Same
// idle ctrl_initiator binding as default_buses.
// ---------------------------------------------------------------------------
struct mmio_capture_buses
{
    TlmRamStub        bus_data  {"bus_data"};
    CapturingRamStub  bus_mmio  {"bus_mmio"};
    TlmRamStub        bus_ifetch{"bus_ifetch"};
    ctrl_initiator    ctrl      {"ctrl_init"};

    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_sticky{"wdt_sticky"};
    sc_core::sc_signal<bool> rst_primary_n{"rst_primary_n"};
    sc_core::sc_signal<bool> wdt_first{"wdt_first"};
    sc_core::sc_signal<bool> wdt_second{"wdt_second"};

    void bind(smc::smc_cpu_cluster& cluster)
    {
        cluster.data  .bind(bus_data  .socket);
        cluster.mmio  .bind(bus_mmio  .socket);
        cluster.ifetch.bind(bus_ifetch.socket);
        ctrl.socket.bind(cluster.ctrl);
        const unsigned nh = cluster.num_harts();
        if (wdt_sticky.size() != nh) {
            wdt_sticky.init(nh);
        }
        rst_primary_n.write(true);
        for (unsigned i = 0; i < nh; ++i) {
            wdt_sticky[i].write(false);
            cluster.wdt_timeout_cluster_i[i].bind(wdt_sticky[i]);
        }
        cluster.rst_primary_n_i.bind(rst_primary_n);
        cluster.wdt_first_timeout_o.bind(wdt_first);
        cluster.wdt_second_timeout_o.bind(wdt_second);
    }
};

// ---------------------------------------------------------------------------
// bind_signal_drivers: convenience wiring of N sc_signal<bool> drivers to
// the cluster's per-hart irq_sw / irq_timer / irq_ext input ports.
// ---------------------------------------------------------------------------
inline void bind_signal_drivers(
    smc::smc_cpu_cluster& cluster,
    sc_core::sc_vector<sc_core::sc_signal<bool>>& sig_sw,
    sc_core::sc_vector<sc_core::sc_signal<bool>>& sig_timer,
    sc_core::sc_vector<sc_core::sc_signal<bool>>& sig_ext)
{
    for (unsigned i = 0; i < cluster.num_harts(); ++i) {
        sig_sw   [i].write(false);
        sig_timer[i].write(false);
        sig_ext  [i].write(false);
        cluster.irq_sw   [i].bind(sig_sw   [i]);
        cluster.irq_timer[i].bind(sig_timer[i]);
        cluster.irq_ext  [i].bind(sig_ext  [i]);
    }
}

/// Bind stage-2 WDT sidebands to idle (deasserted) signals.
inline void bind_wdt_stage2_idle(
    smc::smc_cpu_cluster& cluster,
    sc_core::sc_vector<sc_core::sc_signal<bool>>& sticky,
    sc_core::sc_signal<bool>& rst_primary_n,
    sc_core::sc_signal<bool>& first_o,
    sc_core::sc_signal<bool>& second_o)
{
    const unsigned nh = cluster.num_harts();
    if (sticky.size() != nh) {
        sticky.init(nh);
    }
    rst_primary_n.write(true);
    for (unsigned i = 0; i < nh; ++i) {
        sticky[i].write(false);
        cluster.wdt_timeout_cluster_i[i].bind(sticky[i]);
    }
    cluster.rst_primary_n_i.bind(rst_primary_n);
    cluster.wdt_first_timeout_o.bind(first_o);
    cluster.wdt_second_timeout_o.bind(second_o);
}

}  // namespace smc_test
