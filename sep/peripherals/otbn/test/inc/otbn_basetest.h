/**
 * @file otbn_basetest.h
 * @brief OTBN base test class with register definitions and TLM initiator
 * 
 * Provides base test infrastructure for OTBN tests including:
 * - TLM initiator socket for register access
 * - Register offset enumerations for all OTBN registers
 * - Register access permission masks (read/write)
 * - Register reset values
 * - Register property structure for test automation
 * 
 * This base class is extended by otbn_test to implement specific test cases.
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @brief Base test class for OTBN verification
 * 
 * Provides register definitions and TLM initiator socket for accessing OTBN
 * registers during test execution. Contains enumerations for all register
 * offsets, access permissions, and reset values.
 */
class otbn_basetest : public sc_module
{
  public:
    /// @brief TLM initiator socket for register read/write transactions
    tlm_utils::simple_initiator_socket<otbn_basetest, 32> initiator_socket;
    
    /**
     * @brief OTBN register address offsets
     * 
     * Defines byte offsets for all OTBN registers from base address.
     */
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xc + 0x00), 
      CMD_OFFSET = (0x10 + 0x00), 
      CTRL_OFFSET = (0x14 + 0x00), 
      STATUS_OFFSET = (0x18 + 0x00), 
      ERR_BITS_OFFSET = (0x1c + 0x00), 
      FATAL_ALERT_CAUSE_OFFSET = (0x20 + 0x00), 
      INSN_CNT_OFFSET = (0x24 + 0x00), 
      LOAD_CHECKSUM_OFFSET = (0x28 + 0x00), 
      IMEM_OFFSET = (0x4000 + 0x00), 
      DMEM_OFFSET = (0x8000 + 0x00)  
    };

    /**
     * @brief Register read access permission masks
     * 
     * Bit masks indicating which bits are readable for each register.
     * 0x0 = write-only, 0xffffffff = fully readable.
     */
    enum Register_Read_Access
    {
      INTR_STATE_READ = (0x1), 
      INTR_ENABLE_READ = (0x1), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      CMD_READ = (0x0), 
      CTRL_READ = (0x1), 
      STATUS_READ = (0xff), 
      ERR_BITS_READ = (0xff00ff), 
      FATAL_ALERT_CAUSE_READ = (0xff), 
      INSN_CNT_READ = (0xffffffff), 
      LOAD_CHECKSUM_READ = (0xffffffff), 
      IMEM_READ = (0xffffffff), 
      DMEM_READ = (0xffffffff)  
    };

    /**
     * @brief Register write access permission masks
     * 
     * Bit masks indicating which bits are writable for each register.
     * 0x0 = read-only, 0xffffffff = fully writable.
     */
    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x1), 
      INTR_ENABLE_WRITE = (0x1), 
      INTR_TEST_WRITE = (0x1), 
      ALERT_TEST_WRITE = (0x3), 
      CMD_WRITE = (0xff), 
      CTRL_WRITE = (0x1), 
      STATUS_WRITE = (0x0), 
      ERR_BITS_WRITE = (0xff00ff), 
      FATAL_ALERT_CAUSE_WRITE = (0x0), 
      INSN_CNT_WRITE = (0xffffffff), 
      LOAD_CHECKSUM_WRITE = (0xffffffff), 
      IMEM_WRITE = (0xffffffff), 
      DMEM_WRITE = (0xffffffff)
    };

    /**
     * @brief Register reset values
     * 
     * Default values for each register after system reset.
     */
    enum Register_Reset_Val
    {
      INTR_STATE_RESET = (0), 
      INTR_ENABLE_RESET = (0), 
      INTR_TEST_RESET = (0), 
      ALERT_TEST_RESET = (0), 
      CMD_RESET = (0), 
      CTRL_RESET = (0), 
      STATUS_RESET = (4), 
      ERR_BITS_RESET = (0), 
      FATAL_ALERT_CAUSE_RESET = (0), 
      INSN_CNT_RESET = (0), 
      LOAD_CHECKSUM_RESET = (0), 
      IMEM_RESET = (0), 
      DMEM_RESET = (0)
    };
     
    /**
     * @brief Register property structure
     * 
     * Encapsulates all properties of a register for test automation:
     * - Address offset
     * - Read/write permission masks
     * - Reset value
     * - Human-readable name
     */
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    /**
     * @brief  Constructor
     * @param name SystemC module name
     */
    otbn_basetest(sc_module_name name) : sc_module(name)
    {

    }

};