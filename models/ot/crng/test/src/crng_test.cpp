#include "crng_test.h"
#include <tlm.h>

crng_test::crng_test(sc_module_name name)
    : crng_basetest(name)
{
    // Initialize logger
    logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Note: No ports to initialize for crng_base testing
    // Port initialization will be added when CRNG class is implemented
}

void crng_test::initialize_signals()
{
    // Note: No signals to initialize for crng_base testing
    // Signal initialization will be added when CRNG class is implemented
}

void crng_test::register_read_8(unsigned int offset, uint8_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = sc_time(10, SC_NS);

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&read_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
    {
        CSML_ERROR(0, logger) << "Register read failed at offset 0x" << std::hex << offset;
    }
}

void crng_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = sc_time(10, SC_NS);

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&write_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
    {
        CSML_ERROR(0, logger) << "Register write failed at offset 0x" << std::hex << offset;
    }
}

void crng_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = sc_time(10, SC_NS);

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&read_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
    {
        CSML_ERROR(0, logger) << "Register read failed at offset 0x" << std::hex << offset;
    }
}

void crng_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = sc_time(10, SC_NS);

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&write_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    wait(delay);

    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
    {
        CSML_ERROR(0, logger) << "Register write failed at offset 0x" << std::hex << offset;
    }
}

// Note: Port-related helper methods removed for crng_base testing
// These will be re-added when CRNG class with ports is implemented
