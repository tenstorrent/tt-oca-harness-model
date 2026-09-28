// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox_test.cpp
 * @brief Implementation of register access helper functions
 *
 * Provides TLM-2.0 generic payload-based register read/write functions
 * for mailbox register access via the initiator socket.
 *
 * These helpers used to inspect the response status and then zero the caller's
 * buffer when it indicated an error. That is worse than ignoring the status: a
 * refused transaction became a well-formed 0, which is exactly the value most
 * reset and write-only expectations are looking for. The buffer is now left
 * alone and the failure is recorded, so it surfaces as a test failure instead.
 */

#include "mailbox_test.h"
#include "reg_logger.h"
#include "tlm_probe.h"

#include <sstream>

void mailbox_test::note_transport(const simtlm::access_result &r, const char *op,
                                  unsigned int offset)
{
    if (r.ok())
        return;

    ++m_transport_failures;

    std::ostringstream oss;
    oss << op << " at offset 0x" << std::hex << offset
        << " returned " << simtlm::response_name(r.status);
    m_last_transport_error = oss.str();
}

void mailbox_test::clear_transport_failures()
{
    m_transport_failures = 0;
    m_last_transport_error.clear();
}

/**
 * @brief Read 64-bit value from register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param read_value Reference to store 64-bit read data
 */
void mailbox_test::register_read_64(unsigned int offset, uint64_t &read_value)
{
    const auto r = simtlm::read_word<uint64_t>(initiator_socket, offset, read_value);
    note_transport(r, "register_read_64", offset);
}

/**
 * @brief Write 64-bit value to register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param write_value 64-bit data to write
 */
void mailbox_test::register_write_64(unsigned int offset, uint64_t write_value)
{
    const auto r = simtlm::write_word<uint64_t>(initiator_socket, offset, write_value);
    note_transport(r, "register_write_64", offset);
}

/**
 * @brief Read 32-bit value from register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param read_value Reference to store 32-bit read data
 *
 * Reads lower 32 bits of a 64-bit register for compatibility testing.
 */
void mailbox_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    const auto r = simtlm::read_word<uint32_t>(initiator_socket, offset, read_value);
    note_transport(r, "register_read_32", offset);
}

/**
 * @brief Write 32-bit value to register at specified offset
 * @param offset Register address offset (0x00-0x48)
 * @param write_value 32-bit data to write
 */
void mailbox_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    const auto r = simtlm::write_word<uint32_t>(initiator_socket, offset, write_value);
    note_transport(r, "register_write_32", offset);
}

simtlm::access_result mailbox_test::probe(simtlm::defect d,
                                          const simtlm::target_geometry &geo,
                                          tlm::tlm_command cmd)
{
    return simtlm::probe_defect(initiator_socket, d, geo, cmd);
}
