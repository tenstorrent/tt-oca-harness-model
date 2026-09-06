// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * Copyright header goes here...
 */

#pragma once
#include <iostream>
#include <systemc.h>
#include "reg_file.h"

namespace sep_filter_ctrl {

template<unsigned int N>
class FILTER_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    FILTER_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x8000000001ff0113, 0x8000000001ff0113, 0x0),
      read_allowed(reg_name + ".read_allowed", *this, 0, 1),
      write_allowed(reg_name + ".write_allowed", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 2),
      entry_enabled(reg_name + ".entry_enabled", *this, 4, 1),
      reserved1(reg_name + ".reserved1", *this, 5, 3),
      allow_ns(reg_name + ".allow_ns", *this, 8, 1),
      reserved2(reg_name + ".reserved2", *this, 9, 3),
      data_bus_width(reg_name + ".data_bus_width", *this, 12, 3),
      reserved3(reg_name + ".reserved3", *this, 15, 1),
      src_id(reg_name + ".src_id", *this, 16, 4),
      group_id(reg_name + ".group_id", *this, 20, 4),
      allow_burst(reg_name + ".allow_burst", *this, 24, 1),
      reserved4(reg_name + ".reserved4", *this, 25, 38),
      locked(reg_name + ".locked", *this, 63, 1)
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
    regmodel::Bitfield<N> read_allowed;
    regmodel::Bitfield<N> write_allowed;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> entry_enabled;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> allow_ns;
    regmodel::Bitfield<N> reserved2;
    regmodel::Bitfield<N> data_bus_width;
    regmodel::Bitfield<N> reserved3;
    regmodel::Bitfield<N> src_id;
    regmodel::Bitfield<N> group_id;
    regmodel::Bitfield<N> allow_burst;
    regmodel::Bitfield<N> reserved4;
    regmodel::Bitfield<N> locked;
};

template<unsigned int N>
class START_ADDR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    START_ADDR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffff, 0xffffffffffffff, 0x0),
      start_addr(reg_name + ".start_addr", *this, 0, 56),
      reserved(reg_name + ".reserved", *this, 56, 8)
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
    regmodel::Bitfield<N> start_addr;
    regmodel::Bitfield<N> reserved;
};

template<unsigned int N>
class END_ADDR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    END_ADDR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffff, 0xffffffffffffff, 0x7),
      end_addr(reg_name + ".end_addr", *this, 0, 56),
      reserved(reg_name + ".reserved", *this, 56, 8)
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
    regmodel::Bitfield<N> end_addr;
    regmodel::Bitfield<N> reserved;
};

}
