// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file hmac_base.h
 * @brief Base class for HMAC IP module with register definitions
 * 
 * This class provides the base SystemC module for the HMAC IP, including
 * all register definitions and memory-mapped interface. It uses the regmodel
 * register framework for register management.
 */

#pragma once
#include "hmac_register.h"
#include <string.h>

/**
 * @brief Base class for HMAC IP module
 * 
 * This class provides the foundation for the HMAC IP implementation, including:
 * - Memory-mapped register interface via TLM socket
 * - All HMAC register definitions (interrupts, configuration, status, etc.)
 * - Register reset functionality
 * 
 * The memory is bound directly to the target socket for TLM transactions.
 */
class hmac_base : public sc_module
{
  public:
    // Data type for register values
    typedef typename regmodel::Reg<32>::DT DT;
    
    hmac_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)),
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)),
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)),
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xc + 0x00)/sizeof(unsigned int)),
       CFG(std::string(name) + ".CFG", memory, (0x10 + 0x00)/sizeof(unsigned int)),
       CMD(std::string(name) + ".CMD", memory, (0x14 + 0x00)/sizeof(unsigned int)),
       STATUS(std::string(name) + ".STATUS", memory, (0x18 + 0x00)/sizeof(unsigned int)),
       ERR_CODE(std::string(name) + ".ERR_CODE", memory, (0x1c + 0x00)/sizeof(unsigned int)),
       WIPE_SECRET(std::string(name) + ".WIPE_SECRET", memory, (0x20 + 0x00)/sizeof(unsigned int)),
       KEY(std::string(name) + ".KEY", memory, (0x24 + 0x00)/sizeof(unsigned int), 1),
       DIGEST(std::string(name) + ".DIGEST", memory, (0xa4 + 0x00)/sizeof(unsigned int), 1),
       MSG_LENGTH_LOWER(std::string(name) + ".MSG_LENGTH_LOWER", memory, (0xe4 + 0x00)/sizeof(unsigned int)),
       MSG_LENGTH_UPPER(std::string(name) + ".MSG_LENGTH_UPPER", memory, (0xe8 + 0x00)/sizeof(unsigned int)),
       MSG_FIFO(std::string(name) + ".MSG_FIFO", memory, (0x1000 + 0x00)/sizeof(unsigned int), 1)
       {
         // Bind socket directly to memory - no wrapper needed
         memory.bind_to_socket(target_socket);
       }

      // Memory object for register storage
      regmodel::Memory<32> memory;
      // TLM target socket for memory-mapped access
      tlm_utils::simple_target_socket<regmodel::Memory<32>, 32> target_socket;

      // Interrupt State Register
      hmac::INTR_STATE_type<32> INTR_STATE;

      // Interrupt Enable Register
      hmac::INTR_ENABLE_type<32> INTR_ENABLE;

      // Interrupt Test Register
      hmac::INTR_TEST_type<32> INTR_TEST;

      // Alert Test Register
      hmac::ALERT_TEST_type<32> ALERT_TEST;

      // Configuration Register
      hmac::CFG_type<32> CFG;

      // Command Register
      hmac::CMD_type<32> CMD;

      // Status Register
      hmac::STATUS_type<32> STATUS;

      // Error Code Register
      hmac::ERR_CODE_type<32> ERR_CODE;

      // Wipe Secret Register
      hmac::WIPE_SECRET_type<32> WIPE_SECRET;

      // Key Registers (32 registers for up to 1024-bit keys)
      regmodel::RegVector<hmac::KEY_type<32>, 32> KEY;

      // Digest Registers (16 registers for up to 512-bit digests)
      regmodel::RegVector<hmac::DIGEST_type<32>, 16> DIGEST;

      // Message Length Lower Register
      hmac::MSG_LENGTH_LOWER_type<32> MSG_LENGTH_LOWER;

      // Message Length Upper Register
      hmac::MSG_LENGTH_UPPER_type<32> MSG_LENGTH_UPPER;

      // Message FIFO Registers (1024 registers mapping to logical FIFO)
      regmodel::RegVector<hmac::MSG_FIFO_type<32>, 1024> MSG_FIFO;
      
      // Reset all registers to their default values
      void reset_all_registers();
};
