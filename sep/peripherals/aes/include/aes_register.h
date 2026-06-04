/**
 * @file aes_register.h
 * @brief Register type definitions for the AES module
 * 
 * This file defines the template-based register and bitfield layout for the 
 * AES module using the CSML library. Each register is defined as a class 
 * that specifies its offset, reset value, and bitfield structure.
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

namespace aes {

/**
 * @class ALERT_TEST_type
 * @brief Register class for ALERT_TEST (offset 0x00)
 * 
 * Used to trigger hardware alerts via software for testing purposes.
 * This is a write-only register.
 */
template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      recov_ctrl_update_err(reg_name + ".recov_ctrl_update_err", *this, 0, 1), 
      fatal_fault(reg_name + ".fatal_fault", *this, 1, 1), 
      reserved0(reg_name + ".reserved0", *this, 2, 30)
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

    csml_bitfield<N> recov_ctrl_update_err; ///< Trigger recoverable control update error alert
    csml_bitfield<N> fatal_fault;          ///< Trigger fatal hardware fault alert
    csml_bitfield<N> reserved0;            ///< Reserved bits
};

/**
 * @class KEY_SHARE0_type
 * @brief Register class for KEY_SHARE0_0 through KEY_SHARE0_7 (offsets 0x04-0x20)
 * 
 * Holds the first share of the AES key. The actual key is reconstructed by
 * XORing KEY_SHARE0 and KEY_SHARE1.
 */
template<unsigned int N>
class KEY_SHARE0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    KEY_SHARE0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      key_share0(reg_name + ".key_share0", *this, 0, 32)
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

    csml_bitfield<N> key_share0; ///< 32-bit word of key share 0
};

/**
 * @class KEY_SHARE1_type
 * @brief Register class for KEY_SHARE1_0 through KEY_SHARE1_7 (offsets 0x24-0x40)
 * 
 * Holds the second share of the AES key. The actual key is reconstructed by
 * XORing KEY_SHARE0 and KEY_SHARE1.
 */
template<unsigned int N>
class KEY_SHARE1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    KEY_SHARE1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      key_share1(reg_name + ".key_share1", *this, 0, 32)
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

    csml_bitfield<N> key_share1; ///< 32-bit word of key share 1
};

/**
 * @class IV_type
 * @brief Register class for IV_0 through IV_3 (offsets 0x44-0x50)
 * 
 * Initialization Vector registers used for non-ECB block cipher modes.
 */
template<unsigned int N>
class IV_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    IV_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      iv(reg_name + ".iv", *this, 0, 32)
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

    csml_bitfield<N> iv; ///< 32-bit word of the initialization vector
};

/**
 * @class DATA_IN_type
 * @brief Register class for DATA_IN_0 through DATA_IN_3 (offsets 0x54-0x60)
 * 
 * Input data registers for encryption or decryption.
 */
template<unsigned int N>
class DATA_IN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DATA_IN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      data_in(reg_name + ".data_in", *this, 0, 32)
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

    csml_bitfield<N> data_in; ///< 32-bit word of input data
};

/**
 * @class DATA_OUT_type
 * @brief Register class for DATA_OUT_0 through DATA_OUT_3 (offsets 0x64-0x70)
 * 
 * Output data registers containing the result of encryption or decryption.
 */
template<unsigned int N>
class DATA_OUT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DATA_OUT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      data_out(reg_name + ".data_out", *this, 0, 32)
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

    csml_bitfield<N> data_out; ///< 32-bit word of output data
};

/**
 * @class CTRL_SHADOWED_type
 * @brief Register class for CTRL_SHADOWED (offset 0x74)
 * 
 * Main control register for AES operation, mode, and key length.
 * Implements a shadowed two-write protocol for security.
 */
template<unsigned int N>
class CTRL_SHADOWED_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_SHADOWED_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x1181),
      OPERATION(reg_name + ".OPERATION", *this, 0, 2), 
      MODE(reg_name + ".MODE", *this, 2, 6), 
      KEY_LEN(reg_name + ".KEY_LEN", *this, 8, 3), 
      SIDELOAD(reg_name + ".SIDELOAD", *this, 11, 1), 
      PRNG_RESEED_RATE(reg_name + ".PRNG_RESEED_RATE", *this, 12, 3), 
      MANUAL_OPERATION(reg_name + ".MANUAL_OPERATION", *this, 15, 1), 
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

    csml_bitfield<N> OPERATION;         ///< 0x1: Encryption, 0x2: Decryption
    csml_bitfield<N> MODE;              ///< AES mode: 0x01: ECB, 0x02: CBC, 0x04: CFB, 0x08: OFB, 0x10: CTR
    csml_bitfield<N> KEY_LEN;           ///< Key length: 0x1: 128 bit, 0x2: 192 bit, 0x4: 256 bit
    csml_bitfield<N> SIDELOAD;          ///< true: Use key from sideload interface
    csml_bitfield<N> PRNG_RESEED_RATE;  ///< PRNG reseed rate threshold
    csml_bitfield<N> MANUAL_OPERATION;  ///< true: Start cipher via TRIGGER.START
    csml_bitfield<N> reserved0;         ///< Reserved bits
};

/**
 * @class CTRL_AUX_SHADOWED_type
 * @brief Register class for CTRL_AUX_SHADOWED (offset 0x78)
 * 
 * Auxiliary control register for security hardening parameters.
 * Implements a shadowed two-write protocol for security.
 */
template<unsigned int N>
class CTRL_AUX_SHADOWED_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_AUX_SHADOWED_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x1),
      KEY_TOUCH_FORCES_RESEED(reg_name + ".KEY_TOUCH_FORCES_RESEED", *this, 0, 1), 
      FORCE_MASKS(reg_name + ".FORCE_MASKS", *this, 1, 1), 
      reserved0(reg_name + ".reserved0", *this, 2, 30)
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

    csml_bitfield<N> KEY_TOUCH_FORCES_RESEED; ///< Auto-reseed PRNG when key is written
    csml_bitfield<N> FORCE_MASKS;             ///< Force use of masking PRNG (hardening)
    csml_bitfield<N> reserved0;               ///< Reserved bits
};

/**
 * @class CTRL_AUX_REGWEN_type
 * @brief Register class for CTRL_AUX_REGWEN (offset 0x7C)
 * 
 * Write enablement register for CTRL_AUX_SHADOWED.
 */
template<unsigned int N>
class CTRL_AUX_REGWEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_AUX_REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x1),
      CTRL_AUX_REGWEN(reg_name + ".CTRL_AUX_REGWEN", *this, 0, 1),
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

    csml_bitfield<N> CTRL_AUX_REGWEN; ///< 1: CTRL_AUX can be written, 0: Locked
    csml_bitfield<N> reserved0;       ///< Reserved bits
};

/**
 * @class TRIGGER_type
 * @brief Register class for TRIGGER (offset 0x80)
 * 
 * Triggers asynchronous manual operations like start, clear, or reseed.
 * Bits are self-clearing.
 */
template<unsigned int N>
class TRIGGER_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    TRIGGER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0xe),
      START(reg_name + ".START", *this, 0, 1), 
      KEY_IV_DATA_IN_CLEAR(reg_name + ".KEY_IV_DATA_IN_CLEAR", *this, 1, 1), 
      DATA_OUT_CLEAR(reg_name + ".DATA_OUT_CLEAR", *this, 2, 1), 
      PRNG_RESEED(reg_name + ".PRNG_RESEED", *this, 3, 1), 
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

    csml_bitfield<N> START;                ///< Start manual encryption/decryption
    csml_bitfield<N> KEY_IV_DATA_IN_CLEAR; ///< Clear key, IV, and input data with fake PRNG
    csml_bitfield<N> DATA_OUT_CLEAR;        ///< Clear output registers with fake PRNG
    csml_bitfield<N> PRNG_RESEED;          ///< Manually trigger PRNG reseed using OpenSSL
    csml_bitfield<N> reserved0;            ///< Reserved bits
};

/**
 * @class STATUS_type
 * @brief Register class for STATUS (offset 0x84)
 * 
 * Provides visibility into the current internal state and error flags of the AES module.
 */
template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      IDLE(reg_name + ".IDLE", *this, 0, 1), 
      STALL(reg_name + ".STALL", *this, 1, 1), 
      OUTPUT_LOST(reg_name + ".OUTPUT_LOST", *this, 2, 1), 
      OUTPUT_VALID(reg_name + ".OUTPUT_VALID", *this, 3, 1), 
      INPUT_READY(reg_name + ".INPUT_READY", *this, 4, 1), 
      ALERT_RECOV_CTRL_UPDATE_ERR(reg_name + ".ALERT_RECOV_CTRL_UPDATE_ERR", *this, 5, 1), 
      ALERT_FATAL_FAULT(reg_name + ".ALERT_FATAL_FAULT", *this, 6, 1), 
      reserved0(reg_name + ".reserved0", *this, 7, 25)
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

    csml_bitfield<N> IDLE;                        ///< true: AES module is idle
    csml_bitfield<N> STALL;                       ///< true: Module is stalled waiting for output read
    csml_bitfield<N> OUTPUT_LOST;                 ///< true: New output overwrote previous unread output
    csml_bitfield<N> OUTPUT_VALID;                ///< true: DATA_OUT registers contain valid result
    csml_bitfield<N> INPUT_READY;                 ///< true: Module is ready to accept new DATA_IN
    csml_bitfield<N> ALERT_RECOV_CTRL_UPDATE_ERR; ///< Mirror of recoverable alert status
    csml_bitfield<N> ALERT_FATAL_FAULT;           ///< Mirror of fatal alert status
    csml_bitfield<N> reserved0;                   ///< Reserved bits
};

}