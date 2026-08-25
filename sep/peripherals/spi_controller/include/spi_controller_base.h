// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_controller_base.h
 * @brief Base class for SPI Controller providing register interface
 *
 * This header defines the spi_controller_base class which provides:
 * - Register memory storage using csml_memory
 * - TLM target socket for register access
 * - All hardware register instances
 * - Register reset functionality
 */

#pragma once
#include "spi_controller_register.h"
#include <string.h>

/**
 * @class spi_controller_base
 * @brief Base class for SPI Controller module providing register interface
 */
class spi_controller_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;  ///< Data type for 32-bit registers

    /**
     * @brief Constructor for spi_controller_base
     * @param name Module name for SystemC registration
     * @param type Module type string
     * @param memory_size Size of register memory space in bytes
     */
    spi_controller_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATUS(std::string(name) + ".INTR_STATUS", memory, (0x0 + 0x00)/sizeof(unsigned int)),
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)),
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)),
       CTRL(std::string(name) + ".CTRL", memory, (0x10 + 0x00)/sizeof(unsigned int)),
       STATUS(std::string(name) + ".STATUS", memory, (0x14 + 0x00)/sizeof(unsigned int)),
       CFG(std::string(name) + ".CFG", memory, (0x18 + 0x00)/sizeof(unsigned int)),
       CSID(std::string(name) + ".CSID", memory, (0x1c + 0x00)/sizeof(unsigned int)),
       CMD(std::string(name) + ".CMD", memory, (0x20 + 0x00)/sizeof(unsigned int)),
       RXDATA(std::string(name) + ".RXDATA", memory, (0x24 + 0x00)/sizeof(unsigned int)),
       TXDATA(std::string(name) + ".TXDATA", memory, (0x28 + 0x00)/sizeof(unsigned int)),
       ERROR_ENABLE(std::string(name) + ".ERROR_ENABLE", memory, (0x2c + 0x00)/sizeof(unsigned int)),
       ERROR_STATUS(std::string(name) + ".ERROR_STATUS", memory, (0x30 + 0x00)/sizeof(unsigned int)),
       EVENT_ENABLE(std::string(name) + ".EVENT_ENABLE", memory, (0x34 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;  ///< Module type identifier
      csml_memory<32> memory;  ///< Register memory storage
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;  ///< TLM target socket for register access

      spi_controller::INTR_STATUS_type<32> INTR_STATUS;  ///< Interrupt status register (W1C)
      spi_controller::INTR_ENABLE_type<32> INTR_ENABLE;  ///< Interrupt enable register
      spi_controller::INTR_TEST_type<32> INTR_TEST;  ///< Interrupt test register
      spi_controller::CTRL_type<32> CTRL;  ///< Control register (SPIEN, SW_RST, OUTPUT_EN, watermarks)
      spi_controller::STATUS_type<32> STATUS;  ///< Status register (READY, ACTIVE, FIFO flags)
      spi_controller::CFG_type<32> CFG;  ///< Configuration register (per-device timing)
      spi_controller::CSID_type<32> CSID;  ///< Chip select ID register
      spi_controller::CMD_type<32> CMD;  ///< Command register (segment descriptor)
      spi_controller::RXDATA_type<32> RXDATA;  ///< Receive FIFO data register
      spi_controller::TXDATA_type<32> TXDATA;  ///< Transmit FIFO data register
      spi_controller::ERROR_ENABLE_type<32> ERROR_ENABLE;  ///< Error interrupt enable register
      spi_controller::ERROR_STATUS_type<32> ERROR_STATUS;  ///< Error status register (W1C)
      spi_controller::EVENT_ENABLE_type<32> EVENT_ENABLE;  ///< Event interrupt enable register

      /**
       * @brief Reset all registers to their default values
       */
      void reset_all_registers();
};
