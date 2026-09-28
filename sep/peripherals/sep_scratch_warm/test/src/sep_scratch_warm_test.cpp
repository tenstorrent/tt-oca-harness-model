// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_warm_test.h"

#include <tlm.h>

#include "tlm_probe.h"

// Byte-granular CSR access. Memory::write_registers()/read_registers() handle
// partial and unaligned accesses within a word, so these reach a single byte of
// a 64-bit SCRATCH entry — which is what makes them useful for checking that the
// reserved upper half stays masked no matter how narrowly it is written.
//
// Both return the transport outcome rather than swallowing it. The read leaves
// `read_value` untouched unless the access actually succeeded, so a failed
// transaction can never be mistaken for a register that read zero.

simtlm::access_result sep_scratch_warm_test::register_read_8(unsigned int offset,
                                                             uint8_t &read_value)
{
    return simtlm::read_word<uint8_t>(initiator_socket, offset, read_value);
}

simtlm::access_result sep_scratch_warm_test::register_write_8(unsigned int offset,
                                                              uint8_t write_value)
{
    return simtlm::write_word<uint8_t>(initiator_socket, offset, write_value);
}
