// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file entropy_src_test.cpp
 * @brief entropy_src test harness implementation
 ******************************************************************************/

#include "entropy_src_test.h"

#include <stdexcept>
#include <sstream>

namespace {

[[noreturn]] void fail_tlm(const char* op, unsigned int offset,
                           tlm::tlm_response_status st)
{
    std::ostringstream oss;
    oss << op << " TLM response error at offset 0x" << std::hex << offset
        << " status=" << static_cast<int>(st);
    throw std::runtime_error(oss.str());
}

}  // namespace

void entropy_src_test::register_read_32(unsigned int offset, uint32_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
        fail_tlm("register_read_32", offset, trans.get_response_status());

    wait(delay);
}

void entropy_src_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
        fail_tlm("register_write_32", offset, trans.get_response_status());

    wait(delay);
}

void entropy_src_test::register_read_8(unsigned int offset, uint8_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(&read_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
        fail_tlm("register_read_8", offset, trans.get_response_status());

    wait(delay);
}

void entropy_src_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(static_cast<sc_dt::uint64>(offset));
    trans.set_data_ptr(&write_value);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);

    if (trans.is_response_error())
        fail_tlm("register_write_8", offset, trans.get_response_status());

    wait(delay);
}
