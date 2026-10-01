// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * Copyright header goes here...
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "reg_file.h"

namespace sep_output_remap_ctrl {

// Note: the RDL specifies accesswidth=64 (this register is defined as a
// full-64-bit-access-only CSR), and RTL's generated regblock additionally
// merges partial (sub-64-bit) writes using wstrb-derived bit-enables. The
// underlying regmodel::Memory/regmodel::Reg write path used here always applies
// a full 64-bit overwrite regardless of the TLM byte_enable mask — a partial
// write would incorrectly clobber the untouched bytes. This is a shared
// limitation of the regmodel register library, not specific to this
// peripheral, and matches the RDL's own accesswidth=64 declaration; no
// firmware access pattern found (ap_stee_output_remap_test.c) uses anything
// but 64-bit reads/writes to this register, so it has no observed practical
// impact today.
template<unsigned int N>
class REGION_ATTRS_type : public regmodel::Reg<N>
{
public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;

    // Layout (output_remap.rdl, tt-oca-hw #2572):
    //   offset [55:0]   sw=rw hw=r  reset 0  translation for this region
    //   [62:56]         reserved (RAZ/WI)
    //   valid  [63]     sw=rw hw=r  reset 0  if set the region is remapped by
    //                   offset; if clear the address passes through unchanged
    REGION_ATTRS_type(std::string reg_name, memory_type& memory, unsigned int offset)
        : regmodel::Reg<N>(reg_name, memory, offset,
                      0x80ffffffffffffff, // read_bit_mask  — [63] + [55:0]
                      0x80ffffffffffffff, // write_bit_mask — [63] + [55:0]
                      0x0)                // reset value
        , offset    (reg_name + ".offset",    *this, 0,  56)
        , Reserved0 (reg_name + ".Reserved0", *this, 56,  7)
        , valid     (reg_name + ".valid",     *this, 63,  1)
    {
        this->set_read_write_restrictions(memory);
    }

    using regmodel::Reg<N>::operator=;
    using regmodel::Reg<N>::operator+=;
    using regmodel::Reg<N>::operator-=;
    using regmodel::Reg<N>::operator/=;
    using regmodel::Reg<N>::operator*=;
    using regmodel::Reg<N>::operator%=;
    using regmodel::Reg<N>::operator^=;
    using regmodel::Reg<N>::operator&=;
    using regmodel::Reg<N>::operator|=;
    using regmodel::Reg<N>::operator>>=;
    using regmodel::Reg<N>::operator<<=;

    regmodel::Bitfield<N> offset;
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> valid;
};

} // namespace sep_output_remap_ctrl
