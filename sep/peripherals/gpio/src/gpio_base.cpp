/// @file gpio_base.cpp
/// @brief GPIO Base Class Implementation
/// @author Vayavya Labs
/// @date 2025

/**
 * @addtogroup GPIO_Base
 * @{
 *
 * @section Implementation Implementation Details
 *
 * This file implements:
 * - Register reset functionality (reset_all_registers)
 * - TLM transaction filtering (b_transport_with_filter)
 * - ACCESS_FILTER checking logic (check_access_filter)
 *
 * @subsection AccessFilter ACCESS_FILTER Security Model
 *
 * The ACCESS_FILTER register (offset 0x8) provides AXI AxPROT-based security:
 * - Separate enable bits for read and write filtering
 * - Separate PROT requirements for reads (ARPROT) and writes (AWPROT)
 * - Default requirements: 0x1 (privileged/SEP access only)
 *
 * **Integration modes:**
 * - Full filtering: Use gpio_prot_extension in TLM payloads
 * - Default mode: Use set_prot_mode(0x0 or 0x1)
 * - Bypass mode: Use set_prot_mode(0xFF) to disable filtering
 *
 * @subsection ResetBehavior Reset Behavior
 *
 * reset_all_registers() resets registers to RDL-specified defaults:
 * - DATA_CTRL: 0x00000000 (all fields zero)
 * - ACCESS_FILTER: 0x00010100 (arprot_requirement=0x1, awprot_requirement=0x1)
 * - CONTROL: 0x00000002 (drive_strength=0x2 medium, others zero)
 */

#include "gpio_base.h"

/**
 * @brief Reset all registers to their RDL-specified default values
 *
 * @details Calls reset() on each register instance:
 *
 * **Reset Values:**
 * | Register | Offset | Reset Value | Description |
 * |----------|--------|-------------|-------------|
 * | DATA_CTRL | 0x00 | 0x00000000 | All control fields zero |
 * | ACCESS_FILTER | 0x08 | 0x00010100 | PROT requirements = 0x1 (SEP) |
 * | CONTROL | 0x10 | 0x00000002 | drive_strength = 0x2 (medium) |
 *
 * Called by gpio_ip::reset_handler() when rst_ni is asserted (active-low reset).
 *
 * @see gpio_register.h for register definitions and reset values
 */
void gpio_base::reset_all_registers()
{
    DATA_CTRL.reset();
    ACCESS_FILTER.reset();
    CONTROL.reset();
}

/**
 * @brief Custom b_transport with ACCESS_FILTER enforcement
 *
 * @param trans TLM generic payload (may contain gpio_prot_extension)
 * @param delay Transaction delay (passed through to csml_memory)
 *
 * @details Implements AXI AxPROT-based security filtering before forwarding transactions
 * to csml_memory for register access.
 *
 * **Transaction Flow:**
 *
 * **Step 1: Check bypass mode**
 * - If m_default_prot == 0xFF: Skip filtering, forward directly to memory
 * - Use case: Integration testing without PROT extensions
 *
 * **Step 2: Determine PROT value**
 * - Priority 1: Extract AWPROT/ARPROT from gpio_prot_extension (if attached)
 * - Priority 2: Use m_default_prot (configured via set_prot_mode)
 *
 * **Step 3: Apply ACCESS_FILTER**
 * - Exception: ACCESS_FILTER register (offset 0x8) is always accessible
 *   (allows software to configure filtering even when blocked)
 * - Call check_access_filter() to validate PROT vs requirements
 * - If blocked: Set TLM_COMMAND_ERROR_RESPONSE and return
 *
 * **Step 4: Forward to csml_memory**
 * - Allowed transactions forwarded to memory.b_transport()
 * - Register read/write performed by csml infrastructure
 *
 * **Example scenarios:**
 * @code
 * // Scenario 1: Transaction with extension (full control)
 * gpio_prot_extension ext;
 * ext.set_awprot(0x1);  // SEP write
 * trans.set_extension(&ext);
 * // Uses AWPROT=0x1 from extension
 *
 * // Scenario 2: Transaction without extension (uses default)
 * gpio.set_prot_mode(0x0);  // Unprivileged default
 * // Uses m_default_prot=0x0
 *
 * // Scenario 3: Bypass mode (integration)
 * gpio.set_prot_mode(0xFF);  // Disable filtering
 * // All transactions allowed, no filter checks
 * @endcode
 *
 * @see check_access_filter() for filtering logic details
 * @see gpio_prot_extension for PROT signal attachment
 * @see set_prot_mode() for default PROT configuration
 */
void gpio_base::b_transport_with_filter(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    // Special case: If m_default_prot == 0xFF, bypass all filtering
    // This is for integration where CPU/router cannot add PROT extension
    if (m_default_prot == 0xFF) {
        memory.b_transport(trans, delay);
        return;
    }

    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64 addr = trans.get_address();
    bool is_write = (cmd == tlm::TLM_WRITE_COMMAND);

    // Extract PROT signals from extension (if present), else use default
    uint8_t prot = m_default_prot;  // Use configured default
    gpio_prot_extension* ext = nullptr;
    trans.get_extension(ext);
    if (ext != nullptr) {
        prot = is_write ? ext->get_awprot() : ext->get_arprot();
    }

    // Check ACCESS_FILTER (unless accessing ACCESS_FILTER register itself)
    // Note: We always allow access to ACCESS_FILTER register so software can configure it
    if (addr != 0x8) {
        if (!check_access_filter(is_write, prot)) {
            // Access blocked by filter
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
    }

    // Forward to csml_memory for actual register access
    memory.b_transport(trans, delay);
}

/**
 * @brief Check if transaction passes ACCESS_FILTER requirements
 *
 * @param is_write True for write transaction, false for read
 * @param prot PROT[2:0] value from transaction (AxPROT signal)
 * @return True if access allowed, false if blocked
 *
 * @details Reads ACCESS_FILTER register and validates transaction PROT against requirements.
 *
 * **RDL Specification (gpio_intf.rdl):**
 * | Bitfield | Bits | Description | Default |
 * |----------|------|-------------|---------|
 * | write_filter_enable | [0] | Enable write filtering | 0x0 |
 * | read_filter_enable | [1] | Enable read filtering | 0x0 |
 * | awprot_requirement | [10:8] | Required AWPROT value | 0x1 |
 * | arprot_requirement | [18:16] | Required ARPROT value | 0x1 |
 *
 * **Filtering Logic:**
 *
 * For **write transactions**:
 * 1. Check write_filter_enable bit [0]
 * 2. If disabled (0): Allow write
 * 3. If enabled (1): Compare prot vs awprot_requirement[10:8]
 *    - Match: Allow
 *    - Mismatch: Block (return false)
 *
 * For **read transactions**:
 * 1. Check read_filter_enable bit [1]
 * 2. If disabled (0): Allow read
 * 3. If enabled (1): Compare prot vs arprot_requirement[18:16]
 *    - Match: Allow
 *    - Mismatch: Block (return false)
 *
 * **Example:**
 * @code
 * // Configure ACCESS_FILTER for SEP-only writes:
 * // ACCESS_FILTER = 0x00010101 (write_filter_enable=1, awprot_requirement=0x1)
 *
 * check_access_filter(true, 0x1);  // Returns true  (SEP write allowed)
 * check_access_filter(true, 0x0);  // Returns false (non-SEP write blocked)
 * check_access_filter(false, 0x0); // Returns true  (read filter disabled)
 * @endcode
 *
 * @note This method only checks filtering rules. Bypass mode (m_default_prot == 0xFF)
 *       is handled by b_transport_with_filter() before calling this method.
 *
 * @see gpio_register.h ACCESS_FILTER_type for bitfield definitions
 * @see b_transport_with_filter() for overall transaction flow
 */
bool gpio_base::check_access_filter(bool is_write, uint8_t prot)
{
    uint32_t filter_reg = ACCESS_FILTER;  // Implicit conversion via operator uint32_t()

    if (is_write) {
        // Check write filter
        bool write_filter_enable = (filter_reg & 0x1) != 0;
        if (write_filter_enable) {
            uint8_t awprot_requirement = (filter_reg >> 8) & 0x7;
            if (prot != awprot_requirement) {
                // Write access blocked
                return false;
            }
        }
    } else {
        // Check read filter
        bool read_filter_enable = (filter_reg & 0x2) != 0;
        if (read_filter_enable) {
            uint8_t arprot_requirement = (filter_reg >> 16) & 0x7;
            if (prot != arprot_requirement) {
                // Read access blocked
                return false;
            }
        }
    }

    return true;  // Access allowed
}

/** @} */ // End of GPIO_Base group
