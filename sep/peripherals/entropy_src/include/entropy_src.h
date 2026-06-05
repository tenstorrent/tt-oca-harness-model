/******************************************************************************
 * @file entropy_src.h
 * @brief entropy_src SystemC TLM model — user extension class
 *
 * Extends entropy_src_base with the port interfaces defined by the IP
 * specification and implements all register callbacks declared in
 * entropy_src_interface.h.
 *
 * ## TLM Transport (FUNC-001)
 *
 * The `b_transport` handler on `target_socket` (the `reg_socket` per the port
 * interface specification) is registered by `csml_memory<32>` during
 * `entropy_src_base` construction via `memory.bind_to_socket(target_socket)`.
 * No separate `b_transport` override is required in `entropy_src_ip`.
 *
 * CSML handles:
 *  - Storage and default values for all 42 memory-mapped registers (0x00–0xEC)
 *  - Write-mask enforcement (RW, WO, W1C, RO per register type definition)
 *  - Reserved-bit masking
 *  - Routing of write/read transactions to the registered callbacks
 *  - `TLM_OK_RESPONSE` set on each payload before return
 *
 * Eight behavioural callbacks registered in the constructor augment CSML:
 *  - 7 write callbacks: CTRL, INTR_STATUS, INTR_ENABLE, INTR_TEST,
 *    FIFO_CTRL, HEALTH_TEST_CTRL, STARTUP_CTRL
 *  - 1 read callback:  FIFO_RDATA (destructive pop)
 *
 * ## Interrupt Bit Layout
 *
 * INTR_STATUS (0x10), INTR_ENABLE (0x14), and INTR_TEST (0x18) all share
 * the same active-bit positions (architecture-behaviour map, registers
 * section; confirmed in entropy_src_register.h bitfield definitions):
 *   bit  0 : HEALTH_TEST_FAILED  (INTR_BIT_HEALTH_TEST_FAILED = 0x00000001)
 *   bit  4 : FIFO_ERROR          (INTR_BIT_FIFO_ERROR         = 0x00000010)
 *   bit  8 : FIFO_OVERFLOW       (INTR_BIT_FIFO_OVERFLOW      = 0x00000100)
 *   bit 12 : FIFO_UNDERFLOW      (INTR_BIT_FIFO_UNDERFLOW     = 0x00001000)
 *   Combined mask                (INTR_ALL_BITS_MASK           = 0x00001111)
 *
 * ## Port Summary
 *
 *  - sc_core::sc_out<bool> irq_o : Combined interrupt output (OR of all enabled interrupts)
 *
 * ## Functional Compliance
 *  - FUNC-001 through FUNC-008 foundation.
 *
 * The class is named entropy_src_ip to avoid a name collision with the
 * existing `namespace entropy_src {}` in entropy_src_register.h.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "entropy_src_base.h"
#include "entropy_src_interface.h"
#include "csml_logger.h"
#include "csml_parameter.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <queue>
#include <cstdint>

/******************************************************************************
 * @class entropy_src_ip
 * @brief Concrete entropy_src TLM model
 *
 * Inherits the register bank and CSML memory layer from entropy_src_base
 * and the callback interface contract from entropy_src_if.  Adds:
 *   - A single sc_out<bool> interrupt output port (irq_o) that is the OR
 *     of INTR_STATUS & INTR_ENABLE.  Software reads INTR_STATUS to determine
 *     the interrupt source.
 *   - A background SC_THREAD (entropy_generation_thread) that continuously
 *     fills the FIFO queue with OpenSSL RAND_bytes() generated entropy.
 *   - Eight register callback implementations covering the behavioural
 *     side-effects.
 *   - A tlm_utils::tlm_quantumkeeper for temporal decoupling.
 *
 * The TLM register access socket (reg_socket per the port interface spec) is
 * provided by entropy_src_base::target_socket, which is already bound to the
 * CSML memory object by the base constructor.  The testbench binds the test
 * harness initiator_socket to target_socket directly.
 ******************************************************************************/
class entropy_src_ip : public entropy_src_base, public entropy_src_if
{
public:
    SC_HAS_PROCESS(entropy_src_ip);

    // =========================================================================
    // Port declarations
    // =========================================================================

    /**
     * @brief Active-low asynchronous hardware reset input.
     *
     * When driven low, the model immediately resets all registers to defaults,
     * drains the FIFO, de-asserts interrupts, and interrupts the background
     * entropy generation thread.  The thread waits for rst_ni to return high
     * before resuming operation.
     *
     * Architecture: README.md line 41 — "rst_ni is assumed to be SoC internal
     * global clock and active low, asynchronous reset"
     */
    sc_core::sc_in<bool> rst_ni;

    /**
     * @brief Combined interrupt output (OR of all enabled interrupts).
     *
     * Driven high when (INTR_STATUS & INTR_ENABLE) != 0, i.e. at least one
     * enabled interrupt source is pending.  Software reads INTR_STATUS to
     * determine which source fired.
     */
    sc_core::sc_out<bool> irq_o;

    /**
     * @brief Logging verbosity (runtime-overridable via ini file)
     */
    csml_param<int> verbosity;

    // =========================================================================
    // Constructor
    // =========================================================================

    /**
     * @brief Constructor
     *
     * Initialises all port names, registers the eight register callbacks with
     * the CSML memory layer, initialises the PRNG, and declares the background
     * entropy generation SC_THREAD.
     *
     * @param n           SystemC module name
     * @param memory_size Byte size of the register address space.
     *                    Must be at least 0x160 to cover all 66 registers
     *                    (highest offset 0x150 + 4 bytes).
     */
    entropy_src_ip(sc_module_name n, unsigned int memory_size = 0x160)
        : entropy_src_base(n, memory_size)
        , entropy_src_if()
        , rst_ni("rst_ni")
        , irq_o("irq_o")
        , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
        , m_wptr(0u)
        , m_rptr(0u)
        , m_fifo_enabled(true)
        , m_health_test_enabled(false)
        , m_reset_in_progress(false)
        , m_hw_reset_in_progress(false)
        , m_startup_delay_ns(0u)
        , m_fifo_fill_event("m_fifo_fill_event")
        , m_reset_event("m_reset_event")
        , m_qk()
        , logger()
    {
        // Initialize temporal decoupling quantum keeper
        m_qk.reset();

        logger.setMaxVerbosity(verbosity.get_param_value());
        logger.setLogFormat(
            "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);

        // Register the eight behavioural callbacks with the CSML memory layer.
        // Offsets are divided by sizeof(DT) (= 4) to obtain the word index
        // used internally by csml_memory.
        memory.register_write_callback(
            [this](DT v) { return this->handle_write_CTRL(v); },
            CTRL.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_INTR_STATUS(v); },
            INTR_STATUS.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_INTR_ENABLE(v); },
            INTR_ENABLE.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_INTR_TEST(v); },
            INTR_TEST.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_FIFO_CTRL(v); },
            FIFO_CTRL.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_HEALTH_TEST_CTRL(v); },
            HEALTH_TEST_CTRL.offset);

        memory.register_write_callback(
            [this](DT v) { return this->handle_write_STARTUP_CTRL(v); },
            STARTUP_CTRL.offset);

        memory.register_read_callback(
            [this](DT &v) { return this->handle_read_FIFO_RDATA(v); },
            FIFO_RDATA.offset);

        // Declare background entropy generation thread.
        SC_THREAD(entropy_generation_thread);

        // Hardware reset process — sensitive to falling edge of rst_ni (active-low).
        SC_METHOD(reset_process);
        sensitive << rst_ni;
        dont_initialize();

        SC_METHOD(interrupt_output_method);
        sensitive << m_interrupt_update_event;
        dont_initialize();

        CSML_INFO(2, logger) << "entropy_src model constructed";
    }

    /// @brief Destructor
    ~entropy_src_ip() override = default;

    // =========================================================================
    // entropy_src_if — write callback implementations
    // =========================================================================

    /**
     * @brief Handle write to CTRL register
     *
     * If CTRL.RESET (bit 0) is set, execute the software reset sequence:
     * call reset_all_registers(), de-assert interrupt output port,
     * and self-clear CTRL.RESET to 0.
     *
     * @param value 32-bit value written to CTRL
     * @return true always
     */
    bool handle_write_CTRL(uint32_t value) override;

    /**
     * @brief Handle write to INTR_STATUS register (W1C)
     *
     * Clears status bits corresponding to set bits in @p value, then
     * re-evaluates all four interrupt output ports.
     *
     * @param value 32-bit value written to INTR_STATUS
     * @return true always
     */
    bool handle_write_INTR_STATUS(uint32_t value) override;

    /**
     * @brief Handle write to INTR_ENABLE register
     *
     * Stores the new enable mask and re-evaluates interrupt output ports.
     *
     * @param value 32-bit value written to INTR_ENABLE
     * @return true always
     */
    bool handle_write_INTR_ENABLE(uint32_t value) override;

    /**
     * @brief Handle write to INTR_TEST register (WO inject)
     *
     * Sets INTR_STATUS bits corresponding to set bits in @p value, then
     * re-evaluates interrupt output port subject to INTR_ENABLE masking.
     *
     * @param value 32-bit value written to INTR_TEST
     * @return true always
     */
    bool handle_write_INTR_TEST(uint32_t value) override;

    /**
     * @brief Handle write to FIFO_CTRL register
     *
     * If FIFO_ENABLE (bit 0) transitions from 0 to 1, notify the entropy 
     * thread to resume filling. Note: Disabling the FIFO does NOT drain it; 
     * data is preserved for software observability.
     *
     * @param value 32-bit value written to FIFO_CTRL
     * @return true always
     */
    bool handle_write_FIFO_CTRL(uint32_t value) override;

    /**
     * @brief Handle write to HEALTH_TEST_CTRL register
     *
     * Captures the new health test configuration and updates
     * m_health_test_enabled to reflect the HEALTH_TEST_ENABLE field.
     *
     * @param value 32-bit value written to HEALTH_TEST_CTRL
     * @return true always
     */
    bool handle_write_HEALTH_TEST_CTRL(uint32_t value) override;

    /**
     * @brief Handle write to STARTUP_CTRL register
     *
     * Captures the STARTUP_DELAY field (bits [15:0]) into m_startup_delay_ns.
     *
     * @param value 32-bit value written to STARTUP_CTRL
     * @return true always
     */
    bool handle_write_STARTUP_CTRL(uint32_t value) override;

    // =========================================================================
    // entropy_src_if — read callback implementation
    // =========================================================================

    /**
     * @brief Handle read of FIFO_RDATA register (destructive pop)
     *
     * Pops the head of m_fifo into @p value.  If the queue is empty,
     * asserts the fifo_underflow interrupt (subject to INTR_ENABLE masking)
     * and returns @p value = 0.
     *
     * @param value Reference populated with the popped 32-bit entropy word
     * @return true on successful pop; false if the FIFO was empty
     */
    bool handle_read_FIFO_RDATA(uint32_t& value) override;

private:
    // =========================================================================
    // Internal helpers
    // =========================================================================

    /**
     * @brief Re-evaluate and drive all four interrupt output ports
     *
     * For each of the four interrupt sources the output port is driven by
     * the logical AND of the corresponding INTR_STATUS bit and the
     * corresponding INTR_ENABLE bit.  This is the single authoritative point
     * where the four sc_out<bool> interrupt ports are written.
     *
     * Interrupt bit positions (from entropy_src-architecture-behaviour-map.json
     * Interrupt bit positions:
     *   bit  0 : HEALTH_TEST_FAILED  (INTR_BIT_HEALTH_TEST_FAILED)
     *   bit  4 : FIFO_ERROR          (INTR_BIT_FIFO_ERROR)
     *   bit  8 : FIFO_OVERFLOW       (INTR_BIT_FIFO_OVERFLOW)
     *   bit 12 : FIFO_UNDERFLOW      (INTR_BIT_FIFO_UNDERFLOW)
     */
    void update_interrupt_outputs();

    /**
     * @brief Update FIFO_STATUS register from the current FIFO queue state.
     *
     * Atomically encodes all three FIFO_STATUS fields into a single 32-bit
     * CSML write, preventing any intermediate state where LEVEL, WPTR, and
     * RPTR are inconsistent from software's perspective:
     *
     *  - LEVEL  (bits [6:0])  : current m_fifo.size(), capped at FIFO_DEPTH (32)
     *  - WPTR   (bits [12:8]) : current m_wptr (5-bit, wraps modulo 32)
     *  - RPTR   (bits [20:16]): current m_rptr (5-bit, wraps modulo 32)
     *  - Reserved bits [31:21]: always zero
     *
     * Called immediately after each successful FIFO push (background thread)
     * and after each successful FIFO pop (handle_read_FIFO_RDATA) so that
     * FIFO_STATUS is always current with no deferred synchronisation.
     */
    void update_fifo_status();

    /**
     * @brief Background SC_THREAD — four-state background entropy generation process
     *
     * **WAITING_FOR_ENABLE**:
     *   The thread is blocked on `wait(m_fifo_fill_event | m_reset_event)`.
     *   Entered when m_fifo_enabled is false.  Exited when either:
     *   - m_fifo_fill_event fires (FIFO_CTRL[0] written to 1) → STARTUP_DELAY
     *   - m_reset_event fires (software reset) → RESET_PENDING
     *
     * **STARTUP_DELAY** (post-enable or post-reset hold-off):
     *   The thread honours the programmable startup delay captured from
     *   STARTUP_CTRL.DELAY_CYCLES into m_startup_delay_ns.  If non-zero,
     *   the thread waits for that many nanoseconds before entering RUNNING.
     *   The wait is interruptible by m_reset_event.
     *
     * **RUNNING**:
     *   One iteration per loop:
     *   1. If m_fifo_enabled and FIFO not full: push one entropy word (OpenSSL), 
     *      call update_fifo_status().
     *   2. If m_fifo_enabled and FIFO full: assert INTR_STATUS.FIFO_OVERFLOW;
     *      notify m_interrupt_update_event.
     *   3. If m_health_test_enabled: (no-op stub).
     *   4. Compute iteration delay from CTRL.DOWNSAMPLE_RATE (bits [25:16]):
     *      - If DOWNSAMPLE_RATE == 0: wait(SC_ZERO_TIME).
     *      - If DOWNSAMPLE_RATE != 0: wait(BASE_ITERATION_PERIOD_NS *
     *        (1 + rate) ns, m_reset_event).
     *      If m_reset_event fires during the wait, exit RUNNING immediately.
     *
     * **RESET_PENDING** (reset recovery path):
     *   The thread detects m_reset_in_progress == true (set by handle_write_CTRL
     *   synchronously before notifying m_reset_event).  Because handle_write_CTRL
     *   runs inside b_transport and updates all register state and internal mirrors
     *   (m_fifo_enabled, m_startup_delay_ns) BEFORE returning, all post-reset
     *   state is already consistent when the thread enters this state.  The thread
     *   clears m_reset_in_progress, optionally waits the post-reset startup delay,
     *   then transitions to WAITING_FOR_ENABLE or RUNNING.
     *
     * Functional references:
     *   - BackgroundEntropyProcess (IDLE / RUNNING)
     *   - FIFOFillControl (FILL / FULL_WAIT)
     *   - timing_constraints: startup-holdoff, no-cycle-accuracy
     *
     * Single-writer compliance: this thread never calls .write() directly on any
     * sc_out<bool> port.  All interrupt output updates go through
     * m_interrupt_update_event → interrupt_output_method().
     */
    void entropy_generation_thread();

    /**
     * @brief Reset recovery helper — called when m_reset_in_progress is detected.
     *
     * Waits for m_reset_complete_event from handle_write_CTRL, then re-derives
     * m_fifo_enabled, m_health_test_enabled, and m_startup_delay_ns from
     * post-reset CSML register values. Resets the quantum keeper.
     */
    void handle_reset_recovery();

    /**
     * @brief SC_METHOD — sole driver of all four interrupt output ports.
     *
     * Sensitive to m_interrupt_update_event. Calls update_interrupt_outputs()
     * to re-evaluate and write sc_out<bool> port. This is the only
     * SystemC process permitted to call update_interrupt_outputs() or write
     * to interrupt port, eliminating the E115 dual-driver violation.
     */
    void interrupt_output_method();

    /**
     * @brief SC_METHOD — handles asynchronous hardware reset (active-low rst_ni).
     *
     * Sensitive to any transition on rst_ni. When rst_ni reads low:
     *  1. Drains the FIFO and resets pointers.
     *  2. Calls reset_all_registers() to restore all CSML registers to defaults.
     *  3. Clears internal state mirrors (m_fifo_enabled, m_health_test_enabled, etc.).
     *  4. Sets m_hw_reset_in_progress to interrupt the background thread.
     *  5. Notifies m_reset_event and m_interrupt_update_event.
     *
     * Pattern follows AES/keymgr_tt/GPIO convention.
     */
    void reset_process();

    // =========================================================================
    // Internal state
    // =========================================================================

    /// FIFO queue holding generated entropy words (max depth = 32)
    std::queue<uint32_t> m_fifo;

    /// FIFO write pointer — 5-bit counter that wraps modulo 32.
    ///
    /// Incremented by the background entropy generation thread on every
    /// successful push into m_fifo.  Encoded in FIFO_STATUS bits [12:8]
    /// (WPTR field) and kept in sync with the CSML register storage via the
    /// atomic FIFO_STATUS update in update_fifo_status_full().
    ///
    /// Initialized to 0 at construction and reset to 0 on every software
    /// reset (handle_write_CTRL) and on handle_reset_recovery().
    ///
    /// Functional references:
    ///   - registers.FIFO_STATUS.fields.WPTR: bits [12:8], updated on push
    ///   - "5-bit unsigned value, wraps modulo 32"
    uint8_t m_wptr;

    /// FIFO read pointer — 5-bit counter that wraps modulo 32.
    ///
    /// Incremented by handle_read_FIFO_RDATA on every successful pop from
    /// m_fifo.  Encoded in FIFO_STATUS bits [20:16] (RPTR field) and kept in
    /// sync with the CSML register storage via the atomic FIFO_STATUS update.
    ///
    /// Initialized to 0 at construction and reset to 0 on every software
    /// reset (handle_write_CTRL) and on handle_reset_recovery().
    ///
    /// Functional references:
    ///   - registers.FIFO_STATUS.fields.RPTR: bits [20:16], updated on pop
    ///   - "5-bit unsigned value, wraps modulo 32"
    uint8_t m_rptr;

    /// Mirrors FIFO_CTRL.FIFO_ENABLE (bit 0); updated by handle_write_FIFO_CTRL
    bool m_fifo_enabled;

    /// Mirrors HEALTH_TEST_CTRL health-test-enable field; updated by callback
    bool m_health_test_enabled;

    /// Set to true by handle_write_CTRL immediately before notifying m_reset_event,
    /// and cleared to false by the background thread after it receives
    /// m_reset_complete_event and completes reset recovery.
    ///
    /// Used as a reliable out-of-band signal so that the background thread can
    /// distinguish between a timed wait that expired normally and one that was
    /// interrupted by m_reset_event.  Without this flag there is no SystemC API
    /// call that reliably reports which reason caused an early return from a
    /// `wait(sc_time, sc_event)` call.
    bool m_reset_in_progress;

    /// Set to true by reset_process() when rst_ni goes low (hardware reset).
    /// The background thread checks this flag alongside m_reset_in_progress
    /// to detect and handle asynchronous hardware resets.
    bool m_hw_reset_in_progress;

    /// Startup delay captured from STARTUP_CTRL register (nanoseconds)
    uint32_t m_startup_delay_ns;


    /// Event used to wake the entropy generation thread when FIFO is re-enabled.
    ///
    /// Notified by handle_write_FIFO_CTRL when FIFO_CTRL.ENABLE transitions from
    /// 0 to 1.  The background thread waits on this event (combined with
    /// m_reset_event via sc_event_or_list) in the WAITING_FOR_ENABLE state.
    sc_core::sc_event m_fifo_fill_event;

    /// Event used to interrupt the background entropy generation thread during a
    /// software reset (FUNC-007 / FUNC-004 coordination).
    ///
    /// Notified by handle_write_CTRL immediately after the register and FIFO
    /// clearing actions are complete (before CTRL.RESET self-clears).  The
    /// background thread monitors this event through all timed and event-based
    /// wait calls so that it can break out of any wait state — WAITING_FOR_ENABLE,
    /// RUNNING iteration delay, or STARTUP_DELAY — in response to a reset.
    ///
    /// Architecture: transition RUNNING → IDLE triggered by "Software reset"
    sc_core::sc_event m_reset_event;

    /** Notified by handle_write_CTRL after Action 8 (CTRL self-clear) to signal
     *  full reset sequence completion. The background thread waits on this event
     *  in RESET_PENDING before re-deriving post-reset state.
     */
    sc_core::sc_event m_reset_complete_event;

    /// Event used to request re-evaluation of all four interrupt output ports.
    /// Notified by any context (callback or SC_THREAD) that modifies INTR_STATUS
    /// or INTR_ENABLE. The sole driver of the four sc_out<bool> ports is
    /// interrupt_output_method, which is sensitive to this event.
    sc_core::sc_event m_interrupt_update_event;

    /// TLM-2.0 quantum keeper for temporal decoupling
    tlm_utils::tlm_quantumkeeper m_qk;

    /// CSML logger for model diagnostics
    CsmlLogger logger;

    // =========================================================================
    // Constants
    // =========================================================================

    /// Maximum FIFO depth (from entropy_src-config-parameters.md)
    static constexpr unsigned int FIFO_DEPTH = 32u;

    /// INTR_STATUS / INTR_ENABLE / INTR_TEST bit positions.
    ///
    /// These values are derived from the architecture-behaviour map register
    /// definition for INTR_STATUS (0x10), INTR_ENABLE (0x14), and INTR_TEST
    /// (0x18).  All three registers share the same four active bit positions
    /// (0, 4, 8, 12) as specified in the register type definitions in
    /// entropy_src_register.h and confirmed by the architecture map entry for
    /// each register's field list.
    ///
    /// The combined mask of all four valid interrupt bits is 0x00001111.
    static constexpr uint32_t INTR_BIT_HEALTH_TEST_FAILED = (1u << 0u);
    static constexpr uint32_t INTR_BIT_FIFO_ERROR         = (1u << 4u);
    static constexpr uint32_t INTR_BIT_FIFO_OVERFLOW      = (1u << 8u);
    static constexpr uint32_t INTR_BIT_FIFO_UNDERFLOW     = (1u << 12u);

    /// Combined mask covering all four valid interrupt bit positions (0, 4, 8, 12).
    /// Used to gate W1C clear operations and INTR_TEST injection to architecturally
    /// defined bit positions only.
    static constexpr uint32_t INTR_ALL_BITS_MASK          = 0x00001111u;

    /// Base iteration period for the background entropy generation thread (ns).
    ///
    /// Represents one abstract "sample period" of the entropy peripheral at LT
    /// abstraction.  Chosen as 100 ns, which is in the range of tens-to-hundreds
    /// of nanoseconds
    ///
    /// When CTRL.DOWNSAMPLE_RATE (bits [25:16]) is non-zero, the effective period
    /// is scaled to BASE_ITERATION_PERIOD_NS * (1 + DOWNSAMPLE_RATE).
    /// When CTRL.DOWNSAMPLE_RATE is zero, the thread uses wait(SC_ZERO_TIME) to
    /// yield without advancing simulation time.
    ///
    /// Architecture map reference:
    ///   - timing_constraints: "no-cycle-accuracy; simple periodic sc_time-based delay"
    static constexpr double BASE_ITERATION_PERIOD_NS = 100.0;

    /// Software reset stabilization delay (nanoseconds).
    ///
    /// Represents the minimum 20 APB clock cycle stabilization period required
    /// after the software reset deasserts before registers are considered stable
    ///
    /// Derivation:
    ///   Nominal APB clock period = 5 ns (200 MHz APB assumed for the LT model).
    ///   Minimum stabilization = 20 APB cycles × 5 ns = 100 ns.
    ///
    /// This delay is applied as a blocking `sc_core::wait()` call inside
    /// `handle_write_CTRL` between Action 6 (interrupt port de-assertion) and
    /// Action 8 (CTRL self-clear).  Because `handle_write_CTRL` is invoked from
    /// the TLM `b_transport` handler, which runs in the SystemC SC_THREAD context
    /// of the initiator, the `wait()` call is legal and advances simulation time
    /// in a software-observable manner.
    ///
    /// Functional references:
    ///   - "minimum 20 APB clock cycles for stabilization"
    ///   - FUNC-007 description: "loosely-timed stabilization hold-off"
    static constexpr double RESET_STABILIZATION_DELAY_NS = 100.0;
};
