// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer_interface.h
 * @brief AON Timer abstract interface class for TLM-2.0 register access.
 *
 * This header defines the aon_timer_if pure virtual interface class. It provides
 * a common abstract contract for both the model (aon_timer) and any test proxy
 * that must expose register read/write operations without depending on the concrete
 * model implementation. By depending on this interface rather than aon_timer
 * directly, test harnesses can operate in a fully decoupled manner.
 *
 * The interface exposes 32-bit and 8-bit register access methods that map to
 * TLM-2.0 b_transport transactions on the underlying target socket.
 *
 * Design Pattern:
 *   - Model side   : aon_timer inherits aon_timer_base (register infrastructure)
 *   - Test side    : aon_timer_test uses initiator_socket to issue TLM transactions
 *   - Interface    : aon_timer_if provides a virtual abstraction for layered tests
 *
 * Usage:
 * @code
 *   class my_checker : public aon_timer_if {
 *       void write_register_32(unsigned int offset, uint32_t value) override { ... }
 *       void read_register_32(unsigned int offset, uint32_t& value) override  { ... }
 *   };
 * @endcode
 */

#pragma once
#include <systemc.h>
#include <cstdint>

/**
 * @class aon_timer_if
 * @brief Pure virtual interface class for AON Timer register access operations.
 *
 * Defines the minimal contract required to perform 32-bit and 8-bit register
 * read/write operations against an AON Timer instance. Both the concrete test
 * class (aon_timer_test) and any stub or mock class implementing this interface
 * can be used interchangeably by test layers.
 *
 * All offsets are byte offsets relative to the AON Timer base address (0x0).
 * The 14 valid offsets are 0x00 through 0x34 in 0x4-byte steps, corresponding
 * to the registers defined in aon_timer_basetest::Register_offset.
 */
class aon_timer_if : virtual public sc_interface
{
public:
   /**
    * @brief Perform a 32-bit register write.
    * @param offset      Byte offset of the target register (0x00 to 0x34, 4-byte aligned).
    * @param write_value 32-bit value to write to the register.
    *
    * Issues a TLM_WRITE_COMMAND transaction with 4-byte data length to the DUT.
    * The transaction response is implementation-defined but should be TLM_OK_RESPONSE
    * for valid register addresses.
    */
   virtual void write_register_32(unsigned int offset, uint32_t write_value) = 0;

   /**
    * @brief Perform a 32-bit register read.
    * @param offset     Byte offset of the target register (0x00 to 0x34, 4-byte aligned).
    * @param read_value Reference to a uint32_t that will receive the read value.
    *
    * Issues a TLM_READ_COMMAND transaction with 4-byte data length to the DUT.
    * For write-only registers (ALERT_TEST, INTR_TEST), read_value will be 0x0.
    */
   virtual void read_register_32(unsigned int offset, uint32_t& read_value) = 0;

   /**
    * @brief Perform an 8-bit register read.
    * @param offset     Byte offset of the target register.
    * @param read_value Reference to a uint8_t that will receive the read byte value.
    *
    * Issues a TLM_READ_COMMAND transaction with 1-byte data length to the DUT.
    * For write-only registers, the byte at the given offset reads as 0x0.
    */
   virtual void read_register_8(unsigned int offset, uint8_t& read_value) = 0;

   /**
    * @brief Perform an 8-bit register write.
    * @param offset      Byte offset of the target register.
    * @param write_value The byte value to write to the register.
    *
    * Issues a TLM_WRITE_COMMAND transaction with 1-byte data length to the DUT.
    */
   virtual void write_register_8(unsigned int offset, uint8_t write_value) = 0;

   /**
    * @brief Virtual destructor to allow proper cleanup of derived classes.
    */
   virtual ~aon_timer_if() = default;
};
