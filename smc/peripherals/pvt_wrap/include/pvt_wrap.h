// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// smc/peripherals/pvt_wrap/include/pvt_wrap.h
// ===========================================================================

#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include <cci_configuration>

#include "reg_access.h"
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// pvt_wrap_cfg
// ---------------------------------------------------------------------------

struct pvt_wrap_cfg {
    /// Default TLM b_transport annotated delay in nanoseconds.
    double access_delay_ns = 2.0;

    /// Simulated sensor/tick period in nanoseconds. 0 disables auto-tick.
    double tick_period_ns = 10.0;

    /// Base address in the SMC address map (informational only).
    static constexpr uint64_t SMC_BASE_ADDR = 0xC040'2000ULL;

    /// Window size (4 KiB).
    static constexpr uint64_t WINDOW_SIZE = 0x1000ULL;

    // Register offsets from hw/periph/pvt/data/registers/rdl/pvt_wrap.rdl
    static constexpr uint64_t PROCESS_CTRL_OFF            = 0x00;
    static constexpr uint64_t PROCESS_STATUS_OFF          = 0x04;
    static constexpr uint64_t REF_CLK_COUNT_PERIOD_LO_OFF = 0x08;
    static constexpr uint64_t REF_CLK_COUNT_PERIOD_HI_OFF = 0x0C;
    static constexpr uint64_t PROCESS_CLOCK_COUNTER_LO_OFF = 0x10;
    static constexpr uint64_t PROCESS_CLOCK_COUNTER_HI_OFF = 0x14;
    static constexpr uint64_t VOLTAGE_CTRL_OFF            = 0x18;
    static constexpr uint64_t VOLTAGE_STATUS_OFF          = 0x1C;
    static constexpr uint64_t TEMP_CTRL_OFF               = 0x20;
    static constexpr uint64_t TEMP_STATUS_OFF             = 0x24;
    static constexpr uint64_t TEMP_INTERRUPT_OFF           = 0x28;
};

// ---------------------------------------------------------------------------
// pvt_wrap
// ---------------------------------------------------------------------------

class pvt_wrap : public sc_core::sc_module
{
public:
    // TLM-2.0 target socket (32-bit AXI-Lite-style register access).
    tlm_utils::simple_target_socket<pvt_wrap, 32> reg_socket{"reg_socket"};

    // Active-low synchronous reset.
    sc_core::sc_in<bool> rst_n_i{"rst_n_i"};

    // Process clock observation outputs.
    sc_core::sc_out<bool> process_clk_obs_o{"process_clk_obs_o"};
    sc_core::sc_out<bool> process_clk_obs_en_o{"process_clk_obs_en_o"};

    // Voltage droop code output.
    sc_core::sc_out<uint32_t> voltage_code_o{"voltage_code_o"};

    // Temperature interrupt output.
    sc_core::sc_out<bool> temp_interrupt_o{"temp_interrupt_o"};

    SC_HAS_PROCESS(pvt_wrap);
    explicit pvt_wrap(sc_core::sc_module_name name, pvt_wrap_cfg cfg = {});

private:
    // CCI parameters.
    cci::cci_param<double> access_delay_ns_p_{"access_delay_ns", 2.0,
        "TLM b_transport annotated delay in nanoseconds."};
    cci::cci_param<double> tick_period_ns_p_{"tick_period_ns", 10.0,
        "Simulated PVT sensor tick period in nanoseconds. 0 disables auto-tick."};

    // Register state.
    uint32_t process_ctrl_ = 0;
    uint32_t process_status_ = 0;
    uint32_t refclk_count_period_lo_ = 0;
    uint32_t refclk_count_period_hi_ = 0;
    uint64_t process_clock_counter_ = 0;
    uint32_t voltage_ctrl_ = 0;
    uint32_t voltage_status_ = 0;
    uint32_t temp_ctrl_ = 0;
    uint32_t temp_status_ = 0;
    uint32_t temp_interrupt_ = 0;

    // Counter/tick state.
    uint64_t refclk_cnt_ = 0;
    bool process_clk_count_valid_ = false;
    bool voltage_ready_ = false;
    bool temp_ready_ = false;

    // Time-typed delays.
    sc_core::sc_time access_delay_;
    sc_core::sc_time tick_period_;

    // Events.
    sc_core::sc_event tick_event_;
    sc_core::sc_event recompute_event_;

    // Cached output values.
    bool prev_process_clk_obs_en_ = false;
    bool prev_process_clk_obs_ = false;
    uint32_t prev_voltage_code_ = 0;

    // Configuration copy.
    pvt_wrap_cfg cfg_;

    // TLM handlers.
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    bool reg_read(uint64_t off, uint32_t& data) const;
    bool reg_write(uint64_t off, uint32_t data);

    // SC processes.
    void reset_proc();
    void tick_method();
    void output_method();
    void update_status_regs();

    void schedule_recompute();
    void schedule_tick();

    // Helper accessors.
    bool process_enable() const { return regmodel::bit(process_ctrl_, 0); }
    bool process_obs_clk_enable() const { return regmodel::bit(process_ctrl_, 4); }
    bool process_clk_count_en() const { return regmodel::bit(process_ctrl_, 8); }
    bool voltage_reset_n() const { return regmodel::bit(voltage_ctrl_, 0); }
    bool temp_en() const { return regmodel::bit(temp_ctrl_, 0); }

    uint64_t refclk_period() const {
        return (uint64_t{refclk_count_period_hi_} << 32) | refclk_count_period_lo_;
    }
};

}  // namespace smc
