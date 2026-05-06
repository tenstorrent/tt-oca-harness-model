/// @file gpio_base.h
/// @brief GPIO Base Class with TLM Infrastructure and Register Definitions
/// @author Vayavya Labs
/// @date 2025

/**
 * @defgroup GPIO_Base GPIO Base Infrastructure
 * @brief TLM target socket and register memory infrastructure
 *
 * @details Provides the register storage and TLM transaction handling for GPIO.
 * This base class contains:
 * - csml_memory (20 bytes for 3 registers per pin)
 * - TLM target socket with ACCESS_FILTER enforcement
 * - 3 register instances (DATA_CTRL, ACCESS_FILTER, CONTROL)
 * - Reset functionality
 * - AXI AxPROT-based security filtering
 *
 * Derived classes (gpio_ip) add functional behavior on top of this infrastructure.
 *
 * @section RegOffsets Register Address Map
 * | Offset | Size | Register | Description |
 * |--------|------|----------|-------------|
 * | 0x00 | 8B | DATA_CTRL | Data and interface control |
 * | 0x08 | 8B | ACCESS_FILTER | Security access filtering |
 * | 0x10 | 8B | CONTROL | PAD configuration and strap |
 * | Total | 20B | | Fixed memory size per pin |
 *
 * @{
 */

#pragma once
#include "gpio_register.h"
#include "gpio_prot_extension.h"
#include <string.h>

/**
 * @class gpio_base
 * @brief GPIO Base Class with TLM Infrastructure
 *
 * @details Base class for GPIO single-pin module containing:
 * - Register memory infrastructure (csml_memory)
 * - TLM target socket with custom b_transport
 * - ACCESS_FILTER enforcement for security
 * - Three 64-bit registers (DATA_CTRL, ACCESS_FILTER, CONTROL)
 *
 * This class provides the register storage and TLM transaction handling.
 * Derived classes (gpio_ip) add functional behavior (interrupts, output control, etc.).
 *
 * @par Memory Layout
 * Fixed 20-byte memory region (5 x 32-bit words) per pin:
 * - Offset 0x00: DATA_CTRL register
 * - Offset 0x08: ACCESS_FILTER register
 * - Offset 0x10: CONTROL register
 *
 * @par ACCESS_FILTER Enforcement
 * b_transport_with_filter() checks AxPROT signals against ACCESS_FILTER settings.
 * Transactions can be blocked if PROT values don't match requirements.
 * Use set_prot_mode(0xFF) to disable filtering for integration testing.
 */
class gpio_base : public sc_module
{
  public:
    /// Data type for register operations
    typedef typename csml_reg<32>::DT DT;

    /**
     * @brief Constructor for GPIO base infrastructure
     * @param name Module name for SystemC hierarchy
     *
     * @details Initializes:
     * - csml_memory with fixed 20-byte size
     * - TLM target socket with custom b_transport handler
     * - Three register instances (DATA_CTRL, ACCESS_FILTER, CONTROL)
     *
     * @note Memory size is fixed at 20 bytes for 3 registers per pin.
     *       Constructor no longer takes memory_size parameter.
     */
    gpio_base(sc_module_name name) :
        sc_module(name),
        memory(std::string(name) + ".Memory", 20/sizeof(unsigned int)),  // 20 bytes = 5 words
        target_socket("target_socket"),
        DATA_CTRL(std::string(name) + ".DATA_CTRL", memory, 0x0/sizeof(unsigned int)),
        ACCESS_FILTER(std::string(name) + ".ACCESS_FILTER", memory, 0x8/sizeof(unsigned int)),
        CONTROL(std::string(name) + ".CONTROL", memory, 0x10/sizeof(unsigned int))
    {
        // Register custom b_transport to enforce ACCESS_FILTER
        target_socket.register_b_transport(this, &gpio_base::b_transport_with_filter);
    }

    /**
     * @name Register Infrastructure
     * @brief TLM memory and socket for register access
     * @{
     */

    /// CSML memory storage (20 bytes for 3 registers)
    csml_memory<32> memory;

    /// TLM target socket with custom b_transport for ACCESS_FILTER enforcement
    tlm_utils::simple_target_socket<gpio_base, 32> target_socket;

    /** @} */ // End of Register Infrastructure

    /**
     * @name Register Instances
     * @brief Three RDL-based 64-bit registers per GPIO pin
     * @{
     */

    /// DATA_CTRL register (Offset 0x0) - Data and interface control
    gpio::DATA_CTRL_type<32> DATA_CTRL;

    /// ACCESS_FILTER register (Offset 0x8) - Security filtering based on AxPROT
    gpio::ACCESS_FILTER_type<32> ACCESS_FILTER;

    /// CONTROL register (Offset 0x10) - PAD configuration and hardware strap
    gpio::CONTROL_type<32> CONTROL;

    /** @} */ // End of Register Instances

    /**
     * @brief Reset all registers to their RDL-specified default values
     *
     * @details Resets:
     * - DATA_CTRL: All fields to 0x0
     * - ACCESS_FILTER: awprot_requirement=0x1, arprot_requirement=0x1
     * - CONTROL: drive_strength=0x2, others to 0x0
     *
     * Called by gpio_ip::reset_handler() on rst_ni assertion.
     */
    void reset_all_registers();

    /**
     * @brief Set default PROT mode for transactions without extension
     *
     * @param prot Default PROT[2:0] value for transactions
     *
     * @details This allows integration without modifying CPU/router to add gpio_prot_extension.
     *
     * Valid values:
     * - 0x0 = Unprivileged (default) - Non-SEP transactions
     * - 0x1 = Privileged - SEP transactions only
     * - 0xFF = **Disable ACCESS_FILTER enforcement** (bypass filtering)
     *
     * @par Usage Examples
     * @code
     * // For integration testing (disable filtering):
     * gpio.set_prot_mode(0xFF);
     *
     * // For SEP-only mode (privileged required):
     * gpio.set_prot_mode(0x1);
     *
     * // For non-SEP mode (unprivileged):
     * gpio.set_prot_mode(0x0);
     * @endcode
     *
     * @note When gpio_prot_extension is present in TLM payload, it overrides this default.
     */
    void set_prot_mode(uint8_t prot) {
        m_default_prot = prot;
    }

    /**
     * @brief Get current default PROT mode
     * @return Current m_default_prot value (0x0, 0x1, or 0xFF)
     */
    uint8_t get_prot_mode() const {
        return m_default_prot;
    }

    /**
     * @brief Custom b_transport with ACCESS_FILTER enforcement
     *
     * @param trans TLM generic payload (may contain gpio_prot_extension)
     * @param delay Transaction delay (passed to csml_memory)
     *
     * @details This method implements AXI AxPROT-based security filtering:
     *
     * **Transaction Flow:**
     * 1. Extract PROT signals from gpio_prot_extension (if present)
     * 2. Use m_default_prot if no extension attached
     * 3. Check ACCESS_FILTER register settings (unless m_default_prot == 0xFF)
     * 4. Block access if PROT doesn't match awprot_requirement/arprot_requirement
     * 5. Forward allowed transactions to csml_memory for register access
     *
     * **Blocked transactions:**
     * - Response status: TLM_COMMAND_ERROR_RESPONSE
     * - No register access performed
     * - Error message logged
     *
     * **Bypass mode:**
     * - set_prot_mode(0xFF) disables filtering entirely
     * - All transactions forwarded directly to csml_memory
     *
     * @see check_access_filter() for filtering logic
     * @see gpio_prot_extension for PROT signal attachment
     */
    virtual void b_transport_with_filter(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

  protected:
    /**
     * @brief Check if transaction passes ACCESS_FILTER requirements
     *
     * @param is_write True for write transaction, false for read
     * @param prot PROT[2:0] value from transaction (AxPROT signal)
     * @return True if access allowed, false if blocked
     *
     * @details Reads ACCESS_FILTER register and compares:
     * - For writes: Checks write_filter_enable[0] and awprot_requirement[10:8]
     * - For reads: Checks read_filter_enable[1] and arprot_requirement[18:16]
     *
     * **Filtering Rules:**
     * - If filter disabled (enable bit = 0): Allow
     * - If filter enabled (enable bit = 1):
     *   - Allow if prot == requirement
     *   - Block if prot != requirement
     *
     * **Example:**
     * @code
     * // ACCESS_FILTER = 0x00010101 (write filtering enabled, awprot_requirement=0x1)
     * check_access_filter(true, 0x1);  // Returns true (SEP allowed)
     * check_access_filter(true, 0x0);  // Returns false (non-SEP blocked)
     * @endcode
     */
    bool check_access_filter(bool is_write, uint8_t prot);

  private:
    /**
     * @brief Default PROT value for transactions without gpio_prot_extension
     *
     * @details Valid values:
     * - 0x0 = Unprivileged (default) - Non-SEP transactions
     * - 0x1 = Privileged - SEP transactions only
     * - 0xFF = Disable ACCESS_FILTER enforcement (bypass mode)
     *
     * Modified via set_prot_mode() and retrieved via get_prot_mode().
     * Overridden by gpio_prot_extension when present in TLM payload.
     */
    uint8_t m_default_prot = 0x0;
};

/** @} */ // End of GPIO_Base group
