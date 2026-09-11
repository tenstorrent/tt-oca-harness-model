// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_test.cpp
 * @brief KMAC test harness implementation
 *
 * This file implements the KMAC test harness constructor, port binding,
 * and register access helper functions using TLM b_transport.
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#include "kmac_test.h"

/******************************************************************************
 * @brief KMAC test harness constructor
 *
 * Initializes all complementary ports and interface channels for connecting
 * to the KMAC model.
 ******************************************************************************/
kmac_test::kmac_test(sc_module_name name, unsigned int num_app_intf)
    : kmac_basetest(name)
    , idle_i("idle_i")
    , lc_escalate_en_o("lc_escalate_en_o")
    , rst_no("rst_no")
    , clk_o("clk_o")
    , keymgr_socket("keymgr_socket")
    , app_port(nullptr)
    , NumAppIntf(num_app_intf)
    , logger()
{
    // Configure logger
    logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Allocate application interface ports (TLM pattern)
    if (NumAppIntf > 0) {
        app_port = new sc_port<kmac_app_if>[NumAppIntf];
    }

    // Initialize control outputs
    lc_escalate_en_o.initialize(false); // No escalation by default
    rst_no.initialize(true);             // Reset inactive (active-low)
    clk_o.initialize(false);             // Clock signal (initial low state)

    // Register clock driver process
    SC_THREAD(clock_driver);

    REG_INFO(2, logger) << "KMAC test harness instantiated with "
                         << NumAppIntf << " application interfaces";
}

/******************************************************************************
 * @brief KMAC test harness destructor
 *
 * Cleans up dynamically allocated arrays.
 ******************************************************************************/
kmac_test::~kmac_test()
{
    if (app_port != nullptr) {
        delete[] app_port;
        app_port = nullptr;
    }
}

/******************************************************************************
 * @brief Clock driver process
 *
 * Maintains clock frequency outputs for model timing calculations.
 ******************************************************************************/
void kmac_test::clock_driver()
{
    while (true) {
        wait(1, SC_MS); // Update clock values periodically if needed
        // Clock frequencies remain constant for basic testing
    }
}

/******************************************************************************
 * @brief Read 32-bit register via TLM
 *
 * Performs blocking TLM b_transport read transaction to KMAC model.
 ******************************************************************************/
void kmac_test::register_read_32(unsigned int offset, uint32_t &read_value)
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
        std::cerr << "[ERROR] " << "TLM read error at offset 0x" << std::hex << offset << std::endl;
    }
}

/******************************************************************************
 * @brief Write 32-bit register via TLM
 *
 * Performs blocking TLM b_transport write transaction to KMAC model.
 ******************************************************************************/
void kmac_test::register_write_32(unsigned int offset, uint32_t write_value)
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
        std::cerr << "[ERROR] " << "TLM write error at offset 0x" << std::hex << offset << std::endl;
    }
}

/******************************************************************************
 * @brief Read 8-bit register via TLM
 *
 * Performs blocking TLM b_transport read transaction (byte access).
 ******************************************************************************/
void kmac_test::register_read_8(unsigned int offset, uint8_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        std::cerr << "[ERROR] " << "TLM read error at offset 0x" << std::hex << offset << std::endl;
    }
}

/******************************************************************************
 * @brief Write 8-bit register via TLM
 *
 * Performs blocking TLM b_transport write transaction (byte access).
 ******************************************************************************/
void kmac_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error()) {
        std::cerr << "[ERROR] " << "TLM write error at offset 0x" << std::hex << offset << std::endl;
    }
}

void kmac_test::keymgr_write_word(uint64_t offset, uint32_t value)
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

void kmac_test::set_keymgr_key(const uint32_t* s0, const uint32_t* s1, size_t len_bytes)
{
    size_t num_words = (len_bytes + 3) / 4;
    if (num_words > 8) num_words = 8;

    for (size_t i = 0; i < num_words; i++)
        keymgr_write_word(i * 4,        s0[i]);  // KEY_SHARE0: 0x00-0x1C
    for (size_t i = 0; i < num_words; i++)
        keymgr_write_word(0x20 + i * 4, s1[i]);  // KEY_SHARE1: 0x20-0x3C
    keymgr_write_word(0x40, 0x1u);               // KEY_CTRL: valid
}

void kmac_test::clear_keymgr_key()
{
    keymgr_write_word(0x40, 0x0u);               // KEY_CTRL: invalid
}

void kmac_test::keymgr_read_word(uint64_t offset, uint32_t &value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    keymgr_socket->b_transport(trans, delay);
}
