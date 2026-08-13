
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace local_alias_remap {

template<unsigned int N>
class REGION_START_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REGION_START_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xfffffffffff000, 0xfffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      start_addr(reg_name + ".start_addr", *this, 12, 44), 
      Reserved1(reg_name + ".Reserved1", *this, 56, 8)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> start_addr;
    csml_bitfield<N> Reserved1;
};

template<unsigned int N>
class REGION_END_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REGION_END_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xfffffffffff000, 0xfffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      end_addr(reg_name + ".end_addr", *this, 12, 44), 
      Reserved1(reg_name + ".Reserved1", *this, 56, 8)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> end_addr;
    csml_bitfield<N> Reserved1;
};

template<unsigned int N>
class REGION_ATTRS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REGION_ATTRS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xc0fffffffffff000, 0xc0fffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      offset(reg_name + ".offset", *this, 12, 44), 
      cacheable(reg_name + ".cacheable", *this, 62, 1), 
      valid(reg_name + ".valid", *this, 63, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> offset;
    csml_bitfield<N> cacheable;
    csml_bitfield<N> valid;
};


}