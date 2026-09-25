// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file cpu_ctrl.cpp
 * @brief SMC CPU Control — SystemC/TLM-2.0 LT implementation.
 */

#include "cpu_ctrl.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace smc {

namespace {

constexpr uint64_t kBootRomResetVector = 0x0000'0000'C004'0000ULL;

bool naturally_aligned(uint64_t addr, unsigned len)
{
    if (len == 0 || (len & (len - 1)) != 0) return false;
    return (addr & (len - 1)) == 0;
}

uint64_t mask_upper_unused(uint64_t v, unsigned valid_bytes)
{
    if (valid_bytes >= 8) return v;
    const uint64_t mask = (1ULL << (valid_bytes * 8)) - 1ULL;
    return v & mask;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

cpu_ctrl::cpu_ctrl(sc_core::sc_module_name name, cpu_ctrl_cfg cfg)
    : sc_core::sc_module(name)
    , base_addr_p_(
          "base_addr",
          cfg.base_addr,
          "Absolute SMC CPU-control base address.  Transactions whose "
          "address falls in [base_addr, base_addr + WINDOW_SIZE) are "
          "decoded relative to base_addr; offset-only accesses are also "
          "accepted for standalone benches.")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM b_transport annotated delay in nanoseconds.")
    , wdt_stage2_tick_ns_p_(
          "wdt_stage2_tick_ns",
          cfg.wdt_stage2_tick_ns,
          "Stage-2 WDT countdown tick period in nanoseconds; 0 disables auto-tick.")
    , cfg_(cfg)
{
    cfg_.base_addr       = base_addr_p_.get_value();
    cfg_.access_delay_ns = access_delay_ns_p_.get_value();
    cfg_.wdt_stage2_tick_ns = wdt_stage2_tick_ns_p_.get_value();

    base_addr_p_.add_metadata("rdl_block", cci::cci_value(std::string("cpu_ctrl")));
    base_addr_p_.add_metadata("default",   cci::cci_value(std::string("0xC0039000")));
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    wdt_timeout_cluster_i.init(cpu_ctrl_cfg::NUM_CORES);
    wdt_stage2_count_.fill(0x4000);
    wdt_stage2_reload_pulse_.fill(false);
    wdt_stage2_tick_period_ =
        sc_core::sc_time(cfg_.wdt_stage2_tick_ns, sc_core::SC_NS);

    // Fix regmodel register masks/callbacks for the module's lifetime here;
    // reset_regs() below (and on soft reset) only resets the stored values
    // via Register<Word>::reset() -- see .cursor/rules/register-access-helpers.mdc.

    // SCRATCH[16]: plain RW32, no side effects.
    for (auto& r : scratch_)
        r = regmodel::Register32{0xFFFF'FFFFu, 0xFFFF'FFFFu, 0u};

    // DUMMY_ROM_*: plain RW64, no side effects.
    dummy_rom_0_ = regmodel::Register64{~0ULL, ~0ULL, 0ULL};
    dummy_rom_1_ = regmodel::Register64{~0ULL, ~0ULL, 0ULL};
    dummy_rom_2_ = regmodel::Register64{~0ULL, ~0ULL, 0ULL};
    dummy_rom_3_ = regmodel::Register64{~0ULL, ~0ULL, 0ULL};
    for (auto& r : dummy_rom_null_)
        r = regmodel::Register64{~0ULL, ~0ULL, 0ULL};

    // WB_PC: SW read-only (write_mask=0 -> Register<Word>::write() is a
    // structural no-op); HW updates go through set_wb_pc()'s set_raw().
    for (auto& core : wb_pc_)
        for (auto& r : core)
            r = regmodel::Register64{~0ULL, 0ULL, 0ULL};

    // MUTEX: 1-bit availability flag. SW write always forces it back to
    // available (1) regardless of the data written.
    for (auto& r : mutex_) {
        r = regmodel::Register32{0x1u, 0x1u, 1u};
        r.on_write([](uint32_t /*cur*/, uint32_t /*in*/) { return 1u; });
    }

    reset_regs();

    reg_socket.register_b_transport(this, &cpu_ctrl::b_transport);
    reg_socket.register_transport_dbg(this, &cpu_ctrl::transport_dbg);

    SC_METHOD(wdt_stage2_tick_method);
    sensitive << wdt_stage2_tick_event_;
    dont_initialize();

    SC_METHOD(wdt_stage2_output_method);
    sensitive << wdt_stage2_recompute_event_;
    dont_initialize();

    SC_METHOD(wdt_stage2_input_method);
    sensitive << rst_primary_n_i;
    for (unsigned i = 0; i < cpu_ctrl_cfg::NUM_CORES; ++i) {
        sensitive << wdt_timeout_cluster_i[i];
    }
    dont_initialize();

    if (wdt_stage2_tick_period_ != sc_core::SC_ZERO_TIME) {
        wdt_stage2_tick_event_.notify(wdt_stage2_tick_period_);
    }

    SC_REPORT_INFO(name,
        ("cpu_ctrl instantiated: base_addr=0x" +
         ([&] {
             std::ostringstream os;
             os << std::hex << cfg_.base_addr;
             return os.str();
         })() +
         ", access_delay_ns=" + std::to_string(cfg_.access_delay_ns)).c_str());
}

void cpu_ctrl::reset_regs()
{
    reset_vector_.fill(kBootRomResetVector);

    // RESET_CTRL defaults: core reset de-asserted, uncore de-asserted, debug held in reset.
    reset_ctrl_ = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 8);

    core_reset_pulse_count_ = (0x10ULL << 16) | 0x8ULL | (0xFULL << 32);
    reset_timeout_          = 0;
    reference_counter_      = 0;
    wdt_timeout_            = 0x4000;
    wdt_timeout_reset_      = 0;
    wdt_stage2_count_.fill(static_cast<uint32_t>(wdt_timeout_));
    wdt_stage2_reload_pulse_.fill(false);
    wdt_first_timeout_  = false;
    wdt_second_timeout_ = false;
    for (auto& r : scratch_) r.reset(0);
    test_ctrl_              = 0;
    for (auto& core : wb_pc_)
        for (auto& r : core) r.reset(0);
    smc_attributes_         = 0;
    for (auto& r : mutex_) r.reset(1);  // available
    sema_.fill(0);
    dummy_rom_0_.reset(0x0145'0513'0000'0517ULL);
    dummy_rom_1_.reset(0xFFF0'0693'3055'1073ULL);
    dummy_rom_2_.reset(0x1050'0073'3046'B073ULL);
    dummy_rom_3_.reset(0x0000'0000'FFDF'F06FULL);
    for (auto& r : dummy_rom_null_) r.reset(0);
}

uint64_t cpu_ctrl::normalize_addr(uint64_t addr) const
{
    const uint64_t base = base_addr_p_.get_value();
    if (addr >= base && addr < base + cpu_ctrl_cfg::WINDOW_SIZE)
        return addr - base;
    return addr;
}

// ---------------------------------------------------------------------------
// Register access helpers
// ---------------------------------------------------------------------------

uint64_t cpu_ctrl::read_qword(uint64_t off) const
{
    if (off >= cpu_ctrl_cfg::OFF_RESET_VECTOR &&
        off < cpu_ctrl_cfg::OFF_RESET_VECTOR + 8 * cpu_ctrl_cfg::NUM_CORES) {
        return reset_vector_[(off - cpu_ctrl_cfg::OFF_RESET_VECTOR) / 8];
    }
    switch (off) {
    case cpu_ctrl_cfg::OFF_RESET_CTRL:             return reset_ctrl_;
    case cpu_ctrl_cfg::OFF_CORE_RESET_PULSE_COUNT: return core_reset_pulse_count_;
    case cpu_ctrl_cfg::OFF_RESET_TIMEOUT:          return reset_timeout_;
    case cpu_ctrl_cfg::OFF_REFERENCE_COUNTER:      return reference_counter_;
    case cpu_ctrl_cfg::OFF_WDT_TIMEOUT:            return wdt_timeout_;
    case cpu_ctrl_cfg::OFF_WDT_TIMEOUT_RESET:      return 0;  // singlepulse reads as 0
    case cpu_ctrl_cfg::OFF_TEST_CTRL:              return test_ctrl_;
    case cpu_ctrl_cfg::OFF_SMC_ATTRIBUTES:         return smc_attributes_;
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_0:            return dummy_rom_0_.read();
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_1:            return dummy_rom_1_.read();
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_2:            return dummy_rom_2_.read();
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_3:            return dummy_rom_3_.read();
    default:
        break;
    }

    if (off >= cpu_ctrl_cfg::OFF_SCRATCH &&
        off < cpu_ctrl_cfg::OFF_SCRATCH + 8 * CPU_CTRL_SCRATCH_COUNT) {
        return scratch_[(off - cpu_ctrl_cfg::OFF_SCRATCH) / 8].read();
    }

    for (unsigned c = 0; c < cpu_ctrl_cfg::NUM_CORES; ++c) {
        const uint64_t base =
            cpu_ctrl_cfg::OFF_WB_PC_CORE0 + uint64_t(c) * 0x40u;
        if (off >= base && off < base + 8 * cpu_ctrl_cfg::WB_PC_PER_CORE)
            return wb_pc_[c][(off - base) / 8].read();
    }

    if (off >= cpu_ctrl_cfg::OFF_MUTEX &&
        off < cpu_ctrl_cfg::OFF_MUTEX + 8 * cpu_ctrl_cfg::NUM_MUTEX) {
        // Side-effect-free peek (dbg_reg()/transport_dbg()); the atomic
        // test-and-set for real bus reads lives in b_transport().
        const unsigned idx = static_cast<unsigned>((off - cpu_ctrl_cfg::OFF_MUTEX) / 8);
        return mutex_[idx].read();
    }

    if (off >= cpu_ctrl_cfg::OFF_SEMA &&
        off < cpu_ctrl_cfg::OFF_SEMA + 8 * cpu_ctrl_cfg::NUM_SEMA) {
        const unsigned idx = static_cast<unsigned>((off - cpu_ctrl_cfg::OFF_SEMA) / 8);
        return sema_[idx];
    }

    if (off >= cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL &&
        off < cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL +
                  8 * cpu_ctrl_cfg::DUMMY_ROM_NULLS) {
        return dummy_rom_null_[(off - cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL) / 8].read();
    }

    return 0;  // hole: RAZ
}

bool cpu_ctrl::write_qword(uint64_t off, uint64_t val, unsigned byte_off,
                           unsigned len)
{
    const uint64_t mask64 =
        (len >= 8) ? ~0ULL
                   : ((1ULL << (len * 8)) - 1ULL) << (byte_off * 8);
    const uint64_t insert = (val << (byte_off * 8)) & mask64;

    auto merge = [&](uint64_t& dst, uint64_t new_val) {
        dst = (dst & ~mask64) | (new_val & mask64);
    };

    if (off >= cpu_ctrl_cfg::OFF_RESET_VECTOR &&
        off < cpu_ctrl_cfg::OFF_RESET_VECTOR + 8 * cpu_ctrl_cfg::NUM_CORES) {
        auto& dst = reset_vector_[(off - cpu_ctrl_cfg::OFF_RESET_VECTOR) / 8];
        merge(dst, insert | (dst & ~mask64));
        dst &= 0x00FF'FFFF'FFFF'FFFFULL;  // vector[55:0]
        return true;
    }

    switch (off) {
    case cpu_ctrl_cfg::OFF_RESET_CTRL: {
        merge(reset_ctrl_, insert);
        // Single-pulse core reset request bits self-clear (functional abstraction).
        reset_ctrl_ &= ~((1u << 4) | (1u << 5) | (1u << 6) | (1u << 7));
        return true;
    }
    case cpu_ctrl_cfg::OFF_CORE_RESET_PULSE_COUNT: {
        merge(core_reset_pulse_count_, insert);
        core_reset_pulse_count_ |= (0xFULL << 32);  // core_resets_done RO
        return true;
    }
    case cpu_ctrl_cfg::OFF_RESET_TIMEOUT: {
        // timeout_value[15:0] + timeout_mode[16] are RW; status bits [32]/[36] RO.
        constexpr uint64_t k_rw = 0x1FFFFULL;
        constexpr uint64_t k_ro = (1ULL << 32) | (1ULL << 36);
        const uint64_t merged = (reset_timeout_ & ~mask64) | (insert & mask64);
        reset_timeout_ = (reset_timeout_ & k_ro) | (merged & k_rw);
        return true;
    }
    case cpu_ctrl_cfg::OFF_REFERENCE_COUNTER:
        merge(reference_counter_, insert);
        return true;
    case cpu_ctrl_cfg::OFF_WDT_TIMEOUT:
        merge(wdt_timeout_, insert);
        wdt_timeout_ &= 0xFFFF'FFFFULL;
        return true;
    case cpu_ctrl_cfg::OFF_WDT_TIMEOUT_RESET:
        apply_wdt_timeout_reset(static_cast<uint32_t>(insert & 0xFu));
        wdt_timeout_reset_ = 0;  // singlepulse
        return true;
    case cpu_ctrl_cfg::OFF_TEST_CTRL:
        return true;  // SW read-only
    case cpu_ctrl_cfg::OFF_SMC_ATTRIBUTES:
        return true;  // SW read-only
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_0:
        dummy_rom_0_.write((dummy_rom_0_.raw() & ~mask64) | (insert & mask64));
        return true;
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_1:
        dummy_rom_1_.write((dummy_rom_1_.raw() & ~mask64) | (insert & mask64));
        return true;
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_2:
        dummy_rom_2_.write((dummy_rom_2_.raw() & ~mask64) | (insert & mask64));
        return true;
    case cpu_ctrl_cfg::OFF_DUMMY_ROM_3:
        dummy_rom_3_.write((dummy_rom_3_.raw() & ~mask64) | (insert & mask64));
        return true;
    default:
        break;
    }

    if (off >= cpu_ctrl_cfg::OFF_SCRATCH &&
        off < cpu_ctrl_cfg::OFF_SCRATCH + 8 * CPU_CTRL_SCRATCH_COUNT) {
        auto&          reg    = scratch_[(off - cpu_ctrl_cfg::OFF_SCRATCH) / 8];
        const uint64_t merged = (uint64_t{reg.raw()} & ~mask64) | (insert & mask64);
        reg.write(static_cast<uint32_t>(merged));  // data[31:0]; upper qword half is WI
        return true;
    }

    for (unsigned c = 0; c < cpu_ctrl_cfg::NUM_CORES; ++c) {
        const uint64_t base =
            cpu_ctrl_cfg::OFF_WB_PC_CORE0 + uint64_t(c) * 0x40u;
        if (off >= base && off < base + 8 * cpu_ctrl_cfg::WB_PC_PER_CORE)
            return true;  // SW read-only
    }

    if (off >= cpu_ctrl_cfg::OFF_MUTEX &&
        off < cpu_ctrl_cfg::OFF_MUTEX + 8 * cpu_ctrl_cfg::NUM_MUTEX) {
        // Any write releases -- the on_write callback ignores the data.
        const unsigned idx = static_cast<unsigned>((off - cpu_ctrl_cfg::OFF_MUTEX) / 8);
        mutex_[idx].write(1u);
        return true;
    }

    if (off >= cpu_ctrl_cfg::OFF_SEMA &&
        off < cpu_ctrl_cfg::OFF_SEMA + 8 * cpu_ctrl_cfg::NUM_SEMA) {
        const unsigned idx = static_cast<unsigned>((off - cpu_ctrl_cfg::OFF_SEMA) / 8);
        const int16_t  delta =
            static_cast<int16_t>(mask_upper_unused(val >> (byte_off * 8), len) & 0xFFFFu);
        sema_[idx] = static_cast<uint16_t>(sema_[idx] + delta);
        return true;
    }

    if (off >= cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL &&
        off < cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL +
                  8 * cpu_ctrl_cfg::DUMMY_ROM_NULLS) {
        auto& reg = dummy_rom_null_[(off - cpu_ctrl_cfg::OFF_DUMMY_ROM_NULL) / 8];
        reg.write((reg.raw() & ~mask64) | (insert & mask64));
        return true;
    }

    return true;  // hole: WI
}

bool cpu_ctrl::reg_read(uint64_t off, unsigned len, uint8_t* buf) const
{
    if (off + len > cpu_ctrl_cfg::WINDOW_SIZE) return false;

     // 8-byte aligned so for 64-bit reads, we can read the entire 64-bit word at once
    const uint64_t aligned = off & ~7ULL;
    // compute byte offset within the 64-bit word
    const unsigned sub     = static_cast<unsigned>(off & 7u);
    uint64_t       word    = read_qword(aligned);

    if (aligned == cpu_ctrl_cfg::OFF_CORE_RESET_PULSE_COUNT)
        word |= (0xFULL << 32);

    // copy the desired number of bytes from the word to the buffer
    std::memcpy(buf, reinterpret_cast<const uint8_t*>(&word) + sub, len);
    return true;
}

bool cpu_ctrl::reg_write(uint64_t off, unsigned len, const uint8_t* buf)
{
    if (off + len > cpu_ctrl_cfg::WINDOW_SIZE) return false;

    const uint64_t aligned = off & ~7ULL;
    const unsigned sub     = static_cast<unsigned>(off & 7u);

    uint64_t raw = 0;
    std::memcpy(&raw, buf, len);
    return write_qword(aligned, raw, sub, len);
}

// ---------------------------------------------------------------------------
// Debug / API
// ---------------------------------------------------------------------------

uint64_t cpu_ctrl::dbg_reg(uint64_t byte_off) const
{
    const uint64_t off = normalize_addr(byte_off);
    if (off + 8 > cpu_ctrl_cfg::WINDOW_SIZE) return 0;
    return read_qword(off & ~7ULL);
}

void cpu_ctrl::set_smc_attributes(uint64_t v) { smc_attributes_ = v; }

void cpu_ctrl::set_test_ctrl(uint32_t v) { test_ctrl_ = v; }

void cpu_ctrl::set_wb_pc(unsigned core, unsigned slot, uint64_t pc)
{
    if (core >= cpu_ctrl_cfg::NUM_CORES ||
        slot >= cpu_ctrl_cfg::WB_PC_PER_CORE)
        return;
    wb_pc_[core][slot].set_raw(pc & 0x00FF'FFFF'FFFF'FFFFULL);
}

uint32_t cpu_ctrl::scratch(uint64_t idx) const
{
    if (idx >= CPU_CTRL_SCRATCH_COUNT) return 0;
    return scratch_[idx].read();
}

void cpu_ctrl::set_scratch(uint64_t idx, uint32_t v)
{
    if (idx >= CPU_CTRL_SCRATCH_COUNT) return;
    scratch_[idx].set_raw(v);
}

void cpu_ctrl::dump_state(std::ostream& os) const
{
    os << "cpu_ctrl @ " << sc_core::sc_time_stamp() << "\n"
       << std::hex << std::setfill('0')
       << "  RESET_VECTOR[0] = 0x" << std::setw(16) << reset_vector_[0] << "\n"
       << "  SCRATCH[8]  = 0x" << std::setw(8) << scratch(8) << " (manifest)\n"
       << "  SCRATCH[9]  = 0x" << std::setw(8) << scratch(9) << " (status)\n"
       << "  SCRATCH[11] = 0x" << std::setw(8) << scratch(11) << " (status buf)\n"
       << "  SCRATCH[13] = 0x" << std::setw(8) << scratch(13) << " (sep off)\n"
       << "  SCRATCH[14] = 0x" << std::setw(8) << scratch(14) << " (sep size)\n"
       << "  SCRATCH[15] = 0x" << std::setw(8) << scratch(15) << " (mem repair)\n"
       << std::dec << std::setfill(' ');
}

// ---------------------------------------------------------------------------
// TLM
// ---------------------------------------------------------------------------

void cpu_ctrl::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = normalize_addr(gp.get_address());
    const unsigned         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (buf == nullptr) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (!naturally_aligned(adr, len) || (len != 1 && len != 2 && len != 4 && len != 8)) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_streaming_width() != len) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_byte_enable_ptr() != nullptr && gp.get_byte_enable_length() != 0) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }
    if (adr + len > cpu_ctrl_cfg::WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    bool ok = false;
    if (cmd == tlm::TLM_READ_COMMAND &&
        adr >= cpu_ctrl_cfg::OFF_MUTEX &&
        adr < cpu_ctrl_cfg::OFF_MUTEX + 8 * cpu_ctrl_cfg::NUM_MUTEX &&
        (adr & 7u) == 0) {
        const unsigned idx =
            static_cast<unsigned>((adr - cpu_ctrl_cfg::OFF_MUTEX) / 8);
        uint64_t result = 0;
        if (mutex_[idx].raw() == 1) {
            mutex_[idx].set_raw(0);
            result = 1;
        } else {
            result = 0;
        }
        std::memcpy(buf, &result, len);
        ok = true;
    } else {
        ok = (cmd == tlm::TLM_READ_COMMAND) ? reg_read(adr, len, buf)
                                            : reg_write(adr, len, buf);
    }
    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                            : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false);
}

unsigned int cpu_ctrl::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = normalize_addr(gp.get_address());
    const unsigned         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (!naturally_aligned(adr, len) || (len != 1 && len != 2 && len != 4 && len != 8))
        return 0;
    if (adr + len > cpu_ctrl_cfg::WINDOW_SIZE) return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        const uint64_t aligned = adr & ~7ULL;
        const unsigned sub     = static_cast<unsigned>(adr & 7u);
        const uint64_t word    = read_qword(aligned);
        std::memcpy(buf, reinterpret_cast<const uint8_t*>(&word) + sub, len);
        return len;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        if (!reg_write(adr, len, buf)) return 0;
        return len;
    }
    return 0;
}

void cpu_ctrl::schedule_wdt_stage2_recompute()
{
    wdt_stage2_recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void cpu_ctrl::apply_wdt_timeout_reset(uint32_t pulse_bits)
{
    for (unsigned i = 0; i < cpu_ctrl_cfg::NUM_CORES; ++i) {
        if ((pulse_bits >> i) & 1u) {
            wdt_stage2_reload_pulse_[i] = true;
            wdt_stage2_count_[i] =
                static_cast<uint32_t>(wdt_timeout_ & 0xFFFF'FFFFu);
        }
    }
    // Reload / refresh outputs only — do not advance the countdown.
    wdt_stage2_step_once(/*do_decrement=*/false);
    schedule_wdt_stage2_recompute();
}

void cpu_ctrl::wdt_stage2_step_once(bool do_decrement)
{
    bool any_first  = false;
    bool any_second = false;
    for (unsigned i = 0; i < cpu_ctrl_cfg::NUM_CORES; ++i) {
        const bool sticky = wdt_timeout_cluster_i[i].read();
        const bool reload =
            !rst_primary_n_i.read() ||
            wdt_stage2_reload_pulse_[i] ||
            !sticky;
        if (reload) {
            wdt_stage2_count_[i] =
                static_cast<uint32_t>(wdt_timeout_ & 0xFFFF'FFFFu);
            wdt_stage2_reload_pulse_[i] = false;
        } else if (do_decrement && sticky && wdt_stage2_count_[i] != 0) {
            --wdt_stage2_count_[i];
        }
        any_first  = any_first || sticky;
        any_second = any_second || (wdt_stage2_count_[i] == 0);
    }
    wdt_first_timeout_  = any_first;
    wdt_second_timeout_ = any_second;
}

void cpu_ctrl::wdt_stage2_tick_method()
{
    if (!rst_primary_n_i.read()) {
        wdt_stage2_count_.fill(static_cast<uint32_t>(wdt_timeout_ & 0xFFFF'FFFFu));
        wdt_first_timeout_  = false;
        wdt_second_timeout_ = false;
        schedule_wdt_stage2_recompute();
    } else {
        wdt_stage2_step_once();
        schedule_wdt_stage2_recompute();
    }
    if (wdt_stage2_tick_period_ != sc_core::SC_ZERO_TIME) {
        wdt_stage2_tick_event_.notify(wdt_stage2_tick_period_);
    }
}

void cpu_ctrl::wdt_stage2_input_method()
{
    // Sticky / primary-reset edges reload or refresh outputs; countdown
    // advances only on the periodic (or dbg) tick path.
    wdt_stage2_step_once(/*do_decrement=*/false);
    schedule_wdt_stage2_recompute();
}

void cpu_ctrl::wdt_stage2_output_method()
{
    if (wdt_first_timeout_ != wdt_first_cache_) {
        wdt_first_timeout_o.write(wdt_first_timeout_);
        wdt_first_cache_ = wdt_first_timeout_;
    }
    if (wdt_second_timeout_ != wdt_second_cache_) {
        wdt_second_timeout_o.write(wdt_second_timeout_);
        wdt_second_cache_ = wdt_second_timeout_;
    }
}

void cpu_ctrl::dbg_wdt_stage2_tick(unsigned n)
{
    for (unsigned k = 0; k < n; ++k) {
        wdt_stage2_step_once();
    }
    schedule_wdt_stage2_recompute();
}

}  // namespace smc
