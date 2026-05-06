/******************************************************************************
 * @file xspi_ctrl_test.cpp
 * @brief xspi_ctrl test harness implementation
 *
 * Implements register_read_32, register_write_32, register_read_8, and
 * register_write_8 following the same TLM b_transport pattern used by
 * dma_test.cpp. Each helper constructs a TLM generic payload, sets the
 * data_length appropriate to the access width, and calls
 * initiator_socket->b_transport.
 *
 * Reference:
 *   - dma/test/src/dma_test.cpp (structural pattern)
 *   - docs/sections/xspi_ctrl-port-interfaces.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "xspi_ctrl_test.h"

/******************************************************************************
 * @brief 32-bit register read via TLM b_transport
 *
 * Constructs a TLM_READ_COMMAND payload with data_length=4 and issues it
 * through initiator_socket->b_transport. The 32-bit bus width of
 * xspi_ctrl_basetest::initiator_socket and the scml2 register file both
 * support atomic 32-bit word accesses.
 *
 * @param offset     Byte offset of the target register
 * @param read_value Reference to receive the 32-bit register value
 ******************************************************************************/
void xspi_ctrl_test::register_read_32(unsigned int offset, uint32_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(0, logger) << "register_read_32: TLM read error at offset 0x"
                              << std::hex << offset;
    }

    wait(delay);
}

/******************************************************************************
 * @brief 32-bit register write via TLM b_transport
 *
 * Constructs a TLM_WRITE_COMMAND payload with data_length=4 and issues it
 * through initiator_socket->b_transport.
 *
 * @param offset      Byte offset of the target register
 * @param write_value 32-bit value to write
 ******************************************************************************/
void xspi_ctrl_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(0, logger) << "register_write_32: TLM write error at offset 0x"
                              << std::hex << offset;
    }

    wait(delay);
}

/******************************************************************************
 * @brief 8-bit register read via TLM b_transport
 *
 * Constructs a TLM_READ_COMMAND payload with data_length=1 and issues it
 * through initiator_socket->b_transport.
 *
 * @param offset     Byte offset of the target register
 * @param read_value Reference to receive the 8-bit register value
 ******************************************************************************/
void xspi_ctrl_test::register_read_8(unsigned int offset, uint8_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(&read_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(0, logger) << "register_read_8: TLM read error at offset 0x"
                              << std::hex << offset;
    }

    wait(delay);
}

/******************************************************************************
 * @brief 8-bit register write via TLM b_transport
 *
 * Constructs a TLM_WRITE_COMMAND payload with data_length=1 and issues it
 * through initiator_socket->b_transport.
 *
 * @param offset      Byte offset of the target register
 * @param write_value 8-bit value to write
 ******************************************************************************/
void xspi_ctrl_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(&write_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(0, logger) << "register_write_8: TLM write error at offset 0x"
                              << std::hex << offset;
    }

    wait(delay);
}
