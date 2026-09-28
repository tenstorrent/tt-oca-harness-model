// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "el2_pic_test.h"
#include "tlm_probe.h"
#include <tlm.h>
#include <sstream>

el2_pic_test::el2_pic_test(sc_module_name name, unsigned num_irq)
    : el2_pic_basetest(name)
    , rst_no("rst_no")
    , irq_o("irq_o", num_irq)
{
    logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);
}

void el2_pic_test::note_transport(const simtlm::access_result& r, const char* op,
                                  unsigned byte_offset)
{
    if (r.ok())
        return;

    ++m_transport_failures;

    std::ostringstream oss;
    oss << op << " at byte offset 0x" << std::hex << byte_offset
        << " returned " << simtlm::response_name(r.status);
    m_last_transport_error = oss.str();

    REG_WARN(1, logger) << "TRANSPORT: " << m_last_transport_error << std::endl;
}

void el2_pic_test::clear_transport_failures()
{
    m_transport_failures = 0;
    m_last_transport_error.clear();
}

void el2_pic_test::reg_write_32(unsigned byte_offset, uint32_t value)
{
    const auto r = simtlm::write_word<uint32_t>(initiator_socket, byte_offset, value);
    note_transport(r, "reg_write_32", byte_offset);
}

uint32_t el2_pic_test::reg_read_32(unsigned byte_offset)
{
    uint32_t data = 0;
    const auto r = simtlm::read_word<uint32_t>(initiator_socket, byte_offset, data);
    note_transport(r, "reg_read_32", byte_offset);
    return data;
}

simtlm::access_result el2_pic_test::probe(simtlm::defect d,
                                          const simtlm::target_geometry& geo,
                                          tlm::tlm_command cmd)
{
    // Deliberately not routed through note_transport(): the caller is asking for
    // a rejection, so a non-OK status here is the expected result rather than a
    // suite failure.
    return simtlm::probe_defect(initiator_socket, d, geo, cmd);
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
