// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file edn_base.h
 * @brief EDN base register infrastructure class
 *
 * This file defines the base class for the EDN (Entropy Distribution Network) module,
 * providing the register infrastructure and memory-mapped interface. The edn_base class
 * serves as the foundation for the complete EDN model implementation.
 *
 * The EDN module distributes entropy from a CSRNG (Cryptographically Secure Random Number
 * Generator) to up to 8 peripheral endpoints through three operating modes:
 * - Boot-Time Request Mode: Fast pre-FIPS entropy for system startup
 * - Auto Request Mode: Hardware-managed continuous entropy distribution
 * - Software Port Mode: Firmware-controlled entropy generation
 *
 * @note This is a CSML-generated base class. User implementation should extend this class.
 * @note Memory size is configurable at instantiation (default addresses 0x00-0x44).
 */

#pragma once
#include "edn_register.h"
#include <string.h>

/**
 * @class edn_base
 * @brief Base infrastructure class for EDN register model
 *
 * Provides the register map, memory infrastructure, and TLM target socket for
 * EDN configuration and status access. Contains all 17 EDN registers with proper
 * offsets, reset values, and access control.
 *
 * Register Map:
 * - 0x00-0x0C: Interrupt and Alert Control
 * - 0x10-0x14: Configuration and Write Protection
 * - 0x18-0x1C: Boot-Time Command Configuration
 * - 0x20-0x28: Software and Hardware Command Interface
 * - 0x2C-0x34: Auto Request Mode Configuration
 * - 0x38-0x44: Error Reporting and State Observation
 *
 * @note Derived classes should implement behavioral logic and register callbacks.
 */
class edn_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;

    /**
     * @brief Constructor for EDN base module
     * @param name SystemC module name
     * @param memory_size Size of register memory space in bytes (default covers 0x00-0x48)
     *
     * Initializes all 17 EDN registers with correct offsets and reset values:
     * - INTR_STATE (0x00): Reset 0x00000000, RW with W1C
     * - CTRL (0x14): Reset 0x00009999, RW protected by REGWEN
     * - BOOT_INS_CMD (0x18): Reset 0x00000901
     * - BOOT_GEN_CMD (0x1C): Reset 0x00FFF003
     * - MAIN_SM_STATE (0x44): Reset 0x000000C1 (Idle state)
     * And sets up the TLM target socket for register access.
     */
    edn_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xC + 0x00)/sizeof(unsigned int)), 
       REGWEN(std::string(name) + ".REGWEN", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       CTRL(std::string(name) + ".CTRL", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       BOOT_INS_CMD(std::string(name) + ".BOOT_INS_CMD", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       BOOT_GEN_CMD(std::string(name) + ".BOOT_GEN_CMD", memory, (0x1C + 0x00)/sizeof(unsigned int)), 
       SW_CMD_REQ(std::string(name) + ".SW_CMD_REQ", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       SW_CMD_STS(std::string(name) + ".SW_CMD_STS", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       HW_CMD_STS(std::string(name) + ".HW_CMD_STS", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       RESEED_CMD(std::string(name) + ".RESEED_CMD", memory, (0x2C + 0x00)/sizeof(unsigned int)), 
       GENERATE_CMD(std::string(name) + ".GENERATE_CMD", memory, (0x30 + 0x00)/sizeof(unsigned int)), 
       MAX_NUM_REQS_BETWEEN_RESEEDS(std::string(name) + ".MAX_NUM_REQS_BETWEEN_RESEEDS", memory, (0x34 + 0x00)/sizeof(unsigned int)), 
       RECOV_ALERT_STS(std::string(name) + ".RECOV_ALERT_STS", memory, (0x38 + 0x00)/sizeof(unsigned int)), 
       ERR_CODE(std::string(name) + ".ERR_CODE", memory, (0x3C + 0x00)/sizeof(unsigned int)), 
       ERR_CODE_TEST(std::string(name) + ".ERR_CODE_TEST", memory, (0x40 + 0x00)/sizeof(unsigned int)), 
       MAIN_SM_STATE(std::string(name) + ".MAIN_SM_STATE", memory, (0x44 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      /// CSML memory infrastructure for register storage (32-bit memory template)
      csml_memory<32> memory;

      /// TLM-2.0 simple target socket for register bus access (32-bit transactions)
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      // === Interrupt Control Registers (0x00-0x08) ===

      /// Interrupt state register (0x00, RW with W1C). Fields: edn_cmd_req_done[0], edn_fatal_err[1]
      edn::INTR_STATE_type<32> INTR_STATE;

      /// Interrupt enable register (0x04, RW). Enables corresponding interrupts in INTR_STATE
      edn::INTR_ENABLE_type<32> INTR_ENABLE;

      /// Interrupt test register (0x08, WO). Write 1 to force interrupts for testing
      edn::INTR_TEST_type<32> INTR_TEST;

      // === Alert Control Register (0x0C) ===

      /// Alert test register (0x0C, WO). Fields: recov_alert[0], fatal_alert[1]
      edn::ALERT_TEST_type<32> ALERT_TEST;

      // === Configuration and Control Registers (0x10-0x14) ===

      /// Register write enable (0x10, RW with W0C). Write 0 to lock CTRL register permanently
      edn::REGWEN_type<32> REGWEN;

      /// EDN control register (0x14, RW, protected by REGWEN). Multi-bit encoded fields for mode control
      edn::CTRL_type<32> CTRL;

      // === Boot-Time Configuration Registers (0x18-0x1C) ===

      /// Boot instantiate command (0x18, RW). 32-bit CSRNG command for boot-time instantiate
      edn::BOOT_INS_CMD_type<32> BOOT_INS_CMD;

      /// Boot generate command (0x1C, RW). 32-bit CSRNG command for boot-time generate
      edn::BOOT_GEN_CMD_type<32> BOOT_GEN_CMD;

      // === Software Command Interface (0x20-0x24) ===

      /// Software command request (0x20, WO). FIFO for software-driven CSRNG commands (up to 13 words)
      edn::SW_CMD_REQ_type<32> SW_CMD_REQ;

      /// Software command status (0x24, RO). Fields: CMD_REG_RDY[0], CMD_RDY[1], CMD_ACK[2], CMD_STS[5:3]
      edn::SW_CMD_STS_type<32> SW_CMD_STS;

      // === Hardware Command Status (0x28) ===

      /// Hardware command status (0x28, RO). Shows boot/auto mode status and command acknowledgment
      edn::HW_CMD_STS_type<32> HW_CMD_STS;

      // === Auto Request Mode Configuration (0x2C-0x34) ===

      /// Reseed command FIFO (0x2C, WO). Auto request mode reseed commands (up to 13 words)
      edn::RESEED_CMD_type<32> RESEED_CMD;

      /// Generate command FIFO (0x30, WO). Auto request mode generate commands (up to 13 words)
      edn::GENERATE_CMD_type<32> GENERATE_CMD;

      /// Maximum requests between reseeds (0x34, RW). Down-counter for auto mode reseed interval
      edn::MAX_NUM_REQS_BETWEEN_RESEEDS_type<32> MAX_NUM_REQS_BETWEEN_RESEEDS;

      // === Error and Status Registers (0x38-0x44) ===

      /// Recoverable alert status (0x38, RW with W0C). Multi-bit encoding and bus consistency errors
      edn::RECOV_ALERT_STS_type<32> RECOV_ALERT_STS;

      /// Fatal error code register (0x3C, RO, sticky). FIFO and state machine fatal errors
      edn::ERR_CODE_type<32> ERR_CODE;

      /// Error code test register (0x40, RW). Write bit position to force ERR_CODE bits
      edn::ERR_CODE_TEST_type<32> ERR_CODE_TEST;

      /// Main state machine state (0x44, RO). Sparse-encoded state value for debug visibility
      edn::MAIN_SM_STATE_type<32> MAIN_SM_STATE;

      /**
       * @brief Reset all registers to their default values
       *
       * Sets all EDN registers to their hardware reset values:
       * - Most registers reset to 0x00000000
       * - REGWEN resets to 0x00000001 (unlocked)
       * - CTRL resets to 0x00009999 (all modes disabled)
       * - BOOT_INS_CMD resets to 0x00000901
       * - BOOT_GEN_CMD resets to 0x00FFF003
       * - MAIN_SM_STATE resets to 0x000000C1 (Idle state)
       */
      void reset_all_registers();
};
