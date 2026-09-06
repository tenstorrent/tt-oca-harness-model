// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox_base.h
 * @brief Single-port register infrastructure for mailbox IP
 *
 * Each mailbox_base instance holds the register state for one mailbox port: one
 * regmodel::Memory and one set of 10 registers.
 *
 * Two instances (b0, b1) are composed in mailbox_ip to form the complete dual-port mailbox.
 * mailbox_ip owns the TLM target sockets and drives these memories directly, so that it
 * can apply the AXI-Lite access rules before the register access happens.
 */

#pragma once
#include "mailbox_register.h"
#include <string.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class mailbox_base
 * @brief Pure register container for one mailbox port
 *
 * Provides:
 * - One independent regmodel::Memory<64> instance
 * - One set of 10 registers (WRITE_DATA through CTRL) at fixed word offsets
 *
 * Has no socket of its own; mailbox_ip calls memory.b_transport() directly.
 */
class mailbox_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<64>::DT DT;

    /**
     * @brief Constructor for single-port mailbox register infrastructure
     * @param name SystemC module hierarchical name
     * @param memory_size Total memory size for register space (minimum 0x50 bytes for 10 registers)
     */
    mailbox_base(sc_module_name name, unsigned int memory_size) : sc_module(name),
       memory(std::string(name) + ".memory", memory_size/sizeof(unsigned long long)),
       WRITE_DATA(std::string(name) + ".WRITE_DATA", memory, 0x00/sizeof(unsigned long long)),
       READ_DATA(std::string(name)  + ".READ_DATA",  memory, 0x08/sizeof(unsigned long long)),
       STATUS(std::string(name)     + ".STATUS",     memory, 0x10/sizeof(unsigned long long)),
       ERROR_FLAGS(std::string(name)+ ".ERROR_FLAGS",memory, 0x18/sizeof(unsigned long long)),
       WIRQT(std::string(name)      + ".WIRQT",      memory, 0x20/sizeof(unsigned long long)),
       RIRQT(std::string(name)      + ".RIRQT",      memory, 0x28/sizeof(unsigned long long)),
       IRQS(std::string(name)       + ".IRQS",       memory, 0x30/sizeof(unsigned long long)),
       IRQEN(std::string(name)      + ".IRQEN",      memory, 0x38/sizeof(unsigned long long)),
       IRQP(std::string(name)       + ".IRQP",       memory, 0x40/sizeof(unsigned long long)),
       CTRL(std::string(name)       + ".CTRL",       memory, 0x48/sizeof(unsigned long long))
       {
       }

      /// @brief regmodel::Memory backing store for registers
      regmodel::Memory<64> memory;

      // Registers (addresses 0x00-0x48)
      mailbox::WRITE_DATA_type<64> WRITE_DATA; ///< Write data register (0x00, WO)
      mailbox::READ_DATA_type<64>  READ_DATA;  ///< Read data register (0x08, RO)
      mailbox::STATUS_type<64>     STATUS;     ///< Status register (0x10, RO)
      mailbox::ERROR_FLAGS_type<64> ERROR_FLAGS; ///< Error flags register (0x18, RO)
      mailbox::WIRQT_type<64>      WIRQT;      ///< Write interrupt threshold (0x20, RW)
      mailbox::RIRQT_type<64>      RIRQT;      ///< Read interrupt threshold (0x28, RW)
      mailbox::IRQS_type<64>       IRQS;       ///< Interrupt status (0x30, RW/W1C)
      mailbox::IRQEN_type<64>      IRQEN;      ///< Interrupt enable (0x38, RW)
      mailbox::IRQP_type<64>       IRQP;       ///< Interrupt pending (0x40, RO)
      mailbox::CTRL_type<64>       CTRL;       ///< Control register (0x48, WO)

      /**
       * @brief Reset all registers to their default values
       *
       * Resets all 10 registers to RDL-specified reset values.
       * Called during initialization and asynchronous reset handling.
       */
      void reset_registers();
};
