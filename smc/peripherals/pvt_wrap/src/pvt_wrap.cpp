// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// smc/peripherals/pvt_wrap/src/pvt_wrap.cpp
// ===========================================================================

#include "pvt_wrap.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>

namespace smc {

namespace {

using regmodel::apply_write_mask;
using regmodel::bit;

// Register field masks (from hw/periph/pvt/data/registers/rdl/pvt_wrap.rdl).
constexpr uint32_t PROCESS_CTRL_MASK            = 0x0000'0111u; // bits 0, 4, 8
constexpr uint32_t PROCESS_STATUS_MASK          = 0x0000'0001u; // bit 0
constexpr uint32_t VOLTAGE_CTRL_MASK            = 0x0000'0001u; // bit 0
constexpr uint32_t VOLTAGE_STATUS_MASK          = 0x0007'0007u; // bits 0, 4, 18:16
constexpr uint32_t VOLTAGE_STATUS_CODE_SHIFT    = 16;
constexpr uint32_t TEMP_CTRL_MASK               = 0x0000'0001u; // bit 0
constexpr uint32_t TEMP_STATUS_MASK             = 0xFFFF'0001u; // bits 0, 31:16
constexpr uint32_t TEMP_STATUS_OUTPUT_SHIFT     = 16;
constexpr uint32_t TEMP_INTERRUPT_MASK          = 0x0000'0001u; // bit 0

constexpr uint32_t VOLTAGE_CODE_DEFAULT         = 0x4u; // mid-scale droop code
constexpr uint32_t TEMP_OUTPUT_DEFAULT          = 0xABCDu; // simulated temperature reading

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

pvt_wrap::pvt_wrap(sc_core::sc_module_name name, pvt_wrap_cfg cfg)
    : sc_core::sc_module(name)
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM b_transport annotated delay in nanoseconds. "
          "Approximates AXI4-Lite register-access latency. Mutable at run-time.")
    , tick_period_ns_p_(
          "tick_period_ns",
          cfg.tick_period_ns,
          "Simulated PVT sensor tick period in nanoseconds. "
          "Default 10 ns; set to 0.0 to disable auto-tick.")
    , cfg_(cfg)
{
    cfg_.access_delay_ns = access_delay_ns_p_.get_value();
    cfg_.tick_period_ns  = tick_period_ns_p_.get_value();

    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));
    tick_period_ns_p_.add_metadata("unit",       cci::cci_value(std::string("nanoseconds")));

    if (cfg_.access_delay_ns < 0.0) {
        SC_REPORT_FATAL(name, "pvt_wrap access_delay_ns must be >= 0");
    }
    if (cfg_.tick_period_ns < 0.0) {
        SC_REPORT_FATAL(name, "pvt_wrap tick_period_ns must be >= 0");
    }

    access_delay_ = sc_core::sc_time(cfg_.access_delay_ns, sc_core::SC_NS);
    tick_period_  = sc_core::sc_time(cfg_.tick_period_ns,  sc_core::SC_NS);

    reg_socket.register_b_transport  (this, &pvt_wrap::b_transport);
    reg_socket.register_transport_dbg(this, &pvt_wrap::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_METHOD(tick_method);
    sensitive << tick_event_;
    dont_initialize();

    SC_METHOD(output_method);
    sensitive << recompute_event_;
    // Initialize so outputs are driven to their reset levels at time 0.

    SIM_LOG_INFO(this,
        "pvt_wrap instantiated: access_delay_ns=" << cfg_.access_delay_ns
        << ", tick_period_ns=" << cfg_.tick_period_ns);
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void pvt_wrap::reset_proc()
{
    if (rst_n_i.read()) {
        return; // de-assertion edge: nothing to do
    }

    process_ctrl_            = 0;
    process_status_          = 0;
    refclk_count_period_lo_  = 0;
    refclk_count_period_hi_  = 0;
    process_clock_counter_   = 0;
    voltage_ctrl_            = 0;
    voltage_status_          = 0;
    temp_ctrl_               = 0;
    temp_status_             = 0;
    temp_interrupt_          = 0;

    refclk_cnt_              = 0;
    process_clk_count_valid_ = false;
    voltage_ready_           = false;
    temp_ready_              = false;

    tick_event_.cancel();
    recompute_event_.cancel();

    update_status_regs();
    schedule_recompute();
}

// ---------------------------------------------------------------------------
// Periodic tick
// ---------------------------------------------------------------------------

void pvt_wrap::tick_method()
{
    if (!rst_n_i.read()) {
        return;
    }

    if (process_clk_count_en()) {
        const uint64_t period = refclk_period();
        if (period != 0 && refclk_cnt_ >= period) {
            process_clk_count_valid_ = true;
            process_clock_counter_   = refclk_cnt_;
            refclk_cnt_              = 0;
        } else {
            ++refclk_cnt_;
        }
        update_status_regs();
        schedule_recompute();
        schedule_tick();
    }
}

void pvt_wrap::schedule_tick()
{
    if (tick_period_ != sc_core::SC_ZERO_TIME) {
        tick_event_.notify(tick_period_);
    }
}

// ---------------------------------------------------------------------------
// Output driver
// ---------------------------------------------------------------------------

void pvt_wrap::output_method()
{
    const bool process_clk_obs_en = process_obs_clk_enable();
    if (process_clk_obs_en != prev_process_clk_obs_en_) {
        process_clk_obs_en_o.write(process_clk_obs_en);
        prev_process_clk_obs_en_ = process_clk_obs_en;
    }

    const bool process_clk_obs = process_clk_obs_en && process_enable();
    if (process_clk_obs != prev_process_clk_obs_) {
        process_clk_obs_o.write(process_clk_obs);
        prev_process_clk_obs_ = process_clk_obs;
    }

    const uint32_t voltage_code = voltage_reset_n() ? VOLTAGE_CODE_DEFAULT : 0;
    if (voltage_code != prev_voltage_code_) {
        voltage_code_o.write(voltage_code & 0x7u);
        prev_voltage_code_ = voltage_code;
    }

    temp_interrupt_o.write(temp_ready_ && process_clk_count_valid_);
}

void pvt_wrap::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void pvt_wrap::update_status_regs()
{
    voltage_ready_ = voltage_reset_n();
    temp_ready_    = temp_en();

    process_status_ = (process_clk_count_valid_ ? 1u : 0u) & PROCESS_STATUS_MASK;

    voltage_status_ = 0;
    if (voltage_ready_) {
        voltage_status_ |= 0x1u;                           // voltage_ready
        voltage_status_ |= ((VOLTAGE_CODE_DEFAULT & 0x7u) << VOLTAGE_STATUS_CODE_SHIFT);
    }
    voltage_status_ &= VOLTAGE_STATUS_MASK;

    temp_status_ = 0;
    if (temp_ready_) {
        temp_status_ |= 0x1u;                            // temp_ready
        temp_status_ |= ((TEMP_OUTPUT_DEFAULT & 0xFFFFu) << TEMP_STATUS_OUTPUT_SHIFT);
    }
    temp_status_ &= TEMP_STATUS_MASK;

    temp_interrupt_ = ((temp_ready_ && process_clk_count_valid_) ? 1u : 0u) & TEMP_INTERRUPT_MASK;
}

// ---------------------------------------------------------------------------
// TLM b_transport
// ---------------------------------------------------------------------------

void pvt_wrap::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    // The upstream router has already rebased the address to a 0-based offset
    // within this peripheral's window, so use it directly.
    const uint64_t off = gp.get_address();

    if (!gp.is_read() && !gp.is_write()) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (gp.get_data_length() != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_data_ptr() == nullptr) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    delay += access_delay_;

    bool ok = false;
    uint32_t data = 0;
    if (gp.is_read()) {
        ok = reg_read(off, data);
        if (ok) {
            std::memcpy(gp.get_data_ptr(), &data, 4);
        }
        SIM_LOG_TRACE(this, "read off=0x" << std::hex << off
                      << " data=0x" << data << (ok ? "" : " (decode miss)"));
    } else {
        std::memcpy(&data, gp.get_data_ptr(), 4);
        ok = reg_write(off, data);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << off
                      << " data=0x" << data << (ok ? "" : " (decode miss)"));
    }

    if (!ok) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    } else {
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

unsigned int pvt_wrap::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const uint64_t off = gp.get_address();

    if (gp.get_data_ptr() == nullptr || gp.get_data_length() != 4)
        return 0;

    uint32_t data = 0;
    if (gp.is_read()) {
        if (reg_read(off, data)) {
            std::memcpy(gp.get_data_ptr(), &data, 4);
            return 4;
        }
    } else if (gp.is_write()) {
        uint32_t value = 0;
        std::memcpy(&value, gp.get_data_ptr(), 4);
        if (reg_write(off, value)) {
            return 4;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Register read
// ---------------------------------------------------------------------------

bool pvt_wrap::reg_read(uint64_t off, uint32_t& data) const
{
    data = 0;
    if (off >= pvt_wrap_cfg::WINDOW_SIZE) {
        return false;
    }

    switch (off) {
    case pvt_wrap_cfg::PROCESS_CTRL_OFF:
        data = process_ctrl_ & PROCESS_CTRL_MASK;
        return true;
    case pvt_wrap_cfg::PROCESS_STATUS_OFF:
        data = process_status_ & PROCESS_STATUS_MASK;
        return true;
    case pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF:
        data = refclk_count_period_lo_;
        return true;
    case pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF:
        data = refclk_count_period_hi_;
        return true;
    case pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_LO_OFF:
        data = static_cast<uint32_t>(process_clock_counter_ & 0xFFFF'FFFFu);
        return true;
    case pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF:
        data = static_cast<uint32_t>((process_clock_counter_ >> 32) & 0xFFFF'FFFFu);
        return true;
    case pvt_wrap_cfg::VOLTAGE_CTRL_OFF:
        data = voltage_ctrl_ & VOLTAGE_CTRL_MASK;
        return true;
    case pvt_wrap_cfg::VOLTAGE_STATUS_OFF:
        data = voltage_status_;
        return true;
    case pvt_wrap_cfg::TEMP_CTRL_OFF:
        data = temp_ctrl_ & TEMP_CTRL_MASK;
        return true;
    case pvt_wrap_cfg::TEMP_STATUS_OFF:
        data = temp_status_;
        return true;
    case pvt_wrap_cfg::TEMP_INTERRUPT_OFF:
        data = temp_interrupt_;
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Register write
// ---------------------------------------------------------------------------

bool pvt_wrap::reg_write(uint64_t off, uint32_t data)
{
    if (off >= pvt_wrap_cfg::WINDOW_SIZE) {
        return false;
    }

    switch (off) {
    case pvt_wrap_cfg::PROCESS_CTRL_OFF: {
        const bool was_counting = process_clk_count_en();
        process_ctrl_ = apply_write_mask(process_ctrl_, data, PROCESS_CTRL_MASK);
        if (process_clk_count_en() && !was_counting) {
            refclk_cnt_ = 0;
            schedule_tick();
        }
        if (!process_clk_count_en()) {
            process_clk_count_valid_ = false;
        }
        update_status_regs();
        schedule_recompute();
        return true;
    }
    case pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_LO_OFF:
        refclk_count_period_lo_ = data;
        return true;
    case pvt_wrap_cfg::REF_CLK_COUNT_PERIOD_HI_OFF:
        refclk_count_period_hi_ = data;
        return true;
    case pvt_wrap_cfg::VOLTAGE_CTRL_OFF:
        voltage_ctrl_ = apply_write_mask(voltage_ctrl_, data, VOLTAGE_CTRL_MASK);
        update_status_regs();
        schedule_recompute();
        return true;
    case pvt_wrap_cfg::TEMP_CTRL_OFF:
        temp_ctrl_ = apply_write_mask(temp_ctrl_, data, TEMP_CTRL_MASK);
        update_status_regs();
        schedule_recompute();
        return true;
    case pvt_wrap_cfg::PROCESS_STATUS_OFF:
    case pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_LO_OFF:
    case pvt_wrap_cfg::PROCESS_CLOCK_COUNTER_HI_OFF:
    case pvt_wrap_cfg::VOLTAGE_STATUS_OFF:
    case pvt_wrap_cfg::TEMP_STATUS_OFF:
    case pvt_wrap_cfg::TEMP_INTERRUPT_OFF:
        // Read-only registers.
        return true;
    default:
        return false;
    }
}

}  // namespace smc
