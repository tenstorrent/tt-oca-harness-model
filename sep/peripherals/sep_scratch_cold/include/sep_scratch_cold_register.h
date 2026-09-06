// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "reg_file.h"

namespace sep_scratch_cold {

template<unsigned int N>
class SCRATCH_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SCRATCH_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      data(reg_name + ".data", *this, 0, 32), 
      Reserved0(reg_name + ".Reserved0", *this, 32, 32)
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
    regmodel::Bitfield<N> data;
    regmodel::Bitfield<N> Reserved0;
};


}
