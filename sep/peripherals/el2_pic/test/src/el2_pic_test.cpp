// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "el2_pic_test.h"
#include <tlm.h>

el2_pic_test::el2_pic_test(sc_module_name name, unsigned num_irq)
    : el2_pic_basetest(name)
    , rst_no("rst_no")
    , irq_o("irq_o", num_irq)
{
    logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);
}

void el2_pic_test::reg_write_32(unsigned byte_offset, uint32_t value)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = value;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(byte_offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
}

uint32_t el2_pic_test::reg_read_32(unsigned byte_offset)
{
    tlm::tlm_generic_payload trans;
    uint32_t data = 0;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(byte_offset);
    trans.set_data_ptr(reinterpret_cast<uint8_t*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    return data;
}

void el2_pic_test::drive_irq(unsigned src, bool level)
{
    irq_o[src].write(level);
}

void el2_pic_test::assert_reset()
{
    rst_no.write(false);
}

void el2_pic_test::deassert_reset()
{
    rst_no.write(true);
}
