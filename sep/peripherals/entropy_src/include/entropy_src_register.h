// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * Entropy Source Register Type Definitions
 * Auto-generated from RDL specification
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "reg_file.h"

namespace entropy_src {

template<unsigned int N>
class COMPONENT_ID_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    COMPONENT_ID_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x01000001),
      NAME(reg_name + ".NAME", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 8), 
      MINOR_VERSION(reg_name + ".MINOR_VERSION", *this, 24, 4), 
      MAJOR_VERSION(reg_name + ".MAJOR_VERSION", *this, 28, 4)
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
    regmodel::Bitfield<N> NAME;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> MINOR_VERSION;
    regmodel::Bitfield<N> MAJOR_VERSION;
};

template<unsigned int N>
class CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x13FF0113, 0x13FF0112, 0x10000002),
      RSVD0(reg_name + ".RSVD0", *this, 0, 1),
      MODULE_ENABLE(reg_name + ".MODULE_ENABLE", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 2),
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
    regmodel::Bitfield<N> RSVD0;
    regmodel::Bitfield<N> MODULE_ENABLE;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> AUTOTUNE_ENABLE;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> BYPASS_ENTROPY_COMPRESSOR;
    regmodel::Bitfield<N> reserved2;
    regmodel::Bitfield<N> DOWNSAMPLE_RATE;
    regmodel::Bitfield<N> reserved3;
    regmodel::Bitfield<N> SHA256_WHITENING_ENABLE;
    regmodel::Bitfield<N> reserved4;
};

template<unsigned int N>
class STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000001, 0x00000000, 0x00000000),
      RSVD(reg_name + ".RSVD", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 31)
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
    regmodel::Bitfield<N> RSVD;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class SHA256_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    SHA256_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000007F1, 0x00000000, 0x00000000),
      BUSY(reg_name + ".BUSY", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      INPUT_COUNT(reg_name + ".INPUT_COUNT", *this, 4, 4), 
      OUTPUT_COUNT(reg_name + ".OUTPUT_COUNT", *this, 8, 3), 
      reserved1(reg_name + ".reserved1", *this, 11, 21)
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
    regmodel::Bitfield<N> BUSY;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> INPUT_COUNT;
    regmodel::Bitfield<N> OUTPUT_COUNT;
    regmodel::Bitfield<N> reserved1;
};

template<unsigned int N>
class DEBUG_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    DEBUG_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000007FF, 0x000007FF, 0x00000000),
      SELECT_SIGNAL(reg_name + ".SELECT_SIGNAL", *this, 0, 8), 
      SELECT_FREQ_DIV(reg_name + ".SELECT_FREQ_DIV", *this, 8, 3), 
      reserved0(reg_name + ".reserved0", *this, 11, 21)
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
    regmodel::Bitfield<N> SELECT_SIGNAL;
    regmodel::Bitfield<N> SELECT_FREQ_DIV;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class INTR_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x11111111, 0x00000000, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 3),
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1),
      reserved1(reg_name + ".reserved1", *this, 5, 3),
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1),
      reserved2(reg_name + ".reserved2", *this, 9, 3),
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1),
      reserved3(reg_name + ".reserved3", *this, 13, 3),
      PERSISTENT_FAILURE(reg_name + ".PERSISTENT_FAILURE", *this, 16, 1),
      reserved4(reg_name + ".reserved4", *this, 17, 3),
      AUTOTUNE_FAIL(reg_name + ".AUTOTUNE_FAIL", *this, 20, 1),
      reserved5(reg_name + ".reserved5", *this, 21, 3),
      BIW_OBS_OVERFLOW(reg_name + ".BIW_OBS_OVERFLOW", *this, 24, 1),
      reserved6(reg_name + ".reserved6", *this, 25, 3),
      NOISE_OBS_OVERFLOW(reg_name + ".NOISE_OBS_OVERFLOW", *this, 28, 1),
      reserved7(reg_name + ".reserved7", *this, 29, 3)
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
    regmodel::Bitfield<N> HEALTH_TEST_FAILED;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> FIFO_ERROR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> FIFO_OVERFLOW;
    regmodel::Bitfield<N> reserved2;
    regmodel::Bitfield<N> FIFO_UNDERFLOW;
    regmodel::Bitfield<N> reserved3;
    regmodel::Bitfield<N> PERSISTENT_FAILURE;
    regmodel::Bitfield<N> reserved4;
    regmodel::Bitfield<N> AUTOTUNE_FAIL;
    regmodel::Bitfield<N> reserved5;
    regmodel::Bitfield<N> BIW_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved6;
    regmodel::Bitfield<N> NOISE_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved7;
};


template<unsigned int N>
class INTR_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x11111111, 0x11111111, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 3),
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1),
      reserved1(reg_name + ".reserved1", *this, 5, 3),
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1),
      reserved2(reg_name + ".reserved2", *this, 9, 3),
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1),
      reserved3(reg_name + ".reserved3", *this, 13, 3),
      PERSISTENT_FAILURE(reg_name + ".PERSISTENT_FAILURE", *this, 16, 1),
      reserved4(reg_name + ".reserved4", *this, 17, 3),
      AUTOTUNE_FAIL(reg_name + ".AUTOTUNE_FAIL", *this, 20, 1),
      reserved5(reg_name + ".reserved5", *this, 21, 3),
      BIW_OBS_OVERFLOW(reg_name + ".BIW_OBS_OVERFLOW", *this, 24, 1),
      reserved6(reg_name + ".reserved6", *this, 25, 3),
      NOISE_OBS_OVERFLOW(reg_name + ".NOISE_OBS_OVERFLOW", *this, 28, 1),
      reserved7(reg_name + ".reserved7", *this, 29, 3)
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
    regmodel::Bitfield<N> HEALTH_TEST_FAILED;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> FIFO_ERROR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> FIFO_OVERFLOW;
    regmodel::Bitfield<N> reserved2;
    regmodel::Bitfield<N> FIFO_UNDERFLOW;
    regmodel::Bitfield<N> reserved3;
    regmodel::Bitfield<N> PERSISTENT_FAILURE;
    regmodel::Bitfield<N> reserved4;
    regmodel::Bitfield<N> AUTOTUNE_FAIL;
    regmodel::Bitfield<N> reserved5;
    regmodel::Bitfield<N> BIW_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved6;
    regmodel::Bitfield<N> NOISE_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved7;
};

template<unsigned int N>
class INTR_TEST_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000000, 0x11111111, 0x00000000),
      HEALTH_TEST_FAILED(reg_name + ".HEALTH_TEST_FAILED", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 3),
      FIFO_ERROR(reg_name + ".FIFO_ERROR", *this, 4, 1),
      reserved1(reg_name + ".reserved1", *this, 5, 3),
      FIFO_OVERFLOW(reg_name + ".FIFO_OVERFLOW", *this, 8, 1),
      reserved2(reg_name + ".reserved2", *this, 9, 3),
      FIFO_UNDERFLOW(reg_name + ".FIFO_UNDERFLOW", *this, 12, 1),
      reserved3(reg_name + ".reserved3", *this, 13, 3),
      PERSISTENT_FAILURE(reg_name + ".PERSISTENT_FAILURE", *this, 16, 1),
      reserved4(reg_name + ".reserved4", *this, 17, 3),
      AUTOTUNE_FAIL(reg_name + ".AUTOTUNE_FAIL", *this, 20, 1),
      reserved5(reg_name + ".reserved5", *this, 21, 3),
      BIW_OBS_OVERFLOW(reg_name + ".BIW_OBS_OVERFLOW", *this, 24, 1),
      reserved6(reg_name + ".reserved6", *this, 25, 3),
      NOISE_OBS_OVERFLOW(reg_name + ".NOISE_OBS_OVERFLOW", *this, 28, 1),
      reserved7(reg_name + ".reserved7", *this, 29, 3)
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
    regmodel::Bitfield<N> HEALTH_TEST_FAILED;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> FIFO_ERROR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> FIFO_OVERFLOW;
    regmodel::Bitfield<N> reserved2;
    regmodel::Bitfield<N> FIFO_UNDERFLOW;
    regmodel::Bitfield<N> reserved3;
    regmodel::Bitfield<N> PERSISTENT_FAILURE;
    regmodel::Bitfield<N> reserved4;
    regmodel::Bitfield<N> AUTOTUNE_FAIL;
    regmodel::Bitfield<N> reserved5;
    regmodel::Bitfield<N> BIW_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved6;
    regmodel::Bitfield<N> NOISE_OBS_OVERFLOW;
    regmodel::Bitfield<N> reserved7;
};

template<unsigned int N>
class FIFO_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    FIFO_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000011, 0x00000011, 0x00000001),
      ENABLE(reg_name + ".ENABLE", *this, 0, 1), 
      reserved0(reg_name + ".reserved0", *this, 1, 3), 
      ENTROPY_CHURN_ENABLE(reg_name + ".ENTROPY_CHURN_ENABLE", *this, 4, 1), 
      reserved1(reg_name + ".reserved1", *this, 5, 27)
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
    regmodel::Bitfield<N> ENABLE;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> ENTROPY_CHURN_ENABLE;
    regmodel::Bitfield<N> reserved1;
};

/******************************************************************************
 * @brief MAIN_SM_STATUS register (offset 0xB4, RO)
 *
 * Status of the entropy source's main state machine. Firmware polls
 * BOOT_PHASE_DONE to learn that the startup health-test window has passed and
 * entropy is reaching the whitener/FIFO -- the SEP boot ROM's entropy bring-up
 * gates on exactly this bit before enabling EDN, and treats ALERT or ERR as a
 * terminal startup-health failure.
 *
 * Fields: STATE[8:0], IDLE[9], ALERT[10], ERR[11], BOOT_PHASE_DONE[12],
 * ALERT_CNTR_CLR_OK[13].
 ******************************************************************************/
template<unsigned int N>
class MAIN_SM_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MAIN_SM_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00003FFF, 0x00000000, 0x00000000),
      STATE(reg_name + ".STATE", *this, 0, 9),
      IDLE(reg_name + ".IDLE", *this, 9, 1),
      ALERT(reg_name + ".ALERT", *this, 10, 1),
      ERR(reg_name + ".ERR", *this, 11, 1),
      BOOT_PHASE_DONE(reg_name + ".BOOT_PHASE_DONE", *this, 12, 1),
      ALERT_CNTR_CLR_OK(reg_name + ".ALERT_CNTR_CLR_OK", *this, 13, 1),
      reserved0(reg_name + ".reserved0", *this, 14, 18)
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
    regmodel::Bitfield<N> STATE;
    regmodel::Bitfield<N> IDLE;
    regmodel::Bitfield<N> ALERT;
    regmodel::Bitfield<N> ERR;
    regmodel::Bitfield<N> BOOT_PHASE_DONE;
    regmodel::Bitfield<N> ALERT_CNTR_CLR_OK;
    regmodel::Bitfield<N> reserved0;
};

/******************************************************************************
 * @brief FIPS_LOCK register (offset 0x154, W1S)
 *
 * Write-one-to-set lock over the certified entropy-source configuration
 * (conditioning, health-test thresholds, compressor bypass). Once set it stays
 * set until the entropy chain is reset; DEBUG_CTRL is intentionally outside it.
 *
 * The SEP boot ROM applies this by default after the startup health test passes
 * and reads it back, failing the boot if it did not stick -- so it has to be a
 * real settable bit here, not a stub that reads 0.
 ******************************************************************************/
template<unsigned int N>
class FIPS_LOCK_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    FIPS_LOCK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000001, 0x00000001, 0x00000000),
      LOCK(reg_name + ".LOCK", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 31)
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

    // W1S is enforced by the entropy_src_ip write callback, not by a
    // handle_write() override here. regmodel::Memory::write_callbacks is a
    // std::map keyed on word offset, so the callback entropy_src_ip registers
    // for FIPS_LOCK.offset *replaces* the one Reg<N>'s constructor bound to
    // Reg<N>::handle_write. An override would therefore never be reached.

    regmodel::Bitfield<N> LOCK;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class FIFO_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    FIFO_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x003F3F7F, 0x00000000, 0x00000000),
      LEVEL(reg_name + ".LEVEL", *this, 0, 7),
      reserved0(reg_name + ".reserved0", *this, 7, 1),
      WPTR(reg_name + ".WPTR", *this, 8, 6),
      reserved1(reg_name + ".reserved1", *this, 14, 2),
      RPTR(reg_name + ".RPTR", *this, 16, 6),
      reserved2(reg_name + ".reserved2", *this, 22, 10)
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
    regmodel::Bitfield<N> LEVEL;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> WPTR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> RPTR;
    regmodel::Bitfield<N> reserved2;
};

template<unsigned int N>
class FIFO_RDATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    FIFO_RDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      RDATA(reg_name + ".RDATA", *this, 0, 32)
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
    regmodel::Bitfield<N> RDATA;
};

template<unsigned int N>
class HEALTH_TEST_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    HEALTH_TEST_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00001907),
      ENABLE(reg_name + ".ENABLE", *this, 0, 8), 
      REPETITION_LIMIT(reg_name + ".REPETITION_LIMIT", *this, 8, 8), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> ENABLE;
    regmodel::Bitfield<N> REPETITION_LIMIT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class HEALTH_TEST_WINDOW_SIZE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    HEALTH_TEST_WINDOW_SIZE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000800),
      SIZE(reg_name + ".SIZE", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> SIZE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class MARKOV_TEST_PROB_THRESHOLDS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MARKOV_TEST_PROB_THRESHOLDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0xFFFFFFFF, 0x006404B0),
      PROB_01_THRESHOLD(reg_name + ".PROB_01_THRESHOLD", *this, 0, 16), 
      PROB_10_THRESHOLD(reg_name + ".PROB_10_THRESHOLD", *this, 16, 16)
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
    regmodel::Bitfield<N> PROB_01_THRESHOLD;
    regmodel::Bitfield<N> PROB_10_THRESHOLD;
};

template<unsigned int N>
class HEALTH_TEST_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    HEALTH_TEST_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      HEALTH_STATUS(reg_name + ".HEALTH_STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> HEALTH_STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class REPETITION_TEST_COUNT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    REPETITION_TEST_COUNT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      REPETITION_COUNT(reg_name + ".REPETITION_COUNT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> REPETITION_COUNT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PATTERN_COUNT_1BIT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_PATTERN_COUNT_1BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3FFFFFF1, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 16), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 16, 4), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved0(reg_name + ".reserved0", *this, 30, 2)
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
    regmodel::Bitfield<N> PATTERN_COUNT;
    regmodel::Bitfield<N> TARGET_PATTERN;
    regmodel::Bitfield<N> SAMPLES_PROCESSED;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PATTERN_COUNT_2BIT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_PATTERN_COUNT_2BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x3FFFFFF1, 0x00000000, 0x00000000),
      PATTERN_COUNT(reg_name + ".PATTERN_COUNT", *this, 0, 16), 
      TARGET_PATTERN(reg_name + ".TARGET_PATTERN", *this, 16, 4), 
      SAMPLES_PROCESSED(reg_name + ".SAMPLES_PROCESSED", *this, 20, 10), 
      reserved0(reg_name + ".reserved0", *this, 30, 2)
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
    regmodel::Bitfield<N> PATTERN_COUNT;
    regmodel::Bitfield<N> TARGET_PATTERN;
    regmodel::Bitfield<N> SAMPLES_PROCESSED;
    regmodel::Bitfield<N> reserved0;
};
template<unsigned int N>
class APT_PROPORTION_1BIT_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_PROPORTION_1BIT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x000004B0),
      LIMIT(reg_name + ".LIMIT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> LIMIT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class APT_PROPORTION_LO_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_PROPORTION_LO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000350),
      LIMIT(reg_name + ".LIMIT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> LIMIT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class MARKOV_TEST_COUNTS_0_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MARKOV_TEST_COUNTS_0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      COUNT_01(reg_name + ".COUNT_01", *this, 0, 16), 
      COUNT_10(reg_name + ".COUNT_10", *this, 16, 16)
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
    regmodel::Bitfield<N> COUNT_01;
    regmodel::Bitfield<N> COUNT_10;
};
template<unsigned int N>
class RING_OSC_ENABLE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RING_OSC_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF),
      ENABLE(reg_name + ".ENABLE", *this, 0, 12), 
      SAMPLE_CLK_ENABLE(reg_name + ".SAMPLE_CLK_ENABLE", *this, 12, 12), 
      reserved0(reg_name + ".reserved0", *this, 24, 8)
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
    regmodel::Bitfield<N> ENABLE;
    regmodel::Bitfield<N> SAMPLE_CLK_ENABLE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class RING_OSC_TUNE_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RING_OSC_TUNE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00FFFFFF, 0x00FFFFFF, 0x00000000),
      DETUNE(reg_name + ".DETUNE", *this, 0, 12), 
      SAMPLE_CLK_DETUNE(reg_name + ".SAMPLE_CLK_DETUNE", *this, 12, 12), 
      reserved0(reg_name + ".reserved0", *this, 24, 8)
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
    regmodel::Bitfield<N> DETUNE;
    regmodel::Bitfield<N> SAMPLE_CLK_DETUNE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class RING_OSC_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RING_OSC_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000FFF, 0x00000FFF, 0x00000FFF),
      SAMPLE_CLK_SELECT(reg_name + ".SAMPLE_CLK_SELECT", *this, 0, 12), 
      reserved0(reg_name + ".reserved0", *this, 12, 20)
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
    regmodel::Bitfield<N> SAMPLE_CLK_SELECT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class DECORRELATOR_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    DECORRELATOR_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0xFFFFFFFF, 0x0003F000),
      BYPASS(reg_name + ".BYPASS", *this, 0, 12), 
      SAMPLE_CLK_DIV(reg_name + ".SAMPLE_CLK_DIV", *this, 12, 20)
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
    regmodel::Bitfield<N> BYPASS;
    regmodel::Bitfield<N> SAMPLE_CLK_DIV;
};

template<unsigned int N>
class DECORRELATOR_MASK_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    DECORRELATOR_MASK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x000000FF),
      ENTROPY_BYTE_MASK(reg_name + ".ENTROPY_BYTE_MASK", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> ENTROPY_BYTE_MASK;
    regmodel::Bitfield<N> reserved0;
};
/******************************************************************************
 * @brief STARTUP_CTRL register (offset 0xB0, RW)
 *
 * DELAY_CYCLES[15:0] programs the abstract startup hold-off (nanoseconds in
 * this LT model) applied when the entropy generation thread leaves
 * WAITING_FOR_ENABLE or boots after rst_ni release.
 ******************************************************************************/
template<unsigned int N>
class STARTUP_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    STARTUP_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000000),
      DELAY_CYCLES(reg_name + ".DELAY_CYCLES", *this, 0, 16),
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> DELAY_CYCLES;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_0_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_0_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_1_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_1_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_2_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_2_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_3_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_3_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_4_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_4_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_5_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_5_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_6_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_6_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_7_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_7_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_8_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_8_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_9_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_9_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_10_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_10_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_11_HEALTH_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_11_HEALTH_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x00000000),
      STATUS(reg_name + ".STATUS", *this, 0, 8), 
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> STATUS;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_0_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_0_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_1_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_1_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_2_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_2_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_3_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_3_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_4_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_4_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_5_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_5_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_6_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_6_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_7_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_7_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000000),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_8_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_8_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_9_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_9_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000001),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_10_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_10_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class GENERATOR_11_SAMPLE_CLK_CONFIG_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    GENERATOR_11_SAMPLE_CLK_CONFIG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000001F, 0x0000001F, 0x00000002),
      SAMPLE_CLK_DIVIDE(reg_name + ".SAMPLE_CLK_DIVIDE", *this, 0, 5), 
      reserved0(reg_name + ".reserved0", *this, 5, 27)
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
    regmodel::Bitfield<N> SAMPLE_CLK_DIVIDE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class HT_WATERMARK_NUM_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    HT_WATERMARK_NUM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000000F, 0x0000000F, 0x00000000),
      WATERMARK_NUM(reg_name + ".WATERMARK_NUM", *this, 0, 4), 
      reserved0(reg_name + ".reserved0", *this, 4, 28)
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
    regmodel::Bitfield<N> WATERMARK_NUM;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class HT_WATERMARK_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    HT_WATERMARK_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      WATERMARK_VALUE(reg_name + ".WATERMARK_VALUE", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> WATERMARK_VALUE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class REPCNT_TOTAL_FAILS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    REPCNT_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    regmodel::Bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class APT_HI_TOTAL_FAILS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_HI_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    regmodel::Bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class APT_LO_TOTAL_FAILS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    APT_LO_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    regmodel::Bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class MARKOV_HI_TOTAL_FAILS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MARKOV_HI_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    regmodel::Bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class MARKOV_LO_TOTAL_FAILS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MARKOV_LO_TOTAL_FAILS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      FAIL_COUNT(reg_name + ".FAIL_COUNT", *this, 0, 32)
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
    regmodel::Bitfield<N> FAIL_COUNT;
};

template<unsigned int N>
class ALERT_SUMMARY_FAIL_COUNTS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ALERT_SUMMARY_FAIL_COUNTS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x00000000, 0x00000000),
      ANY_FAIL_COUNT(reg_name + ".ANY_FAIL_COUNT", *this, 0, 16), 
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> ANY_FAIL_COUNT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class ALERT_FAIL_COUNTS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ALERT_FAIL_COUNTS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000FFFFF, 0x00000000, 0x00000000),
      APT_LO_FAIL_COUNT(reg_name + ".APT_LO_FAIL_COUNT", *this, 0, 4), 
      APT_HI_FAIL_COUNT(reg_name + ".APT_HI_FAIL_COUNT", *this, 4, 4), 
      MARKOV_LO_FAIL_COUNT(reg_name + ".MARKOV_LO_FAIL_COUNT", *this, 8, 4), 
      MARKOV_HI_FAIL_COUNT(reg_name + ".MARKOV_HI_FAIL_COUNT", *this, 12, 4), 
      REPCNT_FAIL_COUNT(reg_name + ".REPCNT_FAIL_COUNT", *this, 16, 4), 
      reserved0(reg_name + ".reserved0", *this, 20, 12)
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
    regmodel::Bitfield<N> APT_LO_FAIL_COUNT;
    regmodel::Bitfield<N> APT_HI_FAIL_COUNT;
    regmodel::Bitfield<N> MARKOV_LO_FAIL_COUNT;
    regmodel::Bitfield<N> MARKOV_HI_FAIL_COUNT;
    regmodel::Bitfield<N> REPCNT_FAIL_COUNT;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class ALERT_THRESHOLD_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    ALERT_THRESHOLD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x0000FFFF, 0x0000FFFF, 0x00000004),
      THRESHOLD(reg_name + ".THRESHOLD", *this, 0, 16),
      reserved0(reg_name + ".reserved0", *this, 16, 16)
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
    regmodel::Bitfield<N> THRESHOLD;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class MIN_ENTROPY_H_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    MIN_ENTROPY_H_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000FF, 0x000000FF, 0x0000000C),
      H(reg_name + ".H", *this, 0, 8),
      reserved0(reg_name + ".reserved0", *this, 8, 24)
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
    regmodel::Bitfield<N> H;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class RECOMMENDED_THRESHOLDS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    RECOMMENDED_THRESHOLDS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      RCT_LIMIT(reg_name + ".RCT_LIMIT", *this, 0, 16),
      APT_LIMIT(reg_name + ".APT_LIMIT", *this, 16, 16)
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
    regmodel::Bitfield<N> RCT_LIMIT;
    regmodel::Bitfield<N> APT_LIMIT;
};

template<unsigned int N>
class BIW_OBS_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    BIW_OBS_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x00000001, 0x00000001, 0x00000000),
      RAW_ENABLE(reg_name + ".RAW_ENABLE", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 31)
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
    regmodel::Bitfield<N> RAW_ENABLE;
    regmodel::Bitfield<N> reserved0;
};

template<unsigned int N>
class BIW_OBS_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    BIW_OBS_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x003F3F7F, 0x00000000, 0x00000000),
      LEVEL(reg_name + ".LEVEL", *this, 0, 7),
      reserved0(reg_name + ".reserved0", *this, 7, 1),
      WPTR(reg_name + ".WPTR", *this, 8, 6),
      reserved1(reg_name + ".reserved1", *this, 14, 2),
      RPTR(reg_name + ".RPTR", *this, 16, 6),
      reserved2(reg_name + ".reserved2", *this, 22, 10)
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
    regmodel::Bitfield<N> LEVEL;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> WPTR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> RPTR;
    regmodel::Bitfield<N> reserved2;
};

template<unsigned int N>
class BIW_OBS_RDATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    BIW_OBS_RDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      RDATA(reg_name + ".RDATA", *this, 0, 32)
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
    regmodel::Bitfield<N> RDATA;
};

template<unsigned int N>
class NOISE_OBS_CTRL_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    NOISE_OBS_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x000000F1, 0x000000F3, 0x00000000),
      RAW_ENABLE(reg_name + ".RAW_ENABLE", *this, 0, 1),
      FLUSH(reg_name + ".FLUSH", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 2),
      LANE_SEL(reg_name + ".LANE_SEL", *this, 4, 4),
      reserved1(reg_name + ".reserved1", *this, 8, 24)
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
    regmodel::Bitfield<N> RAW_ENABLE;
    regmodel::Bitfield<N> FLUSH;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> LANE_SEL;
    regmodel::Bitfield<N> reserved1;
};

template<unsigned int N>
class NOISE_OBS_STATUS_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    NOISE_OBS_STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0x003F3F7F, 0x00000000, 0x00000000),
      LEVEL(reg_name + ".LEVEL", *this, 0, 7),
      reserved0(reg_name + ".reserved0", *this, 7, 1),
      WPTR(reg_name + ".WPTR", *this, 8, 6),
      reserved1(reg_name + ".reserved1", *this, 14, 2),
      RPTR(reg_name + ".RPTR", *this, 16, 6),
      reserved2(reg_name + ".reserved2", *this, 22, 10)
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
    regmodel::Bitfield<N> LEVEL;
    regmodel::Bitfield<N> reserved0;
    regmodel::Bitfield<N> WPTR;
    regmodel::Bitfield<N> reserved1;
    regmodel::Bitfield<N> RPTR;
    regmodel::Bitfield<N> reserved2;
};

template<unsigned int N>
class NOISE_OBS_RDATA_type : public regmodel::Reg<N>
{
  public:
    using typename regmodel::Reg<N>::memory_type;
    typedef typename regmodel::Word<N>::wordtype DT;
    NOISE_OBS_RDATA_type(std::string reg_name, memory_type &memory, unsigned int offset):
      regmodel::Reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x00000000),
      RDATA(reg_name + ".RDATA", *this, 0, 32)
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
    regmodel::Bitfield<N> RDATA;
};


}