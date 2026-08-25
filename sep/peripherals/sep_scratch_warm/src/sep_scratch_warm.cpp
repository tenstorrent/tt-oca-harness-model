// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_warm.h"

#include <cstdint>

namespace {

// CSML 64-bit words receive narrow TLM stores as a value with only those bytes
// set (the rest 0). The legacy write callback then applies the 32-bit data
// mask against that zero-padded word and wipes SCRATCH.data. Merge with the
// current word using the beat's byte-enable before handle_write().
uint64_t byte_enable_to_mask(uint8_t be)
{
    uint64_t mask = 0;
    for (unsigned b = 0; b < 8; ++b) {
        if ((be & static_cast<uint8_t>(1u << b)) != 0)
            mask |= 0xFFull << (b * 8);
    }
    return mask;
}

uint64_t merge_be(uint64_t current, uint64_t value, uint8_t be)
{
    const uint64_t mask = byte_enable_to_mask(be);
    return (current & ~mask) | (value & mask);
}

}  // namespace

sep_scratch_warm_ip::sep_scratch_warm_ip(sc_module_name n)
    : sep_scratch_warm_base(n, "sep_scratch_warm", 8 * sizeof(unsigned long long))
    , rst_ni("rst_ni")
{
    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    for (unsigned i = 0; i < SCRATCH.size(); ++i) {
        memory.register_write_callback_with_be(
            [this, i](DT value, uint8_t be) -> bool {
                const DT merged = merge_be(static_cast<uint64_t>(SCRATCH[i]),
                                           static_cast<uint64_t>(value), be);
                return SCRATCH[i].handle_write(merged, SCRATCH[i].write_bit_mask);
            },
            SCRATCH[i].offset);
    }

    reset_all_registers();
}

void sep_scratch_warm_ip::reset_handler()
{
    if (!rst_ni.read())
        reset_all_registers();
}
