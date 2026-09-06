// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "reg_file.h"

namespace sep_cpu_ctrl {

template<unsigned int N>
class CLOCK_GATE_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    CLOCK_GATE_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      // Reset 0x0, not 0x1F0021: sep_cpu_ctrl.rdl now declares this register a
      // "reserved placeholder, not yet implemented" carrying a single
      // pka_cg_enable field that resets to 0, and och_sep_top_reg.h agrees
      // (SEP_CPU_CTRL_CLOCK_GATE_CTRL_REG_DEFAULT = 0x0). The named fields below
      // are kept because they cost nothing and document the intended layout, but
      // nothing gates a clock on them in the VP, so the old reset was inventing
      // state that neither the RTL nor any model consumes.
      regmodel::Reg<N>(reg_name, memory, offset, 0x3F07FF, 0x3F07FF, 0x0),
      pka_cg_enable(reg_name + ".pka_cg_enable", *this, 0, 1),
      dma_cg_enable(reg_name + ".dma_cg_enable", *this, 1, 1),
      mailbox_cg_en(reg_name + ".mailbox_cg_en", *this, 2, 1),
      fabric_cg_enable(reg_name + ".fabric_cg_enable", *this, 3, 1),
      filter_in_cg_enable(reg_name + ".filter_in_cg_enable", *this, 4, 1),
      sram_cg_enable(reg_name + ".sram_cg_enable", *this, 5, 1),
      zeroer_cg_enable(reg_name + ".zeroer_cg_enable", *this, 6, 1),
      alias_remap_cg_enable(reg_name + ".alias_remap_cg_enable", *this, 7, 1),
      filter_out_cg_enable(reg_name + ".filter_out_cg_enable", *this, 8, 1),
      ot_hmac_cg_enable(reg_name + ".ot_hmac_cg_enable", *this, 9, 1),
      entropy_fifo_cg_enable(reg_name + ".entropy_fifo_cg_enable", *this, 10, 1),
      Reserved0(reg_name + ".Reserved0", *this, 11, 5),
      cg_hysteresis(reg_name + ".cg_hysteresis", *this, 16, 6),
      Reserved1(reg_name + ".Reserved1", *this, 22, 42)
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
    regmodel::Bitfield<N> pka_cg_enable;
    regmodel::Bitfield<N> dma_cg_enable;
    regmodel::Bitfield<N> mailbox_cg_en;
    regmodel::Bitfield<N> fabric_cg_enable;
    regmodel::Bitfield<N> filter_in_cg_enable;
    regmodel::Bitfield<N> sram_cg_enable;
    regmodel::Bitfield<N> zeroer_cg_enable;
    regmodel::Bitfield<N> alias_remap_cg_enable;
    regmodel::Bitfield<N> filter_out_cg_enable;
    regmodel::Bitfield<N> ot_hmac_cg_enable;
    regmodel::Bitfield<N> entropy_fifo_cg_enable;
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> cg_hysteresis;
    regmodel::Bitfield<N> Reserved1;
};

template<unsigned int N>
class REFERENCE_COUNTER_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    REFERENCE_COUNTER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffffffULL, 0xffffffffffffffffULL, 0),
      rc(reg_name + ".rc", *this, 0, 64)
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
    regmodel::Bitfield<N> rc;
};

template<unsigned int N>
class TIMEOUT_INTERRUPT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_INTERRUPT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xff, 0x0, 0),
      sys_in_timeout_int(reg_name + ".sys_in_timeout_int", *this, 0, 1),
      dma_data_timeout_int(reg_name + ".dma_data_timeout_int", *this, 1, 1),
      alias_remap_timeout_int(reg_name + ".alias_remap_timeout_int", *this, 2, 1),
      filter_out_timeout_int(reg_name + ".filter_out_timeout_int", *this, 3, 1),
      entropy_read_timeout_int(reg_name + ".entropy_read_timeout_int", *this, 4, 1),
      entropy_write_timeout_int(reg_name + ".entropy_write_timeout_int", *this, 5, 1),
      inbound_mailbox_timeout_int(reg_name + ".inbound_mailbox_timeout_int", *this, 6, 1),
      outbound_mailbox_timeout_int(reg_name + ".outbound_mailbox_timeout_int", *this, 7, 1),
      Reserved0(reg_name + ".Reserved0", *this, 8, 56)
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
    regmodel::Bitfield<N> sys_in_timeout_int;
    regmodel::Bitfield<N> dma_data_timeout_int;
    regmodel::Bitfield<N> alias_remap_timeout_int;
    regmodel::Bitfield<N> filter_out_timeout_int;
    regmodel::Bitfield<N> entropy_read_timeout_int;
    regmodel::Bitfield<N> entropy_write_timeout_int;
    regmodel::Bitfield<N> inbound_mailbox_timeout_int;
    regmodel::Bitfield<N> outbound_mailbox_timeout_int;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class PKA_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    PKA_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x7, 0x7, 0x0),
      pka_dpa_disable(reg_name + ".pka_dpa_disable", *this, 0, 1), 
      pka_noise_src(reg_name + ".pka_noise_src", *this, 1, 1), 
      pka_noise_src_valid(reg_name + ".pka_noise_src_valid", *this, 2, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 61)
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
    regmodel::Bitfield<N> pka_dpa_disable;
    regmodel::Bitfield<N> pka_noise_src;
    regmodel::Bitfield<N> pka_noise_src_valid;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class TIMEOUT_COUNT_DMA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_DMA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_SYS_IN_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_SYS_IN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_MAILBOX_INBOUND_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_MAILBOX_INBOUND_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_MAILBOX_OUTBOUND_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_MAILBOX_OUTBOUND_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_ENTROPY_WRITE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_ENTROPY_WRITE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_ENTROPY_READ_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_ENTROPY_READ_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_FILTER_OUT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_FILTER_OUT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_COUNT_ALIAS_REMAP_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_COUNT_ALIAS_REMAP_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffff, 0xffffffffffff, 0),
      data(reg_name + ".data", *this, 0, 48), 
      Reserved0(reg_name + ".Reserved0", *this, 48, 16)
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

template<unsigned int N>
class TIMEOUT_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xff, 0xff, 0),
      sys_in_timeout_en(reg_name + ".sys_in_timeout_en", *this, 0, 1),
      dma_data_timeout_en(reg_name + ".dma_data_timeout_en", *this, 1, 1),
      alias_remap_timeout_en(reg_name + ".alias_remap_timeout_en", *this, 2, 1),
      filter_out_timeout_en(reg_name + ".filter_out_timeout_en", *this, 3, 1),
      entropy_read_timeout_en(reg_name + ".entropy_read_timeout_en", *this, 4, 1),
      entropy_write_timeout_en(reg_name + ".entropy_write_timeout_en", *this, 5, 1),
      inbound_mailbox_timeout_en(reg_name + ".inbound_mailbox_timeout_en", *this, 6, 1),
      outbound_mailbox_timeout_en(reg_name + ".outbound_mailbox_timeout_en", *this, 7, 1),
      Reserved0(reg_name + ".Reserved0", *this, 8, 56)
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
    regmodel::Bitfield<N> sys_in_timeout_en;
    regmodel::Bitfield<N> dma_data_timeout_en;
    regmodel::Bitfield<N> alias_remap_timeout_en;
    regmodel::Bitfield<N> filter_out_timeout_en;
    regmodel::Bitfield<N> entropy_read_timeout_en;
    regmodel::Bitfield<N> entropy_write_timeout_en;
    regmodel::Bitfield<N> inbound_mailbox_timeout_en;
    regmodel::Bitfield<N> outbound_mailbox_timeout_en;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class TIMEOUT_CLEAR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_CLEAR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0xff, 0),
      sys_in_timeout_clear(reg_name + ".sys_in_timeout_clear", *this, 0, 1),
      dma_data_timeout_clear(reg_name + ".dma_data_timeout_clear", *this, 1, 1),
      alias_remap_timeout_clear(reg_name + ".alias_remap_timeout_clear", *this, 2, 1),
      filter_out_timeout_clear(reg_name + ".filter_out_timeout_clear", *this, 3, 1),
      entropy_read_timeout_clear(reg_name + ".entropy_read_timeout_clear", *this, 4, 1),
      entropy_write_timeout_clear(reg_name + ".entropy_write_timeout_clear", *this, 5, 1),
      inbound_mailbox_timeout_clear(reg_name + ".inbound_mailbox_timeout_clear", *this, 6, 1),
      outbound_mailbox_timeout_clear(reg_name + ".outbound_mailbox_timeout_clear", *this, 7, 1),
      Reserved0(reg_name + ".Reserved0", *this, 8, 56)
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
    regmodel::Bitfield<N> sys_in_timeout_clear;
    regmodel::Bitfield<N> dma_data_timeout_clear;
    regmodel::Bitfield<N> alias_remap_timeout_clear;
    regmodel::Bitfield<N> filter_out_timeout_clear;
    regmodel::Bitfield<N> entropy_read_timeout_clear;
    regmodel::Bitfield<N> entropy_write_timeout_clear;
    regmodel::Bitfield<N> inbound_mailbox_timeout_clear;
    regmodel::Bitfield<N> outbound_mailbox_timeout_clear;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class TIMEOUT_MODE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TIMEOUT_MODE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0xffff, 0),
      sys_in_timeout_mode(reg_name + ".sys_in_timeout_mode", *this, 0, 2),
      dma_data_timeout_mode(reg_name + ".dma_data_timeout_mode", *this, 2, 2),
      alias_remap_timeout_mode(reg_name + ".alias_remap_timeout_mode", *this, 4, 2),
      filter_out_timeout_mode(reg_name + ".filter_out_timeout_mode", *this, 6, 2),
      entropy_read_timeout_mode(reg_name + ".entropy_read_timeout_mode", *this, 8, 2),
      entropy_write_timeout_mode(reg_name + ".entropy_write_timeout_mode", *this, 10, 2),
      inbound_mailbox_timeout_mode(reg_name + ".inbound_mailbox_timeout_mode", *this, 12, 2),
      outbound_mailbox_timeout_mode(reg_name + ".outbound_mailbox_timeout_mode", *this, 14, 2),
      Reserved0(reg_name + ".Reserved0", *this, 16, 48)
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
    regmodel::Bitfield<N> sys_in_timeout_mode;
    regmodel::Bitfield<N> dma_data_timeout_mode;
    regmodel::Bitfield<N> alias_remap_timeout_mode;
    regmodel::Bitfield<N> filter_out_timeout_mode;
    regmodel::Bitfield<N> entropy_read_timeout_mode;
    regmodel::Bitfield<N> entropy_write_timeout_mode;
    regmodel::Bitfield<N> inbound_mailbox_timeout_mode;
    regmodel::Bitfield<N> outbound_mailbox_timeout_mode;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_TEST_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_TEST_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFC000000ULL, 0x0, 0),
      Reserved0(reg_name + ".Reserved0", *this, 0, 26),
      sep_standalone(reg_name + ".sep_standalone", *this, 26, 1),
      fast_pka_en(reg_name + ".fast_pka_en", *this, 27, 1),
      fast_sram_en(reg_name + ".fast_sram_en", *this, 28, 1),
      fast_dccm_en(reg_name + ".fast_dccm_en", *this, 29, 1),
      fast_iccm_en(reg_name + ".fast_iccm_en", *this, 30, 1),
      fast_spi_en(reg_name + ".fast_spi_en", *this, 31, 1)
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
    regmodel::Bitfield<N> sep_standalone;
    regmodel::Bitfield<N> fast_pka_en;
    regmodel::Bitfield<N> fast_sram_en;
    regmodel::Bitfield<N> fast_dccm_en;
    regmodel::Bitfield<N> fast_iccm_en;
    regmodel::Bitfield<N> fast_spi_en;
};

template<unsigned int N>
class SEP_GLOBAL_BASE_ADDR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_GLOBAL_BASE_ADDR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffff, 0xffffffffffffff, 0),
      addr(reg_name + ".addr", *this, 0, 56), 
      Reserved0(reg_name + ".Reserved0", *this, 56, 8)
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
    regmodel::Bitfield<N> addr;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_LOCAL_BASE_ADDR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_LOCAL_BASE_ADDR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffff, 0xffffffffffffff, 0xD0000000),
      addr(reg_name + ".addr", *this, 0, 56), 
      Reserved0(reg_name + ".Reserved0", *this, 56, 8)
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
    regmodel::Bitfield<N> addr;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_REGION_SIZE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_REGION_SIZE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x01000000),
      size(reg_name + ".size", *this, 0, 32), 
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
    regmodel::Bitfield<N> size;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SMU_GLOBAL_BASE_ADDR_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SMU_GLOBAL_BASE_ADDR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffffffffff, 0xffffffffffffff, 0x80000000),
      addr(reg_name + ".addr", *this, 0, 56), 
      Reserved0(reg_name + ".Reserved0", *this, 56, 8)
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
    regmodel::Bitfield<N> addr;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SMU_REGION_SIZE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SMU_REGION_SIZE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x40000000),
      size(reg_name + ".size", *this, 0, 32), 
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
    regmodel::Bitfield<N> size;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SMC_FUSE_SENSE_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SMC_FUSE_SENSE_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x0, 0x0),
      smc_fuse_sense_done(reg_name + ".smc_fuse_sense_done", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 63)
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
    regmodel::Bitfield<N> smc_fuse_sense_done;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_FUSE_SENSE_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_FUSE_SENSE_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x0, 0x0),
      sep_fuse_sense_done(reg_name + ".sep_fuse_sense_done", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 63)
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
    regmodel::Bitfield<N> sep_fuse_sense_done;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_STRAPS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_STRAPS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x0, 0x0),
      test_en(reg_name + ".test_en", *this, 0, 1), 
      bypass_mem_repair(reg_name + ".bypass_mem_repair", *this, 1, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 2, 30)
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
    regmodel::Bitfield<N> test_en;
    regmodel::Bitfield<N> bypass_mem_repair;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class RAS_BANK_INFO_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RAS_BANK_INFO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x0),
      bank_chip(reg_name + ".bank_chip", *this, 0, 4), 
      bank_instance(reg_name + ".bank_instance", *this, 4, 4), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> bank_chip;
    regmodel::Bitfield<N> bank_instance;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_SW_DEBUG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_SW_DEBUG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      sep_sw_debug(reg_name + ".sep_sw_debug", *this, 0, 32), 
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
    regmodel::Bitfield<N> sep_sw_debug;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_NMI_VEC_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_NMI_VEC_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xfffffffe, 0xfffffffe, 0x0000000060000080),
      rsvd(reg_name + ".rsvd", *this, 0, 1), 
      nmi_vec(reg_name + ".nmi_vec", *this, 1, 31), 
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
    regmodel::Bitfield<N> rsvd;
    regmodel::Bitfield<N> nmi_vec;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_NMI_VEC_LOCK_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_NMI_VEC_LOCK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x0),
      lock(reg_name + ".lock", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 63)
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
    regmodel::Bitfield<N> lock;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class EXT_TRNG_SRC_SEL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    EXT_TRNG_SRC_SEL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x7, 0x7, 0x7),
      sel(reg_name + ".sel", *this, 0, 3), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 61)
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
    regmodel::Bitfield<N> sel;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class EXT_TRNG_SRC_SEL_LOCK_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    EXT_TRNG_SRC_SEL_LOCK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x0),
      lock(reg_name + ".lock", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 63)
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
    regmodel::Bitfield<N> lock;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class KM_WIPE_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    KM_WIPE_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x0),
      wipe_state(reg_name + ".wipe_state", *this, 0, 1),
      Reserved0(reg_name + ".Reserved0", *this, 1, 63)
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
    regmodel::Bitfield<N> wipe_state;
    regmodel::Bitfield<N> Reserved0;
};

template<unsigned int N>
class SEP_VERSION_ID_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SEP_VERSION_ID_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x0, 0xdeadbeef),
      version_id(reg_name + ".version_id", *this, 0, 32), 
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
    regmodel::Bitfield<N> version_id;
    regmodel::Bitfield<N> Reserved0;
};


}