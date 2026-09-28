// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file octs_system_timer.h
 * @brief SystemC/TLM-2.0 cycle-accurate model of the OCTS System Timer.
 *
 * Models the "System Timer OCTS" (Open Chiplet Time Synchronization) IP: a
 * 64-bit timer that provides nanosecond-level time coordination across
 * chiplets using a hierarchical PRIMARY/SECONDARY architecture.  A PRIMARY
 * timer owns the master timeline and emits `sync_load` / `credit` pulses;
 * SECONDARY timers load from the same preset on `sync_load` and then advance
 * on a credit budget, halting when their credits are exhausted.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/ip/system_timer_octs/data/registers/rdl/system_timer_octs.rdl` | **Ground-truth register map** |
 * | `hw/ip/system_timer_octs/rtl/system_timer_octs_core.sv`            | **Ground-truth datapath / FSM** |
 * | `hw/ip/system_timer_octs/rtl/templates/system_timer_octs.sv.tpl`   | Top-level CSR wiring (STEP, CREDIT_EXPIRED max tracker) |
 * | `hw/common/prim/rtl/prim_edge_detector.sv`                         | Input synchronizer + edge-pulse semantics |
 * | `hw/ip/system_timer_octs/doc/{architecture,interface,memmap}.adoc` | Behavioural spec (also in `doc/dist/ocah-documentation.pdf`) |
 *
 * ---
 * ## Register map (offsets relative to the IP's window base)
 *
 * ```
 * Off   Name                SW   Reset        Description
 * ─────────────────────────────────────────────────────────────────────────────
 * 0x00  TIMER_START         RW   0x00000000   START[0], singlepulse. Writing 1
 *                                             emits a one-cycle start pulse:
 *                                             loads the preset and (PRIMARY)
 *                                             emits sync_load. Reads back 0.
 * 0x04  CTRL                RW   0x0001020A   CREDIT_VAL[7:0]   = 0x0A
 *                                             PULSE_WIDTH[15:8] = 0x02
 *                                             STEP[23:16]       = 0x01
 *                                             [31:24] reserved (RAZ/WI)
 * 0x08  STATUS              RO   hw           MODE[0]    0=PRIMARY, 1=SECONDARY
 *                                             RUNNING[4] enable && count > 0
 * 0x0C  TIMER_PRESET_LO     RW   0x00000000   Preset[31:0]
 * 0x10  TIMER_PRESET_HI     RW   0x00000000   Preset[63:32]
 * 0x14  TIMER_COUNT_LO      RO   hw           Live count[31:0]
 * 0x18  TIMER_COUNT_HI      RO   hw           Live count[63:32]
 * 0x1C  CREDIT_EXPIRED      R/W0 0x00000000   MAX_CYCLES_EXPIRED[31:0]: peak
 *                                             number of consecutive cycles the
 *                                             credit budget stayed exhausted
 *                                             (SECONDARY only; always 0 for a
 *                                             PRIMARY). Writing *any* value
 *                                             clears it.
 * 0x20  TIMER_GPIO_ENABLE   RW   0x00000000   GPIO_ENABLE[0] -> gpio_enable_o
 *
 * Window size 0x24 (36 B). Offsets >= 0x24 -> TLM_ADDRESS_ERROR_RESPONSE.
 * ```
 *
 * Per `memmap.adoc`, **only naturally aligned 32-bit accesses are supported**;
 * byte/halfword or misaligned accesses return `TLM_BURST_ERROR_RESPONSE`.  The
 * 64-bit preset and count are therefore split into adjacent LO/HI registers.
 *
 * ---
 * ## Timing model
 *
 * Unlike the loosely-timed register-file peripherals in this tree, the OCTS
 * timer's whole function *is* its per-cycle behaviour (counter increment,
 * credit accounting, programmable pulse widths, synchronizer latency).  This
 * model is therefore **clock-driven**: it takes an explicit `clk_i` port and
 * evaluates one RTL clock cycle per rising edge, reproducing
 * `system_timer_octs_core.sv` register-transfer semantics (all next-state
 * values are computed from the current state, then committed together).
 *
 * That makes pulse widths, credit periods and synchronizer latency directly
 * observable and regression-testable, at the cost of requiring a clock.  A
 * testbench just binds an `sc_clock`.
 *
 * ### Input synchronizer latency
 *
 * `timer_sync_load_i` / `timer_cnt_credit_i` are asynchronous (they cross from
 * the PRIMARY's clock domain).  The RTL path is: one input flop, then
 * `prim_edge_detector` (a 2-flop synchronizer plus one edge-detect flop).  A
 * rising edge on an input is therefore *consumed* by the datapath
 * @ref SYNC_LATENCY_CYCLES rising edges later.  This is modelled exactly, so a
 * SECONDARY's count trails its PRIMARY by that many cycles.
 *
 * ---
 * ## Mode behaviour
 *
 * `is_primary_i` selects the role at run time (it is a signal, not a
 * parameter, matching the RTL).
 *
 * - **PRIMARY**: `enable` latches on the START pulse and the counter loads the
 *   preset, then increments by 1 every cycle.  An 8-bit credit counter runs
 *   0..CREDIT_VAL-1; on wrap it emits a credit pulse.  Both `sync_load_o` and
 *   `cnt_credit_o` are asserted for PULSE_WIDTH cycles (0 is treated as 1).
 *   Sync *inputs* are ignored.
 * - **SECONDARY**: idle until a `sync_load` pulse arrives, which loads the
 *   preset and latches `enable`.  Thereafter the counter advances by STEP each
 *   cycle while `cur_credits < CREDIT_VAL`, accumulating `cur_credits` by STEP.
 *   Once the budget is exhausted the counter *halts* and a 32-bit
 *   credit-expired counter ticks (surfaced as the peak via CREDIT_EXPIRED).  A
 *   credit pulse resets the budget and re-anchors the counter to
 *   `expected_count + CREDIT_VAL`.  Sync *outputs* stay low.
 *
 * `CREDIT_VAL` must be greater than `PULSE_WIDTH` (otherwise credit pulses are
 * missed) and non-zero; the model warns once if software violates either, in
 * lieu of the RTL's `CreditValGreaterThanPulseWidth_A` assertion.  The warning
 * is deliberately once per model lifetime (it is a modelling aid standing in
 * for an RTL assertion, and re-warning every cycle would drown the log); reset
 * does not re-arm it.
 *
 * ### CREDIT_VAL = 0
 *
 * `CREDIT_VAL = 0` violates the IP constraint.  The RTL compares the credit
 * generator against `CREDIT_VAL - 1`, which underflows to 255 and yields a
 * 256-cycle period rather than anything meaningful.  Rather than reproduce an
 * accident, this model defines `CREDIT_VAL = 0` as **credit generation
 * disabled**: a PRIMARY emits no credit pulses and holds its generator at 0,
 * and a SECONDARY (whose budget `cur_credits < CREDIT_VAL` can never be
 * satisfied) is starved from the outset.  Both are tested.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type   | Default | Mutability | Purpose |
 * |-------------------|--------|---------|------------|---------|
 * | `access_delay_ns` | double | 2.0     | mutable    | TLM `b_transport` annotated delay. |
 */

#ifndef SMC_OCTS_SYSTEM_TIMER_H_
#define SMC_OCTS_SYSTEM_TIMER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>

#include <cstdint>
#include <iostream>

// Canonical SMC sideband type; this peripheral must not redefine it.
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// octs_system_timer_cfg
// ---------------------------------------------------------------------------

/// Address-map constants and construction defaults, taken from
/// `system_timer_octs.rdl`.  The offsets are fixed by the RDL.
struct octs_system_timer_cfg {
    /// TLM `b_transport` annotated delay, in nanoseconds (CCI-overridable).
    double access_delay_ns = 2.0;

    // Register offsets (bytes).
    static constexpr uint64_t OFF_TIMER_START       = 0x00;
    static constexpr uint64_t OFF_CTRL              = 0x04;
    static constexpr uint64_t OFF_STATUS            = 0x08;
    static constexpr uint64_t OFF_TIMER_PRESET_LO   = 0x0C;
    static constexpr uint64_t OFF_TIMER_PRESET_HI   = 0x10;
    static constexpr uint64_t OFF_TIMER_COUNT_LO    = 0x14;
    static constexpr uint64_t OFF_TIMER_COUNT_HI    = 0x18;
    static constexpr uint64_t OFF_CREDIT_EXPIRED    = 0x1C;
    static constexpr uint64_t OFF_TIMER_GPIO_ENABLE = 0x20;

    /// Total register window: 0x00..0x23 inclusive (36 B).
    static constexpr uint64_t WINDOW_SIZE = 0x24;

    // ---- Reset values (RDL field defaults) ----
    static constexpr uint32_t CTRL_RESET = 0x0001020A;  // STEP=1, PW=2, CV=10

    // ---- Field masks / positions ----
    static constexpr uint32_t START_MASK       = 0x1u;
    static constexpr uint32_t CTRL_WMASK       = 0x00FFFFFFu;  // [31:24] rsvd
    static constexpr uint32_t STATUS_MODE      = 0x1u << 0;
    static constexpr uint32_t STATUS_RUNNING   = 0x1u << 4;
    static constexpr uint32_t STATUS_RMASK     = STATUS_MODE | STATUS_RUNNING;
    static constexpr uint32_t GPIO_ENABLE_MASK = 0x1u;
};

// ---------------------------------------------------------------------------
// octs_system_timer
// ---------------------------------------------------------------------------

/**
 * @brief Cycle-accurate SystemC model of the OCTS System Timer.
 *
 * ### Ports
 *
 * | Port                  | Dir | Width | Description |
 * |-----------------------|-----|-------|-------------|
 * | `reg_socket`          | tgt | —     | TLM-2.0 target; 32-bit register access |
 * | `clk_i`               | in  | 1b    | Clock; one RTL cycle per rising edge |
 * | `rst_n_i`             | in  | 1b    | Active-low reset (async assert) |
 * | `is_primary_i`        | in  | 1b    | 1 = PRIMARY, 0 = SECONDARY |
 * | `timer_sync_load_i`   | in  | 1b    | Sync-load pulse in (SECONDARY) |
 * | `timer_cnt_credit_i`  | in  | 1b    | Credit pulse in (SECONDARY) |
 * | `timer_sync_load_o`   | out | 1b    | Sync-load pulse out (PRIMARY) |
 * | `timer_cnt_credit_o`  | out | 1b    | Credit pulse out (PRIMARY) |
 * | `timer_count_o`       | out | 64b   | Live counter value |
 * | `timer_gpio_enable_o` | out | 1b    | TIMER_GPIO_ENABLE.GPIO_ENABLE |
 * | `cur_credits_debug_o` | out | 9b    | Credit accumulator (debug) |
 * | `credits_left_debug_o`| out | 1b    | `cur_credits < CREDIT_VAL` (debug) |
 *
 * ### Internal processes
 *
 * | Process         | Sensitivity        | Purpose |
 * |-----------------|--------------------|---------|
 * | `tick_method`   | `clk_i.pos()`      | Evaluate one RTL clock cycle |
 * | `reset_method`  | `rst_n_i`          | Asynchronous reset assertion |
 * | `output_method` | `recompute_event_` + `is_primary_i` | **Sole driver** of all outputs |
 *
 * Keeping `output_method` the only writer of the output signals satisfies
 * SystemC's single-driver rule (the same pattern the PLIC and CLINT models
 * use); state-mutating code calls `schedule_recompute()`.
 */
class octs_system_timer : public sc_core::sc_module {
protected:
    /// TLM register-access annotated delay, in ns.  Mutable at run time.
    cci::cci_param<double> access_delay_ns_p_;

public:
    /// Rising edges from an input assertion until the datapath consumes the
    /// resulting edge pulse: 1 input flop + 2-flop synchronizer + 1
    /// edge-detect flop (see `prim_edge_detector.sv`).
    static constexpr unsigned SYNC_LATENCY_CYCLES = 4;

    SC_HAS_PROCESS(octs_system_timer);

    tlm_utils::simple_target_socket<octs_system_timer> reg_socket;

    sc_core::sc_in<bool> clk_i;
    sc_core::sc_in<bool> rst_n_i;
    sc_core::sc_in<bool> is_primary_i;

    sc_core::sc_in<bool> timer_sync_load_i;
    sc_core::sc_in<bool> timer_cnt_credit_i;

    sc_core::sc_out<bool>     timer_sync_load_o;
    sc_core::sc_out<bool>     timer_cnt_credit_o;
    sc_core::sc_out<uint64_t> timer_count_o;
    sc_core::sc_out<bool>     timer_gpio_enable_o;
    sc_core::sc_out<uint32_t> cur_credits_debug_o;
    sc_core::sc_out<bool>     credits_left_debug_o;

    explicit octs_system_timer(
        sc_core::sc_module_name name,
        octs_system_timer_cfg cfg = octs_system_timer_cfg{});

    // ---- Debug back door (const, no side effects) -------------------------

    uint64_t dbg_count()           const { return timer_count_; }
    uint64_t dbg_expected_count()  const { return expected_count_; }
    uint32_t dbg_cur_credits()     const { return cur_credits_; }
    uint32_t dbg_credit_expired()  const { return credit_expired_; }
    uint32_t dbg_credit_expired_max() const { return credit_expired_max_; }
    bool     dbg_enabled()         const { return enable_; }
    uint8_t  dbg_credit_counter()  const { return credit_counter_; }

    uint8_t  credit_val()   const { return static_cast<uint8_t>(ctrl_ & 0xFFu); }
    uint8_t  pulse_width()  const { return static_cast<uint8_t>((ctrl_ >> 8) & 0xFFu); }
    uint8_t  step()         const { return static_cast<uint8_t>((ctrl_ >> 16) & 0xFFu); }
    /// True when CREDIT_VAL is 0, i.e. credit generation is disabled.
    bool credit_disabled() const { return credit_val() == 0; }

    /// PULSE_WIDTH with the RTL's "0 rounds up to 1" rule applied.
    uint8_t  eff_pulse_width() const {
        const uint8_t pw = pulse_width();
        return pw == 0 ? uint8_t{1} : pw;
    }

    void dump_state(std::ostream& os = std::cout) const;

private:
    /// Pulse-generator FSM states (mirrors `pulse_state_t` in the RTL).
    enum class pulse_state : uint8_t { idle = 0, sync_load = 1, credit = 2 };

    // ---- TLM ------------------------------------------------------------
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    /// Side-effect-free back door: reads CSR state without the START
    /// singlepulse, the CREDIT_EXPIRED write-to-clear, or an annotated delay.
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);
    /// Always denies DMI — reads are live counter samples and writes trigger
    /// the START pulse and the CREDIT_EXPIRED clear.
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& gp,
                            tlm::tlm_dmi& dmi_data);
    /// Shared bus-contract check.  On failure sets @p status to the TLM
    /// response the caller should report.
    bool check_access(const tlm::tlm_generic_payload& gp,
                      tlm::tlm_response_status& status) const;
    bool reg_read (uint64_t off, uint32_t& data) const;
    bool reg_write(uint64_t off, uint32_t data);

    // ---- Processes -------------------------------------------------------
    void tick_method();
    void reset_method();
    void output_method();
    void schedule_recompute() { recompute_event_.notify(sc_core::SC_ZERO_TIME); }

    /// Restore all datapath and register state to reset values.
    void reset_state();
    /// Warn once if CTRL violates the RTL's CREDIT_VAL/PULSE_WIDTH assertions.
    void check_ctrl_constraints();

    // ---- Software-visible register state --------------------------------
    uint32_t ctrl_               = octs_system_timer_cfg::CTRL_RESET;
    uint32_t preset_lo_          = 0;
    uint32_t preset_hi_          = 0;
    bool     gpio_enable_        = false;
    /// START is a singlepulse: a write of 1 arms this, and the next rising
    /// edge presents it to the datapath for exactly one cycle.
    bool     start_req_          = false;
    /// Peak of `credit_expired_`, surfaced by the CREDIT_EXPIRED register.
    uint32_t credit_expired_max_ = 0;

    // ---- Core datapath state (values valid for the current cycle) -------
    bool        enable_          = false;   ///< Sticky; set by START / sync_load.
    uint64_t    timer_count_     = 0;
    uint8_t     credit_counter_  = 0;       ///< PRIMARY credit generator.
    uint64_t    expected_count_  = 0;       ///< SECONDARY expected timeline.
    uint32_t    cur_credits_     = 0;       ///< SECONDARY budget (9-bit).
    uint32_t    credit_expired_  = 0;       ///< SECONDARY expiry counter.
    pulse_state pulse_active_    = pulse_state::idle;
    uint8_t     pulse_counter_   = 0;

    // Input synchronizer chain: input flop -> 2-flop sync -> edge-detect flop.
    bool sl_flop_ = false, sl_sync0_ = false, sl_sync1_ = false, sl_last_ = false;
    bool cc_flop_ = false, cc_sync0_ = false, cc_sync1_ = false, cc_last_ = false;

    bool ctrl_warned_ = false;

    sc_core::sc_event recompute_event_;
    sc_core::sc_time  access_delay_;
};

}  // namespace smc

#endif  // SMC_OCTS_SYSTEM_TIMER_H_
