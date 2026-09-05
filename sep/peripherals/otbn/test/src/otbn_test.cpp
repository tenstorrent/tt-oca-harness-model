// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_test.cpp
 * @brief OTBN test class TLM transaction implementations
 * 
 * Implements TLM register access methods for OTBN testing:
 * - 32-bit register read/write via TLM transactions
 * - Register value validation with error reporting
 * - Transaction payload setup and execution
 * 
 * These methods are used by the testbench to interact with the OTBN DUT
 * through the TLM target socket.
 */

#include "otbn_test.h"
#include <tlm.h>

// ============================================================================
// OTBN Test Class Implementation
// ============================================================================

/**
 * @brief Read a 32-bit register via TLM transaction
 * @param offset Register byte offset from base address
 * @param read_value Reference to store the read value
 * 
 * Executes a TLM blocking read transaction to the OTBN DUT.
 * Reports error if transaction fails.
 */
void otbn_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Setup transaction for read
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);  // 32 bits = 4 bytes
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);

    // Check response
    if (trans.is_response_error()) {
        SC_REPORT_ERROR("TLM", "Read transaction failed");
    }
}

/**
 * @brief Write a 32-bit register via TLM transaction
 * @param offset Register byte offset from base address
 * @param write_value Value to write to the register
 * 
 * Executes a TLM blocking write transaction to the OTBN DUT.
 * Reports error if transaction fails.
 */
void otbn_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    // Setup transaction for write
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);  // 32 bits = 4 bytes
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Execute blocking transport
    initiator_socket->b_transport(trans, delay);

    // Check response
    if (trans.is_response_error()) {
        SC_REPORT_ERROR("TLM", "Write transaction failed");
    }
}

/**
 * @brief Validate register value and report result
 * @param offset Register byte offset from base address
 * @param expected_value Expected register value
 * @param reg_name Human-readable register name for reporting
 * @return true if register matches expected value, false otherwise
 * 
 * Reads the register and compares with expected value.
 * Prints PASS or FAIL message with register name and values.
 */
bool otbn_test::assert_register_value(unsigned int offset, uint32_t expected_value, const char* reg_name)
{
    uint32_t read_value;
    register_read_32(offset, read_value);

    if (read_value == expected_value) {
        REG_INFO(1, logger) << "  PASS: " << reg_name << " = 0x" << std::hex << read_value;
        return true;
    } else {
        REG_ERROR(0, logger) << "  FAIL: " << reg_name << " = 0x" << std::hex << read_value
                  << ", expected 0x" << expected_value;
        return false;
    }
}
