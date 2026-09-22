// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox_register.h
 * @brief regmodel register definitions for mailbox IP
 *
 * This file contains register type definitions and bitfield structures for the
 * mailbox IP with dual-port bidirectional FIFO communication. All registers are
 * 64-bit wide with 8-byte aligned offsets (0x00-0x48).
 *
 * Generated from: mailbox-register-map.csv
 * Memory Template: 64-bit
 */

#pragma once
#include "reg_file.h"
#include <iostream>
#include <systemc.h>

/**
 * @namespace mailbox
 * @brief Register type definitions for mailbox IP
 */
namespace mailbox {

/**
 * @class WRITE_DATA_type
 * @brief Write-only data register for mailbox FIFO (Offset: 0x00, Access: WO,
 * Reset: 0x0)
 *
 * Write data to mailbox FIFO. Data written here appears on peer port's
 * READ_DATA. Returns axi_pkg::RESP_SLVERR on write-to-full (sets
 * ERROR_FLAGS[1]). Triggers WTIRQ when FIFO level exceeds WIRQT threshold.
 */
template <unsigned int N> class WRITE_DATA_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for WRITE_DATA register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  WRITE_DATA_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0xffffffffffffffff, 0x0),
        write_data(reg_name + ".write_data", *this, 0, 64) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> write_data; ///< 64-bit data payload for write FIFO [63:0]
};

/**
 * @class READ_DATA_type
 * @brief Read-only data register for mailbox FIFO (Offset: 0x08, Access: RO,
 * Reset: 0x0)
 *
 * Read data from mailbox FIFO. Returns data written by peer port.
 * Returns axi_pkg::RESP_SLVERR on read-from-empty (sets ERROR_FLAGS[0]).
 * Triggers RTIRQ when FIFO level exceeds RIRQT threshold.
 */
template <unsigned int N> class READ_DATA_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for READ_DATA register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  READ_DATA_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffffff, 0x0, 0x0),
        read_data(reg_name + ".read_data", *this, 0, 64) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> read_data; ///< 64-bit data payload from read FIFO [63:0]
};

/**
 * @class STATUS_type
 * @brief FIFO status flags register (Offset: 0x10, Access: RO, Reset: 0x1)
 *
 * FIFO status flags. Indicates empty/full conditions and threshold level
 * comparisons for interrupt generation. Empty resets to 1 (FIFO empty).
 */
template <unsigned int N> class STATUS_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for STATUS register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x000000000000000f, 0x0, 0x1),
        empty(reg_name + ".empty", *this, 0, 1),
        full(reg_name + ".full", *this, 1, 1),
        write_level_above_thresh(reg_name + ".write_level_above_thresh", *this,
                                 2, 1),
        read_level_above_thresh(reg_name + ".read_level_above_thresh", *this, 3,
                                1),
        reserved0(reg_name + ".reserved0", *this, 4, 60) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> empty; ///< 0=data available to read, 1=no data available
                          ///< (FIFO empty) [0:0]
  regmodel::Bitfield<N> full;  ///< 0=space available to write, 1=no space available
                          ///< (FIFO full) [1:1]
  regmodel::Bitfield<N>
      write_level_above_thresh; ///< Set when write FIFO usage level exceeds
                                ///< WIRQT threshold [2:2]
  regmodel::Bitfield<N> read_level_above_thresh; ///< Set when read FIFO fill level
                                            ///< exceeds RIRQT threshold [3:3]
  regmodel::Bitfield<N> reserved0; ///< Reserved bits - read as 0 [63:4]
};

/**
 * @class ERROR_FLAGS_type
 * @brief Error condition flags register (Offset: 0x18, Access: RO, Reset: 0x0)
 *
 * Error condition flags (clear-on-read). Both flags reset to 0; a flag is
 * raised only by an access that actually fails. Reading this register clears
 * all error flags.
 */
template <unsigned int N> class ERROR_FLAGS_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for ERROR_FLAGS register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  ERROR_FLAGS_type(std::string reg_name, memory_type &memory,
                   unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0000000000000003, 0x0, 0x0),
        read_error(reg_name + ".read_error", *this, 0, 1),
        write_error(reg_name + ".write_error", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 62) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> read_error;  ///< 1=attempted read from empty mailbox [0:0]
  regmodel::Bitfield<N> write_error; ///< 1=attempted write to full mailbox [1:1]
  regmodel::Bitfield<N> reserved0;   ///< Reserved bits - read as 0 [63:2]
};

/**
 * @class WIRQT_type
 * @brief Write interrupt threshold register (Offset: 0x20, Access: RW, Reset:
 * 0x0)
 *
 * Write interrupt threshold. When write FIFO usage exceeds this value, WTIRQ
 * triggers and STATUS[2] sets. Values >= MailboxDepth saturate to
 * (MailboxDepth-1).
 */
template <unsigned int N> class WIRQT_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for WIRQT register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  WIRQT_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x00000000000000ff,
                    0x00000000000000ff, 0x0),
        wirqt(reg_name + ".wirqt", *this, 0, 8),
        reserved0(reg_name + ".reserved0", *this, 8, 56) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N>
      wirqt; ///< Threshold for write FIFO interrupt (8-bit value) [7:0]
  regmodel::Bitfield<N>
      reserved0; ///< Reserved bits - read as 0, writes ignored [63:8]
};

/**
 * @class RIRQT_type
 * @brief Read interrupt threshold register (Offset: 0x28, Access: RW, Reset:
 * 0x0)
 *
 * Read interrupt threshold. When read FIFO fill level exceeds this value, RTIRQ
 * triggers and STATUS[3] sets. Values >= MailboxDepth saturate to
 * (MailboxDepth-1).
 */
template <unsigned int N> class RIRQT_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for RIRQT register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  RIRQT_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x00000000000000ff,
                    0x00000000000000ff, 0x0),
        rirqt(reg_name + ".rirqt", *this, 0, 8),
        reserved0(reg_name + ".reserved0", *this, 8, 56) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N>
      rirqt; ///< Threshold for read FIFO interrupt (8-bit value) [7:0]
  regmodel::Bitfield<N>
      reserved0; ///< Reserved bits - read as 0, writes ignored [63:8]
};

/**
 * @class IRQS_type
 * @brief Interrupt request status register (Offset: 0x30, Access: RW, Reset:
 * 0x0)
 *
 * Interrupt request status (sticky, write-1-to-clear). Register updates occur
 * regardless of IRQEN state. Software must explicitly write 1 to clear.
 */
template <unsigned int N> class IRQS_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for IRQS register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  IRQS_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0000000000000007,
                    0x0000000000000007, 0x0),
        wtirq(reg_name + ".wtirq", *this, 0, 1),
        rtirq(reg_name + ".rtirq", *this, 1, 1),
        eirq(reg_name + ".eirq", *this, 2, 1),
        reserved0(reg_name + ".reserved0", *this, 3, 61) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> wtirq; ///< Write threshold IRQ status (W1C). Read: 0=no IRQ,
                          ///< 1=IRQ pending. Write: 0=no ack, 1=clear IRQ [0:0]
  regmodel::Bitfield<N> rtirq; ///< Read threshold IRQ status (W1C). Read: 0=no IRQ,
                          ///< 1=IRQ pending. Write: 0=no ack, 1=clear IRQ [1:1]
  regmodel::Bitfield<N> eirq;  ///< Error IRQ status (W1C). Read: 0=no IRQ, 1=IRQ
                          ///< pending. Write: 0=no ack, 1=clear IRQ [2:2]
  regmodel::Bitfield<N>
      reserved0; ///< Reserved bits - read as 0, writes ignored [63:3]
};

/**
 * @class IRQEN_type
 * @brief Interrupt enable control register (Offset: 0x38, Access: RW, Reset:
 * 0x0)
 *
 * Interrupt enable control. Controls which interrupts are sent to the CPU via
 * the irq_o output signal.
 */
template <unsigned int N> class IRQEN_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for IRQEN register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  IRQEN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0000000000000007,
                    0x0000000000000007, 0x0),
        wtirq(reg_name + ".wtirq", *this, 0, 1),
        rtirq(reg_name + ".rtirq", *this, 1, 1),
        eirq(reg_name + ".eirq", *this, 2, 1),
        reserved0(reg_name + ".reserved0", *this, 3, 61) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> wtirq; ///< 0=write threshold IRQ disabled, 1=enabled [0:0]
  regmodel::Bitfield<N> rtirq; ///< 0=read threshold IRQ disabled, 1=enabled [1:1]
  regmodel::Bitfield<N> eirq;  ///< 0=error IRQ disabled, 1=enabled [2:2]
  regmodel::Bitfield<N>
      reserved0; ///< Reserved bits - read as 0, writes ignored [63:3]
};

/**
 * @class IRQP_type
 * @brief Interrupt pending status register (Offset: 0x40, Access: RO, Reset:
 * 0x0)
 *
 * Interrupt pending status (hardware-generated: IRQP = IRQS & IRQEN).
 * Aggregate OR of bits drives irq_o[port] signal.
 */
template <unsigned int N> class IRQP_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for IRQP register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  IRQP_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0000000000000007, 0x0, 0x0),
        wtirq(reg_name + ".wtirq", *this, 0, 1),
        rtirq(reg_name + ".rtirq", *this, 1, 1),
        eirq(reg_name + ".eirq", *this, 2, 1),
        reserved0(reg_name + ".reserved0", *this, 3, 61) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N> wtirq; ///< 0=no write threshold IRQ pending, 1=pending [0:0]
  regmodel::Bitfield<N> rtirq; ///< 0=no read threshold IRQ pending, 1=pending [1:1]
  regmodel::Bitfield<N> eirq;  ///< 0=no error IRQ pending, 1=pending [2:2]
  regmodel::Bitfield<N> reserved0; ///< Reserved bits - read as 0 [63:3]
};

/**
 * @class CTRL_type
 * @brief FIFO flush control register (Offset: 0x48, Access: WO, Reset: 0x0)
 *
 * FIFO flush control (self-clearing). Flush signal is OR combination of
 * respective bit from both ports. Register resets after write completes.
 */
template <unsigned int N> class CTRL_type : public regmodel::Reg<N> {
public:
  using typename regmodel::Reg<N>::memory_type;
  typedef typename regmodel::Word<N>::wordtype DT;
  /**
   * @brief Constructor for CTRL register
   * @param reg_name Hierarchical name for the register
   * @param memory Reference to regmodel::Memory backing store
   * @param offset Memory offset for register location
   */
  CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x0000000000000003, 0x0),
        wflush(reg_name + ".wflush", *this, 0, 1),
        rflush(reg_name + ".rflush", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 62) {
    this->set_read_write_restrictions(memory);
  }

  using regmodel::Reg<N>::operator=;
  using regmodel::Reg<N>::operator+=;
  using regmodel::Reg<N>::operator-=;
  using regmodel::Reg<N>::operator/=;
  using regmodel::Reg<N>::operator*=;
  using regmodel::Reg<N>::operator%=;
  using regmodel::Reg<N>::operator^=;
  using regmodel::Reg<N>::operator&=;
  using regmodel::Reg<N>::operator|=;
  using regmodel::Reg<N>::operator>>=;
  using regmodel::Reg<N>::operator<<=;
  regmodel::Bitfield<N>
      wflush; ///< Flush write FIFO for this port (self-clearing) [0:0]
  regmodel::Bitfield<N>
      rflush; ///< Flush read FIFO for this port (self-clearing) [1:1]
  regmodel::Bitfield<N> reserved0; ///< Reserved bits - writes ignored [63:2]
};

} // namespace mailbox