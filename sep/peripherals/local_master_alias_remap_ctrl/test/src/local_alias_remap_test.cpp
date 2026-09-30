// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "local_alias_remap_test.h"

using namespace sc_core;

tlm::tlm_response_status local_alias_remap_test::csr_transport(
    tlm::tlm_command cmd,
    unsigned int offset,
    unsigned char* data,
    unsigned int len,
    unsigned int streaming_width,
    unsigned char* be,
    unsigned int be_len)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(cmd);
    trans.set_address(offset);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(streaming_width);
    if (be != nullptr) {
        trans.set_byte_enable_ptr(be);
        trans.set_byte_enable_length(be_len);
    } else {
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
    }
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    return trans.get_response_status();
}

tlm::tlm_response_status local_alias_remap_test::register_read_64(
    unsigned int offset, uint64_t& read_value)
{
    return csr_transport(tlm::TLM_READ_COMMAND, offset,
                         reinterpret_cast<unsigned char*>(&read_value),
                         8, 8);
}

tlm::tlm_response_status local_alias_remap_test::register_write_64(
    unsigned int offset, uint64_t write_value)
{
    return csr_transport(tlm::TLM_WRITE_COMMAND, offset,
                         reinterpret_cast<unsigned char*>(&write_value),
                         8, 8);
}

tlm::tlm_response_status local_alias_remap_test::register_read_8(
    unsigned int offset, uint8_t& read_value)
{
    return csr_transport(tlm::TLM_READ_COMMAND, offset,
                         reinterpret_cast<unsigned char*>(&read_value),
                         1, 1);
}

tlm::tlm_response_status local_alias_remap_test::register_write_8(
    unsigned int offset, uint8_t write_value)
{
    return csr_transport(tlm::TLM_WRITE_COMMAND, offset,
                         reinterpret_cast<unsigned char*>(&write_value),
                         1, 1);
}
