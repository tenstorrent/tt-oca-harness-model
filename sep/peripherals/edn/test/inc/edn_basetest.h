// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file edn_basetest.h
 * @brief EDN test harness base class for register access verification
 *
 * Provides the base infrastructure for EDN SystemC/TLM testbenches including:
 * - TLM-2.0 initiator socket for register read/write transactions
 * - Register offset enumerations for all 17 EDN registers
 * - Read/write access mask enumerations for access control verification
 * - Helper methods for register access (read_reg, write_reg)
 * - Transaction utilities for TLM-2.0 generic payload operations
 *
 * This class serves as the foundation for all EDN test cases, enabling:
 * - Register reset value verification
 * - Access type testing (RO, WO, RW, W1C, W0C)
 * - Register field validation
 * - Functional scenario testing
 *
 * @note Test classes should derive from this base to inherit register access utilities.
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class edn_basetest
 * @brief Base test harness class for EDN register model verification
 *
 * Provides TLM initiator infrastructure and register access utilities for
 * testing the EDN module. Includes offset and access mask enumerations for
 * all 17 registers with proper read/write access control.
 *
 * Register Access Masks:
 * - 0xFFFFFFFF: Full read or write access
 * - 0x00000000: No read (write-only) or no write (read-only) access
 *
 * Test Implementation Pattern:
 * 1. Derive test class from edn_basetest
 * 2. Use read_reg/write_reg methods for register transactions
 * 3. Verify register behavior against specifications
 * 4. Check side effects (interrupts, alerts, state changes)
 */
class edn_basetest : public sc_module
{
  public:
    /// TLM-2.0 initiator socket for register bus transactions (32-bit)
    tlm_utils::simple_initiator_socket<edn_basetest, 32> initiator_socket;

    /**
     * @brief Register offset enumeration for all EDN registers
     *
     * Provides symbolic names for register addresses (byte offsets from base).
     * Use these enumerations with read_reg/write_reg methods for type-safe access.
     */
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xC + 0x00), 
      REGWEN_OFFSET = (0x10 + 0x00), 
      CTRL_OFFSET = (0x14 + 0x00), 
      BOOT_INS_CMD_OFFSET = (0x18 + 0x00), 
      BOOT_GEN_CMD_OFFSET = (0x1C + 0x00), 
      SW_CMD_REQ_OFFSET = (0x20 + 0x00), 
      SW_CMD_STS_OFFSET = (0x24 + 0x00), 
      HW_CMD_STS_OFFSET = (0x28 + 0x00), 
      RESEED_CMD_OFFSET = (0x2C + 0x00), 
      GENERATE_CMD_OFFSET = (0x30 + 0x00), 
      MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET = (0x34 + 0x00), 
      RECOV_ALERT_STS_OFFSET = (0x38 + 0x00), 
      ERR_CODE_OFFSET = (0x3C + 0x00), 
      ERR_CODE_TEST_OFFSET = (0x40 + 0x00), 
      MAIN_SM_STATE_OFFSET = (0x44 + 0x00)  
    };

    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x3), 
      INTR_ENABLE_READ = (0x3), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      REGWEN_READ = (0x1), 
      CTRL_READ = (0xffff), 
      BOOT_INS_CMD_READ = (0xffffffff), 
      BOOT_GEN_CMD_READ = (0xffffffff), 
      SW_CMD_REQ_READ = (0x0), 
      SW_CMD_STS_READ = (0x3f), 
      HW_CMD_STS_READ = (0x3ff), 
      RESEED_CMD_READ = (0x0), 
      GENERATE_CMD_READ = (0x0), 
      MAX_NUM_REQS_BETWEEN_RESEEDS_READ = (0xffffffff), 
      RECOV_ALERT_STS_READ = (0x300f), 
      ERR_CODE_READ = (0x70700003), 
      ERR_CODE_TEST_READ = (0x1f), 
      MAIN_SM_STATE_READ = (0x1ff)  
    };

    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x3), 
      INTR_ENABLE_WRITE = (0x3), 
      INTR_TEST_WRITE = (0x3), 
      ALERT_TEST_WRITE = (0x3), 
      REGWEN_WRITE = (0x1), 
      CTRL_WRITE = (0xffff), 
      BOOT_INS_CMD_WRITE = (0xffffffff), 
      BOOT_GEN_CMD_WRITE = (0xffffffff), 
      SW_CMD_REQ_WRITE = (0xffffffff), 
      SW_CMD_STS_WRITE = (0x0), 
      HW_CMD_STS_WRITE = (0x0), 
      RESEED_CMD_WRITE = (0xffffffff), 
      GENERATE_CMD_WRITE = (0xffffffff), 
      MAX_NUM_REQS_BETWEEN_RESEEDS_WRITE = (0xffffffff), 
      RECOV_ALERT_STS_WRITE = (0x300f), 
      ERR_CODE_WRITE = (0x0), 
      ERR_CODE_TEST_WRITE = (0x1f), 
      MAIN_SM_STATE_WRITE = (0x0)
    };

    enum Register_Reset_Val
    {
      INTR_STATE_RESET = 0x00000000,
      INTR_ENABLE_RESET = 0x00000000,
      INTR_TEST_RESET = 0x00000000,
      ALERT_TEST_RESET = 0x00000000,
      REGWEN_RESET = 0x00000001,
      CTRL_RESET = 0x00009999,
      BOOT_INS_CMD_RESET = 0x00000901,
      BOOT_GEN_CMD_RESET = 0x00FFF003,
      SW_CMD_REQ_RESET = 0x00000000,
      SW_CMD_STS_RESET = 0x00000000,
      HW_CMD_STS_RESET = 0x00000000,
      RESEED_CMD_RESET = 0x00000000,
      GENERATE_CMD_RESET = 0x00000000,
      MAX_NUM_REQS_BETWEEN_RESEEDS_RESET = 0x00000000,
      RECOV_ALERT_STS_RESET = 0x00000000,
      ERR_CODE_RESET = 0x00000000,
      ERR_CODE_TEST_RESET = 0x00000000,
      MAIN_SM_STATE_RESET = 0x000000C1
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    edn_basetest(sc_module_name name) : sc_module(name)
    {

    }

};