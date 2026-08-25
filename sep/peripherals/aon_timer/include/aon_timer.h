// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer.h
 * @brief AON Timer top-level SystemC TLM-2.0 model class (aon_timer_ip).
 *
 * This header declares the aon_timer_ip module, which derives from aon_timer_base
 * and adds all port interface declarations required by the AON Timer specification.
 * The model provides sc_out/sc_in ports for interrupts, power management, lifecycle
 * escalation, alert, clock, reset, and optional RACL interfaces. Port initialization
 * is performed in the constructor using SC_CTOR-compatible initialization lists.
 *
 * The aon_timer models the following functional behavior at LT abstraction level:
 *   - 64-bit upcounting wakeup timer with 12-bit prescaler and threshold comparison
 *   - 32-bit upcounting watchdog timer with dual bark/bite thresholds
 *   - RW1C interrupt state management (INTR_STATE)
 *   - RW0C wakeup cause management (WKUP_CAUSE)
 *   - RW0C watchdog configuration lock (WDOG_REGWEN)
 *   - Lifecycle escalation halt for both timers
 *   - Watchdog pause-in-sleep feature
 *   - Write-only alert test (ALERT_TEST) and interrupt test (INTR_TEST)
 *   - Wakeup timer counter tick engine (FUNC003)
 *
 * All registers use asynchronous CDC write completion semantics: a read-back
 * after any write is required to guarantee propagation to the AON clock domain.
 *
 * Port Interfaces:
 *   - tl_socket             : tlm_target_socket<32>  - inherited from aon_timer_base
 *   - intr_wkup_timer_expired: sc_out<bool>           - wakeup timer interrupt output
 *   - intr_wdog_timer_bark   : sc_out<bool>           - watchdog bark interrupt output
 *   - nmi_wdog_timer_bark    : sc_out<bool>           - watchdog NMI output (copy of bark)
 *   - wkup_req               : sc_out<bool>           - wakeup request to power manager
 *   - aon_timer_rst_req      : sc_out<bool>           - reset request to power manager
 *   - sleep_mode             : sc_in<bool>            - sleep mode indication from power manager
 *   - lc_escalate_en         : sc_in<bool>            - lifecycle escalation enable input
 *   - fatal_fault            : sc_out<bool>           - fatal TL-UL integrity alert output
 *   - clk_aon_freq           : sc_in<double>          - AON clock frequency in Hz (abstract)
 *   - clk_sys_freq           : sc_in<double>          - SYS clock frequency in Hz (abstract)
 *   - rst_n                  : sc_in<bool>            - active-low SYS domain reset
 *   - rst_aon_n              : sc_in<bool>            - active-low AON domain reset
 *   - racl_policies          : sc_in<uint32_t>        - RACL policy vector (EnableRacl=1 only)
 *   - racl_error             : sc_out<bool>           - RACL violation flag (EnableRacl=1 only)
 *
 * FUNC001 - Register Interface and Bus Transport:
 *   All 14 registers are covered by functional write and read callbacks registered
 *   in the aon_timer_ip constructor. Callbacks implement W1C (INTR_STATE), RW0C
 *   (WKUP_CAUSE, WDOG_REGWEN), WO read-as-zero (ALERT_TEST, INTR_TEST), reserved
 *   bit masking, and WDOG_REGWEN lock gating. CDC abstraction is implemented by
 *   advancing the quantum keeper on writes and synchronizing on reads.
 *
 * FUNC003 - Wakeup Timer Engine Operation:
 *   Implements the 64-bit upcounting wakeup timer tick engine as an SC_THREAD
 *   (wkup_timer_tick_thread). When m_wkup_enabled=true and !m_lc_escalate_active,
 *   the thread sleeps for sc_time(1.0/clk_aon_freq * (prescaler+1), SC_SEC) then
 *   increments m_wkup_counter (with uint64 overflow wrap), and calls
 *   evaluate_wkup_threshold(). When disabled or escalation-active the thread waits
 *   on m_ev_wkup_tick. WKUP_CTRL writes reset the prescaler accumulator and notify
 *   the thread. lc_escalate_en transitions are handled by the lc_escalate_handler()
 *   SC_METHOD which updates m_lc_escalate_active and notifies m_ev_wkup_tick.
 *   Non-atomic 64-bit counter race is modeled in handle_read_WKUP_COUNT_HI by setting
 *   m_wkup_hi_read_pending=true; handle_read_WKUP_COUNT_LO advances the counter by
 *   one tick if the flag is set and the timer is enabled.
 *
 * FUNC002 - System Reset and Initialization:
 *   Implements dual-domain active-low reset behavior via the reset_process() SC_THREAD
 *   sensitive to rst_n and rst_aon_n. Two independent reset paths are distinguished:
 *
 *   rst_n (SYS-domain reset, active-low):
 *     Full power-on reset of ALL internal state: both 64-bit wakeup and 32-bit watchdog
 *     counters cleared to 0; both timers disabled; WDOG_REGWEN lock unconditionally
 *     cleared (m_wdog_regwen_locked = false, register shadow = 0x1); all interrupt state
 *     bits cleared; WKUP_CAUSE cleared; prescaler accumulator reset to 0; all output
 *     signals de-asserted via m_ev_output_update notification.
 *     reset_all_registers() is called to restore all 14 register shadows to hardware
 *     power-on defaults.
 *
 *   rst_aon_n (AON-domain reset, active-low, independent):
 *     Partial reset of AON-domain output state only: m_wkup_cause_active cleared,
 *     WKUP_CAUSE shadow cleared, m_wdog_bite_active cleared; outputs wkup_req and
 *     aon_timer_rst_req de-asserted via m_ev_output_update. SYS-domain state (counters,
 *     thresholds, interrupt flags, lock, prescaler) is NOT modified by rst_aon_n alone.
 *
 *   In normal system operation both resets are asserted and de-asserted together.
 *   Independent rst_aon_n behavior is modeled through separate ports to reflect the
 *   dual clock domain architecture.
 *
 *   The reset_process also notifies m_ev_counter_cancel when rst_n is asserted, allowing
 *   future counter tick SC_THREADs (FUNC003, FUNC004) to cancel pending increment events.
 */

#pragma once
#include "aon_timer_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <functional>

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

/**
 * @class aon_timer_ip
 * @brief AON Timer top-level TLM-2.0 LT model (derives from aon_timer_base).
 *
 * Subclass of aon_timer_base that declares all port interfaces required by the
 * AON Timer hardware specification. Named aon_timer_ip to avoid conflict with
 * the aon_timer namespace defined in aon_timer_register.h which encloses all
 * register type definitions. Instantiate this class in a SystemC simulation
 * environment, bind all sc_out/sc_in ports via sc_signal intermediaries in a
 * testbench, and bind the inherited target_socket to a TLM initiator to provide
 * software access to all 14 AON Timer registers.
 *
 * The EnableRacl constructor parameter controls presence of the RACL port pair:
 *   - EnableRacl = false (default): racl_policies and racl_error ports are present
 *     but must still be bound to sc_signal instances in the testbench.
 *   - EnableRacl = true: RACL enforcement logic is active and racl_error may be
 *     asserted on access violation.
 *
 * Temporal decoupling is implemented via tlm_utils::tlm_quantumkeeper (m_qk).
 * No cycle-accurate timing is modeled; counter increments and CDC delays use
 * nominal quantum-keeper-managed delays.
 *
 * FUNC001 Implementation Notes:
 *   All 14 register write callbacks and 12 register read callbacks (excluding
 *   ALERT_TEST and INTR_TEST which are write-only) are registered in the
 *   constructor after base-class initialization. Write callbacks implement the
 *   full hardware semantics for each register including W1C, RW0C, lock gating,
 *   and counter resets. Read callbacks return live internal state (counters,
 *   interrupt flags, wakeup cause) rather than stale register shadows.
 *
 * FUNC002 Implementation Notes:
 *   The reset_process() SC_THREAD distinguishes between rst_n (full SYS-domain
 *   reset) and rst_aon_n-only (AON-domain partial reset). A dedicated
 *   m_wdog_bite_active boolean flag is introduced to decouple the aon_timer_rst_req
 *   output from the inline counter/threshold evaluation, enabling the AON-domain
 *   reset to de-assert aon_timer_rst_req independently of the SYS-domain counter
 *   state. The m_ev_counter_cancel sc_event is provided for FUNC003/FUNC004 to
 *   cancel running counter tick events when rst_n is asserted.
 *
 * FUNC003 Implementation Notes:
 *   The wkup_timer_tick_thread() SC_THREAD implements the autonomous counter tick
 *   engine. It uses the current WKUP_CTRL.prescaler field value to compute the
 *   next tick delay: sc_time(1.0/clk_aon_freq * (prescaler+1), SC_SEC). The thread
 *   awaits m_ev_wkup_tick when disabled or halted by escalation. It monitors
 *   m_ev_counter_cancel (from rst_n assertion) to restart cleanly. The
 *   lc_escalate_handler() SC_METHOD is sensitive to lc_escalate_en and notifies
 *   m_ev_wkup_tick to wake the thread after state change. Non-atomic race modeling
 *   is provided by m_wkup_hi_read_pending flag in count read callbacks.
 *
 * FUNC004 - Watchdog Timer Engine Operation:
 *   Implements the 32-bit upcounting watchdog timer tick engine as an SC_THREAD
 *   (wdog_timer_tick_thread). When m_wdog_enabled=true, !m_lc_escalate_active, and
 *   NOT paused (!(m_wdog_pause_in_sleep && sleep_mode.read())), the thread sleeps for
 *   sc_time(1.0/clk_aon_freq, SC_SEC) then increments m_wdog_counter (with uint32
 *   overflow wrap, 0xFFFFFFFF -> 0x00000000), calls evaluate_bark_threshold() and
 *   evaluate_bite_threshold(). When disabled, paused, or escalation-active the
 *   thread waits on m_ev_wdog_tick. The wdog_sleep_mode_handler() SC_METHOD is
 *   sensitive to sleep_mode and notifies m_ev_wdog_tick when sleep_mode changes,
 *   allowing the watchdog thread to re-evaluate the pause condition. The
 *   lc_escalate_handler() is extended to also notify m_ev_wdog_tick on escalation
 *   state changes. handle_write_WDOG_CTRL and handle_write_WDOG_COUNT notify
 *   m_ev_wdog_tick to wake the thread after state-changing register writes.
 *   Pause condition: counter halts when m_wdog_pause_in_sleep=true AND
 *   sleep_mode.read()=true; resumes from frozen value when either is removed.
 */
class aon_timer_ip : public aon_timer_base
{
public:
   SC_HAS_PROCESS(aon_timer_ip);

   // =========================================================================
   // Interrupt Output Ports (SYS Domain)
   // =========================================================================

   /// @brief Level-sensitive wakeup timer expiry interrupt output.
   ///        Asserted when wakeup counter >= WKUP_THOLD and WKUP_CTRL.enable=1.
   ///        De-asserted when software writes 1 to INTR_STATE.wkup_timer_expired.
   ///        Bound to the test class sc_signal<bool> intr_wkup_timer_expired_sig.
   sc_out<bool> intr_wkup_timer_expired;

   /// @brief Level-sensitive watchdog bark interrupt output.
   ///        Asserted when watchdog counter >= WDOG_BARK_THOLD and WDOG_CTRL.enable=1.
   ///        De-asserted when software writes 1 to INTR_STATE.wdog_timer_bark.
   ///        Bound to the test class sc_signal<bool> intr_wdog_timer_bark_sig.
   sc_out<bool> intr_wdog_timer_bark;

   /// @brief Non-maskable interrupt output; logical copy of intr_wdog_timer_bark.
   ///        Driven identically and simultaneously with intr_wdog_timer_bark.
   ///        Provides an NMI path independent of the normal interrupt routing.
   ///        Bound to the test class sc_signal<bool> nmi_wdog_timer_bark_sig.
   sc_out<bool> nmi_wdog_timer_bark;

   // =========================================================================
   // Power Management Output Ports (AON Domain)
   // =========================================================================

   /// @brief Level wakeup request output to the power manager (AON domain).
   ///        Asserted when wakeup timer threshold or watchdog bark threshold is crossed.
   ///        Remains asserted until reset or software writes 0 to WKUP_CAUSE.
   ///        Bound to the test class sc_signal<bool> wkup_req_sig.
   sc_out<bool> wkup_req;

   /// @brief Reset request output to the power manager (AON domain).
   ///        Asserted when watchdog counter >= WDOG_BITE_THOLD and WDOG_CTRL.enable=1.
   ///        Triggers a system reset independent of the bark interrupt path.
   ///        Bound to the test class sc_signal<bool> aon_timer_rst_req_sig.
   sc_out<bool> aon_timer_rst_req;

   // =========================================================================
   // Power Management Input Port (SYS Domain)
   // =========================================================================

   /// @brief Sleep mode indication input from the power manager.
   ///        When asserted and WDOG_CTRL.pause_in_sleep=1, the watchdog counter halts.
   ///        The wakeup timer is unaffected by this signal (always-on).
   ///        Corresponds to hardware pin sleep_mode_i.
   ///        Bound to the test class sc_signal<bool> sleep_mode_sig.
   sc_in<bool> sleep_mode;

   // =========================================================================
   // Lifecycle and Security Input Port
   // =========================================================================

   /// @brief Lifecycle escalation enable input from the lifecycle controller.
   ///        RTL type is lc_ctrl_pkg::lc_tx; modeled as sc_in<bool> at TLM abstraction.
   ///        When asserted, both the wakeup timer and watchdog timer halt counting.
   ///        Corresponds to hardware pin lc_escalate_en_i.
   ///        Bound to the test class sc_signal<bool> lc_escalate_en_sig.
   sc_in<bool> lc_escalate_en;

   // =========================================================================
   // Security Alert Output Port
   // =========================================================================

   /// @brief Fatal alert output for TL-UL bus integrity violations.
   ///        Asserted when a fatal TL-UL bus integrity fault is detected.
   ///        Software can trigger a test alert via ALERT_TEST.fatal_fault.
   ///        Bound to the test class sc_signal<bool> fatal_fault_sig.
   sc_out<bool> fatal_fault;

   // =========================================================================
   // Clock Input Ports (Abstract Timing)
   // =========================================================================

   /// @brief AON clock frequency input in Hz (abstract, not a pin-level clock).
   ///        Models functional timing for counter increment events (~200 kHz typical).
   ///        Counter increment rate = clk_aon_freq / (prescaler + 1) for wakeup timer;
   ///        directly at clk_aon_freq for watchdog timer.
   ///        Replaces hardware pin clk_aon_i.
   ///        Bound to the test class sc_signal<double> clk_aon_freq_sig.
   sc_in<double> clk_aon_freq;

   /// @brief System clock frequency input in Hz (abstract, not a pin-level clock).
   ///        Models TL-UL register interface timing domain (SYS domain).
   ///        CDC effects from SYS to AON domain are modeled as quantum-keeper delays.
   ///        Replaces hardware pin clk_i.
   ///        Bound to the test class sc_signal<double> clk_sys_freq_sig.
   sc_in<double> clk_sys_freq;

   // =========================================================================
   // Reset Input Ports
   // =========================================================================

   /// @brief Active-low system reset input (SYS domain).
   ///        Resets all timer state, clears counters, de-asserts all output signals,
   ///        and restores WDOG_REGWEN to its unlocked default (0x1).
   ///        Bound to the test class sc_signal<bool> rst_n_sig.
   sc_in<bool> rst_n;

   /// @brief Active-low AON domain reset input.
   ///        Resets AON-domain state and output registers including wkup_req and
   ///        aon_timer_rst_req. In normal operation both resets are asserted and
   ///        de-asserted together; modeled as a separate port to reflect the dual
   ///        clock domain architecture.
   ///        Bound to the test class sc_signal<bool> rst_aon_n_sig.
   sc_in<bool> rst_aon_n;

   // =========================================================================
   // Optional RACL Security Ports (Conditional on EnableRacl parameter)
   // =========================================================================

   /// @brief RACL policy vector input (simplified type for TLM abstraction).
   ///        RTL type is top_racl_pkg::racl_policy_vec; modeled as sc_in<uint32_t>.
   ///        Present in all instances; RACL enforcement is active only when
   ///        EnableRacl=true. Corresponds to hardware port racl_policies_i.
   ///        Bound to the test class sc_signal<uint32_t> racl_policies_sig.
   sc_in<uint32_t> racl_policies;

   /// @brief RACL error output flag (simplified type for TLM abstraction).
   ///        RTL type is top_racl_pkg::racl_error_log; modeled as sc_out<bool>.
   ///        Asserted when an unauthorized register access is detected.
   ///        Present in all instances; active only when EnableRacl=true.
   ///        Corresponds to hardware port racl_error_o.
   ///        Bound to the test class sc_signal<bool> racl_error_sig.
   sc_out<bool> racl_error;

   // =========================================================================
   // Configuration Parameter
   // =========================================================================

   /// @brief Build-time RACL enable parameter (false = disabled, true = enabled).
   ///        When true, racl_policies input is evaluated per-access and racl_error
   ///        may be asserted on policy violation. Does not affect port presence.
   const bool EnableRacl;

   // =========================================================================
   // FUNC001 Fix: SC_METHOD output driver event and process
   // =========================================================================

   /// @brief Shared event used to schedule deferred output port updates.
   ///        Notified with SC_ZERO_TIME by any callback that modifies internal
   ///        state that maps to an sc_out<bool> port. The drive_outputs() SC_METHOD
   ///        is triggered by this event and performs the actual port writes from a
   ///        safe SystemC process context, avoiding sc_out::write() inside b_transport.
   sc_event m_ev_output_update;

   /// @brief Counter tick cancellation event (FUNC002 reset readiness / FUNC003+FUNC004 hook).
   ///        Notified by reset_process() when rst_n is asserted (active-low) to signal
   ///        any running counter increment SC_THREAD/SC_EVENT (wakeup tick, watchdog tick)
   ///        that they must cancel their pending scheduled increments and halt counting.
   ///        Counter tick processes implemented in FUNC003 (wakeup timer) and FUNC004
   ///        (watchdog timer) must be sensitive to this event and cancel on receipt.
   ///        Notified with SC_ZERO_TIME to ensure cancellation occurs in the immediately
   ///        following delta cycle, before the counter processes could fire.
   sc_event m_ev_counter_cancel;

   // =========================================================================
   // FUNC003: Wakeup Timer Tick Engine Events
   // =========================================================================

   /// @brief Wakeup timer tick thread wake-up event (FUNC003).
   ///
   ///        Notified (with SC_ZERO_TIME) by:
   ///          - handle_write_WKUP_CTRL: whenever the enable bit or prescaler changes,
   ///            or on any WKUP_CTRL write (prescaler accumulator reset side-effect).
   ///            Wakes the thread so it can re-evaluate timer state immediately.
   ///          - lc_escalate_handler() SC_METHOD: on lc_escalate_en assertion or
   ///            de-assertion, to freeze or resume the counter tick loop.
   ///          - reset_process(): implicitly via m_ev_counter_cancel; wkup_timer_tick_thread
   ///            monitors both events and restarts cleanly on reset.
   ///
   ///        The wkup_timer_tick_thread SC_THREAD waits on this event when the timer is
   ///        disabled (m_wkup_enabled=false) or when lifecycle escalation is active
   ///        (m_lc_escalate_active=true). When the timer is enabled and escalation is
   ///        inactive, the thread uses a timed wait for the next prescaler-controlled
   ///        increment interval instead.
   sc_event m_ev_wkup_tick;

   // =========================================================================
   // FUNC004: Watchdog Timer Tick Engine Events
   // =========================================================================

   /// @brief Watchdog timer tick thread wake-up event (FUNC004).
   ///
   ///        Notified (with SC_ZERO_TIME) by:
   ///          - handle_write_WDOG_CTRL: on any WDOG_CTRL write (enable/disable change,
   ///            pause_in_sleep change). Wakes the thread to re-evaluate timer state.
   ///          - handle_write_WDOG_COUNT: on watchdog pet (any write resets counter to 0).
   ///            Wakes the thread so it re-evaluates from the reset counter state.
   ///          - wdog_sleep_mode_handler() SC_METHOD: on any sleep_mode transition,
   ///            to freeze (if pause_in_sleep=1 and sleep_mode asserts) or resume
   ///            (when sleep_mode de-asserts or pause_in_sleep is cleared).
   ///          - lc_escalate_handler() SC_METHOD: on lc_escalate_en assertion or
   ///            de-assertion, to freeze or resume the watchdog counter tick loop.
   ///          - reset_process(): via m_ev_counter_cancel notification path; watchdog
   ///            thread monitors both events and restarts cleanly on reset.
   ///
   ///        The wdog_timer_tick_thread SC_THREAD waits on this event when:
   ///          - m_wdog_enabled is false (watchdog disabled), OR
   ///          - m_lc_escalate_active is true (escalation halt), OR
   ///          - m_wdog_pause_in_sleep is true AND sleep_mode.read() is true (pause).
   ///
   ///        When enabled, not escalated, and not paused, the thread uses a timed
   ///        wait of exactly one AON clock period (1.0/clk_aon_freq seconds).
   ///        Architecture Map: timing_constraints[counter-increment-rate] - no prescaler.
   sc_event m_ev_wdog_tick;

   // =========================================================================
   // Constructor
   // =========================================================================

   /**
    * @brief Construct the aon_timer_ip module with all port interfaces.
    * @param n          SystemC hierarchical module name.
    * @param memory_size Size in bytes of the memory region covering all registers
    *                    (default 0x40 covers offsets 0x00 through 0x34 with 4-byte alignment).
    * @param enable_racl Build-time RACL enable flag (default false = disabled).
    *
    * Calls aon_timer_base constructor to initialize the register infrastructure,
    * then initializes all sc_out/sc_in ports by name. All output ports are driven
    * to their de-asserted reset states (false / 0) during initialization. The
    * quantum keeper is initialized for temporal decoupling.
    *
    * FUNC001: After base-class construction, registers functional write and read
    * callbacks for all 14 registers. These callbacks override the default CSML
    * register-level callbacks and implement the full hardware access semantics
    * including W1C, RW0C, WO, WDOG_REGWEN lock gating, reserved bit masking,
    * and CDC quantum-keeper annotation.
    *
    * The constructor registers a reset-and-port-init SC_THREAD sensitive to
    * rst_n and rst_aon_n for proper reset behavior during simulation.
    *
    * FUNC002: m_wdog_bite_active is initialized to false (de-asserted) reflecting
    * the hardware power-on state where no bite condition exists. The
    * m_ev_counter_cancel event is provided for FUNC003/FUNC004 counter threads
    * to monitor for reset cancellation. EnableRacl parameter has no effect on
    * the reset domain architecture.
    */
   aon_timer_ip(sc_module_name n,
                unsigned int memory_size = 0x40,
                bool enable_racl = false)
      : aon_timer_base(n, memory_size),
        intr_wkup_timer_expired("intr_wkup_timer_expired"),
        intr_wdog_timer_bark("intr_wdog_timer_bark"),
        nmi_wdog_timer_bark("nmi_wdog_timer_bark"),
        wkup_req("wkup_req"),
        aon_timer_rst_req("aon_timer_rst_req"),
        sleep_mode("sleep_mode"),
        lc_escalate_en("lc_escalate_en"),
        fatal_fault("fatal_fault"),
        clk_aon_freq("clk_aon_freq"),
        clk_sys_freq("clk_sys_freq"),
        rst_n("rst_n"),
        rst_aon_n("rst_aon_n"),
        racl_policies("racl_policies"),
        racl_error("racl_error"),
        EnableRacl(enable_racl),
        verbosity("verbosity", CSML_DEFAULT_VERBOSITY),
        /* Internal state variables - all reset to hardware power-on defaults */
        m_wkup_counter(0ULL),
        m_wkup_threshold(0ULL),
        m_wkup_prescaler_count(0),
        m_wkup_enabled(false),
        m_wdog_counter(0U),
        m_wdog_bark_threshold(0U),
        m_wdog_bite_threshold(0U),
        m_wdog_enabled(false),
        m_wdog_pause_in_sleep(false),
        m_wdog_regwen_locked(false),
        m_intr_state_wkup(false),
        m_intr_state_bark(false),
        m_wdog_bark_latched(false),
        m_wkup_cause_active(false),
        m_wkup_threshold_latched(false),
        m_wdog_bite_active(false),
        m_lc_escalate_active(false),
        m_sleep_mode_active(false),
        m_fatal_fault_pending(false),
        m_racl_error_active(false),
        m_wkup_hi_read_pending(false)
   {
      logger.setMaxVerbosity(verbosity.get_param_value());
      SC_THREAD(reset_process);
      sensitive << rst_n << rst_aon_n;

      /* -----------------------------------------------------------------------
       * FUNC001 Fix: Register the deferred output driver SC_METHOD.
       * drive_outputs() is triggered by m_ev_output_update whenever a callback
       * modifies an internal flag that maps to an sc_out<bool> port.
       * dont_initialize() prevents a spurious firing at time zero.
       * --------------------------------------------------------------------- */
      SC_METHOD(drive_outputs);
      sensitive << m_ev_output_update;
      dont_initialize();

      /* -----------------------------------------------------------------------
       * FUNC003: Register the wakeup timer tick engine SC_THREAD.
       * wkup_timer_tick_thread() runs the autonomous prescaler-controlled counter
       * increment loop. dont_initialize() is not applicable for SC_THREAD.
       * The thread blocks immediately on m_ev_wkup_tick because the timer starts
       * disabled at power-on (m_wkup_enabled=false at construction time).
       * --------------------------------------------------------------------- */
      SC_THREAD(wkup_timer_tick_thread);

      /* -----------------------------------------------------------------------
       * FUNC003: Register the lifecycle escalation handler SC_METHOD.
       * lc_escalate_handler() is sensitive to the lc_escalate_en input port.
       * It updates m_lc_escalate_active and notifies m_ev_wkup_tick (and
       * m_ev_wdog_tick, added in FUNC004) to freeze or resume both timer tick
       * threads immediately.
       * dont_initialize() prevents spurious firing at time zero.
       * --------------------------------------------------------------------- */
      SC_METHOD(lc_escalate_handler);
      sensitive << lc_escalate_en;
      dont_initialize();

      /* -----------------------------------------------------------------------
       * FUNC004: Register the watchdog timer tick engine SC_THREAD.
       * wdog_timer_tick_thread() runs the autonomous 32-bit no-prescaler counter
       * increment loop at the direct AON clock rate (1 count per AON tick).
       * The thread blocks immediately on m_ev_wdog_tick because the watchdog
       * starts disabled at power-on (m_wdog_enabled=false at construction time).
       * Architecture Map: state_machines[WDOG_TIMER].transitions.
       * --------------------------------------------------------------------- */
      SC_THREAD(wdog_timer_tick_thread);

      /* -----------------------------------------------------------------------
       * FUNC004: Register the sleep-mode change handler SC_METHOD.
       * wdog_sleep_mode_handler() is sensitive to the sleep_mode input port.
       * It updates m_sleep_mode_active and notifies m_ev_wdog_tick to freeze
       * or resume the watchdog tick thread based on pause_in_sleep state.
       * dont_initialize() prevents spurious firing at time zero.
       * Architecture Map: state_machines[WDOG_TIMER].transitions[COUNTING->PAUSED].
       * --------------------------------------------------------------------- */
      SC_METHOD(wdog_sleep_mode_handler);
      sensitive << sleep_mode;
      dont_initialize();

      /* -----------------------------------------------------------------------
       * FUNC001: Register all functional callbacks for the 14 AON Timer
       * registers. These are registered after base-class construction so they
       * override the default CSML register-level read/write callbacks.
       * Offsets are word-addressed (byte_offset / sizeof(uint32_t)).
       * --------------------------------------------------------------------- */
      register_all_callbacks();

      CSML_INFO(1, logger) << name() << ": aon_timer_ip constructed"
                           << " (EnableRacl=" << enable_racl << ")";
   }

   /// @brief CCI-backed verbosity parameter (runtime-overridable via ini file).
   csml_param<int> verbosity;

private:
   // =========================================================================
   // Internal Hardware State Variables (FUNC001)
   // =========================================================================

   /**
    * @brief Live 64-bit wakeup counter value.
    *
    * Incremented by counter tick events when wakeup timer is enabled and not
    * halted by lifecycle escalation. Compared against m_wkup_threshold on
    * every increment and on relevant register writes.
    * Reset to 0 on system reset. Directly exposed via read callbacks for
    * WKUP_COUNT_HI and WKUP_COUNT_LO.
    */
   uint64_t m_wkup_counter;

   /**
    * @brief Assembled 64-bit wakeup timer threshold.
    *
    * Composed as: (WKUP_THOLD_HI << 32) | WKUP_THOLD_LO.
    * Updated by handle_write_WKUP_THOLD_HI and handle_write_WKUP_THOLD_LO.
    * Reset to 0 on system reset.
    */
   uint64_t m_wkup_threshold;

   /**
    * @brief 12-bit prescaler accumulator for the wakeup timer.
    *
    * Counts AON clock ticks; resets to 0 on every write to WKUP_CTRL
    * (mandatory side-effect, even for writes that do not change the value).
    * Not directly software-visible; drives the counter increment period.
    */
   uint32_t m_wkup_prescaler_count;

   /**
    * @brief Wakeup timer enable state (mirrors WKUP_CTRL.enable bit[0]).
    *
    * When true, counter increment events are scheduled. When false, events
    * are cancelled and the counter halts at its current value.
    */
   bool m_wkup_enabled;

   /**
    * @brief Live 32-bit watchdog counter value.
    *
    * Incremented by counter tick events when watchdog is enabled and not
    * paused or halted. Any write to WDOG_COUNT resets this to 0 (watchdog
    * pet). Directly exposed via handle_read_WDOG_COUNT.
    * Reset to 0 on system reset.
    */
   uint32_t m_wdog_counter;

   /**
    * @brief Watchdog bark threshold (mirrors WDOG_BARK_THOLD.threshold[31:0]).
    *
    * Bark fires when m_wdog_counter >= m_wdog_bark_threshold and watchdog
    * is enabled. Gated by WDOG_REGWEN lock.
    */
   uint32_t m_wdog_bark_threshold;

   /**
    * @brief Watchdog bite threshold (mirrors WDOG_BITE_THOLD.threshold[31:0]).
    *
    * Bite fires when m_wdog_counter >= m_wdog_bite_threshold and watchdog
    * is enabled. Gated by WDOG_REGWEN lock.
    */
   uint32_t m_wdog_bite_threshold;

   /**
    * @brief Watchdog timer enable state (mirrors WDOG_CTRL.enable bit[0]).
    *
    * When true, counter increment events are scheduled. When false, events
    * are cancelled and the counter halts.
    */
   bool m_wdog_enabled;

   /**
    * @brief Watchdog pause-in-sleep state (mirrors WDOG_CTRL.pause_in_sleep bit[1]).
    *
    * When true, the watchdog counter halts while m_sleep_mode_active is true.
    */
   bool m_wdog_pause_in_sleep;

   /**
    * @brief Watchdog configuration lock state (mirrors WDOG_REGWEN.regwen bit[0]).
    *
    * When false: WDOG_REGWEN.regwen = 1 (unlocked, reset default).
    * When true:  WDOG_REGWEN.regwen = 0 (locked permanently until system reset).
    * Writes to WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD are silently
    * discarded when this is true.
    */
   bool m_wdog_regwen_locked;

   /**
    * @brief Live wakeup timer interrupt pending flag.
    *
    * Mirrors INTR_STATE.wkup_timer_expired bit[0] and the live state of the
    * intr_wkup_timer_expired output port. Set by threshold comparisons and
    * cleared by W1C writes to INTR_STATE.
    */
   bool m_intr_state_wkup;

   /**
    * @brief Live watchdog bark interrupt pending flag.
    *
    * Mirrors INTR_STATE.wdog_timer_bark bit[1] and the live state of the
    * intr_wdog_timer_bark and nmi_wdog_timer_bark output ports. Set by bark
    * threshold comparisons and cleared by W1C writes to INTR_STATE.
    */
   bool m_intr_state_bark;

   /**
    * @brief Watchdog bark threshold crossing latch.
    *
    * Edge memory for the bark condition, mirroring the RTL's prim_edge_detector:
    * aon_timer.sv feeds prim_intr_hw from q_posedge_pulse_o, so INTR_STATE is set
    * by the *rising edge* of (enabled && count >= bark_thold), never by its level.
    * Without a separate latch, a W1C that clears m_intr_state_bark also re-arms
    * the detector, and the very next tick re-asserts the bit because the counter
    * is still above the threshold -- software can then never clear the interrupt
    * from its own handler. The latch clears when the condition goes false (a pet,
    * a disable, or a raised threshold), which re-arms the edge for the next
    * crossing.
    *
    * Reset to false by both rst_n and rst_aon_n.
    */
   bool m_wdog_bark_latched;

   /**
    * @brief Live wakeup request active flag.
    *
    * Mirrors WKUP_CAUSE.cause bit[0] and the live state of the wkup_req output
    * port. Set when wakeup or bark threshold is crossed. Cleared by software
    * writing 0 to WKUP_CAUSE (RW0C).
    * Reset to false by both rst_n (full system reset) and rst_aon_n (AON-domain
    * reset) per FUNC002 dual-domain reset architecture.
    */
   bool m_wkup_cause_active;

   /**
    * @brief Wakeup threshold crossing latch.
    *
    * Set to true when the wakeup counter first meets or exceeds m_wkup_threshold
    * while enabled. Prevents evaluate_wkup_threshold() from re-asserting
    * WKUP_CAUSE after software has cleared it with a write-0 while the counter
    * is still above the threshold. The latch is cleared only when the counter
    * drops back below the threshold, re-arming the edge-triggered fire for the
    * next crossing event.
    *
    * Reset to false by both rst_n and rst_aon_n.
    */
   bool m_wkup_threshold_latched;

   /**
    * @brief Watchdog bite condition active flag (FUNC002).
    *
    * Set to true by evaluate_bite_threshold() when:
    *   m_wdog_enabled == true AND m_wdog_counter >= m_wdog_bite_threshold.
    * Cleared to false when the condition is no longer met (e.g., after watchdog
    * petting or watchdog disable) or when a reset clears it.
    *
    * This dedicated flag decouples the aon_timer_rst_req output drive from an
    * inline counter/threshold evaluation inside drive_outputs(), enabling the
    * AON-domain reset (rst_aon_n) to de-assert aon_timer_rst_req independently
    * without modifying SYS-domain counter or threshold state.
    *
    * Mapped exclusively to the aon_timer_rst_req output port by drive_outputs()
    * SC_METHOD (single-writer rule compliance).
    *
    * Reset to false by both rst_n (full system reset) and rst_aon_n (AON-domain
    * reset) per FUNC002 dual-domain reset architecture.
    * Architecture Map Reference: events[wdog_timer_bite].propagation
    */
   bool m_wdog_bite_active;

   /**
    * @brief Lifecycle escalation active state.
    *
    * Set when lc_escalate_en input is asserted. Halts both timer counters.
    * Monitored by counter increment event handlers (implemented in FUNC008).
    */
   bool m_lc_escalate_active;

   /**
    * @brief Sleep mode active state.
    *
    * Set when sleep_mode input is asserted. If m_wdog_pause_in_sleep is also
    * true, the watchdog counter halts. Monitored by watchdog increment handlers
    * (implemented in FUNC004).
    */
   bool m_sleep_mode_active;

   /**
    * @brief Fatal fault output pending flag (FUNC001 Fix).
    *
    * Set to true when ALERT_TEST.fatal_fault bit[0] is written as 1, requesting
    * assertion of the fatal_fault output port. Cleared to false by drive_outputs()
    * after the port write is performed (transient test pulse semantics).
    * This flag allows the deferred drive_outputs() SC_METHOD to propagate the
    * alert test assertion without calling sc_out::write() inside b_transport.
    */
   bool m_fatal_fault_pending;

   /**
    * @brief RACL error output active flag (FUNC001 Fix).
    *
    * Set to true when a RACL policy violation is detected (EnableRacl=true path).
    * Cleared to false on system reset. Mapped to the racl_error output port
    * exclusively by drive_outputs() SC_METHOD to preserve single-writer rule.
    * In FUNC001 scope this flag is always false; RACL enforcement is added in
    * a later functional increment.
    */
   bool m_racl_error_active;

   /**
    * @brief Non-atomic 64-bit counter HI-read pending flag (FUNC003).
    *
    * Set to true by handle_read_WKUP_COUNT_HI() when it returns the upper 32
    * bits of the live wakeup counter. handle_read_WKUP_COUNT_LO() checks this
    * flag: if true AND the timer is enabled AND escalation is inactive, the
    * counter is advanced by one tick before returning the LO value. This models
    * the hardware race condition where the counter may increment between the HI
    * and LO register reads of a non-atomic 64-bit read sequence.
    *
    * Cleared by handle_read_WKUP_COUNT_LO() after the conditional advance, and
    * also cleared on system reset (FUNC002).
    *
    * Architecture Map Reference: timing_constraints[non-atomic-64-bit-access].
    */
   bool m_wkup_hi_read_pending;

   // =========================================================================
   // Internal State: Temporal Decoupling
   // =========================================================================

   /**
    * @brief TLM-2.0 quantum keeper for temporal decoupling.
    *
    * Manages local time accumulation and synchronization with the SystemC kernel.
    * Used to implement non-cycle-accurate timing for counter increment events,
    * CDC crossing delays, and interrupt/reset assertion timing.
    * Temporal decoupling allows the model to accumulate local time and synchronize
    * with the kernel only when the quantum is exceeded, improving simulation performance.
    *
    * FUNC001 CDC Usage: On register writes, the quantum keeper is advanced by
    * a representative CDC synchronizer delay (2 AON clock cycles) to model the
    * SYS-to-AON domain propagation latency. Read-back transactions synchronize
    * via m_qk.sync() to ensure prior writes have fully propagated.
    */
   tlm_utils::tlm_quantumkeeper m_qk;

   // =========================================================================
   // Logger
   // =========================================================================

   /// @brief CSML logger instance for structured logging (INFO, ERROR, DEBUG).
   mutable CsmlLogger logger;

   // =========================================================================
   // FUNC001: Callback Registration
   // =========================================================================

   /**
    * @brief Register all functional write and read callbacks for the 14 AON Timer
    *        registers. Called once from the constructor after base-class initialization.
    *
    * Offsets passed to memory.register_write_callback / register_read_callback are
    * word-addressed (byte_offset / 4) as required by the csml_memory API.
    *
    * Write callbacks registered: ALERT_TEST, WKUP_CTRL, WKUP_THOLD_HI,
    *   WKUP_THOLD_LO, WKUP_COUNT_HI, WKUP_COUNT_LO, WDOG_REGWEN, WDOG_CTRL,
    *   WDOG_BARK_THOLD, WDOG_BITE_THOLD, WDOG_COUNT, INTR_STATE, INTR_TEST,
    *   WKUP_CAUSE.
    *
    * Read callbacks registered (12, excludes WO registers ALERT_TEST and INTR_TEST):
    *   WKUP_CTRL, WKUP_THOLD_HI, WKUP_THOLD_LO, WKUP_COUNT_HI, WKUP_COUNT_LO,
    *   WDOG_REGWEN, WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD, WDOG_COUNT,
    *   INTR_STATE, WKUP_CAUSE.
    */
   void register_all_callbacks();

   // =========================================================================
   // FUNC001: Write Callback Methods
   // =========================================================================

   /**
    * @brief Write callback for ALERT_TEST register (offset 0x00, WO).
    * @param value     32-bit value written by software.
    * @param write_mask Write mask from the register descriptor (not used for WO).
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[ALERT_TEST].fields[fatal_fault].write_effects
    *
    * Side effects:
    *   - Bit[0] = 1: asserts fatal_fault output immediately (transient test pulse).
    *   - Bits[31:1]: reserved; silently ignored.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    *
    * ALERT_TEST has no storage; reads always return 0x0 via the WO restriction
    * callback already registered by the CSML register framework.
    */
   bool handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_CTRL register (offset 0x04, RW).
    * @param value     32-bit value written by software.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_CTRL].fields[enable,prescaler].write_effects
    *
    * Side effects:
    *   - Unconditionally resets m_wkup_prescaler_count to 0 (mandatory on every write).
    *   - Updates m_wkup_enabled from bit[0]; m_wkup_prescaler from bits[12:1].
    *   - Updates WKUP_CTRL register shadow (bits[12:0] only; reserved bits[31:13] cleared).
    *   - If enable 0->1: schedules wakeup counter increment event.
    *   - If enable 1->0: cancels pending counter increment events.
    *   - Triggers immediate wakeup threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_CTRL(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_THOLD_HI register (offset 0x08, RW).
    * @param value     32-bit value written as upper threshold bits[63:32].
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_THOLD_HI].fields[threshold_hi].write_effects
    *
    * Side effects:
    *   - Stores written value as upper 32 bits of m_wkup_threshold.
    *   - Triggers immediate wakeup threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_THOLD_HI(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_THOLD_LO register (offset 0x0C, RW).
    * @param value     32-bit value written as lower threshold bits[31:0].
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_THOLD_LO].fields[threshold_lo].write_effects
    *
    * Side effects:
    *   - Stores written value as lower 32 bits of m_wkup_threshold.
    *   - Triggers immediate wakeup threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_THOLD_LO(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_COUNT_HI register (offset 0x10, RW).
    * @param value     32-bit value to write as upper counter bits[63:32].
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_COUNT_HI].fields[count_hi].write_effects
    *
    * Side effects:
    *   - Stores written value as upper 32 bits of m_wkup_counter.
    *   - Updates WKUP_COUNT_HI register shadow.
    *   - Triggers immediate wakeup threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_COUNT_HI(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_COUNT_LO register (offset 0x14, RW).
    * @param value     32-bit value to write as lower counter bits[31:0].
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_COUNT_LO].fields[count_lo].write_effects
    *
    * Side effects:
    *   - Stores written value as lower 32 bits of m_wkup_counter.
    *   - Updates WKUP_COUNT_LO register shadow.
    *   - Triggers immediate wakeup threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_COUNT_LO(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WDOG_REGWEN register (offset 0x18, RW0C).
    * @param value     32-bit value written by software.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WDOG_REGWEN].fields[regwen].write_effects
    *
    * RW0C semantics:
    *   - Bit[0] = 0 and not already locked: sets m_wdog_regwen_locked = true;
    *     clears WDOG_REGWEN shadow to 0x0. Lock is permanent until system reset.
    *   - Bit[0] = 0 and already locked: no effect.
    *   - Bit[0] = 1: no effect (writing 1 cannot unlock).
    *   - Bits[31:1]: reserved; silently ignored.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WDOG_REGWEN(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WDOG_CTRL register (offset 0x1C, RW, gated by WDOG_REGWEN).
    * @param value     32-bit value written by software.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WDOG_CTRL].fields[enable,pause_in_sleep].write_effects
    *
    * Lock gating: if m_wdog_regwen_locked is true, silently discard write; return true
    * (bus transaction completes normally, no error signaled to initiator).
    *
    * Side effects (when not locked):
    *   - Updates m_wdog_enabled from bit[0]; m_wdog_pause_in_sleep from bit[1].
    *   - Updates WDOG_CTRL register shadow (bits[1:0] only; reserved bits[31:2] cleared).
    *   - If enable 0->1: schedules watchdog counter increment event.
    *   - If enable 1->0: cancels pending counter increment events.
    *   - Triggers immediate bark and bite threshold comparisons.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WDOG_CTRL(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WDOG_BARK_THOLD register (offset 0x20, RW, gated by WDOG_REGWEN).
    * @param value     32-bit value written as bark threshold.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WDOG_BARK_THOLD].fields[threshold].write_effects
    *
    * Lock gating: if m_wdog_regwen_locked, silently discard write.
    *
    * Side effects (when not locked):
    *   - Stores value as m_wdog_bark_threshold.
    *   - Updates WDOG_BARK_THOLD register shadow.
    *   - Triggers immediate bark threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WDOG_BARK_THOLD(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WDOG_BITE_THOLD register (offset 0x24, RW, gated by WDOG_REGWEN).
    * @param value     32-bit value written as bite threshold.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WDOG_BITE_THOLD].fields[threshold].write_effects
    *
    * Lock gating: if m_wdog_regwen_locked, silently discard write.
    *
    * Side effects (when not locked):
    *   - Stores value as m_wdog_bite_threshold.
    *   - Updates WDOG_BITE_THOLD register shadow.
    *   - Triggers immediate bite threshold comparison.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WDOG_BITE_THOLD(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WDOG_COUNT register (offset 0x28, RW). Watchdog petting.
    * @param value     32-bit written value (completely ignored per spec).
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WDOG_COUNT].fields[count].write_effects
    * Detailed Design Section 7.5: any write resets counter to zero regardless of data.
    *
    * NOT gated by WDOG_REGWEN. Petting is always permitted.
    *
    * Side effects:
    *   - Discards written value entirely.
    *   - Forces m_wdog_counter to 0.
    *   - Updates WDOG_COUNT register shadow to 0.
    *   - Evaluates bark condition (counter is now 0, so bark is false if threshold > 0).
    *   - Evaluates bite condition (counter is now 0, so bite is false if threshold > 0).
    *   - Evaluates wkup_req: de-asserts if neither wakeup timer nor bark is active.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WDOG_COUNT(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for INTR_STATE register (offset 0x2C, RW1C).
    * @param value     32-bit value written by software (W1C: bit=1 clears that bit).
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[INTR_STATE].fields[wkup_timer_expired,wdog_timer_bark].write_effects
    * Detailed Design Section 7.6.
    *
    * W1C semantics:
    *   - Bit[0] = 1: clears m_intr_state_wkup; de-asserts intr_wkup_timer_expired.
    *   - Bit[1] = 1: clears m_intr_state_bark; de-asserts intr_wdog_timer_bark and nmi_wdog_timer_bark.
    *   - Bit[x] = 0: no change to that interrupt bit.
    *   - Bits[31:2]: reserved; silently ignored.
    *   - Updates INTR_STATE shadow register accordingly.
    *   - Re-assertion after clear is handled by the next counter increment event (not here).
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_INTR_STATE(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for INTR_TEST register (offset 0x30, WO). Force-assert interrupts.
    * @param value     32-bit value written by software (bit=1 force-asserts that interrupt).
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[INTR_TEST].fields[wkup_timer_expired,wdog_timer_bark].write_effects
    * Detailed Design Section 5.3.13.
    *
    * INTR_TEST has no storage; reads always return 0x0.
    *
    * Side effects:
    *   - Bit[0] = 1: sets m_intr_state_wkup = true; asserts intr_wkup_timer_expired; updates INTR_STATE shadow.
    *   - Bit[1] = 1: sets m_intr_state_bark = true; asserts intr_wdog_timer_bark and nmi_wdog_timer_bark; updates INTR_STATE shadow.
    *   - Counter and threshold values not modified.
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_INTR_TEST(uint32_t value, uint32_t write_mask);

   /**
    * @brief Write callback for WKUP_CAUSE register (offset 0x34, RW0C).
    * @param value     32-bit value written by software.
    * @param write_mask Write mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Architecture Map Reference: registers[WKUP_CAUSE].fields[cause].write_effects
    * Detailed Design Section 5.3.14.
    *
    * RW0C semantics:
    *   - Bit[0] = 0: clears m_wkup_cause_active = false; de-asserts wkup_req; clears WKUP_CAUSE shadow.
    *   - Bit[0] = 1: no effect (writing 1 cannot set the cause bit from software).
    *   - Bits[31:1]: reserved; silently ignored.
    *   - Does not affect INTR_STATE or interrupt output signals (independent path).
    *   - CDC annotation: m_qk advanced by 2 AON clock cycles.
    */
   bool handle_write_WKUP_CAUSE(uint32_t value, uint32_t write_mask);

   // =========================================================================
   // FUNC001: Read Callback Methods
   // =========================================================================

   /**
    * @brief Read callback for WKUP_CTRL register (offset 0x04, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current enable and prescaler field values from the register shadow.
    * Reserved bits[31:13] are masked to zero.
    * Serves as CDC read-back synchronization: m_qk.sync() is called to stall
    * until all prior writes have propagated.
    */
   bool handle_read_WKUP_CTRL(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WKUP_THOLD_HI register (offset 0x08, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns upper 32 bits of m_wkup_threshold. CDC read-back synchronization applied.
    */
   bool handle_read_WKUP_THOLD_HI(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WKUP_THOLD_LO register (offset 0x0C, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns lower 32 bits of m_wkup_threshold. CDC read-back synchronization applied.
    */
   bool handle_read_WKUP_THOLD_LO(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WKUP_COUNT_HI register (offset 0x10, RW, volatile).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns upper 32 bits of the live m_wkup_counter value (volatile).
    * Do not return stale shadow; the counter may have advanced since last write.
    * CDC read-back synchronization applied: m_qk.sync() stalls until prior writes
    * have propagated, satisfying the software synchronization guarantee.
    */
   bool handle_read_WKUP_COUNT_HI(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WKUP_COUNT_LO register (offset 0x14, RW, volatile).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns lower 32 bits of the live m_wkup_counter value (volatile).
    * CDC read-back synchronization applied.
    */
   bool handle_read_WKUP_COUNT_LO(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WDOG_REGWEN register (offset 0x18, RW0C).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns 0x1 (unlocked) or 0x0 (locked) based on m_wdog_regwen_locked.
    * Reserved bits[31:1] are always zero.
    * CDC read-back synchronization applied.
    */
   bool handle_read_WDOG_REGWEN(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WDOG_CTRL register (offset 0x1C, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current enable and pause_in_sleep field values.
    * Reserved bits[31:2] are always zero.
    * CDC read-back synchronization applied.
    */
   bool handle_read_WDOG_CTRL(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WDOG_BARK_THOLD register (offset 0x20, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current m_wdog_bark_threshold. CDC read-back synchronization applied.
    */
   bool handle_read_WDOG_BARK_THOLD(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WDOG_BITE_THOLD register (offset 0x24, RW).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current m_wdog_bite_threshold. CDC read-back synchronization applied.
    */
   bool handle_read_WDOG_BITE_THOLD(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WDOG_COUNT register (offset 0x28, RW, volatile).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns the live m_wdog_counter value (volatile; increments asynchronously).
    * CDC read-back synchronization applied.
    */
   bool handle_read_WDOG_COUNT(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for INTR_STATE register (offset 0x2C, RW1C, volatile).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current interrupt pending state: bit[0] = m_intr_state_wkup,
    * bit[1] = m_intr_state_bark. Volatile; can change on any AON clock tick.
    * Reserved bits[31:2] are always zero.
    * CDC read-back synchronization applied.
    */
   bool handle_read_INTR_STATE(uint32_t& value, uint32_t read_mask);

   /**
    * @brief Read callback for WKUP_CAUSE register (offset 0x34, RW0C, volatile).
    * @param value     Reference to 32-bit storage for the read value to return.
    * @param read_mask Read mask from the register descriptor.
    * @return true on successful callback execution.
    *
    * Returns current wakeup request state: bit[0] = m_wkup_cause_active.
    * Volatile; can be set by threshold comparisons on any AON clock tick.
    * Reserved bits[31:1] are always zero.
    * CDC read-back synchronization applied.
    */
   bool handle_read_WKUP_CAUSE(uint32_t& value, uint32_t read_mask);

   // =========================================================================
   // FUNC001: Internal Helper Methods
   // =========================================================================

   /**
    * @brief Compute the CDC propagation delay based on the current AON clock frequency.
    * @return sc_time representing 2 AON clock cycles (representative synchronizer delay).
    *
    * The exact number of CDC synchronizer stages is not modeled. A representative
    * 2-cycle delay is used as annotated by the quantum keeper. If the AON clock
    * frequency is not yet configured (zero or not readable), returns SC_ZERO_TIME
    * to avoid division by zero.
    */
   sc_time compute_cdc_delay() const;

   /**
    * @brief Evaluate the wakeup timer threshold condition and update outputs.
    *
    * Checks if m_wkup_counter >= m_wkup_threshold AND m_wkup_enabled is true.
    * If so, asserts intr_wkup_timer_expired, updates INTR_STATE shadow,
    * and asserts wkup_req via m_wkup_cause_active.
    *
    * This method does NOT de-assert interrupts - de-assertion is performed only
    * by W1C writes to INTR_STATE (and by reset). This avoids inadvertently
    * suppressing a pending interrupt during a threshold write.
    *
    * Called by: handle_write_WKUP_CTRL, handle_write_WKUP_THOLD_HI,
    *            handle_write_WKUP_THOLD_LO, handle_write_WKUP_COUNT_HI,
    *            handle_write_WKUP_COUNT_LO, and wakeup counter increment events.
    */
   void evaluate_wkup_threshold();

   /**
    * @brief Evaluate the watchdog bark threshold condition and update bark outputs.
    *
    * Checks if m_wdog_counter >= m_wdog_bark_threshold AND m_wdog_enabled is true.
    * If so, asserts intr_wdog_timer_bark, nmi_wdog_timer_bark, and wkup_req.
    *
    * This method does NOT de-assert bark interrupts - de-assertion is via W1C
    * writes to INTR_STATE or via watchdog petting (WDOG_COUNT write).
    *
    * Called by: handle_write_WDOG_CTRL, handle_write_WDOG_BARK_THOLD,
    *            handle_write_WDOG_COUNT, and watchdog counter increment events.
    */
   void evaluate_bark_threshold();

   /**
    * @brief Evaluate the watchdog bite threshold condition and update bite output.
    *
    * Checks if m_wdog_counter >= m_wdog_bite_threshold AND m_wdog_enabled is true.
    * If so, asserts aon_timer_rst_req. De-asserts aon_timer_rst_req when the
    * condition is false (e.g., after watchdog petting resets counter to 0).
    *
    * Called by: handle_write_WDOG_CTRL, handle_write_WDOG_BITE_THOLD,
    *            handle_write_WDOG_COUNT, and watchdog counter increment events.
    */
   void evaluate_bite_threshold();

   /**
    * @brief Update the WKUP_CAUSE and wkup_req output based on active sources.
    *
    * wkup_req is the logical OR of the wakeup timer threshold condition and the
    * watchdog bark condition. This method evaluates m_wkup_cause_active and
    * drives the wkup_req port accordingly.
    *
    * Does not de-assert wkup_req if the software has not cleared WKUP_CAUSE:
    * m_wkup_cause_active can only be cleared by explicit software write-0 to
    * WKUP_CAUSE (handle_write_WKUP_CAUSE).
    */
   void update_wkup_req_output();

   // =========================================================================
   // FUNC003: Wakeup Timer Tick Engine Methods
   // =========================================================================

   /**
    * @brief SC_THREAD: Autonomous wakeup timer counter tick engine (FUNC003).
    *
    * Sensitivity: driven internally; uses timed wait and m_ev_wkup_tick event.
    *
    * This thread implements the 64-bit upcounting wakeup timer tick engine.
    * It runs continuously for the lifetime of the simulation. The behavioral
    * loop is:
    *
    *   1. If m_wkup_enabled is false OR m_lc_escalate_active is true:
    *        Wait on m_ev_wkup_tick (until enabled and not escalated).
    *
    *   2. Once enabled and escalation-inactive, compute next tick delay:
    *        prescaler_val = WKUP_CTRL.prescaler (current value, 12-bit)
    *        tick_delay = sc_time(1.0 / clk_aon_freq.read() * (prescaler_val+1), SC_SEC)
    *
    *   3. Wait for tick_delay OR m_ev_wkup_tick OR m_ev_counter_cancel using
    *        wait(tick_delay, m_ev_wkup_tick | m_ev_counter_cancel).
    *        This allows the thread to be interrupted by:
    *          - WKUP_CTRL writes (prescaler change, enable/disable): m_ev_wkup_tick
    *          - System reset: m_ev_counter_cancel
    *          - lc_escalate_en change: m_ev_wkup_tick
    *
    *   4. If awakened by m_ev_counter_cancel: re-enter the disabled wait at step 1.
    *
    *   5. If awakened by m_ev_wkup_tick before tick_delay expired:
    *        Re-evaluate state at step 1 without incrementing the counter.
    *
    *   6. If tick_delay expired (no event): increment m_wkup_counter by 1
    *        with standard uint64 unsigned overflow wrap (0xFFFFFFFFFFFFFFFF -> 0).
    *        Then call evaluate_wkup_threshold() to check if threshold is crossed.
    *
    * Architecture Map Reference:
    *   - state_machines[Wakeup Timer FSM].transitions
    *   - timing_constraints[counter-increment-rate]
    *   - side_effects[WKUP_COUNT_HI/WKUP_COUNT_LO].hardware-auto-increment
    *
    * Single-writer compliance: this thread only writes m_wkup_counter.
    * It never writes sc_out<bool> ports directly; threshold assertion is
    * delegated to evaluate_wkup_threshold() which notifies m_ev_output_update.
    *
    * Reset behavior: when m_ev_counter_cancel fires (rst_n asserted in
    * reset_process()), the thread unblocks from its timed wait, observes
    * m_wkup_enabled=false (reset_process clears it before notifying), and
    * re-enters the disabled wait at step 1.
    */
   void wkup_timer_tick_thread();

   /**
    * @brief SC_METHOD: Lifecycle escalation handler - updates m_lc_escalate_active (FUNC003+FUNC004).
    *
    * Sensitive to: lc_escalate_en (sc_in<bool> input port).
    *
    * On every change of lc_escalate_en:
    *   1. Reads the current value of lc_escalate_en.
    *   2. Updates m_lc_escalate_active = lc_escalate_en.read().
    *   3. Notifies m_ev_wkup_tick with SC_ZERO_TIME so that the
    *      wkup_timer_tick_thread re-evaluates its state immediately:
    *        - If lc_escalate_en asserted: thread will find m_lc_escalate_active=true
    *          and enter the disabled-wait, freezing the counter.
    *        - If lc_escalate_en de-asserted: thread will find m_lc_escalate_active=false
    *          and, if m_wkup_enabled=true, resume the timed tick loop.
    *   4. Notifies m_ev_wdog_tick with SC_ZERO_TIME so that the
    *      wdog_timer_tick_thread (FUNC004) re-evaluates its state immediately:
    *        - If asserted: thread finds m_lc_escalate_active=true, enters disabled wait.
    *        - If de-asserted: thread finds m_lc_escalate_active=false; resumes if enabled.
    *
    * Architecture Map Reference:
    *   - state_machines[Wakeup Timer FSM].transitions[COUNTING->ESCALATION_HALT]
    *   - state_machines[Wakeup Timer FSM].transitions[ESCALATION_HALT->COUNTING]
    *   - state_machines[WDOG_TIMER].transitions[COUNTING->ESCALATION_HALT]
    *   - state_machines[WDOG_TIMER].transitions[ESCALATION_HALT->COUNTING]
    *
    * Single-writer compliance: this method only updates m_lc_escalate_active and
    * notifies events. It does NOT write any sc_out<bool> port directly.
    */
   void lc_escalate_handler();

   // =========================================================================
   // FUNC004: Watchdog Timer Tick Engine Methods
   // =========================================================================

   /**
    * @brief SC_THREAD: Autonomous 32-bit watchdog timer counter tick engine (FUNC004).
    *
    * Sensitivity: driven internally; uses timed wait and m_ev_wdog_tick event.
    *
    * This thread implements the no-prescaler 32-bit upcounting watchdog timer tick
    * engine. It runs continuously for the lifetime of the simulation. The behavioral
    * loop has three internal states:
    *
    *   DISABLED / PAUSED / ESCALATION_HALT state:
    *     Entered when any of the following holds:
    *       - m_wdog_enabled is false (watchdog disabled), OR
    *       - m_lc_escalate_active is true (lifecycle escalation halts the counter), OR
    *       - m_wdog_pause_in_sleep is true AND sleep_mode.read() is true (sleep pause).
    *     In this state the thread blocks on m_ev_wdog_tick.
    *     Wakeup triggers:
    *       - handle_write_WDOG_CTRL (enable/pause_in_sleep changes): m_ev_wdog_tick
    *       - handle_write_WDOG_COUNT (pet, resets counter to 0): m_ev_wdog_tick
    *       - wdog_sleep_mode_handler (sleep_mode change): m_ev_wdog_tick
    *       - lc_escalate_handler (escalation asserted/de-asserted): m_ev_wdog_tick
    *       - reset_process (rst_n asserted): m_ev_counter_cancel + m_ev_wdog_tick
    *
    *   COUNTING state (enabled, not escalated, not paused):
    *     Tick delay = sc_time(1.0 / clk_aon_freq.read(), SC_SEC) (one AON tick).
    *     Performs wait(tick_delay, m_ev_wdog_tick | m_ev_counter_cancel).
    *     If timeout fires: increment m_wdog_counter with uint32 overflow wrap,
    *       update WDOG_COUNT shadow, call evaluate_bark_threshold() and
    *       evaluate_bite_threshold(). Loop back to check state.
    *     If m_ev_wdog_tick fires: re-evaluate state at top of loop (no increment).
    *     If m_ev_counter_cancel fires: same as m_ev_wdog_tick; reset will have
    *       cleared m_wdog_enabled so thread re-enters disabled state.
    *
    *   RESET (m_ev_counter_cancel notified by reset_process):
    *     After unblocking, thread re-evaluates state. reset_process clears
    *     m_wdog_enabled before notifying, so thread re-enters disabled state.
    *
    * Architecture Map References:
    *   - state_machines[WDOG_TIMER].transitions
    *   - timing_constraints[counter-increment-rate]: tick = 1/clk_aon_freq (no prescaler)
    *   - side_effects[WDOG_COUNT].hardware-auto-increment
    *   - side_effects[WDOG_CTRL.pause_in_sleep]: halts counter when sleep_mode asserted
    *
    * Single-writer compliance: only m_wdog_counter is modified here; no sc_out<bool>
    * ports are written directly. evaluate_bark/bite_threshold() delegates port updates
    * to drive_outputs() SC_METHOD via m_ev_output_update.
    *
    * Pause-in-sleep semantics (Architecture Map):
    *   Counter halts when BOTH m_wdog_pause_in_sleep=true AND sleep_mode.read()=true.
    *   Resumes from the frozen counter value when either condition is removed.
    *   sleep_mode is sampled at each loop iteration to detect dynamic changes.
    */
   void wdog_timer_tick_thread();

   /**
    * @brief SC_METHOD: Sleep mode change handler for watchdog pause-in-sleep (FUNC004).
    *
    * Sensitive to: sleep_mode (sc_in<bool> input port).
    *
    * Invoked on every change of the sleep_mode input signal. Updates the cached
    * m_sleep_mode_active flag and notifies m_ev_wdog_tick with SC_ZERO_TIME so
    * that wdog_timer_tick_thread re-evaluates the pause condition immediately:
    *
    *   - sleep_mode asserts (true) AND m_wdog_pause_in_sleep=true:
    *       Thread wakes and finds pause condition true, enters disabled-wait.
    *   - sleep_mode de-asserts (false), regardless of m_wdog_pause_in_sleep:
    *       Thread wakes and re-evaluates; if enabled and not paused, resumes counting.
    *   - sleep_mode asserts (true) AND m_wdog_pause_in_sleep=false:
    *       Thread wakes and re-evaluates; pause condition false, continues counting.
    *
    * Architecture Map Reference:
    *   - state_machines[WDOG_TIMER].transitions[COUNTING->PAUSED]
    *   - state_machines[WDOG_TIMER].transitions[PAUSED->COUNTING]
    *   - registers[WDOG_CTRL].fields[pause_in_sleep].write_effects
    *
    * Single-writer compliance: this method only updates m_sleep_mode_active and
    * notifies m_ev_wdog_tick. It does NOT write any sc_out<bool> port directly.
    */
   void wdog_sleep_mode_handler();

   // =========================================================================
   // Reset Process
   // =========================================================================

   /**
    * @brief SC_THREAD: monitors rst_n and rst_aon_n; implements dual-domain reset.
    *
    * Sensitive to: rst_n, rst_aon_n (via SC_THREAD sensitivity list in constructor).
    *
    * FUNC002: Implements the dual active-low reset domain architecture. Two distinct
    * reset paths are executed depending on which reset input(s) are asserted:
    *
    * Path A - SYS-domain full reset (rst_n asserted, active-low):
    *   Performs a complete power-on reset of ALL internal state variables and ALL 14
    *   register shadows. This is the canonical system reset that purges every timer
    *   state, clears the WDOG_REGWEN lock, de-asserts all outputs, and cancels any
    *   pending counter tick events via m_ev_counter_cancel.
    *
    *   Internal state cleared:
    *     m_wkup_counter, m_wkup_threshold, m_wkup_prescaler_count, m_wkup_enabled,
    *     m_wdog_counter, m_wdog_bark_threshold, m_wdog_bite_threshold, m_wdog_enabled,
    *     m_wdog_pause_in_sleep, m_wdog_regwen_locked (= false => WDOG_REGWEN = 0x1),
    *     m_intr_state_wkup, m_intr_state_bark, m_wkup_cause_active, m_wdog_bite_active,
    *     m_lc_escalate_active, m_sleep_mode_active, m_fatal_fault_pending,
    *     m_racl_error_active.
    *   Register reset: reset_all_registers() called (all 14 registers).
    *   Output notification: m_ev_output_update notified with SC_ZERO_TIME.
    *   Counter cancel: m_ev_counter_cancel + m_ev_wdog_tick notified with SC_ZERO_TIME
    *     for FUNC003/FUNC004 to cancel running counter tick events.
    *
    * Path B - AON-domain partial reset (rst_aon_n asserted low, rst_n high):
    *   Resets only the AON-domain output state. SYS-domain register shadows, counter
    *   values, threshold values, interrupt flags, and the WDOG_REGWEN lock are NOT
    *   modified.
    *
    *   Internal state cleared (AON-domain only):
    *     m_wkup_cause_active (= false), m_wdog_bite_active (= false).
    *   Register shadow: WKUP_CAUSE.cause = 0 (AON-domain register).
    *   Output notification: m_ev_output_update notified with SC_ZERO_TIME.
    *   drive_outputs() then de-asserts wkup_req and aon_timer_rst_req.
    *
    * When both rst_n and rst_aon_n are asserted simultaneously, Path A (full reset)
    * is applied because rst_n subsumes all AON-domain reset effects as well.
    *
    * Single-writer compliance: drive_outputs() SC_METHOD is the sole writer for ALL
    * sc_out<bool> ports at all times, including during reset. reset_process() updates
    * only internal state flags and notifies m_ev_output_update with SC_ZERO_TIME.
    * It NEVER calls port.write() directly.
    *
    * Output port values after SYS-domain reset (Path A):
    *   intr_wkup_timer_expired = false, intr_wdog_timer_bark = false,
    *   nmi_wdog_timer_bark = false, wkup_req = false, aon_timer_rst_req = false,
    *   fatal_fault = false, racl_error = false.
    *
    * Output port values after AON-domain reset only (Path B):
    *   wkup_req = false, aon_timer_rst_req = false.
    *   All SYS-domain outputs (intr_*, fatal_fault, racl_error) are unchanged.
    *
    * Architecture Map Reference: reset_behavior.reset_types[system-reset],
    *   reset_behavior.post_reset_state.
    */
   void reset_process()
   {
      while (true)
      {
         const bool sys_reset_asserted = !rst_n.read();
         const bool aon_reset_asserted = !rst_aon_n.read();

         if (sys_reset_asserted)
         {
            /* ---------------------------------------------------------------
             * Path A: Full SYS-domain reset.
             * Resets ALL internal state and ALL 14 register shadows.
             * This is the only mechanism that clears WDOG_REGWEN lock.
             * Architecture Map: reset_behavior.reset_types[system-reset]
             * ------------------------------------------------------------- */
            CSML_INFO(1, logger) << name()
               << ": rst_n asserted - executing full system reset (SYS + AON domains)";

            /* Cancel any pending counter tick events for FUNC003/FUNC004.
             * Counter tick SC_THREADs must be sensitive to m_ev_counter_cancel
             * and cancel their scheduled increment events on receipt.
             * Also notify m_ev_wkup_tick so that wkup_timer_tick_thread can
             * wake from any timed wait and observe the cancelled state.
             * Notify m_ev_wdog_tick so that wdog_timer_tick_thread (FUNC004)
             * likewise wakes from any timed wait and observes the cancelled state.
             * Architecture Map: reset_behavior.reset_types[system-reset] - all
             * counter events cancelled before state is zeroed. */
            m_ev_counter_cancel.notify(SC_ZERO_TIME);
            m_ev_wkup_tick.notify(SC_ZERO_TIME);
            m_ev_wdog_tick.notify(SC_ZERO_TIME);

            /* Restore all 14 register shadows to hardware power-on defaults.
             * WDOG_REGWEN resets to 0x1 (unlocked); all others reset to 0x0. */
            reset_all_registers();

            /* --- SYS-domain internal state reset to power-on defaults --- */
            m_wkup_counter         = 0ULL;
            m_wkup_threshold       = 0ULL;
            m_wkup_prescaler_count = 0U;
            m_wkup_enabled         = false;

            m_wdog_counter         = 0U;
            m_wdog_bark_threshold  = 0U;
            m_wdog_bite_threshold  = 0U;
            m_wdog_enabled         = false;
            m_wdog_pause_in_sleep  = false;

            /* Unconditionally clear WDOG_REGWEN lock (only reset can do this). */
            m_wdog_regwen_locked   = false;

            m_intr_state_wkup      = false;
            m_intr_state_bark      = false;
            m_wdog_bark_latched    = false;
            m_lc_escalate_active   = false;
            m_sleep_mode_active    = false;
            m_fatal_fault_pending  = false;
            m_racl_error_active    = false;

            /* FUNC003: clear non-atomic HI-read pending flag on reset. */
            m_wkup_hi_read_pending = false;

            /* --- AON-domain internal state reset (subsumed by full reset) --- */
            m_wkup_cause_active      = false;
            m_wkup_threshold_latched = false;
            m_wdog_bite_active       = false;

            /* Notify drive_outputs() SC_METHOD to de-assert all output ports.
             * Single-writer rule: reset_process must NOT write ports directly. */
            m_ev_output_update.notify(SC_ZERO_TIME);

            CSML_INFO(1, logger) << name()
               << ": Full system reset complete: all registers and outputs at power-on defaults";
         }
         else if (aon_reset_asserted)
         {
            /* ---------------------------------------------------------------
             * Path B: AON-domain reset only (rst_aon_n asserted, rst_n high).
             * Clears only AON-domain output state: wkup_req and aon_timer_rst_req.
             * SYS-domain state (counters, thresholds, interrupt flags, lock,
             * prescaler, register shadows other than WKUP_CAUSE) is preserved.
             * Architecture Map: Dual reset domain architecture.
             * ------------------------------------------------------------- */
            CSML_INFO(1, logger) << name()
               << ": rst_aon_n asserted (rst_n high) - executing AON-domain partial reset";

            /* Clear AON-domain wakeup cause flag and threshold latch. */
            m_wkup_cause_active      = false;
            m_wkup_threshold_latched = false;
            WKUP_CAUSE.cause         = 0U;

            /* Clear AON-domain bite active flag (de-asserts aon_timer_rst_req). */
            m_wdog_bite_active     = false;

            /* Notify drive_outputs() SC_METHOD to de-assert wkup_req and
             * aon_timer_rst_req. SYS-domain outputs are re-driven to their
             * current (unchanged) internal state values. */
            m_ev_output_update.notify(SC_ZERO_TIME);

            CSML_INFO(1, logger) << name()
               << ": AON-domain partial reset complete: wkup_req and aon_timer_rst_req de-asserted";
         }

         wait();
      }
   }

   // =========================================================================
   // FUNC001 Fix: Deferred output driver SC_METHOD
   // =========================================================================

   /**
    * @brief SC_METHOD: drives all sc_out<bool> ports from internal state variables.
    *
    * Sensitive to: m_ev_output_update (notified with SC_ZERO_TIME by write callbacks,
    * threshold evaluation helpers, and reset_process() whenever any output-mapped
    * internal boolean flag changes).
    *
    * This method is the single point where internal boolean state is propagated
    * to sc_out<bool> ports. It replaces all direct sc_out::write() calls inside
    * register write callbacks and threshold evaluation helpers, which are invoked
    * from the b_transport chain. In SystemC 3.0, sc_out::write() called directly
    * inside b_transport throws a std::exception; using an SC_METHOD with a notified
    * event defers the port drive to a safe delta-cycle process context.
    *
    * Output mapping (internal flag -> port):
    *   - m_intr_state_wkup    -> intr_wkup_timer_expired
    *   - m_intr_state_bark    -> intr_wdog_timer_bark
    *   - m_intr_state_bark    -> nmi_wdog_timer_bark
    *   - m_wkup_cause_active  -> wkup_req
    *   - m_wdog_bite_active   -> aon_timer_rst_req  (FUNC002: dedicated flag, not inline evaluation)
    *   - m_fatal_fault_pending -> fatal_fault (transient pulse; cleared after drive)
    *   - m_racl_error_active  -> racl_error
    *
    * FUNC002: aon_timer_rst_req is now driven from m_wdog_bite_active instead of
    * an inline counter/threshold evaluation. This allows the AON-domain reset path
    * (rst_aon_n) to independently de-assert aon_timer_rst_req by clearing
    * m_wdog_bite_active without modifying SYS-domain counter state.
    *
    * Every sc_out<bool> port is written on every invocation, ensuring this method
    * is the unconditional sole writer of all output ports (single-driver rule).
    *
    * Called exclusively by the SystemC scheduler when m_ev_output_update fires.
    * Never called directly.
    */
   void drive_outputs();
};
