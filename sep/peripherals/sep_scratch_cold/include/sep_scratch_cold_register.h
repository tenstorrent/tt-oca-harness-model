
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace sep_scratch_cold {

template<unsigned int N>
class SCRATCH_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    SCRATCH_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      data(reg_name + ".data", *this, 0, 32), 
      Reserved0(reg_name + ".Reserved0", *this, 32, 32)
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
    csml_bitfield<N> data;
    csml_bitfield<N> Reserved0;
};


}