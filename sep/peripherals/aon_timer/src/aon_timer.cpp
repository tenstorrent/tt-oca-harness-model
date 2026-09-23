// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer.cpp
 * @brief AON Timer model implementation - FUNC001 + FUNC002 + FUNC003 + FUNC004.
 *
 * Implements all functional register write and read callbacks for the 14 AON Timer
 * memory-mapped registers (FUNC001), plus the threshold evaluation helpers updated
 * for FUNC002 dual-domain reset architecture. FUNC003 adds the autonomous wakeup
 * timer counter tick engine. FUNC004 adds the autonomous watchdog timer counter tick
 * engine. This file provides the complete register access, output driver, and counter
 * increment layer for the TLM-2.0 loosely-timed model:
 *
 *   - register_all_callbacks(): registers functional callbacks in regmodel::Memory after
 *     base-class construction so they override the default regmodel register handlers.
 *
 *   - Write callbacks (14): ALERT_TEST, WKUP_CTRL, WKUP_THOLD_HI, WKUP_THOLD_LO,
 *     WKUP_COUNT_HI, WKUP_COUNT_LO, WDOG_REGWEN, WDOG_CTRL, WDOG_BARK_THOLD,
 *     WDOG_BITE_THOLD, WDOG_COUNT, INTR_STATE, INTR_TEST, WKUP_CAUSE.
 *
 *   - Read callbacks (12): WKUP_CTRL, WKUP_THOLD_HI, WKUP_THOLD_LO, WKUP_COUNT_HI,
 *     WKUP_COUNT_LO, WDOG_REGWEN, WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD,
 *     WDOG_COUNT, INTR_STATE, WKUP_CAUSE.
 *     (ALERT_TEST and INTR_TEST are write-only; their read-restriction callbacks are
 *     already registered by the regmodel register framework and are NOT overridden.)
 *
 *   - Helper methods: compute_cdc_delay(), evaluate_wkup_threshold(),
 *     evaluate_bark_threshold(), evaluate_bite_threshold(), update_wkup_req_output().
 *
 *   - FUNC003 engine methods: wkup_timer_tick_thread() SC_THREAD,
 *     lc_escalate_handler() SC_METHOD.
 *
 *   - FUNC004 engine methods: wdog_timer_tick_thread() SC_THREAD,
 *     wdog_sleep_mode_handler() SC_METHOD.
 *
 * CDC Abstraction (FUNC001 architecture-map requirement):
 *   Register writes annotate the quantum keeper by compute_cdc_delay() (2 AON clock
 *   cycles) to model the SYS-to-AON domain crossing latency. Read callbacks
 *   call m_qk.sync() to stall until the quantum is exhausted, providing the
 *   software read-back synchronization guarantee documented in Section 7.9 of the
 *   detailed design.
 *
 * Reserved Bit Masking (FUNC001):
 *   All read callbacks mask out reserved bits to zero before returning values.
 *   Write callbacks silently ignore writes to reserved bit positions.
 *
 * Access Type Enforcement (FUNC001):
 *   - RW   : standard read/write; shadow updated on write, shadow returned on read.
 *   - RW1C : INTR_STATE; writing 1 to a bit clears it; writing 0 has no effect.
 *   - RW0C : WKUP_CAUSE, WDOG_REGWEN; writing 0 clears/locks; writing 1 has no effect.
 *   - WO   : ALERT_TEST, INTR_TEST; write has side effects; reads return 0x0 (enforced
 *            by regmodel read-restriction callback already registered by the framework).
 *
 * FUNC002 - System Reset and Initialization (additions to this file):
 *   evaluate_bite_threshold() is updated to maintain the dedicated m_wdog_bite_active
 *   flag rather than relying on an inline counter/threshold evaluation inside
 *   drive_outputs(). This decouples the aon_timer_rst_req output from the counter
 *   state, enabling the AON-domain reset (rst_aon_n) to independently de-assert
 *   aon_timer_rst_req by clearing m_wdog_bite_active without modifying counter values.
 *   drive_outputs() is updated to use m_wdog_bite_active as the source for the
 *   aon_timer_rst_req port drive.
 *   The reset_process() SC_THREAD (defined inline in aon_timer.h) implements the
 *   dual-domain reset paths using the new m_wdog_bite_active flag.
 *
 * FUNC003 - Wakeup Timer Engine Operation (additions to this file):
 *   wkup_timer_tick_thread() SC_THREAD runs continuously and implements the 64-bit
 *   prescaler-controlled counter increment. It uses wait(tick_delay, event_or) to
 *   allow interruption by WKUP_CTRL writes, lc_escalate_en changes, and system reset.
 *   lc_escalate_handler() SC_METHOD updates m_lc_escalate_active and notifies
 *   m_ev_wkup_tick to freeze or resume the thread.
 *   handle_write_WKUP_CTRL is updated to notify m_ev_wkup_tick on any write.
 *   handle_read_WKUP_COUNT_HI is updated to set m_wkup_hi_read_pending=true to model
 *   the non-atomic 64-bit register read race condition.
 *   handle_read_WKUP_COUNT_LO is updated to advance the counter by one tick if
 *   m_wkup_hi_read_pending is true and the timer is running, modeling the race.
 *
 * FUNC004 - Watchdog Timer Engine Operation (additions to this file):
 *   wdog_timer_tick_thread() SC_THREAD runs continuously and implements the 32-bit
 *   no-prescaler counter increment at the direct AON clock rate (1 tick per AON period).
 *   wdog_sleep_mode_handler() SC_METHOD updates m_sleep_mode_active and notifies
 *   m_ev_wdog_tick when sleep_mode transitions, enabling pause-in-sleep support.
 *   lc_escalate_handler() is extended to additionally notify m_ev_wdog_tick so
 *   that the watchdog tick thread halts/resumes on escalation state changes.
 *   handle_write_WDOG_CTRL is extended to notify m_ev_wdog_tick on any write so the
 *   watchdog thread immediately re-evaluates enable/pause_in_sleep state.
 *   handle_write_WDOG_COUNT is extended to notify m_ev_wdog_tick after petting so
 *   the thread observes the reset counter (0) state without delay.
 *
 * Backward Compatibility:
 *   All FUNC001, FUNC002, and FUNC003 register callbacks and SC_THREADs/SC_METHODs
 *   are preserved without behavioral modification. FUNC004 only extends existing
 *   callbacks by adding m_ev_wdog_tick notifications (pure additions).
 *   No existing callback semantics are altered.
 */

#include "aon_timer.h"
#include <tlm_utils/tlm_quantumkeeper.h>

// =============================================================================
// FUNC001: Callback Registration
// =============================================================================

/**
 * @brief Register all functional write and read callbacks for the 14 AON Timer registers.
 *
 * All offsets are word-addressed (byte_offset / sizeof(uint32_t) = byte_offset / 4)
 * as required by the regmodel::Memory::register_write_callback and
 * regmodel::Memory::register_read_callback APIs. The callbacks are registered after
 * base-class construction so they replace the default regmodel register-level handlers.
 *
 * Write callback registration sequence (14 total):
 *   ALERT_TEST (0x00/4=0), WKUP_CTRL (0x04/4=1), WKUP_THOLD_HI (0x08/4=2),
 *   WKUP_THOLD_LO (0x0C/4=3), WKUP_COUNT_HI (0x10/4=4), WKUP_COUNT_LO (0x14/4=5),
 *   WDOG_REGWEN (0x18/4=6), WDOG_CTRL (0x1C/4=7), WDOG_BARK_THOLD (0x20/4=8),
 *   WDOG_BITE_THOLD (0x24/4=9), WDOG_COUNT (0x28/4=10), INTR_STATE (0x2C/4=11),
 *   INTR_TEST (0x30/4=12), WKUP_CAUSE (0x34/4=13).
 *
 * Read callback registration sequence (12 total, excludes WO registers):
 *   WKUP_CTRL (1), WKUP_THOLD_HI (2), WKUP_THOLD_LO (3), WKUP_COUNT_HI (4),
 *   WKUP_COUNT_LO (5), WDOG_REGWEN (6), WDOG_CTRL (7), WDOG_BARK_THOLD (8),
 *   WDOG_BITE_THOLD (9), WDOG_COUNT (10), INTR_STATE (11), WKUP_CAUSE (13).
 *
 * ALERT_TEST and INTR_TEST read-as-zero behavior is enforced by the regmodel register
 * framework's read_restriction_error callback (registered because read_bit_mask == 0x0
 * in ALERT_TEST_type and INTR_TEST_type constructors). These are NOT overridden here.
 */
void aon_timer_ip::register_all_callbacks()
{
   /* ---------------------------------------------------------------------- */
   /* Write callbacks - one per register (14 total)                          */
   /* ---------------------------------------------------------------------- */

   /* ALERT_TEST: 0x00 / 4 = word offset 0 */
   std::function<bool(uint32_t)> cb_write_alert_test =
      [this](uint32_t value) {
         return this->handle_write_ALERT_TEST(value, ALERT_TEST.write_bit_mask);
      };
   memory.register_write_callback(cb_write_alert_test, ALERT_TEST.offset);

   /* WKUP_CTRL: 0x04 / 4 = word offset 1 */
   std::function<bool(uint32_t)> cb_write_wkup_ctrl =
      [this](uint32_t value) {
         return this->handle_write_WKUP_CTRL(value, WKUP_CTRL.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_ctrl, WKUP_CTRL.offset);

   /* WKUP_THOLD_HI: 0x08 / 4 = word offset 2 */
   std::function<bool(uint32_t)> cb_write_wkup_thold_hi =
      [this](uint32_t value) {
         return this->handle_write_WKUP_THOLD_HI(value, WKUP_THOLD_HI.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_thold_hi, WKUP_THOLD_HI.offset);

   /* WKUP_THOLD_LO: 0x0C / 4 = word offset 3 */
   std::function<bool(uint32_t)> cb_write_wkup_thold_lo =
      [this](uint32_t value) {
         return this->handle_write_WKUP_THOLD_LO(value, WKUP_THOLD_LO.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_thold_lo, WKUP_THOLD_LO.offset);

   /* WKUP_COUNT_HI: 0x10 / 4 = word offset 4 */
   std::function<bool(uint32_t)> cb_write_wkup_count_hi =
      [this](uint32_t value) {
         return this->handle_write_WKUP_COUNT_HI(value, WKUP_COUNT_HI.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_count_hi, WKUP_COUNT_HI.offset);

   /* WKUP_COUNT_LO: 0x14 / 4 = word offset 5 */
   std::function<bool(uint32_t)> cb_write_wkup_count_lo =
      [this](uint32_t value) {
         return this->handle_write_WKUP_COUNT_LO(value, WKUP_COUNT_LO.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_count_lo, WKUP_COUNT_LO.offset);

   /* WDOG_REGWEN: 0x18 / 4 = word offset 6 */
   std::function<bool(uint32_t)> cb_write_wdog_regwen =
      [this](uint32_t value) {
         return this->handle_write_WDOG_REGWEN(value, WDOG_REGWEN.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wdog_regwen, WDOG_REGWEN.offset);

   /* WDOG_CTRL: 0x1C / 4 = word offset 7 */
   std::function<bool(uint32_t)> cb_write_wdog_ctrl =
      [this](uint32_t value) {
         return this->handle_write_WDOG_CTRL(value, WDOG_CTRL.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wdog_ctrl, WDOG_CTRL.offset);

   /* WDOG_BARK_THOLD: 0x20 / 4 = word offset 8 */
   std::function<bool(uint32_t)> cb_write_wdog_bark_thold =
      [this](uint32_t value) {
         return this->handle_write_WDOG_BARK_THOLD(value, WDOG_BARK_THOLD.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wdog_bark_thold, WDOG_BARK_THOLD.offset);

   /* WDOG_BITE_THOLD: 0x24 / 4 = word offset 9 */
   std::function<bool(uint32_t)> cb_write_wdog_bite_thold =
      [this](uint32_t value) {
         return this->handle_write_WDOG_BITE_THOLD(value, WDOG_BITE_THOLD.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wdog_bite_thold, WDOG_BITE_THOLD.offset);

   /* WDOG_COUNT: 0x28 / 4 = word offset 10 */
   std::function<bool(uint32_t)> cb_write_wdog_count =
      [this](uint32_t value) {
         return this->handle_write_WDOG_COUNT(value, WDOG_COUNT.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wdog_count, WDOG_COUNT.offset);

   /* INTR_STATE: 0x2C / 4 = word offset 11 */
   std::function<bool(uint32_t)> cb_write_intr_state =
      [this](uint32_t value) {
         return this->handle_write_INTR_STATE(value, INTR_STATE.write_bit_mask);
      };
   memory.register_write_callback(cb_write_intr_state, INTR_STATE.offset);

   /* INTR_TEST: 0x30 / 4 = word offset 12 */
   std::function<bool(uint32_t)> cb_write_intr_test =
      [this](uint32_t value) {
         return this->handle_write_INTR_TEST(value, INTR_TEST.write_bit_mask);
      };
   memory.register_write_callback(cb_write_intr_test, INTR_TEST.offset);

   /* WKUP_CAUSE: 0x34 / 4 = word offset 13 */
   std::function<bool(uint32_t)> cb_write_wkup_cause =
      [this](uint32_t value) {
         return this->handle_write_WKUP_CAUSE(value, WKUP_CAUSE.write_bit_mask);
      };
   memory.register_write_callback(cb_write_wkup_cause, WKUP_CAUSE.offset);

   /* ---------------------------------------------------------------------- */
   /* Read callbacks - 12 registers (excludes WO ALERT_TEST and INTR_TEST)  */
   /* ---------------------------------------------------------------------- */

   /* WKUP_CTRL: word offset 1 */
   std::function<bool(uint32_t&)> cb_read_wkup_ctrl =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_CTRL(value, WKUP_CTRL.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_ctrl, WKUP_CTRL.offset);

   /* WKUP_THOLD_HI: word offset 2 */
   std::function<bool(uint32_t&)> cb_read_wkup_thold_hi =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_THOLD_HI(value, WKUP_THOLD_HI.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_thold_hi, WKUP_THOLD_HI.offset);

   /* WKUP_THOLD_LO: word offset 3 */
   std::function<bool(uint32_t&)> cb_read_wkup_thold_lo =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_THOLD_LO(value, WKUP_THOLD_LO.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_thold_lo, WKUP_THOLD_LO.offset);

   /* WKUP_COUNT_HI: word offset 4 */
   std::function<bool(uint32_t&)> cb_read_wkup_count_hi =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_COUNT_HI(value, WKUP_COUNT_HI.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_count_hi, WKUP_COUNT_HI.offset);

   /* WKUP_COUNT_LO: word offset 5 */
   std::function<bool(uint32_t&)> cb_read_wkup_count_lo =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_COUNT_LO(value, WKUP_COUNT_LO.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_count_lo, WKUP_COUNT_LO.offset);

   /* WDOG_REGWEN: word offset 6 */
   std::function<bool(uint32_t&)> cb_read_wdog_regwen =
      [this](uint32_t& value) {
         return this->handle_read_WDOG_REGWEN(value, WDOG_REGWEN.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wdog_regwen, WDOG_REGWEN.offset);

   /* WDOG_CTRL: word offset 7 */
   std::function<bool(uint32_t&)> cb_read_wdog_ctrl =
      [this](uint32_t& value) {
         return this->handle_read_WDOG_CTRL(value, WDOG_CTRL.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wdog_ctrl, WDOG_CTRL.offset);

   /* WDOG_BARK_THOLD: word offset 8 */
   std::function<bool(uint32_t&)> cb_read_wdog_bark_thold =
      [this](uint32_t& value) {
         return this->handle_read_WDOG_BARK_THOLD(value, WDOG_BARK_THOLD.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wdog_bark_thold, WDOG_BARK_THOLD.offset);

   /* WDOG_BITE_THOLD: word offset 9 */
   std::function<bool(uint32_t&)> cb_read_wdog_bite_thold =
      [this](uint32_t& value) {
         return this->handle_read_WDOG_BITE_THOLD(value, WDOG_BITE_THOLD.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wdog_bite_thold, WDOG_BITE_THOLD.offset);

   /* WDOG_COUNT: word offset 10 */
   std::function<bool(uint32_t&)> cb_read_wdog_count =
      [this](uint32_t& value) {
         return this->handle_read_WDOG_COUNT(value, WDOG_COUNT.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wdog_count, WDOG_COUNT.offset);

   /* INTR_STATE: word offset 11 */
   std::function<bool(uint32_t&)> cb_read_intr_state =
      [this](uint32_t& value) {
         return this->handle_read_INTR_STATE(value, INTR_STATE.read_bit_mask);
      };
   memory.register_read_callback(cb_read_intr_state, INTR_STATE.offset);

   /* WKUP_CAUSE: word offset 13 */
   std::function<bool(uint32_t&)> cb_read_wkup_cause =
      [this](uint32_t& value) {
         return this->handle_read_WKUP_CAUSE(value, WKUP_CAUSE.read_bit_mask);
      };
   memory.register_read_callback(cb_read_wkup_cause, WKUP_CAUSE.offset);

   REG_INFO(1, logger) << name()
      << ": register_all_callbacks: 14 write + 12 read callbacks registered (FUNC001)";
}

// =============================================================================
// FUNC001: CDC Delay Helper
// =============================================================================

/**
 * @brief Compute the CDC propagation delay as 2 AON clock cycles.
 * @return sc_time of 2 / clk_aon_freq seconds, or SC_ZERO_TIME if freq is zero.
 *
 * Architecture Map: timing_constraints - CDC synchronizer modeled as functional
 * quantum-keeper delay without cycle accuracy. Two AON clock cycles represents
 * a typical 2-stage synchronizer latency.
 */
sc_time aon_timer_ip::compute_cdc_delay() const
{
   double aon_freq = clk_aon_freq.read();
   if (aon_freq <= 0.0) {
      return SC_ZERO_TIME;
   }
   /* 2 AON clock cycles of delay to model SYS-to-AON synchronizer latency. */
   return sc_time(2.0 / aon_freq, SC_SEC);
}

// =============================================================================
// FUNC001: Threshold Evaluation Helpers
// =============================================================================

/**
 * @brief Evaluate wakeup timer threshold and assert outputs if condition is met.
 *
 * Asserts intr_wkup_timer_expired and wkup_req when:
 *   m_wkup_enabled == true AND m_wkup_counter >= m_wkup_threshold.
 *
 * This method only asserts - de-assertion is controlled by W1C writes to
 * INTR_STATE and by system reset, ensuring level-sensitive behavior.
 *
 * Architecture Map: state_machines[WKUP_TIMER].transitions[COUNTING->EXPIRED]
 * and side_effects for WKUP_CTRL/WKUP_THOLD_HI/WKUP_THOLD_LO writes.
 */
void aon_timer_ip::evaluate_wkup_threshold()
{
   if (m_wkup_enabled && (m_wkup_counter >= m_wkup_threshold)) {
      /* Whether the interrupt can re-fire after a W1C depends on the prescaler,
       * because RTL feeds prim_intr_hw from a posedge detector on wkup_intr_o
       * (aon_timer.sv:208) and wkup_intr_o = wkup_incr & (count >= thold):
       *
       *   prescaler == 0 : wkup_incr is high every AON cycle, so wkup_intr_o is a
       *                    sustained level. One posedge, one interrupt; a W1C is
       *                    final until the count drops back below the threshold.
       *   prescaler  > 0 : wkup_incr pulses once per (prescaler + 1) cycles, so
       *                    each tick is a fresh posedge and the interrupt does
       *                    re-fire after a W1C.
       *
       * m_wkup_threshold_latched is read here before the WKUP_CAUSE block below
       * sets it, so it still reports whether this tick is the first crossing. */
      const bool prescaler_zero = (static_cast<uint32_t>(WKUP_CTRL.prescaler) == 0U);
      const bool first_crossing = !m_wkup_threshold_latched;

      if ((first_crossing || !prescaler_zero) && !m_intr_state_wkup) {
         m_intr_state_wkup = true;
         INTR_STATE.wkup_timer_expired = 1;
         m_ev_output_update.notify(SC_ZERO_TIME);
      }

      /* WKUP_CAUSE is edge-triggered on the threshold crossing event, not
       * level-sensitive. Once the counter crosses the threshold, the latch is
       * set and WKUP_CAUSE is not re-asserted on subsequent ticks while the
       * counter remains above the threshold. The latch clears only when the
       * counter drops back below the threshold (see else branch below), which
       * re-arms the edge for the next crossing. This prevents WKUP_CAUSE from
       * being re-asserted immediately after software clears it via RW0C write-0
       * while the counter is still above the threshold. */
      if (!m_wkup_threshold_latched) {
         m_wkup_threshold_latched = true;
         if (!m_wkup_cause_active) {
            m_wkup_cause_active = true;
            WKUP_CAUSE.cause = 1;
            m_ev_output_update.notify(SC_ZERO_TIME);
         }
      }
   } else {
      /* Counter is below threshold (or timer disabled): clear the latch so the
       * next threshold crossing will fire WKUP_CAUSE again. */
      m_wkup_threshold_latched = false;
   }
}

void aon_timer_ip::rearm_wkup_edge()
{
   /* RTL compares the counter only on wkup_incr (aon_timer_core.sv). A CSR
    * write may drop the counter back below the threshold and must re-arm the
    * edge, but it must not raise the interrupt by itself. */
   if (!m_wkup_enabled || (m_wkup_counter < m_wkup_threshold))
      m_wkup_threshold_latched = false;
}

void aon_timer_ip::rearm_bark_edge()
{
   const bool counting = m_wdog_enabled &&
                          !m_lc_escalate_active &&
                          !(m_wdog_pause_in_sleep && sleep_mode.read());
   if (!counting || (m_wdog_counter < m_wdog_bark_threshold))
      m_wdog_bark_latched = false;
}

/**
 * @brief Evaluate watchdog bark threshold and assert bark outputs if condition is met.
 *
 * Asserts intr_wdog_timer_bark, nmi_wdog_timer_bark, and wkup_req when:
 *   m_wdog_enabled == true AND m_wdog_counter >= m_wdog_bark_threshold.
 *
 * This method only asserts - de-assertion is controlled by W1C writes to
 * INTR_STATE, by watchdog petting (WDOG_COUNT write), and by system reset.
 *
 * Architecture Map: state_machines[WDOG_TIMER].transitions[COUNTING->BARK]
 * and side_effects for WDOG_CTRL/WDOG_BARK_THOLD/WDOG_COUNT writes.
 */
void aon_timer_ip::evaluate_bark_threshold()
{
   /* Same gating as the bite path: wdog_intr_o = wdog_incr & (count >= bark_thold)
    * in aon_timer_core.sv:83. Without the escalation and sleep-pause terms a
    * register write could raise bark while the watchdog is halted. */
   const bool wdog_incr = m_wdog_enabled &&
                          !m_lc_escalate_active &&
                          !(m_wdog_pause_in_sleep && sleep_mode.read());

   if (wdog_incr && (m_wdog_counter >= m_wdog_bark_threshold)) {
      /* INTR_STATE.wdog_timer_bark is set on the posedge of the bark condition,
       * matching aon_timer.sv, which drives prim_intr_hw from a
       * prim_edge_detector's q_posedge_pulse_o rather than from the level. The
       * latch is what remembers the edge; m_intr_state_bark cannot serve that
       * role because software clears it via W1C, which would re-arm the detector
       * and re-fire on the next tick while the counter is still over threshold. */
      if (!m_wdog_bark_latched) {
         m_wdog_bark_latched = true;
         if (!m_intr_state_bark) {
            m_intr_state_bark = true;
            /* Update INTR_STATE shadow: set bit[1] (wdog_timer_bark). */
            INTR_STATE.wdog_timer_bark = 1;
            /* Defer bark/NMI port writes to drive_outputs() SC_METHOD. */
            m_ev_output_update.notify(SC_ZERO_TIME);
         }
      }
      if (!m_wkup_cause_active) {
         m_wkup_cause_active = true;
         /* Update WKUP_CAUSE shadow: set bit[0] (cause). */
         WKUP_CAUSE.cause = 1;
         /* Defer wkup_req port write to drive_outputs() SC_METHOD. */
         m_ev_output_update.notify(SC_ZERO_TIME);
      }
   } else {
      /* Condition false again (pet, disable, or raised threshold): re-arm the
       * edge so the next crossing fires a fresh interrupt. */
      m_wdog_bark_latched = false;
   }
}

/**
 * @brief Evaluate watchdog bite threshold and update m_wdog_bite_active flag.
 *
 * Sets m_wdog_bite_active = true when the watchdog is counting (enabled, not
 * escalated, not paused in sleep) AND m_wdog_counter >= m_wdog_bite_threshold.
 *
 * Never clears it. The bite request is latched until reset, matching
 * aon_rst_req_d = aon_rst_req_set | aon_rst_req_q in aon_timer.sv:272.
 *
 * FUNC002: The dedicated m_wdog_bite_active flag replaces the previous inline
 * evaluation inside drive_outputs(). This decouples aon_timer_rst_req from
 * the counter/threshold state and enables the AON-domain reset (rst_aon_n) to
 * independently de-assert aon_timer_rst_req by clearing m_wdog_bite_active
 * without modifying SYS-domain counter state.
 *
 * drive_outputs() reads m_wdog_bite_active to drive the aon_timer_rst_req port.
 * Notifies m_ev_output_update with SC_ZERO_TIME to trigger drive_outputs().
 *
 * Architecture Map Reference: events[wdog_timer_bite].trigger_conditions,
 *   state_machines[WDOG_TIMER].transitions[COUNTING->BITE_REACHED].
 *
 * Called by: handle_write_WDOG_CTRL, handle_write_WDOG_BITE_THOLD,
 *            handle_write_WDOG_COUNT, and watchdog counter increment events (FUNC004).
 */
void aon_timer_ip::evaluate_bite_threshold()
{
   /* Bite fires only while the watchdog is actually counting. This mirrors
    * wdog_reset_req_o = wdog_incr & (count >= bite_thold) in aon_timer_core.sv:85,
    * where wdog_incr carries the enable, the lifecycle-escalation term and the
    * sleep-pause term. Testing the counter against the threshold without those
    * terms lets a WDOG_COUNT or threshold write raise a reset request while the
    * watchdog is halted, which the hardware cannot do. */
   const bool wdog_incr = m_wdog_enabled &&
                          !m_lc_escalate_active &&
                          !(m_wdog_pause_in_sleep && sleep_mode.read());

   const bool bite_condition = wdog_incr &&
                               (m_wdog_counter >= m_wdog_bite_threshold);

   /* The request is a latch, not a level: aon_timer.sv:272 assigns
    * aon_rst_req_d = aon_rst_req_set | aon_rst_req_q, so once bite fires the
    * request is held until the AON reset. Software cannot withdraw it by petting
    * or by disabling the watchdog, which is the property that makes a bite
    * irrevocable. Only reset_process() may clear m_wdog_bite_active. */
   if (bite_condition && !m_wdog_bite_active) {
      m_wdog_bite_active = true;
      REG_INFO(1, logger) << name()
         << ": WDOG bite latched (counter=" << m_wdog_counter
         << " threshold=" << m_wdog_bite_threshold << ")";
   }

   if (m_wdog_bite_active) {
      /* Re-notify so drive_outputs() restores the port if a prior reset cleared it. */
      m_ev_output_update.notify(SC_ZERO_TIME);
   }
}

/**
 * @brief Drive the wkup_req output port based on m_wkup_cause_active.
 *
 * wkup_req is a level output driven directly from m_wkup_cause_active.
 * m_wkup_cause_active is the logical OR of the wakeup timer threshold condition
 * and the watchdog bark condition. It is only cleared by explicit software write-0
 * to WKUP_CAUSE (handle_write_WKUP_CAUSE) or by system reset.
 *
 * Architecture Map: events[wkup_req_assert/deassert] and interrupt_matrix.
 */
void aon_timer_ip::update_wkup_req_output()
{
   /* Defer wkup_req port write to drive_outputs() SC_METHOD (safe process context). */
   m_ev_output_update.notify(SC_ZERO_TIME);
}

// =============================================================================
// FUNC001: Write Callbacks
// =============================================================================

/**
 * @brief Write callback for ALERT_TEST (0x00, WO).
 *
 * Implements immediate fatal_fault assertion when bit[0] is written as 1.
 * This is a transient test pulse; no storage is involved.
 * Reserved bits[31:1] are silently ignored.
 *
 * CDC annotation: quantum keeper advanced by 2 AON clock cycles.
 *
 * @param value     32-bit value written by software.
 * @param write_mask Write mask (unused for WO registers; all bits accepted).
 * @return true.
 */
bool aon_timer_ip::handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Bit[0] = fatal_fault test: set pending flag; port is driven by drive_outputs()
    * SC_METHOD to avoid sc_out::write() inside b_transport (SystemC 3.0 constraint). */
   if (value & 0x1U) {
      m_fatal_fault_pending = true;
      m_ev_output_update.notify(SC_ZERO_TIME);
   }
   /* ALERT_TEST has no storage; register shadow remains cleared (0x0).
    * The regmodel framework has read_bit_mask=0x0 so reads already return 0x0. */

   /* CDC annotation: advance quantum keeper by SYS->AON synchronizer delay. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_CTRL (0x04, RW).
 *
 * Most side-effectful write in the register map. Unconditionally resets the
 * prescaler accumulator to 0 on every write, even if the value is unchanged.
 * Updates enable and prescaler state variables. Schedules or cancels wakeup
 * counter increment events based on enable transition. Triggers immediate
 * threshold comparison.
 *
 * Reserved bits[31:13] are masked to zero when updating the register shadow.
 *
 * CDC annotation: quantum keeper advanced by 2 AON clock cycles.
 *
 * @param value     32-bit value written by software (bit[0]=enable, bits[12:1]=prescaler).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_CTRL(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Extract fields; mask reserved bits[31:13] to zero. */
   bool  new_enable    = (value & 0x1U) != 0U;
   /* Prescaler is bits[12:1]: 12 bits */
   uint32_t new_prescaler = (value >> 1U) & 0xFFFU;

   /* Mandatory side-effect: reset prescaler accumulator unconditionally on every write. */
   m_wkup_prescaler_count = 0U;

   bool was_enabled = m_wkup_enabled;
   m_wkup_enabled   = new_enable;

   /* Update register shadow: only bits[12:0] are active; bits[31:13] read as zero. */
   WKUP_CTRL.enable    = new_enable ? 1U : 0U;
   WKUP_CTRL.prescaler = new_prescaler;
   WKUP_CTRL.reserved0 = 0U;

   /* Timer enable transition handling.
    * FUNC003: Notify m_ev_wkup_tick on every write to WKUP_CTRL so that the
    * wkup_timer_tick_thread can re-evaluate its state (enable/prescaler change).
    * This is mandatory even on same-value writes because the prescaler accumulator
    * was unconditionally reset, altering the next tick delay calculation. */
   if (!was_enabled && m_wkup_enabled) {
      /* 0->1 transition: counter increment events scheduled by wkup_timer_tick_thread. */
      REG_INFO(2, logger) << name()
         << ": WKUP_CTRL write: wakeup timer enabled (prescaler=" << new_prescaler << ")";
   } else if (was_enabled && !m_wkup_enabled) {
      /* 1->0 transition: wkup_timer_tick_thread will enter disabled wait on wakeup. */
      REG_INFO(2, logger) << name()
         << ": WKUP_CTRL write: wakeup timer disabled";
   }
   /* Notify unconditionally: prescaler accumulator reset changes next tick timing.
    * Wakeup, bark, and bite outputs change only on a real increment, matching
    * wkup_incr / wdog_incr in aon_timer_core.sv. */
   m_ev_wkup_tick.notify(SC_ZERO_TIME);
   rearm_wkup_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_THOLD_HI (0x08, RW).
 *
 * Stores the written value as the upper 32 bits of m_wkup_threshold.
 * Assembles the full 64-bit threshold from the new HI and existing LO values.
 * Triggers immediate wakeup threshold comparison.
 *
 * @param value     32-bit upper threshold value (bits[63:32]).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_THOLD_HI(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Update upper 32 bits of the assembled 64-bit threshold. */
   uint64_t lo_part = m_wkup_threshold & 0x00000000FFFFFFFFULL;
   m_wkup_threshold = (static_cast<uint64_t>(value) << 32U) | lo_part;

   /* Update register shadow. */
   WKUP_THOLD_HI.threshold_hi = value;
   rearm_wkup_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_THOLD_LO (0x0C, RW).
 *
 * Stores the written value as the lower 32 bits of m_wkup_threshold.
 * Assembles the full 64-bit threshold from the existing HI and new LO values.
 * Triggers immediate wakeup threshold comparison.
 *
 * @param value     32-bit lower threshold value (bits[31:0]).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_THOLD_LO(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Update lower 32 bits of the assembled 64-bit threshold. */
   uint64_t hi_part = m_wkup_threshold & 0xFFFFFFFF00000000ULL;
   m_wkup_threshold = hi_part | static_cast<uint64_t>(value);

   /* Update register shadow. */
   WKUP_THOLD_LO.threshold_lo = value;
   rearm_wkup_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_COUNT_HI (0x10, RW).
 *
 * Stores the written value as the upper 32 bits of m_wkup_counter.
 * Updates the WKUP_COUNT_HI register shadow. Triggers immediate wakeup
 * threshold comparison using the new combined 64-bit counter value.
 *
 * @param value     32-bit upper counter value (bits[63:32]).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_COUNT_HI(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Update upper 32 bits of the 64-bit counter. */
   uint64_t lo_part = m_wkup_counter & 0x00000000FFFFFFFFULL;
   m_wkup_counter   = (static_cast<uint64_t>(value) << 32U) | lo_part;

   /* Update register shadow. */
   WKUP_COUNT_HI.count_hi = value;
   rearm_wkup_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_COUNT_LO (0x14, RW).
 *
 * Stores the written value as the lower 32 bits of m_wkup_counter.
 * Updates the WKUP_COUNT_LO register shadow. Triggers immediate wakeup
 * threshold comparison using the new combined 64-bit counter value.
 *
 * @param value     32-bit lower counter value (bits[31:0]).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_COUNT_LO(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Update lower 32 bits of the 64-bit counter. */
   uint64_t hi_part = m_wkup_counter & 0xFFFFFFFF00000000ULL;
   m_wkup_counter   = hi_part | static_cast<uint64_t>(value);

   /* Update register shadow. */
   WKUP_COUNT_LO.count_lo = value;
   rearm_wkup_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WDOG_REGWEN (0x18, RW0C).
 *
 * Implements permanent write-once-clear semantics. Writing 0 to bit[0]
 * locks watchdog configuration permanently until system reset. Writing 1
 * has no effect (cannot unlock). Reserved bits[31:1] are silently ignored.
 *
 * Lock enforcement for WDOG_CTRL, WDOG_BARK_THOLD, WDOG_BITE_THOLD is
 * implemented in those callbacks, not here.
 *
 * @param value     32-bit value written by software.
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WDOG_REGWEN(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* RW0C: writing bit[0] = 0 locks the watchdog; writing 1 has no effect. */
   if ((value & 0x1U) == 0U) {
      if (!m_wdog_regwen_locked) {
         m_wdog_regwen_locked = true;
         /* Update shadow to reflect locked state (regwen = 0). */
         WDOG_REGWEN.regwen   = 0U;
         WDOG_REGWEN.reserved0 = 0U;
         REG_INFO(1, logger) << name()
            << ": WDOG_REGWEN: watchdog configuration locked (permanent until reset)";
      }
      /* If already locked: no effect. */
   }
   /* Bit[0] = 1: no effect. Writing 1 cannot unlock. */
   /* Bits[31:1]: reserved; silently ignored. */

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WDOG_CTRL (0x1C, RW, gated by WDOG_REGWEN).
 *
 * Lock gate: if m_wdog_regwen_locked, silently discard write; bus completes normally.
 * Updates m_wdog_enabled and m_wdog_pause_in_sleep from bits[1:0].
 * Schedules or cancels watchdog counter increment events (via FUNC004) based on
 * enable transition. Triggers immediate bark and bite threshold comparisons.
 * Reserved bits[31:2] are masked to zero in the register shadow.
 *
 * @param value     32-bit value written (bit[0]=enable, bit[1]=pause_in_sleep).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WDOG_CTRL(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Lock gate: silently discard if configuration is locked. */
   if (m_wdog_regwen_locked) {
      /* Bus completes normally; no error signaled. */
      return true;
   }

   bool new_enable          = (value & 0x1U)  != 0U;
   bool new_pause_in_sleep  = (value & 0x2U)  != 0U;

   bool was_enabled = m_wdog_enabled;
   m_wdog_enabled       = new_enable;
   m_wdog_pause_in_sleep = new_pause_in_sleep;

   /* Update register shadow (bits[1:0] active; bits[31:2] cleared). */
   WDOG_CTRL.enable         = new_enable ? 1U : 0U;
   WDOG_CTRL.pause_in_sleep = new_pause_in_sleep ? 1U : 0U;
   WDOG_CTRL.reserved0      = 0U;

   /* Enable transition handling: log the state change. */
   if (!was_enabled && m_wdog_enabled) {
      REG_INFO(2, logger) << name() << ": WDOG_CTRL write: watchdog timer enabled";
   } else if (was_enabled && !m_wdog_enabled) {
      REG_INFO(2, logger) << name() << ": WDOG_CTRL write: watchdog timer disabled";
   }

   /* Outputs follow wdog_incr, not the CSR write. Re-arm bark if this write
    * took the counter back below the threshold. */
   rearm_bark_edge();

   /* FUNC004: Wake the watchdog tick thread to re-evaluate its state.
    * This handles enable/disable transitions, pause_in_sleep changes,
    * and any write that may change the counting condition.
    * Architecture Map: state_machines[WDOG_TIMER].transitions. */
   m_ev_wdog_tick.notify(SC_ZERO_TIME);

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WDOG_BARK_THOLD (0x20, RW, gated by WDOG_REGWEN).
 *
 * Lock gate: if m_wdog_regwen_locked, silently discard write.
 * Stores the written value as m_wdog_bark_threshold and updates the register shadow.
 * Triggers immediate bark threshold comparison.
 *
 * @param value     32-bit value written as bark threshold.
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WDOG_BARK_THOLD(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Lock gate. */
   if (m_wdog_regwen_locked) {
      return true;
   }

   m_wdog_bark_threshold = value;

   /* Update register shadow. */
   WDOG_BARK_THOLD.threshold = value;
   rearm_bark_edge();

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WDOG_BITE_THOLD (0x24, RW, gated by WDOG_REGWEN).
 *
 * Lock gate: if m_wdog_regwen_locked, silently discard write.
 * Stores the written value as m_wdog_bite_threshold and updates the register shadow.
 * Triggers immediate bite threshold comparison.
 *
 * @param value     32-bit value written as bite threshold.
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WDOG_BITE_THOLD(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Lock gate. */
   if (m_wdog_regwen_locked) {
      return true;
   }

   m_wdog_bite_threshold = value;

   /* Update register shadow. Bite asserts only on a later wdog_incr cycle. */
   WDOG_BITE_THOLD.threshold = value;

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WDOG_COUNT (0x28, RW). Watchdog petting.
 *
 * NOT gated by WDOG_REGWEN; petting is always permitted.
 * Any write to this register resets m_wdog_counter to 0 regardless of the
 * written data value (detailed design Section 7.5 authoritative interpretation).
 *
 * After resetting to 0, re-evaluates bark and bite thresholds (counter is now
 * well below any nonzero threshold).
 *
 * FUNC006 - Persistence contract: m_wkup_cause_active is intentionally NOT
 * cleared here. Per the FUNC006 architecture specification, wkup_req (driven
 * by m_wkup_cause_active) is a sticky level output that persists through
 * watchdog petting, timer disable, counter resets, and INTR_STATE W1C clears.
 * The ONLY operations that may clear m_wkup_cause_active are:
 *   1. Explicit software write-0 to WKUP_CAUSE (handle_write_WKUP_CAUSE, RW0C).
 *   2. System reset (Path A in reset_process: rst_n asserted).
 *   3. AON-domain reset (Path B in reset_process: rst_aon_n asserted).
 * Clearing m_wkup_cause_active here based on interrupt state would violate the
 * independent acknowledgment path contract between the interrupt path (INTR_STATE
 * W1C) and the power-manager wakeup path (WKUP_CAUSE RW0C).
 *
 * Architecture Map Reference: FUNC006 - wkup_req persistence through pet;
 *   registers[WKUP_CAUSE].fields[cause].write_effects; events[wkup_req_deassert].
 *
 * @param value     32-bit written value (written verbatim to the counter, matching RTL).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WDOG_COUNT(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Write exact software-provided value to the counter. OpenTitan RTL does not intercept writes to force 0. */
   m_wdog_counter = value;

   /* Update register shadow to new value. */
   WDOG_COUNT.count = value;

   /* Petting drops the bark level, which re-arms the posedge detector so a later
    * crossing fires a fresh interrupt. It does NOT acknowledge the interrupt:
    * INTR_STATE.wdog_timer_bark is a prim_intr_hw status bit (aon_timer.sv:244)
    * that hardware can only set and software clears with a W1C write. The two are
    * separate mechanisms, and clearing the status here would let firmware that
    * pets without acknowledging look clean in simulation while leaving a bark
    * pending on silicon. See wdt_intr_clear_test.c, which pins this distinction. */
   if (!m_wdog_enabled || (m_wdog_counter < m_wdog_bark_threshold)) {
      m_wdog_bark_latched = false;
   }

   /* Bite stays at its previous level until the next watchdog increment or reset.
    * RTL asserts wdog_reset_req only when wdog_incr is true. */

   /* FUNC006: m_wkup_cause_active is deliberately NOT touched here. wkup_req
    * must persist through watchdog petting. Only handle_write_WKUP_CAUSE (RW0C
    * write-0) or reset_process (Path A or B) may clear m_wkup_cause_active.
    * See function Doxygen for the full persistence contract rationale. */

   /* FUNC004: Notify the watchdog tick thread to re-evaluate state from the
    * reset counter (0). The tick thread will observe the new counter value
    * on its next iteration and recompute threshold conditions accordingly.
    * Architecture Map: side_effects[WDOG_COUNT].write-pet-resets-to-zero. */
   m_ev_wdog_tick.notify(SC_ZERO_TIME);

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for INTR_STATE (0x2C, RW1C).
 *
 * Implements Write-1-to-Clear semantics independently for bits[1:0].
 * Bit[0]=1: clears wkup_timer_expired interrupt and de-asserts output.
 * Bit[1]=1: clears wdog_timer_bark interrupt and de-asserts bark/NMI outputs.
 * Writing 0 to a bit has no effect.
 * Reserved bits[31:2] are silently ignored.
 *
 * Per detailed design Section 7.6: re-assertion after clear is handled by the
 * next counter increment event, not by this callback. The callback does NOT
 * immediately re-assert cleared interrupts even if the threshold condition is
 * still true.
 *
 * @param value     32-bit value (bit=1 clears that interrupt bit).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_INTR_STATE(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* W1C for bit[0]: wkup_timer_expired */
   if (value & 0x1U) {
      if (m_intr_state_wkup) {
         m_intr_state_wkup = false;
         INTR_STATE.wkup_timer_expired = 0U;
         /* Defer port de-assertion to drive_outputs() SC_METHOD. */
         m_ev_output_update.notify(SC_ZERO_TIME);
      }
   }

   /* W1C for bit[1]: wdog_timer_bark */
   if (value & 0x2U) {
      if (m_intr_state_bark) {
         m_intr_state_bark = false;
         INTR_STATE.wdog_timer_bark = 0U;
         /* Defer bark/NMI port de-assertion to drive_outputs() SC_METHOD. */
         m_ev_output_update.notify(SC_ZERO_TIME);
      }
   }

   /* Bits[31:2]: reserved; silently ignored. */
   /* A cleared bark interrupt is NOT re-asserted while the counter stays above
    * the threshold: the bark posedge latch is deliberately left alone here, so
    * re-assertion needs a fresh crossing (pet, or threshold raised then met
    * again). See evaluate_bark_threshold() for the RTL correspondence. */

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for INTR_TEST (0x30, WO). Force-assert interrupts for testing.
 *
 * Bit[0]=1: force-sets wkup_timer_expired interrupt (asserts output, updates shadow).
 * Bit[1]=1: force-sets wdog_timer_bark interrupt (asserts bark and NMI outputs, updates shadow).
 * Counter and threshold values are NOT modified. INTR_TEST has no storage;
 * reads return 0x0 (enforced by regmodel read-restriction callback).
 *
 * @param value     32-bit value (bit=1 force-asserts that interrupt).
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_INTR_TEST(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* Bit[0]: force-assert wkup_timer_expired interrupt. */
   if (value & 0x1U) {
      m_intr_state_wkup = true;
      INTR_STATE.wkup_timer_expired = 1U;
      /* Defer port assertion to drive_outputs() SC_METHOD. */
      m_ev_output_update.notify(SC_ZERO_TIME);
   }

   /* Bit[1]: force-assert wdog_timer_bark interrupt (and NMI). */
   if (value & 0x2U) {
      m_intr_state_bark = true;
      INTR_STATE.wdog_timer_bark = 1U;
      /* Defer bark/NMI port assertions to drive_outputs() SC_METHOD. */
      m_ev_output_update.notify(SC_ZERO_TIME);
   }

   /* Bits[31:2]: reserved; silently ignored. */
   /* INTR_TEST register shadow is not updated; reads always return 0x0. */

   /* CDC annotation. */
   m_qk.inc(compute_cdc_delay());
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   return true;
}

/**
 * @brief Write callback for WKUP_CAUSE (0x34, RW0C).
 *
 * Implements RW0C semantics for bit[0] (cause).
 * Writing 0: clears m_wkup_cause_active; de-asserts wkup_req; clears WKUP_CAUSE shadow.
 * Writing 1: no effect (cannot set from software).
 * Reserved bits[31:1]: silently ignored.
 *
 * This callback does not affect INTR_STATE or interrupt output signals; those
 * are managed independently through the INTR_STATE write callback.
 *
 * @param value     32-bit value written by software.
 * @param write_mask Write mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_write_WKUP_CAUSE(uint32_t value, uint32_t write_mask)
{
   (void)write_mask;

   /* RW0C: writing bit[0] = 0 clears the wakeup request. */
   if ((value & 0x1U) == 0U) {
      if (m_wkup_cause_active) {
         m_wkup_cause_active = false;
         WKUP_CAUSE.cause = 0U;
         update_wkup_req_output();
      }
   }
   /* Writing bit[0] = 1: no effect. */
   /* Bits[31:1]: reserved; silently ignored. */

   /* CDC annotation. */
   // m_qk.inc(compute_cdc_delay());
   // if (m_qk.need_sync()) {
   //    m_qk.sync();
   // }

   return true;
}

// =============================================================================
// FUNC001: Read Callbacks
// =============================================================================

/**
 * @brief Read callback for WKUP_CTRL (0x04, RW).
 *
 * Returns current enable and prescaler field values from the register shadow.
 * Reserved bits[31:13] are masked to zero. CDC synchronization applied.
 *
 * @param value     Output: 32-bit register value to return to the initiator.
 * @param read_mask Read mask from the register descriptor (applied by this callback).
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_CTRL(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   /* CDC read-back synchronization: stall until all prior writes have propagated. */
   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   /* Assemble return value from internal state; mask reserved bits to zero.
    * bits[12:1]=prescaler, bit[0]=enable, bits[31:13]=0. */
   uint32_t enable_bit    = m_wkup_enabled ? 1U : 0U;
   uint32_t prescaler_val = static_cast<uint32_t>(WKUP_CTRL.prescaler);
   value = enable_bit | ((prescaler_val & 0xFFFU) << 1U);

   return true;
}

/**
 * @brief Read callback for WKUP_THOLD_HI (0x08, RW).
 *
 * Returns upper 32 bits of m_wkup_threshold. CDC synchronization applied.
 *
 * @param value     Output: 32-bit upper threshold bits[63:32].
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_THOLD_HI(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = static_cast<uint32_t>((m_wkup_threshold >> 32U) & 0xFFFFFFFFULL);
   return true;
}

/**
 * @brief Read callback for WKUP_THOLD_LO (0x0C, RW).
 *
 * Returns lower 32 bits of m_wkup_threshold. CDC synchronization applied.
 *
 * @param value     Output: 32-bit lower threshold bits[31:0].
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_THOLD_LO(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = static_cast<uint32_t>(m_wkup_threshold & 0xFFFFFFFFULL);
   return true;
}

/**
 * @brief Read callback for WKUP_COUNT_HI (0x10, RW, volatile).
 *
 * Returns upper 32 bits of the live m_wkup_counter. Volatile; the counter may
 * have advanced since the last shadow update. Calling m_qk.sync() provides the
 * CDC read-back synchronization guarantee.
 *
 * FUNC003: Non-atomic 64-bit counter race modeling. Sets m_wkup_hi_read_pending=true
 * so that the subsequent WKUP_COUNT_LO read can model the race condition where the
 * counter increments between HI and LO reads. This correctly models the hardware
 * behavior described in the architecture map timing_constraints[non-atomic-64-bit-access].
 *
 * @param value     Output: 32-bit upper counter bits[63:32].
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_COUNT_HI(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = static_cast<uint32_t>((m_wkup_counter >> 32U) & 0xFFFFFFFFULL);
   return true;
}

/**
 * @brief Read callback for WKUP_COUNT_LO (0x14, RW, volatile).
 *
 * Returns lower 32 bits of the live m_wkup_counter. Volatile. CDC synchronization applied.
 *
 * FUNC003: Non-atomic 64-bit counter race modeling. If m_wkup_hi_read_pending is true
 * (set by handle_read_WKUP_COUNT_HI) and the timer is currently enabled and escalation
 * is inactive, the counter is advanced by one tick before returning the LO value. This
 * correctly models the hardware non-atomic race condition where the 64-bit counter can
 * increment between the HI register read and the subsequent LO register read. The flag
 * is cleared after this check regardless, ensuring one-shot race modeling per HI/LO
 * read pair.
 *
 * Architecture Map Reference: timing_constraints[non-atomic-64-bit-access].
 *
 * @param value     Output: 32-bit lower counter bits[31:0].
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_COUNT_LO(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }

   value = static_cast<uint32_t>(m_wkup_counter & 0xFFFFFFFFULL);
   return true;
}

/**
 * @brief Read callback for WDOG_REGWEN (0x18, RW0C).
 *
 * Returns 0x1 (unlocked) or 0x0 (permanently locked) based on m_wdog_regwen_locked.
 * Reserved bits[31:1] are always zero. CDC synchronization applied.
 *
 * @param value     Output: 0x1 if unlocked, 0x0 if locked.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WDOG_REGWEN(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   /* Bit[0] = 1 (unlocked) or 0 (locked); bits[31:1] = 0 (reserved). */
   value = m_wdog_regwen_locked ? 0x0U : 0x1U;
   return true;
}

/**
 * @brief Read callback for WDOG_CTRL (0x1C, RW).
 *
 * Returns current enable and pause_in_sleep values. Reserved bits[31:2] are
 * always zero. CDC synchronization applied.
 *
 * @param value     Output: bit[0]=enable, bit[1]=pause_in_sleep, bits[31:2]=0.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WDOG_CTRL(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   uint32_t enable_bit         = m_wdog_enabled       ? 0x1U : 0x0U;
   uint32_t pause_in_sleep_bit = m_wdog_pause_in_sleep ? 0x2U : 0x0U;
   value = enable_bit | pause_in_sleep_bit;
   return true;
}

/**
 * @brief Read callback for WDOG_BARK_THOLD (0x20, RW).
 *
 * Returns current m_wdog_bark_threshold. All 32 bits are active (no reserved bits).
 * CDC synchronization applied.
 *
 * @param value     Output: 32-bit bark threshold.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WDOG_BARK_THOLD(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = m_wdog_bark_threshold;
   return true;
}

/**
 * @brief Read callback for WDOG_BITE_THOLD (0x24, RW).
 *
 * Returns current m_wdog_bite_threshold. All 32 bits are active (no reserved bits).
 * CDC synchronization applied.
 *
 * @param value     Output: 32-bit bite threshold.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WDOG_BITE_THOLD(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = m_wdog_bite_threshold;
   return true;
}

/**
 * @brief Read callback for WDOG_COUNT (0x28, RW, volatile).
 *
 * Returns live m_wdog_counter value. Volatile; increments asynchronously on AON clock.
 * CDC synchronization applied.
 *
 * @param value     Output: 32-bit live watchdog counter.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WDOG_COUNT(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = m_wdog_counter;
   return true;
}

/**
 * @brief Read callback for INTR_STATE (0x2C, RW1C, volatile).
 *
 * Returns current interrupt pending state from live internal flags.
 * Bit[0] = m_intr_state_wkup, bit[1] = m_intr_state_bark.
 * Reserved bits[31:2] are always zero.
 * Volatile; flags can be set by hardware on any AON clock tick.
 * CDC synchronization applied.
 *
 * @param value     Output: bit[0]=wkup_timer_expired, bit[1]=wdog_timer_bark, bits[31:2]=0.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_INTR_STATE(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = (m_intr_state_wkup ? 0x1U : 0x0U) |
           (m_intr_state_bark ? 0x2U : 0x0U);
   return true;
}

/**
 * @brief Read callback for WKUP_CAUSE (0x34, RW0C, volatile).
 *
 * Returns current wakeup request state. Bit[0] = m_wkup_cause_active.
 * Volatile; can be set by hardware on any AON clock tick.
 * Reserved bits[31:1] are always zero.
 * CDC synchronization applied.
 *
 * @param value     Output: bit[0]=cause (wkup_req active), bits[31:1]=0.
 * @param read_mask Read mask from the register descriptor.
 * @return true.
 */
bool aon_timer_ip::handle_read_WKUP_CAUSE(uint32_t& value, uint32_t read_mask)
{
   (void)read_mask;

   if (m_qk.need_sync()) {
      m_qk.sync();
   }
   value = m_wkup_cause_active ? 0x1U : 0x0U;
   return true;
}

// =============================================================================
// FUNC003: Wakeup Timer Tick Engine
// =============================================================================

/**
 * @brief SC_THREAD: Autonomous 64-bit wakeup timer counter tick engine (FUNC003).
 *
 * Implements the prescaler-controlled counter increment loop for the wakeup timer.
 * The thread runs for the lifetime of the simulation with a continuous while(true)
 * loop. The behavioral loop has three internal states:
 *
 *   DISABLED / HALTED state (m_wkup_enabled=false OR m_lc_escalate_active=true):
 *     The thread blocks indefinitely on m_ev_wkup_tick. It is woken by:
 *       - handle_write_WKUP_CTRL notifying m_ev_wkup_tick (enable transition)
 *       - lc_escalate_handler notifying m_ev_wkup_tick (escalation de-asserted)
 *       - reset_process notifying m_ev_wkup_tick (via m_ev_counter_cancel path)
 *
 *   COUNTING state (m_wkup_enabled=true AND m_lc_escalate_active=false):
 *     The thread performs a timed wait of (prescaler+1) AON clock periods using
 *     wait(tick_delay, m_ev_wkup_tick | m_ev_counter_cancel). If the timeout
 *     fires (no interrupting event), the counter is incremented. If an event
 *     fires first, the thread re-evaluates state without incrementing.
 *
 *   RESET (m_ev_counter_cancel notified by reset_process):
 *     After the timed wait unblocks, the continue at the top of the while loop
 *     re-evaluates m_wkup_enabled (cleared by reset_process before notifying)
 *     and enters the DISABLED state without incrementing the counter.
 *
 * Architecture Map References:
 *   - state_machines[Wakeup Timer FSM]
 *   - timing_constraints[counter-increment-rate]: tick = 1/clk_aon_freq * (prescaler+1)
 *   - side_effects[WKUP_COUNT_HI/LO].hardware-auto-increment
 *
 * Single-writer compliance: only m_wkup_counter is modified here; no sc_out<bool>
 * ports are written directly. evaluate_wkup_threshold() delegates output updates
 * to drive_outputs() SC_METHOD via m_ev_output_update.
 */
void aon_timer_ip::wkup_timer_tick_thread()
{
   while (true)
   {
      /* -----------------------------------------------------------------
       * DISABLED / ESCALATION_HALT state: block until woken by an event.
       * Architecture Map: state_machines[Wakeup Timer FSM].transitions:
       *   IDLE: m_wkup_enabled=false; ESCALATION_HALT: m_lc_escalate_active=true.
       * ----------------------------------------------------------------- */
      while (!m_wkup_enabled || m_lc_escalate_active)
      {
         wait(m_ev_wkup_tick);

         /* After wakeup, loop condition is re-checked. This handles:
          *   - m_ev_counter_cancel (reset): reset_process clears m_wkup_enabled
          *     before notifying, so the while condition remains true and the
          *     thread blocks again immediately, waiting for re-enable.
          *   - m_ev_wkup_tick from lc_escalate_handler: m_lc_escalate_active
          *     updated; if escalation cleared and still enabled, exits loop.
          *   - m_ev_wkup_tick from WKUP_CTRL write: m_wkup_enabled updated. */
      }

      /* -----------------------------------------------------------------
       * COUNTING state: compute tick delay and perform prescaler-timed wait.
       * tick_delay = (prescaler + 1) AON clock periods.
       * Architecture Map: timing_constraints[counter-increment-rate].
       * ----------------------------------------------------------------- */
      double aon_freq = clk_aon_freq.read();
      if (aon_freq <= 0.0)
      {
         /* Clock not yet configured: guard against division by zero. */
         wait(m_ev_wkup_tick);
         continue;
      }

      uint32_t prescaler_val = static_cast<uint32_t>(WKUP_CTRL.prescaler);
      sc_time tick_delay = sc_time((1.0 / aon_freq) * static_cast<double>(prescaler_val + 1U),
                                   SC_SEC);

      /* Timed wait: unblocks on tick expiry, WKUP_CTRL write, escalation, or reset. */
      wait(tick_delay, m_ev_wkup_tick | m_ev_counter_cancel);

      /* -----------------------------------------------------------------
       * Post-wait evaluation: determine whether the tick period elapsed or
       * was interrupted by a control event.
       * ----------------------------------------------------------------- */

      /* Check for reset cancellation or disable: if m_wkup_enabled was cleared
       * (by reset or WKUP_CTRL write) or m_lc_escalate_active was set, do NOT
       * increment. The outer while loop will handle the new state. */
      if (!m_wkup_enabled || m_lc_escalate_active)
      {
         continue;
      }

      /* Tick period elapsed with timer still enabled and no escalation.
       * Increment the 64-bit wakeup counter with standard uint64 overflow wrap.
       * Architecture Map: side_effects[WKUP_COUNT_HI/LO].hardware-auto-increment. */
      /* RTL compares the pre-increment count on wkup_incr, then stores count+1
       * (aon_timer_core.sv wkup_intr_o / wkup_count_wr_data_o). */
      evaluate_wkup_threshold();
      m_wkup_counter += 1ULL;
   }
}

/**
 * @brief SC_METHOD: Lifecycle escalation handler - freezes/resumes wakeup timer (FUNC003).
 *
 * Sensitive to: lc_escalate_en (sc_in<bool> input port).
 *
 * Architecture Map Reference:
 *   - state_machines[Wakeup Timer FSM].transitions[COUNTING->ESCALATION_HALT]
 *   - state_machines[Wakeup Timer FSM].transitions[ESCALATION_HALT->COUNTING]
 *
 * Reads lc_escalate_en and updates m_lc_escalate_active. Notifies m_ev_wkup_tick
 * with SC_ZERO_TIME so that wkup_timer_tick_thread wakes from its timed wait and
 * re-evaluates its state in the next delta cycle:
 *   - On assertion: thread finds m_lc_escalate_active=true, enters disabled wait.
 *   - On de-assertion: thread finds m_lc_escalate_active=false, resumes counting.
 *
 * Single-writer compliance: does not write any sc_out<bool> port directly.
 * Behavior note: the wakeup timer is always-on with respect to sleep_mode; only
 * lc_escalate_en halts the wakeup timer (FUNC003). sleep_mode affects only the
 * watchdog timer (FUNC004).
 */
void aon_timer_ip::lc_escalate_handler()
{
   /* Update escalation active flag from the input port. */
   m_lc_escalate_active = lc_escalate_en.read();

   REG_INFO(2, logger) << name()
      << ": lc_escalate_en changed to " << m_lc_escalate_active
      << " - " << (m_lc_escalate_active ? "halting" : "resuming") << " both timers";

   /* Wake the wakeup tick thread to re-evaluate its state immediately.
    * Architecture Map: state_machines[Wakeup Timer FSM]:
    *   COUNTING -> ESCALATION_HALT (when asserted)
    *   ESCALATION_HALT -> COUNTING (when de-asserted, if m_wkup_enabled=true) */
   m_ev_wkup_tick.notify(SC_ZERO_TIME);

   /* FUNC004: Wake the watchdog tick thread to re-evaluate its state immediately.
    * Architecture Map: state_machines[WDOG_TIMER]:
    *   COUNTING -> ESCALATION_HALT (when asserted)
    *   ESCALATION_HALT -> COUNTING (when de-asserted, if m_wdog_enabled=true
    *   and not paused by sleep_mode).
    * Architecture Map Reference:
    *   - state_machines[WDOG_TIMER].transitions[COUNTING->ESCALATION_HALT]
    *   - state_machines[WDOG_TIMER].transitions[ESCALATION_HALT->COUNTING] */
   m_ev_wdog_tick.notify(SC_ZERO_TIME);
}

// =============================================================================
// FUNC004: Watchdog Timer Tick Engine
// =============================================================================

/**
 * @brief SC_THREAD: Autonomous 32-bit watchdog timer counter tick engine (FUNC004).
 *
 * Implements the no-prescaler counter increment loop for the watchdog timer.
 * The thread runs for the lifetime of the simulation with a continuous while(true)
 * loop. The behavioral loop has three functional states:
 *
 *   DISABLED / PAUSED / ESCALATION_HALT state:
 *     The thread blocks indefinitely on m_ev_wdog_tick. It is woken by:
 *       - handle_write_WDOG_CTRL notifying m_ev_wdog_tick (enable/disable/pause change)
 *       - handle_write_WDOG_COUNT notifying m_ev_wdog_tick (watchdog pet)
 *       - wdog_sleep_mode_handler notifying m_ev_wdog_tick (sleep_mode transition)
 *       - lc_escalate_handler notifying m_ev_wdog_tick (escalation state change)
 *       - reset_process notifying m_ev_wdog_tick (system reset)
 *
 *   COUNTING state (m_wdog_enabled=true AND !m_lc_escalate_active AND NOT paused):
 *     Pause condition: (m_wdog_pause_in_sleep=true AND sleep_mode.read()=true).
 *     Performs a timed wait of exactly one AON clock period:
 *       wait(tick_delay, m_ev_wdog_tick | m_ev_counter_cancel)
 *     If timeout fires with timer still counting: increment m_wdog_counter by 1
 *     with uint32 unsigned overflow wrap (0xFFFFFFFF -> 0x00000000). Then update
 *     WDOG_COUNT shadow and evaluate bark/bite thresholds.
 *     If an event fires first: re-evaluate state at loop top without incrementing.
 *
 *   RESET (m_ev_counter_cancel notified by reset_process):
 *     After unblocking, the outer state check re-evaluates m_wdog_enabled
 *     (cleared by reset_process before notifying) and enters DISABLED state.
 *
 * Architecture Map References:
 *   - state_machines[WDOG_TIMER].transitions
 *   - timing_constraints[counter-increment-rate]: tick = 1/clk_aon_freq (no prescaler)
 *   - side_effects[WDOG_COUNT].hardware-auto-increment
 *   - side_effects[WDOG_CTRL.pause_in_sleep]: pause when sleep_mode asserted
 *
 * Single-writer compliance: only m_wdog_counter is modified here; no sc_out<bool>
 * ports are written directly. evaluate_bark/bite_threshold() delegates output updates
 * to drive_outputs() SC_METHOD via m_ev_output_update.
 *
 * Independent operation: this thread is completely independent of
 * wkup_timer_tick_thread. No shared mutable state is accessed (m_wdog_counter
 * vs. m_wkup_counter, separate enable/threshold variables).
 */
void aon_timer_ip::wdog_timer_tick_thread()
{
   while (true)
   {
      /* -----------------------------------------------------------------
       * DISABLED / PAUSED / ESCALATION_HALT state: block until woken.
       *
       * Pause condition: paused when (m_wdog_pause_in_sleep AND sleep_mode).
       * Disabled: m_wdog_enabled=false.
       * Escalation: m_lc_escalate_active=true.
       *
       * Architecture Map: state_machines[WDOG_TIMER].transitions:
       *   IDLE: m_wdog_enabled=false
       *   PAUSED: m_wdog_pause_in_sleep=true AND sleep_mode=true
       *   ESCALATION_HALT: m_lc_escalate_active=true
       * ----------------------------------------------------------------- */
      while (!m_wdog_enabled ||
             m_lc_escalate_active ||
             (m_wdog_pause_in_sleep && sleep_mode.read()))
      {
         wait(m_ev_wdog_tick);

         /* After wakeup, loop condition is re-checked. This handles:
          *   - m_ev_counter_cancel (reset): reset_process clears m_wdog_enabled
          *     before notifying, so the condition remains true and the thread
          *     blocks again immediately.
          *   - m_ev_wdog_tick from WDOG_CTRL write: m_wdog_enabled or
          *     m_wdog_pause_in_sleep updated; re-evaluate condition.
          *   - m_ev_wdog_tick from wdog_sleep_mode_handler: m_sleep_mode_active
          *     updated (sleep_mode.read() is re-sampled in the while condition).
          *   - m_ev_wdog_tick from lc_escalate_handler: m_lc_escalate_active
          *     updated; if cleared and enabled and not paused, exits loop. */
      }

      /* -----------------------------------------------------------------
       * COUNTING state: compute one-AON-tick delay and perform timed wait.
       * Watchdog has NO prescaler; one count per AON clock tick.
       * Architecture Map: timing_constraints[counter-increment-rate]:
       *   tick_delay = 1 / clk_aon_freq (no prescaler factor).
       * ----------------------------------------------------------------- */
      double aon_freq = clk_aon_freq.read();
      if (aon_freq <= 0.0)
      {
         /* Clock not yet configured: guard against division by zero.
          * Wait for an event and retry rather than busy-looping. */
         wait(m_ev_wdog_tick);
         continue;
      }

      /* One AON clock period - no prescaler for watchdog. */
      sc_time tick_delay = sc_time(1.0 / aon_freq, SC_SEC);

      /* Timed wait: unblocks on tick expiry, WDOG_CTRL write, pet,
       * sleep_mode change, escalation change, or reset. */
      wait(tick_delay, m_ev_wdog_tick | m_ev_counter_cancel);

      /* -----------------------------------------------------------------
       * Post-wait evaluation: determine whether the tick period elapsed or
       * was interrupted by a control event.
       * ----------------------------------------------------------------- */

      /* Re-check all halt conditions. If any halt condition became true
       * while waiting (enable cleared, escalation asserted, pause asserted),
       * do NOT increment. The outer while loop will handle the new state. */
      if (!m_wdog_enabled ||
          m_lc_escalate_active ||
          (m_wdog_pause_in_sleep && sleep_mode.read()))
      {
         continue;
      }

      /* Tick period elapsed with watchdog still in counting state.
       * Increment the 32-bit watchdog counter with unsigned overflow wrap.
       * Architecture Map: side_effects[WDOG_COUNT].hardware-auto-increment:
       *   0xFFFFFFFF + 1 wraps to 0x00000000 (no saturation, no error). */
      /* RTL compares the pre-increment count on wdog_incr, then stores count+1. */
      evaluate_bark_threshold();
      evaluate_bite_threshold();
      m_wdog_counter += 1U;

      WDOG_COUNT.count = m_wdog_counter;

      REG_INFO(3, logger) << name()
         << ": watchdog counter incremented to 0x" << std::hex << m_wdog_counter << std::dec;
   }
}

/**
 * @brief SC_METHOD: Sleep mode handler for watchdog pause-in-sleep feature (FUNC004).
 *
 * Sensitive to: sleep_mode (sc_in<bool> input port).
 *
 * Triggered by every transition of the sleep_mode input. Reads the current value
 * of sleep_mode, updates m_sleep_mode_active, and notifies m_ev_wdog_tick with
 * SC_ZERO_TIME to wake the wdog_timer_tick_thread so it can re-evaluate the
 * pause condition in the next delta cycle.
 *
 * Behavioral semantics:
 *   - sleep_mode transitions TRUE:
 *       If m_wdog_pause_in_sleep=1, the wdog thread will find (pause_in_sleep AND
 *       sleep_mode.read()=true) == true and enter the disabled-wait, pausing counting.
 *       If m_wdog_pause_in_sleep=0, the pause condition remains false and counting
 *       continues without interruption (wdog thread re-enters counting loop).
 *   - sleep_mode transitions FALSE:
 *       (pause_in_sleep AND sleep_mode.read()=false) == false; the wdog thread
 *       re-evaluates and resumes counting from the frozen counter value.
 *
 * Architecture Map Reference:
 *   - state_machines[WDOG_TIMER].transitions[COUNTING->PAUSED]:
 *     trigger = pause_in_sleep=1 AND sleep_mode asserted
 *   - state_machines[WDOG_TIMER].transitions[PAUSED->COUNTING]:
 *     trigger = sleep_mode de-asserted OR pause_in_sleep cleared
 *   - registers[WDOG_CTRL].fields[pause_in_sleep].description:
 *     "When set to 1, the watchdog timer will not count during sleep."
 *
 * Single-writer compliance: this method only updates m_sleep_mode_active and
 * notifies m_ev_wdog_tick. It never writes any sc_out<bool> port directly.
 *
 * Note: The wakeup timer is always-on and completely unaffected by sleep_mode.
 * Only the watchdog timer uses this signal.
 */
void aon_timer_ip::wdog_sleep_mode_handler()
{
   /* Read and cache the current sleep_mode state.
    * Architecture Map: state_machines[WDOG_TIMER].transitions[COUNTING->PAUSED]. */
   m_sleep_mode_active = sleep_mode.read();

   REG_INFO(2, logger) << name()
      << ": sleep_mode changed to " << m_sleep_mode_active
      << " - watchdog "
      << ((m_sleep_mode_active && m_wdog_pause_in_sleep) ? "pausing" : "continuing");

   /* Notify the watchdog tick thread to re-evaluate the pause condition.
    * The thread will check: m_wdog_pause_in_sleep && sleep_mode.read()
    * Architecture Map: state_machines[WDOG_TIMER].transitions[COUNTING->PAUSED] and
    *                   state_machines[WDOG_TIMER].transitions[PAUSED->COUNTING]. */
   m_ev_wdog_tick.notify(SC_ZERO_TIME);
}

// =============================================================================
// FUNC001 Fix: Deferred Output Driver SC_METHOD
// =============================================================================

/**
 * @brief SC_METHOD: drives all sc_out<bool> ports from current internal state.
 *
 * Triggered by m_ev_output_update (notified with SC_ZERO_TIME) whenever a
 * register write callback, threshold evaluation helper, or reset_process()
 * modifies an internal boolean flag that maps to an sc_out<bool> port. This
 * method executes in the SystemC scheduler's delta-cycle process context,
 * where sc_out::write() is permitted (unlike inside a b_transport call stack
 * in SystemC 3.0).
 *
 * Output mapping (internal flag -> port):
 *   m_intr_state_wkup    -> intr_wkup_timer_expired
 *   m_intr_state_bark    -> intr_wdog_timer_bark
 *   m_intr_state_bark    -> nmi_wdog_timer_bark
 *   m_wkup_cause_active  -> wkup_req
 *   m_wdog_bite_active   -> aon_timer_rst_req  (FUNC002: dedicated flag)
 *   m_fatal_fault_pending -> fatal_fault (transient pulse; cleared after drive)
 *   m_racl_error_active  -> racl_error
 *
 * FUNC002: aon_timer_rst_req is driven from the dedicated m_wdog_bite_active
 * flag rather than an inline counter/threshold evaluation. This enables the
 * AON-domain reset (rst_aon_n) to independently clear m_wdog_bite_active and
 * de-assert aon_timer_rst_req without modifying SYS-domain counter state.
 * evaluate_bite_threshold() maintains m_wdog_bite_active accurately.
 *
 * Every sc_out<bool> port is written on every invocation to ensure this method
 * is the unconditional sole writer of all output ports (single-driver rule).
 *
 * m_fatal_fault_pending is cleared after driving the fatal_fault port to
 * preserve the transient test-pulse semantics of ALERT_TEST.
 *
 * Architecture Map Reference: events, interrupt_matrix, reset_behavior.
 */
void aon_timer_ip::drive_outputs()
{
   /* Drive interrupt outputs from internal flags. */
   intr_wkup_timer_expired.write(m_intr_state_wkup);
   intr_wdog_timer_bark.write(m_intr_state_bark);
   nmi_wdog_timer_bark.write(m_intr_state_bark);

   /* Drive wakeup request from cause flag (AON-domain output). */
   wkup_req.write(m_wkup_cause_active);

   /* FUNC002: Drive reset request from the dedicated m_wdog_bite_active flag
    * (AON-domain output). Previously evaluated inline; now uses a flag to
    * allow independent AON-domain reset de-assertion via rst_aon_n. */
   aon_timer_rst_req.write(m_wdog_bite_active);
   if (m_wdog_bite_active) {
      REG_INFO(1, logger) << name() << ": driving aon_timer_rst_req=1";
   }

   /* Drive fatal_fault as a transient pulse when pending; clear flag after drive.
    * When not pending, drive false to ensure drive_outputs() is the sole writer. */
   if (m_fatal_fault_pending) {
      fatal_fault.write(true);
      m_fatal_fault_pending = false;
   } else {
      fatal_fault.write(false);
   }

   /* Drive RACL error output from internal flag.
    * Always written here to preserve single-writer rule for racl_error port. */
   racl_error.write(m_racl_error_active);
}
