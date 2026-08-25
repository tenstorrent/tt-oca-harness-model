// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file hmac_register.h
 * @brief HMAC register type definitions using CSML register framework
 * 
 * This file defines all register types for the HMAC IP block, including
 * interrupt control, configuration, command, status, and data registers.
 * All registers are templated on bit width and use the CSML register framework.
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

namespace hmac {

/**
 * @brief Interrupt State Register
 * @tparam N Register bit width
 * 
 * This register contains the current state of interrupt signals.
 * Interrupts are write-1-to-clear (W1C) for hmac_done and hmac_err.
 */
template<unsigned int N>
class INTR_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x5, 0),
      hmac_done(reg_name + ".hmac_done", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      hmac_err(reg_name + ".hmac_err", *this, 2, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 29)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // HMAC operation completion interrupt (bit 0)
    csml_bitfield<N> hmac_done;
    // FIFO empty interrupt (bit 1)
    csml_bitfield<N> fifo_empty;
    // HMAC error interrupt (bit 2)
    csml_bitfield<N> hmac_err;
    // Reserved bits (bits 3-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Interrupt Enable Register
 * @tparam N Register bit width
 * 
 * This register controls which interrupts are enabled.
 * Only enabled interrupts will assert the corresponding output port.
 */
template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7, 0x7, 0),
      hmac_done(reg_name + ".hmac_done", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      hmac_err(reg_name + ".hmac_err", *this, 2, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 29)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Enable HMAC done interrupt (bit 0)
    csml_bitfield<N> hmac_done;
    // Enable FIFO empty interrupt (bit 1)
    csml_bitfield<N> fifo_empty;
    // Enable HMAC error interrupt (bit 2)
    csml_bitfield<N> hmac_err;
    // Reserved bits (bits 3-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Interrupt Test Register
 * @tparam N Register bit width
 * 
 * This register allows software to force interrupt assertion for testing.
 * Writing 1 to a bit forces the corresponding interrupt in INTR_STATE.
 */
template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x7, 0),
      hmac_done(reg_name + ".hmac_done", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      hmac_err(reg_name + ".hmac_err", *this, 2, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 29)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Force hmac_done interrupt (bit 0)
    csml_bitfield<N> hmac_done;
    // Force fifo_empty interrupt (bit 1)
    csml_bitfield<N> fifo_empty;
    // Force hmac_err interrupt (bit 2)
    csml_bitfield<N> hmac_err;
    // Reserved bits (bits 3-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Alert Test Register
 * @tparam N Register bit width
 * 
 * This register allows software to trigger alert signals for testing.
 * Writing 1 to fatal_fault asserts the alert_fatal_fault output.
 */
template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x1, 0),
      fatal_fault(reg_name + ".fatal_fault", *this, 0, 1), 
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
    
    // Trigger fatal fault alert (bit 0)
    csml_bitfield<N> fatal_fault;
    // Reserved bits (bits 1-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Configuration Register
 * @tparam N Register bit width
 * 
 * This register configures the HMAC/SHA-2 operation mode, algorithm selection,
 * and byte ordering options. Only writable when the engine is idle.
 */
template<unsigned int N>
class CFG_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    CFG_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x7fff, 0x7fff, 16640),
      hmac_en(reg_name + ".hmac_en", *this, 0, 1), 
      sha_en(reg_name + ".sha_en", *this, 1, 1), 
      endian_swap(reg_name + ".endian_swap", *this, 2, 1), 
      digest_swap(reg_name + ".digest_swap", *this, 3, 1), 
      key_swap(reg_name + ".key_swap", *this, 4, 1), 
      digest_size(reg_name + ".digest_size", *this, 5, 4), 
      key_length(reg_name + ".key_length", *this, 9, 6), 
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
    
    // Enable HMAC mode (bit 0)
    csml_bitfield<N> hmac_en;
    // Enable SHA-2 engine (bit 1)
    csml_bitfield<N> sha_en;
    // Swap byte order for message data (bit 2)
    csml_bitfield<N> endian_swap;
    // Swap byte order for digest output (bit 3)
    csml_bitfield<N> digest_swap;
    // Swap byte order for key input (bit 4)
    csml_bitfield<N> key_swap;
    // Digest size selection: 0x1=SHA-256, 0x2=SHA-384, 0x4=SHA-512 (bits 5-8)
    csml_bitfield<N> digest_size;
    // Key length selection: 0x1=128b, 0x2=256b, 0x4=384b, 0x8=512b, 0x10=1024b (bits 9-14)
    csml_bitfield<N> key_length;
    // Reserved bits (bits 15-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Command Register
 * @tparam N Register bit width
 * 
 * This register controls HMAC/SHA-2 operations. Commands are write-1-to-trigger
 * and self-clear. Reading always returns 0.
 */
template<unsigned int N>
class CMD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    CMD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xf, 0),
      hash_start(reg_name + ".hash_start", *this, 0, 1), 
      hash_process(reg_name + ".hash_process", *this, 1, 1), 
      hash_stop(reg_name + ".hash_stop", *this, 2, 1), 
      hash_continue(reg_name + ".hash_continue", *this, 3, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 4, 28)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Start hash operation (bit 0)
    csml_bitfield<N> hash_start;
    // Process remaining data and finalize (bit 1)
    csml_bitfield<N> hash_process;
    // Stop current operation and save context (bit 2)
    csml_bitfield<N> hash_stop;
    // Continue from saved context (bit 3)
    csml_bitfield<N> hash_continue;
    // Reserved bits (bits 4-31)
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Status Register
 * @tparam N Register bit width
 * 
 * This read-only register reports the current state of the HMAC engine
 * and message FIFO. Values are computed dynamically.
 */
template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x3f7, 0x0, 3),
      hmac_idle(reg_name + ".hmac_idle", *this, 0, 1), 
      fifo_empty(reg_name + ".fifo_empty", *this, 1, 1), 
      fifo_full(reg_name + ".fifo_full", *this, 2, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 3, 1), 
      fifo_depth(reg_name + ".fifo_depth", *this, 4, 6), 
      Reserved1(reg_name + ".Reserved1", *this, 10, 22)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Engine is idle (bit 0)
    csml_bitfield<N> hmac_idle;
    // Message FIFO is empty (bit 1)
    csml_bitfield<N> fifo_empty;
    // Message FIFO is full (bit 2)
    csml_bitfield<N> fifo_full;
    // Reserved bit (bit 3)
    csml_bitfield<N> Reserved0;
    // Current FIFO depth in words (bits 4-9)
    csml_bitfield<N> fifo_depth;
    // Reserved bits (bits 10-31)
    csml_bitfield<N> Reserved1;
};

/**
 * @brief Error Code Register
 * @tparam N Register bit width
 * 
 * This read-only register contains the error code when an error occurs.
 * Error codes are set when invalid operations or configurations are detected.
 */
template<unsigned int N>
class ERR_CODE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    ERR_CODE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0x0, 0),
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
    
    // Error code value (bits 0-31)
    csml_bitfield<N> err_code;
};

/**
 * @brief Wipe Secret Register
 * @tparam N Register bit width
 * 
 * Writing to this register wipes sensitive data (keys and digests) with
 * the provided value. The register itself does not store the value.
 */
template<unsigned int N>
class WIPE_SECRET_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    WIPE_SECRET_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0),
      secret(reg_name + ".secret", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Wipe value to write to sensitive data (bits 0-31)
    csml_bitfield<N> secret;
};

/**
 * @brief Key Register
 * @tparam N Register bit width
 * 
 * Write-only register for storing HMAC key material. There are 32 KEY registers
 * (KEY[0] through KEY[31]) allowing up to 1024 bits of key data.
 * Only writable when the engine is idle.
 */
template<unsigned int N>
class KEY_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    KEY_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0),
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
    
    // Key data word (bits 0-31)
    csml_bitfield<N> key;
};

/**
 * @brief Digest Register
 * @tparam N Register bit width
 * 
 * Read/write register for storing the computed HMAC/SHA-2 digest.
 * There are 16 DIGEST registers (DIGEST[0] through DIGEST[15]) allowing
 * up to 512 bits of digest data. Only writable when the engine is idle.
 */
template<unsigned int N>
class DIGEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    DIGEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      digest(reg_name + ".digest", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Digest data word (bits 0-31)
    csml_bitfield<N> digest;
};

/**
 * @brief Message Length Lower Register
 * @tparam N Register bit width
 * 
 * Lower 32 bits of the message length in bits. Only writable when the engine is idle.
 * Automatically updated as message data is written to MSG_FIFO.
 */
template<unsigned int N>
class MSG_LENGTH_LOWER_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    MSG_LENGTH_LOWER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      v(reg_name + ".v", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Lower 32 bits of message length in bits (bits 0-31)
    csml_bitfield<N> v;
};

/**
 * @brief Message Length Upper Register
 * @tparam N Register bit width
 * 
 * Upper 32 bits of the message length in bits. Only writable when the engine is idle.
 * Automatically updated as message data is written to MSG_FIFO.
 */
template<unsigned int N>
class MSG_LENGTH_UPPER_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    MSG_LENGTH_UPPER_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      v(reg_name + ".v", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    
    // Upper 32 bits of message length in bits (bits 0-31)
    csml_bitfield<N> v;
};

/**
 * @brief Message FIFO Register
 * @tparam N Register bit width
 * 
 * Write-only register for pushing message data into the HMAC/SHA-2 engine.
 * There are 1024 MSG_FIFO registers, but they all map to the same logical FIFO.
 * Supports byte-level writes via byte enable signals.
 */
template<unsigned int N>
class MSG_FIFO_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    
    MSG_FIFO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0),
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
    
    // Message data word (bits 0-31)
    csml_bitfield<N> data;
};


}