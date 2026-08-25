// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file kmac_register.h
 * @brief KMAC register type definitions for CSML-based TLM model
 *
 * This file contains all register structure definitions for the KMAC/SHA3 IP.
 * Generated from CSV specification using CSML generator tool.
 * Includes 17 register types with complete field definitions for interrupt control,
 * configuration, command/status, entropy management, secret keys, and error reporting.
 *
 * Copyright header goes here...
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

namespace kmac {

/**
 * @class INTR_STATE_type
 * @brief Interrupt State Register (offset 0x0)
 *
 * Holds interrupt state flags for KMAC/SHA3 operations.
 * Reset value: 0x00000000
 * Access: RW (with W1C for interrupt bits)
 */
template<unsigned int N>
class INTR_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    /**
     * @brief Constructor for INTR_STATE register
     * @param reg_name Register name string
     * @param memory Reference to register memory
     * @param offset Register offset address
     */
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x5, 0x0),
      kmac_done(reg_name + ".kmac_done", *this, 0, 1),
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1),
      kmac_err(reg_name + ".kmac_err", *this, 2, 1),
      reserved0(reg_name + ".reserved0", *this, 3, 29)
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

    csml_bitfield<N> kmac_done;     ///< Bit[0]: KMAC/SHA3 absorbing completed (RW1C)
    csml_bitfield<N> fifo_empty;    ///< Bit[1]: Message FIFO empty indicator (RO)
    csml_bitfield<N> kmac_err;      ///< Bit[2]: KMAC/SHA3 error occurred - see ERR_CODE (RW1C)
    csml_bitfield<N> reserved0;     ///< Bits[31:3]: Reserved, read as 0
};

template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x7, 0x0),
      kmac_done(reg_name + ".kmac_done", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      kmac_err(reg_name + ".kmac_err", *this, 2, 1), 
      reserved0(reg_name + ".reserved0", *this, 3, 29)
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
    csml_bitfield<N> kmac_done;
    csml_bitfield<N> fifo_empty;
    csml_bitfield<N> kmac_err;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x7, 0x0),
      kmac_done(reg_name + ".kmac_done", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      kmac_err(reg_name + ".kmac_err", *this, 2, 1), 
      reserved0(reg_name + ".reserved0", *this, 3, 29)
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
    csml_bitfield<N> kmac_done;
    csml_bitfield<N> fifo_empty;
    csml_bitfield<N> kmac_err;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x3, 0x0),
      recov_operation_err(reg_name + ".recov_operation_err", *this, 0, 1), 
      fatal_fault_err(reg_name + ".fatal_fault_err", *this, 1, 1), 
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
    csml_bitfield<N> recov_operation_err;
    csml_bitfield<N> fatal_fault_err;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class CFG_REGWEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CFG_REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x0, 0x1),
      en(reg_name + ".en", *this, 0, 1), 
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
    csml_bitfield<N> en;
    csml_bitfield<N> reserved0;
};

/**
 * @class CFG_SHADOWED_type
 * @brief KMAC Configuration Register - Shadowed (offset 0x14)
 *
 * Main configuration register for KMAC/SHA3 operations. Shadowed register requiring
 * duplicate write sequence for fault protection. Protected by CFG_REGWEN.
 * Only writable when STATUS.sha3_idle=1.
 * Reset value: 0x00000000
 * Access: RW
 */
template<unsigned int N>
class CFG_SHADOWED_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    /**
     * @brief Constructor for CFG_SHADOWED register
     * @param reg_name Register name string
     * @param memory Reference to register memory
     * @param offset Register offset address (0x14)
     */
    CFG_SHADOWED_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x51b133f, 0x51b133f, 0x1000),
      kmac_en(reg_name + ".kmac_en", *this, 0, 1),
      kstrength(reg_name + ".kstrength", *this, 1, 3),
      mode(reg_name + ".mode", *this, 4, 2),
      reserved0(reg_name + ".reserved0", *this, 6, 2),
      msg_endianness(reg_name + ".msg_endianness", *this, 8, 1),
      state_endianness(reg_name + ".state_endianness", *this, 9, 1),
      reserved1(reg_name + ".reserved1", *this, 10, 2),
      sideload(reg_name + ".sideload", *this, 12, 1),
      reserved2(reg_name + ".reserved2", *this, 13, 3),
      entropy_mode(reg_name + ".entropy_mode", *this, 16, 2),
      reserved3(reg_name + ".reserved3", *this, 18, 1),
      entropy_fast_process(reg_name + ".entropy_fast_process", *this, 19, 1),
      msg_mask(reg_name + ".msg_mask", *this, 20, 1),
      reserved4(reg_name + ".reserved4", *this, 21, 3),
      entropy_ready(reg_name + ".entropy_ready", *this, 24, 1),
      reserved5(reg_name + ".reserved5", *this, 25, 1),
      en_unsupported_modestrength(reg_name + ".en_unsupported_modestrength", *this, 26, 1),
      reserved6(reg_name + ".reserved6", *this, 27, 5)
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

    csml_bitfield<N> kmac_en;                      ///< Bit[0]: KMAC datapath enable (0=SHA3/SHAKE/cSHAKE 1=KMAC)
    csml_bitfield<N> kstrength;                    ///< Bits[3:1]: Hashing strength (0=L128 1=L224 2=L256 3=L384 4=L512)
    csml_bitfield<N> mode;                         ///< Bits[5:4]: Keccak mode (0=SHA3 2=SHAKE 3=cSHAKE)
    csml_bitfield<N> reserved0;                    ///< Bits[7:6]: Reserved
    csml_bitfield<N> msg_endianness;               ///< Bit[8]: Message endianness (0=little 1=big)
    csml_bitfield<N> state_endianness;             ///< Bit[9]: State endianness (0=little 1=big)
    csml_bitfield<N> reserved1;                    ///< Bits[11:10]: Reserved
    csml_bitfield<N> sideload;                     ///< Bit[12]: Use sideloaded key from KeyMgr
    csml_bitfield<N> reserved2;                    ///< Bits[15:13]: Reserved
    csml_bitfield<N> entropy_mode;                 ///< Bits[17:16]: Entropy mode (0=idle 1=edn 2=sw)
    csml_bitfield<N> reserved3;                    ///< Bit[18]: Reserved
    csml_bitfield<N> entropy_fast_process;         ///< Bit[19]: Entropy fast process mode
    csml_bitfield<N> msg_mask;                     ///< Bit[20]: Message masking with PRNG
    csml_bitfield<N> reserved4;                    ///< Bits[23:21]: Reserved
    csml_bitfield<N> entropy_ready;                ///< Bit[24]: Entropy ready status
    csml_bitfield<N> reserved5;                    ///< Bit[25]: Reserved
    csml_bitfield<N> en_unsupported_modestrength;  ///< Bit[26]: Enable unsupported mode/strength combinations
    csml_bitfield<N> reserved6;                    ///< Bits[31:27]: Reserved
};

/**
 * @class CMD_type
 * @brief KMAC/SHA3 Command Register (offset 0x18)
 *
 * Control register for KMAC/SHA3 operations with sparse-encoded commands.
 * Commands: 0x1D=START, 0x2E=PROCESS, 0x31=RUN, 0x16=DONE
 * R0W1C access - write 1 triggers action and bit self-clears
 * Reset value: 0x00000000
 * Access: RW (R0W1C for command bits)
 */
template<unsigned int N>
class CMD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    /**
     * @brief Constructor for CMD register
     * @param reg_name Register name string
     * @param memory Reference to register memory
     * @param offset Register offset address (0x18)
     */
    CMD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x73f, 0x0),
      cmd(reg_name + ".cmd", *this, 0, 6),
      reserved0(reg_name + ".reserved0", *this, 6, 2),
      entropy_req(reg_name + ".entropy_req", *this, 8, 1),
      hash_cnt_clr(reg_name + ".hash_cnt_clr", *this, 9, 1),
      err_processed(reg_name + ".err_processed", *this, 10, 1),
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

    csml_bitfield<N> cmd;            ///< Bits[5:0]: Sparse command (0x1D=start 0x2E=process 0x31=run 0x16=done)
    csml_bitfield<N> reserved0;      ///< Bits[7:6]: Reserved
    csml_bitfield<N> entropy_req;    ///< Bit[8]: Manual entropy request trigger
    csml_bitfield<N> hash_cnt_clr;   ///< Bit[9]: Clear entropy refresh hash counter
    csml_bitfield<N> err_processed;  ///< Bit[10]: Error processed - reset FSM from error state
    csml_bitfield<N> reserved1;      ///< Bits[31:11]: Reserved
};

/**
 * @class STATUS_type
 * @brief KMAC/SHA3 Status Register (offset 0x1C)
 *
 * Dynamically updated status register reflecting FSM state, FIFO status, and alerts.
 * All fields are read-only and updated by hardware.
 * Reset value: 0x00004001 (sha3_idle=1, fifo_empty=1)
 * Access: RO
 */
template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;

    /**
     * @brief Constructor for STATUS register
     * @param reg_name Register name string
     * @param memory Reference to register memory
     * @param offset Register offset address (0x1C)
     */
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3df07, 0x0, 0x4001),
      sha3_idle(reg_name + ".sha3_idle", *this, 0, 1),
      sha3_absorb(reg_name + ".sha3_absorb", *this, 1, 1),
      sha3_squeeze(reg_name + ".sha3_squeeze", *this, 2, 1),
      reserved0(reg_name + ".reserved0", *this, 3, 5),
      fifo_depth(reg_name + ".fifo_depth", *this, 8, 5),
      reserved1(reg_name + ".reserved1", *this, 13, 1),
      fifo_empty(reg_name + ".fifo_empty", *this, 14, 1),
      fifo_full(reg_name + ".fifo_full", *this, 15, 1),
      ALERT_FATAL_FAULT(reg_name + ".ALERT_FATAL_FAULT", *this, 16, 1),
      ALERT_RECOV_CTRL_UPDATE_ERR(reg_name + ".ALERT_RECOV_CTRL_UPDATE_ERR", *this, 17, 1),
      reserved2(reg_name + ".reserved2", *this, 18, 14)
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

    csml_bitfield<N> sha3_idle;                    ///< Bit[0]: SHA3 hashing engine in idle state
    csml_bitfield<N> sha3_absorb;                  ///< Bit[1]: SHA3 receiving and processing message stream
    csml_bitfield<N> sha3_squeeze;                 ///< Bit[2]: SHA3 completed absorbing, digest available
    csml_bitfield<N> reserved0;                    ///< Bits[7:3]: Reserved
    csml_bitfield<N> fifo_depth;                   ///< Bits[12:8]: Message FIFO occupied entry count
    csml_bitfield<N> reserved1;                    ///< Bit[13]: Reserved
    csml_bitfield<N> fifo_empty;                   ///< Bit[14]: Message FIFO empty indicator
    csml_bitfield<N> fifo_full;                    ///< Bit[15]: Message FIFO full indicator
    csml_bitfield<N> ALERT_FATAL_FAULT;            ///< Bit[16]: Fatal fault occurred - requires reset
    csml_bitfield<N> ALERT_RECOV_CTRL_UPDATE_ERR;  ///< Bit[17]: Shadowed register update error - recoverable
    csml_bitfield<N> reserved2;                    ///< Bits[31:18]: Reserved
};

template<unsigned int N>
class ENTROPY_PERIOD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ENTROPY_PERIOD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffff03ff, 0xffff03ff, 0x0),
      prescaler(reg_name + ".prescaler", *this, 0, 10), 
      reserved0(reg_name + ".reserved0", *this, 10, 6), 
      wait_timer(reg_name + ".wait_timer", *this, 16, 16)
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
    csml_bitfield<N> prescaler;
    csml_bitfield<N> reserved0;
    csml_bitfield<N> wait_timer;
};

template<unsigned int N>
class ENTROPY_REFRESH_HASH_CNT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ENTROPY_REFRESH_HASH_CNT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3ff, 0x0),
      hash_cnt(reg_name + ".hash_cnt", *this, 0, 10), 
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
    csml_bitfield<N> hash_cnt;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class ENTROPY_REFRESH_THRESHOLD_SHADOWED_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ENTROPY_REFRESH_THRESHOLD_SHADOWED_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3ff, 0x3ff, 0x0),
      threshold(reg_name + ".threshold", *this, 0, 10), 
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
    csml_bitfield<N> threshold;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class ENTROPY_SEED_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ENTROPY_SEED_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      seed(reg_name + ".seed", *this, 0, 32)
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
    csml_bitfield<N> seed;
};

template<unsigned int N>
class KEY_SHARE0_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    KEY_SHARE0_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      key(reg_name + ".key", *this, 0, 32)
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
    csml_bitfield<N> key;
};

template<unsigned int N>
class KEY_SHARE1_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    KEY_SHARE1_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      key(reg_name + ".key", *this, 0, 32)
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
    csml_bitfield<N> key;
};

template<unsigned int N>
class KEY_LEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    KEY_LEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x7, 0x0),
      len(reg_name + ".len", *this, 0, 3), 
      reserved0(reg_name + ".reserved0", *this, 3, 29)
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
    csml_bitfield<N> len;
    csml_bitfield<N> reserved0;
};

template<unsigned int N>
class PREFIX_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    PREFIX_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      prefix(reg_name + ".prefix", *this, 0, 32)
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
    csml_bitfield<N> prefix;
};

template<unsigned int N>
class ERR_CODE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERR_CODE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      err_code(reg_name + ".err_code", *this, 0, 32)
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
    csml_bitfield<N> err_code;
};

/**
 * @class STATE_type
 * @brief Keccak State Memory Window (offset 0x400, 128x32-bit words)
 *
 * Read-only memory window for reading Keccak state (1600 bits = 50x32-bit words).
 * Array size: 128 words (512 bytes).
 * Reset value: 0x00000000
 * Access: RO
 */
template<unsigned int N>
class STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0x0),
      state(reg_name + ".state", *this, 0, 32)
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
    csml_bitfield<N> state;
};

/**
 * @class MSG_FIFO_type
 * @brief Message FIFO Memory Window (offset 0x800, 512x32-bit words)
 *
 * Write-only memory window for writing message data to KMAC FIFO.
 * Array size: 512 words (2048 bytes).
 * Reset value: 0x00000000
 * Access: WO
 */
template<unsigned int N>
class MSG_FIFO_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MSG_FIFO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      data(reg_name + ".data", *this, 0, 32)
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
    csml_bitfield<N> data;
};

}