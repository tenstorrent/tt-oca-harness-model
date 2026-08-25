// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox_test.cpp
 * @brief Implementation of register access helper functions
 *
 * Provides TLM-2.0 generic payload-based register read/write functions
 * for 64-bit mailbox register access via initiator socket.
 */

#include "mailbox_test.h"
#include "csml_logger.h"

/**
 * @brief Read 64-bit value from register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param read_value Reference to store 64-bit read data
 *
 * Implements TLM-2.0 blocking transport with:
 * - Generic payload with READ command
 * - 64-bit data width (8 bytes)
 * - Zero delay for untimed operation
 */
void mailbox_test::register_read_64(unsigned int offset, uint64_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Configure generic payload for 64-bit read
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(8);  // 64-bit = 8 bytes
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);

    // Check response status
    if (trans.is_response_error()) {
        // For write-only registers, read will return error response
        read_value = 0;  // Undefined value
    }
}

/**
 * @brief Write 64-bit value to register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param write_value 64-bit data to write
 *
 * Implements TLM-2.0 blocking transport with:
 * - Generic payload with WRITE command
 * - 64-bit data width (8 bytes)
 * - Zero delay for untimed operation
 */
void mailbox_test::register_write_64(unsigned int offset, uint64_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Configure generic payload for 64-bit write
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(8);  // 64-bit = 8 bytes
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);

    // Check response status
    if (trans.is_response_error()) {
        // For read-only registers or error conditions, write fails
        // No action needed - error response recorded
    }
}

/**
 * @brief Read 32-bit value from register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param read_value Reference to store 32-bit read data
 *
 * Reads lower 32 bits of 64-bit register for compatibility testing.
 */
void mailbox_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Configure generic payload for 32-bit read
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);  // 32-bit = 4 bytes
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);

    // Check response status
    if (trans.is_response_error()) {
        read_value = 0;
    }
}

/**
 * @brief Write 32-bit value to register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param write_value 32-bit data to write
 *
 * Writes lower 32 bits of 64-bit register for compatibility testing.
 */
void mailbox_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Configure generic payload for 32-bit write
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);  // 32-bit = 4 bytes
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);
}
