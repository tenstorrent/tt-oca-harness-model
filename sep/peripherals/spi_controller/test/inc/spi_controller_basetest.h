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
      INTR_STATE_OFFSET = (0x0 + 0x00),
      INTR_ENABLE_OFFSET = (0x4 + 0x00),
      INTR_TEST_OFFSET = (0x8 + 0x00),
      ALERT_TEST_OFFSET = (0xc + 0x00),
      CONTROL_OFFSET = (0x10 + 0x00),
      STATUS_OFFSET = (0x14 + 0x00),
      CONFIGOPTS_OFFSET = (0x18 + 0x00),
      CSID_OFFSET = (0x1c + 0x00),
      COMMAND_OFFSET = (0x20 + 0x00),
      RXDATA_OFFSET = (0x24 + 0x00),
      TXDATA_OFFSET = (0x28 + 0x00),
      ERROR_ENABLE_OFFSET = (0x2c + 0x00),
      ERROR_STATUS_OFFSET = (0x30 + 0x00),
      EVENT_ENABLE_OFFSET = (0x34 + 0x00)
    };

    /// Register read access mask enumeration
    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x3),
      INTR_ENABLE_READ = (0x3),
      INTR_TEST_READ = (0x3),
      ALERT_TEST_READ = (0x0),
      CONTROL_READ = (0xe000ffff),
      STATUS_READ = (0xffefffff),
      CONFIGOPTS_READ = (0xefffffff),
      CSID_READ = (0xffffffff),
      COMMAND_READ = (0x0),
      RXDATA_READ = (0xffffffff),
      TXDATA_READ = (0x0),
      ERROR_ENABLE_READ = (0x1f),
      ERROR_STATUS_READ = (0x3f),
      EVENT_ENABLE_READ = (0x3f)
    };

    /// Register write access mask enumeration
    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x3),
      INTR_ENABLE_WRITE = (0x3),
      INTR_TEST_WRITE = (0x3),
      ALERT_TEST_WRITE = (0x1),
      CONTROL_WRITE = (0xe000ffff),
      STATUS_WRITE = (0x0),
      CONFIGOPTS_WRITE = (0xefffffff),
      CSID_WRITE = (0xffffffff),
      COMMAND_WRITE = (0x1ffffff),
      RXDATA_WRITE = (0x0),
      TXDATA_WRITE = (0xffffffff),
      ERROR_ENABLE_WRITE = (0x1f),
      ERROR_STATUS_WRITE = (0x3f),
      EVENT_ENABLE_WRITE = (0x3f)
    };

    /// Register reset value enumeration
    enum Register_Reset_Val
    {
      INTR_STATE_RESET = (0),
      INTR_ENABLE_RESET = (0),
      INTR_TEST_RESET = (0),
      ALERT_TEST_RESET = (0),
      CONTROL_RESET = (127),
      STATUS_RESET = (0),
      CONFIGOPTS_RESET = (0),
      CSID_RESET = (0),
      COMMAND_RESET = (0),
      RXDATA_RESET = (0),
      TXDATA_RESET = (0),
      ERROR_ENABLE_RESET = (0x1f),
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