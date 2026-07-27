// ===========================================================================
// include/smc_cpu_cluster.h
//
// PURPOSE
// -------
// SystemC TLM-2.0 module representing the CPU cluster IP of the SMC.  This
// is the top-level deliverable for the "CPU Cluster Wrapper" task.
//
// Per 02_SMC_IP_LowLevel_Design.pdf §3 the cluster exposes:
//   * 3 initiator sockets:
//        data    -- AXI4 high-perf path (memory + DMA-visible windows)
//        mmio    -- AXI4-Lite peripheral xbar (PLIC, CLINT, mailbox, ...)
//        ifetch  -- AXI4 instruction fetch (currently routed through `data`
//                   internally because Whisper's MEM_CALLBACKS does not
//                   distinguish fetch from load; the socket is exposed for
//                   forward compatibility with §3.6).
//   * 1 target socket:
//        ctrl    -- 4 KB CPU-Control register file at BASE+0x003_9000
//                   (RESET_VECTOR, RESET_CTRL, scratch, attributes, mutex,
//                    semaphore, and dummy ROM words).  See cpu_ctrl.rdl.
//   * Per-hart IRQ inputs:
//        irq_sw[i]    (= msip_in[i]    in §3.7 -- CLINT software IRQ)
//        irq_timer[i] (= mtip_in[i]    in §3.7 -- CLINT timer IRQ)
//        irq_ext[i]   (= meip_in[i]    in §3.7 -- PLIC M-mode external)
//
//   * Debug API for the testbench / debug master:
//        load_elf, hart(i).{step,reset,read_csr,inject_nmi,last_commit, ...}
//        inject_nmi(hart_idx)  (cluster-level alias for hart(i).inject_nmi())
//
// Every outgoing transaction carries an `smc_axi_extension` (see §3.10);
// targets that don't care simply ignore the extension.
// ===========================================================================

#pragma once

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <cci_configuration>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "iss_hart.h"

// Forward declarations.
namespace WdRiscv { template <typename URV> class System; }

namespace smc {

class iss_backend_whisper;  // concrete backend, defined in .cpp side
class smc_axi_extension;    // see smc_axi_extension.h

class smc_cpu_cluster : public sc_core::sc_module
{
public:
    // -- Construction parameters ---------------------------------------------
    struct config
    {
        unsigned    num_harts    = 1;                  // 1..4
        uint64_t    hart_id_base = 0;
        uint64_t    reset_pc     = 0x80000000;
        uint64_t    mem_size     = 1ull << 32;         // 4 GiB total addrspace

        // Internal "fast-mem" window served from a flat std::vector<uint8_t>.
        uint64_t    fast_mem_lo  = 0x00000000;
        uint64_t    fast_mem_hi  = 0x80000000;

        // §3.6 bus-bridge routing.  Addresses in [mmio_lo, mmio_hi) leave
        // the cluster on the `mmio` socket; everything else outside the
        // fast-mem window leaves on `data`.  Set mmio_lo == mmio_hi to
        // disable the carve-out (everything non-fast-mem goes to `data`).
        uint64_t    mmio_lo      = 0x80000000;
        uint64_t    mmio_hi      = 0x90000000;

        std::string isa          = "rv64imafdc";
        uint64_t    quantum_ns   = 1000;               // TLM LT quantum (~1 us)
        unsigned    quantum_insts = 1000;              // K in step(K) per loop

        // §3.10 Atomicity: when true, the wrapper attempts to detect AMO/LR-SC
        // accesses and assert prot[3]=1 on the corresponding TLM transaction.
        // Today the detection is best-effort (Whisper's MEM_CALLBACKS path
        // does not pass instruction info); set false to opt out.
        bool        amo_lock_detect = true;

        // smc_axi_extension defaults (initiator-side identity).
        uint16_t    source_id     = 0x10;              // SMC_CPU_SOURCE_ID

        // §3.8 control-register window.  Writes to the ctrl socket land
        // here; offsets are taken modulo 0x1000 so the socket can sit
        // anywhere in the system address map.
        uint64_t    ctrl_size_bytes = 0x1000;          // 4 KiB

        // Construction strap retained for SMC_ATTRIBUTES/debug APIs; LOCAL_BASE
        // is now exposed by smc_base_config, not cpu_ctrl.
        uint64_t    local_base_default = 0xC000'0000ull;

        /// Stage-2 WDT tick period (ns).  0 disables auto-tick (use dbg_wdt_stage2_tick).
        double      wdt_stage2_tick_ns = 100.0;
    };

    // -- TLM / signal ports --------------------------------------------------
    // §3.3 -- three initiator sockets.  See `pick_socket()` in the .cpp.
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> data  {"data"};
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> mmio  {"mmio"};
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> ifetch{"ifetch"};

    // §3.8 -- the CPU-Control register file, accessed via b_transport.
    tlm_utils::simple_target_socket<smc_cpu_cluster, 64>    ctrl  {"ctrl"};

    // §3.7 -- per-hart IRQ inputs.
    sc_core::sc_vector<sc_core::sc_in<bool>>                irq_sw   {"irq_sw"};
    sc_core::sc_vector<sc_core::sc_in<bool>>                irq_timer{"irq_timer"};
    sc_core::sc_vector<sc_core::sc_in<bool>>                irq_ext  {"irq_ext"};

    // Stage-2 WDT (RTL smc_cpu_ctrl_wrap): countdown while per-core stage-1
    // sticky rst is high.  first = OR(sticky); second → reset_unit.
    sc_core::sc_vector<sc_core::sc_in<bool>>                wdt_timeout_cluster_i{"wdt_timeout_cluster_i"};
    sc_core::sc_in<bool>                                   rst_primary_n_i{"rst_primary_n_i"};
    sc_core::sc_out<bool>                                  wdt_first_timeout_o{"wdt_first_timeout_o"};
    sc_core::sc_out<bool>                                  wdt_second_timeout_o{"wdt_second_timeout_o"};

    // -- Construction --------------------------------------------------------
    SC_HAS_PROCESS(smc_cpu_cluster);
    explicit smc_cpu_cluster(sc_core::sc_module_name name);
    smc_cpu_cluster(sc_core::sc_module_name name, const config& cfg);
    ~smc_cpu_cluster() override;

    // -- ELF loading (called by testbench before sc_start) ------------------
    bool load_elf(const std::vector<std::string>& elf_paths);

    // -- §3.9 debug API -----------------------------------------------------
    unsigned                   num_harts() const { return num_harts_p_.get_value(); }
    iss_hart&                  hart(unsigned i);
    const iss_hart&            hart(unsigned i) const;
    WdRiscv::System<uint64_t>& whisper_system();

    void inject_nmi(unsigned hart_idx, uint64_t cause = 0);

    // -- §3.8 register accessors -------------------------------------------
    uint64_t reset_vector_n  (unsigned i) const;
    void     set_init_mem_done(bool done);    // peer IP / firmware hook
    bool     init_mem_done() const { return regs_.init_mem_done != 0; }
    void     set_disable_sram_autoinit(bool disable);
    void     set_mem_repair_status(uint32_t v);

    // §3.8 strap sampled by scratchpad_sram on reset (see INIT_MEM_DONE handshake).
    uint32_t disable_sram_autoinit() const { return regs_.disable_sram_autoinit; }

    /// Advance stage-2 countdown by @p n ticks (test / tick_period_ns==0).
    void dbg_wdt_stage2_tick(unsigned n = 1);
    bool dbg_wdt_first_timeout() const { return wdt_first_timeout_; }
    bool dbg_wdt_second_timeout() const { return wdt_second_timeout_; }
    uint32_t dbg_wdt_stage2_count(unsigned core) const;

private:
    // -- CCI configuration (Phase 1 — defaults match struct config; broker
    //    presets override before construction).  Declared before processes so
    //    elaboration order matches the PLIC IP pattern.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_harts_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> hart_id_base_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> reset_pc_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> mem_size_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> fast_mem_lo_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> fast_mem_hi_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> mmio_lo_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> mmio_hi_p_;
    cci::cci_param<std::string, cci::CCI_IMMUTABLE_PARAM> isa_p_;
    cci::cci_param<uint64_t>                            quantum_ns_p_;
    cci::cci_param<unsigned>                             quantum_insts_p_;
    cci::cci_param<bool>                                 amo_lock_detect_p_;
    cci::cci_param<uint16_t>                             source_id_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> ctrl_size_bytes_p_;
    cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> local_base_default_p_;
    cci::cci_param<double, cci::CCI_IMMUTABLE_PARAM>   wdt_stage2_tick_ns_p_;

    // -- Processes -----------------------------------------------------------
    void hart_thread(unsigned i);       // SC_THREAD: step loop
    void irq_aggregator(unsigned i);    // SC_METHOD: signals -> MIP
    void wdt_stage2_tick_method();
    void wdt_stage2_output_method();
    void wdt_stage2_input_method();

    // -- Memory callback bodies (registered on whisper_sys_) ----------------
    bool mem_read_cb (uint64_t addr, unsigned size, uint64_t& data);
    bool mem_write_cb(uint64_t addr, unsigned size, uint64_t  data);

    // -- TLM helpers ---------------------------------------------------------
    bool tlm_access(tlm::tlm_command cmd, uint64_t addr,
                    unsigned size, uint64_t& data /*in or out*/);

    // §3.6 socket selector.  Returns one of {&data, &mmio, &ifetch}.
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64>*
    pick_socket(uint64_t addr);

    // ctrl socket b_transport handler.
    void ctrl_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&);

    // RESET_CTRL write helper. Lower bits are active-high run enables in LT
    // because the RDL fields are active-low reset_n controls.
    void apply_reset_ctrl(uint64_t new_value);

    void apply_wdt_timeout_reset(uint32_t pulse_bits);
    void wdt_stage2_step_once();
    void schedule_wdt_stage2_recompute();

    // -- State ---------------------------------------------------------------
    std::unique_ptr<WdRiscv::System<uint64_t>>        whisper_sys_;
    std::vector<std::unique_ptr<iss_backend_whisper>> harts_;
    sc_core::sc_vector<sc_core::sc_event>             wfi_event_      {"wfi_event"};
    sc_core::sc_vector<sc_core::sc_event>             core_enable_event_{"core_enable_event"};
    std::vector<tlm_utils::tlm_quantumkeeper>         qk_;
    std::vector<uint8_t>                              mem_buf_;

    // Set by hart_thread() before each step() so the memory callbacks
    // (which run synchronously inside singleStep) know whose quantum
    // keeper to advance.  Safe because SystemC is cooperatively
    // scheduled -- only one hart_thread runs at a time.
    unsigned                                          current_hart_ = 0;

    // §3.8 register file ----------------------------------------------------
    struct ctrl_regs
    {
        std::array<uint64_t, 4> reset_vector{};
        uint64_t reset_ctrl             = 0x0000'0000'0000'010FULL;
        uint64_t core_reset_pulse_count = 0x0000'000F'0010'0008ULL;
        uint64_t reset_timeout          = 0;
        uint64_t reference_counter      = 0;
        uint32_t wdt_timeout            = 0x4000;
        uint32_t test_ctrl              = 0;             // RO
        std::array<uint32_t, 16> scratch{};
        std::array<uint64_t, 32> wb_pc{};
        uint64_t smc_attributes         = 0;             // RO strap image
        std::array<bool, 4> mutex_available{{true, true, true, true}};
        std::array<int16_t, 4> sema{};
        std::array<uint64_t, 4> dummy_rom{{
            0x0145'0513'0000'0517ULL,
            0xFFF0'0693'3055'1073ULL,
            0x1050'0073'3046'B073ULL,
            0x0000'0000'FFDF'F06FULL,
        }};
        std::array<uint64_t, 4> dummy_rom_null{};

        // Internal hooks retained for peer LT models that have not yet moved to
        // their RTL register blocks.
        uint32_t init_mem_done          = 0;
        uint32_t disable_sram_autoinit  = 0;
        uint32_t mem_repair_status      = 0;
    } regs_;

    // Stage-2 WDT countdown (one counter per hart, max 4).
    std::array<uint32_t, 4> wdt_stage2_count_{};
    std::array<bool, 4>     wdt_stage2_reload_pulse_{};
    bool wdt_first_timeout_  = false;
    bool wdt_second_timeout_ = false;
    bool wdt_first_cache_    = false;
    bool wdt_second_cache_   = false;
    sc_core::sc_event wdt_stage2_tick_event_;
    sc_core::sc_event wdt_stage2_recompute_event_;
    sc_core::sc_time  wdt_stage2_tick_period_{};
};

} // namespace smc
