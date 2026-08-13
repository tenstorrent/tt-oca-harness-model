
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

namespace sep_reset_ctrl {

template<unsigned int N>
class SW_RESET_N_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    SW_RESET_N_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1f, 0x1f, 0x1e),
      km_sw_rst_n(reg_name + ".km_sw_rst_n", *this, 0, 1), 
      otbn_sw_rst_n(reg_name + ".otbn_sw_rst_n", *this, 1, 1), 
      aes_sw_rst_n(reg_name + ".aes_sw_rst_n", *this, 2, 1), 
      hmac_sw_rst_n(reg_name + ".hmac_sw_rst_n", *this, 3, 1), 
      kmac_sw_rst_n(reg_name + ".kmac_sw_rst_n", *this, 4, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 5, 59)
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
    csml_bitfield<N> km_sw_rst_n;
    csml_bitfield<N> otbn_sw_rst_n;
    csml_bitfield<N> aes_sw_rst_n;
    csml_bitfield<N> hmac_sw_rst_n;
    csml_bitfield<N> kmac_sw_rst_n;
    csml_bitfield<N> Reserved0;
};


}