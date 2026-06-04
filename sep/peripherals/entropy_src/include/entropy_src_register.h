
/**
 * Entropy Source Register Type Definitions
 * Auto-generated from RDL specification
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace entropy_src {

template<unsigned int N>
class COMPONENT_ID_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    COMPONENT_ID_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x01000001),
      NAME(reg_name + ".NAME", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 8), 
      MINOR_VERSION(reg_name + ".MINOR_VERSION", *this, 24, 4), 
      MAJOR_VERSION(reg_name + ".MAJOR_VERSION", *this, 28, 4)
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
    csml_bitfield<N> NAME;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> MINOR_VERSION;
    csml_bitfield<N> MAJOR_VERSION;
};

template<unsigned int N>
class CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x13FF0111, 0x13FF0111, 0x10000000),
      RESET(reg_name + ".RESET", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      AUTOTUNE_ENABLE(reg_name + ".AUTOTUNE_ENABLE", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 3), 
      BYPASS_ENTROPY_COMPRESSOR(reg_name + ".BYPASS_ENTROPY_COMPRESSOR", *this, 8, 1), 
      reserved2(reg_name + ".reserved2", *this, 9, 7), 
      DOWNSAMPLE_RATE(reg_name + ".DOWNSAMPLE_RATE", *this, 16, 10), 
      reserved3(reg_name + ".reserved3", *this, 26, 2), 
      SHA256_WHITENING_ENABLE(reg_name + ".SHA256_WHITENING_ENABLE", *this, 28, 1), 
      reserved4(reg_name + ".reserved4", *this, 29, 3)
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
    csml_bitfield<N> RESET;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> AUTOTUNE_ENABLE;
    csml_bitfield<N> reserved1;
    csml_bitfield<N> BYPASS_ENTROPY_COMPRESSOR;
    csml_bitfield<N> reserved2;
    csml_bitfield<N> DOWNSAMPLE_RATE;
    csml_bitfield<N> reserved3;
    csml_bitfield<N> SHA256_WHITENING_ENABLE;
    csml_bitfield<N> reserved4;
};

template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00000001, 0x00000000, 0x00000000),
      RSVD(reg_name + ".RSVD", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 31)
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
    csml_bitfield<N> RSVD;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class SHA256_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    SHA256_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000007F1, 0x00000000, 0x00000000),
      BUSY(reg_name + ".BUSY", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      INPUT_COUNT(reg_name + ".INPUT_COUNT", *this, 4, 4), 
      OUTPUT_COUNT(reg_name + ".OUTPUT_COUNT", *this, 8, 3), 
      reserved1(reg_name + ".reserved1", *this, 11, 21)
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
    csml_bitfield<N> BUSY;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> INPUT_COUNT;
    csml_bitfield<N> OUTPUT_COUNT;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class DEBUG_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DEBUG_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000007FF, 0x000007FF, 0x00000000),
      SELECT_SIGNAL(reg_name + ".SELECT_SIGNAL", *this, 0, 8), 
      SELECT_FREQ_DIV(reg_name + ".SELECT_FREQ_DIV", *this, 8, 3), 
      reserved0(reg_name + ".reserved0", *this, 11, 21)
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
    csml_bitfield<N> SELECT_SIGNAL;
    csml_bitfield<N> SELECT_FREQ_DIV;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class INTR_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 3), 
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1), 
      reserved2(reg_name + ".reserved2", *this, 9, 3), 
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1), 
      reserved3(reg_name + ".reserved3", *this, 13, 19)
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
    csml_bitfield<N> HEALTH_TEST_FAILED;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> FIFO_ERROR;
    csml_bitfield<N> reserved1;
    csml_bitfield<N> FIFO_OVERFLOW;
    csml_bitfield<N> reserved2;
    csml_bitfield<N> FIFO_UNDERFLOW;
    csml_bitfield<N> reserved3;
};


template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00001111, 0x00001111, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 3), 
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1), 
      reserved2(reg_name + ".reserved2", *this, 9, 3), 
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1), 
      reserved3(reg_name + ".reserved3", *this, 13, 19)
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
    csml_bitfield<N> HEALTH_TEST_FAILED;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> FIFO_ERROR;
    csml_bitfield<N> reserved1;
    csml_bitfield<N> FIFO_OVERFLOW;
    csml_bitfield<N> reserved2;
    csml_bitfield<N> FIFO_UNDERFLOW;
    csml_bitfield<N> reserved3;
};

template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00000000, 0x00001111, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 3), 
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1), 
      reserved2(reg_name + ".reserved2", *this, 9, 3), 
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1), 
      reserved3(reg_name + ".reserved3", *this, 13, 19)
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
    csml_bitfield<N> HEALTH_TEST_FAILED;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> FIFO_ERROR;
    csml_bitfield<N> reserved1;
    csml_bitfield<N> FIFO_OVERFLOW;
    csml_bitfield<N> reserved2;
    csml_bitfield<N> FIFO_UNDERFLOW;
    csml_bitfield<N> reserved3;
};

template<unsigned int N>
class FIFO_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FIFO_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00000011, 0x00000011, 0x00000001),
      ENABLE(reg_name + ".ENABLE", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      ENTROPY_CHURN_ENABLE(reg_name + ".ENTROPY_CHURN_ENABLE", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 27)
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
    csml_bitfield<N> reserved0;
    csml_bitfield<N> ENTROPY_CHURN_ENABLE;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class FIFO_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FIFO_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x001F1F7F, 0x00000000, 0x00000000),
      LEVEL(reg_name + ".LEVEL", *this, 0, 7), 
      reserved0(reg_name + ".reserved0", *this, 7, 1), 
      WPTR(reg_name + ".WPTR", *this, 8, 5), 
      reserved1(reg_name + ".reserved1", *this, 13, 3), 
      RPTR(reg_name + ".RPTR", *this, 16, 5), 
      reserved2(reg_name + ".reserved2", *this, 21, 11)
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
    csml_bitfield<N> LEVEL;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> WPTR;
    csml_bitfield<N> reserved1;
    csml_bitfield<N> RPTR;
    csml_bitfield<N> reserved2;
};

template<unsigned int N>
class FIFO_RDATA_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FIFO_RDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      RDATA(reg_name + ".RDATA", *this, 0, 32)
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
    csml_bitfield<N> RDATA;
};

template<unsigned int N>
class HEALTH_TEST_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HEALTH_TEST_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00001907),
      ENABLE(reg_name + ".ENABLE", *this, 0, 8), 
      REPETITION_LIMIT(reg_name + ".REPETITION_LIMIT", *this, 8, 8), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> REPETITION_LIMIT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class HEALTH_TEST_WINDOW_SIZE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HEALTH_TEST_WINDOW_SIZE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000800),
      SIZE(reg_name + ".SIZE", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> SIZE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class MARKOV_TEST_PROB_THRESHOLDS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_TEST_PROB_THRESHOLDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0xFFFFFFFF, 0x006404B0),
      PROB_01_THRESHOLD(reg_name + ".PROB_01_THRESHOLD", *this, 0, 16), 
      PROB_10_THRESHOLD(reg_name + ".PROB_10_THRESHOLD", *this, 16, 16)
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
    csml_bitfield<N> PROB_01_THRESHOLD;
    csml_bitfield<N> PROB_10_THRESHOLD;
};

template<unsigned int N>
class HEALTH_TEST_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HEALTH_TEST_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      HEALTH_STATUS(reg_name + ".HEALTH_STATUS", *this, 0, 8), 
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
    csml_bitfield<N> HEALTH_STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class REPETITION_TEST_COUNT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REPETITION_TEST_COUNT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      REPETITION_COUNT(reg_name + ".REPETITION_COUNT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> REPETITION_COUNT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PATTERN_COUNT_1BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PATTERN_COUNT_1BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3FFFFFF1, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 16), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 16, 4), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved0(reg_name + ".reserved0", *this, 30, 2)
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
    csml_bitfield<N> PATTERN_COUNT;
    csml_bitfield<N> TARGET_PATTERN;
    csml_bitfield<N> SAMPLES_PROCESSED;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PATTERN_COUNT_2BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PATTERN_COUNT_2BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3FFFFFF1, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 16), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 16, 4), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved0(reg_name + ".reserved0", *this, 30, 2)
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
    csml_bitfield<N> PATTERN_COUNT;
    csml_bitfield<N> TARGET_PATTERN;
    csml_bitfield<N> SAMPLES_PROCESSED;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PATTERN_COUNT_3BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PATTERN_COUNT_3BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3FFFFFFF, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 10), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 10, 4), 
      reserved0(reg_name + ".reserved0", *this, 14, 6), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved1(reg_name + ".reserved1", *this, 30, 2)
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
    csml_bitfield<N> PATTERN_COUNT;
    csml_bitfield<N> TARGET_PATTERN;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> SAMPLES_PROCESSED;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class APT_PATTERN_COUNT_4BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PATTERN_COUNT_4BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3FFFFFFF, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 10), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 10, 4), 
      reserved0(reg_name + ".reserved0", *this, 14, 6), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved1(reg_name + ".reserved1", *this, 30, 2)
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
    csml_bitfield<N> PATTERN_COUNT;
    csml_bitfield<N> TARGET_PATTERN;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> SAMPLES_PROCESSED;
    csml_bitfield<N> reserved1;
};

template<unsigned int N>
class APT_PROPORTION_1BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PROPORTION_1BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x000004B0),
      LIMIT(reg_name + ".LIMIT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> LIMIT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PROPORTION_2BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PROPORTION_2BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000003FF, 0x000003FF, 0x00000080),
      LIMIT(reg_name + ".LIMIT", *this, 0, 10), 
      reserved0(reg_name + ".reserved0", *this, 10, 22)
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
    csml_bitfield<N> LIMIT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PROPORTION_3BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PROPORTION_3BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000003FF, 0x000003FF, 0x00000040),
      LIMIT(reg_name + ".LIMIT", *this, 0, 10), 
      reserved0(reg_name + ".reserved0", *this, 10, 22)
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
    csml_bitfield<N> LIMIT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PROPORTION_4BIT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_PROPORTION_4BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000003FF, 0x000003FF, 0x00000020),
      LIMIT(reg_name + ".LIMIT", *this, 0, 10), 
      reserved0(reg_name + ".reserved0", *this, 10, 22)
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
    csml_bitfield<N> LIMIT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class MARKOV_TEST_COUNTS_0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_TEST_COUNTS_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      COUNT_01(reg_name + ".COUNT_01", *this, 0, 16), 
      COUNT_10(reg_name + ".COUNT_10", *this, 16, 16)
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
    csml_bitfield<N> COUNT_01;
    csml_bitfield<N> COUNT_10;
};

template<unsigned int N>
class MARKOV_TEST_COUNTS_1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_TEST_COUNTS_1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      COUNT_00(reg_name + ".COUNT_00", *this, 0, 16), 
      COUNT_11(reg_name + ".COUNT_11", *this, 16, 16)
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
    csml_bitfield<N> COUNT_00;
    csml_bitfield<N> COUNT_11;
};

template<unsigned int N>
class MARKOV_TEST_PROBABILITIES_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_TEST_PROBABILITIES_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      PROB_01(reg_name + ".PROB_01", *this, 0, 8), 
      PROB_10(reg_name + ".PROB_10", *this, 8, 8), 
      PROB_00(reg_name + ".PROB_00", *this, 16, 8), 
      PROB_11(reg_name + ".PROB_11", *this, 24, 8)
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
    csml_bitfield<N> PROB_01;
    csml_bitfield<N> PROB_10;
    csml_bitfield<N> PROB_00;
    csml_bitfield<N> PROB_11;
};

template<unsigned int N>
class RING_OSC_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RING_OSC_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF),
      ENABLE(reg_name + ".ENABLE", *this, 0, 12), 
      SAMPLE_CLK_ENABLE(reg_name + ".SAMPLE_CLK_ENABLE", *this, 12, 12), 
      reserved0(reg_name + ".reserved0", *this, 24, 8)
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
    csml_bitfield<N> SAMPLE_CLK_ENABLE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class RING_OSC_TUNE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RING_OSC_TUNE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00FFFFFF, 0x00FFFFFF, 0x00000000),
      DETUNE(reg_name + ".DETUNE", *this, 0, 12), 
      SAMPLE_CLK_DETUNE(reg_name + ".SAMPLE_CLK_DETUNE", *this, 12, 12), 
      reserved0(reg_name + ".reserved0", *this, 24, 8)
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
    csml_bitfield<N> DETUNE;
    csml_bitfield<N> SAMPLE_CLK_DETUNE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class RING_OSC_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RING_OSC_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x00000FFF, 0x00000FFF, 0x00000FFF),
      SAMPLE_CLK_SELECT(reg_name + ".SAMPLE_CLK_SELECT", *this, 0, 12), 
      reserved0(reg_name + ".reserved0", *this, 12, 20)
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
    csml_bitfield<N> SAMPLE_CLK_SELECT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class DECORRELATOR_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DECORRELATOR_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0xFFFFFFFF, 0x0003F000),
      BYPASS(reg_name + ".BYPASS", *this, 0, 12), 
      SAMPLE_CLK_DIV(reg_name + ".SAMPLE_CLK_DIV", *this, 12, 20)
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
    csml_bitfield<N> BYPASS;
    csml_bitfield<N> SAMPLE_CLK_DIV;
};

template<unsigned int N>
class DECORRELATOR_MASK_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DECORRELATOR_MASK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x000000FF),
      ENTROPY_BYTE_MASK(reg_name + ".ENTROPY_BYTE_MASK", *this, 0, 8), 
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
    csml_bitfield<N> ENTROPY_BYTE_MASK;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class STARTUP_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STARTUP_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000000),
      DELAY_CYCLES(reg_name + ".DELAY_CYCLES", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> DELAY_CYCLES;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_0_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_0_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_1_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_1_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_2_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_2_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_3_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_3_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_4_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_4_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_5_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_5_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_6_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_6_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_7_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_7_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_8_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_8_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_9_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_9_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_10_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_10_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_11_HEALTH_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_11_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
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
    csml_bitfield<N> STATUS;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_0_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_0_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_1_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_1_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_2_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_2_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_3_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_3_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_4_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_4_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_5_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_5_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_6_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_6_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_7_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_7_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_8_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_8_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_9_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_9_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_10_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_10_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_11_SAMPLE_CLK_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    GENERATOR_11_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    csml_bitfield<N> SAMPLE_CLK_DIVIDE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class HT_WATERMARK_NUM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HT_WATERMARK_NUM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000000F, 0x0000000F, 0x00000000),
      WATERMARK_NUM(reg_name + ".WATERMARK_NUM", *this, 0, 4), 
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
    csml_bitfield<N> WATERMARK_NUM;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class HT_WATERMARK_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    HT_WATERMARK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      WATERMARK_VALUE(reg_name + ".WATERMARK_VALUE", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> WATERMARK_VALUE;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class REPCNT_TOTAL_FAILS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    REPCNT_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    csml_bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class APT_HI_TOTAL_FAILS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_HI_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    csml_bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class APT_LO_TOTAL_FAILS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    APT_LO_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    csml_bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class MARKOV_HI_TOTAL_FAILS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_HI_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    csml_bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class MARKOV_LO_TOTAL_FAILS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MARKOV_LO_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    csml_bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class ALERT_SUMMARY_FAIL_COUNTS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_SUMMARY_FAIL_COUNTS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      ANY_FAIL_COUNT(reg_name + ".ANY_FAIL_COUNT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    csml_bitfield<N> ANY_FAIL_COUNT;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class ALERT_FAIL_COUNTS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_FAIL_COUNTS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x000FFFFF, 0x00000000, 0x00000000),
      APT_LO_FAIL_COUNT(reg_name + ".APT_LO_FAIL_COUNT", *this, 0, 4), 
      APT_HI_FAIL_COUNT(reg_name + ".APT_HI_FAIL_COUNT", *this, 4, 4), 
      MARKOV_LO_FAIL_COUNT(reg_name + ".MARKOV_LO_FAIL_COUNT", *this, 8, 4), 
      MARKOV_HI_FAIL_COUNT(reg_name + ".MARKOV_HI_FAIL_COUNT", *this, 12, 4), 
      REPCNT_FAIL_COUNT(reg_name + ".REPCNT_FAIL_COUNT", *this, 16, 4), 
      reserved0(reg_name + ".reserved0", *this, 20, 12)
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
    csml_bitfield<N> APT_LO_FAIL_COUNT;
    csml_bitfield<N> APT_HI_FAIL_COUNT;
    csml_bitfield<N> MARKOV_LO_FAIL_COUNT;
    csml_bitfield<N> MARKOV_HI_FAIL_COUNT;
    csml_bitfield<N> REPCNT_FAIL_COUNT;
    csml_bitfield<N> reserved0;
};


}