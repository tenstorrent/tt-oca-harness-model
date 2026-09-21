// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_controller_base.h
 * @brief Base class for SPI Controller providing register interface
 *
 * This header defines the spi_controller_base class which provides:
 * - Register memory storage using regmodel::Memory
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
    typedef typename regmodel::Reg<32>::DT DT;  ///< Data type for 32-bit registers

    /**
     * @brief Constructor for spi_controller_base
     * @param name Module name for SystemC registration
     * @param type Module type string
     * @param memory_size Size of register memory space in bytes
     */
    spi_controller_base(sc_module_name name, std::string type, unsigned int memory_size) : sc_module(name), type(type), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)),
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)),
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)),
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xc + 0x00)/sizeof(unsigned int)),
       CONTROL(std::string(name) + ".CONTROL", memory, (0x10 + 0x00)/sizeof(unsigned int)),
       STATUS(std::string(name) + ".STATUS", memory, (0x14 + 0x00)/sizeof(unsigned int)),
       CONFIGOPTS(std::string(name) + ".CONFIGOPTS", memory, (0x18 + 0x00)/sizeof(unsigned int)),
       CSID(std::string(name) + ".CSID", memory, (0x1c + 0x00)/sizeof(unsigned int)),
       COMMAND(std::string(name) + ".COMMAND", memory, (0x20 + 0x00)/sizeof(unsigned int)),
       RXDATA(std::string(name) + ".RXDATA", memory, (0x24 + 0x00)/sizeof(unsigned int)),
       TXDATA(std::string(name) + ".TXDATA", memory, (0x28 + 0x00)/sizeof(unsigned int)),
       ERROR_ENABLE(std::string(name) + ".ERROR_ENABLE", memory, (0x2c + 0x00)/sizeof(unsigned int)),
       ERROR_STATUS(std::string(name) + ".ERROR_STATUS", memory, (0x30 + 0x00)/sizeof(unsigned int)),
       EVENT_ENABLE(std::string(name) + ".EVENT_ENABLE", memory, (0x34 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      std::string type;  ///< Module type identifier
      regmodel::Memory<32> memory;  ///< Register memory storage
      tlm_utils::simple_target_socket<regmodel::Memory<32>, 32> target_socket;  ///< TLM target socket for register access

      spi_controller::INTR_STATE_type<32> INTR_STATE;  ///< Interrupt status register (W1C)
      spi_controller::INTR_ENABLE_type<32> INTR_ENABLE;  ///< Interrupt enable register
      spi_controller::INTR_TEST_type<32> INTR_TEST;  ///< Interrupt test register
      spi_controller::ALERT_TEST_type<32> ALERT_TEST;  ///< Alert test (WO; fatal_fault not modelled)
      spi_controller::CONTROL_type<32> CONTROL;  ///< Control register (SPIEN, SW_RST, OUTPUT_EN, watermarks)
      spi_controller::STATUS_type<32> STATUS;  ///< Status register (READY, ACTIVE, FIFO flags)
      spi_controller::CONFIGOPTS_type<32> CONFIGOPTS;  ///< Configuration register (per-device timing)
      spi_controller::CSID_type<32> CSID;  ///< Chip select ID register
      spi_controller::COMMAND_type<32> COMMAND;  ///< Command register (segment descriptor)
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
