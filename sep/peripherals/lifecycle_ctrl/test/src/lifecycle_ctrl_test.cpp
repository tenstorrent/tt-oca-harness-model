// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "lifecycle_ctrl_test.h"
#include <tlm.h>

void lifecycle_ctrl_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = 0;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t *>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_ok())
        read_value = data;
}

void lifecycle_ctrl_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = write_value;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t *>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
}
