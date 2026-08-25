// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class spi_controller_basetest
 * @brief Base test class for SPI Controller
 */
class spi_controller_basetest : public sc_module
{
  public:
    /// TLM initiator socket for register access
    tlm_utils::simple_initiator_socket<spi_controller_basetest, 32> initiator_socket;

    /// Register offset enumeration
    enum Register_offset
    {
      INTR_STATUS_OFFSET = (0x0 + 0x00),
      INTR_ENABLE_OFFSET = (0x4 + 0x00),
      INTR_TEST_OFFSET = (0x8 + 0x00),
      CTRL_OFFSET = (0x10 + 0x00),
      STATUS_OFFSET = (0x14 + 0x00),
      CFG_OFFSET = (0x18 + 0x00),
      CSID_OFFSET = (0x1c + 0x00),
      CMD_OFFSET = (0x20 + 0x00),
      RXDATA_OFFSET = (0x24 + 0x00),
      TXDATA_OFFSET = (0x28 + 0x00),
      ERROR_ENABLE_OFFSET = (0x2c + 0x00),
      ERROR_STATUS_OFFSET = (0x30 + 0x00),
      EVENT_ENABLE_OFFSET = (0x34 + 0x00)
    };

    /// Register read access mask enumeration
    enum Register_Read_Access
    {
      INTR_STATUS_READ = (0x11),
      INTR_ENABLE_READ = (0x11),
      INTR_TEST_READ = (0x11),
      CTRL_READ = (0xe000ffff),
      STATUS_READ = (0xffefffff),
      CFG_READ = (0xefffffff),
      CSID_READ = (0xffffffff),
      CMD_READ = (0x0),
      RXDATA_READ = (0xffffffff),
      TXDATA_READ = (0x0),
      ERROR_ENABLE_READ = (0x11111),
      ERROR_STATUS_READ = (0x111111),
      EVENT_ENABLE_READ = (0x111111)
    };

    /// Register write access mask enumeration
    enum Register_Write_Access
    {
      INTR_STATUS_WRITE = (0x11),
      INTR_ENABLE_WRITE = (0x11),
      INTR_TEST_WRITE = (0x11),
      CTRL_WRITE = (0xe000ffff),
      STATUS_WRITE = (0x0),
      CFG_WRITE = (0xefffffff),
      CSID_WRITE = (0xffffffff),
      CMD_WRITE = (0x3fff),
      RXDATA_WRITE = (0x0),
      TXDATA_WRITE = (0xffffffff),
      ERROR_ENABLE_WRITE = (0x11111),
      ERROR_STATUS_WRITE = (0x111111),
      EVENT_ENABLE_WRITE = (0x111111)
    };

    /// Register reset value enumeration
    enum Register_Reset_Val
    {
      INTR_STATUS_RESET = (0),
      INTR_ENABLE_RESET = (0),
      INTR_TEST_RESET = (0),
      CTRL_RESET = (127),
      STATUS_RESET = (0),
      CFG_RESET = (0),
      CSID_RESET = (0),
      CMD_RESET = (0),
      RXDATA_RESET = (0),
      TXDATA_RESET = (0),
      ERROR_ENABLE_RESET = (0x11111),
      ERROR_STATUS_RESET = (0),
      EVENT_ENABLE_RESET = (0)
    };

    /// Register property structure
    struct Register_Property_t
    {
		  unsigned int reg_offset;   /// Register offset
		  unsigned int read_mask;    /// Read access mask
		  unsigned int write_mask;   /// Write access mask
		  unsigned int reg_reset;    /// Reset value
		  std::string reg_name;      /// Register name
    };

    /**
     * @brief Base test class for SPI Controller
     * @param name SystemC module name
     */
    spi_controller_basetest(sc_module_name name) : sc_module(name)
    {

    }

};