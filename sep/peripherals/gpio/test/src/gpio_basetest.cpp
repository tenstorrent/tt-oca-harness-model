/**
 * GPIO Base Test Implementation (RDL-Based Single-Pin)
 *
 * Defines the register map for the RDL-based GPIO model.
 */

#include "gpio_basetest.h"

/**
 * Register Map Table
 *
 * Contains properties for all 3 RDL registers:
 * - Register offset
 * - Read mask (which bits can be read)
 * - Write mask (which bits can be written)
 * - Reset value (default after reset)
 * - Register name
 */
gpio_basetest::Register_Property_t reg_map[3] = {
    // DATA_CTRL Register (0x00)
    // - Data and interface control
    // - Read: pad2core[31], lsio_enable[25], RW fields
    // - Write: Only RW fields (not RO bits)
    // - Reset: All zeros
    {
        gpio_basetest::DATA_CTRL_OFFSET,
        gpio_basetest::DATA_CTRL_READ,
        gpio_basetest::DATA_CTRL_WRITE,
        gpio_basetest::DATA_CTRL_RESET,
        "DATA_CTRL"
    },

    // ACCESS_FILTER Register (0x08)
    // - Security access filtering
    // - Read/Write: All fields are RW
    // - Reset: arprot_requirement=0x1, awprot_requirement=0x1
    {
        gpio_basetest::ACCESS_FILTER_OFFSET,
        gpio_basetest::ACCESS_FILTER_READ,
        gpio_basetest::ACCESS_FILTER_WRITE,
        gpio_basetest::ACCESS_FILTER_RESET,
        "ACCESS_FILTER"
    },

    // CONTROL Register (0x10)
    // - PAD configuration and strap sampling
    // - Read: strap_value[23], strap_valid[22], RW fields
    // - Write: Only RW fields (not strap bits)
    // - Reset: drive_strength=0x2 (medium drive)
    {
        gpio_basetest::CONTROL_OFFSET,
        gpio_basetest::CONTROL_READ,
        gpio_basetest::CONTROL_WRITE,
        gpio_basetest::CONTROL_RESET,
        "CONTROL"
    }
};
