// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file entropy_src.cpp
 * @brief entropy_src TLM model — register callback and FIFO thread
 *        implementations
 *
 * Provides concrete implementations of the eight register callbacks declared
 * in entropy_src_interface.h and the background entropy generation SC_THREAD.
 *
 * ## TLM Transport Architecture
 *
 * The TLM-2.0 `b_transport` handler for the register address space
 * is owned entirely by the regmodel framework layer.  The
 * `regmodel::Memory<32>` instance in `entropy_src_base` registers its own
 * `b_transport` with `target_socket` during elaboration via
 * `memory.bind_to_socket(target_socket)` (entropy_src_base.h).  This
 * fully satisfies the TLM-2.0 blocking-transport requirement without any
 * additional `b_transport` override in `entropy_src_ip`.
 *
 * The regmodel `b_transport` implementation:
 *  1. Extracts address, command (read/write), data pointer, and length from
 *     the `tlm_generic_payload`.
 *  2. Routes each word-aligned access to the registered read or write callback
 *     for that word offset.
 *  3. Sets `TLM_OK_RESPONSE` on the payload before returning.
 *  4. Handles storage, default values, write masks, access-type enforcement
 *     (RO, RW, WO, W1C semantics via `set_read_write_restrictions`), and
 *     reserved-bit masking for all 42 registers.
 *
 * The eight behavioural callbacks registered in the `entropy_src_ip`
 * constructor override the default regmodel storage callbacks for the seven
 * write-side-effect registers and one read-side-effect register.  All 34
 * remaining registers are served by regmodel default callbacks with no
 * additional code required here.
 *
 * ## Interrupt Bit Layout
 *
 * INTR_STATUS (0x10), INTR_ENABLE (0x14), and INTR_TEST (0x18) all use the
 * same active-bit layout (architecture-behaviour map, registers section):
 *   bit  0 : HEALTH_TEST_FAILED
 *   bit  4 : FIFO_ERROR
 *   bit  8 : FIFO_OVERFLOW
 *   bit 12 : FIFO_UNDERFLOW
 *
 * The combined mask 0x00001111 (INTR_ALL_BITS_MASK) equals the write_bit_mask
 * defined in the generated register types (entropy_src_register.h).
 *
 * ## Design Notes
 *
 *  - All INTR_STATUS manipulation uses the direct regmodel::Reg assignment operators
 *    so that the regmodel register layer keeps its internal storage consistent.
 *  - The FIFO queue (m_fifo) is a std::queue<uint32_t> bounded to FIFO_DEPTH
 *    (32 entries).  All accesses happen either in the SC_THREAD or in
 *    b_transport callbacks; since both execute in the same SystemC thread
 *    context there are no concurrency hazards.
 *  - Interrupt outputs are always updated through update_interrupt_outputs(),
 *    which is the single point that reads INTR_STATUS and INTR_ENABLE and
 *    drives the sc_out<bool> port.
 *  - No `nb_transport` or DMI paths are registered.  Blocking transport only.
 *
 * @see entropy_src_base.h for the regmodel socket binding
 * @see reg_file.h for the regmodel::Memory<32>::b_transport implementation
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "entropy_src.h"
#include <openssl/rand.h>

// =============================================================================
// Internal helpers
// =============================================================================

/******************************************************************************
 * @brief Re-evaluate and drive the combined interrupt output port
 *
 * The output port is driven high when (INTR_STATUS & INTR_ENABLE) != 0,
 * i.e. at least one enabled interrupt source is pending.  Software reads
 * INTR_STATUS to determine which source fired.
 *
 * Interrupt bit layout (all three registers INTR_STATUS / INTR_ENABLE /
 * INTR_TEST share this layout — architecture-behaviour map, registers section):
 *   bit  0 : HEALTH_TEST_FAILED  (INTR_BIT_HEALTH_TEST_FAILED = 0x00000001)
 *   bit  4 : FIFO_ERROR          (INTR_BIT_FIFO_ERROR         = 0x00000010)
 *   bit  8 : FIFO_OVERFLOW       (INTR_BIT_FIFO_OVERFLOW      = 0x00000100)
 *   bit 12 : FIFO_UNDERFLOW      (INTR_BIT_FIFO_UNDERFLOW     = 0x00001000)
 *
 * The combined mask of all active interrupt bits is INTR_ALL_BITS_MASK
 * (0x00001111), which equals the INTR_ENABLE write_bit_mask (0x1111) and the
 * INTR_TEST write_bit_mask (0x1111) from entropy_src_register.h.
 ******************************************************************************/
void entropy_src_ip::update_interrupt_outputs()
{
    uint32_t status = static_cast<uint32_t>(INTR_STATUS);
    uint32_t enable = static_cast<uint32_t>(INTR_ENABLE);
    uint32_t active = status & enable;

    // Single combined interrupt output: OR of all enabled sources
    irq_o.write(active != 0u);

    REG_INFO(3, logger)
        << "update_interrupt_outputs: status=0x" << std::hex << status
        << " enable=0x" << enable << " active=0x" << active
        << " irq_o=" << (active != 0u);
}

/******************************************************************************
 * @brief SC_METHOD — sole driver of irq_o output port
 *
 * Triggered by m_interrupt_update_event. Delegates to update_interrupt_outputs()
 * to re-evaluate and write the irq_o sc_out<bool> port.
 ******************************************************************************/
void entropy_src_ip::interrupt_output_method()
{
    update_interrupt_outputs();
}

/******************************************************************************
 * @brief Update FIFO_STATUS register from current queue occupancy and pointers
 *
 * Performs a single atomic 32-bit regmodel write that encodes all three
 * FIFO_STATUS fields simultaneously, preventing software from observing an
 * intermediate state where LEVEL is inconsistent with WPTR or RPTR.
 *
 *  - bits [6:0]   LEVEL : m_fifo.size() capped at FIFO_DEPTH (32)
 *  - bits [12:8]  WPTR  : m_wptr & 0x1F (5-bit write pointer, mod-32, shifted left by 8)
 *  - bits [20:16] RPTR  : m_rptr & 0x1F (5-bit read pointer, mod-32, shifted left by 16)
 *  - bits [31:21] reserved : always zero
 *
 * The single-assignment (FIFO_STATUS = ...) uses the regmodel::Reg assignment
 * operator, which writes directly to the backing memory word without
 * triggering any registered callback (RO register — no write callback exists
 * for FIFO_STATUS).  This is the correct internal-write path that bypasses
 * regmodel mask enforcement, consistent with how all RO registers are updated by
 * the model.
 *
 * Called:
 *  - By the background entropy_generation_thread immediately after each
 *    successful push into m_fifo (WPTR has already been incremented).
 *  - By handle_read_FIFO_RDATA immediately after each successful pop from
 *    m_fifo (RPTR has already been incremented).
 *
 * Functional reference:
 *   - registers.FIFO_STATUS (offset 0x24): LEVEL[6:0], WPTR[12:8], RPTR[20:16]
 *   - atomic FIFO_STATUS write invariant
 *   - push: LEVEL and WPTR updated
 *   - pop: LEVEL and RPTR updated
 *   - description: "Each FIFO_STATUS update is written atomically
 *     (single 32-bit write combining LEVEL, WPTR, RPTR)"
 ******************************************************************************/
void entropy_src_ip::update_fifo_status()
{
    // LEVEL: current queue occupancy, saturated at FIFO_DEPTH (32).
    // m_fifo.size() can never exceed FIFO_DEPTH because the background thread
    // guards against overflow, but the cap is applied defensively.
    uint32_t level = static_cast<uint32_t>(m_fifo.size());
    if (level > FIFO_DEPTH)
    {
        level = FIFO_DEPTH;
    }

    // Encode all three fields into a single 32-bit word:
    //   bits [6:0]   = LEVEL
    //   bits [12:8]  = WPTR (5 bits, shifted left by 8)
    //   bits [20:16] = RPTR (5 bits, shifted left by 16)
    //   bits [31:21] = 0 (reserved, always zero)
    uint32_t fifo_status_val =
          (level                       & 0x7Fu)          // bits [6:0]
        | (static_cast<uint32_t>(m_wptr & 0x1Fu) << 8u)   // bits [12:8]
        | (static_cast<uint32_t>(m_rptr & 0x1Fu) << 16u);  // bits [20:16]

    FIFO_STATUS = fifo_status_val;

    REG_INFO(3, logger)
        << "update_fifo_status: LEVEL=" << std::dec << level
        << " WPTR=" << static_cast<unsigned>(m_wptr & 0x7Fu)
        << " RPTR=" << static_cast<unsigned>(m_rptr & 0x7Fu)
        << " FIFO_STATUS=0x" << std::hex << fifo_status_val;
}

// =============================================================================
// Write callbacks
// =============================================================================

/******************************************************************************
 * @brief Write callback for CTRL register (offset 0x04) — Software
 *        Reset Sequence
 *
 * Invoked by regmodel after every write to CTRL (0x04).  When CTRL.RESET (bit 0)
 * is set in the written value, this callback executes the full eight-action
 * software reset sequence.
 *
 * Writes that do NOT set CTRL[0] (e.g. writes to DOWNSAMPLE_RATE, AUTOTUNE_ENABLE,
 * BYPASS_COMPRESSOR only) are stored by regmodel before the callback fires; this
 * callback applies the write mask and returns without further action.
 *
 * ## Eight-Action Software Reset Sequence
 *
 * **Action 1 — Interrupt background thread:**
 *   Set `m_reset_in_progress = true`, then notify `m_reset_event` with
 *   SC_ZERO_TIME.  The flag is set BEFORE the notification to guarantee
 *   coherency: the background SC_THREAD checks `m_reset_in_progress` after
 *   every `wait()` return to distinguish a reset interrupt from a normal
 *   timeout, even in the edge case where both events are scheduled at the same
 *   delta cycle.
 *   Trigger "Software reset (CTRL[0]=1)".
 *
 * **Action 2 — Drain internal FIFO queue:**
 *   Pop all entries from `m_fifo` until empty.  Reset `m_wptr` and `m_rptr`
 *   to 0.  Any in-flight entropy values are permanently discarded.
 *   "FIFO cleared on reset"; FIFO_STATUS fields
 *   LEVEL[6:0], WPTR[12:8], RPTR[20:16] — all reset to 0.
 *
 * **Action 3 — Clear FIFO_STATUS register:**
 *   Write 0x00000000 directly to the FIFO_STATUS regmodel register object via the
 *   assignment operator (bypasses b_transport and callback dispatch — correct
 *   for an RO register that has no write callback).  Encodes LEVEL=0, WPTR=0,
 *   RPTR=0 atomically in a single 32-bit write.
 *   Architecture map: registers.FIFO_STATUS (0x24), reset_value 0x00000000.
 *
 * **Action 4 — Clear all 21 health test counter and status registers:**
 *   Write 0x00000000 to each of the following RO registers via regmodel direct
 *   assignment (no callback for RO registers):
 *     HEALTH_TEST_STATUS        (0x40)
 *     REPETITION_TEST_COUNT     (0x44)
 *     APT_PATTERN_COUNT_1BIT    (0x50)
 *     APT_PATTERN_COUNT_2BIT    (0x54)
 *     APT_PATTERN_COUNT_3BIT    (0x58)
 *     APT_PATTERN_COUNT_4BIT    (0x5C)
 *     MARKOV_TEST_COUNTS_0      (0x80)
 *     MARKOV_TEST_COUNTS_1      (0x84)
 *     MARKOV_TEST_PROBABILITIES (0x88)
 *     GENERATOR_0–11_HEALTH_STATUS (0xC0–0xEC, 12 registers)
 *   Registers have reset_value 0x00000000.
 *
 * **Action 5 — Clear INTR_STATUS register:**
 *   Write 0x00000000 directly to INTR_STATUS via regmodel assignment.  All four
 *   interrupt status bits (bits 0, 4, 8, 12) are cleared simultaneously,
 *   bypassing the W1C callback (which is only invoked on TLM write transactions,
 *   not on internal model writes via the assignment operator).
 *   Architecture map: registers.INTR_STATUS (0x10), reset_value 0x00000000.
 *   INTR_ENABLE (0x14) is intentionally NOT modified by reset
 *
 * **Action 6 — Re-evaluate interrupt output port:**
 *   Notify `m_interrupt_update_event` with SC_ZERO_TIME to trigger
 *   `interrupt_output_method` (the sole driver of sc_out<bool> interrupt port).
 *   Because INTR_STATUS is now 0x00000000, all port values resolve to false
 *   regardless of the current INTR_ENABLE value.
 *   Single-writer compliance: this callback does NOT call .write() on any port.
 *
 * **Action 7 — Stabilization delay (20 APB clock cycles = 100 ns):**
 *   Execute `sc_core::wait(RESET_STABILIZATION_DELAY_NS, SC_NS)` to advance
 *   simulation time by the minimum stabilization period required after reset
 *   deasserts before software can safely access registers.  This wait is legal
 *   because `b_transport` (and therefore this callback) runs in the SystemC
 *   SC_THREAD context of the TLM initiator; a blocking `wait()` inside an
 *   SC_THREAD-initiated `b_transport` call conforms to the TLM-2.0 LT model.
 *   The delay is observable in simulation time as a gap between the CTRL write
 *   and the first post-reset entropy value being available.
 *   Timing constraint: "minimum 20 APB clock cycles".
 *
 * **Action 8 — Self-clear CTRL register:**
 *   Write 0x10000000 to CTRL via regmodel assignment.  Clears the RESET bit and
 *   all other CTRL fields (DOWNSAMPLE_RATE, BYPASS_COMPRESSOR, AUTOTUNE_ENABLE)
 *   to their hardware reset defaults simultaneously.  Software polling CTRL[0]
 *   after the stabilization period will read 0x10000000, confirming completion.
 *
 * ## Thread Coordination
 *
 * `m_reset_in_progress` is set in Action 1 and cleared by the background
 * SC_THREAD inside its RESET_PENDING state, AFTER the thread has woken from
 * the `m_reset_event` notification.  The background thread re-derives
 * `m_fifo_enabled` and `m_health_test_enabled` from the regmodel register values
 * that were restored by Actions 3–8.  No second synchronisation event is
 * required because all state updates happen before the SC_ZERO_TIME
 * `m_reset_event` notification is delivered (SystemC delta-cycle semantics
 * guarantee that `.notify(SC_ZERO_TIME)` is evaluated in the next delta, after
 * the current process — this callback — finishes all its synchronous work
 * including the `wait()` stabilisation delay).
 *
 * ## Entropy Generation State
 *
 * Entropy is generated using the OpenSSL RAND_bytes() API to ensure
 * cryptographically secure data. The internal state is preserved across 
 * software resets to ensure entropy generation can resume immediately 
 * after the thread restarts.
 *
 * @param value  32-bit value written to CTRL.  The write mask (0x03FF0111) is
 *               applied by this callback before storage; reserved bits are
 *               silently discarded.
 * @return true always (callback return value is not used by regmodel for error
 *                      propagation in this model).
 ******************************************************************************/
bool entropy_src_ip::handle_write_CTRL(uint32_t value)
{
    // Apply the CTRL write mask (0x03FF0111) to silently discard writes to
    // reserved bits [31:26], [15:9], [7:5], [3:1].  The resulting value is
    // stored in the regmodel register object via the regmodel::Reg assignment operator.
    //
    // Architecturally valid writable fields within 0x03FF0111:
    //   bit [0]      RESET
    //   bit [4]      AUTOTUNE_ENABLE
    //   bit [8]      BYPASS_COMPRESSOR
    //   bits [25:16] DOWNSAMPLE_RATE
    CTRL = value & static_cast<uint32_t>(CTRL.write_bit_mask);

    // Examine the RESET bit AFTER masking so that reserved-bit writes cannot
    // falsely assert RESET.  Bit [0] is the sole trigger for the reset sequence.
    bool reset_requested = (static_cast<uint32_t>(CTRL.RESET) != 0u);

    if (!reset_requested)
    {
        // No reset: CTRL fields (DOWNSAMPLE_RATE, BYPASS_COMPRESSOR,
        // AUTOTUNE_ENABLE) have been stored above; no behavioural side-effects
        // are required for these fields beyond regmodel storage.
        REG_INFO(3, logger)
            << "CTRL write (no reset): stored 0x"
            << std::hex << static_cast<uint32_t>(CTRL);
        return true;
    }

    // =========================================================================
    // Software Reset Sequence — eight actions in architectural order
    // Architecture map: side_effects[0] CTRL.RESET auto-clear;
    //   transaction_timelines.SoftwareReset_Sequence;
    //   detailed-design.md Section 11.2
    // =========================================================================

    REG_INFO(2, logger) << "handle_write_CTRL: CTRL.RESET=1 — software reset sequence begins";

    // -------------------------------------------------------------------------
    // Action 1: Interrupt background SC_THREAD
    //
    // Set m_reset_in_progress to true BEFORE notifying m_reset_event.  The
    // background thread tests this flag after each wait() return to determine
    // whether a reset preempted the wait; without this flag, there is no
    // SystemC API call that reliably indicates which reason caused a combined
    // timed/event wait to return early.
    //
    // m_reset_event is notified with SC_ZERO_TIME so that the scheduler
    // delivers the interrupt to the background thread in the immediately
    // following delta cycle, after this callback's current synchronous
    // execution phase completes.
    // -------------------------------------------------------------------------
    m_reset_in_progress = true;
    m_reset_event.notify(sc_core::SC_ZERO_TIME);

    // -------------------------------------------------------------------------
    // Action 2: Drain internal FIFO queue and reset write/read pointers
    //
    // All entropy values currently in the FIFO are abandoned; any in-flight
    // generation is discarded.  m_wptr and m_rptr are reset to 0 so that
    // FIFO_STATUS accurately reflects the post-reset empty state (LEVEL=0,
    // WPTR=0, RPTR=0) after the register reset in Actions 3–5.
    //
    // RESET EFFECTS:
    //   - "FIFO cleared on reset: Internal FIFO queue drained to
    //     empty; wptr and rptr member variables reset to 0"
    //   - registers.FIFO_STATUS: reset_value 0x00000000
    // -------------------------------------------------------------------------
    while (!m_fifo.empty())
    {
        m_fifo.pop();
    }
    m_wptr = 0u;
    m_rptr = 0u;

    // -------------------------------------------------------------------------
    // Actions 3–5: Restore all register defaults via regmodel and preserve
    //              INTR_ENABLE
    //
    // requires that "all registers return to reset values" after a
    // software reset.  This covers:
    //   - All RO status registers (FIFO_STATUS, health counters, HEALTH_TEST_STATUS,
    //     GENERATOR_0..11_HEALTH_STATUS, INTR_STATUS) → 0x00000000
    //   - All RW configuration registers (FIFO_CTRL, HEALTH_TEST_CTRL, STARTUP_CTRL,
    //     APT_PROPORTION_*, MARKOV_TEST_PROB_THRESHOLDS, RING_OSC_*, DECORRELATOR_*,
    //     DEBUG_CTRL, CTRL, INTR_ENABLE) → their hardware reset defaults
    //
    // The mechanism is reset_all_registers() which calls .reset() on every
    // register object, restoring the regmodel-stored default value defined in each
    // register type constructor.  COMPONENT_ID.reset() is also called but its
    // reset value is 0x01000001 (the synthesis-time constant), so it is
    // effectively immune — calling .reset() on an RO register with a non-zero
    // default simply restores the same value.
    //
     // To honour the exception, the pre-reset INTR_ENABLE value
     // is saved before the call and written back immediately after.
     //
     // SIDE EFFECTS:
     //   - "All RW register fields restored to reset values by regmodel"
     //   - "all registers return to reset values"
     // -------------------------------------------------------------------------

    // Save INTR_ENABLE to preserve it across the reset_all_registers() call.
   // const uint32_t preserved_intr_enable = static_cast<uint32_t>(INTR_ENABLE);

    // Actions performed during software reset:
    // 1. Resolve pending entropy generation cycle (via m_reset_event).
    // 2. Set stabilization delay (100 ns).
    // 3. Clear all registers to reset values.
    // ------------------------------------------------------------------------------------------------------------------------------------------------

    // Restore all 42 registers to their regmodel-defined hardware reset defaults.
    // This single call covers Actions 3, 4, and 5 as described above.
    reset_all_registers();

    REG_INFO(3, logger)
        << "handle_write_CTRL: registers reset to hardware defaults.";

    // INTR_STATUS is now 0x00000000 (restored to its reset default of
    // 0x00000000 by the reset_all_registers() call above).
    // Architecture map: Action 5; registers.INTR_STATUS reset_value.

    // -------------------------------------------------------------------------
    // Action 6: Re-evaluate interrupt output port
    //
    // Notify m_interrupt_update_event to trigger interrupt_output_method (the
    // sole SC_METHOD driver of sc_out<bool> interrupt port).  Because
    // INTR_STATUS is now 0x00000000, all four port values resolve to false
    // regardless of the current INTR_ENABLE value:
    //   port_value = INTR_STATUS[bit] AND INTR_ENABLE[bit] = 0 AND x = 0
    //
    // Single-writer compliance: this callback NEVER calls .write() on any
    // sc_out<bool> port directly.  Port updates are exclusively driven by
    // interrupt_output_method in response to m_interrupt_update_event.
    //
    // SIDE EFFECTS:
    //   - all sources de-asserted on reset
    // -------------------------------------------------------------------------
    m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);

    REG_INFO(3, logger) << "handle_write_CTRL: INTR_STATUS cleared; interrupt ports de-assertion scheduled";

    // -------------------------------------------------------------------------
    // Action 7: Stabilization delay (minimum 20 APB clock cycles = 100 ns)
    //
    // Model the hardware stabilization period using the quantum keeper for
    // temporal decoupling (consistent with the project-wide LT modelling
    // pattern).  The delay is accumulated in the quantum keeper and only
    // synchronised with the SystemC kernel when the global quantum is
    // exceeded, yielding faster simulation speed than a blocking wait().
    //
    // STABILIZATION:
    //   - "minimum 20 APB clock cycles for stabilization"
    //   - "loosely-timed stabilization hold-off"
    // -------------------------------------------------------------------------
    m_qk.inc(sc_core::sc_time(RESET_STABILIZATION_DELAY_NS, sc_core::SC_NS));
    if (m_qk.need_sync())
    {
        m_qk.sync();
    }

    REG_INFO(2, logger)
        << "handle_write_CTRL: stabilization delay complete ("
        << RESET_STABILIZATION_DELAY_NS << " ns)";

    // -------------------------------------------------------------------------
    // Action 8: Self-clear CTRL register (regmodel internal write)
    //
    // Write 0x10000000u to CTRL via direct regmodel assignment.  This clears:
    //   bit [0]      RESET            → 0 (self-clear)
    //   bit [4]      AUTOTUNE_ENABLE  → 0 (reset default)
    //   bit [8]      BYPASS_COMPRESSOR→ 0 (reset default)
    //   bits [25:16] DOWNSAMPLE_RATE  → 0 (reset default)
    //
    // After this write, software polling CTRL[0] will read 0x10000000,
    // confirming the reset has completed and the peripheral is ready.
    //
    // The internal state mirrors (m_fifo_enabled, m_health_test_enabled,
    // m_startup_delay_ns) are NOT updated here.  Per they are
    // re-derived by the background SC_THREAD from the post-reset regmodel register
    // values when it processes the RESET_PENDING state.  This separation of
    // concerns ensures that the thread always reads the authoritative regmodel state
    // rather than a redundant in-memory mirror that could become stale.
    //
    // SIDE EFFECTS:
    // CTRL.RESET auto-clear to 0x10000000.
    // This ensures that a subsequent read of CTRL after the reset returns 0x10000000 as expected.
    // -------------------------------------------------------------------------
    CTRL = 0x10000000u;

    // Notify background thread that the full reset sequence is complete.
    m_reset_complete_event.notify(sc_core::SC_ZERO_TIME);

    REG_INFO(2, logger)
        << "handle_write_CTRL: software reset sequence complete — CTRL=0x"
        << std::hex << static_cast<uint32_t>(CTRL);

    return true;
}

/******************************************************************************
 * @brief Write callback for INTR_STATUS register (W1C, offset 0x10)
 *
 * Each bit set in @p value clears the corresponding INTR_STATUS bit.  The
 * stored INTR_STATUS is updated and the interrupt output port is
 * re-evaluated.
 *
 * @param value 32-bit value written to INTR_STATUS
 * @return true always
 ******************************************************************************/
bool entropy_src_ip::handle_write_INTR_STATUS(uint32_t value)
{
    uint32_t current = static_cast<uint32_t>(INTR_STATUS);

    // W1C semantics: only bits at the architecturally valid positions (0, 4, 8,
    // 12) participate in the clear operation.  Mask the written value to those
    // positions before applying the complement-AND so that reserved-bit writes
    // cannot inadvertently suppress a set interrupt bit.
    //
    // SIDE EFFECTS:
    // "write-1-clear" at fields HEALTH_TEST_FAILED (bit 0), FIFO_ERROR (bit 4),
    // FIFO_OVERFLOW (bit 8), FIFO_UNDERFLOW (bit 12).
    uint32_t clear_mask = value & INTR_ALL_BITS_MASK;
    uint32_t updated    = current & ~clear_mask;
    INTR_STATUS = updated;

    REG_INFO(3, logger)
        << "INTR_STATUS W1C: before=0x" << std::hex << current
        << " write=0x" << value
        << " clear_mask=0x" << clear_mask
        << " after=0x" << updated;

    m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);
    return true;
}

/******************************************************************************
 * @brief Write callback for INTR_ENABLE register (offset 0x14)
 *
 * Stores the new enable mask (subject to the write-mask 0x1111 defined in
 * the register type) and re-evaluates interrupt output port.
 *
 * @param value 32-bit value written to INTR_ENABLE
 * @return true always
 ******************************************************************************/
bool entropy_src_ip::handle_write_INTR_ENABLE(uint32_t value)
{
    INTR_ENABLE = value & static_cast<uint32_t>(INTR_ENABLE.write_bit_mask);

    REG_INFO(3, logger)
        << "INTR_ENABLE updated to 0x" << std::hex
        << static_cast<uint32_t>(INTR_ENABLE);

    m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);
    return true;
}

/******************************************************************************
 * @brief Write callback for INTR_TEST register (WO inject, offset 0x18)
 *
 * Sets INTR_STATUS bits corresponding to set bits in @p value (masked to
 * the four valid interrupt bits), then re-evaluates interrupt output port.
 * The INTR_TEST register value itself is not stored (write-only).
 *
 * @param value 32-bit value written to INTR_TEST
 * @return true always
 ******************************************************************************/
bool entropy_src_ip::handle_write_INTR_TEST(uint32_t value)
{
    // INTR_TEST is write-only; its active bits mirror the INTR_STATUS/INTR_ENABLE
    // layout: HEALTH_TEST_FAILED at bit 0, FIFO_ERROR at bit 4, FIFO_OVERFLOW
    // at bit 8, FIFO_UNDERFLOW at bit 12.
    //
    // Mask the written value to the architecturally valid interrupt bit positions
    // (INTR_ALL_BITS_MASK = 0x00001111) before OR-ing into INTR_STATUS.  This
    // prevents reserved-bit writes from polluting the status register.
    //
    // SIDE EFFECTS:
    // "write-inject-interrupt" at fields HEALTH_TEST_FAILED (bit 0),
    // FIFO_ERROR (bit 4), FIFO_OVERFLOW (bit 8), FIFO_UNDERFLOW (bit 12).
    uint32_t inject_mask    = value & INTR_ALL_BITS_MASK;
    uint32_t current_status = static_cast<uint32_t>(INTR_STATUS);
    INTR_STATUS = current_status | inject_mask;

    REG_INFO(3, logger)
        << "INTR_TEST inject: raw_write=0x" << std::hex << value
        << " inject_mask=0x" << inject_mask
        << " INTR_STATUS now 0x" << static_cast<uint32_t>(INTR_STATUS);

    m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);
    return true;
}

/******************************************************************************
 * @brief Write callback for FIFO_CTRL register (offset 0x20)
 *
 * If FIFO_ENABLE (bit 0) transitions from 0 to 1, the entropy generation
 * thread is notified to resume filling.  Note: Disabling the FIFO does
 * NOT drain it; data is preserved for software observability.
 *
 * @param value 32-bit value written to FIFO_CTRL
 * @return true always
 ******************************************************************************/
bool entropy_src_ip::handle_write_FIFO_CTRL(uint32_t value)
{
    bool new_enable = (value & 0x1u) != 0u;
    bool prev_enable = m_fifo_enabled;

    FIFO_CTRL = value & static_cast<uint32_t>(FIFO_CTRL.write_bit_mask);
    m_fifo_enabled = new_enable;

    // if (prev_enable && !new_enable)
    // {
    //     // FIFO disabled: drain queue and reset all FIFO state to zero.
    //     while (!m_fifo.empty())
    //     {
    //         m_fifo.pop();
    //     }
    //     m_wptr      = 0u;
    //     m_rptr      = 0u;
    //     FIFO_STATUS = 0u;
    //     REG_INFO(2, logger) << "FIFO_CTRL: FIFO disabled — FIFO drained, wptr/rptr reset";
    // }
    // else 
    if (!prev_enable && new_enable)
    {
        // FIFO re-enabled: wake the entropy generation thread.
        m_fifo_fill_event.notify(sc_core::SC_ZERO_TIME);
        REG_INFO(2, logger) << "FIFO_CTRL: FIFO enabled — entropy thread notified";
    }

    return true;
}

/******************************************************************************
 * @brief Write callback for HEALTH_TEST_CTRL register (offset 0x30)
 *
 * Stores the new value subject to the write mask (0x0000FFFF) and updates
 * the internal `m_health_test_enabled` mirror flag that gates all health
 * test counter increments inside the background SC_THREAD.
 *
 * ## ENABLE field semantics
 *
 * HEALTH_TEST_CTRL[7:0] is the ENABLE field.  The field is an 8-bit bitmask
 * where each bit enables an independent test category (bit 0 = repetition
 * test, bit 1 = APT, bit 2 = Markov, etc., per the architecture map).  At
 * the TLM abstraction level the individual bit semantics are not distinguished;
 * any non-zero ENABLE value activates all modelled counter types.  Writing
 * ENABLE = 0x00 disables all counters.
 *
 * REPETITION_LIMIT field (bits [15:8]) is pure configuration storage; it is
 * retained in regmodel and readable by software but is not compared against any
 * counter value in the TLM abstraction.
 *
 * Architecture map reference:
 *   - registers.HEALTH_TEST_CTRL.fields.ENABLE (bits 7:0): "ENABLE[7:0] != 0"
 *     activates the HealthTestCounterControl COUNTING state.
 *   - side_effects: HEALTH_TEST_CTRL.ENABLE enable-disable-counter-update:
 *     "Writing ENABLE=0x00 stops all health test counter increments."
 *   - state_machines[2]: HealthTestCounterControl STOPPED→COUNTING:
 *     "HEALTH_TEST_CTRL.ENABLE written to non-zero value"
 *   - description: "All counter increments stop when
 *     HEALTH_TEST_CTRL[7:0] (ENABLE) is written to 0x00."
 *
 * @param value  32-bit value written to HEALTH_TEST_CTRL.  Write mask
 *               0x0000FFFF is applied by the callback before storage.
 * @return true always (callback return value is not used by regmodel for error
 *                      propagation in this model)
 ******************************************************************************/
bool entropy_src_ip::handle_write_HEALTH_TEST_CTRL(uint32_t value)
{
    // Mask to the architecturally writable bits (0x0000FFFF) before storage.
    // This is consistent with the HEALTH_TEST_CTRL_type constructor which
    // specifies write_bit_mask = 0x0000FFFF.
    //
    // Note: some regmodel versions auto-apply the write mask before calling the
    // callback; writing the masked value here is therefore idempotent and safe.
    HEALTH_TEST_CTRL = value & static_cast<uint32_t>(HEALTH_TEST_CTRL.write_bit_mask);

    // Derive the enable state from bits [7:0] (ENABLE field).
    // Any non-zero ENABLE value transitions the HealthTestCounterControl state
    // machine from STOPPED to COUNTING.  ENABLE = 0x00 transitions it from
    // COUNTING to STOPPED.
    //
    // ENABLE/DISABLE COUNTER UPDATE:
    // Writing ENABLE=0x00 stops all health test counter increments..
    //
    // Architecture map: state_machines[2] HealthTestCounterControl,
    // transitions STOPPED→COUNTING and COUNTING→STOPPED.
    uint32_t enable_field = static_cast<uint32_t>(HEALTH_TEST_CTRL.ENABLE);
    m_health_test_enabled = (enable_field != 0u);

    REG_INFO(3, logger)
        << "HEALTH_TEST_CTRL write: raw=0x" << std::hex << value
        << " stored=0x" << static_cast<uint32_t>(HEALTH_TEST_CTRL)
        << " ENABLE_field=0x" << enable_field
        << " health_test_enabled=" << std::boolalpha << m_health_test_enabled;

    return true;
}

/******************************************************************************
 * @brief Write callback for STARTUP_CTRL register (offset 0xB0)
 *
 * Captures STARTUP_DELAY from bits [15:0] of @p value into
 * m_startup_delay_ns.
 *
 * @param value 32-bit value written to STARTUP_CTRL
 * @return true always
 ******************************************************************************/
bool entropy_src_ip::handle_write_STARTUP_CTRL(uint32_t value)
{
    m_startup_delay_ns = value & 0xFFFFu;
    STARTUP_CTRL = value & static_cast<uint32_t>(STARTUP_CTRL.write_bit_mask);

    REG_INFO(3, logger)
        << "STARTUP_CTRL: startup_delay=" << m_startup_delay_ns << " ns";

    return true;
}

/******************************************************************************
 * @brief Write callback for RING_OSC_ENABLE register (offset 0x90)
 *
 * Models the startup health-test gate that MAIN_SM_STATUS.BOOT_PHASE_DONE
 * reports. Firmware brings the entropy source up in two steps -- configure with
 * the ring-oscillator generators OFF, then enable them -- and then polls
 * BOOT_PHASE_DONE to learn that the startup window has passed and entropy is
 * reaching the whitener/FIFO. The SEP boot ROM gates its EDN enable on exactly
 * that bit, so without it the ROM waits forever and stops secure boot.
 *
 * The model asserts BOOT_PHASE_DONE as soon as at least one generator is enabled
 * in RING_OSC_ENABLE, and clears IDLE to match. That is the only condition: it
 * is NOT additionally gated on CTRL.MODULE_ENABLE, which this model's CTRL does
 * not implement (see the comment on the gate below). It does NOT model the
 * health tests themselves either: this is a functional model of the handshake
 * firmware observes, not of the analog startup behaviour. A test that needs a
 * startup FAILURE should drive ALERT/ERR through the health-test path rather
 * than expect this gate to withhold BOOT_PHASE_DONE.
 *
 * @param value 32-bit value written to RING_OSC_ENABLE
 * @return true (write always accepted)
 ******************************************************************************/
bool entropy_src_ip::handle_write_RING_OSC_ENABLE(uint32_t value)
{
    RING_OSC_ENABLE = value & static_cast<uint32_t>(RING_OSC_ENABLE.write_bit_mask);

    // Gated on the generators alone, NOT on CTRL.MODULE_ENABLE: this model's CTRL
    // does not implement that field. Its bit 0 is RESET and bits 1-3 are
    // reserved, whereas the RDL defines bit 0 as reserved and bit 1 as
    // MODULE_ENABLE (reset 1). That drift is pre-existing and left alone here --
    // changing CTRL's layout would alter the software-reset behaviour existing
    // tests rely on. It is harmless for this gate: firmware cannot usefully run
    // the generators without the module enabled anyway.
    const bool generators_on = (static_cast<uint32_t>(RING_OSC_ENABLE.ENABLE) != 0u);

    if (generators_on) {
        if (static_cast<uint32_t>(MAIN_SM_STATUS.BOOT_PHASE_DONE) == 0u) {
            MAIN_SM_STATUS.BOOT_PHASE_DONE = 1;
            MAIN_SM_STATUS.IDLE = 0;
            REG_INFO(3, logger)
                << "RING_OSC_ENABLE: generators enabled -- "
                << "MAIN_SM_STATUS.BOOT_PHASE_DONE asserted";
        }
    }

    return true;
}

// =============================================================================
// Read callbacks
// =============================================================================

/******************************************************************************
 * @brief Read callback for FIFO_RDATA register (destructive pop, offset 0x28)
 *
 * Pops the head entry from m_fifo into @p value.  If the FIFO is empty,
 * sets @p value to 0, asserts INTR_STATUS.fifo_underflow (bit 3), and
 * re-evaluates interrupt outputs.
 *
 * @param value Reference populated with the popped entropy word (0 on underflow)
 * @return true on successful pop; false on empty-FIFO underflow
 ******************************************************************************/
bool entropy_src_ip::handle_read_FIFO_RDATA(uint32_t& value)
{
    if (m_fifo.empty())
    {
        value = 0u;
        // Assert fifo_underflow interrupt status bit.
        uint32_t status = static_cast<uint32_t>(INTR_STATUS);
        INTR_STATUS = status | INTR_BIT_FIFO_UNDERFLOW;
        m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);

        REG_WARN(1, logger) << "FIFO_RDATA read: FIFO empty — underflow";
        return true;
    }

    value = m_fifo.front();
    m_fifo.pop();

    // Increment rptr modulo 32 BEFORE calling update_fifo_status()
    // so that the FIFO_STATUS.RPTR field reflects the new pointer value
    // atomically together with the decremented LEVEL.
    //
    // FIFO STATUS UPDATE:
    //   - registers.FIFO_STATUS.fields.RPTR: bits [20:16], "updated on
    //     every successful pop"
    //   - "wptr and rptr are 5-bit counters that wrap modulo 32"
    m_rptr = static_cast<uint8_t>((m_rptr + 1u) % 32);

    update_fifo_status();

    REG_INFO(3, logger)
        << "FIFO_RDATA pop: value=0x" << std::hex << value
        << " fifo_depth=" << std::dec << m_fifo.size()
        << " rptr=" << static_cast<unsigned>(m_rptr);

    return true;
}

// =============================================================================
// Hardware reset process (SC_METHOD)
// =============================================================================

/******************************************************************************
 * @brief Handles asynchronous hardware reset (active-low rst_ni).
 *
 * Sensitive to any transition on rst_ni. When rst_ni reads low:
 *  1. Drains the FIFO queue and resets write/read pointers.
 *  2. Calls reset_all_registers() to restore all regmodel registers to defaults.
 *  3. Clears internal state mirrors.
 *  4. Sets m_hw_reset_in_progress to signal the background thread.
 *  5. Notifies m_reset_event and m_interrupt_update_event.
 *
 * When rst_ni returns high, no action is taken — the background thread
 * handles the re-enable path via its level-sensitive rst_ni gate.
 *
 * Pattern follows AES::reset_process() and keymgr_tt::reset_process().
 ******************************************************************************/
void entropy_src_ip::reset_process()
{
    if (!rst_ni.read())
    {
        REG_INFO(2, logger) << "reset_process: rst_ni asserted (active-low) "
                                "— executing hardware reset";

        // Drain the FIFO and reset pointers
        while (!m_fifo.empty())
        {
            m_fifo.pop();
        }
        m_wptr = 0u;
        m_rptr = 0u;

        // Reset all regmodel registers to their defaults
        reset_all_registers();

        // Update FIFO_STATUS to reflect the empty FIFO
        update_fifo_status();

        // Clear internal state mirrors to defaults
        m_fifo_enabled = true;   // FIFO_CTRL.ENABLE default is 1
        m_health_test_enabled = false;
        m_startup_delay_ns = 0u;

        // Signal to the background thread
        m_hw_reset_in_progress = true;
        m_reset_in_progress = false;  // Clear any pending software reset

        // Notify the background thread to break out of any wait
        m_reset_event.notify(sc_core::SC_ZERO_TIME);

        // Re-evaluate interrupt outputs (all will de-assert since
        // INTR_STATUS is cleared by reset_all_registers)
        m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);

        REG_INFO(2, logger) << "reset_process: hardware reset complete";
    }
}

// =============================================================================
// Background SC_THREAD
// =============================================================================

/******************************************************************************
 * @brief Background entropy generation thread — four-state state machine
 *
 * Implements the BackgroundEntropyProcess and FIFOFillControl state machines
 * from the architecture-behaviour map in a single SC_THREAD that runs for the
 * entire simulation duration.
 *
 * ## State Machine Summary
 *
 * The thread cycles through four logical states:
 *
 *  1. STARTUP_DELAY — Initial or post-reset hold-off.
 *     Duration: m_startup_delay_ns nanoseconds (from STARTUP_CTRL.DELAY_CYCLES).
 *     Wait is interruptible by m_reset_event.
 *
 *  2. WAITING_FOR_ENABLE — FIFO is disabled (m_fifo_enabled = false).
 *     Thread blocks on wait(m_fifo_fill_event | m_reset_event).
 *     Exited when m_fifo_fill_event fires → STARTUP_DELAY.
 *     Exited when m_reset_event fires → RESET_PENDING.
 *
 *  3. RUNNING — Active entropy generation loop.
 *     Per-iteration actions (architecture map Section 7.2):
 *       a. FIFO fill: if FIFO not full, push one entropy word (OpenSSL) and update status.
 *       b. FIFO overflow: if FIFO full, assert INTR_STATUS[8] once.
 *       c. Health test stub: no-op (implements the real counter logic).
 *       d. Iteration delay: wait(BASE_ITERATION_PERIOD_NS * (1 + rate), SC_NS,
 *                                m_reset_event)  when DOWNSAMPLE_RATE != 0,
 *                           wait(SC_ZERO_TIME)  when DOWNSAMPLE_RATE == 0.
 *     If m_reset_event fires during step (d) → RESET_PENDING.
 *     If m_fifo_enabled becomes false within the iteration → WAITING_FOR_ENABLE.
 *
 *  4. RESET_PENDING — Reset recovery.
 *     Thread waits for m_reset_complete_event (posted by handle_write_CTRL
 *     after all register clearing and CTRL.RESET self-clear are done).
 *     After receiving m_reset_complete_event the thread re-derives state
 *     from m_fifo_enabled and m_startup_delay_ns (already updated by
 *     handle_write_CTRL) and transitions to WAITING_FOR_ENABLE or
 *     STARTUP_DELAY.
 *
 * ## Functional Summary
 *
 * This thread never calls .write() on any sc_out<bool> port.  All interrupt
 * output updates are deferred to interrupt_output_method (SC_METHOD sensitive
 * to m_interrupt_update_event) to satisfy the single-writer contract.
 *
 * ## State Machine References
 *   - BackgroundEntropyProcess (IDLE / RUNNING)
 *   - FIFOFillControl (FILL / FULL_WAIT)
 *   - timing_constraints: startup-holdoff, no-cycle-accuracy
 *   - events: FIFO_OVERFLOW (trigger condition and propagation)
 ******************************************************************************/
void entropy_src_ip::entropy_generation_thread()
{
    m_qk.reset();

    REG_INFO(2, logger) << "entropy_generation_thread: starting";

    // =========================================================================
    // Level-sensitive rst_ni gate — wait for reset to be released at boot.
    // This handles the case where rst_ni is already low at elaboration time.
    // Pattern follows keymgr_tt::fw_thread().
    // =========================================================================
    while (!rst_ni.read())
    {
        REG_INFO(2, logger)
            << "entropy_generation_thread: waiting for rst_ni de-assertion";
        wait(rst_ni.value_changed_event());
    }

    REG_INFO(2, logger) << "entropy_generation_thread: starting";

    // =========================================================================
    // Initial startup delay — use quantum keeper (no blocking wait)
    // =========================================================================
    if (m_startup_delay_ns > 0u)
    {
        m_qk.inc(sc_core::sc_time(
            static_cast<double>(m_startup_delay_ns), sc_core::SC_NS));
        if (m_qk.need_sync())
        {
            m_qk.sync();
        }
    }

    REG_INFO(2, logger) << "entropy_generation_thread: entering main loop";

    // =========================================================================
    // Main loop — simplified event-driven design with quantum keeper pacing
    // =========================================================================
    while (true)
    {
        // ---------------------------------------------------------------------
        // Hardware reset check — rst_ni asserted asynchronously
        // ---------------------------------------------------------------------
        if (!rst_ni.read() || m_hw_reset_in_progress)
        {
            REG_INFO(2, logger)
                << "entropy_generation_thread: hardware reset detected "
                   "— waiting for rst_ni release";
            m_hw_reset_in_progress = false;
            m_qk.reset();

            while (!rst_ni.read())
            {
                wait(rst_ni.value_changed_event());
            }

            REG_INFO(2, logger)
                << "entropy_generation_thread: rst_ni released "
                   "— re-deriving state from regmodel registers";

            // Re-derive internal state from post-reset regmodel register values
            m_fifo_enabled = (static_cast<uint32_t>(FIFO_CTRL.ENABLE) != 0u);
            m_health_test_enabled =
                (static_cast<uint32_t>(HEALTH_TEST_CTRL.ENABLE) != 0u);
            m_startup_delay_ns = static_cast<uint32_t>(STARTUP_CTRL.DELAY_CYCLES);
            m_reset_in_progress = false;

            continue;
        }

        // ---------------------------------------------------------------------
        // Software reset check — top of every loop iteration
        // ---------------------------------------------------------------------
        if (m_reset_in_progress)
        {
            handle_reset_recovery();
            continue;
        }

        // ---------------------------------------------------------------------
        // WAITING_FOR_ENABLE — block on event (not a timed wait)
        //
        // If the FIFO is disabled, wait for re-enable or reset event.
        // This is an event-based wait required for IDLE→RUNNING transition.
        // ---------------------------------------------------------------------
        if (!m_fifo_enabled)
        {
            REG_INFO(2, logger)
                << "entropy_generation_thread: WAITING_FOR_ENABLE — "
                   "blocking on fifo_fill_event | reset_event";

            wait(m_fifo_fill_event | m_reset_event | rst_ni.value_changed_event());

            // Check for hardware reset
            if (!rst_ni.read() || m_hw_reset_in_progress)
            {
                // Hardware reset — go back to top of loop which will
                // wait for rst_ni release.
                continue;
            }

            if (m_reset_in_progress)
            {
                handle_reset_recovery();
                continue;
            }

            // FIFO re-enabled — apply startup delay via quantum keeper
            if (m_startup_delay_ns > 0u)
            {
                REG_INFO(2, logger)
                    << "entropy_generation_thread: STARTUP_DELAY after FIFO re-enable ("
                    << m_startup_delay_ns << " ns)";
                m_qk.inc(sc_core::sc_time(
                    static_cast<double>(m_startup_delay_ns), sc_core::SC_NS));
                if (m_qk.need_sync())
                {
                    m_qk.sync();
                }
                if (m_reset_in_progress)
                {
                    handle_reset_recovery();
                    continue;
                }
            }

            REG_INFO(2, logger)
                << "entropy_generation_thread: FIFO re-enabled — entering RUNNING";
        }

        // ---------------------------------------------------------------------
        // RUNNING — one entropy generation iteration
        // ---------------------------------------------------------------------

        // --- Action A: FIFO fill / overflow detection -----------------------
        if (m_fifo_enabled)
        {
            if (m_fifo.size() < FIFO_DEPTH)
            {
                // Push one high-quality entropy word from OpenSSL into the FIFO
                uint32_t entropy_word = 0u;
                if (RAND_bytes(reinterpret_cast<unsigned char*>(&entropy_word), sizeof(entropy_word)) != 1) {
                    REG_ERROR(0, logger) << "RAND_bytes() failed - entropy generation stalled";
                    break; 
                }
                m_fifo.push(entropy_word);

                m_wptr = static_cast<uint8_t>((m_wptr + 1u) % 32);
                update_fifo_status();

                // REG_DEBUG(1, logger)
                //     << "entropy_generation_thread: RUNNING — pushed 0x"
                //     << std::hex << entropy_word
                //     << " fifo_depth=" << std::dec << m_fifo.size()
                //     << " wptr=" << static_cast<unsigned>(m_wptr);
            }
            else
            {
                // FIFO full — assert FIFO_OVERFLOW if not already set
                uint32_t status = static_cast<uint32_t>(INTR_STATUS);
                if ((status & INTR_BIT_FIFO_OVERFLOW) == 0u)
                {
                    INTR_STATUS = status | INTR_BIT_FIFO_OVERFLOW;
                    m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);
                    // REG_WARN(1, logger)
                    //     << "entropy_generation_thread: FIFO full — FIFO_OVERFLOW asserted";
                }
                // Back-pressure pacing when FIFO is full
                m_qk.inc(sc_core::sc_time(BASE_ITERATION_PERIOD_NS, sc_core::SC_NS));
                if (m_qk.need_sync())
                {
                    m_qk.sync();
                }
            }
        }

        // --- Action C: Iteration pacing via quantum keeper ------------------
        //
        // DOWNSAMPLE_RATE field (CTRL[25:16]):
        //   Value of N means output every (N+1) cycles.
        //   Default=0 → no additional downsampling (multiply by 1).
        {
            uint32_t downsample_rate =
                static_cast<uint32_t>(CTRL.DOWNSAMPLE_RATE);

            double effective_period_ns =
                BASE_ITERATION_PERIOD_NS
                * static_cast<double>(1u + downsample_rate);

            m_qk.inc(sc_core::sc_time(effective_period_ns, sc_core::SC_NS));
            if (m_qk.need_sync())
            {
                m_qk.sync();
            }
        }

        // Check for reset after pacing
        if (m_reset_in_progress)
        {
            handle_reset_recovery();
            continue;
        }
    }
}

/******************************************************************************
 * @brief Reset recovery helper — handles the RESET_PENDING state cleanly.
 *
 * Waits for the m_reset_complete_event (posted by handle_write_CTRL after
 * Actions 1–8 complete), then re-derives all internal mirrors from the
 * post-reset regmodel register state and resets the quantum keeper.
 *
 * Functional references:
 *   - BackgroundEntropyProcess IDLE→RUNNING transition after reset
 *   - re-derived flags from post-reset regmodel defaults; 
 *     explicit FIFO drain to ensure clean architectural state.
 ******************************************************************************/
void entropy_src_ip::handle_reset_recovery()
{
    // Block until handle_write_CTRL completes stabilization delay and
    // CTRL self-clear.
    wait(m_reset_complete_event);

    m_reset_in_progress = false;
 
    // Drain FIFO and reset pointers during software reset recovery.
    while (!m_fifo.empty())
    {
        m_fifo.pop();
    }
    m_wptr = 0u;
    m_rptr = 0u;
    update_fifo_status();

    // Re-derive state from post-reset regmodel register values
    m_fifo_enabled = (static_cast<uint32_t>(FIFO_CTRL.ENABLE) != 0u);
    m_health_test_enabled =
        (static_cast<uint32_t>(HEALTH_TEST_CTRL.ENABLE) != 0u);
    m_startup_delay_ns = static_cast<uint32_t>(STARTUP_CTRL.DELAY_CYCLES);

    // Reset quantum keeper for clean post-reset timing
    m_qk.reset();

    REG_INFO(2, logger)
        << "handle_reset_recovery: resolved — "
        << "fifo_enabled=" << std::boolalpha << m_fifo_enabled
        << " health_test_enabled=" << m_health_test_enabled
        << " startup_delay_ns=" << std::dec << m_startup_delay_ns;

    // Post-reset startup delay via quantum keeper
    if (m_startup_delay_ns > 0u)
    {
        REG_INFO(2, logger)
            << "handle_reset_recovery: post-reset STARTUP_DELAY ("
            << m_startup_delay_ns << " ns)";
        m_qk.inc(sc_core::sc_time(
            static_cast<double>(m_startup_delay_ns), sc_core::SC_NS));
        if (m_qk.need_sync())
        {
            m_qk.sync();
        }
    }
}

