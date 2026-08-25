// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "../inc/hmac_test.h"
#include <iostream>
#include <iomanip>

// Read 32-bit register via TLM
void hmac_test::read_register_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;

    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, logger) << "ERROR: Read transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}

// Write 32-bit register via TLM
void hmac_test::write_register_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, logger) << "ERROR: Write transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}


// Write 8-bit (byte) register via TLM
void hmac_test::write_register_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    payload.set_data_length(1);
    payload.set_streaming_width(1);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, logger) << "ERROR: Byte write transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}

// Write 16-bit (halfword) register via TLM
void hmac_test::write_register_16(unsigned int offset, uint16_t write_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    payload.set_data_length(2);
    payload.set_streaming_width(2);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, logger) << "ERROR: Halfword write transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}



// Write a 32-bit word to the key manager private sideload bus
void hmac_test::keymgr_write_word(uint64_t offset, uint32_t value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = SC_ZERO_TIME;

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    keymgr_initiator_socket->b_transport(payload, delay);

    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(0, logger) << "ERROR: keymgr write failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}

// Assert function to validate expected vs actual values
void hmac_test::assert_equal(uint32_t expected, uint32_t actual, const char* message)
{
    if (expected == actual) {
        CSML_INFO(1, logger) << "PASS: " << message << std::endl;
        CSML_INFO(1, logger) << "      Expected: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << expected << ", Got: 0x" << std::setw(8) << std::setfill('0')
                  << actual << std::dec << std::endl;
    } else {
        CSML_ERROR(0, logger) << "FAIL: " << message << std::endl;
        CSML_ERROR(0, logger) << "      Expected: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << expected << ", Got: 0x" << std::setw(8) << std::setfill('0')
                  << actual << std::dec << std::endl;
        sc_stop();
    }
}

// Assert function to validate read-only protection (values should NOT match)
void hmac_test::assert_not_equal(uint32_t write_value, uint32_t read_value, const char* message)
{
    if (write_value != read_value) {
        CSML_INFO(1, logger) << "PASS: " << message << " (write blocked)" << std::endl;
        CSML_INFO(1, logger) << "      Written value: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << write_value << ", Read value: 0x" << std::setw(8) << std::setfill('0')
                  << read_value << std::dec << std::endl;
    } else {
        CSML_ERROR(0, logger) << "FAIL: " << message << " (should be read-only!)" << std::endl;
        CSML_ERROR(0, logger) << "      Written value: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << write_value << ", Read value: 0x" << std::setw(8) << std::setfill('0')
                  << read_value << std::dec << std::endl;
        sc_stop();
    }
}
