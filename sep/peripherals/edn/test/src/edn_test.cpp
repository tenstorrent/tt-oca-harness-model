/**
 * @file edn_test.cpp
 * @brief EDN test harness implementation
 *
 * Implements register access utilities, CSRNG simulation, and endpoint
 * request generation for EDN verification.
 */

#include "edn_test.h"
#include "edn.h"
#include <iomanip>

/**
 * @brief EDN test constructor
 * @param name SystemC module name
 *
 * Initializes all ports, binds CSRNG command channel, and configures logger.
 */
edn_test::edn_test(sc_module_name name)
    : edn_basetest(name)
    , intr_edn_cmd_req_done("intr_edn_cmd_req_done")
    , intr_edn_fatal_err("intr_edn_fatal_err")
    , alert_recov_alert("alert_recov_alert")
    , alert_fatal_alert("alert_fatal_alert")
    , clk_o("clk_o")
    , rst_no("rst_no")
{
    // Configure logger
    logger.setMaxVerbosity(2);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    SC_METHOD(mock_endpoint_process);
    for (int i = 0; i < 8; ++i) {
        sensitive << edn_req[i];
    }
    dont_initialize();

    CSML_INFO(2, logger) << "EDN test harness constructed";
}

/**
 * @brief EDN test destructor
 */
edn_test::~edn_test()
{
    CSML_INFO(2, logger) << "EDN test harness destroyed";
}

/**
 * @brief Read 8-bit value from register via TLM
 * @param offset Register byte offset
 * @param read_value Output parameter for read data
 */
void edn_test::register_read_8(unsigned int offset, uint8_t &read_value)
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

    if (trans.is_response_error()) {
        CSML_ERROR(1, logger) << "Register read failed at offset 0x" << std::hex << offset;
    }
}

/**
 * @brief Write 8-bit value to register via TLM
 * @param offset Register byte offset
 * @param write_value Data to write
 */
void edn_test::register_write_8(unsigned int offset, uint8_t write_value)
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

    if (trans.is_response_error()) {
        CSML_ERROR(1, logger) << "Register write failed at offset 0x" << std::hex << offset;
    }
}

/**
 * @brief Read 32-bit value from register via TLM
 * @param offset Register byte offset
 * @param read_value Output parameter for read data
 */
void edn_test::register_read_32(unsigned int offset, uint32_t &read_value)
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
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(1, logger) << "Register read failed at offset 0x" << std::hex << offset;
    } else {
        CSML_INFO(2, logger) << "Read 0x" << std::hex << std::setw(8) << std::setfill('0')
                              << read_value << " from offset 0x" << offset;
    }
}

/**
 * @brief Write 32-bit value to register via TLM
 * @param offset Register byte offset
 * @param write_value Data to write
 */
void edn_test::register_write_32(unsigned int offset, uint32_t write_value)
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
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        CSML_ERROR(1, logger) << "Register write failed at offset 0x" << std::hex << offset;
    } else {
        CSML_INFO(2, logger) << "Wrote 0x" << std::hex << std::setw(8) << std::setfill('0')
                              << write_value << " to offset 0x" << offset;
    }
}

/**
 * @brief Apply reset sequence
 * @param duration_ns Reset duration in nanoseconds
 */
void edn_test::apply_reset(double duration_ns)
{
    CSML_INFO(1, logger) << "Applying reset for " << duration_ns << " ns";

    rst_no.write(false);  // Assert reset (active-low)
    wait(duration_ns, SC_NS);
    rst_no.write(true);   // De-assert reset
    wait(10, SC_NS);      // Allow reset propagation

    CSML_INFO(1, logger) << "Reset sequence complete";
}

/**
 * @brief Set clock frequency
 * @param freq_hz Clock frequency in Hz
 */
void edn_test::set_clock_frequency(double freq_hz)
{
    CSML_INFO(2, logger) << "Setting clock frequency to " << freq_hz << " Hz";
    clk_o.write(freq_hz);
}

void edn_test::set_forced_csrng_ack_status(uint32_t status)
{
    if (m_dut) {
        m_dut->force_csrng_ack_status(status);
    }
}

void edn_test::provide_csrng_entropy(const uint32_t genbits[4], bool fips_compliance)
{
    if (m_dut) {
        m_dut->receive_csrng_entropy(genbits, fips_compliance);
    }
    m_mock_fips = fips_compliance;
    for (int i = 0; i < 4; i++) {
        m_mock_buffer.push(genbits[i]);
    }
}


