// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file octs_system_timer.cpp
 * @brief Implementation of the OCTS System Timer model.
 *
 * `tick_method()` is a direct transcription of `system_timer_octs_core.sv`
 * (plus the CREDIT_EXPIRED peak tracker from the top-level wrapper): every
 * next-state value is computed from the current cycle's state, and only then
 * are they committed together — the same semantics as the RTL's
 * `always_ff` blocks all sampling the same pre-edge values.
 */

#include "octs_system_timer.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>

namespace smc {

using cfg_t = octs_system_timer_cfg;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

octs_system_timer::octs_system_timer(sc_core::sc_module_name name,
                                     octs_system_timer_cfg cfg)
    : sc_core::sc_module(name)
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
                         "TLM register-access annotated delay (ns). "
                         "Mutable at run time.")
    , reg_socket("reg_socket")
    , clk_i("clk_i")
    , rst_n_i("rst_n_i")
    , is_primary_i("is_primary_i")
    , timer_sync_load_i("timer_sync_load_i")
    , timer_cnt_credit_i("timer_cnt_credit_i")
    , timer_sync_load_o("timer_sync_load_o")
    , timer_cnt_credit_o("timer_cnt_credit_o")
    , timer_count_o("timer_count_o")
    , timer_gpio_enable_o("timer_gpio_enable_o")
    , cur_credits_debug_o("cur_credits_debug_o")
    , credits_left_debug_o("credits_left_debug_o")
{
    access_delay_ns_p_.add_metadata("unit",
                                    cci::cci_value(std::string("nanoseconds")));

    reg_socket.register_b_transport(this, &octs_system_timer::b_transport);
    reg_socket.register_transport_dbg(this, &octs_system_timer::transport_dbg);
    reg_socket.register_get_direct_mem_ptr(
        this, &octs_system_timer::get_direct_mem_ptr);

    SC_METHOD(tick_method);
    sensitive << clk_i.pos();
    dont_initialize();

    SC_METHOD(reset_method);
    sensitive << rst_n_i;
    dont_initialize();

    // Sole driver of every output signal.  Also re-evaluated when the mode
    // input changes, since the pulse outputs and STATUS.MODE are gated by it.
    SC_METHOD(output_method);
    sensitive << recompute_event_ << is_primary_i;
    dont_initialize();

    reset_state();

    SIM_LOG_INFO(this, "octs_system_timer instantiated: window=0x"
                           << std::hex << cfg_t::WINDOW_SIZE << " CTRL reset=0x"
                           << cfg_t::CTRL_RESET << std::dec
                           << " sync_latency=" << SYNC_LATENCY_CYCLES
                           << " cycles");
}

void octs_system_timer::reset_state()
{
    ctrl_               = cfg_t::CTRL_RESET;
    preset_lo_          = 0;
    preset_hi_          = 0;
    gpio_enable_        = false;
    start_req_          = false;
    credit_expired_max_ = 0;

    enable_         = false;
    timer_count_    = 0;
    credit_counter_ = 0;
    expected_count_ = 0;
    cur_credits_    = 0;
    credit_expired_ = 0;
    pulse_active_   = pulse_state::idle;
    pulse_counter_  = 0;

    sl_flop_ = sl_sync0_ = sl_sync1_ = sl_last_ = false;
    cc_flop_ = cc_sync0_ = cc_sync1_ = cc_last_ = false;
}

// ---------------------------------------------------------------------------
// Reset (asynchronous assertion, mirroring `negedge rst_ni`)
// ---------------------------------------------------------------------------

void octs_system_timer::reset_method()
{
    if (rst_n_i.read()) return;  // active low
    reset_state();
    schedule_recompute();
}

// ---------------------------------------------------------------------------
// One RTL clock cycle
// ---------------------------------------------------------------------------

void octs_system_timer::tick_method()
{
    if (!rst_n_i.read()) {          // synchronous hold while reset asserted
        reset_state();
        schedule_recompute();
        return;
    }

    // ---- Current-cycle (pre-edge) values --------------------------------
    const bool    primary   = is_primary_i.read();
    const uint8_t cv        = credit_val();
    const uint8_t st        = step();
    const uint8_t pw        = eff_pulse_width();
    const uint64_t preset   = (static_cast<uint64_t>(preset_hi_) << 32) | preset_lo_;

    // START is a singlepulse: visible to the datapath for exactly one cycle.
    const bool start = start_req_;

    // Edge-detect pulses are combinational (`q_sync_d & ~q_sync_q`) and are
    // gated off in PRIMARY mode, which ignores its sync inputs.
    const bool sl_pulse = !primary && (sl_sync1_ && !sl_last_);
    const bool cc_pulse = !primary && (cc_sync1_ && !cc_last_);

    // CREDIT_VAL = 0 violates the IP constraint and would underflow the
    // CREDIT_VAL-1 terminal count to 255; the model defines it as "credit
    // generation disabled" instead (see the header).  One named condition,
    // used both to hold the generator and to gate the pulse, so the two
    // cannot drift apart.
    const bool credit_enabled = primary && (cv != 0);

    // PRIMARY emits a credit pulse when the generator reaches CREDIT_VAL-1.
    const bool credit_gen_pulse =
        credit_enabled && enable_ &&
        (credit_counter_ == static_cast<uint8_t>(cv - 1));

    // ---- Next state ------------------------------------------------------

    // `enable` is sticky: only reset clears it.
    const bool enable_d = enable_ || start || sl_pulse;

    // Main counter.
    uint64_t count_d = timer_count_;
    if (!enable_ && !start && !sl_pulse) {
        count_d = 0;
    } else if (primary) {
        count_d = start ? preset : (timer_count_ + 1);
    } else {
        if (sl_pulse) {
            count_d = preset;
        } else if (cc_pulse) {
            // Re-anchor to the PRIMARY's timeline.
            count_d = expected_count_ + cv;
        } else if (cur_credits_ < cv) {
            count_d = timer_count_ + st;
        }
        // else: budget exhausted -> hold.
    }

    // PRIMARY credit generator (held at 0 in SECONDARY mode).
    uint8_t credit_counter_d = 0;
    if (credit_enabled) {
        if (!enable_ || start) {
            credit_counter_d = 0;
        } else if (credit_counter_ >= static_cast<uint8_t>(cv - 1)) {
            credit_counter_d = 0;
        } else {
            credit_counter_d = static_cast<uint8_t>(credit_counter_ + 1);
        }
    }

    // SECONDARY expected timeline (held at 0 in PRIMARY mode).
    uint64_t expected_d = 0;
    uint32_t cur_credits_d = 0;
    uint32_t credit_expired_d = 0;
    if (!primary) {
        expected_d = expected_count_;
        if (sl_pulse) {
            expected_d = preset;
        } else if (cc_pulse) {
            expected_d = expected_count_ + cv;
        }

        cur_credits_d    = cur_credits_;
        credit_expired_d = credit_expired_;
        if (cc_pulse || sl_pulse) {
            // Credits replenished.
            cur_credits_d    = 0;
            credit_expired_d = 0;
        } else if (cur_credits_ < cv) {
            // 9-bit accumulator; the RTL slices to 8 bits before adding.
            cur_credits_d = ((cur_credits_ & 0xFFu) + st) & 0x1FFu;
        } else {
            // Budget exhausted: count the cycles we are starved for.
            credit_expired_d = credit_expired_ + 1;
        }
    }

    // Pulse-generator FSM.  Note the RTL does not gate the *state* on
    // is_primary_i — only the outputs — so this is modelled verbatim.
    pulse_state ps_d = pulse_active_;
    uint8_t     pc_d = pulse_counter_;
    if (start && pulse_active_ == pulse_state::idle) {
        ps_d = pulse_state::sync_load;
        pc_d = 0;
    } else if (enable_ && credit_gen_pulse &&
               pulse_active_ == pulse_state::idle) {
        ps_d = pulse_state::credit;
        pc_d = 0;
    } else if (pulse_active_ != pulse_state::idle &&
               pulse_counter_ < static_cast<uint8_t>(pw - 1)) {
        pc_d = static_cast<uint8_t>(pulse_counter_ + 1);
    } else {
        ps_d = pulse_state::idle;
        pc_d = 0;
    }

    // Input synchronizer chain (input flop -> 2FF sync -> edge-detect flop).
    const bool sl_flop_d  = timer_sync_load_i.read();
    const bool sl_sync0_d = sl_flop_;
    const bool sl_sync1_d = sl_sync0_;
    const bool sl_last_d  = sl_sync1_;
    const bool cc_flop_d  = timer_cnt_credit_i.read();
    const bool cc_sync0_d = cc_flop_;
    const bool cc_sync1_d = cc_sync0_;
    const bool cc_last_d  = cc_sync1_;

    // CREDIT_EXPIRED peak tracker (top-level wrapper logic).
    const uint32_t credit_expired_max_d =
        (credit_expired_max_ < credit_expired_) ? credit_expired_
                                               : credit_expired_max_;

    // ---- Commit ----------------------------------------------------------
    enable_             = enable_d;
    timer_count_        = count_d;
    credit_counter_     = credit_counter_d;
    expected_count_     = expected_d;
    cur_credits_        = cur_credits_d;
    credit_expired_     = credit_expired_d;
    pulse_active_       = ps_d;
    pulse_counter_      = pc_d;
    sl_flop_            = sl_flop_d;
    sl_sync0_           = sl_sync0_d;
    sl_sync1_           = sl_sync1_d;
    sl_last_            = sl_last_d;
    cc_flop_            = cc_flop_d;
    cc_sync0_           = cc_sync0_d;
    cc_sync1_           = cc_sync1_d;
    cc_last_            = cc_last_d;
    credit_expired_max_ = credit_expired_max_d;

    start_req_ = false;  // consume the singlepulse

    schedule_recompute();
}

// ---------------------------------------------------------------------------
// Output driver (sole writer of the output signals)
// ---------------------------------------------------------------------------

void octs_system_timer::output_method()
{
    const bool primary = is_primary_i.read();

    timer_sync_load_o .write(primary && pulse_active_ == pulse_state::sync_load);
    timer_cnt_credit_o.write(primary && pulse_active_ == pulse_state::credit);
    timer_count_o     .write(timer_count_);
    timer_gpio_enable_o.write(gpio_enable_);
    cur_credits_debug_o .write(cur_credits_);
    credits_left_debug_o.write(cur_credits_ < credit_val());
}

// ---------------------------------------------------------------------------
// Register decode
// ---------------------------------------------------------------------------

bool octs_system_timer::reg_read(uint64_t off, uint32_t& data) const
{
    switch (off) {
        case cfg_t::OFF_TIMER_START:
            // singlepulse: never observable as set by software.
            data = 0;
            return true;

        case cfg_t::OFF_CTRL:
            data = ctrl_ & cfg_t::CTRL_WMASK;
            return true;

        case cfg_t::OFF_STATUS: {
            uint32_t v = 0;
            // MODE: 0 = PRIMARY, 1 = SECONDARY.
            if (!is_primary_i.read()) v |= cfg_t::STATUS_MODE;
            if (enable_ && timer_count_ > 0) v |= cfg_t::STATUS_RUNNING;
            data = v;
            return true;
        }

        case cfg_t::OFF_TIMER_PRESET_LO: data = preset_lo_; return true;
        case cfg_t::OFF_TIMER_PRESET_HI: data = preset_hi_; return true;

        case cfg_t::OFF_TIMER_COUNT_LO:
            data = static_cast<uint32_t>(timer_count_ & 0xFFFFFFFFu);
            return true;
        case cfg_t::OFF_TIMER_COUNT_HI:
            data = static_cast<uint32_t>(timer_count_ >> 32);
            return true;

        case cfg_t::OFF_CREDIT_EXPIRED: data = credit_expired_max_; return true;

        case cfg_t::OFF_TIMER_GPIO_ENABLE:
            data = gpio_enable_ ? 1u : 0u;
            return true;

        default:
            // Defensive only: the 0x24 window is exactly nine 32-bit words and
            // all nine are mapped, so an aligned in-window access can never
            // land here.  Kept so that growing the window cannot silently
            // return uninitialised data.
            data = 0;
            return true;
    }
}

bool octs_system_timer::reg_write(uint64_t off, uint32_t data)
{
    switch (off) {
        case cfg_t::OFF_TIMER_START:
            if ((data & cfg_t::START_MASK) != 0) {
                start_req_ = true;
                SIM_LOG_DEBUG(this, "START written: arming sync_load pulse");
            }
            return true;

        case cfg_t::OFF_CTRL:
            ctrl_ = regmodel::apply_write_mask(ctrl_, data, cfg_t::CTRL_WMASK);
            check_ctrl_constraints();
            schedule_recompute();  // credits_left_o depends on CREDIT_VAL
            return true;

        case cfg_t::OFF_TIMER_PRESET_LO: preset_lo_ = data; return true;
        case cfg_t::OFF_TIMER_PRESET_HI: preset_hi_ = data; return true;

        case cfg_t::OFF_CREDIT_EXPIRED:
            // "To reset the value of this register, write anything to it."
            credit_expired_max_ = 0;
            return true;

        case cfg_t::OFF_TIMER_GPIO_ENABLE:
            gpio_enable_ = (data & cfg_t::GPIO_ENABLE_MASK) != 0;
            schedule_recompute();
            return true;

        // Read-only: STATUS and the live count. Writes are ignored (WI).
        case cfg_t::OFF_STATUS:
        case cfg_t::OFF_TIMER_COUNT_LO:
        case cfg_t::OFF_TIMER_COUNT_HI:
            return true;

        default:
            // Defensive only -- see reg_read(); the window has no holes today.
            return true;  // reserved hole: write-ignored
    }
}

void octs_system_timer::check_ctrl_constraints()
{
    if (ctrl_warned_) return;
    const uint8_t cv = credit_val();
    const uint8_t pw = eff_pulse_width();
    if (cv == 0 || cv <= pw) {
        SIM_LOG_WARN(this, "CTRL violates the IP constraint CREDIT_VAL > "
                           "PULSE_WIDTH (CREDIT_VAL="
                               << unsigned(cv) << ", PULSE_WIDTH="
                               << unsigned(pw)
                               << "); credit pulses may be missed");
        ctrl_warned_ = true;
    }
}

// ---------------------------------------------------------------------------
// TLM
// ---------------------------------------------------------------------------

bool octs_system_timer::check_access(const tlm::tlm_generic_payload& gp,
                                     tlm::tlm_response_status& status) const
{
    const uint64_t adr = gp.get_address();
    const unsigned len = gp.get_data_length();

    if (gp.get_data_ptr() == nullptr || len == 0) {
        status = tlm::TLM_GENERIC_ERROR_RESPONSE;
        return false;
    }

    // memmap.adoc: "Only 32-bit accesses are supported; byte or halfword
    // accesses are not allowed and may cause errors."
    if (len != 4 || (adr & 0x3u) != 0) {
        status = tlm::TLM_BURST_ERROR_RESPONSE;
        return false;
    }

    // Overflow-safe window bound: `adr + len` wraps for an address near
    // UINT64_MAX and would alias a wild access into the register window.
    if (adr >= cfg_t::WINDOW_SIZE || len > cfg_t::WINDOW_SIZE - adr) {
        status = tlm::TLM_ADDRESS_ERROR_RESPONSE;
        return false;
    }

    // Byte enables are not modelled; any non-null pointer is refused whatever
    // its length, including the zero-length and all-lanes-enabled forms.
    if (gp.get_byte_enable_ptr() != nullptr) {
        status = tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;
        return false;
    }

    // Single-beat access: TLM-2.0 requires streaming_width >= data_length.
    if (gp.get_streaming_width() < len) {
        status = tlm::TLM_BURST_ERROR_RESPONSE;
        return false;
    }

    status = tlm::TLM_OK_RESPONSE;
    return true;
}

void octs_system_timer::b_transport(tlm::tlm_generic_payload& gp,
                                    sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    unsigned char* const   ptr = gp.get_data_ptr();

    // Reads sample a live counter and writes fire the START singlepulse and
    // the CREDIT_EXPIRED clear, so this window can never be DMI'd.
    gp.set_dmi_allowed(false);

    // Optional canonical sideband.  Authorization for this CSR port belongs to
    // the fabric's axi_filter upstream, so the fields are inspected for tracing
    // only and the payload is left untouched.
    const smc::smc_axi_extension* axi =
        gp.get_extension<smc::smc_axi_extension>();
    (void)axi;

    tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
    if (!check_access(gp, status)) {
        gp.set_response_status(status);
        if (status == tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            SIM_LOG_DEBUG(this, "decode miss at off=0x" << std::hex << adr);
        }
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t data = 0;
        reg_read(adr, data);
        std::memcpy(ptr, &data, sizeof(data));
        SIM_LOG_TRACE(this, "read off=0x" << std::hex << adr << " data=0x"
                                          << data << std::dec);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t data = 0;
        std::memcpy(&data, ptr, sizeof(data));
        reg_write(adr, data);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr << " data=0x"
                                           << data << std::dec);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int octs_system_timer::transport_dbg(tlm::tlm_generic_payload& gp)
{
    tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
    if (!check_access(gp, status)) return 0;

    // Side-effect-free: a debug read goes through the same decode as a
    // software read (the CSRs have no read side effects), but a debug write is
    // refused outright rather than arming START or clearing CREDIT_EXPIRED.
    if (gp.get_command() != tlm::TLM_READ_COMMAND) return 0;

    uint32_t data = 0;
    reg_read(gp.get_address(), data);
    std::memcpy(gp.get_data_ptr(), &data, sizeof(data));
    return gp.get_data_length();
}

bool octs_system_timer::get_direct_mem_ptr(tlm::tlm_generic_payload& gp,
                                           tlm::tlm_dmi& dmi_data)
{
    (void)gp;
    dmi_data.allow_none();
    dmi_data.set_start_address(0);
    dmi_data.set_end_address(cfg_t::WINDOW_SIZE - 1);
    return false;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void octs_system_timer::dump_state(std::ostream& os) const
{
    const bool primary = is_primary_i.read();
    os << "[" << name() << "] octs_system_timer state:\n"
       << "  mode           : " << (primary ? "PRIMARY" : "SECONDARY") << "\n"
       << "  enable         : " << enable_ << "\n"
       << "  CTRL           : 0x" << std::hex << ctrl_ << std::dec
       << "  (CREDIT_VAL=" << unsigned(credit_val())
       << " PULSE_WIDTH=" << unsigned(pulse_width())
       << " STEP=" << unsigned(step()) << ")\n"
       << "  preset         : 0x" << std::hex
       << ((static_cast<uint64_t>(preset_hi_) << 32) | preset_lo_) << "\n"
       << "  timer_count    : 0x" << timer_count_ << std::dec
       << " (" << timer_count_ << ")\n"
       << "  expected_count : " << expected_count_ << "\n"
       << "  cur_credits    : " << cur_credits_ << "\n"
       << "  credit_counter : " << unsigned(credit_counter_) << "\n"
       << "  credit_expired : " << credit_expired_
       << " (peak " << credit_expired_max_ << ")\n"
       << "  pulse_active   : "
       << (pulse_active_ == pulse_state::idle
               ? "IDLE"
               : (pulse_active_ == pulse_state::sync_load ? "SYNC_LOAD"
                                                          : "CREDIT"))
       << " counter=" << unsigned(pulse_counter_) << "\n"
       << "  gpio_enable    : " << gpio_enable_ << "\n";
}

}  // namespace smc
