/// @file gpio_register.h
/// @brief GPIO Register Type Definitions (RDL-Based)
/// @author Vayavya Labs
/// @date 2025

/**
 * @defgroup GPIO_Registers GPIO Register Definitions
 * @brief RDL-based register types with bitfield definitions
 *
 * @details This file defines register templates for a single GPIO pin based on
 * SystemRDL specifications (gpio_intf.rdl, gpio_ctrl.rdl, gpio_wrap.rdl).
 *
 * Each register includes:
 * - Read/write masks (enforces RO/RW permissions)
 * - Reset values (per RDL defaults)
 * - Bitfield accessors (csml_bitfield with correct positions)
 * - Reserved bits properly excluded from masks
 *
 * @section RegMap Register Map (20 bytes per pin)
 * | Offset | Name | Description |
 * |--------|------|-------------|
 * | 0x00 | DATA_CTRL | Data and interface control |
 * | 0x08 | ACCESS_FILTER | Security access filtering |
 * | 0x10 | CONTROL | PAD configuration and strap |
 *
 * @{
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "csml_register.h"

/**
 * @namespace gpio
 * @brief GPIO register type namespace
 */
namespace gpio {

/**
 * @class DATA_CTRL_type
 * @brief DATA_CTRL Register (Offset 0x0)
 *
 * @details Controls GPIO pin data, direction, interface selection, and interrupts.
 *
 * @par Bitfield Layout
 * | Bits | Name | Access | Reset | Description |
 * |------|------|--------|-------|-------------|
 * | [0] | core2pad | RW | 0x0 | Output value to PAD |
 * | [5:4] | enable_rx_tx | RW | 0x0 | Direction: 00=neither, 01=TX, 10=RX, 11=neither |
 * | [16] | interface_enable | RW | 0x0 | Use register control (overrides LSIO) |
 * | [17] | lsio_select | RW | 0x0 | Force LSIO interface |
 * | [18] | interrupt_enable | RW | 0x0 | Enable interrupt generation |
 * | [19] | lsio_disable | RW | 0x0 | Block LSIO access |
 * | [21:20] | interrupt_type | RW | 0x0 | 0=lvl-high, 1=lvl-low, 2=rising, 3=falling |
 * | [25] | lsio_enable | RO | 0x0 | LSIO is currently driving (HW status) |
 * | [31] | pad2core | RO | 0x0 | Input value from PAD (HW status) |
 *
 * @par Reserved Bits
 * Bits 1-3, 6-15, 22-24, 26-30 are reserved and excluded from read/write masks.
 *
 * @tparam N Register width in bits (typically 32)
 */
template<unsigned int N>
class DATA_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    DATA_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset,
                  0x823F0031,  // read_mask: pad2core[31], lsio_enable[25], all RW fields
                  0x003F0031,  // write_mask: only RW fields (not RO bits)
                  0x00000000), // reset value
      core2pad(reg_name + ".core2pad", *this, 0, 1),
      enable_rx_tx(reg_name + ".enable_rx_tx", *this, 4, 2),
      interface_enable(reg_name + ".interface_enable", *this, 16, 1),
      lsio_select(reg_name + ".lsio_select", *this, 17, 1),
      interrupt_enable(reg_name + ".interrupt_enable", *this, 18, 1),
      lsio_disable(reg_name + ".lsio_disable", *this, 19, 1),
      interrupt_type(reg_name + ".interrupt_type", *this, 20, 2),
      lsio_enable(reg_name + ".lsio_enable", *this, 25, 1),
      pad2core(reg_name + ".pad2core", *this, 31, 1)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> core2pad;           // [0]     RW - Output to PAD
    csml_bitfield<N> enable_rx_tx;       // [5:4]   RW - Direction control
    csml_bitfield<N> interface_enable;   // [16]    RW - Enable register control
    csml_bitfield<N> lsio_select;        // [17]    RW - Force LSIO
    csml_bitfield<N> interrupt_enable;   // [18]    RW - Enable interrupt
    csml_bitfield<N> lsio_disable;       // [19]    RW - Block LSIO
    csml_bitfield<N> interrupt_type;     // [21:20] RW - Interrupt mode
    csml_bitfield<N> lsio_enable;        // [25]    RO - LSIO driving (HW writes)
    csml_bitfield<N> pad2core;           // [31]    RO - Input from PAD (HW writes)
};

/**
 * @class ACCESS_FILTER_type
 * @brief ACCESS_FILTER Register (Offset 0x8)
 *
 * @details Security access filtering based on AXI-Lite AxPROT bus signals.
 *
 * @par Bitfield Layout
 * | Bits | Name | Access | Reset | Description |
 * |------|------|--------|-------|-------------|
 * | [0] | write_filter_enable | RW | 0x0 | Enable write filtering |
 * | [1] | read_filter_enable | RW | 0x0 | Enable read filtering |
 * | [10:8] | awprot_requirement | RW | 0x1 | Required AWPROT[2:0] for writes |
 * | [18:16] | arprot_requirement | RW | 0x1 | Required ARPROT[2:0] for reads |
 *
 * @par Reserved Bits
 * Bits 2-7, 11-15, 19-31 are reserved and excluded from read/write masks.
 *
 * @par Implementation Note
 * Filtering is fully enforced via gpio_base::b_transport_with_filter().
 * Transactions with gpio_prot_extension provide AWPROT/ARPROT values.
 * Blocked transactions return TLM_COMMAND_ERROR_RESPONSE.
 *
 * @tparam N Register width in bits (typically 32)
 */
template<unsigned int N>
class ACCESS_FILTER_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    ACCESS_FILTER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset,
                  0x00070703,  // read_mask: all RW fields
                  0x00070703,  // write_mask: all RW fields
                  0x00010100), // reset: arprot_requirement=0x1, awprot_requirement=0x1
      write_filter_enable(reg_name + ".write_filter_enable", *this, 0, 1),
      read_filter_enable(reg_name + ".read_filter_enable", *this, 1, 1),
      awprot_requirement(reg_name + ".awprot_requirement", *this, 8, 3),
      arprot_requirement(reg_name + ".arprot_requirement", *this, 16, 3)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> write_filter_enable;  // [0]     RW - Enable write filtering
    csml_bitfield<N> read_filter_enable;   // [1]     RW - Enable read filtering
    csml_bitfield<N> awprot_requirement;   // [10:8]  RW - AWPROT requirement
    csml_bitfield<N> arprot_requirement;   // [18:16] RW - ARPROT requirement
};

/**
 * @class CONTROL_type
 * @brief CONTROL Register (Offset 0x10)
 *
 * @details PAD configuration (drive strength, pull resistors, schmitt trigger) and
 * hardware strap sampling status.
 *
 * @par Bitfield Layout
 * | Bits | Name | Access | Reset | Description |
 * |------|------|--------|-------|-------------|
 * | [2:0] | drive_strength | RW | 0x2 | PAD drive strength (0x0=weakest to 0x7=strongest) |
 * | [7] | pull_enable_n0_scan | RW | 0x0 | Enable pull resistor |
 * | [8] | pull_select | RW | 0x0 | Pull direction: 0=pull-down, 1=pull-up |
 * | [10] | schmitt_select | RW | 0x0 | Enable schmitt trigger for noise immunity |
 * | [15] | config_enable | RW | 0x0 | Enable PAD config from registers |
 * | [22] | strap_valid | RO | 0x0 | Strap valid indicator (HW writes) |
 * | [23] | strap_value | RO | 0x0 | Captured strap value (HW writes) |
 *
 * @par Reserved Bits
 * Bits 3-6, 9, 11-14, 16-21, 24-31 are reserved and excluded from read/write masks.
 *
 * @par Strap Sampling
 * For strap pins (is_strap=true), strap_value captures gpio_in_i at reset deassertion.
 * strap_valid indicates successful sampling (set to 1 when captured).
 *
 * @tparam N Register width in bits (typically 32)
 */
template<unsigned int N>
class CONTROL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    CONTROL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset,
                  0x00C08587,  // read_mask: strap_value[23], strap_valid[22], all RW fields
                  0x00008587,  // write_mask: only RW fields (not strap bits)
                  0x00000002), // reset: drive_strength=0x2 (medium)
      drive_strength(reg_name + ".drive_strength", *this, 0, 3),
      pull_enable_n0_scan(reg_name + ".pull_enable_n0_scan", *this, 7, 1),
      pull_select(reg_name + ".pull_select", *this, 8, 1),
      schmitt_select(reg_name + ".schmitt_select", *this, 10, 1),
      config_enable(reg_name + ".config_enable", *this, 15, 1),
      strap_valid(reg_name + ".strap_valid", *this, 22, 1),
      strap_value(reg_name + ".strap_value", *this, 23, 1)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> drive_strength;       // [2:0]  RW - PAD drive strength
    csml_bitfield<N> pull_enable_n0_scan;  // [7]    RW - Enable pull resistor
    csml_bitfield<N> pull_select;          // [8]    RW - Pull up/down select
    csml_bitfield<N> schmitt_select;       // [10]   RW - Enable schmitt trigger
    csml_bitfield<N> config_enable;        // [15]   RW - Enable PAD config
    csml_bitfield<N> strap_valid;          // [22]   RO - Strap valid (HW writes)
    csml_bitfield<N> strap_value;          // [23]   RO - Strap value (HW writes)
};

} // namespace gpio

/** @} */ // End of GPIO_Registers group
