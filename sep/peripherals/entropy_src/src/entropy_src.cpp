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
 * The combined mask 0x11111111 (INTR_ALL_BITS_MASK) equals the write_bit_mask
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
#include "reg_access.h"
#include <openssl/rand.h>
#include <algorithm>
entropy_src_ip::entropy_provider_handler::entropy_provider_handler(
    entropy_src_ip& owner)
    : owner_(owner)
{}

bool entropy_src_ip::entropy_provider_handler::get_seed_384(
    uint8_t seed[48], bool& fips_compliant)
{
    fips_compliant = false;
    if (seed == nullptr || owner_.m_reset_in_progress ||
        owner_.m_hw_reset_in_progress || !owner_.m_fifo_enabled) {
        return false;
    }
    if (owner_.m_fifo.size() < 12u) {
        return false;
    }
    for (unsigned i = 0; i < 12; ++i) {
        uint32_t word = 0;
        if (!owner_.handle_read_FIFO_RDATA(word)) {
            return false;
        }
        seed[i * 4 + 0] = static_cast<uint8_t>(word);
        seed[i * 4 + 1] = static_cast<uint8_t>(word >> 8);
        seed[i * 4 + 2] = static_cast<uint8_t>(word >> 16);
        seed[i * 4 + 3] = static_cast<uint8_t>(word >> 24);
    }
    if (owner_.m_health_test_enabled) {
        const uint32_t limit =
            static_cast<uint32_t>(owner_.HEALTH_TEST_CTRL.REPETITION_LIMIT);
        uint32_t run = 1;
        uint32_t max_run = 1;
        for (unsigned i = 1; i < 48; ++i) {
            run = (seed[i] == seed[i - 1]) ? run + 1 : 1;
            max_run = std::max(max_run, run);
        }
        owner_.REPETITION_TEST_COUNT.REPETITION_COUNT = max_run;
        if (limit != 0 && max_run > limit) {
            owner_.REPCNT_TOTAL_FAILS =
                static_cast<uint32_t>(owner_.REPCNT_TOTAL_FAILS) + 1u;
            owner_.INTR_STATUS =
                static_cast<uint32_t>(owner_.INTR_STATUS) |
                entropy_src_ip::INTR_BIT_HEALTH_TEST_FAILED;
            owner_.m_interrupt_update_event.notify(sc_core::SC_ZERO_TIME);
            return false;
        }
    }
    fips_compliant = owner_.m_health_test_enabled;
    return true;
}


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
 * (0x11111111), which equals the INTR_ENABLE write_bit_mask (0x1111) and the
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
 *  - bits [6:0]   LEVEL : m_fifo.size() capped at FIFO_DEPTH (64)
 *  - bits [13:8]  WPTR  : m_wptr & 0x3F (6-bit write pointer, mod-64, shifted left by 8)
 *  - bits [21:16] RPTR  : m_rptr & 0x3F (6-bit read pointer, mod-64, shifted left by 16)
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
 *   - registers.FIFO_STATUS (offset 0x24): LEVEL[6:0], WPTR[13:8], RPTR[21:16]
 *   - atomic FIFO_STATUS write invariant
 *   - push: LEVEL and WPTR updated
 *   - pop: LEVEL and RPTR updated
 *   - description: "Each FIFO_STATUS update is written atomically
 *     (single 32-bit write combining LEVEL, WPTR, RPTR)"
 ******************************************************************************/
void entropy_src_ip::update_fifo_status()
{
    // LEVEL: current queue occupancy. The generation thread never pushes
    // past FIFO_DEPTH, so size() is already the software-visible level.
    uint32_t level = static_cast<uint32_t>(m_fifo.size());

    // Encode all three fields into a single 32-bit word:
    //   bits [6:0]   = LEVEL
    //   bits [13:8]  = WPTR (6 bits)
    //   bits [21:16] = RPTR (6 bits)
    constexpr uint32_t kPtrMask = 0x3Fu;
    uint32_t fifo_status_val =
          (level                       & 0x7Fu)
        | (static_cast<uint32_t>(m_wptr & kPtrMask) << 8u)
        | (static_cast<uint32_t>(m_rptr & kPtrMask) << 16u);

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
 * @brief Write callback for CTRL register (offset 0x04)
 *
 * Stores AUTOTUNE_ENABLE, BYPASS_ENTROPY_COMPRESSOR, DOWNSAMPLE_RATE, and
 * SHA256_WHITENING_ENABLE. Bit 0 is RSVD0 (read-only zero): the coordinated
 * TRNG reset lives on sep_reset_ctrl SW_RESET_N.trng_sw_rst_n and arrives
 * here as rst_ni, not as a CTRL write.
 ******************************************************************************/
bool entropy_src_ip::handle_write_CTRL(uint32_t value)
{
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0xFFFFFFFFu : 0u;
    CTRL = regmodel::apply_lock_gated(
        static_cast<uint32_t>(CTRL),
        value & static_cast<uint32_t>(CTRL.write_bit_mask),
        lock);
    update_boot_phase_done();

    REG_INFO(3, logger)
        << "CTRL write: stored 0x"
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
    // (INTR_ALL_BITS_MASK = 0x11111111) before OR-ing into INTR_STATUS.  This
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
    // RDL: only ENTROPY_CHURN_ENABLE[4] is swwel. ENABLE[0] stays writable.
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0x10u : 0u;
    const uint32_t next = regmodel::apply_lock_gated(
        static_cast<uint32_t>(FIFO_CTRL),
        value & static_cast<uint32_t>(FIFO_CTRL.write_bit_mask),
        lock);
    bool new_enable = (next & 0x1u) != 0u;
    bool prev_enable = m_fifo_enabled;

    FIFO_CTRL = next;
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
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0xFFFFFFFFu : 0u;
    HEALTH_TEST_CTRL = regmodel::apply_lock_gated(
        static_cast<uint32_t>(HEALTH_TEST_CTRL),
        value & static_cast<uint32_t>(HEALTH_TEST_CTRL.write_bit_mask),
        lock);

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
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0xFFFFFFFFu : 0u;
    RING_OSC_ENABLE = regmodel::apply_lock_gated(
        static_cast<uint32_t>(RING_OSC_ENABLE),
        value & static_cast<uint32_t>(RING_OSC_ENABLE.write_bit_mask),
        lock);
    update_boot_phase_done();
    return true;
}

bool entropy_src_ip::handle_write_ALERT_THRESHOLD(uint32_t value)
{
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0xFFFFFFFFu : 0u;
    ALERT_THRESHOLD = regmodel::apply_lock_gated(
        static_cast<uint32_t>(ALERT_THRESHOLD),
        value & static_cast<uint32_t>(ALERT_THRESHOLD.write_bit_mask),
        lock);
    return true;
}

bool entropy_src_ip::handle_write_MIN_ENTROPY_H(uint32_t value)
{
    const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? 0xFFFFFFFFu : 0u;
    MIN_ENTROPY_H = regmodel::apply_lock_gated(
        static_cast<uint32_t>(MIN_ENTROPY_H),
        value & static_cast<uint32_t>(MIN_ENTROPY_H.write_bit_mask),
        lock);
    return true;
}

void entropy_src_ip::update_boot_phase_done()
{
    const bool module_on = (static_cast<uint32_t>(CTRL.MODULE_ENABLE) != 0u);
    const bool generators_on = (static_cast<uint32_t>(RING_OSC_ENABLE.ENABLE) != 0u);

    if (module_on && generators_on) {
        if (static_cast<uint32_t>(MAIN_SM_STATUS.BOOT_PHASE_DONE) == 0u) {
            MAIN_SM_STATUS.BOOT_PHASE_DONE = 1;
            MAIN_SM_STATUS.IDLE = 0;
            REG_INFO(3, logger)
                << "BOOT_PHASE_DONE asserted (MODULE_ENABLE and generators on)";
        }
    } else if (!module_on) {
        MAIN_SM_STATUS.BOOT_PHASE_DONE = 0;
        MAIN_SM_STATUS.IDLE = 1;
    }
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
    m_rptr = static_cast<uint8_t>((m_rptr + 1u) % FIFO_DEPTH);

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

        // Notify the background thread to break out of any wait, including
        // leftover handle_reset_recovery (CTRL.RESET no longer posts this).
        m_reset_event.notify(sc_core::SC_ZERO_TIME);
        m_reset_complete_event.notify(sc_core::SC_ZERO_TIME);

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
            qk_sync_interruptible();
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
            m_startup_delay_ns = 0u;
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
                    qk_sync_interruptible();
                }
                if (!rst_ni.read() || m_hw_reset_in_progress)
                {
                    continue;
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

                m_wptr = static_cast<uint8_t>((m_wptr + 1u) % FIFO_DEPTH);
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
                    qk_sync_interruptible();
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
                qk_sync_interruptible();
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
    m_startup_delay_ns = 0u;

    // Reset quantum keeper for clean post-reset timing
    m_qk.reset();

    REG_INFO(2, logger)
        << "handle_reset_recovery: resolved — "
        << "fifo_enabled=" << std::boolalpha << m_fifo_enabled
        << " health_test_enabled=" << m_health_test_enabled
        << " startup_delay_ns=" << std::dec << m_startup_delay_ns;
}

/******************************************************************************
 * @brief Sync the quantum keeper, returning early on TRNG reset.
 *
 * tlm_quantumkeeper::sync() is a plain timed wait and cannot see rst_ni or
 * m_reset_event. After CTRL.RESET was retired, that left the generation
 * thread parked in a leftover quantum across apply_hw_reset(), so FIFO-fill
 * tests that only yield SC_ZERO_TIME never observed a post-reset word.
 ******************************************************************************/
void entropy_src_ip::qk_sync_interruptible()
{
    const sc_core::sc_time local = m_qk.get_local_time();
    m_qk.reset();
    if (local > sc_core::SC_ZERO_TIME)
    {
        wait(local, m_reset_event | rst_ni.value_changed_event());
    }
}

void entropy_src_ip::register_fips_locked(regmodel::Reg<32>& reg)
{
    memory.register_write_callback(
        [this, &reg](DT incoming) {
            const uint32_t lock = static_cast<uint32_t>(FIPS_LOCK.LOCK) ? ~0u : 0u;
            reg = regmodel::apply_lock_gated(
                static_cast<uint32_t>(reg),
                incoming & static_cast<uint32_t>(reg.write_bit_mask),
                lock);
            return true;
        },
        reg.offset);
}

void entropy_src_ip::register_w1c(regmodel::Reg<32>& reg, uint32_t mask)
{
    memory.register_write_callback(
        [&reg, mask](DT incoming) {
            reg = regmodel::apply_w1c(static_cast<uint32_t>(reg), incoming, mask);
            return true;
        },
        reg.offset);
}

void entropy_src_ip::register_certified_config_locks()
{
    // swwel fields that do not already have a side-effect write handler.
    // FIFO_CTRL.ENABLE is not swwel; its handler locks only bit 4.
    regmodel::Reg<32>* const locked[] = {
        &HEALTH_TEST_WINDOW_SIZE,
        &MARKOV_TEST_PROB_THRESHOLDS,
        &APT_PROPORTION_1BIT,
        &APT_PROPORTION_LO,
        &RING_OSC_TUNE,
        &RING_OSC_CTRL,
        &DECORRELATOR_CTRL,
        &DECORRELATOR_MASK,
        &GENERATOR_0_SAMPLE_CLK_CONFIG,
        &GENERATOR_1_SAMPLE_CLK_CONFIG,
        &GENERATOR_2_SAMPLE_CLK_CONFIG,
        &GENERATOR_3_SAMPLE_CLK_CONFIG,
        &GENERATOR_4_SAMPLE_CLK_CONFIG,
        &GENERATOR_5_SAMPLE_CLK_CONFIG,
        &GENERATOR_6_SAMPLE_CLK_CONFIG,
        &GENERATOR_7_SAMPLE_CLK_CONFIG,
        &GENERATOR_8_SAMPLE_CLK_CONFIG,
        &GENERATOR_9_SAMPLE_CLK_CONFIG,
        &GENERATOR_10_SAMPLE_CLK_CONFIG,
        &GENERATOR_11_SAMPLE_CLK_CONFIG,
    };
    for (regmodel::Reg<32>* reg : locked) {
        register_fips_locked(*reg);
    }
}

void entropy_src_ip::register_health_status_w1c()
{
    regmodel::Reg<32>* const status[] = {
        &HEALTH_TEST_STATUS,
        &GENERATOR_0_HEALTH_STATUS,
        &GENERATOR_1_HEALTH_STATUS,
        &GENERATOR_2_HEALTH_STATUS,
        &GENERATOR_3_HEALTH_STATUS,
        &GENERATOR_4_HEALTH_STATUS,
        &GENERATOR_5_HEALTH_STATUS,
        &GENERATOR_6_HEALTH_STATUS,
        &GENERATOR_7_HEALTH_STATUS,
        &GENERATOR_8_HEALTH_STATUS,
        &GENERATOR_9_HEALTH_STATUS,
        &GENERATOR_10_HEALTH_STATUS,
        &GENERATOR_11_HEALTH_STATUS,
    };
    for (regmodel::Reg<32>* reg : status) {
        register_w1c(*reg, HEALTH_STATUS_W1C_MASK);
    }
}

