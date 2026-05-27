// SPDX-License-Identifier: Apache-2.0
/**
 * @file clint.cpp
 * @brief SMC CLINT — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/clint.h` for the full design description, register map, and
 * compliance notes.
 */

#include "clint.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace smc {

namespace {

/// Mask applied to MSIP writes — only bit[0] is the IPI; bits[31:1] RAZ/WI.
/// This is the explicit `MSIP.value[0:0]` field width from `clint.rdl`.
constexpr uint32_t MSIP_BIT_MASK = 0x1u;

/// 0xFFFF_FFFF_FFFF_FFFF — used as the post-reset MTIMECMP value so MTIP is
/// guaranteed deasserted until firmware programs a real comparator.
constexpr uint64_t MTIMECMP_RESET_VALUE =
    std::numeric_limits<uint64_t>::max();

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

clint::clint(sc_core::sc_module_name name, clint_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params constructed first (declared before public ports in clint.h).
    // The cfg.* values become DEFAULTS; broker presets override them.
    , num_harts_p_(
          "num_harts",
          cfg.num_harts,
          "Number of harts (1..4095). Sizes msip_o, mtip_o, MSIP[], MTIMECMP[]. "
          "SMC default: 4 cores per chip (tt-oca-hw.pdf §6.6.6).")
    , tick_period_ns_p_(
          "tick_period_ns",
          cfg.tick_period_ns,
          "MTIME auto-tick period in nanoseconds. "
          "Default 100 ns ⇒ 10 MHz; set to 0.0 to disable auto-tick "
          "(test-only mode where MTIME is advanced via dbg_set_mtime / writes).")
    , access_delay_ns_p_(
          "access_delay_ns",
          2.0,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    // Ports sized from the (possibly preset-overridden) CCI param values.
    , reg_socket("reg_socket")
    , msip_o   ("msip_o", num_harts_p_.get_value())
    , mtip_o   ("mtip_o", num_harts_p_.get_value())
    , rst_n_i  ("rst_n_i")
    , cfg_(cfg)
{
    // Sync cfg_ with CCI-resolved values so the rest of the constructor and
    // all member functions that read cfg_.* see consistent data.
    cfg_.num_harts      = num_harts_p_.get_value();
    cfg_.tick_period_ns = tick_period_ns_p_.get_value();

    // Provenance metadata — visible to introspection tools and inspectors.
    num_harts_p_.add_metadata("rdl_dimension",
                              cci::cci_value(std::string("MSIP[N], MTIMECMP[N]")));
    num_harts_p_.add_metadata("smc_default", cci::cci_value(4u));
    num_harts_p_.add_metadata("valid_range", cci::cci_value(std::string("1..4095")));
    tick_period_ns_p_.add_metadata("unit",   cci::cci_value(std::string("nanoseconds")));
    tick_period_ns_p_.add_metadata("rtl_signal",
                                   cci::cci_value(std::string("io_rtcTick (period)")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    if (cfg_.num_harts == 0 || cfg_.num_harts > 4095) {
        SC_REPORT_FATAL(name, "CLINT num_harts must be in 1..4095");
    }
    if (cfg_.tick_period_ns < 0.0) {
        SC_REPORT_FATAL(name, "CLINT tick_period_ns must be >= 0");
    }

    // Allocate state arrays.
    mtimecmp_.assign  (cfg_.num_harts, MTIMECMP_RESET_VALUE);
    msip_.assign      (cfg_.num_harts, 0);
    msip_cache_.assign(cfg_.num_harts, false);
    mtip_cache_.assign(cfg_.num_harts, false);

    // Cache time-typed CCI values to avoid repeated double→sc_time conversion.
    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    tick_period_  = sc_core::sc_time(cfg_.tick_period_ns,             sc_core::SC_NS);

    // TLM target socket callbacks.
    reg_socket.register_b_transport  (this, &clint::b_transport);
    reg_socket.register_transport_dbg(this, &clint::transport_dbg);

    // SC_METHOD: reset on rst_n_i value change.
    // Use the port directly (not value_changed_event()) so the sensitivity
    // is captured before the port has been bound — SystemC resolves the
    // event after elaboration completes.
    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    // SC_METHOD: tick MTIME.
    SC_METHOD(tick_method);
    sensitive << tick_event_;
    dont_initialize();

    // SC_METHOD: sole driver of msip_o[*] / mtip_o[*].
    SC_METHOD(output_method);
    sensitive << recompute_event_;
    dont_initialize();

    // Schedule first tick (if auto-tick enabled).
    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.notify(tick_period_);
    }

    SC_REPORT_INFO(name,
        ("CLINT instantiated with num_harts=" + std::to_string(cfg_.num_harts) +
         ", tick_period_ns=" + std::to_string(cfg_.tick_period_ns) +
         ", access_delay_ns=" + std::to_string(access_delay_ns_p_.get_value())).c_str());
}

// ---------------------------------------------------------------------------
// SC_METHOD: reset_proc
// ---------------------------------------------------------------------------
//
// Synchronous active-low: when rst_n_i goes low, zero MTIME / MSIP, and reset
// MTIMECMP to all-1s so MTIP is deasserted.  Writes nothing to outputs
// directly — instead schedules a recompute so output_method remains the
// sole driver.

void clint::reset_proc()
{
    if (rst_n_i.read()) {
        return; // de-assertion edge: nothing to do
    }

    mtime_ = 0;
    std::fill(mtimecmp_.begin(), mtimecmp_.end(), MTIMECMP_RESET_VALUE);
    std::fill(msip_.begin(),     msip_.end(),     0);

    // Cancel any pending tick that was scheduled before reset, then re-arm.
    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.cancel();
        tick_event_.notify(tick_period_);
    }

    schedule_recompute();
}

// ---------------------------------------------------------------------------
// SC_METHOD: tick_method
// ---------------------------------------------------------------------------
//
// Increment MTIME, recompute outputs, and self-arm for the next tick.
// In LT modelling we use one event per tick rather than a continuous clock;
// this is dramatically cheaper for long simulations (firmware idle loops
// frequently span millions of ticks where nothing else changes).

void clint::tick_method()
{
    if (rst_n_i.read() == false) {
        return; // do not tick while in reset
    }

    ++mtime_;
    schedule_recompute();

    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.notify(tick_period_);
    }
}

// ---------------------------------------------------------------------------
// SC_METHOD: output_method
// ---------------------------------------------------------------------------
//
// Sole driver of msip_o[*] and mtip_o[*].  Idempotent — only writes when the
// computed level differs from the cached previous value, which avoids
// spurious value-changed events on downstream consumers.

void clint::output_method()
{
    for (unsigned h = 0; h < cfg_.num_harts; ++h) {
        const bool msip_lvl = (msip_[h] & MSIP_BIT_MASK) != 0;
        const bool mtip_lvl = (mtime_ >= mtimecmp_[h]);

        if (msip_lvl != msip_cache_[h]) {
            msip_o[h].write(msip_lvl);
            msip_cache_[h] = msip_lvl;
        }
        if (mtip_lvl != mtip_cache_[h]) {
            mtip_o[h].write(mtip_lvl);
            mtip_cache_[h] = mtip_lvl;
        }
    }
}

void clint::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

// ---------------------------------------------------------------------------
// reg_read / reg_write — register decode
// ---------------------------------------------------------------------------
//
// `access_size` is 4 (32-bit) or 8 (64-bit).  All higher layers must have
// validated the size and alignment before calling.
//
// The decode is a four-way switch on the offset region:
//   1. MSIP[h]      — 4-byte only
//   2. MTIMECMP[h]  — 4-byte (low/high half) or 8-byte
//   3. MTIME        — 4-byte (low/high half) or 8-byte
//   4. anything else inside the window — RAZ/WI

bool clint::reg_read(uint64_t off, unsigned access_size,
                     uint32_t& data_lo, uint32_t& data_hi) const
{
    data_lo = 0;
    data_hi = 0;

    if (off >= clint_cfg::WINDOW_SIZE) return false;

    // ---- MSIP[h] : 4 bytes only --------------------------------------------
    if (off >= clint_cfg::MSIP_BASE &&
        off <  clint_cfg::MSIP_BASE + uint64_t(cfg_.num_harts) * clint_cfg::MSIP_STRIDE) {
        if (access_size != 4) return false;
        unsigned h = static_cast<unsigned>(
            (off - clint_cfg::MSIP_BASE) / clint_cfg::MSIP_STRIDE);
        data_lo = msip_[h] & MSIP_BIT_MASK;
        return true;
    }

    // ---- MTIMECMP[h] : 4 or 8 bytes ----------------------------------------
    if (off >= clint_cfg::MTIMECMP_BASE &&
        off <  clint_cfg::MTIMECMP_BASE + uint64_t(cfg_.num_harts) * clint_cfg::MTIMECMP_STRIDE) {
        uint64_t local = off - clint_cfg::MTIMECMP_BASE;
        unsigned h     = static_cast<unsigned>(local / clint_cfg::MTIMECMP_STRIDE);
        unsigned half  = static_cast<unsigned>(local % clint_cfg::MTIMECMP_STRIDE);
        if (access_size == 8) {
            if (half != 0) return false;
            data_lo = static_cast<uint32_t>(mtimecmp_[h]);
            data_hi = static_cast<uint32_t>(mtimecmp_[h] >> 32);
        } else { // access_size == 4
            if (half != 0 && half != 4) return false;
            data_lo = static_cast<uint32_t>(mtimecmp_[h] >> (half == 4 ? 32 : 0));
        }
        return true;
    }

    // ---- MTIME : 4 or 8 bytes ----------------------------------------------
    if (off >= clint_cfg::MTIME_OFFSET && off < clint_cfg::MTIME_OFFSET + 8) {
        unsigned half = static_cast<unsigned>(off - clint_cfg::MTIME_OFFSET);
        if (access_size == 8) {
            if (half != 0) return false;
            data_lo = static_cast<uint32_t>(mtime_);
            data_hi = static_cast<uint32_t>(mtime_ >> 32);
        } else { // access_size == 4
            if (half != 0 && half != 4) return false;
            data_lo = static_cast<uint32_t>(mtime_ >> (half == 4 ? 32 : 0));
        }
        return true;
    }

    // ---- Hole inside the window: RAZ/WI ------------------------------------
    return true;
}

bool clint::reg_write(uint64_t off, unsigned access_size,
                      uint32_t data_lo, uint32_t data_hi)
{
    if (off >= clint_cfg::WINDOW_SIZE) return false;

    bool changed = false;

    // ---- MSIP[h] : 4 bytes only --------------------------------------------
    if (off >= clint_cfg::MSIP_BASE &&
        off <  clint_cfg::MSIP_BASE + uint64_t(cfg_.num_harts) * clint_cfg::MSIP_STRIDE) {
        if (access_size != 4) return false;
        unsigned h = static_cast<unsigned>(
            (off - clint_cfg::MSIP_BASE) / clint_cfg::MSIP_STRIDE);
        const uint8_t old = msip_[h];
        msip_[h] = static_cast<uint8_t>(data_lo & MSIP_BIT_MASK);
        if (old != msip_[h]) changed = true;
        if (changed) schedule_recompute();
        return true;
    }

    // ---- MTIMECMP[h] : 4 or 8 bytes ----------------------------------------
    if (off >= clint_cfg::MTIMECMP_BASE &&
        off <  clint_cfg::MTIMECMP_BASE + uint64_t(cfg_.num_harts) * clint_cfg::MTIMECMP_STRIDE) {
        uint64_t local = off - clint_cfg::MTIMECMP_BASE;
        unsigned h     = static_cast<unsigned>(local / clint_cfg::MTIMECMP_STRIDE);
        unsigned half  = static_cast<unsigned>(local % clint_cfg::MTIMECMP_STRIDE);
        const uint64_t old = mtimecmp_[h];
        if (access_size == 8) {
            if (half != 0) return false;
            mtimecmp_[h] = (uint64_t(data_hi) << 32) | uint64_t(data_lo);
        } else { // access_size == 4
            if (half == 0) {
                mtimecmp_[h] = (mtimecmp_[h] & 0xFFFF'FFFF'0000'0000ULL) | uint64_t(data_lo);
            } else if (half == 4) {
                mtimecmp_[h] = (mtimecmp_[h] & 0x0000'0000'FFFF'FFFFULL) |
                               (uint64_t(data_lo) << 32);
            } else {
                return false;
            }
        }
        if (old != mtimecmp_[h]) changed = true;
        if (changed) schedule_recompute();
        return true;
    }

    // ---- MTIME : 4 or 8 bytes (RW per RDL) ---------------------------------
    if (off >= clint_cfg::MTIME_OFFSET && off < clint_cfg::MTIME_OFFSET + 8) {
        unsigned half = static_cast<unsigned>(off - clint_cfg::MTIME_OFFSET);
        const uint64_t old = mtime_;
        if (access_size == 8) {
            if (half != 0) return false;
            mtime_ = (uint64_t(data_hi) << 32) | uint64_t(data_lo);
        } else { // access_size == 4
            if (half == 0) {
                mtime_ = (mtime_ & 0xFFFF'FFFF'0000'0000ULL) | uint64_t(data_lo);
            } else if (half == 4) {
                mtime_ = (mtime_ & 0x0000'0000'FFFF'FFFFULL) | (uint64_t(data_lo) << 32);
            } else {
                return false;
            }
        }
        if (old != mtime_) changed = true;
        if (changed) schedule_recompute();
        return true;
    }

    // ---- Hole inside the window: RAZ/WI ------------------------------------
    return true;
}

// ---------------------------------------------------------------------------
// b_transport — the only blocking TLM entry point
// ---------------------------------------------------------------------------
//
// 1. Validate length / alignment / streaming-width
// 2. Decode the access into reg_read or reg_write
// 3. Annotate `delay` with `access_delay_ns_p_` (per AXI4-Lite latency)

void clint::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    // ---- Width validation --------------------------------------------------
    if (length != 4 && length != 8) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if ((length == 4 && (addr & 0x3) != 0) ||
        (length == 8 && (addr & 0x7) != 0)) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_streaming_width() != length) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_byte_enable_ptr() != nullptr &&
        gp.get_byte_enable_length() != 0) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    // ---- Window check ------------------------------------------------------
    if (addr >= clint_cfg::WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    bool ok = true;
    if (gp.is_read()) {
        uint32_t lo = 0, hi = 0;
        ok = reg_read(addr, length, lo, hi);
        if (ok) {
            std::memcpy(buf, &lo, 4);
            if (length == 8) std::memcpy(buf + 4, &hi, 4);
        }
    } else if (gp.is_write()) {
        uint32_t lo = 0, hi = 0;
        std::memcpy(&lo, buf, 4);
        if (length == 8) std::memcpy(&hi, buf + 4, 4);
        ok = reg_write(addr, length, lo, hi);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    if (!ok) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // Re-cache access_delay each transaction so a CCI-driven change takes
    // effect on the *next* access (cheap; the conversion is two adds).
    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

// ---------------------------------------------------------------------------
// transport_dbg — back-door read/write, no annotated delay
// ---------------------------------------------------------------------------

unsigned int clint::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if ((length != 4 && length != 8) ||
        (length == 4 && (addr & 0x3) != 0) ||
        (length == 8 && (addr & 0x7) != 0) ||
        addr >= clint_cfg::WINDOW_SIZE) {
        return 0;
    }

    if (gp.is_read()) {
        uint32_t lo = 0, hi = 0;
        if (!reg_read(addr, length, lo, hi)) return 0;
        std::memcpy(buf, &lo, 4);
        if (length == 8) std::memcpy(buf + 4, &hi, 4);
    } else {
        uint32_t lo = 0, hi = 0;
        std::memcpy(&lo, buf, 4);
        if (length == 8) std::memcpy(&hi, buf + 4, 4);
        if (!reg_write(addr, length, lo, hi)) return 0;
    }
    return length;
}

// ---------------------------------------------------------------------------
// Debug back-door API
// ---------------------------------------------------------------------------

uint64_t clint::dbg_mtime() const { return mtime_; }

uint64_t clint::dbg_mtimecmp(unsigned hart) const
{
    return (hart < cfg_.num_harts) ? mtimecmp_[hart] : 0;
}

uint32_t clint::dbg_msip(unsigned hart) const
{
    return (hart < cfg_.num_harts) ? (msip_[hart] & MSIP_BIT_MASK) : 0;
}

bool clint::dbg_mtip(unsigned hart) const
{
    return (hart < cfg_.num_harts) ? (mtime_ >= mtimecmp_[hart]) : false;
}

void clint::dbg_set_mtime(uint64_t value)
{
    if (mtime_ == value) return;
    mtime_ = value;
    schedule_recompute();
}

void clint::dump_state(std::ostream& os) const
{
    os << "CLINT state @ " << sc_core::sc_time_stamp() << "\n"
       << "  mtime          = 0x" << std::hex << std::setw(16) << std::setfill('0')
       << mtime_ << std::dec << " (" << mtime_ << ")\n"
       << "  num_harts      = " << cfg_.num_harts << "\n"
       << "  tick_period_ns = " << cfg_.tick_period_ns << "\n";
    for (unsigned h = 0; h < cfg_.num_harts; ++h) {
        os << "  hart[" << h << "]: msip=" << unsigned(msip_[h] & MSIP_BIT_MASK)
           << "  mtimecmp=0x" << std::hex << std::setw(16) << std::setfill('0')
           << mtimecmp_[h] << std::dec
           << "  mtip=" << (mtime_ >= mtimecmp_[h] ? 1 : 0) << "\n";
    }
}

} // namespace smc
