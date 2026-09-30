// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "reg_file.h"

namespace local_alias_remap {

template<unsigned int N>
class REGION_START_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    REGION_START_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xfffffffffff000, 0xfffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      start_addr(reg_name + ".start_addr", *this, 12, 44), 
      Reserved1(reg_name + ".Reserved1", *this, 56, 8)
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
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> start_addr;
    regmodel::Bitfield<N> Reserved1;
};

template<unsigned int N>
class REGION_END_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    REGION_END_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xfffffffffff000, 0xfffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      end_addr(reg_name + ".end_addr", *this, 12, 44), 
      Reserved1(reg_name + ".Reserved1", *this, 56, 8)
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
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> end_addr;
    regmodel::Bitfield<N> Reserved1;
};

template<unsigned int N>
class REGION_ATTRS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    // Layout (alias_remap.rdl, tt-oca-hw #2464):
    //   [11:0]           reserved (RAZ/WI)
    //   offset    [55:12] sw=rw hw=r  added to addr[55:12] on a hit
    //   cacheable [59:56] sw=rw hw=r  replaces AxCACHE bit-for-bit on a hit
    //   [62:60]          reserved (RAZ/WI) -- bit 62 was the old 1-bit cacheable
    //   valid     [63]    sw=rw hw=r  region enable
    REGION_ATTRS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x8ffffffffffff000, 0x8ffffffffffff000, 0x0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 12), 
      offset(reg_name + ".offset", *this, 12, 44), 
      cacheable(reg_name + ".cacheable", *this, 56, 4), 
      Reserved1(reg_name + ".Reserved1", *this, 60, 3), 
      valid(reg_name + ".valid", *this, 63, 1)
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
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> offset;
    regmodel::Bitfield<N> cacheable;
    regmodel::Bitfield<N> Reserved1;
    regmodel::Bitfield<N> valid;
};


}
