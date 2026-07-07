// ===========================================================================
// src/smc_cpu_cluster.cpp
//
// Implementation of the SystemC TLM-2.0 CPU cluster module per
// 02_SMC_IP_LowLevel_Design.pdf §3.  See header for module overview.
// ===========================================================================

#include "smc_cpu_cluster.h"
#include "iss_backend_whisper.h"
#include "smc_axi_extension.h"

// sc_spawn / sc_spawn_options are NOT pulled in by the umbrella systemc.h
// header in 2.3.4 -- include them explicitly here.
#include <sysc/kernel/sc_spawn.h>
#include <sysc/kernel/sc_spawn_options.h>

// Whisper headers -- only this TU pays the compile-time cost.
#include "System.hpp"
#include "Hart.hpp"
#include "InstId.hpp"
#include "DecodedInst.hpp"

#include <cstring>

namespace smc {

// ===========================================================================
// Constructors
// ===========================================================================
smc_cpu_cluster::smc_cpu_cluster(sc_core::sc_module_name name)
    : smc_cpu_cluster(name, config{})
{
}

smc_cpu_cluster::smc_cpu_cluster(sc_core::sc_module_name name,
                                 const config& cfg)
    : sc_core::sc_module(name)
    , num_harts_p_("num_harts", cfg.num_harts,
                   "Number of RV64GC harts (1..4).")
    , hart_id_base_p_("hart_id_base", cfg.hart_id_base,
                      "First MHARTID assigned to hart 0.")
    , reset_pc_p_("reset_pc", cfg.reset_pc,
                  "Default reset vector for each hart.")
    , mem_size_p_("mem_size", cfg.mem_size,
                  "Whisper address-space size in bytes.")
    , fast_mem_lo_p_("fast_mem_lo", cfg.fast_mem_lo,
                     "Lower bound of the internal fast-mem window.")
    , fast_mem_hi_p_("fast_mem_hi", cfg.fast_mem_hi,
                     "Upper bound of the internal fast-mem window.")
    , mmio_lo_p_("mmio_lo", cfg.mmio_lo,
                 "Lower bound of the mmio socket carve-out (§3.6).")
    , mmio_hi_p_("mmio_hi", cfg.mmio_hi,
                 "Upper bound of the mmio socket carve-out.")
    , isa_p_("isa", cfg.isa,
             "Whisper ISA configuration string (e.g. rv64imafdc).")
    , quantum_ns_p_("quantum_ns", cfg.quantum_ns,
                    "TLM LT global quantum in nanoseconds.")
    , quantum_insts_p_("quantum_insts", cfg.quantum_insts,
                       "Instructions retired per step(K) slice.")
    , amo_lock_detect_p_("amo_lock_detect", cfg.amo_lock_detect,
                         "Assert smc_axi_extension prot[3] on AMO/LR-SC.")
    , source_id_p_("source_id", cfg.source_id,
                   "smc_axi_extension source_id on outgoing transactions.")
    , ctrl_size_bytes_p_("ctrl_size_bytes", cfg.ctrl_size_bytes,
                         "Size of the CPU-Control register window (§3.8).")
    , local_base_default_p_("local_base_default", cfg.local_base_default,
                            "LOCAL_BASE reset value reported via the ctrl socket (§3.8).")
    , qk_(num_harts_p_.get_value())
{
    const unsigned nh = num_harts_p_.get_value();

    // ----- 1. Allocate the fast-memory window -------------------------------
    if (fast_mem_hi_p_.get_value() > fast_mem_lo_p_.get_value()) {
        mem_buf_.assign(fast_mem_hi_p_.get_value() - fast_mem_lo_p_.get_value(), 0);
    }

    // ----- 2. Initialise sc_vector ports / events ---------------------------
    irq_sw   .init(nh);
    irq_timer.init(nh);
    irq_ext  .init(nh);
    wfi_event_       .init(nh);
    core_enable_event_.init(nh);

    // ----- 3. ctrl target socket --------------------------------------------
    ctrl.register_b_transport(this, &smc_cpu_cluster::ctrl_b_transport);

    // ----- 4. CPU-Control register file: defaults --------------------------
    regs_.reset_vector.fill(reset_pc_p_.get_value());
    regs_.reset_ctrl = (regs_.reset_ctrl & ~0xFULL) |
                       ((nh >= 32) ? 0xFULL : ((1ULL << nh) - 1ULL));
    regs_.smc_attributes = (static_cast<uint64_t>(nh & 0x7u) << 33);

    // ----- 5. Construct the shared Whisper System ---------------------------
    whisper_sys_ = std::make_unique<WdRiscv::System<uint64_t>>(
        /*coreCount   */ 1u,
        /*hartsPerCore*/ nh,
        /*hartIdOffset*/ nh,
        /*memSize     */ mem_size_p_.get_value(),
        /*pageSize    */ size_t(4096));

    // ----- 6. Register memory callbacks BEFORE backends or ELF load ---------
    whisper_sys_->defineReadMemoryCallback(
        [this](uint64_t addr, unsigned size, uint64_t& data) -> bool {
            return this->mem_read_cb(addr, size, data);
        });
    whisper_sys_->defineWriteMemoryCallback(
        [this](uint64_t addr, unsigned size, uint64_t data) -> bool {
            return this->mem_write_cb(addr, size, data);
        });

    // ----- 7. Quantum keepers -----------------------------------------------
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(double(quantum_ns_p_.get_value()), sc_core::SC_NS));
    for (auto& qk : qk_) {
        qk.reset();
    }

    // ----- 8. Per-hart backends + processes ---------------------------------
    harts_.reserve(nh);
    for (unsigned i = 0; i < nh; ++i) {
        iss_hart_config hcfg;
        hcfg.hart_index = i;
        hcfg.num_harts  = nh;
        hcfg.hart_id    = hart_id_base_p_.get_value() + i;
        hcfg.reset_pc   = reset_pc_p_.get_value();
        hcfg.isa        = isa_p_.get_value();
        hcfg.mem_size   = mem_size_p_.get_value();

        harts_.emplace_back(
            std::make_unique<iss_backend_whisper>(*whisper_sys_, hcfg));

        // SC_THREAD: hart execution loop.
        sc_core::sc_spawn(
            [this, i]() { this->hart_thread(i); },
            sc_core::sc_gen_unique_name("hart_thread"));

        // SC_METHOD: IRQ aggregator.  Sensitivity is by sc_in<bool> port
        // pointer so the kernel resolves value_changed_event() AFTER the
        // testbench binds the port.
        sc_core::sc_spawn_options opts;
        opts.spawn_method();
        opts.dont_initialize();
        opts.set_sensitivity(&irq_sw   [i]);
        opts.set_sensitivity(&irq_timer[i]);
        opts.set_sensitivity(&irq_ext  [i]);

        sc_core::sc_spawn(
            [this, i]() { this->irq_aggregator(i); },
            sc_core::sc_gen_unique_name("irq_agg"),
            &opts);
    }
}

smc_cpu_cluster::~smc_cpu_cluster() = default;

// ===========================================================================
// ELF loading (testbench calls before sc_start)
// ===========================================================================
bool smc_cpu_cluster::load_elf(const std::vector<std::string>& paths)
{
    if (harts_.empty()) return false; // LCOV_EXCL_LINE num_harts is always >= 1
    return harts_[0]->load_elf(paths);
}

// ===========================================================================
// Introspection accessors
// ===========================================================================
iss_hart&        smc_cpu_cluster::hart(unsigned i)        { return *harts_[i]; }
const iss_hart&  smc_cpu_cluster::hart(unsigned i) const  { return *harts_[i]; }
WdRiscv::System<uint64_t>& smc_cpu_cluster::whisper_system() { return *whisper_sys_; }

uint64_t smc_cpu_cluster::reset_vector_n(unsigned i) const
{
    return (i < regs_.reset_vector.size()) ? regs_.reset_vector[i] : 0;
}

void smc_cpu_cluster::set_init_mem_done(bool done)
{
    regs_.init_mem_done = done ? 1u : 0u;
}

void smc_cpu_cluster::set_disable_sram_autoinit(bool disable)
{
    regs_.disable_sram_autoinit = disable ? 1u : 0u;
}

void smc_cpu_cluster::set_mem_repair_status(uint32_t v)
{
    regs_.mem_repair_status = v;
}

// ===========================================================================
// §3.9 -- inject_nmi(hart_idx)
// ===========================================================================
void smc_cpu_cluster::inject_nmi(unsigned i, uint64_t cause)
{
    if (i < harts_.size()) {
        harts_[i]->inject_nmi(cause);
        // If the hart was parked, kick it back into the step loop.
        wfi_event_[i].notify(sc_core::SC_ZERO_TIME);
    }
}

// ===========================================================================
// SC_THREAD: per-hart execution loop with temporal decoupling.
// §3.5 hart_thread + RESET_CTRL gating + step(K).
// ===========================================================================
void smc_cpu_cluster::hart_thread(unsigned i)
{
    auto& hart = *harts_[i];
    auto& qk   = qk_[i];

    hart.reset();

    while (true) {
        // §3.8 RESET_CTRL gate: lower reset_n bits release each hart when set.
        if ((regs_.reset_ctrl & (1ULL << i)) == 0) {
            qk.sync();
            sc_core::wait(core_enable_event_[i]);
            // After being re-enabled, fall through and re-evaluate.
            continue;
        }

        if (hart.is_wfi()) {
            // Park: drain local time so other initiators see it, then wait.
            qk.sync();
            sc_core::wait(wfi_event_[i] | core_enable_event_[i]);
            continue;
        }

        // Memory callbacks need to know which hart is currently running so
        // they can advance the right quantum keeper.
        current_hart_ = i;

        // §3.5 batch step: K instructions per kernel yield.
        const unsigned retired = hart.step(quantum_insts_p_.get_value());

        // Notional 1 ns per retired instruction in LT mode.  When `retired`
        // is zero (e.g. the hart parked itself on WFI mid-batch) we still
        // increment a tiny amount so the quantum keeper makes progress.
        const unsigned tick = retired ? retired : 1;
        qk.inc(sc_core::sc_time(tick, sc_core::SC_NS));
        if (qk.need_sync()) {
            qk.sync();
        }
    }
}

// ===========================================================================
// SC_METHOD: sample IRQ inputs and poke MIP.  See §3.7.
//
// NOTE on ordering: poke_mip(non-zero) is permitted to clear wfi_active_
// as a side-effect, so we must SNAPSHOT is_wfi BEFORE the poke.
// ===========================================================================
void smc_cpu_cluster::irq_aggregator(unsigned i)
{
    uint64_t mip = 0;
    if (irq_sw   [i].read()) mip |= (1ULL << 3);   // MSIP
    if (irq_timer[i].read()) mip |= (1ULL << 7);   // MTIP
    if (irq_ext  [i].read()) mip |= (1ULL << 11);  // MEIP

    const bool was_wfi = harts_[i]->is_wfi();
    harts_[i]->poke_mip(mip);

    if (mip != 0 && was_wfi) {
        harts_[i]->clear_wfi();                    // idempotent
        wfi_event_[i].notify(sc_core::SC_ZERO_TIME);
    }
}

// ===========================================================================
// Memory callbacks (§3.6).  Fast-mem first, then TLM.
// ===========================================================================
bool smc_cpu_cluster::mem_read_cb(uint64_t addr, unsigned size, uint64_t& data)
{
    if (addr >= fast_mem_lo_p_.get_value() &&
        addr + size <= fast_mem_hi_p_.get_value())
    {
        const size_t off = size_t(addr - fast_mem_lo_p_.get_value());
        data = 0;
        for (unsigned k = 0; k < size; ++k) {
            data |= (uint64_t(mem_buf_[off + k]) << (8 * k));
        }
        return true;
    }
    return tlm_access(tlm::TLM_READ_COMMAND, addr, size, data);
}

bool smc_cpu_cluster::mem_write_cb(uint64_t addr, unsigned size, uint64_t data)
{
    if (addr >= fast_mem_lo_p_.get_value() &&
        addr + size <= fast_mem_hi_p_.get_value())
    {
        const size_t off = size_t(addr - fast_mem_lo_p_.get_value());
        for (unsigned k = 0; k < size; ++k) {
            mem_buf_[off + k] = uint8_t((data >> (8 * k)) & 0xFF);
        }
        return true;
    }
    uint64_t d = data;
    return tlm_access(tlm::TLM_WRITE_COMMAND, addr, size, d);
}

// ===========================================================================
// pick_socket -- §3.6 routing decision.
// ===========================================================================
tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64>*
smc_cpu_cluster::pick_socket(uint64_t addr)
{
    if (mmio_lo_p_.get_value() < mmio_hi_p_.get_value()
        && addr >= mmio_lo_p_.get_value() && addr < mmio_hi_p_.get_value()) {
        return &mmio;
    }
    return &data;
}

// ===========================================================================
// TLM bus access helper.  Attaches smc_axi_extension and routes via
// pick_socket().  See §3.10 for the metadata convention.
// ===========================================================================
bool smc_cpu_cluster::tlm_access(tlm::tlm_command cmd, uint64_t addr,
                                 unsigned size, uint64_t& data)
{
    uint8_t buf[8] = {};
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        for (unsigned i = 0; i < size; ++i) {
            buf[i] = uint8_t((data >> (8 * i)) & 0xFF);
        }
    }

    tlm::tlm_generic_payload trans;
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(buf);
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Attach SMC AXI extension.  Lifetime rules:
    //   * `set_extension(ptr)` transfers ownership to the gp.
    //   * On gp destruction the ext is auto-deleted.
    auto* ext = new smc_axi_extension();
    ext->source_id = source_id_p_.get_value();
    ext->axi_id    = uint16_t(current_hart_);
    // Privilege drives prot[2]; current_priv() returns the live U/S/M
    // value, NOT a snapshot from the previous retired instruction.
    const uint8_t priv = harts_[current_hart_]->current_priv();
    ext->set_priv(priv != 0);     // 0 == U, anything else == privileged
    ext->set_secure(false);       // SMC does not model secure mode yet
    ext->set_fetch(false);        // see header note: fetch routed via data
    if (amo_lock_detect_p_.get_value()) {
        // §3.10 AMO/LR-SC detection.  The MEM_CALLBACKS interface does not
        // tell us whether this access is the data side of an AMO, so we
        // approximate by inspecting the most recently retired instruction.
        // This catches the AMO's load AND store halves (both happen during
        // the same singleStep so last_commit() is still the AMO).  False
        // positives are possible only if an AMO is followed by a different
        // load/store that bypasses Whisper's atomic-decode -- not a concern
        // for the SMC ISA today.
        const uint32_t op = harts_[current_hart_]->last_commit().opcode;
        const bool is_amo  = ((op & 0x7Fu) == 0x2Fu);   // AMO opcode
        const bool is_lrsc = is_amo &&
            ((op >> 27) == 0b00010 || (op >> 27) == 0b00011);   // LR/SC
        ext->set_locked(is_amo || is_lrsc);
    }
    trans.set_extension(ext);

    auto* sock = pick_socket(addr);
    auto& qk   = qk_[current_hart_];
    sc_core::sc_time delay = qk.get_local_time();

    (*sock)->b_transport(trans, delay);

    qk.set(delay);
    if (qk.need_sync()) {
        qk.sync();
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        data = 0;
        for (unsigned i = 0; i < size; ++i) {
            data |= (uint64_t(buf[i]) << (8 * i));
        }
    }

    return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

// ===========================================================================
// ctrl target socket b_transport handler -- cpu_ctrl.rdl register file.
//
// Reads of unmapped offsets return 0; writes to RO offsets are silently dropped.
// Writes to RESET_VECTOR[i] update
// the iss_hart's reset_pc immediately (effective on next reset).
// ===========================================================================
void smc_cpu_cluster::ctrl_b_transport(tlm::tlm_generic_payload& trans,
                                       sc_core::sc_time& /*delay*/)
{
    const uint64_t ctrl_size = ctrl_size_bytes_p_.get_value();
    if (ctrl_size == 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const uint64_t off  = trans.get_address() % ctrl_size;
    const unsigned len  = trans.get_data_length();
    uint8_t* const ptr  = trans.get_data_ptr();

    auto load_u32 = [&]() -> uint32_t {
        uint32_t v = 0;
        for (unsigned k = 0; k < len && k < 4; ++k) {
            v |= (uint32_t(ptr[k]) << (8 * k));
        }
        return v;
    };
    auto store_u32 = [&](uint32_t v) {
        for (unsigned k = 0; k < len && k < 4; ++k) {
            ptr[k] = uint8_t((v >> (8 * k)) & 0xFF);
        }
    };
    auto load_u64 = [&]() -> uint64_t {
        uint64_t v = 0;
        for (unsigned k = 0; k < len && k < 8; ++k) {
            v |= (uint64_t(ptr[k]) << (8 * k));
        }
        return v;
    };
    auto store_u64 = [&](uint64_t v) {
        for (unsigned k = 0; k < len && k < 8; ++k) {
            ptr[k] = uint8_t((v >> (8 * k)) & 0xFF);
        }
    };

    // RESET_VECTOR[i] occupies 8 bytes each starting at 0x000.
    if (off < 0x020) {
        const unsigned idx = unsigned(off / 8);
        if (idx >= 4) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            store_u64(regs_.reset_vector[idx]);
        } else {
            const uint64_t v = load_u64();
            regs_.reset_vector[idx] = v;
            if (idx < harts_.size()) {
                harts_[idx]->set_reset_pc(v);   // effective on next reset
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    auto handle_u64 = [&](uint64_t& reg, bool ro) {
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            store_u64(reg);
        } else if (!ro) {
            reg = load_u64();
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    };
    auto handle_u32 = [&](uint32_t& reg, bool ro) {
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            store_u32(reg);
        } else if (!ro) {
            reg = load_u32();
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    };

    switch (off) {
    case 0x020:  // RESET_CTRL
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            store_u64(regs_.reset_ctrl);
        } else {
            apply_reset_ctrl(load_u64());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    case 0x028: handle_u64(regs_.core_reset_pulse_count, /*ro*/ false); return;
    case 0x030: handle_u64(regs_.reset_timeout,          /*ro*/ false); return;
    case 0x040: handle_u64(regs_.reference_counter,      /*ro*/ false); return;
    case 0x050: handle_u32(regs_.wdt_timeout,            /*ro*/ false); return;
    case 0x058:  // WDT_TIMEOUT_RESET single-pulse bits; reads as zero in LT.
        if (trans.get_command() == tlm::TLM_READ_COMMAND)
            store_u32(0);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    case 0x060: handle_u32(regs_.test_ctrl,              /*ro*/ true ); return;
    default:
        break;
    }

    if (off >= 0x200 && off < 0x208) {
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            uint64_t v = regs_.smc_attributes >> (8 * (off - 0x200));
            store_u64(v);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (off >= 0x080 && off < 0x100) {
        handle_u32(regs_.scratch[(off - 0x080) / 8], /*ro*/ false);
        return;
    }

    if (off >= 0x100 && off < 0x200) {
        const unsigned idx = unsigned((off - 0x100) / 8);
        if (idx < regs_.wb_pc.size()) {
            uint64_t& pc = regs_.wb_pc[idx];
            if (idx < harts_.size()) pc = harts_[idx]->get_pc();
            handle_u64(pc, /*ro*/ true);
            return;
        }
    }

    if (off >= 0x240 && off < 0x260) {
        const unsigned idx = unsigned((off - 0x240) / 8);
        uint64_t v = 0;
        if (idx < regs_.mutex_available.size()) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                v = regs_.mutex_available[idx] ? 1u : 0u;
                regs_.mutex_available[idx] = false;
                store_u64(v);
            } else {
                regs_.mutex_available[idx] = true;
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
    }

    if (off >= 0x260 && off < 0x280) {
        const unsigned idx = unsigned((off - 0x260) / 8);
        if (idx < regs_.sema.size()) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                store_u32(static_cast<uint16_t>(regs_.sema[idx]));
            } else {
                regs_.sema[idx] = static_cast<int16_t>(
                    regs_.sema[idx] + static_cast<int16_t>(load_u32() & 0xFFFFu));
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
    }

    if (off >= 0x280 && off < 0x2A0) {
        const unsigned idx = unsigned((off - 0x280) / 8);
        handle_u64(regs_.dummy_rom[idx], /*ro*/ false);
        return;
    }

    if (off >= 0x2A0 && off < 0x2C0) {
        const unsigned idx = unsigned((off - 0x2A0) / 8);
        handle_u64(regs_.dummy_rom_null[idx], /*ro*/ false);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::memset(ptr, 0, len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);   // RAZ/WI
    return;
}

// ===========================================================================
// RESET_CTRL write logic -- §3.8 + §3.4 enable/park bookkeeping.
// ===========================================================================
void smc_cpu_cluster::apply_reset_ctrl(uint64_t new_value)
{
    const uint64_t old_value = regs_.reset_ctrl;
    regs_.reset_ctrl         = new_value;

    for (unsigned i = 0; i < num_harts_p_.get_value(); ++i) {
        const bool was_en = (old_value & (1ULL << i)) != 0;
        const bool is_en  = (new_value & (1ULL << i)) != 0;
        if (!was_en && is_en) {
            // 0 -> 1 transition: wake the parked hart_thread.
            core_enable_event_[i].notify(sc_core::SC_ZERO_TIME);
        }
        // 1 -> 0 transition: hart_thread will sample the bit at the top
        // of its loop and park itself.  No explicit kick needed.
        (void)was_en;
    }
}

} // namespace smc
