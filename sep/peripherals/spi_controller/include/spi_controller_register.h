// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_controller_register.h
 * @brief SPI Controller hardware register definitions
 * 
 * Register map follows upstream OpenTitan spi_host (hw/ip/spi_host):
 * - INTR_STATE - Interrupt status (error[0], spi_event[1], packed)
 * - INTR_ENABLE / INTR_TEST
 * - ALERT_TEST - fatal_fault (WO; bus-integrity alert is not modelled)
 * - CONTROL - SPIEN, SW_RST (level, not singlepulse), OUTPUT_EN, watermarks
 * - STATUS - READY, ACTIVE, FIFO depths
 * - CONFIGOPTS - per-device timing, polarity, phase
 * - CSID / COMMAND (CSAAT[0] SPEED[2:1] DIRECTION[4:3] LEN[24:5])
 * - RXDATA / TXDATA windows
 * - ERROR_ENABLE / ERROR_STATUS / EVENT_ENABLE (contiguous bit packing)
 * 
 * All registers are templated on bit width N (typically 32-bit).
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "reg_file.h"

// macOS <math.h> (pulled in transitively by SystemC) defines OVERFLOW and
// UNDERFLOW as preprocessor macros (#define OVERFLOW 3, #define UNDERFLOW 4),
// which collide with the register bitfield members named OVERFLOW/UNDERFLOW
// below.  Undefine them so the member names are treated as plain identifiers.
#ifdef OVERFLOW
#undef OVERFLOW
#endif
#ifdef UNDERFLOW
#undef UNDERFLOW
#endif

/**
 * @namespace spi_controller
 * @brief SPI Controller register type namespace
 */
namespace spi_controller {

/**
 * @class INTR_STATE_type
 * @brief Interrupt Status Register (rw1c for error; spi_event is status)
 *
 * Upstream spi_host packs the two interrupt bits contiguously:
 * - Bit 1: spi_event
 * - Bit 0: error
 */
template<unsigned int N>
class INTR_STATE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3, 0x3, 0),
      Reserved0(reg_name + ".Reserved0", *this, 2, 30),
      spi_event(reg_name + ".spi_event", *this, 1, 1),
      error(reg_name + ".error", *this, 0, 1)
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
    regmodel::Bitfield<N> spi_event;
    regmodel::Bitfield<N> error;
};

template<unsigned int N>
class INTR_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3, 0x3, 0),
      Reserved0(reg_name + ".Reserved0", *this, 2, 30),
      spi_event(reg_name + ".spi_event", *this, 1, 1),
      error(reg_name + ".error", *this, 0, 1)
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
    regmodel::Bitfield<N> spi_event;
    regmodel::Bitfield<N> error;
};

template<unsigned int N>
class INTR_TEST_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3, 0x3, 0),
      Reserved0(reg_name + ".Reserved0", *this, 2, 30),
      spi_event(reg_name + ".spi_event", *this, 1, 1),
      error(reg_name + ".error", *this, 0, 1)
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
    regmodel::Bitfield<N> spi_event;
    regmodel::Bitfield<N> error;
};

template<unsigned int N>
class ALERT_TEST_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x1, 0),
      Reserved0(reg_name + ".Reserved0", *this, 1, 31),
      FATAL_FAULT(reg_name + ".FATAL_FAULT", *this, 0, 1)
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
    regmodel::Bitfield<N> FATAL_FAULT;
};

template<unsigned int N>
class CONTROL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    CONTROL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xe000ffff, 0xe000ffff, 127),
      SPIEN(reg_name + ".SPIEN", *this, 31, 1), 
      SW_RST(reg_name + ".SW_RST", *this, 30, 1), 
      OUTPUT_EN(reg_name + ".OUTPUT_EN", *this, 29, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 16, 13), 
      TX_WATERMARK(reg_name + ".TX_WATERMARK", *this, 8, 8), 
      RX_WATERMARK(reg_name + ".RX_WATERMARK", *this, 0, 8)
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
    regmodel::Bitfield<N> SPIEN;
    regmodel::Bitfield<N> SW_RST;
    regmodel::Bitfield<N> OUTPUT_EN;
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> TX_WATERMARK;
    regmodel::Bitfield<N> RX_WATERMARK;
};

template<unsigned int N>
class STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffdfffff, 0x0, 0),
      READY(reg_name + ".READY", *this, 31, 1), 
      ACTIVE(reg_name + ".ACTIVE", *this, 30, 1), 
      TXFULL(reg_name + ".TXFULL", *this, 29, 1), 
      TXEMPTY(reg_name + ".TXEMPTY", *this, 28, 1), 
      TXSTALL(reg_name + ".TXSTALL", *this, 27, 1), 
      TXWM(reg_name + ".TXWM", *this, 26, 1), 
      RXFULL(reg_name + ".RXFULL", *this, 25, 1), 
      RXEMPTY(reg_name + ".RXEMPTY", *this, 24, 1), 
      RXSTALL(reg_name + ".RXSTALL", *this, 23, 1), 
      BYTEORDER(reg_name + ".BYTEORDER", *this, 22, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 21, 1), 
      RXWM(reg_name + ".RXWM", *this, 20, 1), 
      CMDQD(reg_name + ".CMDQD", *this, 16, 4), 
      RXQD(reg_name + ".RXQD", *this, 8, 8), 
      TXQD(reg_name + ".TXQD", *this, 0, 8)
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
    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> ACTIVE;
    regmodel::Bitfield<N> TXFULL;
    regmodel::Bitfield<N> TXEMPTY;
    regmodel::Bitfield<N> TXSTALL;
    regmodel::Bitfield<N> TXWM;
    regmodel::Bitfield<N> RXFULL;
    regmodel::Bitfield<N> RXEMPTY;
    regmodel::Bitfield<N> RXSTALL;
    regmodel::Bitfield<N> BYTEORDER;
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> RXWM;
    regmodel::Bitfield<N> CMDQD;
    regmodel::Bitfield<N> RXQD;
    regmodel::Bitfield<N> TXQD;
};

template<unsigned int N>
class CONFIGOPTS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    CONFIGOPTS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xefffffff, 0xefffffff, 0),
      CPOL(reg_name + ".CPOL", *this, 31, 1), 
      CPHA(reg_name + ".CPHA", *this, 30, 1), 
      FULLCYC(reg_name + ".FULLCYC", *this, 29, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 28, 1), 
      CSNLEAD(reg_name + ".CSNLEAD", *this, 24, 4), 
      CSNTRAIL(reg_name + ".CSNTRAIL", *this, 20, 4), 
      CSNIDLE(reg_name + ".CSNIDLE", *this, 16, 4), 
      CLKDIV(reg_name + ".CLKDIV", *this, 0, 16)
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
    regmodel::Bitfield<N> CPOL;
    regmodel::Bitfield<N> CPHA;
    regmodel::Bitfield<N> FULLCYC;
    regmodel::Bitfield<N> Reserved0;
    regmodel::Bitfield<N> CSNLEAD;
    regmodel::Bitfield<N> CSNTRAIL;
    regmodel::Bitfield<N> CSNIDLE;
    regmodel::Bitfield<N> CLKDIV;
};

template<unsigned int N>
class CSID_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    CSID_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      CSID(reg_name + ".CSID", *this, 0, 32)
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
    regmodel::Bitfield<N> CSID;
};

template<unsigned int N>
class COMMAND_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    COMMAND_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0x1ffffff, 0),
      Reserved0(reg_name + ".Reserved0", *this, 25, 7),
      DIRECTION(reg_name + ".DIRECTION", *this, 3, 2),
      SPEED(reg_name + ".SPEED", *this, 1, 2),
      CSAAT(reg_name + ".CSAAT", *this, 0, 1),
      LEN(reg_name + ".LEN", *this, 5, 20)
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
    regmodel::Bitfield<N> DIRECTION;
    regmodel::Bitfield<N> SPEED;
    regmodel::Bitfield<N> CSAAT;
    regmodel::Bitfield<N> LEN;
};

template<unsigned int N>
class RXDATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RXDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0),
      RXDATA(reg_name + ".RXDATA", *this, 0, 32)
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
    regmodel::Bitfield<N> RXDATA;
};

template<unsigned int N>
class TXDATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    TXDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0),
      TXDATA(reg_name + ".TXDATA", *this, 0, 32)
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
    regmodel::Bitfield<N> TXDATA;
};

template<unsigned int N>
class ERROR_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ERROR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x1f, 0x1f, 0x1f),
      Reserved0(reg_name + ".Reserved0", *this, 5, 27),
      CSIDINVAL(reg_name + ".CSIDINVAL", *this, 4, 1),
      CMDINVAL(reg_name + ".CMDINVAL", *this, 3, 1),
      underflow(reg_name + ".underflow", *this, 2, 1),
      overflow(reg_name + ".overflow", *this, 1, 1),
      CMDBUSY(reg_name + ".CMDBUSY", *this, 0, 1)
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
    regmodel::Bitfield<N> CSIDINVAL;
    regmodel::Bitfield<N> CMDINVAL;
    regmodel::Bitfield<N> underflow;
    regmodel::Bitfield<N> overflow;
    regmodel::Bitfield<N> CMDBUSY;
};

template<unsigned int N>
class ERROR_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ERROR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3f, 0x3f, 0),
      Reserved0(reg_name + ".Reserved0", *this, 6, 26),
      ACCESSINVAL(reg_name + ".ACCESSINVAL", *this, 5, 1),
      CSIDINVAL(reg_name + ".CSIDINVAL", *this, 4, 1),
      CMDINVAL(reg_name + ".CMDINVAL", *this, 3, 1),
      underflow(reg_name + ".underflow", *this, 2, 1),
      overflow(reg_name + ".overflow", *this, 1, 1),
      CMDBUSY(reg_name + ".CMDBUSY", *this, 0, 1)
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
    regmodel::Bitfield<N> ACCESSINVAL;
    regmodel::Bitfield<N> CSIDINVAL;
    regmodel::Bitfield<N> CMDINVAL;
    regmodel::Bitfield<N> underflow;
    regmodel::Bitfield<N> overflow;
    regmodel::Bitfield<N> CMDBUSY;
};

template<unsigned int N>
class EVENT_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    EVENT_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3f, 0x3f, 0),
      Reserved0(reg_name + ".Reserved0", *this, 6, 26),
      IDLE(reg_name + ".IDLE", *this, 5, 1),
      READY(reg_name + ".READY", *this, 4, 1),
      TXWM(reg_name + ".TXWM", *this, 3, 1),
      RXWM(reg_name + ".RXWM", *this, 2, 1),
      TXEMPTY(reg_name + ".TXEMPTY", *this, 1, 1),
      RXFULL(reg_name + ".RXFULL", *this, 0, 1)
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
    regmodel::Bitfield<N> IDLE;
    regmodel::Bitfield<N> READY;
    regmodel::Bitfield<N> TXWM;
    regmodel::Bitfield<N> RXWM;
    regmodel::Bitfield<N> TXEMPTY;
    regmodel::Bitfield<N> RXFULL;
};

} // namespace spi_controller

