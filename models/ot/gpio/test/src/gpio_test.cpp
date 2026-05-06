/**
 * GPIO Test Module Implementation (RDL-Based Single-Pin)
 *
 * Provides helper functions for testing single GPIO pin:
 * - TLM register access (with PROT extension support)
 * - GPIO pin control
 * - LSIO interface control
 * - Signal monitoring
 * - Reset control
 */

#include "gpio_test.h"
#include <iomanip>

//=============================================================================
// Register Access Functions (No PROT Extension)
//=============================================================================

// Read 32-bit register via TLM (default PROT=0x0)
void gpio_test::read_register_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = m_qk.get_local_time();  // Start with quantum keeper time

    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    // Update quantum keeper with transaction delay
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        sync_quantum();
    }

    m_last_response = payload.get_response_status();
    if (m_last_response != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(1, logger) << "Read transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}

// Write 32-bit register via TLM (default PROT=0x0)
void gpio_test::write_register_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = m_qk.get_local_time();  // Start with quantum keeper time

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(payload, delay);

    // Update quantum keeper with transaction delay
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        sync_quantum();
    }

    m_last_response = payload.get_response_status();
    if (m_last_response != tlm::TLM_OK_RESPONSE) {
        CSML_ERROR(1, logger) << "Write transaction failed at offset 0x"
                  << std::hex << offset << std::dec << std::endl;
    }
}

//=============================================================================
// Register Access Functions (With PROT Extension)
//=============================================================================

// Read 32-bit register with ARPROT
void gpio_test::read_register_32_with_prot(unsigned int offset, uint32_t &read_value, uint8_t arprot)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = m_qk.get_local_time();  // Start with quantum keeper time

    payload.set_command(tlm::TLM_READ_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Attach PROT extension
    gpio_prot_extension* ext = new gpio_prot_extension();
    ext->set_arprot(arprot);
    payload.set_extension(ext);

    initiator_socket->b_transport(payload, delay);

    // Update quantum keeper with transaction delay
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        sync_quantum();
    }

    m_last_response = payload.get_response_status();
    if (m_last_response != tlm::TLM_OK_RESPONSE) {
        CSML_INFO(2, logger) << "Read transaction blocked at offset 0x"
                  << std::hex << offset << " (ARPROT=0x" << (int)arprot << ")" << std::dec << std::endl;
    }

    // Clean up extension
    payload.release_extension(ext);
}

// Write 32-bit register with AWPROT
void gpio_test::write_register_32_with_prot(unsigned int offset, uint32_t write_value, uint8_t awprot)
{
    tlm::tlm_generic_payload payload;
    sc_time delay = m_qk.get_local_time();  // Start with quantum keeper time

    payload.set_command(tlm::TLM_WRITE_COMMAND);
    payload.set_address(offset);
    payload.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    payload.set_data_length(4);
    payload.set_streaming_width(4);
    payload.set_byte_enable_ptr(0);
    payload.set_dmi_allowed(false);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    // Attach PROT extension
    gpio_prot_extension* ext = new gpio_prot_extension();
    ext->set_awprot(awprot);
    payload.set_extension(ext);

    initiator_socket->b_transport(payload, delay);

    // Update quantum keeper with transaction delay
    m_qk.set(delay);
    if (m_qk.need_sync()) {
        sync_quantum();
    }

    m_last_response = payload.get_response_status();
    if (m_last_response != tlm::TLM_OK_RESPONSE) {
        CSML_INFO(2, logger) << "Write transaction blocked at offset 0x"
                  << std::hex << offset << " (AWPROT=0x" << (int)awprot << ")" << std::dec << std::endl;
    }

    // Clean up extension
    payload.release_extension(ext);
}

//=============================================================================
// Convenience Functions for SEP vs Non-SEP Access
//=============================================================================

// Read register as SEP (PROT=0x1 = privileged, secure, data)
void gpio_test::read_register_32_sep(unsigned int offset, uint32_t &read_value)
{
    read_register_32_with_prot(offset, read_value, 0x1);
}

// Write register as SEP (PROT=0x1)
void gpio_test::write_register_32_sep(unsigned int offset, uint32_t write_value)
{
    write_register_32_with_prot(offset, write_value, 0x1);
}

// Read register as non-SEP (PROT=0x0 = unprivileged, secure, data)
void gpio_test::read_register_32_nonsep(unsigned int offset, uint32_t &read_value)
{
    read_register_32_with_prot(offset, read_value, 0x0);
}

// Write register as non-SEP (PROT=0x0)
void gpio_test::write_register_32_nonsep(unsigned int offset, uint32_t write_value)
{
    write_register_32_with_prot(offset, write_value, 0x0);
}

// Check if last transaction succeeded
bool gpio_test::last_transaction_succeeded() const
{
    return m_last_response == tlm::TLM_OK_RESPONSE;
}

// Assert function to validate expected vs actual values
void gpio_test::assert_equal(uint32_t expected, uint32_t actual, const char* message)
{
    if (expected == actual) {
        CSML_INFO(1, logger) << "PASS: " << message << std::endl;
        CSML_INFO(2, logger) << "      Expected: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << expected << ", Got: 0x" << std::setw(8) << std::setfill('0')
                  << actual << std::dec << std::endl;
    } else {
        CSML_ERROR(1, logger) << "FAIL: " << message << std::endl;
        CSML_ERROR(1, logger) << "      Expected: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << expected << ", Got: 0x" << std::setw(8) << std::setfill('0')
                  << actual << std::dec << std::endl;
    }
}

//=============================================================================
// GPIO Pin Helper Functions
//=============================================================================

// Drive DUT GPIO input
void gpio_test::drive_gpio_pin(bool value)
{
    gpio_in_o.write(value);
    CSML_DEBUG(3, logger) << "Driving GPIO input to " << value << std::endl;
}

// Read DUT GPIO output
bool gpio_test::read_gpio_out()
{
    return gpio_out_i.read();
}

// Read DUT GPIO output enable
bool gpio_test::read_gpio_oe()
{
    return gpio_oe_i.read();
}

//=============================================================================
// LSIO Interface Helper Functions
//=============================================================================

// Drive LSIO output value
void gpio_test::drive_lsio_output(bool value)
{
    lsio_gpio_out_o.write(value);
    CSML_DEBUG(3, logger) << "Driving LSIO output to " << value << std::endl;
}

// Drive LSIO output enable
void gpio_test::drive_lsio_oe(bool value)
{
    lsio_gpio_oe_o.write(value);
    CSML_DEBUG(3, logger) << "Driving LSIO OE to " << value << std::endl;
}

// Set LSIO access indicator
void gpio_test::set_lsio_access(bool active)
{
    lsio_access_o.write(active);
    CSML_DEBUG(3, logger) << "Setting LSIO access to " << active << std::endl;
}

// Read LSIO input (from DUT)
bool gpio_test::read_lsio_input()
{
    return lsio_gpio_in_i.read();
}

//=============================================================================
// PAD Configuration Monitor Functions
//=============================================================================

// Read PAD drive strength
uint32_t gpio_test::read_pad_drive_strength()
{
    return pad_drive_strength_i.read();
}

// Read PAD pull enable
bool gpio_test::read_pad_pull_enable()
{
    return pad_pull_enable_i.read();
}

// Read PAD pull select
bool gpio_test::read_pad_pull_select()
{
    return pad_pull_select_i.read();
}

// Read PAD schmitt trigger enable
bool gpio_test::read_pad_schmitt_enable()
{
    return pad_schmitt_enable_i.read();
}

//=============================================================================
// Interrupt Monitor Function
//=============================================================================

// Read interrupt output
bool gpio_test::read_interrupt()
{
    return interrupt_i.read();
}

//=============================================================================
// Reset Control Functions
//=============================================================================

// Assert reset (active-low)
void gpio_test::assert_reset()
{
    rst_no.write(false);
    CSML_INFO(2, logger) << "Reset asserted" << std::endl;
}

// Deassert reset (release reset)
void gpio_test::deassert_reset()
{
    rst_no.write(true);
    CSML_INFO(2, logger) << "Reset deasserted" << std::endl;
}
