/**
 * @file aon_timer_test.cpp
 * @brief AON Timer test class implementation - register read/write helper methods.
 *
 * Implements the 32-bit and 8-bit register access helper methods declared in
 * aon_timer_test.h. Each method constructs a TLM-2.0 generic payload and issues
 * it via the inherited initiator_socket using blocking transport (b_transport).
 *
 * All methods use a zero sc_time offset (SC_ZERO_TIME) for the blocking transport
 * call. Temporal decoupling with the quantum keeper is managed by the DUT model
 * (aon_timer), not by these access helpers.
 *
 * Error Handling:
 *   - If the TLM response status is not TLM_OK_RESPONSE, an SC_REPORT_ERROR is
 *     issued with the response status value to make test failures visible.
 *
 * Thread Safety:
 *   - These methods are intended to be called from a single SC_THREAD context
 *     (the testbench run_tests() thread). Concurrent calls from multiple threads
 *     are not supported.
 */

#include "aon_timer_test.h"
#include <tlm.h>
#include <cstring>

// =========================================================================
// 32-bit Register Access Implementations
// =========================================================================

/**
 * @brief Perform a 32-bit register write via TLM-2.0 blocking transport.
 *
 * Constructs a TLM generic payload for a 4-byte write at the given byte offset
 * and issues it to the DUT via initiator_socket->b_transport(). After the call
 * the response status is validated.
 */
void aon_timer_test::write_register_32(unsigned int offset, uint32_t write_value)
{
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_WRITE_COMMAND);
   trans.set_address(static_cast<sc_dt::uint64>(offset));
   trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
   trans.set_data_length(4);
   trans.set_streaming_width(4);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_byte_enable_length(0);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   initiator_socket->b_transport(trans, delay);

   if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
   {
      SC_REPORT_ERROR("aon_timer_test",
                      "write_register_32: TLM transaction returned non-OK response");
   }
}

/**
 * @brief Perform a 32-bit register read via TLM-2.0 blocking transport.
 *
 * Constructs a TLM generic payload for a 4-byte read at the given byte offset
 * and issues it to the DUT via initiator_socket->b_transport(). The read value
 * is captured via the data_ptr. After the call the response status is validated.
 */
void aon_timer_test::read_register_32(unsigned int offset, uint32_t& read_value)
{
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   read_value = 0x0;

   trans.set_command(tlm::TLM_READ_COMMAND);
   trans.set_address(static_cast<sc_dt::uint64>(offset));
   trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
   trans.set_data_length(4);
   trans.set_streaming_width(4);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_byte_enable_length(0);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   initiator_socket->b_transport(trans, delay);

   if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
   {
      SC_REPORT_ERROR("aon_timer_test",
                      "read_register_32: TLM transaction returned non-OK response");
   }
}

// =========================================================================
// 8-bit Register Access Implementations
// =========================================================================

/**
 * @brief Perform an 8-bit register read via TLM-2.0 blocking transport.
 *
 * Constructs a TLM generic payload for a 1-byte read at the given byte offset
 * and issues it to the DUT via initiator_socket->b_transport(). The read value
 * is captured via the data_ptr. After the call the response status is validated.
 */
void aon_timer_test::read_register_8(unsigned int offset, uint8_t& read_value)
{
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   read_value = 0x0;

   trans.set_command(tlm::TLM_READ_COMMAND);
   trans.set_address(static_cast<sc_dt::uint64>(offset));
   trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
   trans.set_data_length(1);
   trans.set_streaming_width(1);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_byte_enable_length(0);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   initiator_socket->b_transport(trans, delay);

   if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
   {
      SC_REPORT_ERROR("aon_timer_test",
                      "read_register_8: TLM transaction returned non-OK response");
   }
}

/**
 * @brief Perform an 8-bit register write via TLM-2.0 blocking transport.
 *
 * Constructs a TLM generic payload for a 1-byte write at the given byte offset
 * and issues it to the DUT via initiator_socket->b_transport(). After the call
 * the response status is validated.
 */
void aon_timer_test::write_register_8(unsigned int offset, uint8_t write_value)
{
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_WRITE_COMMAND);
   trans.set_address(static_cast<sc_dt::uint64>(offset));
   trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
   trans.set_data_length(1);
   trans.set_streaming_width(1);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_byte_enable_length(0);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   initiator_socket->b_transport(trans, delay);

   if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
   {
      SC_REPORT_ERROR("aon_timer_test",
                      "write_register_8: TLM transaction returned non-OK response");
   }
}
