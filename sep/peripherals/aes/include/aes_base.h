// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aes_base.h
 * @brief Base class for the AES TLM model providing register infrastructure
 * 
 * This header defines the aes_base class which encapsulates the hardware 
 * register definitions and the memory-mapped interface for the AES module.
 * It uses the CSML (Common System Modeling Library) for register and 
 * memory modeling.
 */

#pragma once
#include "aes_register.h"
#include <string.h>

/**
 * @class aes_base
 * @brief Base class providing the hardware register infrastructure for AES
 * 
 * Provides:
 * - All AES hardware registers (key shares, IV, data, control, status)
 * - Memory-mapped register access via a TLM target socket
 * - Basic register reset functionality
 * 
 * The main aes_model class inherits from this base to implement the 
 * behavioral cipher logic on top of these registers.
 */
class aes_base : public sc_module
{
  public:
    /// @brief Data type for 32-bit registers
    typedef typename csml_reg<32>::DT DT;

    /** 
     * @brief Constructor for the aes_base class
     * @param name SystemC module name
     * @param memory_size Total size of the register memory space in bytes
     * 
     * Initializes all hardware registers at their specified offsets and
     * binds the memory storage to the TLM target socket.
     */
    aes_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       KEY_SHARE0(std::string(name) + ".KEY_SHARE0", memory, (0x4 + 0x00)/sizeof(unsigned int), 1), 
       KEY_SHARE1(std::string(name) + ".KEY_SHARE1", memory, (0x24 + 0x00)/sizeof(unsigned int), 1), 
       IV(std::string(name) + ".IV", memory, (0x44 + 0x00)/sizeof(unsigned int), 1), 
       DATA_IN(std::string(name) + ".DATA_IN", memory, (0x54 + 0x00)/sizeof(unsigned int), 1), 
       DATA_OUT(std::string(name) + ".DATA_OUT", memory, (0x64 + 0x00)/sizeof(unsigned int), 1), 
       CTRL_SHADOWED(std::string(name) + ".CTRL_SHADOWED", memory, (0x74 + 0x00)/sizeof(unsigned int)), 
       CTRL_AUX_SHADOWED(std::string(name) + ".CTRL_AUX_SHADOWED", memory, (0x78 + 0x00)/sizeof(unsigned int)), 
       CTRL_AUX_REGWEN(std::string(name) + ".CTRL_AUX_REGWEN", memory, (0x7C + 0x00)/sizeof(unsigned int)), 
       TRIGGER(std::string(name) + ".TRIGGER", memory, (0x80 + 0x00)/sizeof(unsigned int)), 
       STATUS(std::string(name) + ".STATUS", memory, (0x84 + 0x00)/sizeof(unsigned int)),
       CTRL_GCM_SHADOWED(std::string(name) + ".CTRL_GCM_SHADOWED", memory, (0x88 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      /// @brief Register memory storage for the module
      csml_memory<32> memory;
      
      /// @brief TLM target socket for memory-mapped register access
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      // =========================================================================
      // Hardware Registers
      // =========================================================================

      aes::ALERT_TEST_type<32> ALERT_TEST;               ///< Alert test register (offset 0x00)

      csml_reg_vector<aes::KEY_SHARE0_type<32>, 8> KEY_SHARE0; ///< Key share 0 registers (0x04 - 0x20)

      csml_reg_vector<aes::KEY_SHARE1_type<32>, 8> KEY_SHARE1; ///< Key share 1 registers (0x24 - 0x40)

      csml_reg_vector<aes::IV_type<32>, 4> IV;           ///< Initialization vector registers (0x44 - 0x50)

      csml_reg_vector<aes::DATA_IN_type<32>, 4> DATA_IN; ///< Input data registers (0x54 - 0x60)

      csml_reg_vector<aes::DATA_OUT_type<32>, 4> DATA_OUT; ///< Output data registers (0x64 - 0x70)

      aes::CTRL_SHADOWED_type<32> CTRL_SHADOWED;         ///< Shadowed control register (offset 0x74)

      aes::CTRL_AUX_SHADOWED_type<32> CTRL_AUX_SHADOWED; ///< Shadowed auxiliary control register (offset 0x78)

      aes::CTRL_AUX_REGWEN_type<32> CTRL_AUX_REGWEN;     ///< Register write enable for CTRL_AUX (offset 0x7C)

      aes::TRIGGER_type<32> TRIGGER;                     ///< Trigger register for starting operations (offset 0x80)

      aes::STATUS_type<32> STATUS;                       ///< Module status register (offset 0x84)

      aes::CTRL_GCM_SHADOWED_type<32> CTRL_GCM_SHADOWED; ///< Shadowed GCM control register (offset 0x88)

      
      /** @brief Resets all hardware registers to their default reset values */
      void reset_all_registers();
};
