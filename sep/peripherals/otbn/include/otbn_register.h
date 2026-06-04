/**
 * @file otbn_register.h
 * @brief OTBN hardware register type definitions
 * 
 * This header defines all OTBN hardware register types using CSML register templates.
 * Each register class includes:
 * - Bitfield accessors for register fields
 * - Reset values and access permissions
 * - Memory-mapped backing store integration
 * 
 * Register types defined:
 * - INTR_STATE, INTR_ENABLE, INTR_TEST - Interrupt control registers
 * - ALERT_TEST - Alert testing register
 * - CMD - Command execution register
 * - CTRL - Configuration control register
 * - STATUS - Current state status register
 * - ERR_BITS - Error bit accumulator
 * - FATAL_ALERT_CAUSE - Fatal error cause register
 * - INSN_CNT - Instruction count register
 * - LOAD_CHECKSUM - CRC-32-IEEE checksum register
 * - IMEM - Instruction memory register array (2048 words)
 * - DMEM - Data memory register array (768 words host-accessible)
 * 
 * All register types are defined within the otbn namespace.
 */


#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

/// @brief OTBN register type namespace - contains all hardware register class definitions
namespace otbn {

/**
 * @brief Interrupt State Register (INTR_STATE)
 * 
 * W1C (Write-1-to-Clear) register for interrupt status.
 * Bit 0: done - Set when operation completes, cleared by writing 1
 */
template<unsigned int N>
class INTR_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0),
      done(reg_name + ".done", *this, 0, 1), 
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
    csml_bitfield<N> done;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Interrupt Enable Register (INTR_ENABLE)
 * 
 * Controls interrupt masking.
 * Bit 0: done - Enable/disable done interrupt
 */
template<unsigned int N>
class INTR_ENABLE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_ENABLE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0),
      done(reg_name + ".done", *this, 0, 1), 
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
    csml_bitfield<N> done;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Interrupt Test Register (INTR_TEST)
 * 
 * Write-only register to test interrupt generation.
 * Writing 1 to bit 0 sets INTR_STATE.done
 */
template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x1, 0),
      done(reg_name + ".done", *this, 0, 1), 
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
    csml_bitfield<N> done;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Alert Test Register (ALERT_TEST)
 * 
 * Write-only register to test alert outputs.
 * Bit 0: fatal - Trigger fatal alert
 * Bit 1: recov - Trigger recoverable alert
 */
template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0x3, 0),
      fatal(reg_name + ".fatal", *this, 0, 1), 
      recov(reg_name + ".recov", *this, 1, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 2, 30)
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
    csml_bitfield<N> fatal;
    csml_bitfield<N> recov;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Command Register (CMD)
 * 
 * Write-only register to execute commands. Valid only when STATUS == IDLE.
 * Command values: 0xd8 (EXECUTE), 0xc3 (SEC_WIPE_DMEM), 0x1e (SEC_WIPE_IMEM)
 */
template<unsigned int N>
class CMD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CMD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xff, 0),
      cmd(reg_name + ".cmd", *this, 0, 8), 
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
    csml_bitfield<N> cmd;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Control Register (CTRL)
 * 
 * Configuration register. Writable only when IDLE.
 * Bit 0: software_errs_fatal - If set, software errors cause fatal alert and LOCKED state
 */
template<unsigned int N>
class CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0),
      software_errs_fatal(reg_name + ".software_errs_fatal", *this, 0, 1), 
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
    csml_bitfield<N> software_errs_fatal;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Status Register (STATUS)
 * 
 * Read-only register indicating current OTBN state.
 * Values: 0x00 (IDLE), 0x01 (BUSY_EXECUTE), 0x02-0x04 (BUSY_SEC_WIPE), 0xFF (LOCKED)
 */
template<unsigned int N>
class STATUS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    STATUS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 4),
      status(reg_name + ".status", *this, 0, 8), 
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
    csml_bitfield<N> status;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Error Bits Register (ERR_BITS)
 * 
 * Sticky error flags. W1C when IDLE or LOCKED.
 * Bits 0-7: Software errors (bad_data_addr, key_invalid, rnd failures, etc.)
 * Bits 16-23: Fatal errors (integrity violations, illegal_bus_access, lifecycle_escalation, fatal_software)
 */
template<unsigned int N>
class ERR_BITS_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    ERR_BITS_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff00ff, 0xff00ff, 0),
      bad_data_addr(reg_name + ".bad_data_addr", *this, 0, 1), 
      bad_insn_addr(reg_name + ".bad_insn_addr", *this, 1, 1), 
      call_stack(reg_name + ".call_stack", *this, 2, 1), 
      illegal_insn(reg_name + ".illegal_insn", *this, 3, 1), 
      loop(reg_name + ".loop", *this, 4, 1), 
      key_invalid(reg_name + ".key_invalid", *this, 5, 1), 
      rnd_rep_chk_fail(reg_name + ".rnd_rep_chk_fail", *this, 6, 1), 
      rnd_fips_chk_fail(reg_name + ".rnd_fips_chk_fail", *this, 7, 1), 
      Reserved0(reg_name + ".Reserved0", *this, 8, 8), 
      imem_intg_violation(reg_name + ".imem_intg_violation", *this, 16, 1), 
      dmem_intg_violation(reg_name + ".dmem_intg_violation", *this, 17, 1), 
      reg_intg_violation(reg_name + ".reg_intg_violation", *this, 18, 1), 
      bus_intg_violation(reg_name + ".bus_intg_violation", *this, 19, 1), 
      bad_internal_state(reg_name + ".bad_internal_state", *this, 20, 1), 
      illegal_bus_access(reg_name + ".illegal_bus_access", *this, 21, 1), 
      lifecycle_escalation(reg_name + ".lifecycle_escalation", *this, 22, 1), 
      fatal_software(reg_name + ".fatal_software", *this, 23, 1), 
      Reserved1(reg_name + ".Reserved1", *this, 24, 8)
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
    csml_bitfield<N> bad_data_addr;
    csml_bitfield<N> bad_insn_addr;
    csml_bitfield<N> call_stack;
    csml_bitfield<N> illegal_insn;
    csml_bitfield<N> loop;
    csml_bitfield<N> key_invalid;
    csml_bitfield<N> rnd_rep_chk_fail;
    csml_bitfield<N> rnd_fips_chk_fail;
    csml_bitfield<N> Reserved0;
    csml_bitfield<N> imem_intg_violation;
    csml_bitfield<N> dmem_intg_violation;
    csml_bitfield<N> reg_intg_violation;
    csml_bitfield<N> bus_intg_violation;
    csml_bitfield<N> bad_internal_state;
    csml_bitfield<N> illegal_bus_access;
    csml_bitfield<N> lifecycle_escalation;
    csml_bitfield<N> fatal_software;
    csml_bitfield<N> Reserved1;
};

/**
 * @brief Fatal Alert Cause Register (FATAL_ALERT_CAUSE)
 * 
 * Read-only register containing the persisted fatal error cause.
 * Set when fatal alert is triggered, persists until reset.
 */
template<unsigned int N>
class FATAL_ALERT_CAUSE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    FATAL_ALERT_CAUSE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xff, 0x0, 0),
      imem_intg_violation(reg_name + ".imem_intg_violation", *this, 0, 1), 
      dmem_intg_violation(reg_name + ".dmem_intg_violation", *this, 1, 1), 
      reg_intg_violation(reg_name + ".reg_intg_violation", *this, 2, 1), 
      bus_intg_violation(reg_name + ".bus_intg_violation", *this, 3, 1), 
      bad_internal_state(reg_name + ".bad_internal_state", *this, 4, 1), 
      illegal_bus_access(reg_name + ".illegal_bus_access", *this, 5, 1), 
      lifecycle_escalation(reg_name + ".lifecycle_escalation", *this, 6, 1), 
      fatal_software(reg_name + ".fatal_software", *this, 7, 1), 
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
    csml_bitfield<N> imem_intg_violation;
    csml_bitfield<N> dmem_intg_violation;
    csml_bitfield<N> reg_intg_violation;
    csml_bitfield<N> bus_intg_violation;
    csml_bitfield<N> bad_internal_state;
    csml_bitfield<N> illegal_bus_access;
    csml_bitfield<N> lifecycle_escalation;
    csml_bitfield<N> fatal_software;
    csml_bitfield<N> Reserved0;
};

/**
 * @brief Instruction Count Register (INSN_CNT)
 * 
 * Read-only register containing the instruction count from last execution.
 * Updated after algorithm completes. Can be cleared when IDLE or LOCKED.
 */
template<unsigned int N>
class INSN_CNT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    INSN_CNT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      insn_cnt(reg_name + ".insn_cnt", *this, 0, 32)
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
    csml_bitfield<N> insn_cnt;
};

/**
 * @brief Load Checksum Register (LOAD_CHECKSUM)
 * 
 * CRC-32-IEEE checksum of IMEM and DMEM writes.
 * Updated on each memory write. Can be written to reset accumulator.
 */
template<unsigned int N>
class LOAD_CHECKSUM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    LOAD_CHECKSUM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
      checksum(reg_name + ".checksum", *this, 0, 32)
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
    csml_bitfield<N> checksum;
};

/**
 * @brief Instruction Memory Register Type (IMEM)
 * 
 * 32-bit word of instruction memory. Array of 2048 words (8KB total).
 * Accessible only when IDLE. Writes update LOAD_CHECKSUM.
 */
template<unsigned int N>
class IMEM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    IMEM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
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

/**
 * @brief Data Memory Register Type (DMEM)
 * 
 * 32-bit word of data memory. Array of 768 words (3KB host-accessible).
 * Full DMEM is 4KB but only first 3KB accessible via register interface.
 * Accessible only when IDLE. Writes update LOAD_CHECKSUM.
 */
template<unsigned int N>
class DMEM_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    DMEM_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0),
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