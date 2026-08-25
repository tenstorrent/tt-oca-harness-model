// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file wdt.cpp
 * @brief SMC SiFive TLWDT — SystemC/TLM-2.0 LT implementation (stage 1).
 */

#include "wdt.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>
#include <string>

namespace smc {

namespace {

constexpr uint32_t COUNT_MASK = 0x7FFF'FFFFu;

}  // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

wdt::wdt(sc_core::sc_module_name name, wdt_cfg cfg)
    : sc_core::sc_module(name)
    , tick_period_ns_p_(
          "tick_period_ns",
          cfg.tick_period_ns,
          "WDT counter tick period in nanoseconds. "
          "Default 100 ns; set 0.0 to disable auto-tick (test-controlled).")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds. Mutable.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , core_rst_i("core_rst_i")
    , irq_o("irq_o")
    , rst_sticky_o("rst_sticky_o")
    , cfg_(cfg)
{
    cfg_.tick_period_ns  = tick_period_ns_p_.get_value();
    cfg_.access_delay_ns = access_delay_ns_p_.get_value();

    tick_period_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase",
                                    cci::cci_value(std::string("annotated_delay")));

    if (cfg_.tick_period_ns < 0.0) {
        SC_REPORT_FATAL(name, "WDT tick_period_ns must be >= 0");
    }

    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    tick_period_  = sc_core::sc_time(cfg_.tick_period_ns, sc_core::SC_NS);

    reg_socket.register_b_transport(this, &wdt::b_transport);
    reg_socket.register_transport_dbg(this, &wdt::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_METHOD(tick_method);
    sensitive << tick_event_;
    dont_initialize();

    SC_METHOD(output_method);
    sensitive << recompute_event_;
    dont_initialize();

    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.notify(tick_period_);
    }

    SIM_LOG_INFO(this,
                 "WDT instantiated tick_period_ns=" << cfg_.tick_period_ns
                 << " access_delay_ns=" << access_delay_ns_p_.get_value());
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

uint16_t wdt::dbg_scaled() const
{
    return static_cast<uint16_t>((count_ >> scale_) & 0xFFFFu);
}

bool wdt::dbg_elapsed() const
{
    return dbg_scaled() >= cmp_;
}

bool wdt::counting_enabled() const
{
    if (always_) {
        return true;
    }
    if (awake_ && !core_rst_i.read()) {
        return true;
    }
    return false;
}

uint32_t wdt::ctrl_read_value() const
{
    uint32_t v = 0;
    v |= (static_cast<uint32_t>(scale_) & wdt_cfg::CTRL_SCALE_MASK);
    if (rsten_) {
        v |= wdt_cfg::CTRL_RSTEN_BIT;
    }
    if (zerocmp_) {
        v |= wdt_cfg::CTRL_ZEROCMP_BIT;
    }
    if (always_) {
        v |= wdt_cfg::CTRL_ALWAYS_BIT;
    }
    if (awake_) {
        v |= wdt_cfg::CTRL_AWAKE_BIT;
    }
    if (ip_) {
        v |= wdt_cfg::CTRL_IP_BIT;
    }
    return v;
}

void wdt::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void wdt::apply_elapsed_side_effects(bool fed_this_cycle)
{
    if (!dbg_elapsed()) {
        return;
    }
    ip_ = true;
    if (rsten_ && !fed_this_cycle) {
        rst_sticky_ = true;
    }
    if (zerocmp_) {
        count_ = 0;
    }
}

// ---------------------------------------------------------------------------
// SC_METHODs
// ---------------------------------------------------------------------------

void wdt::reset_proc()
{
    if (rst_n_i.read()) {
        return;
    }

    count_      = 0;
    cmp_        = wdt_cfg::CMP_RESET;
    scale_      = 0;
    rsten_      = false;
    zerocmp_    = false;
    always_     = false;
    awake_      = false;
    ip_         = false;
    unlocked_   = false;
    rst_sticky_ = false;

    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.cancel();
        tick_event_.notify(tick_period_);
    }
    schedule_recompute();
}

void wdt::tick_method()
{
    if (!rst_n_i.read()) {
        return;
    }
    if (counting_enabled()) {
        count_ = (count_ + 1u) & COUNT_MASK;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
    }
    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.notify(tick_period_);
    }
}

void wdt::output_method()
{
    const bool irq_lvl = ip_;
    if (irq_lvl != irq_cache_) {
        irq_o.write(irq_lvl);
        irq_cache_ = irq_lvl;
    }
    if (rst_sticky_ != rst_sticky_cache_) {
        rst_sticky_o.write(rst_sticky_);
        rst_sticky_cache_ = rst_sticky_;
    }
}

void wdt::dbg_tick(unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        if (counting_enabled()) {
            count_ = (count_ + 1u) & COUNT_MASK;
            apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        }
    }
    schedule_recompute();
}

void wdt::dbg_set_count(uint32_t v)
{
    count_ = v & COUNT_MASK;
    apply_elapsed_side_effects(/*fed_this_cycle=*/false);
    schedule_recompute();
}

void wdt::dump_state(std::ostream& os) const
{
    os << "WDT count=0x" << std::hex << count_
       << " scaled=0x" << dbg_scaled()
       << " cmp=0x" << cmp_
       << " scale=" << std::dec << unsigned(scale_)
       << " unlocked=" << unlocked_
       << " ip=" << ip_
       << " sticky=" << rst_sticky_
       << " always=" << always_
       << " awake=" << awake_
       << " rsten=" << rsten_
       << " zerocmp=" << zerocmp_ << "\n";
}

// ---------------------------------------------------------------------------
// Register decode
// ---------------------------------------------------------------------------

void wdt::do_unlock_write_key(uint32_t data)
{
    unlocked_ = (data == wdt_cfg::KEY_MAGIC);
}

void wdt::do_feed(uint32_t data)
{
    if (data != wdt_cfg::FEED_MAGIC) {
        unlocked_ = false;
        return;
    }
    const bool was_elapsed = dbg_elapsed();
    count_ = 0;
    // Plan: successful feed clears sticky latch.
    rst_sticky_ = false;
    // Concurrent feed while elapsed also prevents sticky set this cycle.
    if (was_elapsed) {
        ip_ = true;  // elapsed still sets IP; feed does not clear IP by itself
        if (zerocmp_) {
            count_ = 0;
        }
    }
    unlocked_ = false;
}

bool wdt::reg_read(uint64_t off, unsigned access_size, uint32_t& data) const
{
    data = 0;
    if (off >= wdt_cfg::WINDOW_SIZE) {
        return false;
    }
    if (access_size != 4 && access_size != 8) {
        return false;
    }

    // 64-bit access at naturally aligned bases that span two 32-bit regs.
    if (access_size == 8) {
        uint32_t lo = 0;
        uint32_t hi = 0;
        if (!reg_read(off, 4, lo)) {
            return false;
        }
        if (off + 4 < wdt_cfg::WINDOW_SIZE) {
            (void)reg_read(off + 4, 4, hi);
        }
        // Caller of 64-bit path uses data as low word; b_transport packs both.
        data = lo;
        (void)hi;
        return true;
    }

    switch (off) {
    case wdt_cfg::OFF_CTRL:
        data = ctrl_read_value();
        return true;
    case wdt_cfg::OFF_COUNT:
        data = count_ & COUNT_MASK;
        return true;
    case wdt_cfg::OFF_SCALED_COUNT:
        data = dbg_scaled();
        return true;
    case wdt_cfg::OFF_FEED:
        data = 0;
        return true;
    case wdt_cfg::OFF_KEY:
        data = unlocked_ ? 1u : 0u;
        return true;
    case wdt_cfg::OFF_CMP:
        data = cmp_;
        return true;
    default:
        // Hole inside window: RAZ
        return true;
    }
}

bool wdt::reg_write(uint64_t off, unsigned access_size, uint32_t data)
{
    if (off >= wdt_cfg::WINDOW_SIZE) {
        return false;
    }
    if (access_size != 4) {
        return false;  // 64-bit writes handled in b_transport as two halves
    }

    switch (off) {
    case wdt_cfg::OFF_KEY:
        do_unlock_write_key(data);
        schedule_recompute();
        return true;

    case wdt_cfg::OFF_FEED:
        if (!unlocked_) {
            return true;  // locked: ignore write, no unlock change for FEED path
        }
        do_feed(data);
        schedule_recompute();
        return true;

    case wdt_cfg::OFF_SCALED_COUNT:
        // Write has no effect on value but locks (SiFive semantics).
        unlocked_ = false;
        return true;

    case wdt_cfg::OFF_CTRL:
        if (!unlocked_) {
            return true;
        }
        scale_   = static_cast<uint8_t>(data & wdt_cfg::CTRL_SCALE_MASK);
        rsten_   = (data & wdt_cfg::CTRL_RSTEN_BIT) != 0;
        zerocmp_ = (data & wdt_cfg::CTRL_ZEROCMP_BIT) != 0;
        always_  = (data & wdt_cfg::CTRL_ALWAYS_BIT) != 0;
        awake_   = (data & wdt_cfg::CTRL_AWAKE_BIT) != 0;
        // IP: software write ORs with elapsed; writing 0 clears only if !elapsed.
        if ((data & wdt_cfg::CTRL_IP_BIT) != 0 || dbg_elapsed()) {
            ip_ = true;
        } else {
            ip_ = false;
        }
        unlocked_ = false;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
        return true;

    case wdt_cfg::OFF_COUNT:
        if (!unlocked_) {
            return true;
        }
        count_ = regmodel::apply_write_mask(count_, data, COUNT_MASK) & COUNT_MASK;
        unlocked_ = false;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
        return true;

    case wdt_cfg::OFF_CMP:
        if (!unlocked_) {
            return true;
        }
        cmp_ = static_cast<uint16_t>(
            regmodel::apply_write_mask(static_cast<uint32_t>(cmp_), data, 0xFFFFu) &
            0xFFFFu);
        unlocked_ = false;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
        return true;

    default:
        // Hole: WI, but still lock if unlocked? SiFive only locks on known regs.
        return true;
    }
}

// ---------------------------------------------------------------------------
// TLM
// ---------------------------------------------------------------------------

void wdt::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const uint64_t addr = gp.get_address();
    const unsigned len  = gp.get_data_length();
    uint8_t* const ptr  = gp.get_data_ptr();

    if (ptr == nullptr || gp.get_byte_enable_ptr() != nullptr ||
        gp.get_streaming_width() < len) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (len != 4 && len != 8) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if ((addr % len) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (addr >= wdt_cfg::WINDOW_SIZE || (addr + len) > wdt_cfg::WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;

    if (gp.is_read()) {
        if (len == 4) {
            uint32_t data = 0;
            if (!reg_read(addr, 4, data)) {
                gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            std::memcpy(ptr, &data, 4);
        } else {
            uint32_t lo = 0;
            uint32_t hi = 0;
            (void)reg_read(addr, 4, lo);
            (void)reg_read(addr + 4, 4, hi);
            std::memcpy(ptr, &lo, 4);
            std::memcpy(ptr + 4, &hi, 4);
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (gp.is_write()) {
        if (len == 4) {
            uint32_t data = 0;
            std::memcpy(&data, ptr, 4);
            if (!reg_write(addr, 4, data)) {
                gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
        } else {
            // 64-bit: write low then high (KEY often in high half at 0x18).
            uint32_t lo = 0;
            uint32_t hi = 0;
            std::memcpy(&lo, ptr, 4);
            std::memcpy(&hi, ptr + 4, 4);
            // Special case: FEED+KEY at 0x18 — KEY unlock must happen before FEED
            // if both are written in one beat.  SiFive TileLink presents both in
            // one cycle; KEY unlock and FEED in same cycle: unlocked for feed.
            if (addr == wdt_cfg::OFF_FEED) {
                do_unlock_write_key(hi);
                if (unlocked_) {
                    do_feed(lo);
                }
                schedule_recompute();
            } else {
                (void)reg_write(addr, 4, lo);
                (void)reg_write(addr + 4, 4, hi);
            }
        }
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
}

unsigned int wdt::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const uint64_t addr = gp.get_address();
    const unsigned len  = gp.get_data_length();
    uint8_t* const ptr  = gp.get_data_ptr();
    if (ptr == nullptr || (len != 4 && len != 8) || addr >= wdt_cfg::WINDOW_SIZE) {
        return 0;
    }
    if (gp.is_read()) {
        if (len == 4) {
            uint32_t data = 0;
            if (!reg_read(addr, 4, data)) {
                return 0;
            }
            std::memcpy(ptr, &data, 4);
            return 4;
        }
        uint32_t lo = 0;
        uint32_t hi = 0;
        (void)reg_read(addr, 4, lo);
        (void)reg_read(addr + 4, 4, hi);
        std::memcpy(ptr, &lo, 4);
        std::memcpy(ptr + 4, &hi, 4);
        return 8;
    }
    if (gp.is_write()) {
        // Debug writes apply the same side effects as b_transport for testability.
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;
        b_transport(gp, t);
        return (gp.get_response_status() == tlm::TLM_OK_RESPONSE) ? len : 0;
    }
    return 0;
}

}  // namespace smc
