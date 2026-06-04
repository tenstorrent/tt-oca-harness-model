/**
 * @file otbn_base.h
 * @brief OTBN base class with register definitions and memory interface
 * 
 * This header defines the base class for the OTBN (OpenTitan Big Number Accelerator)
 * SystemC TLM model. The base class provides:
 * - Hardware register instances (INTR_STATE, CMD, STATUS, ERR_BITS, etc.)
 * - IMEM (Instruction Memory) - 8KB array of 2048 32-bit words
 * - DMEM (Data Memory) - 3KB array of 768 32-bit words (host-accessible portion)
 * - TLM target socket for memory-mapped register access
 * - Register reset functionality
 * 
 * The otbn_base class is inherited by otbn_ip which implements the functional
 * behavior and algorithm execution logic.
 */

#pragma once
#include "otbn_register.h"
#include <string.h>

/**
 * @brief OTBN base class providing register definitions and memory
 * 
 * Base class for OTBN TLM model that contains:
 * - All hardware register instances per OTBN specification
 * - IMEM (Instruction Memory) register vector - 8KB (2048 words)
 * - DMEM (Data Memory) register vector - 3KB host-accessible (768 words)
 * - CSML memory backing store for memory-mapped access
 * - TLM target socket for register/memory transactions
 * 
 * This class is inherited by otbn_ip which adds functional behavior,
 * algorithm execution, and external interface ports.
 */
class otbn_base : public sc_module
{
  public:
    //Data type for register access (32-bit)
    typedef typename csml_reg<32>::DT DT;
    
    /* Initializes all OTBN registers at their specified offsets and binds
     * the CSML memory to the TLM target socket for memory-mapped access.
     */
    otbn_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xc + 0x00)/sizeof(unsigned int)), 
       CMD(std::string(name) + ".CMD", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       CTRL(std::string(name) + ".CTRL", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       STATUS(std::string(name) + ".STATUS", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       ERR_BITS(std::string(name) + ".ERR_BITS", memory, (0x1c + 0x00)/sizeof(unsigned int)), 
       FATAL_ALERT_CAUSE(std::string(name) + ".FATAL_ALERT_CAUSE", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       INSN_CNT(std::string(name) + ".INSN_CNT", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       LOAD_CHECKSUM(std::string(name) + ".LOAD_CHECKSUM", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       IMEM(std::string(name) + ".IMEM", memory, (0x4000 + 0x00)/sizeof(unsigned int), 1), 
       DMEM(std::string(name) + ".DMEM", memory, (0x8000 + 0x00)/sizeof(unsigned int), 1)
       {
         memory.bind_to_socket(target_socket);
       }

      //CSML memory backing store for all registers and memory regions
      csml_memory<32> memory;
      
      //TLM target socket for memory-mapped register/memory access
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      //Interrupt State Register - W1C for clearing interrupts
      otbn::INTR_STATE_type<32> INTR_STATE;
      
      //Interrupt Enable Register - Controls interrupt masking
      otbn::INTR_ENABLE_type<32> INTR_ENABLE;
      
      //Interrupt Test Register - Write-only for testing interrupts
      otbn::INTR_TEST_type<32> INTR_TEST;
      
      //Alert Test Register - Write-only for testing alert outputs
      otbn::ALERT_TEST_type<32> ALERT_TEST;
      
      //Command Register - Write to execute commands (EXECUTE, SEC_WIPE_DMEM, SEC_WIPE_IMEM)
      otbn::CMD_type<32> CMD;
      
      //Control Register - Configuration (software_errs_fatal bit)
      otbn::CTRL_type<32> CTRL;
      
      //Status Register - Read-only current state (IDLE, BUSY_EXECUTE, LOCKED, etc.)
      otbn::STATUS_type<32> STATUS;
      
      //Error Bits Register - Sticky error flags (W1C when IDLE/LOCKED)
      otbn::ERR_BITS_type<32> ERR_BITS;
      
      //Fatal Alert Cause Register - Read-only persisted fatal error cause
      otbn::FATAL_ALERT_CAUSE_type<32> FATAL_ALERT_CAUSE;
      
      //Instruction Count Register - Read-only execution cycle count
      otbn::INSN_CNT_type<32> INSN_CNT;
      
      //Load Checksum Register - CRC-32-IEEE of IMEM/DMEM writes
      otbn::LOAD_CHECKSUM_type<32> LOAD_CHECKSUM;
      
      //Instruction Memory - 8KB (2048 x 32-bit words) at offset 0x4000
      csml_reg_vector<otbn::IMEM_type<32>, 4096> IMEM;
      
      //Data Memory - 3KB host-accessible (768 x 32-bit words) at offset 0x8000
      csml_reg_vector<otbn::DMEM_type<32>, 4096> DMEM;
      
      void reset_all_registers();
};
