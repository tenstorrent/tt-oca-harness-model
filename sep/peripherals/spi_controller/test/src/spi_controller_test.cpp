// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "spi_controller_test.h"
#include <iomanip>

void spi_controller_test::register_read_8(unsigned int offset, uint8_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&read_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        CSML_ERROR(1, logger) << "[TLM-2] Response error from b_transport" << std::endl;
    }
}

void spi_controller_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&write_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
    {
        CSML_ERROR(1, logger) << "[TLM-2] Response error from b_transport" << std::endl;
    }
}

/**
 * @brief 32-bit register read function
 */
void spi_controller_test::read_register_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    // Note: csml framework doesn't set response status, so we don't check for errors
}

/**
 * @brief 32-bit register write function
 */
void spi_controller_test::write_register_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    // Note: csml framework doesn't set response status, so we don't check for errors
}

/**
 * @brief 32-bit register write with custom byte enables (for testing ACCESSINVAL)
 */
void spi_controller_test::write_register_32_with_byte_enable(unsigned int offset, uint32_t write_value,
                                                         unsigned char be0, unsigned char be1,
                                                         unsigned char be2, unsigned char be3)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    unsigned char byte_enable[4] = {be0, be1, be2, be3};

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(byte_enable);  /// Use custom byte enables
    trans.set_byte_enable_length(4);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    // Note: csml framework doesn't set response status, so we don't check for errors
}

/**
 * @brief Assert function to verify register values
 */
void spi_controller_test::assert_register_value(const char* test_name, uint32_t expected, uint32_t actual)
{
    if (expected == actual)
    {
        CSML_INFO(2, logger) << "[PASS] " << test_name
                  << " - Expected: 0x" << std::hex << std::setw(8) << std::setfill('0') << expected
                  << ", Actual: 0x" << std::hex << std::setw(8) << std::setfill('0') << actual
                  << std::dec << std::endl;
    }
    else
    {
        CSML_ERROR(2, logger) << "[FAIL] " << test_name
                  << " - Expected: 0x" << std::hex << std::setw(8) << std::setfill('0') << expected
                  << ", Actual: 0x" << std::hex << std::setw(8) << std::setfill('0') << actual
                  << std::dec << std::endl;
        CSML_ERROR(1, logger) << "[ASSERTION] " << test_name << std::endl;
    }
}

/**
 * @brief Helper function to toggle reset signal
 */
void spi_controller_test::toggle_reset()
{
    CSML_INFO(1, logger) << "\n[INFO] Toggling reset signal..." << std::endl;

    // Assert reset (active-low, so write 0)
    rst_ni->write(false);
    CSML_INFO(2, logger) << "[INFO] Reset asserted (rst_ni = 0)" << std::endl;
    wait(50, SC_NS);

    // De-assert reset (write 1)
    rst_ni->write(true);
    CSML_INFO(2, logger) << "[INFO] Reset de-asserted (rst_ni = 1)" << std::endl;
    wait(50, SC_NS);

    CSML_INFO(1, logger) << "[INFO] Reset toggle complete\n" << std::endl;
}

void spi_controller_test::end_of_elaboration()
{
   rst_ni.write(true);
   clk_i.write(false);
}