/**
 * @file mailbox_test.h
 * @brief Extended test harness with register access helper methods
 *
 * Extends mailbox_basetest with register read/write helper methods for test
 * case implementation. Provides 8-bit granularity access functions.
 */

#pragma once
#include "mailbox_basetest.h"

/**
 * @class mailbox_test
 * @brief Extended test harness with register access utilities
 *
 * Provides helper methods for register-level transactions via TLM initiator socket.
 * Test cases can inherit from this class to access register read/write functions.
 */
class mailbox_test : public mailbox_basetest
{
public:
   /**
    * @brief Constructor for mailbox test harness
    * @param name SystemC module hierarchical name
    */
   mailbox_test(sc_module_name name) : mailbox_basetest(name)
   {
   }

   /**
    * @brief Read 64-bit value from register at specified offset
    * @param offset Register address offset (0x00-0x48)
    * @param read_value Reference to store 64-bit read data
    *
    * Executes TLM-2.0 blocking transport with READ command. For write-only
    * registers (WRITE_DATA, CTRL), returns 0 with error response.
    */
   void register_read_64(unsigned int offset, uint64_t &read_value);

   /**
    * @brief Write 64-bit value to register at specified offset
    * @param offset Register address offset (0x00-0x48)
    * @param write_value 64-bit data to write
    *
    * Executes TLM-2.0 blocking transport with WRITE command. For read-only
    * registers (READ_DATA, STATUS, ERROR_FLAGS, IRQP), write is rejected.
    */
   void register_write_64(unsigned int offset, uint64_t write_value);

   /**
    * @brief Read 32-bit value from register at specified offset
    * @param offset Register address offset (0x00-0x48)
    * @param read_value Reference to store 32-bit read data
    *
    * Reads lower 32 bits of 64-bit register for compatibility testing.
    */
   void register_read_32(unsigned int offset, uint32_t &read_value);

   /**
    * @brief Write 32-bit value to register at specified offset
    * @param offset Register address offset (0x00-0x48)
    * @param write_value 32-bit data to write
    *
    * Writes lower 32 bits of 64-bit register for compatibility testing.
    */
   void register_write_32(unsigned int offset, uint32_t write_value);

   /**
    * @brief Destructor
    */
   ~mailbox_test() {}
};