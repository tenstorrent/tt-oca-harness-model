/// @file gpio.h
/// @brief GPIO Single-Pin Module Interface
/// @author Vayavya Labs
/// @date 2025

/**
 * @defgroup GPIO GPIO Single-Pin Module
 * @brief RDL-based GPIO controller with LT optimization
 *
 * @details Implements a single GPIO pin with:
 * - Register-based control via TLM interface
 * - LSIO interface support (alternative control path)
 * - PAD configuration outputs (drive strength, pull resistors, schmitt)
 * - Edge/level interrupt generation (configurable pulse duration for edges)
 * - Hardware strap sampling at reset
 * - SC_MANY_WRITERS optimization for direct signal writes
 *
 * This module controls ONE pin. For multi-pin GPIO, instantiate multiple
 * instances with different base addresses.
 *
 * Register Map (20 bytes per pin):
 *   - 0x00: DATA_CTRL     - Data and interface control
 *   - 0x08: ACCESS_FILTER - Security access filtering
 *   - 0x10: CONTROL       - PAD configuration and strap
 *
 * @{
 */

#pragma once
#include "gpio_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"

/**
 * @class gpio_ip
 * @brief GPIO single-pin controller with full feature set
 *
 * @details Derived from gpio_base, adds:
 * - Functional behavior (interrupt generation, output control)
 * - LSIO interface muxing
 * - Strap sampling logic
 * - LT-optimized signal updates (SC_MANY_WRITERS)
 *
 * Performance: 1 context switch per edge interrupt (66% reduction from baseline)
 */
class gpio_ip : public gpio_base
{
  public:
     SC_HAS_PROCESS(gpio_ip);
    /**
     * @brief Constructor for GPIO single-pin module
     *
     * @param name Module name for SystemC hierarchy
     * @param is_strap If true, samples input value at reset deassertion for hardware strap
     * @param log_verbosity Logger verbosity level (0=errors, 1=info, 2=debug, 3=trace)
     * @param interrupt_pulse_duration Duration of edge interrupt pulse (default 10ns, LT timing parameter)
     *
     * @details Initializes:
     * - All ports and signals
     * - SC_METHOD processes for input/LSIO monitoring and edge interrupt clear
     * - Register write callbacks for DATA_CTRL and CONTROL
     * - Logger with specified verbosity
     * - Interrupt pulse duration for proper LT timing (not tied to actual clock signal)
     */
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
    gpio_ip(sc_module_name name,
            sc_time interrupt_pulse_duration = sc_time(10, SC_NS));

    /// Destructor
    ~gpio_ip();

    /**
     * @name GPIO Pin Interfaces
     * @brief Single pin I/O signals
     * @{
     */

    /**
     * @brief GPIO output value signal
     * @details Driven based on priority:
     * 1. Register control (if DATA_CTRL.interface_enable=1)
     * 2. LSIO control (if LSIO active and not disabled)
     * 3. Tristated (neither active)
     *
     * Uses SC_MANY_WRITERS for direct writes (LT optimization)
     */
    sc_out<bool> gpio_out_o;

    /**
     * @brief GPIO output enable signal
     * @details Controlled by DATA_CTRL.enable_rx_tx:
     * - 0b01 = TX mode (output enabled)
     * - Others = output disabled
     *
     * Uses SC_MANY_WRITERS for direct writes (LT optimization)
     */
    sc_out<bool> gpio_oe_o;

    /**
     * @brief GPIO input signal from PAD
     * @details Value written to DATA_CTRL.pad2core[31] when RX enabled
     * - Monitored by input_monitor() SC_METHOD
     * - Used for interrupt generation and strap sampling
     */
    sc_in<bool> gpio_in_i;

    /** @} */ // End of GPIO Pin Interfaces

    /**
     * @name LSIO Interface
     * @brief Low-Speed I/O alternative control path
     *
     * @details LSIO provides hardware function control (e.g., I2C SDA).
     * Software control (interface_enable) has priority over LSIO.
     * @{
     */

    /// LSIO output value (from LSIO controller to PAD)
    sc_in<bool> lsio_gpio_out_i;

    /// LSIO output enable (from LSIO controller)
    sc_in<bool> lsio_gpio_oe_i;

    /**
     * @brief LSIO input feedback signal
     * @details Driven with current gpio_in_i value (always updated)
     * Uses SC_MANY_WRITERS for direct writes (LT optimization)
     */
    sc_out<bool> lsio_gpio_in_o;

    /**
     * @brief LSIO access indicator
     * @details When high, LSIO is actively controlling the pin.
     * Updates DATA_CTRL.lsio_enable[25] (read-only status bit).
     * Monitored by lsio_monitor() SC_METHOD.
     */
    sc_in<bool> lsio_access_i;

    /** @} */ // End of LSIO Interface

    /**
     * @name PAD Configuration Outputs
     * @brief Physical PAD electrical configuration
     *
     * @details Controlled by CONTROL register (offset 0x10).
     * Only active when CONTROL.config_enable=1, otherwise defaults used.
     * Uses SC_MANY_WRITERS for direct writes (LT optimization).
     * @{
     */

    /// PAD drive strength (3 bits): 0x0=weakest to 0x7=strongest, default 0x2=medium
    sc_out<sc_uint<3>> pad_drive_strength_o;

    /// PAD pull resistor enable: true=enabled, false=disabled (high-Z)
    sc_out<bool> pad_pull_enable_o;

    /// PAD pull resistor direction: false=pull-down, true=pull-up
    sc_out<bool> pad_pull_select_o;

    /// PAD Schmitt trigger enable: true=enabled for noise immunity
    sc_out<bool> pad_schmitt_enable_o;

    /** @} */ // End of PAD Configuration Outputs

    /**
     * @name Interrupt Output
     * @{
     */

    /**
     * @brief Interrupt output signal to PLIC/interrupt controller
     *
     * @details Behavior per DATA_CTRL.interrupt_type:
     * - Type 0: Level-high (HIGH when input=1, auto-clears when input=0)
     * - Type 1: Level-low (HIGH when input=0, auto-clears when input=1)
     * - Type 2: Rising-edge (pulse on 0→1 transition, auto-clears after pulse duration)
     * - Type 3: Falling-edge (pulse on 1→0 transition, auto-clears after pulse duration)
     *
     * Only active when DATA_CTRL.interrupt_enable=1 and enable_rx_tx=0b10 (RX mode).
     * Uses SC_MANY_WRITERS for direct writes (LT optimization).
     */
    sc_out<bool> interrupt_o;

    /** @} */ // End of Interrupt Output

    /**
     * @name Reset Signal
     * @{
     */

    /// Active-low reset signal (OpenTitan convention): LOW=reset, HIGH=normal
    sc_in<bool> rst_ni;

    /** @} */ // End of Reset Signal

    csml_param<int> verbosity;      ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
    csml_param<bool> is_strap;     ///< Hardware strap pin mode: true=sample input at reset deassertion
    /// CSML logger instance for debug/trace output
    CsmlLogger logger;

  private:
    /**
     * @name Internal State
     * @brief Private member variables for state tracking
     * @{
     */

    /// Strap pin indicator: true if this pin samples input at reset deassertion
    bool m_is_strap;

    /// Previous input value for edge detection (0→1 or 1→0 transitions)
    bool m_prev_input;

    /// Current interrupt state: true=interrupt active, false=inactive
    bool m_interrupt_state;

    /// Strap sampling flag: true after first reset deassertion (prevents re-sampling)
    bool m_strap_sampled;

    /// Interrupt pulse duration for edge interrupts (LT timing parameter, not tied to actual clock)
    sc_time m_interrupt_pulse_duration;

    /** @} */ // End of Internal State


    /**
     * @name SystemC Events
     * @brief Events for SC_METHOD scheduling
     * @{
     */

    /**
     * @brief Edge interrupt auto-clear event
     * @details Scheduled after interrupt pulse duration when edge interrupt triggers.
     * Notifies edge_interrupt_clear_method() to clear interrupt_o.
     * Only event needed due to SC_MANY_WRITERS optimization.
     */
    sc_event m_edge_interrupt_clear_event;

    /** @} */ // End of SystemC Events

    /**
     * @name SystemC Processes
     * @brief SC_METHOD processes for event-driven behavior
     * @{
     */

    /**
     * @brief Reset handler SC_METHOD
     * @details Sensitive to rst_ni signal.
     * On reset assert: Resets all registers and internal state.
     * On reset deassert: Samples strap value if m_is_strap=true.
     */
    void reset_handler();

    /**
     * @brief Input monitor SC_METHOD
     * @details Sensitive to gpio_in_i signal.
     * - Updates DATA_CTRL.pad2core[31] when RX enabled
     * - Updates lsio_gpio_in_o feedback
     * - Detects edges (0→1 or 1→0) for interrupt generation
     * - Triggers level interrupts based on current input
     */
    void input_monitor();

    /**
     * @brief LSIO monitor SC_METHOD
     * @details Sensitive to lsio_access_i signal.
     * Updates DATA_CTRL.lsio_enable[25] status bit (read-only).
     * Calls update_output() when LSIO access changes.
     */
    void lsio_monitor();

    /**
     * @brief Edge interrupt auto-clear SC_METHOD
     * @details Sensitive to m_edge_interrupt_clear_event (after pulse duration).
     * Clears m_interrupt_state and writes false to interrupt_o.
     * Implements "pulse interrupt that auto-clears" per customer spec.
     */
    void edge_interrupt_clear_method();

    /** @} */ // End of SystemC Processes

    /**
     * @name Register Callbacks
     * @brief TLM register write callbacks
     * @{
     */

    /**
     * @brief Register write callbacks with csml_memory
     * @details Called from constructor. Registers lambdas for:
     * - DATA_CTRL (offset 0x0)
     * - CONTROL (offset 0x10)
     */
    void register_callbacks();

    /**
     * @brief DATA_CTRL register read callback
     * @param value Output: register value to return to caller
     * @return true (callback provides the value)
     * @details Synchronously updates pad2core before returning register value.
     * LT modeling: zero PAD propagation delay (excluded per spec section 3.2.2).
     * - TX mode: pad2core = core2pad (output drives PAD, PAD feeds back instantly)
     * - RX/disabled: pad2core = gpio_in_i.read() (actual input reflected)
     */
    bool handle_read_DATA_CTRL(uint32_t& value);

    /**
     * @brief DATA_CTRL register write callback
     * @param value New register value
     * @param write_mask Writable bits mask
     * @return false (callback handles write manually)
     * @details Updates gpio_out_o, gpio_oe_o, and interrupt logic.
     * Uses direct signal writes (SC_MANY_WRITERS).
     */
    bool handle_write_DATA_CTRL(uint32_t value, uint32_t write_mask);

    /**
     * @brief CONTROL register write callback
     * @param value New register value
     * @param write_mask Writable bits mask
     * @return false (callback handles write manually)
     * @details Updates PAD configuration signals (drive, pull, schmitt).
     * Uses direct signal writes (SC_MANY_WRITERS).
     */
    bool handle_write_CONTROL(uint32_t value, uint32_t write_mask);

    /** @} */ // End of Register Callbacks

    /**
     * @name Helper Methods
     * @brief Internal helper functions for signal updates
     * @{
     */

    /**
     * @brief Update GPIO output signals
     * @details Computes and writes gpio_out_o and gpio_oe_o based on:
     * - DATA_CTRL.interface_enable (software control priority)
     * - LSIO interface state (lsio_select, lsio_enable, lsio_disable)
     * - DATA_CTRL.core2pad and enable_rx_tx values
     *
     * Uses direct writes (SC_MANY_WRITERS, LT optimization).
     */
    void update_output();

    /**
     * @brief Update PAD configuration outputs
     * @details Reads CONTROL register and writes to:
     * - pad_drive_strength_o, pad_pull_enable_o,
     * - pad_pull_select_o, pad_schmitt_enable_o
     *
     * Only active when CONTROL.config_enable=1.
     * Uses direct writes (SC_MANY_WRITERS, LT optimization).
     */
    void update_pad_config();

    /**
     * @brief Update interrupt output signal
     * @details Writes interrupt_o based on:
     * - DATA_CTRL.interrupt_enable
     * - m_interrupt_state (internal latch)
     *
     * Uses direct write (SC_MANY_WRITERS, LT optimization).
     */
    void update_interrupt();

    /**
     * @brief Re-evaluate interrupt condition
     * @details Called when interrupt type or enable changes.
     * For level interrupts: Updates m_interrupt_state based on current input.
     * For edge interrupts: Preserves current state (don't re-trigger).
     */
    void reevaluate_interrupt_condition();

    /** @} */ // End of Helper Methods
};

/** @} */ // End of GPIO group
