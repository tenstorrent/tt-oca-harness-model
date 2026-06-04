#include "aes_test.h"
#include <tlm.h>

aes_test::aes_test(sc_module_name name)
    : aes_basetest(name)
    , clk_o("clk_o")
    , rst_no("rst_no")
    , keymgr_socket("keymgr_socket")
    , alert_recov_ctrl_update_err_i("alert_recov_ctrl_update_err_i")
    , alert_fatal_fault_i("alert_fatal_fault_i")
    , idle_i("idle_i")
    , lc_escalate_en_o("lc_escalate_en_o")
    , m_keymgr_key_valid(false)
{
    // Initialize CSML logger
    logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Initialize signals
    initialize_signals();
}

void aes_test::initialize_signals()
{
    // Initialize clock signal (initial low state)
    clk_o.initialize(false);

    // Initialize reset to inactive (high for active-low)
    rst_no.initialize(true);

    // Initialize life cycle escalation to inactive
    lc_escalate_en_o.initialize(false);
}

void aes_test::register_read_8(unsigned int offset, uint8_t &read_value)
{
    tlm::tlm_generic_payload trans;
    uint8_t data;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&data);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_ok())
    {
        read_value = data;
    }
}

void aes_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    uint8_t data = write_value;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(&data);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
}

void aes_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    uint32_t data;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_ok())
    {
        read_value = data;
    }
}

void aes_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = write_value;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
}

void aes_test::wait_for_idle()
{
    // Poll idle status until AES becomes idle
    int timeout = 1000;
    while (!idle_i.read() && timeout > 0)
    {
        wait(1, SC_NS);
        timeout--;
    }

    if (timeout == 0)
    {
        CSML_WARN(1, logger) << "Timeout waiting for AES to become idle" << std::endl;
    }
}

void aes_test::trigger_reset()
{
    // Assert active-low reset
    rst_no.write(false);
    wait(10, SC_NS);

    // De-assert reset
    rst_no.write(true);
    wait(10, SC_NS);
}

void aes_test::trigger_escalation()
{
    // Assert life cycle escalation
    lc_escalate_en_o.write(true);
    wait(10, SC_NS);
}

void aes_test::check_alerts()
{
    // Check if any alerts are asserted
    if (alert_recov_ctrl_update_err_i.read())
    {
        CSML_INFO(1, logger) << "Recoverable alert detected: CTRL_UPDATE_ERR" << std::endl;
    }

    if (alert_fatal_fault_i.read())
    {
        CSML_ERROR(0, logger) << "Fatal alert detected: FATAL_FAULT" << std::endl;
    }
}

bool aes_test::is_idle()
{
    return idle_i.read();
}

void aes_test::keymgr_write_word(uint64_t offset, uint32_t value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    keymgr_socket->b_transport(trans, delay);
}

void aes_test::set_keymgr_key(const aes_if::keymgr_sideload_key_t& key)
{
    m_keymgr_key = key;
    m_keymgr_key_valid = key.valid;

    for (int i = 0; i < 8; i++)
        keymgr_write_word(i * 4,        key.key_share0[i]);  // 0x00-0x1C
    for (int i = 0; i < 8; i++)
        keymgr_write_word(0x20 + i * 4, key.key_share1[i]);  // 0x20-0x3C
    keymgr_write_word(0x40, key.valid ? 0x1u : 0x0u);        // KEY_CTRL
}

void aes_test::invalidate_keymgr_key()
{
    m_keymgr_key_valid = false;
    keymgr_write_word(0x40, 0x0u);
}

bool aes_test::get_keymgr_key(aes_if::keymgr_sideload_key_t& key)
{
    key = m_keymgr_key;
    return m_keymgr_key_valid;
}
