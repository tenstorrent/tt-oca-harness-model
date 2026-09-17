// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file entropy_src_interface.h
 * @brief Abstract interface class for the entropy_src TLM model
 *
 * Declares the pure-virtual register callback methods that the concrete
 * entropy_src model must implement.  Separating the interface from the
 * implementation allows the testbench and integration environments to
 * depend on the interface contract rather than the model internals.
 *
 * Callback coverage (from docs/sections/entropy_src-register-callbacks.md):
 *  - 7 write callbacks: CTRL (software reset), INTR_STATUS (W1C clear),
 *    INTR_ENABLE (interrupt mask update), INTR_TEST (inject interrupt),
 *    FIFO_CTRL (gate FIFO enable), HEALTH_TEST_CTRL (gate health test),
 *    STARTUP_CTRL (capture startup delay)
 *  - 1 read callback:  FIFO_RDATA (destructive FIFO pop)
 *
 * Reference:
 *   - entropy_src/docs/sections/entropy_src-register-callbacks.md
 *   - entropy_src/docs/entropy_src-detailed-design.md  Section 4
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once

#include <cstdint>

/******************************************************************************
 * @class entropy_src_if
 * @brief Pure-virtual interface for entropy_src register callbacks
 *
 * All eight callback methods correspond to side-effecting register operations
 * identified in the detailed design.  The concrete entropy_src class inherits
 * from this interface and provides implementations.  Pure storage registers
 * (34 total) are handled transparently by the regmodel register layer and do not
 * require explicit callbacks.
 ******************************************************************************/
class entropy_src_if
{
public:
    /// @brief Virtual destructor for safe polymorphic deletion
    virtual ~entropy_src_if() = default;

    // =========================================================================
    // Write Callbacks
    // =========================================================================

    /**
     * @brief Write callback for CTRL register (offset 0x04)
     *
     * Stores writable CTRL fields. Bit 0 is reserved (RSVD0). The entropy
     * source is reset from rst_ni (SW_RESET_N.trng_sw_rst_n), not CTRL.
     *
     * @param value 32-bit value written to CTRL
     * @return true on successful callback execution
     */
    virtual bool handle_write_CTRL(uint32_t value) = 0;

    /**
     * @brief Write callback for INTR_STATUS register (offset 0x10) — W1C
     *
     * Write-1-to-clear semantics: each set bit in @p value clears the
     * corresponding interrupt-status bit.  After clearing, the active
     * interrupt output ports are re-evaluated and updated.
     *
     * @param value 32-bit value written to INTR_STATUS
     * @return true on successful callback execution
     */
    virtual bool handle_write_INTR_STATUS(uint32_t value) = 0;

    /**
     * @brief Write callback for INTR_ENABLE register (offset 0x14)
     *
     * Stores the new enable mask and re-evaluates each interrupt output port
     * (intr_health_test_failed, intr_fifo_error, intr_fifo_overflow,
     * intr_fifo_underflow) by AND-ing the stored INTR_STATUS with the new
     * INTR_ENABLE mask.
     *
     * @param value 32-bit value written to INTR_ENABLE
     * @return true on successful callback execution
     */
    virtual bool handle_write_INTR_ENABLE(uint32_t value) = 0;

    /**
     * @brief Write callback for INTR_TEST register (offset 0x18) — WO
     *
     * Injects a test interrupt: each set bit in @p value sets the
     * corresponding INTR_STATUS bit and re-evaluates the interrupt output
     * ports, subject to INTR_ENABLE masking.  The INTR_TEST register itself
     * is write-only; reads return 0.
     *
     * @param value 32-bit value written to INTR_TEST
     * @return true on successful callback execution
     */
    virtual bool handle_write_INTR_TEST(uint32_t value) = 0;

    /**
     * @brief Write callback for FIFO_CTRL register (offset 0x20)
     *
     * If the FIFO_ENABLE bit (bit 0) transitions from 1 to 0, the FIFO
     * queue is drained and the FIFO_STATUS counters are reset to zero.
     * If FIFO_ENABLE transitions 0→1, the background entropy generation
     * thread is signalled to resume filling the FIFO.
     *
     * @param value 32-bit value written to FIFO_CTRL
     * @return true on successful callback execution
     */
    virtual bool handle_write_FIFO_CTRL(uint32_t value) = 0;

    /**
     * @brief Write callback for HEALTH_TEST_CTRL register (offset 0x30)
     *
     * Captures the updated health test configuration and, if the
     * HEALTH_TEST_ENABLE field is set, enables the health-test counter
     * update logic inside the background SC_THREAD.
     *
     * @param value 32-bit value written to HEALTH_TEST_CTRL
     * @return true on successful callback execution
     */
    virtual bool handle_write_HEALTH_TEST_CTRL(uint32_t value) = 0;

    /**
     * @brief Write callback for STARTUP_CTRL register (offset 0xB0)
     *
     * Captures the STARTUP_DELAY field.  The delay value is used by the
     * background SC_THREAD as the initial wait period before generating
     * entropy words.
     *
     * @param value 32-bit value written to STARTUP_CTRL
     * @return true on successful callback execution
     */
    virtual bool handle_write_STARTUP_CTRL(uint32_t value) = 0;

    // =========================================================================
    // Read Callbacks
    // =========================================================================

    /**
     * @brief Read callback for FIFO_RDATA register (offset 0x28) — destructive pop
     *
     * Pops the head entry from the FIFO queue into @p value.  If the queue
     * is empty, @p value is set to 0 and INTR_STATUS.fifo_underflow is
     * asserted.  If the queue becomes empty after the pop,
     * INTR_STATUS.fifo_underflow may also be raised per the enable mask.
     *
     * @param value Reference that receives the 32-bit FIFO entry
     * @return true on successful pop; false if the FIFO was empty
     */
    virtual bool handle_read_FIFO_RDATA(uint32_t& value) = 0;
};
