// SPDX-License-Identifier: Apache-2.0
/**
 * @file reset_unit.cpp
 * @brief SMC Reset Unit — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/reset_unit.h` for the full design description, register map,
 * reset-derivation equations, FLR flow, and CCI catalogue.
 */

#include "reset_unit.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace smc {

namespace {

/// Pick bit @p i out of a 32-bit mask.
constexpr bool bit(uint32_t v, unsigned i) { return ((v >> i) & 1u) != 0; }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

reset_unit::reset_unit(sc_core::sc_module_name name, reset_unit_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params first (declared before the public ports in the header).
    , num_subsystems_p_(
          "num_subsystems",
          cfg.num_subsystems,
          "Number of per-subsystem reset-control bundles driven on "
          "ss_reset_ctrl_o (1..32). The reset_unit.rdl register fields are "
          "32-bit, so 32 is the architectural maximum.")
    , ref_clk_period_ns_p_(
          "ref_clk_period_ns",
          cfg.ref_clk_period_ns,
          "Reference-clock period in nanoseconds. Used to translate the FLR "
          "counter register values (cycle counts) into simulated time for the "
          "cool-reset pulse. Default 10 ns => 100 MHz.")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM b_transport annotated delay in nanoseconds. Approximates "
          "AXI4-Lite register-access latency. Mutable at run time.")
    // Ports.  ss_reset_ctrl_o sized from the (possibly preset) num_subsystems.
    , reg_socket("reg_socket")
    , powergood_i("powergood_i")
    , rst_cold_ni("rst_cold_ni")
    , fuse_reset_ni("fuse_reset_ni")
    , rst_ext_wdt_ni("rst_ext_wdt_ni")
    , smc_wdt_first_timeout_i("smc_wdt_first_timeout_i")
    , smc_wdt_second_timeout_i("smc_wdt_second_timeout_i")
    , rst_cool_ni("rst_cool_ni")
    , isolate_req_pin_i("isolate_req_pin_i")
    , cfg_flr_pf_active_i("cfg_flr_pf_active_i")
    , ss_reset_complete_i("ss_reset_complete_i")
    , captured_straps_i("captured_straps_i")
    , powergood_stable_o("powergood_stable_o")
    , rst_cold_stable_ref_clk_no("rst_cold_stable_ref_clk_no")
    , rst_cold_stable_smc_clk_no("rst_cold_stable_smc_clk_no")
    , rst_primary_ref_clk_no("rst_primary_ref_clk_no")
    , rst_primary_smc_clk_no("rst_primary_smc_clk_no")
    , rst_primary_periph_clk_no("rst_primary_periph_clk_no")
    , rst_core_smc_clk_no("rst_core_smc_clk_no")
    , rst_wdt_smc_clk_no("rst_wdt_smc_clk_no")
    , rst_cool_no("rst_cool_no")
    , skip_mem_repair_o("skip_mem_repair_o")
    , sync_irq_o("sync_irq_o")
    , isolate_req_o("isolate_req_o")
    , ss_config_o("ss_config_o")
    , ss_reset_ctrl_o("ss_reset_ctrl_o", num_subsystems_p_.get_value())
    , cfg_(cfg)
{
    // Sync cfg_ with CCI-resolved values.
    cfg_.num_subsystems   = num_subsystems_p_.get_value();
    cfg_.ref_clk_period_ns = ref_clk_period_ns_p_.get_value();
    cfg_.access_delay_ns  = access_delay_ns_p_.get_value();

    // Provenance metadata.
    num_subsystems_p_.add_metadata("rdl_dimension",
                                   cci::cci_value(std::string("ss_reset_ctrl_o[31:0]")));
    num_subsystems_p_.add_metadata("valid_range", cci::cci_value(std::string("1..32")));
    ref_clk_period_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    ref_clk_period_ns_p_.add_metadata("rtl_signal", cci::cci_value(std::string("clk_ref_i")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    if (cfg_.num_subsystems == 0 ||
        cfg_.num_subsystems > reset_unit_cfg::MAX_SUBSYSTEMS) {
        SC_REPORT_FATAL(name, "reset_unit num_subsystems must be in 1..32");
    }
    if (cfg_.ref_clk_period_ns < 0.0) {
        SC_REPORT_FATAL(name, "reset_unit ref_clk_period_ns must be >= 0");
    }
    if (cfg_.access_delay_ns < 0.0) {
        SC_REPORT_FATAL(name, "reset_unit access_delay_ns must be >= 0");
    }

    ss_ctrl_cache_.assign(cfg_.num_subsystems, reset_ctrl_t{});

    access_delay_   = sc_core::sc_time(cfg_.access_delay_ns,  sc_core::SC_NS);
    ref_clk_period_ = sc_core::sc_time(cfg_.ref_clk_period_ns, sc_core::SC_NS);

    // TLM target socket callbacks.
    reg_socket.register_b_transport  (this, &reset_unit::b_transport);
    reg_socket.register_transport_dbg(this, &reset_unit::transport_dbg);

    // SC_METHOD: react to any input-port change.
    SC_METHOD(input_method);
    sensitive << powergood_i << rst_cold_ni << fuse_reset_ni << rst_ext_wdt_ni
              << smc_wdt_first_timeout_i << smc_wdt_second_timeout_i
              << rst_cool_ni << isolate_req_pin_i << cfg_flr_pf_active_i
              << ss_reset_complete_i << captured_straps_i;
    dont_initialize();

    // SC_METHOD: FLR cool-reset pulse edges.
    SC_METHOD(flr_assert_method);
    sensitive << flr_assert_event_;
    dont_initialize();

    SC_METHOD(flr_deassert_method);
    sensitive << flr_deassert_event_;
    dont_initialize();

    // SC_METHOD: sole driver of all outputs.
    SC_METHOD(output_method);
    sensitive << recompute_event_;
    dont_initialize();

    SC_REPORT_INFO(name,
        ("reset_unit instantiated: num_subsystems=" +
         std::to_string(cfg_.num_subsystems) +
         ", ref_clk_period_ns=" + std::to_string(cfg_.ref_clk_period_ns) +
         ", access_delay_ns="  + std::to_string(cfg_.access_delay_ns)).c_str());
}

// ---------------------------------------------------------------------------
// start_of_simulation — seed reset-edge tracking and drive initial outputs
// ---------------------------------------------------------------------------

void reset_unit::start_of_simulation()
{
    ss_reset_complete_ = ss_reset_complete_i.read();
    prev_flr_active_   = cfg_flr_pf_active_i.read();

    const derived_t d = derive();
    prev_primary_n_ = d.rst_primary_n;
    prev_cold_n_    = d.stable_cold_rst_n;

    schedule_recompute();
}

// ---------------------------------------------------------------------------
// derive — combinational reset derivation (post-override, post-abstraction)
// ---------------------------------------------------------------------------

reset_unit::derived_t reset_unit::derive() const
{
    derived_t d;

    const bool cold_pin_n  = rst_cold_ni.read();
    const bool fuse_pin_n  = fuse_reset_ni.read();
    const bool cool_pin_n  = rst_cool_ni.read();             // cool-from-pin
    const bool ext_wdt_n   = rst_ext_wdt_ni.read();
    const bool wdt2nd      = smc_wdt_second_timeout_i.read();
    const bool powergood   = powergood_i.read();

    // JTAG overrides.
    const bool cold_n = jtag_.cold_reset_n_ovrd ? jtag_.cold_reset_n_val : cold_pin_n;
    const bool fuse_n = jtag_.fuse_reset_n_ovrd ? jtag_.fuse_reset_n_val : fuse_pin_n;
    const bool cool_from_flr_n =
        jtag_.cool_reset_n_ovrd ? jtag_.cool_reset_n_val : flr_cool_n_;

    // Powergood stretch is abstracted to zero latency.
    d.powergood_stable = powergood;

    // Cold reset de-glitch (320 ns) + 255-cycle extender abstracted away.
    d.stable_cold_rst_n = d.powergood_stable && cold_n;

    // Cool reset de-glitch abstracted away.
    const bool stable_cool_rst_n = cool_pin_n;

    const bool wdt_reset_n = ext_wdt_n && !wdt2nd;

    d.rst_primary_n = d.stable_cold_rst_n && stable_cool_rst_n && cool_from_flr_n;

    const bool rst_core_n = wdt_reset_n && d.rst_primary_n && fuse_n;
    d.rst_core_int_n = jtag_.core_reset_n_ovrd ? jtag_.core_reset_n_val : rst_core_n;

    d.rst_wdt_n = wdt_reset_n;
    return d;
}

// ---------------------------------------------------------------------------
// process_state_change — recompute + reset-assert-edge register clears
// ---------------------------------------------------------------------------

void reset_unit::process_state_change()
{
    const derived_t d = derive();

    // Falling edge (assertion) of the primary-domain reset clears the
    // register block + subsystem registers.
    if (prev_primary_n_ && !d.rst_primary_n) {
        clear_primary_regs();
    }
    // Falling edge (assertion) of the cold-domain reset clears the FLR /
    // isolate registers (qualified PINEN reset honoured inside).
    if (prev_cold_n_ && !d.stable_cold_rst_n) {
        clear_cold_regs(isolate_req_pin_i.read());
    }

    prev_primary_n_ = d.rst_primary_n;
    prev_cold_n_    = d.stable_cold_rst_n;

    schedule_recompute();
}

void reset_unit::clear_primary_regs()
{
    ss_config_            = 0;
    ss_config_lock_       = 0;
    ss_cold_reset_n_      = 0;
    ss_warm_reset_n_      = 0xFFFFFFFFu; // RDL reset value
    ss_config_hold_       = 0;
    ss_sram_hold_         = 0;
    ss_critical_hold_     = 0;
    ss_debug_hold_        = 0;
    ss_cold_reset_lock_   = 0;
    ss_force_to_ref_clk_n_ = 0;
    sync_reg_             = 0;
}

void reset_unit::clear_cold_regs(bool isolate_pin)
{
    isolate_req_reg_       = 0;
    isolate_req_smc_reg_   = 0;
    isolate_req_smcen_reg_ = 0;
    flr_counter_value_       = 0;
    flr_reset_counter_value_ = 0;
    // ISOLATE_REQ_PINEN_REG resets only when the isolate pin is low
    // (smc_cool_reset_wrap.sv: reset gated by ~isolate_req_pin_sync_smc).
    if (!isolate_pin) {
        isolate_req_pinen_reg_ = 0;
    }
}

// ---------------------------------------------------------------------------
// FLR cool-reset pulse
// ---------------------------------------------------------------------------

void reset_unit::flr_kick()
{
    // RTL: the cool-reset flow requires the reset-duration counter to be > 0;
    // a value of 0 prevents initiation regardless of the delay counter.
    if (flr_reset_counter_value_ == 0) {
        return;
    }
    const sc_core::sc_time assert_at  = ref_clk_period_ * double(flr_counter_value_);
    const sc_core::sc_time release_at =
        ref_clk_period_ * double(uint64_t(flr_counter_value_) + flr_reset_counter_value_);

    flr_assert_event_.notify(assert_at);
    flr_deassert_event_.notify(release_at);
}

void reset_unit::flr_assert_method()
{
    flr_cool_n_ = false; // assert cool reset (active-low)
    process_state_change();
}

void reset_unit::flr_deassert_method()
{
    flr_cool_n_ = true;  // release cool reset
    process_state_change();
}

// ---------------------------------------------------------------------------
// input_method — react to input-port changes
// ---------------------------------------------------------------------------

void reset_unit::input_method()
{
    // Track the synced subsystem reset-complete status.
    ss_reset_complete_ = ss_reset_complete_i.read();

    // Detect the rising edge of cfg_flr_pf_active: set the HW isolate-request
    // latch and start the cool-reset pulse sequence.
    const bool flr_active = cfg_flr_pf_active_i.read();
    if (flr_active && !prev_flr_active_) {
        isolate_req_smc_reg_ = 1; // bit0 set by HW; software clears it
        flr_kick();
    }
    prev_flr_active_ = flr_active;

    process_state_change();
}

// ---------------------------------------------------------------------------
// output_method — sole driver of all outputs (idempotent)
// ---------------------------------------------------------------------------

void reset_unit::output_method()
{
    const derived_t d = derive();

    powergood_stable_o.write(d.powergood_stable);

    // Cold-domain resets (zero-delay synchronisers).
    rst_cold_stable_ref_clk_no.write(d.stable_cold_rst_n);
    rst_cold_stable_smc_clk_no.write(d.stable_cold_rst_n);

    // Primary-domain resets.
    rst_primary_ref_clk_no.write(d.rst_primary_n);
    rst_primary_smc_clk_no.write(d.rst_primary_n);
    rst_primary_periph_clk_no.write(d.rst_primary_n);

    // Core / WDT resets.
    rst_core_smc_clk_no.write(d.rst_core_int_n);
    rst_wdt_smc_clk_no.write(d.rst_wdt_n);

    // Outgoing cool reset = post-JTAG-override FLR cool reset.
    const bool cool_no = jtag_.cool_reset_n_ovrd ? jtag_.cool_reset_n_val : flr_cool_n_;
    rst_cool_no.write(cool_no);

    // Isolate request / skip-mem-repair.
    const bool isolate_pin = isolate_req_pin_i.read();
    const bool smc_latch   = (isolate_req_smc_reg_ & 1u) != 0;
    uint32_t isolate = 0;
    for (unsigned i = 0; i < 32; ++i) {
        const bool v = bit(isolate_req_reg_, i) ||
                       (bit(isolate_req_pinen_reg_, i) && isolate_pin) ||
                       (bit(isolate_req_smcen_reg_, i) && smc_latch);
        if (v) isolate |= (1u << i);
    }
    isolate_req_o.write(isolate);
    skip_mem_repair_o.write(isolate_pin || smc_latch);

    // Sync IRQ.
    sync_irq_o.write((sync_reg_ & 1u) != 0);

    // Subsystem config.
    ss_config_o.write(ss_config_);

    // Per-subsystem reset-control bundles (idempotent writes).
    for (unsigned i = 0; i < cfg_.num_subsystems; ++i) {
        reset_ctrl_t r;
        r.cold_reset_n = bit(jtag_.ss_cold_reset_n_ovrd, i)
                             ? bit(jtag_.ss_cold_reset_n_val, i)
                             : bit(ss_cold_reset_n_, i);
        r.warm_reset_n = bit(jtag_.ss_warm_reset_n_ovrd, i)
                             ? bit(jtag_.ss_warm_reset_n_val, i)
                             : bit(ss_warm_reset_n_, i);
        r.config_state_hold    = bit(ss_config_hold_, i);
        r.critical_signal_hold = bit(ss_critical_hold_, i);
        r.sram_hold            = bit(ss_sram_hold_, i);
        r.debug_hold           = bit(ss_debug_hold_, i);
        r.force_to_ref_clk_n   = bit(ss_force_to_ref_clk_n_, i);

        if (r != ss_ctrl_cache_[i]) {
            ss_reset_ctrl_o[i].write(r);
            ss_ctrl_cache_[i] = r;
        }
    }
}

void reset_unit::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

// ---------------------------------------------------------------------------
// reg_read / reg_write — 32-bit register decode
// ---------------------------------------------------------------------------

bool reset_unit::reg_read(uint64_t off, uint32_t& data) const
{
    data = 0;
    if (off >= reset_unit_cfg::WINDOW_SIZE) return false;

    switch (off) {
    case reset_unit_cfg::SS_CONFIG:            data = ss_config_;            break;
    case reset_unit_cfg::SS_CONFIG_LOCK:       data = ss_config_lock_;       break;
    case reset_unit_cfg::SS_COLD_RESET_N:      data = ss_cold_reset_n_;      break;
    case reset_unit_cfg::SS_WARM_RESET_N:      data = ss_warm_reset_n_;      break;
    case reset_unit_cfg::SS_CONFIG_HOLD:       data = ss_config_hold_;       break;
    case reset_unit_cfg::SS_SRAM_HOLD:         data = ss_sram_hold_;         break;
    case reset_unit_cfg::SS_CRITICAL_HOLD:     data = ss_critical_hold_;     break;
    case reset_unit_cfg::SS_DEBUG_HOLD:        data = ss_debug_hold_;        break;
    case reset_unit_cfg::SS_RESET_COMPLETE:    data = ss_reset_complete_;    break;
    case reset_unit_cfg::SS_COLD_RESET_LOCK:   data = ss_cold_reset_lock_;   break;
    case reset_unit_cfg::SS_FORCE_TO_REF_CLK:  data = ss_force_to_ref_clk_n_; break;
    case reset_unit_cfg::STRAPS_LO:
        data = static_cast<uint32_t>(captured_straps_i.read());
        break;
    case reset_unit_cfg::STRAPS_HI:
        data = static_cast<uint32_t>(captured_straps_i.read() >> 32);
        break;
    case reset_unit_cfg::SYNC_REG:             data = sync_reg_ & 1u;        break;
    case reset_unit_cfg::ISOLATE_REQ_REG:      data = isolate_req_reg_;      break;
    case reset_unit_cfg::ISOLATE_REQ_PINEN_REG: data = isolate_req_pinen_reg_; break;
    case reset_unit_cfg::ISOLATE_REQ_SMC_REG:  data = isolate_req_smc_reg_ & 1u; break;
    case reset_unit_cfg::ISOLATE_REQ_SMCEN_REG: data = isolate_req_smcen_reg_; break;
    case reset_unit_cfg::ISOLATE_REQ_VIS: {
        // bit0 = isolate_req_pin, bit4 = cool_reset_n_in, bit8 = cool_reset_n_out
        const bool cool_no = jtag_.cool_reset_n_ovrd ? jtag_.cool_reset_n_val
                                                     : flr_cool_n_;
        data = (isolate_req_pin_i.read() ? (1u << 0) : 0u) |
               (rst_cool_ni.read()       ? (1u << 4) : 0u) |
               (cool_no                  ? (1u << 8) : 0u);
        break;
    }
    case reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE:
        data = flr_counter_value_; break;
    case reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE:
        data = flr_reset_counter_value_; break;
    default:
        // Hole inside the window: RAZ.
        break;
    }
    return true;
}

bool reset_unit::reg_write(uint64_t off, uint32_t data)
{
    if (off >= reset_unit_cfg::WINDOW_SIZE) return false;

    switch (off) {
    case reset_unit_cfg::SS_CONFIG: {
        // Locked bits keep their value (config_filtered_wr_mask = ~lock).
        const uint32_t wmask = ~ss_config_lock_;
        ss_config_ = (data & wmask) | (ss_config_ & ~wmask);
        break;
    }
    case reset_unit_cfg::SS_CONFIG_LOCK:
        ss_config_lock_ |= data; // woset: write-1-to-set, sticky
        break;
    case reset_unit_cfg::SS_COLD_RESET_N: {
        const uint32_t wmask = ~ss_cold_reset_lock_;
        ss_cold_reset_n_ = (data & wmask) | (ss_cold_reset_n_ & ~wmask);
        break;
    }
    case reset_unit_cfg::SS_WARM_RESET_N:    ss_warm_reset_n_     = data; break;
    case reset_unit_cfg::SS_CONFIG_HOLD:     ss_config_hold_      = data; break;
    case reset_unit_cfg::SS_SRAM_HOLD:       ss_sram_hold_        = data; break;
    case reset_unit_cfg::SS_CRITICAL_HOLD:   ss_critical_hold_    = data; break;
    case reset_unit_cfg::SS_DEBUG_HOLD:      ss_debug_hold_       = data; break;
    case reset_unit_cfg::SS_RESET_COMPLETE:  /* SW read-only */         break;
    case reset_unit_cfg::SS_COLD_RESET_LOCK:
        ss_cold_reset_lock_ |= data; // woset
        break;
    case reset_unit_cfg::SS_FORCE_TO_REF_CLK: ss_force_to_ref_clk_n_ = data; break;
    case reset_unit_cfg::STRAPS_LO:          /* SW read-only */         break;
    case reset_unit_cfg::STRAPS_HI:          /* SW read-only */         break;
    case reset_unit_cfg::SYNC_REG:           sync_reg_ = data & 1u;      break;
    case reset_unit_cfg::ISOLATE_REQ_REG:    isolate_req_reg_       = data; break;
    case reset_unit_cfg::ISOLATE_REQ_PINEN_REG: isolate_req_pinen_reg_ = data; break;
    case reset_unit_cfg::ISOLATE_REQ_SMC_REG:
        // HW sets bit0 on FLR; a software write (any bit enabled) clears it.
        isolate_req_smc_reg_ = 0;
        break;
    case reset_unit_cfg::ISOLATE_REQ_SMCEN_REG: isolate_req_smcen_reg_ = data; break;
    case reset_unit_cfg::ISOLATE_REQ_VIS:    /* SW read-only */         break;
    case reset_unit_cfg::ISOLATE_REQ_FLR_COUNTER_VALUE:
        flr_counter_value_ = data; break;
    case reset_unit_cfg::ISOLATE_REQ_FLR_RESET_COUNTER_VALUE:
        flr_reset_counter_value_ = data; break;
    default:
        // Hole inside the window: WI.
        break;
    }

    // Any register write may change a derived output (ss_reset_ctrl, isolate,
    // sync, ss_config); request a single recompute.
    schedule_recompute();
    return true;
}

// ---------------------------------------------------------------------------
// b_transport — the only blocking TLM entry point
// ---------------------------------------------------------------------------

void reset_unit::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if (length != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if ((addr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_streaming_width() != length) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_byte_enable_ptr() != nullptr && gp.get_byte_enable_length() != 0) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }
    if (addr >= reset_unit_cfg::WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    bool ok = true;
    if (gp.is_read()) {
        uint32_t data = 0;
        ok = reg_read(addr, data);
        if (ok) std::memcpy(buf, &data, 4);
    } else if (gp.is_write()) {
        uint32_t data = 0;
        std::memcpy(&data, buf, 4);
        ok = reg_write(addr, data);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    if (!ok) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

// ---------------------------------------------------------------------------
// transport_dbg — back-door register access, no delay, no FLR side effects
// ---------------------------------------------------------------------------

unsigned int reset_unit::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if (length != 4 || (addr & 0x3u) != 0 ||
        addr >= reset_unit_cfg::WINDOW_SIZE) {
        return 0;
    }

    if (gp.is_read()) {
        uint32_t data = 0;
        if (!reg_read(addr, data)) return 0;
        std::memcpy(buf, &data, 4);
    } else {
        uint32_t data = 0;
        std::memcpy(&data, buf, 4);
        if (!reg_write(addr, data)) return 0;
    }
    return length;
}

// ---------------------------------------------------------------------------
// Configuration / debug API
// ---------------------------------------------------------------------------

void reset_unit::set_jtag_ctrl(const jtag_reset_ctrl& j)
{
    jtag_ = j;
    process_state_change();
}

uint32_t reset_unit::dbg_read(uint64_t off) const
{
    uint32_t data = 0;
    reg_read(off, data);
    return data;
}

reset_ctrl_t reset_unit::dbg_ss_reset_ctrl(unsigned i) const
{
    reset_ctrl_t r;
    if (i >= cfg_.num_subsystems) return r;
    r.cold_reset_n = bit(jtag_.ss_cold_reset_n_ovrd, i)
                         ? bit(jtag_.ss_cold_reset_n_val, i)
                         : bit(ss_cold_reset_n_, i);
    r.warm_reset_n = bit(jtag_.ss_warm_reset_n_ovrd, i)
                         ? bit(jtag_.ss_warm_reset_n_val, i)
                         : bit(ss_warm_reset_n_, i);
    r.config_state_hold    = bit(ss_config_hold_, i);
    r.critical_signal_hold = bit(ss_critical_hold_, i);
    r.sram_hold            = bit(ss_sram_hold_, i);
    r.debug_hold           = bit(ss_debug_hold_, i);
    r.force_to_ref_clk_n   = bit(ss_force_to_ref_clk_n_, i);
    return r;
}

void reset_unit::dump_state(std::ostream& os) const
{
    const derived_t d = derive();
    os << "reset_unit state @ " << sc_core::sc_time_stamp() << "\n"
       << "  num_subsystems    = " << cfg_.num_subsystems << "\n"
       << "  ref_clk_period_ns = " << cfg_.ref_clk_period_ns << "\n"
       << std::hex << std::setfill('0')
       << "  SS_CONFIG         = 0x" << std::setw(8) << ss_config_ << "\n"
       << "  SS_CONFIG_LOCK    = 0x" << std::setw(8) << ss_config_lock_ << "\n"
       << "  SS_COLD_RESET_N   = 0x" << std::setw(8) << ss_cold_reset_n_ << "\n"
       << "  SS_WARM_RESET_N   = 0x" << std::setw(8) << ss_warm_reset_n_ << "\n"
       << "  SS_COLD_RST_LOCK  = 0x" << std::setw(8) << ss_cold_reset_lock_ << "\n"
       << "  ISOLATE_REQ_REG   = 0x" << std::setw(8) << isolate_req_reg_ << "\n"
       << "  ISOLATE_REQ_SMC   = 0x" << std::setw(8) << (isolate_req_smc_reg_ & 1u) << "\n"
       << "  FLR_COUNTER       = 0x" << std::setw(8) << flr_counter_value_ << "\n"
       << "  FLR_RESET_COUNTER = 0x" << std::setw(8) << flr_reset_counter_value_ << "\n"
       << std::dec << std::setfill(' ')
       << "  derived: powergood_stable=" << d.powergood_stable
       << " cold_n=" << d.stable_cold_rst_n
       << " primary_n=" << d.rst_primary_n
       << " core_n=" << d.rst_core_int_n
       << " wdt_n=" << d.rst_wdt_n
       << " flr_cool_n=" << flr_cool_n_ << "\n";
}

} // namespace smc
