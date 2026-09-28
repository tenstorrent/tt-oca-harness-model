// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "csrng_test.h"
#include "tlm_probe.h"
#include <tlm.h>
#include <sstream>

csrng_test::csrng_test(sc_module_name name)
    : csrng_basetest(name)
{
    // Initialize logger
    logger.setMaxVerbosity(REG_DEFAULT_VERBOSITY);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Note: No ports to initialize for csrng_base testing
    // Port initialization will be added when CRNG class is implemented
}

void csrng_test::initialize_signals()
{
    // Note: No signals to initialize for csrng_base testing
    // Signal initialization will be added when CRNG class is implemented
}

// Every helper offers the target a 10 ns incoming delay and then waits out
// whatever came back, so the model's own annotation is honoured rather than
// discarded. A non-OK response used to be logged and forgotten; it is now
// recorded, and the testbench fails the enclosing test on it.
namespace {
constexpr int CSRNG_ACCESS_DELAY_NS = 10;
}

void csrng_test::note_transport(const simtlm::access_result &r, const char *op,
                                unsigned int offset)
{
    if (r.ok()) return;

    ++m_transport_failures;

    std::ostringstream oss;
    oss << op << " at offset 0x" << std::hex << offset
        << " returned " << simtlm::response_name(r.status);
    m_last_transport_error = oss.str();

    REG_ERROR(0, logger) << m_last_transport_error;
}

void csrng_test::clear_transport_failures()
{
    m_transport_failures = 0;
    m_last_transport_error.clear();
}

simtlm::access_result csrng_test::probe(simtlm::defect d,
                                        const simtlm::target_geometry &geo,
                                        tlm::tlm_command cmd)
{
    // Deliberate rejections are expected here, so they are not recorded.
    return simtlm::probe_defect(initiator_socket, d, geo, cmd);
}

void csrng_test::register_read_8(unsigned int offset, uint8_t &read_value)
{
    const auto r = simtlm::read_word<uint8_t>(
        initiator_socket, offset, read_value,
        sc_time(CSRNG_ACCESS_DELAY_NS, SC_NS));
    wait(r.delay_out);
    note_transport(r, "register_read_8", offset);
}

void csrng_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    const auto r = simtlm::write_word<uint8_t>(
        initiator_socket, offset, write_value,
        sc_time(CSRNG_ACCESS_DELAY_NS, SC_NS));
    wait(r.delay_out);
    note_transport(r, "register_write_8", offset);
}

void csrng_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    const auto r = simtlm::read_word<uint32_t>(
        initiator_socket, offset, read_value,
        sc_time(CSRNG_ACCESS_DELAY_NS, SC_NS));
    wait(r.delay_out);
    note_transport(r, "register_read_32", offset);
}

void csrng_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    const auto r = simtlm::write_word<uint32_t>(
        initiator_socket, offset, write_value,
        sc_time(CSRNG_ACCESS_DELAY_NS, SC_NS));
    wait(r.delay_out);
    note_transport(r, "register_write_32", offset);
}

// Note: Port-related helper methods removed for csrng_base testing
// These will be re-added when CRNG class with ports is implemented
