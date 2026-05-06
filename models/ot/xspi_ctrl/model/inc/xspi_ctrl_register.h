
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace xspi_ctrl {

template<unsigned int N>
class cmd_reg0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd0(reg_name + ".cmd0", *this, 0, 32)
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
    csml_bitfield<N> cmd0;
};

template<unsigned int N>
class cmd_reg1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd1(reg_name + ".cmd1", *this, 0, 32)
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
    csml_bitfield<N> cmd1;
};

template<unsigned int N>
class cmd_reg2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd2(reg_name + ".cmd2", *this, 0, 32)
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
    csml_bitfield<N> cmd2;
};

template<unsigned int N>
class cmd_reg3_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg3_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd3(reg_name + ".cmd3", *this, 0, 32)
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
    csml_bitfield<N> cmd3;
};

template<unsigned int N>
class cmd_reg4_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg4_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd4(reg_name + ".cmd4", *this, 0, 32)
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
    csml_bitfield<N> cmd4;
};

template<unsigned int N>
class cmd_reg5_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_reg5_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      cmd5(reg_name + ".cmd5", *this, 0, 32)
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
    csml_bitfield<N> cmd5;
};

template<unsigned int N>
class cmd_status_ptr_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_status_ptr_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x7, 0x00000000),
      reserved0(reg_name + ".reserved0", *this, 3, 29), 
      thrd_status_sel(reg_name + ".thrd_status_sel", *this, 0, 3)
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
    csml_bitfield<N> reserved0;
    csml_bitfield<N> thrd_status_sel;
};

template<unsigned int N>
class cmd_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    cmd_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      cmd_status_field(reg_name + ".cmd_status_field", *this, 0, 32)
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
    csml_bitfield<N> cmd_status_field;
};

template<unsigned int N>
class ctrl_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ctrl_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      reserved1(reg_name + ".reserved1", *this, 17, 15), 
      init_comp(reg_name + ".init_comp", *this, 16, 1), 
      reserved2(reg_name + ".reserved2", *this, 10, 6), 
      init_fail(reg_name + ".init_fail", *this, 8, 2), 
      ctrl_busy(reg_name + ".ctrl_busy", *this, 7, 1), 
      discovery_busy(reg_name + ".discovery_busy", *this, 6, 1), 
      reserved3(reg_name + ".reserved3", *this, 5, 1), 
      gcmd_eng_mc_busy(reg_name + ".gcmd_eng_mc_busy", *this, 4, 1), 
      gcmd_eng_busy(reg_name + ".gcmd_eng_busy", *this, 3, 1), 
      acmd_eng_busy(reg_name + ".acmd_eng_busy", *this, 2, 1), 
      mdma_busy(reg_name + ".mdma_busy", *this, 1, 1), 
      sdma_busy(reg_name + ".sdma_busy", *this, 0, 1)
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
    csml_bitfield<N> reserved1;
    csml_bitfield<N> init_comp;
    csml_bitfield<N> reserved2;
    csml_bitfield<N> init_fail;
    csml_bitfield<N> ctrl_busy;
    csml_bitfield<N> discovery_busy;
    csml_bitfield<N> reserved3;
    csml_bitfield<N> gcmd_eng_mc_busy;
    csml_bitfield<N> gcmd_eng_busy;
    csml_bitfield<N> acmd_eng_busy;
    csml_bitfield<N> mdma_busy;
    csml_bitfield<N> sdma_busy;
};

template<unsigned int N>
class trd_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    trd_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      reserved4(reg_name + ".reserved4", *this, 8, 24), 
      trd_busy(reg_name + ".trd_busy", *this, 0, 8)
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
    csml_bitfield<N> reserved4;
    csml_bitfield<N> trd_busy;
};

template<unsigned int N>
class intr_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    intr_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x1ff7f000, 0x00000000),
      reserved5(reg_name + ".reserved5", *this, 29, 3), 
      dir_dev_err(reg_name + ".dir_dev_err", *this, 28, 1), 
      dir_ecc_corr_err(reg_name + ".dir_ecc_corr_err", *this, 27, 1), 
      dir_cmd_err(reg_name + ".dir_cmd_err", *this, 26, 1), 
      dir_dqs_err(reg_name + ".dir_dqs_err", *this, 25, 1), 
      dir_crc_err(reg_name + ".dir_crc_err", *this, 24, 1), 
      stig_done(reg_name + ".stig_done", *this, 23, 1), 
      sdma_err(reg_name + ".sdma_err", *this, 22, 1), 
      sdma_trigg(reg_name + ".sdma_trigg", *this, 21, 1), 
      cmd_ignored(reg_name + ".cmd_ignored", *this, 20, 1), 
      reserved6(reg_name + ".reserved6", *this, 19, 1), 
      ddma_terr(reg_name + ".ddma_terr", *this, 18, 1), 
      cdma_terr(reg_name + ".cdma_terr", *this, 17, 1), 
      ctrl_idle(reg_name + ".ctrl_idle", *this, 16, 1), 
      gp_open_drain_3(reg_name + ".gp_open_drain_3", *this, 15, 1), 
      gp_open_drain_2(reg_name + ".gp_open_drain_2", *this, 14, 1), 
      gp_open_drain_1(reg_name + ".gp_open_drain_1", *this, 13, 1), 
      gp_open_drain_0(reg_name + ".gp_open_drain_0", *this, 12, 1), 
      reserved7(reg_name + ".reserved7", *this, 0, 12)
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
    csml_bitfield<N> reserved5;
    csml_bitfield<N> dir_dev_err;
    csml_bitfield<N> dir_ecc_corr_err;
    csml_bitfield<N> dir_cmd_err;
    csml_bitfield<N> dir_dqs_err;
    csml_bitfield<N> dir_crc_err;
    csml_bitfield<N> stig_done;
    csml_bitfield<N> sdma_err;
    csml_bitfield<N> sdma_trigg;
    csml_bitfield<N> cmd_ignored;
    csml_bitfield<N> reserved6;
    csml_bitfield<N> ddma_terr;
    csml_bitfield<N> cdma_terr;
    csml_bitfield<N> ctrl_idle;
    csml_bitfield<N> gp_open_drain_3;
    csml_bitfield<N> gp_open_drain_2;
    csml_bitfield<N> gp_open_drain_1;
    csml_bitfield<N> gp_open_drain_0;
    csml_bitfield<N> reserved7;
};

template<unsigned int N>
class intr_enable_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    intr_enable_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x9ff7f000, 0x00000000),
      intr_en(reg_name + ".intr_en", *this, 31, 1), 
      reserved8(reg_name + ".reserved8", *this, 29, 2), 
      dir_dev_err_en(reg_name + ".dir_dev_err_en", *this, 28, 1), 
      dir_ecc_corr_err_en(reg_name + ".dir_ecc_corr_err_en", *this, 27, 1), 
      dir_cmd_err_en(reg_name + ".dir_cmd_err_en", *this, 26, 1), 
      dir_dqs_err_en(reg_name + ".dir_dqs_err_en", *this, 25, 1), 
      dir_crc_err_en(reg_name + ".dir_crc_err_en", *this, 24, 1), 
      stig_done_en(reg_name + ".stig_done_en", *this, 23, 1), 
      sdma_err_en(reg_name + ".sdma_err_en", *this, 22, 1), 
      sdma_trigg_en(reg_name + ".sdma_trigg_en", *this, 21, 1), 
      cmd_ignored_en(reg_name + ".cmd_ignored_en", *this, 20, 1), 
      reserved9(reg_name + ".reserved9", *this, 19, 1), 
      ddma_terr_en(reg_name + ".ddma_terr_en", *this, 18, 1), 
      cdma_terr_en(reg_name + ".cdma_terr_en", *this, 17, 1), 
      ctrl_idle_en(reg_name + ".ctrl_idle_en", *this, 16, 1), 
      gp_open_drain_3_en(reg_name + ".gp_open_drain_3_en", *this, 15, 1), 
      gp_open_drain_2_en(reg_name + ".gp_open_drain_2_en", *this, 14, 1), 
      gp_open_drain_1_en(reg_name + ".gp_open_drain_1_en", *this, 13, 1), 
      gp_open_drain_0_en(reg_name + ".gp_open_drain_0_en", *this, 12, 1), 
      reserved10(reg_name + ".reserved10", *this, 0, 12)
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
    csml_bitfield<N> intr_en;
    csml_bitfield<N> reserved8;
    csml_bitfield<N> dir_dev_err_en;
    csml_bitfield<N> dir_ecc_corr_err_en;
    csml_bitfield<N> dir_cmd_err_en;
    csml_bitfield<N> dir_dqs_err_en;
    csml_bitfield<N> dir_crc_err_en;
    csml_bitfield<N> stig_done_en;
    csml_bitfield<N> sdma_err_en;
    csml_bitfield<N> sdma_trigg_en;
    csml_bitfield<N> cmd_ignored_en;
    csml_bitfield<N> reserved9;
    csml_bitfield<N> ddma_terr_en;
    csml_bitfield<N> cdma_terr_en;
    csml_bitfield<N> ctrl_idle_en;
    csml_bitfield<N> gp_open_drain_3_en;
    csml_bitfield<N> gp_open_drain_2_en;
    csml_bitfield<N> gp_open_drain_1_en;
    csml_bitfield<N> gp_open_drain_0_en;
    csml_bitfield<N> reserved10;
};

template<unsigned int N>
class trd_comp_intr_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    trd_comp_intr_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xff, 0x00000000),
      reserved11(reg_name + ".reserved11", *this, 8, 24), 
      trd7_comp(reg_name + ".trd7_comp", *this, 7, 1), 
      trd6_comp(reg_name + ".trd6_comp", *this, 6, 1), 
      trd5_comp(reg_name + ".trd5_comp", *this, 5, 1), 
      trd4_comp(reg_name + ".trd4_comp", *this, 4, 1), 
      trd3_comp(reg_name + ".trd3_comp", *this, 3, 1), 
      trd2_comp(reg_name + ".trd2_comp", *this, 2, 1), 
      trd1_comp(reg_name + ".trd1_comp", *this, 1, 1), 
      trd0_comp(reg_name + ".trd0_comp", *this, 0, 1)
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
    csml_bitfield<N> reserved11;
    csml_bitfield<N> trd7_comp;
    csml_bitfield<N> trd6_comp;
    csml_bitfield<N> trd5_comp;
    csml_bitfield<N> trd4_comp;
    csml_bitfield<N> trd3_comp;
    csml_bitfield<N> trd2_comp;
    csml_bitfield<N> trd1_comp;
    csml_bitfield<N> trd0_comp;
};

template<unsigned int N>
class trd_error_intr_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    trd_error_intr_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xff, 0x00000000),
      reserved12(reg_name + ".reserved12", *this, 8, 24), 
      trd7_error_stat(reg_name + ".trd7_error_stat", *this, 7, 1), 
      trd6_error_stat(reg_name + ".trd6_error_stat", *this, 6, 1), 
      trd5_error_stat(reg_name + ".trd5_error_stat", *this, 5, 1), 
      trd4_error_stat(reg_name + ".trd4_error_stat", *this, 4, 1), 
      trd3_error_stat(reg_name + ".trd3_error_stat", *this, 3, 1), 
      trd2_error_stat(reg_name + ".trd2_error_stat", *this, 2, 1), 
      trd1_error_stat(reg_name + ".trd1_error_stat", *this, 1, 1), 
      trd0_error_stat(reg_name + ".trd0_error_stat", *this, 0, 1)
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
    csml_bitfield<N> reserved12;
    csml_bitfield<N> trd7_error_stat;
    csml_bitfield<N> trd6_error_stat;
    csml_bitfield<N> trd5_error_stat;
    csml_bitfield<N> trd4_error_stat;
    csml_bitfield<N> trd3_error_stat;
    csml_bitfield<N> trd2_error_stat;
    csml_bitfield<N> trd1_error_stat;
    csml_bitfield<N> trd0_error_stat;
};

template<unsigned int N>
class trd_error_intr_en_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    trd_error_intr_en_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xff, 0x00000000),
      reserved13(reg_name + ".reserved13", *this, 8, 24), 
      trd_error_intr_en_field(reg_name + ".trd_error_intr_en_field", *this, 0, 8)
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
    csml_bitfield<N> reserved13;
    csml_bitfield<N> trd_error_intr_en_field;
};

template<unsigned int N>
class dma_target_error_l_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dma_target_error_l_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      target_err_l(reg_name + ".target_err_l", *this, 0, 32)
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
    csml_bitfield<N> target_err_l;
};

template<unsigned int N>
class dma_target_error_h_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dma_target_error_h_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      target_err_h(reg_name + ".target_err_h", *this, 0, 32)
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
    csml_bitfield<N> target_err_h;
};

template<unsigned int N>
class boot_status_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    boot_status_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      reserved14(reg_name + ".reserved14", *this, 3, 29), 
      boot_bus_err(reg_name + ".boot_bus_err", *this, 2, 1), 
      boot_crc_err(reg_name + ".boot_crc_err", *this, 1, 1), 
      boot_dqs_err(reg_name + ".boot_dqs_err", *this, 0, 1)
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
    csml_bitfield<N> reserved14;
    csml_bitfield<N> boot_bus_err;
    csml_bitfield<N> boot_crc_err;
    csml_bitfield<N> boot_dqs_err;
};

template<unsigned int N>
class long_polling_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    long_polling_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffff, 0x000003E8),
      reserved15(reg_name + ".reserved15", *this, 16, 16), 
      long_polling_field(reg_name + ".long_polling_field", *this, 0, 16)
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
    csml_bitfield<N> reserved15;
    csml_bitfield<N> long_polling_field;
};

template<unsigned int N>
class short_polling_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    short_polling_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffff, 0x000001F4),
      reserved16(reg_name + ".reserved16", *this, 16, 16), 
      short_polling_field(reg_name + ".short_polling_field", *this, 0, 16)
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
    csml_bitfield<N> reserved16;
    csml_bitfield<N> short_polling_field;
};

template<unsigned int N>
class ctrl_config_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ctrl_config_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x68, 0x00000000),
      reserved17(reg_name + ".reserved17", *this, 7, 25), 
      work_mode(reg_name + ".work_mode", *this, 5, 2), 
      reserved18(reg_name + ".reserved18", *this, 4, 1), 
      cont_on_err(reg_name + ".cont_on_err", *this, 3, 1), 
      reserved19(reg_name + ".reserved19", *this, 0, 3)
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
    csml_bitfield<N> reserved17;
    csml_bitfield<N> work_mode;
    csml_bitfield<N> reserved18;
    csml_bitfield<N> cont_on_err;
    csml_bitfield<N> reserved19;
};

template<unsigned int N>
class dma_settings_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dma_settings_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xf00ff, 0x000D0000),
      reserved20(reg_name + ".reserved20", *this, 20, 12), 
      word_size(reg_name + ".word_size", *this, 18, 2), 
      sdma_err_rsp(reg_name + ".sdma_err_rsp", *this, 17, 1), 
      OTE(reg_name + ".OTE", *this, 16, 1), 
      reserved21(reg_name + ".reserved21", *this, 8, 8), 
      burst_sel(reg_name + ".burst_sel", *this, 0, 8)
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
    csml_bitfield<N> reserved20;
    csml_bitfield<N> word_size;
    csml_bitfield<N> sdma_err_rsp;
    csml_bitfield<N> OTE;
    csml_bitfield<N> reserved21;
    csml_bitfield<N> burst_sel;
};

template<unsigned int N>
class sdma_size_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    sdma_size_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      sdma_size_field(reg_name + ".sdma_size_field", *this, 0, 32)
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
    csml_bitfield<N> sdma_size_field;
};

template<unsigned int N>
class sdma_trd_info_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    sdma_trd_info_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      reserved22(reg_name + ".reserved22", *this, 9, 23), 
      sdma_dir(reg_name + ".sdma_dir", *this, 8, 1), 
      reserved23(reg_name + ".reserved23", *this, 3, 5), 
      sdma_trd(reg_name + ".sdma_trd", *this, 0, 3)
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
    csml_bitfield<N> reserved22;
    csml_bitfield<N> sdma_dir;
    csml_bitfield<N> reserved23;
    csml_bitfield<N> sdma_trd;
};

template<unsigned int N>
class sdma_addr0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    sdma_addr0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      sdma_addr_l(reg_name + ".sdma_addr_l", *this, 0, 32)
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
    csml_bitfield<N> sdma_addr_l;
};

template<unsigned int N>
class sdma_addr1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    sdma_addr1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      sdma_addr_h(reg_name + ".sdma_addr_h", *this, 0, 32)
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
    csml_bitfield<N> sdma_addr_h;
};

template<unsigned int N>
class discovery_control_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    discovery_control_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x7ffc3, 0x00000000),
      reserved24(reg_name + ".reserved24", *this, 19, 13), 
      discovery_bank(reg_name + ".discovery_bank", *this, 16, 3), 
      discovery_num_lines(reg_name + ".discovery_num_lines", *this, 12, 4), 
      discovery_abnum(reg_name + ".discovery_abnum", *this, 11, 1), 
      discovery_dummy_cnt(reg_name + ".discovery_dummy_cnt", *this, 10, 1), 
      discovery_cmd_type(reg_name + ".discovery_cmd_type", *this, 8, 2), 
      discovery_extop_en(reg_name + ".discovery_extop_en", *this, 7, 1), 
      discovery_extop_val(reg_name + ".discovery_extop_val", *this, 6, 1), 
      discovery_inhibit(reg_name + ".discovery_inhibit", *this, 5, 1), 
      discovery_fail(reg_name + ".discovery_fail", *this, 3, 2), 
      discovery_comp(reg_name + ".discovery_comp", *this, 2, 1), 
      discovery_req_type(reg_name + ".discovery_req_type", *this, 1, 1), 
      discovery_req(reg_name + ".discovery_req", *this, 0, 1)
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
    csml_bitfield<N> reserved24;
    csml_bitfield<N> discovery_bank;
    csml_bitfield<N> discovery_num_lines;
    csml_bitfield<N> discovery_abnum;
    csml_bitfield<N> discovery_dummy_cnt;
    csml_bitfield<N> discovery_cmd_type;
    csml_bitfield<N> discovery_extop_en;
    csml_bitfield<N> discovery_extop_val;
    csml_bitfield<N> discovery_inhibit;
    csml_bitfield<N> discovery_fail;
    csml_bitfield<N> discovery_comp;
    csml_bitfield<N> discovery_req_type;
    csml_bitfield<N> discovery_req;
};

template<unsigned int N>
class xip_mode_cfg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    xip_mode_cfg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffff, 0x00FF0000),
      reserved25(reg_name + ".reserved25", *this, 24, 8),
      xip_en(reg_name + ".xip_en", *this, 0, 8),
      xip_dis_mb_val(reg_name + ".xip_dis_mb_val", *this, 16, 8),
      xip_en_mb_val(reg_name + ".xip_en_mb_val", *this, 8, 8)
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
    csml_bitfield<N> reserved25;
    csml_bitfield<N> xip_en;
    csml_bitfield<N> xip_dis_mb_val;
    csml_bitfield<N> xip_en_mb_val;
};

template<unsigned int N>
class global_seq_cfg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    global_seq_cfg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0000208F),
      global_seq_cfg_field(reg_name + ".global_seq_cfg_field", *this, 0, 32)
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
    csml_bitfield<N> global_seq_cfg_field;
};

template<unsigned int N>
class global_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    global_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      global_seq_cfg_1_field(reg_name + ".global_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> global_seq_cfg_1_field;
};

template<unsigned int N>
class direct_access_cfg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    direct_access_cfg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      direct_access_cfg_field(reg_name + ".direct_access_cfg_field", *this, 0, 32)
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
    csml_bitfield<N> direct_access_cfg_field;
};

template<unsigned int N>
class direct_access_rmp_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    direct_access_rmp_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      remap_offset_l(reg_name + ".remap_offset_l", *this, 0, 32)
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
    csml_bitfield<N> remap_offset_l;
};

template<unsigned int N>
class direct_access_rmp_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    direct_access_rmp_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      remap_offset_h(reg_name + ".remap_offset_h", *this, 0, 32)
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
    csml_bitfield<N> remap_offset_h;
};

template<unsigned int N>
class rst_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    rst_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00019966),
      rst_seq_cfg_0_field(reg_name + ".rst_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> rst_seq_cfg_0_field;
};

template<unsigned int N>
class rst_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    rst_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0xD0669900),
      rst_seq_cfg_1_field(reg_name + ".rst_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> rst_seq_cfg_1_field;
};

template<unsigned int N>
class ers_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ers_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00DF3020),
      ers_seq_cfg_0_field(reg_name + ".ers_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> ers_seq_cfg_0_field;
};

template<unsigned int N>
class ers_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ers_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0000000C),
      ers_seq_cfg_1_field(reg_name + ".ers_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> ers_seq_cfg_1_field;
};

template<unsigned int N>
class ers_seq_cfg_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ers_seq_cfg_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x009F0060),
      ers_seq_cfg_2_field(reg_name + ".ers_seq_cfg_2_field", *this, 0, 32)
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
    csml_bitfield<N> ers_seq_cfg_2_field;
};

template<unsigned int N>
class prog_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    prog_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00003002),
      prog_seq_cfg_0_field(reg_name + ".prog_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> prog_seq_cfg_0_field;
};

template<unsigned int N>
class prog_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    prog_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0000FD00),
      prog_seq_cfg_1_field(reg_name + ".prog_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> prog_seq_cfg_1_field;
};

template<unsigned int N>
class prog_seq_cfg_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    prog_seq_cfg_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000002),
      prog_seq_cfg_2_field(reg_name + ".prog_seq_cfg_2_field", *this, 0, 32)
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
    csml_bitfield<N> prog_seq_cfg_2_field;
};

template<unsigned int N>
class read_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    read_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00003003),
      read_seq_cfg_0_field(reg_name + ".read_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> read_seq_cfg_0_field;
};

template<unsigned int N>
class read_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    read_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0000FC00),
      read_seq_cfg_1_field(reg_name + ".read_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> read_seq_cfg_1_field;
};

template<unsigned int N>
class read_seq_cfg_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    read_seq_cfg_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000F0A),
      read_seq_cfg_2_field(reg_name + ".read_seq_cfg_2_field", *this, 0, 32)
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
    csml_bitfield<N> read_seq_cfg_2_field;
};

template<unsigned int N>
class we_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    we_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x01F90006),
      we_seq_cfg_0_field(reg_name + ".we_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> we_seq_cfg_0_field;
};

template<unsigned int N>
class stat_seq_cfg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_0_field(reg_name + ".stat_seq_cfg_0_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_0_field;
};

template<unsigned int N>
class stat_seq_cfg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_1_field(reg_name + ".stat_seq_cfg_1_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_1_field;
};

template<unsigned int N>
class stat_seq_cfg_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x05000505),
      stat_seq_cfg_2_field(reg_name + ".stat_seq_cfg_2_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_2_field;
};

template<unsigned int N>
class stat_seq_cfg_3_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_3_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0xFA00FAFA),
      stat_seq_cfg_3_field(reg_name + ".stat_seq_cfg_3_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_3_field;
};

template<unsigned int N>
class stat_seq_cfg_4_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_4_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000F00),
      stat_seq_cfg_4_field(reg_name + ".stat_seq_cfg_4_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_4_field;
};

template<unsigned int N>
class stat_seq_cfg_5_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_5_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000040),
      stat_seq_cfg_5_field(reg_name + ".stat_seq_cfg_5_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_5_field;
};

template<unsigned int N>
class stat_seq_cfg_7_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_7_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_7_field(reg_name + ".stat_seq_cfg_7_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_7_field;
};

template<unsigned int N>
class stat_seq_cfg_8_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_8_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_8_field(reg_name + ".stat_seq_cfg_8_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_8_field;
};

template<unsigned int N>
class stat_seq_cfg_9_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_9_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_9_field(reg_name + ".stat_seq_cfg_9_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_9_field;
};

template<unsigned int N>
class stat_seq_cfg_10_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    stat_seq_cfg_10_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      stat_seq_cfg_10_field(reg_name + ".stat_seq_cfg_10_field", *this, 0, 32)
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
    csml_bitfield<N> stat_seq_cfg_10_field;
};

/** @brief 0xF00 RO version ID; layout matches cdns_xspi_ctrl_regBase: magic [31:16], fix [15:8], rev [7:0]; packed reset 0x65220206. */
template<unsigned int N>
class xspi_ctrl_version_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    xspi_ctrl_version_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x65220206),
      xspi_ctrl_magic_number(reg_name + ".xspi_ctrl_magic_number", *this, 16, 16), 
      xspi_ctrl_fix(reg_name + ".xspi_ctrl_fix", *this, 8, 8),
      xspi_ctrl_rev(reg_name + ".xspi_ctrl_rev", *this, 0, 8)
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
    csml_bitfield<N> xspi_ctrl_magic_number;
    csml_bitfield<N> xspi_ctrl_fix;
    csml_bitfield<N> xspi_ctrl_rev;
};

template<unsigned int N>
class ctrl_features_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ctrl_features_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x03710003),
      ctrl_features_field(reg_name + ".ctrl_features_field", *this, 0, 32)
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
    csml_bitfield<N> ctrl_features_field;
};

template<unsigned int N>
class wp_settings_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    wp_settings_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x3, 0x00000001),
      reserved26(reg_name + ".reserved26", *this, 2, 30), 
      wp_enable(reg_name + ".wp_enable", *this, 1, 1), 
      wp(reg_name + ".wp", *this, 0, 1)
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
    csml_bitfield<N> reserved26;
    csml_bitfield<N> wp_enable;
    csml_bitfield<N> wp;
};

template<unsigned int N>
class reset_pin_settings_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    reset_pin_settings_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000001),
      reset_pin_settings_field(reg_name + ".reset_pin_settings_field", *this, 0, 32)
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
    csml_bitfield<N> reset_pin_settings_field;
};

template<unsigned int N>
class clock_mode_settings_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    clock_mode_settings_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x1, 0x00000000),
      reserved27(reg_name + ".reserved27", *this, 1, 31), 
      spi_clock_mode(reg_name + ".spi_clock_mode", *this, 0, 1)
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
    csml_bitfield<N> reserved27;
    csml_bitfield<N> spi_clock_mode;
};

template<unsigned int N>
class jedec_rst_timing_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    jedec_rst_timing_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffff, 0x00008080),
      reserved28(reg_name + ".reserved28", *this, 16, 16), 
      tCSL_delay(reg_name + ".tCSL_delay", *this, 8, 8),
      tCSH_delay(reg_name + ".tCSH_delay", *this, 0, 8)
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
    csml_bitfield<N> reserved28;
    csml_bitfield<N> tCSL_delay;
    csml_bitfield<N> tCSH_delay;
};

template<unsigned int N>
class dev_delay_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dev_delay_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x01000100),
      dev_delay_reg_field(reg_name + ".dev_delay_reg_field", *this, 0, 32)
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
    csml_bitfield<N> dev_delay_reg_field;
};

template<unsigned int N>
class rst_recovery_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    rst_recovery_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xff, 0x0000000A),
      reserved29(reg_name + ".reserved29", *this, 8, 24), 
      rst_recovery(reg_name + ".rst_recovery", *this, 0, 8)
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
    csml_bitfield<N> reserved29;
    csml_bitfield<N> rst_recovery;
};

template<unsigned int N>
class dev_active_max_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dev_active_max_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xff, 0x00000080),
      reserved30(reg_name + ".reserved30", *this, 8, 24), 
      dev_active_max(reg_name + ".dev_active_max", *this, 0, 8)
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
    csml_bitfield<N> reserved30;
    csml_bitfield<N> dev_active_max;
};

template<unsigned int N>
class hf_offset_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    hf_offset_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffff, 0x00000D03),
      reserved31(reg_name + ".reserved31", *this, 16, 16), 
      hf_offset_size(reg_name + ".hf_offset_size", *this, 8, 8), 
      hf_offset_index(reg_name + ".hf_offset_index", *this, 0, 8)
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
    csml_bitfield<N> reserved31;
    csml_bitfield<N> hf_offset_size;
    csml_bitfield<N> hf_offset_index;
};

template<unsigned int N>
class dll_phy_update_cnt_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dll_phy_update_cnt_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      dll_phy_update_cnt_field(reg_name + ".dll_phy_update_cnt_field", *this, 0, 32)
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
    csml_bitfield<N> dll_phy_update_cnt_field;
};

template<unsigned int N>
class dll_phy_ctrl_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    dll_phy_ctrl_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x01030707),
      dll_phy_ctrl_field(reg_name + ".dll_phy_ctrl_field", *this, 0, 32)
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
    csml_bitfield<N> dll_phy_ctrl_field;
};

template<unsigned int N>
class phy_dq_timing_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dq_timing_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x80000002),
      phy_dq_timing_field(reg_name + ".phy_dq_timing_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dq_timing_field;
};

template<unsigned int N>
class phy_dqs_timing_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dqs_timing_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00180004),
      phy_dqs_timing_field(reg_name + ".phy_dqs_timing_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dqs_timing_field;
};

template<unsigned int N>
class phy_gate_lpbk_ctrl_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_gate_lpbk_ctrl_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x01A00000),
      phy_gate_lpbk_ctrl_field(reg_name + ".phy_gate_lpbk_ctrl_field", *this, 0, 32)
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
    csml_bitfield<N> phy_gate_lpbk_ctrl_field;
};

template<unsigned int N>
class phy_dll_master_ctrl_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dll_master_ctrl_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00800000),
      phy_dll_master_ctrl_field(reg_name + ".phy_dll_master_ctrl_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dll_master_ctrl_field;
};

template<unsigned int N>
class phy_dll_slave_ctrl_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dll_slave_ctrl_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_dll_slave_ctrl_field(reg_name + ".phy_dll_slave_ctrl_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dll_slave_ctrl_field;
};

template<unsigned int N>
class phy_ie_timing_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_ie_timing_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00100000),
      phy_ie_timing_field(reg_name + ".phy_ie_timing_field", *this, 0, 32)
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
    csml_bitfield<N> phy_ie_timing_field;
};

template<unsigned int N>
class phy_obs_reg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_obs_reg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_obs_reg_0_field(reg_name + ".phy_obs_reg_0_field", *this, 0, 32)
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
    csml_bitfield<N> phy_obs_reg_0_field;
};

template<unsigned int N>
class phy_dll_obs_reg_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dll_obs_reg_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_dll_obs_reg_0_field(reg_name + ".phy_dll_obs_reg_0_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dll_obs_reg_0_field;
};

template<unsigned int N>
class phy_dll_obs_reg_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dll_obs_reg_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_dll_obs_reg_1_field(reg_name + ".phy_dll_obs_reg_1_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dll_obs_reg_1_field;
};

template<unsigned int N>
class phy_dll_obs_reg_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_dll_obs_reg_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_dll_obs_reg_2_field(reg_name + ".phy_dll_obs_reg_2_field", *this, 0, 32)
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
    csml_bitfield<N> phy_dll_obs_reg_2_field;
};

template<unsigned int N>
class phy_static_togg_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_static_togg_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_static_togg_field(reg_name + ".phy_static_togg_field", *this, 0, 32)
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
    csml_bitfield<N> phy_static_togg_field;
};

template<unsigned int N>
class phy_wr_deskew_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_wr_deskew_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_wr_deskew_field(reg_name + ".phy_wr_deskew_field", *this, 0, 32)
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
    csml_bitfield<N> phy_wr_deskew_field;
};

template<unsigned int N>
class phy_wr_rd_deskew_cmd_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_wr_rd_deskew_cmd_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_wr_rd_deskew_cmd_field(reg_name + ".phy_wr_rd_deskew_cmd_field", *this, 0, 32)
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
    csml_bitfield<N> phy_wr_rd_deskew_cmd_field;
};

template<unsigned int N>
class phy_wr_deskew_pd_ctrl_0_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_wr_deskew_pd_ctrl_0_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_wr_deskew_pd_ctrl_0_field(reg_name + ".phy_wr_deskew_pd_ctrl_0_field", *this, 0, 32)
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
    csml_bitfield<N> phy_wr_deskew_pd_ctrl_0_field;
};

template<unsigned int N>
class phy_wr_deskew_pd_ctrl_1_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_wr_deskew_pd_ctrl_1_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_wr_deskew_pd_ctrl_1_field(reg_name + ".phy_wr_deskew_pd_ctrl_1_field", *this, 0, 32)
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
    csml_bitfield<N> phy_wr_deskew_pd_ctrl_1_field;
};

template<unsigned int N>
class phy_rd_deskew_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_rd_deskew_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_rd_deskew_field(reg_name + ".phy_rd_deskew_field", *this, 0, 32)
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
    csml_bitfield<N> phy_rd_deskew_field;
};

template<unsigned int N>
class phy_version_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_version_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x61820107),
      combo_phy_magic_number(reg_name + ".combo_phy_magic_number", *this, 16, 16), 
      phy_fix(reg_name + ".phy_fix", *this, 8, 8),
      phy_rev(reg_name + ".phy_rev", *this, 0, 8)
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
    csml_bitfield<N> combo_phy_magic_number;
    csml_bitfield<N> phy_fix;
    csml_bitfield<N> phy_rev;
};

template<unsigned int N>
class phy_features_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_features_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00001FFF),
      phy_features_field(reg_name + ".phy_features_field", *this, 0, 32)
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
    csml_bitfield<N> phy_features_field;
};

template<unsigned int N>
class phy_ctrl_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_ctrl_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00004310),
      phy_ctrl_field(reg_name + ".phy_ctrl_field", *this, 0, 32)
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
    csml_bitfield<N> phy_ctrl_field;
};

template<unsigned int N>
class phy_tsel_reg_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_tsel_reg_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_tsel_field(reg_name + ".phy_tsel_field", *this, 0, 32)
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
    csml_bitfield<N> phy_tsel_field;
};

template<unsigned int N>
class phy_gpio_ctrl_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_gpio_ctrl_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_gpio_ctrl_0_field(reg_name + ".phy_gpio_ctrl_0_field", *this, 0, 32)
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
    csml_bitfield<N> phy_gpio_ctrl_0_field;
};

template<unsigned int N>
class phy_gpio_ctrl_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_gpio_ctrl_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x00000000),
      phy_gpio_ctrl_1_field(reg_name + ".phy_gpio_ctrl_1_field", *this, 0, 32)
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
    csml_bitfield<N> phy_gpio_ctrl_1_field;
};

template<unsigned int N>
class phy_gpio_status_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_gpio_status_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_gpio_status_0_field(reg_name + ".phy_gpio_status_0_field", *this, 0, 32)
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
    csml_bitfield<N> phy_gpio_status_0_field;
};

template<unsigned int N>
class phy_gpio_status_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    phy_gpio_status_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x00000000),
      phy_gpio_status_1_field(reg_name + ".phy_gpio_status_1_field", *this, 0, 32)
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
    csml_bitfield<N> phy_gpio_status_1_field;
};


}