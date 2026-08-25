// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * Copyright header goes here...
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "csml.h"

namespace sep_output_remap_ctrl {

// Note: the RDL specifies accesswidth=64 (this register is defined as a
// full-64-bit-access-only CSR), and RTL's generated regblock additionally
// merges partial (sub-64-bit) writes using wstrb-derived bit-enables. The
// underlying csml_memory/csml_reg write path used here always applies a full
// 64-bit overwrite regardless of the TLM byte_enable mask — a partial write
// would incorrectly clobber the untouched bytes. This is a shared limitation
// of the csml register library, not specific to this peripheral, and matches
// the RDL's own accesswidth=64 declaration; no firmware access pattern found
// (ap_stee_output_remap_test.c) uses anything but 64-bit reads/writes to this
// register, so it has no observed practical impact today.
template<unsigned int N>
class REGION_ATTRS_type : public csml_reg<N>
{
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    REGION_ATTRS_type(std::string reg_name, memory_type& memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset,
                      0xffffffffffffff,   // read_bit_mask  — [55:0]
                      0xffffffffffffff,   // write_bit_mask — [55:0]
                      0x0)               // reset value
        , offset    (reg_name + ".offset",    *this, 0,  56)
        , Reserved0 (reg_name + ".Reserved0", *this, 56,  8)
    {
        this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;

    csml_bitfield<N> offset;
    csml_bitfield<N> Reserved0;
};

} // namespace sep_output_remap_ctrl
