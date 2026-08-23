/**
 * @file spi_controller_register.h
 * @brief SPI Controller hardware register definitions
 * 
 * This header defines all hardware register types for the SPI Controller IP:
 * - INTR_STATUS - Interrupt status register (software read-only)
 * - INTR_ENABLE - Interrupt enable register
 * - INTR_TEST - Interrupt test register
 * - CTRL - Control register (SPIEN, SW_RST, OUTPUT_EN, watermarks)
 * - STATUS - Status register (READY, ACTIVE, FIFO depths)
 * - CFG - Configuration register (per-device timing, polarity, phase)
 * - CSID - Chip select ID register
 * - CMD - Command register (segment descriptor)
 * - RXDATA - Receive FIFO data register (read-only)
 * - TXDATA - Transmit FIFO data register (write-only)
 * - ERROR_ENABLE - Error interrupt enable register
 * - ERROR_STATUS - Error status register (W1C)
 * - EVENT_ENABLE - Event interrupt enable register
 * 
 * All registers are templated on bit width N (typically 32-bit).
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

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
 * @class INTR_STATUS_type
 * @brief Interrupt Status Register (W1C)
 * 
 * Provides interrupt status bits that are cleared by writing 1.
 * - Bit 4: spi_event - SPI event interrupt (READY, IDLE, FIFO watermarks)
 * - Bit 0: error - Error interrupt (programming violations)
 */
template<unsigned int N>
class INTR_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11, 0x11, 0),
      Reserved0(reg_name + ".Reserved0", *this, 5, 27),
      spi_event(reg_name + ".spi_event", *this, 4, 1),
      Reserved1(reg_name + ".Reserved1", *this, 1, 3),
      error(reg_name + ".error", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> spi_event;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> error;
};

template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11, 0x11, 0),
      Reserved0(reg_name + ".Reserved0", *this, 5, 27),
      spi_event(reg_name + ".spi_event", *this, 4, 1),
      Reserved1(reg_name + ".Reserved1", *this, 1, 3),
      error(reg_name + ".error", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> spi_event;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> error;
};

template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11, 0x11, 0),
      Reserved0(reg_name + ".Reserved0", *this, 5, 27),
      spi_event(reg_name + ".spi_event", *this, 4, 1),
      Reserved1(reg_name + ".Reserved1", *this, 1, 3),
      error(reg_name + ".error", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> spi_event;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> error;
};

template<unsigned int N>
class CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xe000ffff, 0xe000ffff, 127),
      SPIEN(reg_name + ".SPIEN", *this, 31, 1), 
      SW_RST(reg_name + ".SW_RST", *this, 30, 1), 
      OUTPUT_EN(reg_name + ".OUTPUT_EN", *this, 29, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 16, 13), 
      TX_WATERMARK(reg_name + ".TX_WATERMARK", *this, 8, 8), 
      RX_WATERMARK(reg_name + ".RX_WATERMARK", *this, 0, 8)
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
    csml_bitfield<N> SPIEN;
    csml_bitfield<N> SW_RST;
    csml_bitfield<N> OUTPUT_EN;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> TX_WATERMARK;
    csml_bitfield<N> RX_WATERMARK;
};

template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffdfffff, 0x0, 0),
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
    csml_bitfield<N> READY;
    csml_bitfield<N> ACTIVE;
    csml_bitfield<N> TXFULL;
    csml_bitfield<N> TXEMPTY;
    csml_bitfield<N> TXSTALL;
    csml_bitfield<N> TXWM;
    csml_bitfield<N> RXFULL;
    csml_bitfield<N> RXEMPTY;
    csml_bitfield<N> RXSTALL;
    csml_bitfield<N> BYTEORDER;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> RXWM;
    csml_bitfield<N> CMDQD;
    csml_bitfield<N> RXQD;
    csml_bitfield<N> TXQD;
};

template<unsigned int N>
class CFG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CFG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xefffffff, 0xefffffff, 0),
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
    csml_bitfield<N> CPOL;
    csml_bitfield<N> CPHA;
    csml_bitfield<N> FULLCYC;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> CSNLEAD;
    csml_bitfield<N> CSNTRAIL;
    csml_bitfield<N> CSNIDLE;
    csml_bitfield<N> CLKDIV;
};

template<unsigned int N>
class CSID_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CSID_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      CSID(reg_name + ".CSID", *this, 0, 32)
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
    csml_bitfield<N> CSID;
};

template<unsigned int N>
class CMD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CMD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x3fff, 0),
      Reserved0(reg_name + ".Reserved0", *this, 14, 18),
      DIRECTION(reg_name + ".DIRECTION", *this, 12, 2),
      SPEED(reg_name + ".SPEED", *this, 10, 2),
      CSAAT(reg_name + ".CSAAT", *this, 9, 1),
      LEN(reg_name + ".LEN", *this, 0, 9)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> DIRECTION;
    csml_bitfield<N> SPEED;
    csml_bitfield<N> CSAAT;
    csml_bitfield<N> LEN;
};

template<unsigned int N>
class RXDATA_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    RXDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0),
      RXDATA(reg_name + ".RXDATA", *this, 0, 32)
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
    csml_bitfield<N> RXDATA;
};

template<unsigned int N>
class TXDATA_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    TXDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0),
      TXDATA(reg_name + ".TXDATA", *this, 0, 32)
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
    csml_bitfield<N> TXDATA;
};

template<unsigned int N>
class ERROR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERROR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x11111, 0x11111, 0x11111),
      Reserved0(reg_name + ".Reserved0", *this, 17, 15),
      CSIDINVAL(reg_name + ".CSIDINVAL", *this, 16, 1),
      Reserved1(reg_name + ".Reserved1", *this, 13, 3),
      CMDINVAL(reg_name + ".CMDINVAL", *this, 12, 1),
      Reserved2(reg_name + ".Reserved2", *this, 9, 3),
      underflow(reg_name + ".underflow", *this, 8, 1),
      Reserved3(reg_name + ".Reserved3", *this, 5, 3),
      overflow(reg_name + ".overflow", *this, 4, 1),
      Reserved4(reg_name + ".Reserved4", *this, 1, 3),
      CMDBUSY(reg_name + ".CMDBUSY", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> CSIDINVAL;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> CMDINVAL;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> underflow;
    csml_bitfield<N> Reserved3;
    csml_bitfield<N> overflow;
    csml_bitfield<N> Reserved4;
    csml_bitfield<N> CMDBUSY;
};

template<unsigned int N>
class ERROR_STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERROR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x111111, 0x111111, 0),
      Reserved0(reg_name + ".Reserved0", *this, 21, 11),
      ACCESSINVAL(reg_name + ".ACCESSINVAL", *this, 20, 1),
      Reserved1(reg_name + ".Reserved1", *this, 17, 3),
      CSIDINVAL(reg_name + ".CSIDINVAL", *this, 16, 1),
      Reserved2(reg_name + ".Reserved2", *this, 13, 3),
      CMDINVAL(reg_name + ".CMDINVAL", *this, 12, 1),
      Reserved3(reg_name + ".Reserved3", *this, 9, 3),
      underflow(reg_name + ".underflow", *this, 8, 1),
      Reserved4(reg_name + ".Reserved4", *this, 5, 3),
      overflow(reg_name + ".overflow", *this, 4, 1),
      Reserved5(reg_name + ".Reserved5", *this, 1, 3),
      CMDBUSY(reg_name + ".CMDBUSY", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> ACCESSINVAL;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> CSIDINVAL;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> CMDINVAL;
    csml_bitfield<N> Reserved3;
    csml_bitfield<N> underflow;
    csml_bitfield<N> Reserved4;
    csml_bitfield<N> overflow;
    csml_bitfield<N> Reserved5;
    csml_bitfield<N> CMDBUSY;
};

template<unsigned int N>
class EVENT_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    EVENT_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x111111, 0x111111, 0),
      Reserved0(reg_name + ".Reserved0", *this, 21, 11),
      IDLE(reg_name + ".IDLE", *this, 20, 1),
      Reserved1(reg_name + ".Reserved1", *this, 17, 3),
      READY(reg_name + ".READY", *this, 16, 1),
      Reserved2(reg_name + ".Reserved2", *this, 13, 3),
      TXWM(reg_name + ".TXWM", *this, 12, 1),
      Reserved3(reg_name + ".Reserved3", *this, 9, 3),
      RXWM(reg_name + ".RXWM", *this, 8, 1),
      Reserved4(reg_name + ".Reserved4", *this, 5, 3),
      TXEMPTY(reg_name + ".TXEMPTY", *this, 4, 1),
      Reserved5(reg_name + ".Reserved5", *this, 1, 3),
      RXFULL(reg_name + ".RXFULL", *this, 0, 1)
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
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> IDLE;
    csml_bitfield<N> Reserved1;
    csml_bitfield<N> READY;
    csml_bitfield<N> Reserved2;
    csml_bitfield<N> TXWM;
    csml_bitfield<N> Reserved3;
    csml_bitfield<N> RXWM;
    csml_bitfield<N> Reserved4;
    csml_bitfield<N> TXEMPTY;
    csml_bitfield<N> Reserved5;
    csml_bitfield<N> RXFULL;
};

} // namespace spi_controller

