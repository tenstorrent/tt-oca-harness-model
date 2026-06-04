
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

namespace UART {

template<unsigned int N>
class RBR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RBR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0x0),
      DATA(reg_name + ".DATA", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DATA;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class THR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    THR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xff, 0x0),
      DATA(reg_name + ".DATA", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DATA;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class IER_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    IER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x0),
      ERBFI(reg_name + ".ERBFI", *this, 0, 1), 
      ETBEI(reg_name + ".ETBEI", *this, 1, 1), 
      ELSI(reg_name + ".ELSI", *this, 2, 1), 
      EDSSI(reg_name + ".EDSSI", *this, 3, 1), 
      EFEI(reg_name + ".EFEI", *this, 4, 1), 
      reserved0(reg_name + ".reserved0", *this, 5, 3), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> ERBFI;
    csml_bitfield<N> ETBEI;
    csml_bitfield<N> ELSI;
    csml_bitfield<N> EDSSI;
    csml_bitfield<N> EFEI;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class IIR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    IIR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0x01),
      INT_PENDING(reg_name + ".INT_PENDING", *this, 0, 1), 
      INT_ID(reg_name + ".INT_ID", *this, 1, 3), 
      reserved0(reg_name + ".reserved0", *this, 4, 2), 
      FIFO_STATUS(reg_name + ".FIFO_STATUS", *this, 6, 2), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> INT_PENDING;
    csml_bitfield<N> INT_ID;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> FIFO_STATUS;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class FCR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FCR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xff, 0x0),
      FIFO_Enable(reg_name + ".FIFO_Enable", *this, 0, 1), 
      RCVR_FIFO_RESET(reg_name + ".RCVR_FIFO_RESET", *this, 1, 1), 
      XMIT_FIFO_RESET(reg_name + ".XMIT_FIFO_RESET", *this, 2, 1), 
      DMA_Mode(reg_name + ".DMA_Mode", *this, 3, 1), 
      reserved0(reg_name + ".reserved0", *this, 4, 2), 
      RCVR_TRIGGER(reg_name + ".RCVR_TRIGGER", *this, 6, 2), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> FIFO_Enable;
    csml_bitfield<N> RCVR_FIFO_RESET;
    csml_bitfield<N> XMIT_FIFO_RESET;
    csml_bitfield<N> DMA_Mode;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> RCVR_TRIGGER;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class LCR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    LCR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x0),
      Word_Length(reg_name + ".Word_Length", *this, 0, 2), 
      Stop_Bit(reg_name + ".Stop_Bit", *this, 2, 1), 
      Parity_Enable(reg_name + ".Parity_Enable", *this, 3, 1), 
      Even_Parity(reg_name + ".Even_Parity", *this, 4, 1), 
      Stick_Parity(reg_name + ".Stick_Parity", *this, 5, 1), 
      Set_Break(reg_name + ".Set_Break", *this, 6, 1), 
      DLAB(reg_name + ".DLAB", *this, 7, 1), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> Word_Length;
    csml_bitfield<N> Stop_Bit;
    csml_bitfield<N> Parity_Enable;
    csml_bitfield<N> Even_Parity;
    csml_bitfield<N> Stick_Parity;
    csml_bitfield<N> Set_Break;
    csml_bitfield<N> DLAB;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class LSR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    LSR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0x60),
      DR(reg_name + ".DR", *this, 0, 1), 
      OE(reg_name + ".OE", *this, 1, 1), 
      PE(reg_name + ".PE", *this, 2, 1), 
      FE(reg_name + ".FE", *this, 3, 1), 
      BI(reg_name + ".BI", *this, 4, 1), 
      THRE(reg_name + ".THRE", *this, 5, 1), 
      TEMT(reg_name + ".TEMT", *this, 6, 1), 
      ERROR_IN_RCVR_FIFO(reg_name + ".ERROR_IN_RCVR_FIFO", *this, 7, 1), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DR;
    csml_bitfield<N> OE;
    csml_bitfield<N> PE;
    csml_bitfield<N> FE;
    csml_bitfield<N> BI;
    csml_bitfield<N> THRE;
    csml_bitfield<N> TEMT;
    csml_bitfield<N> ERROR_IN_RCVR_FIFO;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class DLL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DLL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x0),
      DLL_VAL(reg_name + ".DLL_VAL", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DLL_VAL;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class DLM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DLM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x0),
      DLM_VAL(reg_name + ".DLM_VAL", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DLM_VAL;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class MCR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MCR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x00),
      DTR(reg_name + ".DTR", *this, 0, 1), 
      RTS(reg_name + ".RTS", *this, 1, 1), 
      OUT1(reg_name + ".OUT1", *this, 2, 1), 
      OUT2(reg_name + ".OUT2", *this, 3, 1), 
      LOOP(reg_name + ".LOOP", *this, 4, 1), 
      LINE_LOOPBACK(reg_name + ".LINE_LOOPBACK", *this, 5, 1), 
      reserved0(reg_name + ".reserved0", *this, 6, 2), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> DTR;
    csml_bitfield<N> RTS;
    csml_bitfield<N> OUT1;
    csml_bitfield<N> OUT2;
    csml_bitfield<N> LOOP;
    csml_bitfield<N> LINE_LOOPBACK;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class MSR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MSR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0x00),
      DCTS(reg_name + ".DCTS", *this, 0, 1), 
      DDSR(reg_name + ".DDSR", *this, 1, 1), 
      TERI(reg_name + ".TERI", *this, 2, 1), 
      DDCD(reg_name + ".DDCD", *this, 3, 1), 
      CTS(reg_name + ".CTS", *this, 4, 1), 
      DSR(reg_name + ".DSR", *this, 5, 1), 
      RI(reg_name + ".RI", *this, 6, 1), 
      DCD(reg_name + ".DCD", *this, 7, 1), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> DCTS;
    csml_bitfield<N> DDSR;
    csml_bitfield<N> TERI;
    csml_bitfield<N> DDCD;
    csml_bitfield<N> CTS;
    csml_bitfield<N> DSR;
    csml_bitfield<N> RI;
    csml_bitfield<N> DCD;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class SCR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    SCR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x00),
      SCRATCH(reg_name + ".SCRATCH", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    csml_bitfield<N> SCRATCH;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class ECR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ECR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x00),
      RCVR_TRIGGER_MS2B(reg_name + ".RCVR_TRIGGER_MS2B", *this, 0, 2), 
      reserved0(reg_name + ".reserved0", *this, 2, 6), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> RCVR_TRIGGER_MS2B;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class ITR_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ITR_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0x00),
      TRBFI(reg_name + ".TRBFI", *this, 0, 1), 
      TTBEI(reg_name + ".TTBEI", *this, 1, 1), 
      TLSI(reg_name + ".TLSI", *this, 2, 1), 
      TDSSI(reg_name + ".TDSSI", *this, 3, 1), 
      TFEI(reg_name + ".TFEI", *this, 4, 1), 
      TRTI(reg_name + ".TRTI", *this, 5, 1), 
      reserved0(reg_name + ".reserved0", *this, 6, 2), 
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    csml_bitfield<N> TRBFI;
    csml_bitfield<N> TTBEI;
    csml_bitfield<N> TLSI;
    csml_bitfield<N> TDSSI;
    csml_bitfield<N> TFEI;
    csml_bitfield<N> TRTI;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> reserved1;
};


}