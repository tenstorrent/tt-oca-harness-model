
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace crng {

template<unsigned int N>
class INTR_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xf, 0xf, 0x0),
      cs_cmd_req_done(reg_name + ".cs_cmd_req_done", *this, 0, 1), 
      cs_entropy_req(reg_name + ".cs_entropy_req", *this, 1, 1), 
      cs_hw_inst_exc(reg_name + ".cs_hw_inst_exc", *this, 2, 1), 
      cs_fatal_err(reg_name + ".cs_fatal_err", *this, 3, 1), 
      reserved0(reg_name + ".reserved0", *this, 4, 28)
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
    csml_bitfield<N> cs_cmd_req_done;
    csml_bitfield<N> cs_entropy_req;
    csml_bitfield<N> cs_hw_inst_exc;
    csml_bitfield<N> cs_fatal_err;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xf, 0xf, 0x0),
      cs_cmd_req_done(reg_name + ".cs_cmd_req_done", *this, 0, 1), 
      cs_entropy_req(reg_name + ".cs_entropy_req", *this, 1, 1), 
      cs_hw_inst_exc(reg_name + ".cs_hw_inst_exc", *this, 2, 1), 
      cs_fatal_err(reg_name + ".cs_fatal_err", *this, 3, 1), 
      reserved1(reg_name + ".reserved1", *this, 4, 28)
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
    csml_bitfield<N> cs_cmd_req_done;
    csml_bitfield<N> cs_entropy_req;
    csml_bitfield<N> cs_hw_inst_exc;
    csml_bitfield<N> cs_fatal_err;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xf, 0x0),
      cs_cmd_req_done(reg_name + ".cs_cmd_req_done", *this, 0, 1), 
      cs_entropy_req(reg_name + ".cs_entropy_req", *this, 1, 1), 
      cs_hw_inst_exc(reg_name + ".cs_hw_inst_exc", *this, 2, 1), 
      cs_fatal_err(reg_name + ".cs_fatal_err", *this, 3, 1), 
      reserved2(reg_name + ".reserved2", *this, 4, 28)
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
    csml_bitfield<N> cs_cmd_req_done;
    csml_bitfield<N> cs_entropy_req;
    csml_bitfield<N> cs_hw_inst_exc;
    csml_bitfield<N> cs_fatal_err;
    csml_bitfield<N> reserved2;
};

template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x3, 0x0),
      recov_alert(reg_name + ".recov_alert", *this, 0, 1), 
      fatal_alert(reg_name + ".fatal_alert", *this, 1, 1), 
      reserved3(reg_name + ".reserved3", *this, 2, 30)
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
    csml_bitfield<N> recov_alert;
    csml_bitfield<N> fatal_alert;
    csml_bitfield<N> reserved3;
};

template<unsigned int N>
class REGWEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x1),
      REGWEN(reg_name + ".REGWEN", *this, 0, 1), 
      reserved4(reg_name + ".reserved4", *this, 1, 31)
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
    csml_bitfield<N> REGWEN;
    csml_bitfield<N> reserved4;
};

template<unsigned int N>
class CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffff, 0xffff, 0x9999),
      ENABLE(reg_name + ".ENABLE", *this, 0, 4),
      SW_APP_ENABLE(reg_name + ".SW_APP_ENABLE", *this, 4, 4),
      READ_INT_STATE(reg_name + ".READ_INT_STATE", *this, 8, 4),
      FIPS_FORCE_ENABLE(reg_name + ".FIPS_FORCE_ENABLE", *this, 12, 4),
      reserved5(reg_name + ".reserved5", *this, 16, 16)
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
    csml_bitfield<N> ENABLE;
    csml_bitfield<N> SW_APP_ENABLE;
    csml_bitfield<N> READ_INT_STATE;
    csml_bitfield<N> FIPS_FORCE_ENABLE;
    csml_bitfield<N> reserved5;
};

template<unsigned int N>
class CMD_REQ_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CMD_REQ_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      CMD_REQ(reg_name + ".CMD_REQ", *this, 0, 32)
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
    csml_bitfield<N> CMD_REQ;
};

template<unsigned int N>
class RESEED_INTERVAL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RESEED_INTERVAL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0xffffffff),
      RESEED_INTERVAL(reg_name + ".RESEED_INTERVAL", *this, 0, 32)
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
    csml_bitfield<N> RESEED_INTERVAL;
};

template<unsigned int N>
class RESEED_COUNTER_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RESEED_COUNTER_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      RESEED_COUNTER(reg_name + ".RESEED_COUNTER", *this, 0, 32)
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
    csml_bitfield<N> RESEED_COUNTER;
};

template<unsigned int N>
class RESEED_COUNTER_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RESEED_COUNTER_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      RESEED_COUNTER(reg_name + ".RESEED_COUNTER", *this, 0, 32)
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
    csml_bitfield<N> RESEED_COUNTER;
};

template<unsigned int N>
class RESEED_COUNTER_2_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RESEED_COUNTER_2_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      RESEED_COUNTER(reg_name + ".RESEED_COUNTER", *this, 0, 32)
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
    csml_bitfield<N> RESEED_COUNTER;
};

template<unsigned int N>
class SW_CMD_STS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    SW_CMD_STS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3e, 0x0, 0x0),
      reserved6(reg_name + ".reserved6", *this, 0, 1), 
      CMD_RDY(reg_name + ".CMD_RDY", *this, 1, 1), 
      CMD_ACK(reg_name + ".CMD_ACK", *this, 2, 1), 
      CMD_STS(reg_name + ".CMD_STS", *this, 3, 3),
      reserved7(reg_name + ".reserved7", *this, 6, 26)
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
    csml_bitfield<N> reserved6;
    csml_bitfield<N> CMD_RDY;
    csml_bitfield<N> CMD_ACK;
    csml_bitfield<N> CMD_STS;
    csml_bitfield<N> reserved7;
};

template<unsigned int N>
class GENBITS_VLD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENBITS_VLD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3, 0x0, 0x0),
      GENBITS_VLD(reg_name + ".GENBITS_VLD", *this, 0, 1), 
      GENBITS_FIPS(reg_name + ".GENBITS_FIPS", *this, 1, 1), 
      reserved8(reg_name + ".reserved8", *this, 2, 30)
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
    csml_bitfield<N> GENBITS_VLD;
    csml_bitfield<N> GENBITS_FIPS;
    csml_bitfield<N> reserved8;
};

template<unsigned int N>
class GENBITS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENBITS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      GENBITS(reg_name + ".GENBITS", *this, 0, 32)
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
    csml_bitfield<N> GENBITS;
};

template<unsigned int N>
class INT_STATE_READ_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INT_STATE_READ_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x7, 0x7),
      INT_STATE_READ_ENABLE(reg_name + ".INT_STATE_READ_ENABLE", *this, 0, 3),
      reserved9(reg_name + ".reserved9", *this, 3, 29)
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
    csml_bitfield<N> INT_STATE_READ_ENABLE;
    csml_bitfield<N> reserved9;
};

template<unsigned int N>
class INT_STATE_READ_ENABLE_REGWEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INT_STATE_READ_ENABLE_REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x1),
      INT_STATE_READ_ENABLE_REGWEN(reg_name + ".INT_STATE_READ_ENABLE_REGWEN", *this, 0, 1), 
      reserved10(reg_name + ".reserved10", *this, 1, 31)
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
    csml_bitfield<N> INT_STATE_READ_ENABLE_REGWEN;
    csml_bitfield<N> reserved10;
};

template<unsigned int N>
class INT_STATE_NUM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INT_STATE_NUM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xf, 0xf, 0x0),
      INT_STATE_NUM(reg_name + ".INT_STATE_NUM", *this, 0, 4),
      reserved11(reg_name + ".reserved11", *this, 4, 28)
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
    csml_bitfield<N> INT_STATE_NUM;
    csml_bitfield<N> reserved11;
};

template<unsigned int N>
class INT_STATE_VAL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INT_STATE_VAL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      INT_STATE_VAL(reg_name + ".INT_STATE_VAL", *this, 0, 32)
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
    csml_bitfield<N> INT_STATE_VAL;
};

template<unsigned int N>
class FIPS_FORCE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FIPS_FORCE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x7, 0x0),
      FIPS_FORCE(reg_name + ".FIPS_FORCE", *this, 0, 3),
      reserved12(reg_name + ".reserved12", *this, 3, 29)
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
    csml_bitfield<N> FIPS_FORCE;
    csml_bitfield<N> reserved12;
};

template<unsigned int N>
class HW_EXC_STS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HW_EXC_STS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffff, 0xffff, 0x0),
      HW_EXC_STS(reg_name + ".HW_EXC_STS", *this, 0, 16),
      reserved13(reg_name + ".reserved13", *this, 16, 16)
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
    csml_bitfield<N> HW_EXC_STS;
    csml_bitfield<N> reserved13;
};

template<unsigned int N>
class RECOV_ALERT_STS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RECOV_ALERT_STS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xf01f, 0xf01f, 0x0),
      ENABLE_FIELD_ALERT(reg_name + ".ENABLE_FIELD_ALERT", *this, 0, 1), 
      SW_APP_ENABLE_FIELD_ALERT(reg_name + ".SW_APP_ENABLE_FIELD_ALERT", *this, 1, 1), 
      READ_INT_STATE_FIELD_ALERT(reg_name + ".READ_INT_STATE_FIELD_ALERT", *this, 2, 1), 
      FIPS_FORCE_ENABLE_FIELD_ALERT(reg_name + ".FIPS_FORCE_ENABLE_FIELD_ALERT", *this, 3, 1), 
      ACMD_FLAG0_FIELD_ALERT(reg_name + ".ACMD_FLAG0_FIELD_ALERT", *this, 4, 1), 
      reserved14(reg_name + ".reserved14", *this, 5, 7),
      CS_BUS_CMP_ALERT(reg_name + ".CS_BUS_CMP_ALERT", *this, 12, 1), 
      CMD_STAGE_INVALID_ACMD_ALERT(reg_name + ".CMD_STAGE_INVALID_ACMD_ALERT", *this, 13, 1), 
      CMD_STAGE_INVALID_CMD_SEQ_ALERT(reg_name + ".CMD_STAGE_INVALID_CMD_SEQ_ALERT", *this, 14, 1), 
      CMD_STAGE_RESEED_CNT_ALERT(reg_name + ".CMD_STAGE_RESEED_CNT_ALERT", *this, 15, 1), 
      reserved15(reg_name + ".reserved15", *this, 16, 16)
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
    csml_bitfield<N> ENABLE_FIELD_ALERT;
    csml_bitfield<N> SW_APP_ENABLE_FIELD_ALERT;
    csml_bitfield<N> READ_INT_STATE_FIELD_ALERT;
    csml_bitfield<N> FIPS_FORCE_ENABLE_FIELD_ALERT;
    csml_bitfield<N> ACMD_FLAG0_FIELD_ALERT;
    csml_bitfield<N> reserved14;
    csml_bitfield<N> CS_BUS_CMP_ALERT;
    csml_bitfield<N> CMD_STAGE_INVALID_ACMD_ALERT;
    csml_bitfield<N> CMD_STAGE_INVALID_CMD_SEQ_ALERT;
    csml_bitfield<N> CMD_STAGE_RESEED_CNT_ALERT;
    csml_bitfield<N> reserved15;
};

template<unsigned int N>
class ERR_CODE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERR_CODE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x77f0ffff, 0x0, 0x0),
      SFIFO_CMD_ERR(reg_name + ".SFIFO_CMD_ERR", *this, 0, 1), 
      SFIFO_GENBITS_ERR(reg_name + ".SFIFO_GENBITS_ERR", *this, 1, 1), 
      SFIFO_CMDREQ_ERR(reg_name + ".SFIFO_CMDREQ_ERR", *this, 2, 1), 
      SFIFO_RCSTAGE_ERR(reg_name + ".SFIFO_RCSTAGE_ERR", *this, 3, 1), 
      SFIFO_KEYVRC_ERR(reg_name + ".SFIFO_KEYVRC_ERR", *this, 4, 1), 
      SFIFO_UPDREQ_ERR(reg_name + ".SFIFO_UPDREQ_ERR", *this, 5, 1), 
      SFIFO_BENCREQ_ERR(reg_name + ".SFIFO_BENCREQ_ERR", *this, 6, 1), 
      SFIFO_BENCACK_ERR(reg_name + ".SFIFO_BENCACK_ERR", *this, 7, 1), 
      SFIFO_PDATA_ERR(reg_name + ".SFIFO_PDATA_ERR", *this, 8, 1), 
      SFIFO_FINAL_ERR(reg_name + ".SFIFO_FINAL_ERR", *this, 9, 1), 
      SFIFO_GBENCACK_ERR(reg_name + ".SFIFO_GBENCACK_ERR", *this, 10, 1), 
      SFIFO_GRCSTAGE_ERR(reg_name + ".SFIFO_GRCSTAGE_ERR", *this, 11, 1), 
      SFIFO_GGENREQ_ERR(reg_name + ".SFIFO_GGENREQ_ERR", *this, 12, 1), 
      SFIFO_GADSTAGE_ERR(reg_name + ".SFIFO_GADSTAGE_ERR", *this, 13, 1), 
      SFIFO_GGENBITS_ERR(reg_name + ".SFIFO_GGENBITS_ERR", *this, 14, 1), 
      SFIFO_CMDID_ERR(reg_name + ".SFIFO_CMDID_ERR", *this, 15, 1), 
      reserved16(reg_name + ".reserved16", *this, 16, 4),
      CMD_STAGE_SM_ERR(reg_name + ".CMD_STAGE_SM_ERR", *this, 20, 1), 
      MAIN_SM_ERR(reg_name + ".MAIN_SM_ERR", *this, 21, 1), 
      DRBG_GEN_SM_ERR(reg_name + ".DRBG_GEN_SM_ERR", *this, 22, 1), 
      DRBG_UPDBE_SM_ERR(reg_name + ".DRBG_UPDBE_SM_ERR", *this, 23, 1), 
      DRBG_UPDOB_SM_ERR(reg_name + ".DRBG_UPDOB_SM_ERR", *this, 24, 1), 
      AES_CIPHER_SM_ERR(reg_name + ".AES_CIPHER_SM_ERR", *this, 25, 1), 
      CMD_GEN_CNT_ERR(reg_name + ".CMD_GEN_CNT_ERR", *this, 26, 1), 
      reserved17(reg_name + ".reserved17", *this, 27, 1), 
      FIFO_WRITE_ERR(reg_name + ".FIFO_WRITE_ERR", *this, 28, 1), 
      FIFO_READ_ERR(reg_name + ".FIFO_READ_ERR", *this, 29, 1), 
      FIFO_STATE_ERR(reg_name + ".FIFO_STATE_ERR", *this, 30, 1), 
      reserved18(reg_name + ".reserved18", *this, 31, 1)
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
    csml_bitfield<N> SFIFO_CMD_ERR;
    csml_bitfield<N> SFIFO_GENBITS_ERR;
    csml_bitfield<N> SFIFO_CMDREQ_ERR;
    csml_bitfield<N> SFIFO_RCSTAGE_ERR;
    csml_bitfield<N> SFIFO_KEYVRC_ERR;
    csml_bitfield<N> SFIFO_UPDREQ_ERR;
    csml_bitfield<N> SFIFO_BENCREQ_ERR;
    csml_bitfield<N> SFIFO_BENCACK_ERR;
    csml_bitfield<N> SFIFO_PDATA_ERR;
    csml_bitfield<N> SFIFO_FINAL_ERR;
    csml_bitfield<N> SFIFO_GBENCACK_ERR;
    csml_bitfield<N> SFIFO_GRCSTAGE_ERR;
    csml_bitfield<N> SFIFO_GGENREQ_ERR;
    csml_bitfield<N> SFIFO_GADSTAGE_ERR;
    csml_bitfield<N> SFIFO_GGENBITS_ERR;
    csml_bitfield<N> SFIFO_CMDID_ERR;
    csml_bitfield<N> reserved16;
    csml_bitfield<N> CMD_STAGE_SM_ERR;
    csml_bitfield<N> MAIN_SM_ERR;
    csml_bitfield<N> DRBG_GEN_SM_ERR;
    csml_bitfield<N> DRBG_UPDBE_SM_ERR;
    csml_bitfield<N> DRBG_UPDOB_SM_ERR;
    csml_bitfield<N> AES_CIPHER_SM_ERR;
    csml_bitfield<N> CMD_GEN_CNT_ERR;
    csml_bitfield<N> reserved17;
    csml_bitfield<N> FIFO_WRITE_ERR;
    csml_bitfield<N> FIFO_READ_ERR;
    csml_bitfield<N> FIFO_STATE_ERR;
    csml_bitfield<N> reserved18;
};

template<unsigned int N>
class ERR_CODE_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERR_CODE_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1f, 0x1f, 0x0),
      ERR_CODE_TEST(reg_name + ".ERR_CODE_TEST", *this, 0, 5),
      reserved19(reg_name + ".reserved19", *this, 5, 27)
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
    csml_bitfield<N> ERR_CODE_TEST;
    csml_bitfield<N> reserved19;
};

template<unsigned int N>
class MAIN_SM_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MAIN_SM_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0x4e),
      MAIN_SM_STATE(reg_name + ".MAIN_SM_STATE", *this, 0, 8),
      reserved20(reg_name + ".reserved20", *this, 8, 24)
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
    csml_bitfield<N> MAIN_SM_STATE;
    csml_bitfield<N> reserved20;
};


}