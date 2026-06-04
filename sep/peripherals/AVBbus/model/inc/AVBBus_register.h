
/**
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml.h"

namespace AVBBus {

template<unsigned int N>
class AVS_FSM_RESET_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_FSM_RESET_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0),
      AVS_FSM_RESET_N(reg_name + ".AVS_FSM_RESET_N", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 31)
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
    csml_bitfield<N> AVS_FSM_RESET_N;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x111, 0),
      READ_NEXT(reg_name + ".READ_NEXT", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 3), 
      ABORT_CUR(reg_name + ".ABORT_CUR", *this, 4, 1), 
      Reserved1(reg_name + ".Reserved1", *this, 5, 3), 
      ABORT_FLUSH(reg_name + ".ABORT_FLUSH", *this, 8, 1), 
      Reserved2(reg_name + ".Reserved2", *this, 9, 23)
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
    csml_bitfield<N> READ_NEXT;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> ABORT_CUR;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> ABORT_FLUSH;
    csml_bitfield<N> Reserved2;
};

template<unsigned int N>
class AVS_CMD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_CMD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xfdffffff, 0),
      CMD_DATA(reg_name + ".CMD_DATA", *this, 0, 16), 
      SELECT(reg_name + ".SELECT", *this, 16, 4), 
      CMD_DATA_TYPE(reg_name + ".CMD_DATA_TYPE", *this, 20, 4), 
      CMD_GROUP(reg_name + ".CMD_GROUP", *this, 24, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 25, 1), 
      CMD(reg_name + ".CMD", *this, 26, 2), 
      TARGET_ADDR(reg_name + ".TARGET_ADDR", *this, 28, 4)
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
    csml_bitfield<N> CMD_DATA;
    csml_bitfield<N> SELECT;
    csml_bitfield<N> CMD_DATA_TYPE;
    csml_bitfield<N> CMD_GROUP;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> CMD;
    csml_bitfield<N> TARGET_ADDR;
};

template<unsigned int N>
class AVS_READBACK_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_READBACK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x31fffff, 0x0, 0),
      CMD_DATA(reg_name + ".CMD_DATA", *this, 0, 16), 
      STATUS_RESPONSE(reg_name + ".STATUS_RESPONSE", *this, 16, 5), 
      Reserved0(reg_name + ".Reserved0", *this, 21, 3), 
      TARGET_ACK(reg_name + ".TARGET_ACK", *this, 24, 2), 
      Reserved1(reg_name + ".Reserved1", *this, 26, 6)
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
    csml_bitfield<N> CMD_DATA;
    csml_bitfield<N> STATUS_RESPONSE;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> TARGET_ACK;
    csml_bitfield<N> Reserved1;
};

template<unsigned int N>
class AVS_TARGET_ACKS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_TARGET_ACKS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3fffffff, 0x0, 0),
      TARGET_0_ACK(reg_name + ".TARGET_0_ACK", *this, 0, 2), 
      TARGET_1_ACK(reg_name + ".TARGET_1_ACK", *this, 2, 2), 
      TARGET_2_ACK(reg_name + ".TARGET_2_ACK", *this, 4, 2), 
      TARGET_3_ACK(reg_name + ".TARGET_3_ACK", *this, 6, 2), 
      TARGET_4_ACK(reg_name + ".TARGET_4_ACK", *this, 8, 2), 
      TARGET_5_ACK(reg_name + ".TARGET_5_ACK", *this, 10, 2), 
      TARGET_6_ACK(reg_name + ".TARGET_6_ACK", *this, 12, 2), 
      TARGET_7_ACK(reg_name + ".TARGET_7_ACK", *this, 14, 2), 
      TARGET_8_ACK(reg_name + ".TARGET_8_ACK", *this, 16, 2), 
      TARGET_9_ACK(reg_name + ".TARGET_9_ACK", *this, 18, 2), 
      TARGET_10_ACK(reg_name + ".TARGET_10_ACK", *this, 20, 2), 
      TARGET_11_ACK(reg_name + ".TARGET_11_ACK", *this, 22, 2), 
      TARGET_12_ACK(reg_name + ".TARGET_12_ACK", *this, 24, 2), 
      TARGET_13_ACK(reg_name + ".TARGET_13_ACK", *this, 26, 2), 
      TARGET_14_ACK(reg_name + ".TARGET_14_ACK", *this, 28, 2), 
      Reserved0(reg_name + ".Reserved0", *this, 30, 2)
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
    csml_bitfield<N> TARGET_0_ACK;
    csml_bitfield<N> TARGET_1_ACK;
    csml_bitfield<N> TARGET_2_ACK;
    csml_bitfield<N> TARGET_3_ACK;
    csml_bitfield<N> TARGET_4_ACK;
    csml_bitfield<N> TARGET_5_ACK;
    csml_bitfield<N> TARGET_6_ACK;
    csml_bitfield<N> TARGET_7_ACK;
    csml_bitfield<N> TARGET_8_ACK;
    csml_bitfield<N> TARGET_9_ACK;
    csml_bitfield<N> TARGET_10_ACK;
    csml_bitfield<N> TARGET_11_ACK;
    csml_bitfield<N> TARGET_12_ACK;
    csml_bitfield<N> TARGET_13_ACK;
    csml_bitfield<N> TARGET_14_ACK;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_LATEST_TARGET_SUBFRAME_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_LATEST_TARGET_SUBFRAME_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x31fffff, 0x0, 0),
      CMD_DATA(reg_name + ".CMD_DATA", *this, 0, 16), 
      STATUS_RESPONSE(reg_name + ".STATUS_RESPONSE", *this, 16, 5), 
      Reserved0(reg_name + ".Reserved0", *this, 21, 3), 
      TARGET_ACK(reg_name + ".TARGET_ACK", *this, 24, 2), 
      Reserved1(reg_name + ".Reserved1", *this, 26, 6)
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
    csml_bitfield<N> CMD_DATA;
    csml_bitfield<N> STATUS_RESPONSE;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> TARGET_ACK;
    csml_bitfield<N> Reserved1;
};

template<unsigned int N>
class AVS_LATEST_TARGET_ACKS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_LATEST_TARGET_ACKS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3fffffff, 0x0, 0),
      TARGET_0_ACK(reg_name + ".TARGET_0_ACK", *this, 0, 2), 
      TARGET_1_ACK(reg_name + ".TARGET_1_ACK", *this, 2, 2), 
      TARGET_2_ACK(reg_name + ".TARGET_2_ACK", *this, 4, 2), 
      TARGET_3_ACK(reg_name + ".TARGET_3_ACK", *this, 6, 2), 
      TARGET_4_ACK(reg_name + ".TARGET_4_ACK", *this, 8, 2), 
      TARGET_5_ACK(reg_name + ".TARGET_5_ACK", *this, 10, 2), 
      TARGET_6_ACK(reg_name + ".TARGET_6_ACK", *this, 12, 2), 
      TARGET_7_ACK(reg_name + ".TARGET_7_ACK", *this, 14, 2), 
      TARGET_8_ACK(reg_name + ".TARGET_8_ACK", *this, 16, 2), 
      TARGET_9_ACK(reg_name + ".TARGET_9_ACK", *this, 18, 2), 
      TARGET_10_ACK(reg_name + ".TARGET_10_ACK", *this, 20, 2), 
      TARGET_11_ACK(reg_name + ".TARGET_11_ACK", *this, 22, 2), 
      TARGET_12_ACK(reg_name + ".TARGET_12_ACK", *this, 24, 2), 
      TARGET_13_ACK(reg_name + ".TARGET_13_ACK", *this, 26, 2), 
      TARGET_14_ACK(reg_name + ".TARGET_14_ACK", *this, 28, 2), 
      Reserved0(reg_name + ".Reserved0", *this, 30, 2)
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
    csml_bitfield<N> TARGET_0_ACK;
    csml_bitfield<N> TARGET_1_ACK;
    csml_bitfield<N> TARGET_2_ACK;
    csml_bitfield<N> TARGET_3_ACK;
    csml_bitfield<N> TARGET_4_ACK;
    csml_bitfield<N> TARGET_5_ACK;
    csml_bitfield<N> TARGET_6_ACK;
    csml_bitfield<N> TARGET_7_ACK;
    csml_bitfield<N> TARGET_8_ACK;
    csml_bitfield<N> TARGET_9_ACK;
    csml_bitfield<N> TARGET_10_ACK;
    csml_bitfield<N> TARGET_11_ACK;
    csml_bitfield<N> TARGET_12_ACK;
    csml_bitfield<N> TARGET_13_ACK;
    csml_bitfield<N> TARGET_14_ACK;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_NORMAL_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_NORMAL_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11111, 0x0, 0),
      CMD_FIFO_EMPTY(reg_name + ".CMD_FIFO_EMPTY", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 3), 
      CMD_FIFO_FULL(reg_name + ".CMD_FIFO_FULL", *this, 4, 1), 
      Reserved1(reg_name + ".Reserved1", *this, 5, 3), 
      READBACK_FIFO_FULL(reg_name + ".READBACK_FIFO_FULL", *this, 8, 1), 
      Reserved2(reg_name + ".Reserved2", *this, 9, 3), 
      READBACK_HAS_DATA(reg_name + ".READBACK_HAS_DATA", *this, 12, 1), 
      Reserved3(reg_name + ".Reserved3", *this, 13, 3), 
      AVS_BUS_IS_IDLE(reg_name + ".AVS_BUS_IS_IDLE", *this, 16, 1), 
      Reserved4(reg_name + ".Reserved4", *this, 17, 15)
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
    csml_bitfield<N> CMD_FIFO_EMPTY;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> CMD_FIFO_FULL;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> READBACK_FIFO_FULL;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> READBACK_HAS_DATA;
    csml_bitfield<N> Reserved3;
    csml_bitfield<N> AVS_BUS_IS_IDLE;
    csml_bitfield<N> Reserved4;
};

template<unsigned int N>
class AVS_CONTROLLER_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_CONTROLLER_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11, 0x0, 0),
      AVS_CONTROLLER_IS_RETRYING(reg_name + ".AVS_CONTROLLER_IS_RETRYING", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 3), 
      AVS_CONTROLLER_IS_ABORTING(reg_name + ".AVS_CONTROLLER_IS_ABORTING", *this, 4, 1), 
      Reserved1(reg_name + ".Reserved1", *this, 5, 27)
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
    csml_bitfield<N> AVS_CONTROLLER_IS_RETRYING;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> AVS_CONTROLLER_IS_ABORTING;
    csml_bitfield<N> Reserved1;
};

template<unsigned int N>
class AVS_TOTAL_RETRIES_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_TOTAL_RETRIES_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0),
      TOTAL_RETRIES(reg_name + ".TOTAL_RETRIES", *this, 0, 32)
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
    csml_bitfield<N> TOTAL_RETRIES;
};

template<unsigned int N>
class AVS_FIFOS_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_FIFOS_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xf0f0f0f, 0x0, 0),
      CMD_FIFO_OCCUPIED_SLOTS(reg_name + ".CMD_FIFO_OCCUPIED_SLOTS", *this, 0, 4), 
      Reserved0(reg_name + ".Reserved0", *this, 4, 4), 
      CMD_FIFO_VACANT_SLOTS(reg_name + ".CMD_FIFO_VACANT_SLOTS", *this, 8, 4), 
      Reserved1(reg_name + ".Reserved1", *this, 12, 4), 
      READBACK_FIFO_OCCUPIED_SLOTS(reg_name + ".READBACK_FIFO_OCCUPIED_SLOTS", *this, 16, 4), 
      Reserved2(reg_name + ".Reserved2", *this, 20, 4), 
      READBACK_FIFO_VACANT_SLOTS(reg_name + ".READBACK_FIFO_VACANT_SLOTS", *this, 24, 4), 
      Reserved3(reg_name + ".Reserved3", *this, 28, 4)
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
    csml_bitfield<N> CMD_FIFO_OCCUPIED_SLOTS;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> CMD_FIFO_VACANT_SLOTS;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> READBACK_FIFO_OCCUPIED_SLOTS;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> READBACK_FIFO_VACANT_SLOTS;
    csml_bitfield<N> Reserved3;
};

template<unsigned int N>
class AVS_INTERRUPT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_INTERRUPT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0),
      AVS_TARGET_ISSUED_INTERRUPT(reg_name + ".AVS_TARGET_ISSUED_INTERRUPT", *this, 0, 1), 
      CMD_FIFO_FULL_INT(reg_name + ".CMD_FIFO_FULL_INT", *this, 1, 1), 
      READBACK_FIFO_FULL_INT(reg_name + ".READBACK_FIFO_FULL_INT", *this, 2, 1), 
      READBACK_HAS_DATA_INT(reg_name + ".READBACK_HAS_DATA_INT", *this, 3, 1), 
      TARGET_UNRESPONSIVE_BAD_FRAME_INT(reg_name + ".TARGET_UNRESPONSIVE_BAD_FRAME_INT", *this, 4, 1), 
      MAX_RETRIES_ATTEMPTED_INT(reg_name + ".MAX_RETRIES_ATTEMPTED_INT", *this, 5, 1), 
      CMD_FIFO_OVERFLOW_INT(reg_name + ".CMD_FIFO_OVERFLOW_INT", *this, 6, 1), 
      TARGET_BAD_CRC_INT(reg_name + ".TARGET_BAD_CRC_INT", *this, 7, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 24)
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
    csml_bitfield<N> AVS_TARGET_ISSUED_INTERRUPT;
    csml_bitfield<N> CMD_FIFO_FULL_INT;
    csml_bitfield<N> READBACK_FIFO_FULL_INT;
    csml_bitfield<N> READBACK_HAS_DATA_INT;
    csml_bitfield<N> TARGET_UNRESPONSIVE_BAD_FRAME_INT;
    csml_bitfield<N> MAX_RETRIES_ATTEMPTED_INT;
    csml_bitfield<N> CMD_FIFO_OVERFLOW_INT;
    csml_bitfield<N> TARGET_BAD_CRC_INT;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_INTERRUPT_MASK_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_INTERRUPT_MASK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0),
      DISABLE_AVS_TARGET_ISSUED_INTERRUPT(reg_name + ".DISABLE_AVS_TARGET_ISSUED_INTERRUPT", *this, 0, 1), 
      DISABLE_CMD_FIFO_FULL_INT(reg_name + ".DISABLE_CMD_FIFO_FULL_INT", *this, 1, 1), 
      DISABLE_READBACK_FIFO_FULL_INT(reg_name + ".DISABLE_READBACK_FIFO_FULL_INT", *this, 2, 1), 
      DISABLE_READBACK_HAS_DATA_INT(reg_name + ".DISABLE_READBACK_HAS_DATA_INT", *this, 3, 1), 
      DISABLE_TARGET_UNRESPONSIVE_BAD_FRAME_INT(reg_name + ".DISABLE_TARGET_UNRESPONSIVE_BAD_FRAME_INT", *this, 4, 1), 
      DISABLE_MAX_RETRIES_ATTEMPTED_INT(reg_name + ".DISABLE_MAX_RETRIES_ATTEMPTED_INT", *this, 5, 1), 
      DISABLE_CMD_FIFO_OVERFLOW_INT(reg_name + ".DISABLE_CMD_FIFO_OVERFLOW_INT", *this, 6, 1), 
      DISABLE_TARGET_BAD_CRC_INT(reg_name + ".DISABLE_TARGET_BAD_CRC_INT", *this, 7, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 24)
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
    csml_bitfield<N> DISABLE_AVS_TARGET_ISSUED_INTERRUPT;
    csml_bitfield<N> DISABLE_CMD_FIFO_FULL_INT;
    csml_bitfield<N> DISABLE_READBACK_FIFO_FULL_INT;
    csml_bitfield<N> DISABLE_READBACK_HAS_DATA_INT;
    csml_bitfield<N> DISABLE_TARGET_UNRESPONSIVE_BAD_FRAME_INT;
    csml_bitfield<N> DISABLE_MAX_RETRIES_ATTEMPTED_INT;
    csml_bitfield<N> DISABLE_CMD_FIFO_OVERFLOW_INT;
    csml_bitfield<N> DISABLE_TARGET_BAD_CRC_INT;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_INTERRUPT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_INTERRUPT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0),
      TEST_AVS_TARGET_ISSUED_INTERRUPT(reg_name + ".TEST_AVS_TARGET_ISSUED_INTERRUPT", *this, 0, 1), 
      TEST_CMD_FIFO_FULL_INT(reg_name + ".TEST_CMD_FIFO_FULL_INT", *this, 1, 1), 
      TEST_READBACK_FIFO_FULL_INT(reg_name + ".TEST_READBACK_FIFO_FULL_INT", *this, 2, 1), 
      TEST_READBACK_HAS_DATA_INT(reg_name + ".TEST_READBACK_HAS_DATA_INT", *this, 3, 1), 
      TEST_TARGET_UNRESPONSIVE_BAD_FRAME_INT(reg_name + ".TEST_TARGET_UNRESPONSIVE_BAD_FRAME_INT", *this, 4, 1), 
      TEST_MAX_RETRIES_ATTEMPTED_INT(reg_name + ".TEST_MAX_RETRIES_ATTEMPTED_INT", *this, 5, 1), 
      TEST_CMD_FIFO_OVERFLOW_INT(reg_name + ".TEST_CMD_FIFO_OVERFLOW_INT", *this, 6, 1), 
      TEST_TARGET_BAD_CRC_INT(reg_name + ".TEST_TARGET_BAD_CRC_INT", *this, 7, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 24)
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
    csml_bitfield<N> TEST_AVS_TARGET_ISSUED_INTERRUPT;
    csml_bitfield<N> TEST_CMD_FIFO_FULL_INT;
    csml_bitfield<N> TEST_READBACK_FIFO_FULL_INT;
    csml_bitfield<N> TEST_READBACK_HAS_DATA_INT;
    csml_bitfield<N> TEST_TARGET_UNRESPONSIVE_BAD_FRAME_INT;
    csml_bitfield<N> TEST_MAX_RETRIES_ATTEMPTED_INT;
    csml_bitfield<N> TEST_CMD_FIFO_OVERFLOW_INT;
    csml_bitfield<N> TEST_TARGET_BAD_CRC_INT;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_TARGET_ISSUED_INTERRUPT_IDS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_TARGET_ISSUED_INTERRUPT_IDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7fff, 0x0, 0),
      TARGET_0(reg_name + ".TARGET_0", *this, 0, 1), 
      TARGET_1(reg_name + ".TARGET_1", *this, 1, 1), 
      TARGET_2(reg_name + ".TARGET_2", *this, 2, 1), 
      TARGET_3(reg_name + ".TARGET_3", *this, 3, 1), 
      TARGET_4(reg_name + ".TARGET_4", *this, 4, 1), 
      TARGET_5(reg_name + ".TARGET_5", *this, 5, 1), 
      TARGET_6(reg_name + ".TARGET_6", *this, 6, 1), 
      TARGET_7(reg_name + ".TARGET_7", *this, 7, 1), 
      TARGET_8(reg_name + ".TARGET_8", *this, 8, 1), 
      TARGET_9(reg_name + ".TARGET_9", *this, 9, 1), 
      TARGET_10(reg_name + ".TARGET_10", *this, 10, 1), 
      TARGET_11(reg_name + ".TARGET_11", *this, 11, 1), 
      TARGET_12(reg_name + ".TARGET_12", *this, 12, 1), 
      TARGET_13(reg_name + ".TARGET_13", *this, 13, 1), 
      TARGET_14(reg_name + ".TARGET_14", *this, 14, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 15, 17)
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
    csml_bitfield<N> TARGET_0;
    csml_bitfield<N> TARGET_1;
    csml_bitfield<N> TARGET_2;
    csml_bitfield<N> TARGET_3;
    csml_bitfield<N> TARGET_4;
    csml_bitfield<N> TARGET_5;
    csml_bitfield<N> TARGET_6;
    csml_bitfield<N> TARGET_7;
    csml_bitfield<N> TARGET_8;
    csml_bitfield<N> TARGET_9;
    csml_bitfield<N> TARGET_10;
    csml_bitfield<N> TARGET_11;
    csml_bitfield<N> TARGET_12;
    csml_bitfield<N> TARGET_13;
    csml_bitfield<N> TARGET_14;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7fff, 0x0, 0),
      TARGET_0(reg_name + ".TARGET_0", *this, 0, 1), 
      TARGET_1(reg_name + ".TARGET_1", *this, 1, 1), 
      TARGET_2(reg_name + ".TARGET_2", *this, 2, 1), 
      TARGET_3(reg_name + ".TARGET_3", *this, 3, 1), 
      TARGET_4(reg_name + ".TARGET_4", *this, 4, 1), 
      TARGET_5(reg_name + ".TARGET_5", *this, 5, 1), 
      TARGET_6(reg_name + ".TARGET_6", *this, 6, 1), 
      TARGET_7(reg_name + ".TARGET_7", *this, 7, 1), 
      TARGET_8(reg_name + ".TARGET_8", *this, 8, 1), 
      TARGET_9(reg_name + ".TARGET_9", *this, 9, 1), 
      TARGET_10(reg_name + ".TARGET_10", *this, 10, 1), 
      TARGET_11(reg_name + ".TARGET_11", *this, 11, 1), 
      TARGET_12(reg_name + ".TARGET_12", *this, 12, 1), 
      TARGET_13(reg_name + ".TARGET_13", *this, 13, 1), 
      TARGET_14(reg_name + ".TARGET_14", *this, 14, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 15, 17)
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
    csml_bitfield<N> TARGET_0;
    csml_bitfield<N> TARGET_1;
    csml_bitfield<N> TARGET_2;
    csml_bitfield<N> TARGET_3;
    csml_bitfield<N> TARGET_4;
    csml_bitfield<N> TARGET_5;
    csml_bitfield<N> TARGET_6;
    csml_bitfield<N> TARGET_7;
    csml_bitfield<N> TARGET_8;
    csml_bitfield<N> TARGET_9;
    csml_bitfield<N> TARGET_10;
    csml_bitfield<N> TARGET_11;
    csml_bitfield<N> TARGET_12;
    csml_bitfield<N> TARGET_13;
    csml_bitfield<N> TARGET_14;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_TARGET_BAD_CRC_INT_IDS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_TARGET_BAD_CRC_INT_IDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7fff, 0x0, 0),
      TARGET_0(reg_name + ".TARGET_0", *this, 0, 1), 
      TARGET_1(reg_name + ".TARGET_1", *this, 1, 1), 
      TARGET_2(reg_name + ".TARGET_2", *this, 2, 1), 
      TARGET_3(reg_name + ".TARGET_3", *this, 3, 1), 
      TARGET_4(reg_name + ".TARGET_4", *this, 4, 1), 
      TARGET_5(reg_name + ".TARGET_5", *this, 5, 1), 
      TARGET_6(reg_name + ".TARGET_6", *this, 6, 1), 
      TARGET_7(reg_name + ".TARGET_7", *this, 7, 1), 
      TARGET_8(reg_name + ".TARGET_8", *this, 8, 1), 
      TARGET_9(reg_name + ".TARGET_9", *this, 9, 1), 
      TARGET_10(reg_name + ".TARGET_10", *this, 10, 1), 
      TARGET_11(reg_name + ".TARGET_11", *this, 11, 1), 
      TARGET_12(reg_name + ".TARGET_12", *this, 12, 1), 
      TARGET_13(reg_name + ".TARGET_13", *this, 13, 1), 
      TARGET_14(reg_name + ".TARGET_14", *this, 14, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 15, 17)
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
    csml_bitfield<N> TARGET_0;
    csml_bitfield<N> TARGET_1;
    csml_bitfield<N> TARGET_2;
    csml_bitfield<N> TARGET_3;
    csml_bitfield<N> TARGET_4;
    csml_bitfield<N> TARGET_5;
    csml_bitfield<N> TARGET_6;
    csml_bitfield<N> TARGET_7;
    csml_bitfield<N> TARGET_8;
    csml_bitfield<N> TARGET_9;
    csml_bitfield<N> TARGET_10;
    csml_bitfield<N> TARGET_11;
    csml_bitfield<N> TARGET_12;
    csml_bitfield<N> TARGET_13;
    csml_bitfield<N> TARGET_14;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_RETRY_CFG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_RETRY_CFG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffff, 0xffff, 0),
      MAX_RETRIES(reg_name + ".MAX_RETRIES", *this, 0, 16), 
      Reserved0(reg_name + ".Reserved0", *this, 16, 16)
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
    csml_bitfield<N> MAX_RETRIES;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_CLK_CFG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_CLK_CFG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffff1103, 0xffff1103, 2147745795),
      AVS_CLOCK_SELECT(reg_name + ".AVS_CLOCK_SELECT", *this, 0, 2), 
      Reserved0(reg_name + ".Reserved0", *this, 2, 6), 
      STOP_AVS_CLOCK_ON_IDLE(reg_name + ".STOP_AVS_CLOCK_ON_IDLE", *this, 8, 1), 
      Reserved1(reg_name + ".Reserved1", *this, 9, 3), 
      TURN_OFF_ALL_PREMUX_CLOCKS(reg_name + ".TURN_OFF_ALL_PREMUX_CLOCKS", *this, 12, 1), 
      Reserved2(reg_name + ".Reserved2", *this, 13, 3), 
      CLK_DIVIDER_VALUE(reg_name + ".CLK_DIVIDER_VALUE", *this, 16, 8), 
      CLK_DIVIDER_DUTY_CYCLE_NUMERATOR(reg_name + ".CLK_DIVIDER_DUTY_CYCLE_NUMERATOR", *this, 24, 8)
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
    csml_bitfield<N> AVS_CLOCK_SELECT;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> STOP_AVS_CLOCK_ON_IDLE;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> TURN_OFF_ALL_PREMUX_CLOCKS;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> CLK_DIVIDER_VALUE;
    csml_bitfield<N> CLK_DIVIDER_DUTY_CYCLE_NUMERATOR;
};

template<unsigned int N>
class AVS_THROTTLE_CFG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_THROTTLE_CFG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0xff, 0),
      AVS_THROTTLE_CYCLES(reg_name + ".AVS_THROTTLE_CYCLES", *this, 0, 8), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 24)
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
    csml_bitfield<N> AVS_THROTTLE_CYCLES;
    csml_bitfield<N> Reserved0;
};

template<unsigned int N>
class AVS_CONFIG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    AVS_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 1),
      AVS_GPIO_ENABLE(reg_name + ".AVS_GPIO_ENABLE", *this, 0, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 1, 31)
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
    csml_bitfield<N> AVS_GPIO_ENABLE;
    csml_bitfield<N> Reserved0;
};


}