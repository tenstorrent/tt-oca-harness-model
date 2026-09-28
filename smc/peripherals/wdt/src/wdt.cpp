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

    // Keep this guard ahead of the socket and SC_METHOD registrations below.
    // test/wdt_tb.cpp builds a module with a bad config under SC_THROW and
    // catches the report; a process registered before the throw would outlive
    // the unwound object and fire at sc_start().
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

void wdt::reg_read(uint64_t off, uint32_t& data) const
{
    switch (off) {
    case wdt_cfg::OFF_CTRL:
        data = ctrl_read_value();
        return;
    case wdt_cfg::OFF_COUNT:
        data = count_ & COUNT_MASK;
        return;
    case wdt_cfg::OFF_SCALED_COUNT:
        data = dbg_scaled();
        return;
    case wdt_cfg::OFF_FEED:
        data = 0;
        return;
    case wdt_cfg::OFF_KEY:
        data = unlocked_ ? 1u : 0u;
        return;
    case wdt_cfg::OFF_CMP:
        data = cmp_;
        return;
    default:
        // Hole inside window: RAZ
        data = 0;
        return;
    }
}

void wdt::reg_write(uint64_t off, uint32_t data)
{
    switch (off) {
    case wdt_cfg::OFF_KEY:
        do_unlock_write_key(data);
        schedule_recompute();
        return;

    case wdt_cfg::OFF_FEED:
        if (!unlocked_) {
            return;  // locked: ignore write, no unlock change for FEED path
        }
        do_feed(data);
        schedule_recompute();
        return;

    case wdt_cfg::OFF_SCALED_COUNT:
        // Write has no effect on value but locks (SiFive semantics).
        unlocked_ = false;
        return;

    case wdt_cfg::OFF_CTRL:
        if (!unlocked_) {
            return;
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
        return;

    case wdt_cfg::OFF_COUNT:
        if (!unlocked_) {
            return;
        }
        count_ = regmodel::apply_write_mask(count_, data, COUNT_MASK) & COUNT_MASK;
        unlocked_ = false;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
        return;

    case wdt_cfg::OFF_CMP:
        if (!unlocked_) {
            return;
        }
        cmp_ = static_cast<uint16_t>(
            regmodel::apply_write_mask(static_cast<uint32_t>(cmp_), data, 0xFFFFu) &
            0xFFFFu);
        unlocked_ = false;
        apply_elapsed_side_effects(/*fed_this_cycle=*/false);
        schedule_recompute();
        return;

    default:
        // Hole: WI, but still lock if unlocked? SiFive only locks on known regs.
        return;
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
            reg_read(addr, data);
            std::memcpy(ptr, &data, 4);
        } else {
            uint32_t lo = 0;
            uint32_t hi = 0;
            reg_read(addr, lo);
            reg_read(addr + 4, hi);
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
            reg_write(addr, data);
        } else {
            // 64-bit: write low then high (KEY often in high half at 0x18).
            uint32_t lo = 0;
            uint32_t hi = 0;
            std::memcpy(&lo, ptr, 4);
            std::memcpy(&hi, ptr + 4, 4);
            // FEED and KEY share one beat. Authorization uses the pre-write
            // unlock state; the KEY half cannot unlock this same beat.
            if (addr == wdt_cfg::OFF_FEED) {
                const bool was_unlocked = unlocked_;
                if (was_unlocked && lo == wdt_cfg::FEED_MAGIC)
                    do_feed(lo);
                // The KEY half is present in this beat, so it cannot unlock it.
                unlocked_ = false;
                (void)hi;
                schedule_recompute();
            } else {
                reg_write(addr, lo);
                reg_write(addr + 4, hi);
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
    if (ptr == nullptr || (len != 4 && len != 8)) {
        return 0;
    }
    // Same window and alignment contract as b_transport: a debug access must
    // not read past the last register just because only the base was checked,
    // and a misaligned beat must not be serviced.
    if ((addr % len) != 0) {
        return 0;
    }
    if (addr >= wdt_cfg::WINDOW_SIZE || (addr + len) > wdt_cfg::WINDOW_SIZE) {
        return 0;
    }
    if (gp.is_read()) {
        if (len == 4) {
            uint32_t data = 0;
            reg_read(addr, data);
            std::memcpy(ptr, &data, 4);
            return 4;
        }
        uint32_t lo = 0;
        uint32_t hi = 0;
        reg_read(addr, lo);
        reg_read(addr + 4, hi);
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
