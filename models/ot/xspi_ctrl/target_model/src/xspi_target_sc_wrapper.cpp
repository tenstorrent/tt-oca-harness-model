/*
 * xspi_target_sc_wrapper.cpp
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */

#include "xspi_target_sc_wrapper.h"
#include <cstring>
#include <iostream>
#include <iomanip>

xspi_target_sc_module::xspi_target_sc_module(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , target_model()
{
    target_socket.register_b_transport(this, &xspi_target_sc_module::b_transport);
    std::cout << "[XSPI Target SC Module] Created: " << name << std::endl;
}

void xspi_target_sc_module::b_transport(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time& delay)
{
    if (flash_trans_prelude && !flash_trans_prelude(trans, delay)) {
        return;
    }

    cdns_extension* cext = trans.get_extension<cdns_extension>();
    if (cext == nullptr) {
        std::cerr << "[xspi_target] Error: cdns_extension not found in transaction"
                  << std::endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        delay = sc_core::SC_ZERO_TIME;
        return;
    }

    const uint8_t opcode = cext->opcode;
    const uint32_t address = static_cast<uint32_t>(trans.get_address());

    std::vector<uint8_t> rx_buffer;
    std::vector<uint8_t> tx_buffer;

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        rx_buffer.resize(trans.get_data_length());
        const bool success =
            target_model.process_command(opcode, address, rx_buffer);
        if (success) {
            if (rx_buffer.size() <= trans.get_data_length()) {
                std::memcpy(trans.get_data_ptr(), rx_buffer.data(), rx_buffer.size());
                trans.set_data_length(rx_buffer.size());
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
            } else {
                trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            }
        } else {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        }
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        tx_buffer.assign(trans.get_data_ptr(),
                         trans.get_data_ptr() + trans.get_data_length());
        const bool success =
            target_model.process_command(opcode, address, rx_buffer, tx_buffer);
        if (success) {
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        }
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    }

    delay = sc_core::SC_ZERO_TIME;
}
