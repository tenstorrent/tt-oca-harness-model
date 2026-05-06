/**
 * @file edn_register.h
 * @brief EDN (Entropy Distribution Network) register definitions and structures
 *
 * This file contains CSML-generated register type definitions for the EDN IP.
 * It defines all 17 registers with their bitfield structures for the Entropy
 * Distribution Network module, which distributes entropy from CSRNG to up to
 * 8 peripheral endpoints.
 *
 * Register Categories:
 * - Interrupt Control: INTR_STATE, INTR_ENABLE, INTR_TEST
 * - Alert Testing: ALERT_TEST
 * - Configuration: REGWEN, CTRL (multi-bit encoded fields)
 * - Boot-Time Commands: BOOT_INS_CMD, BOOT_GEN_CMD
 * - Software Interface: SW_CMD_REQ, SW_CMD_STS
 * - Hardware Status: HW_CMD_STS
 * - Auto Request Mode: RESEED_CMD, GENERATE_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS
 * - Error Reporting: RECOV_ALERT_STS, ERR_CODE, ERR_CODE_TEST
 * - State Observation: MAIN_SM_STATE
 *
 * Address Range: 0x00 - 0x44
 * Register Width: 32 bits
 * Memory Template: 32-bit access
 *
 * @note All register types are templated on memory width (N) for flexibility.
 * @note Reserved fields are included for hardware compatibility.
 */

#pragma once
#include "csml_register.h"
#include <iostream>
#include <systemc.h>

namespace edn {

template <unsigned int N> class INTR_STATE_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  INTR_STATE_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x3, 0x3, 0x00000000),
        edn_cmd_req_done(reg_name + ".edn_cmd_req_done", *this, 0, 1),
        edn_fatal_err(reg_name + ".edn_fatal_err", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 30) {
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
  csml_bitfield<N> edn_cmd_req_done;
  csml_bitfield<N> edn_fatal_err;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class INTR_ENABLE_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  INTR_ENABLE_type(std::string reg_name, memory_type &memory,
                   unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x3, 0x3, 0x00000000),
        edn_cmd_req_done(reg_name + ".edn_cmd_req_done", *this, 0, 1),
        edn_fatal_err(reg_name + ".edn_fatal_err", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 30) {
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
  csml_bitfield<N> edn_cmd_req_done;
  csml_bitfield<N> edn_fatal_err;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class INTR_TEST_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0x3, 0x00000000),
        edn_cmd_req_done(reg_name + ".edn_cmd_req_done", *this, 0, 1),
        edn_fatal_err(reg_name + ".edn_fatal_err", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 30) {
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
  csml_bitfield<N> edn_cmd_req_done;
  csml_bitfield<N> edn_fatal_err;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class ALERT_TEST_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  ALERT_TEST_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0x3, 0x00000000),
        recov_alert(reg_name + ".recov_alert", *this, 0, 1),
        fatal_alert(reg_name + ".fatal_alert", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 30) {
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
  csml_bitfield<N> recov_alert;
  csml_bitfield<N> fatal_alert;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class REGWEN_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x00000001),
        REGWEN(reg_name + ".REGWEN", *this, 0, 1),
        reserved0(reg_name + ".reserved0", *this, 1, 31) {
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
  csml_bitfield<N> REGWEN;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class CTRL_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffff, 0xffff, 0x00009999),
        EDN_ENABLE(reg_name + ".EDN_ENABLE", *this, 0, 4),
        BOOT_REQ_MODE(reg_name + ".BOOT_REQ_MODE", *this, 4, 4),
        AUTO_REQ_MODE(reg_name + ".AUTO_REQ_MODE", *this, 8, 4),
        CMD_FIFO_RST(reg_name + ".CMD_FIFO_RST", *this, 12, 4),
        reserved0(reg_name + ".reserved0", *this, 16, 16) {
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
  csml_bitfield<N> EDN_ENABLE;
  csml_bitfield<N> BOOT_REQ_MODE;
  csml_bitfield<N> AUTO_REQ_MODE;
  csml_bitfield<N> CMD_FIFO_RST;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class BOOT_INS_CMD_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  BOOT_INS_CMD_type(std::string reg_name, memory_type &memory,
                    unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff,
                    0x00000901),
        BOOT_INS_CMD(reg_name + ".BOOT_INS_CMD", *this, 0, 32) {
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
  csml_bitfield<N> BOOT_INS_CMD;
};

template <unsigned int N> class BOOT_GEN_CMD_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  BOOT_GEN_CMD_type(std::string reg_name, memory_type &memory,
                    unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff,
                    0x00FFF003),
        BOOT_GEN_CMD(reg_name + ".BOOT_GEN_CMD", *this, 0, 32) {
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
  csml_bitfield<N> BOOT_GEN_CMD;
};

template <unsigned int N> class SW_CMD_REQ_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  SW_CMD_REQ_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x00000000),
        SW_CMD_REQ(reg_name + ".SW_CMD_REQ", *this, 0, 32) {
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
  csml_bitfield<N> SW_CMD_REQ;
};

template <unsigned int N> class SW_CMD_STS_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  SW_CMD_STS_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x3f, 0x0, 0x00000000),
        CMD_REG_RDY(reg_name + ".CMD_REG_RDY", *this, 0, 1),
        CMD_RDY(reg_name + ".CMD_RDY", *this, 1, 1),
        CMD_ACK(reg_name + ".CMD_ACK", *this, 2, 1),
        CMD_STS(reg_name + ".CMD_STS", *this, 3, 3),
        reserved0(reg_name + ".reserved0", *this, 6, 26) {
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
  csml_bitfield<N> CMD_REG_RDY;
  csml_bitfield<N> CMD_RDY;
  csml_bitfield<N> CMD_ACK;
  csml_bitfield<N> CMD_STS;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class HW_CMD_STS_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  HW_CMD_STS_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x3ff, 0x0, 0x00000000),
        BOOT_MODE(reg_name + ".BOOT_MODE", *this, 0, 1),
        AUTO_MODE(reg_name + ".AUTO_MODE", *this, 1, 1),
        CMD_TYPE(reg_name + ".CMD_TYPE", *this, 2, 4),
        CMD_ACK(reg_name + ".CMD_ACK", *this, 6, 1),
        CMD_STS(reg_name + ".CMD_STS", *this, 7, 3),
        reserved0(reg_name + ".reserved0", *this, 10, 22) {
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
  csml_bitfield<N> BOOT_MODE;
  csml_bitfield<N> AUTO_MODE;
  csml_bitfield<N> CMD_TYPE;
  csml_bitfield<N> CMD_ACK;
  csml_bitfield<N> CMD_STS;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class RESEED_CMD_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  RESEED_CMD_type(std::string reg_name, memory_type &memory,
                  unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x00000000),
        RESEED_CMD(reg_name + ".RESEED_CMD", *this, 0, 32) {
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
  csml_bitfield<N> RESEED_CMD;
};

template <unsigned int N> class GENERATE_CMD_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  GENERATE_CMD_type(std::string reg_name, memory_type &memory,
                    unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x00000000),
        GENERATE_CMD(reg_name + ".GENERATE_CMD", *this, 0, 32) {
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
  csml_bitfield<N> GENERATE_CMD;
};

template <unsigned int N>
class MAX_NUM_REQS_BETWEEN_RESEEDS_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  MAX_NUM_REQS_BETWEEN_RESEEDS_type(std::string reg_name, memory_type &memory,
                                    unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff,
                    0x00000000),
        MAX_NUM_REQS_BETWEEN_RESEEDS(reg_name + ".MAX_NUM_REQS_BETWEEN_RESEEDS",
                                     *this, 0, 32) {
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
  csml_bitfield<N> MAX_NUM_REQS_BETWEEN_RESEEDS;
};

template <unsigned int N> class RECOV_ALERT_STS_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  RECOV_ALERT_STS_type(std::string reg_name, memory_type &memory,
                       unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x300f, 0x300f, 0x00000000),
        EDN_ENABLE_FIELD_ALERT(reg_name + ".EDN_ENABLE_FIELD_ALERT", *this, 0,
                               1),
        BOOT_REQ_MODE_FIELD_ALERT(reg_name + ".BOOT_REQ_MODE_FIELD_ALERT",
                                  *this, 1, 1),
        AUTO_REQ_MODE_FIELD_ALERT(reg_name + ".AUTO_REQ_MODE_FIELD_ALERT",
                                  *this, 2, 1),
        CMD_FIFO_RST_FIELD_ALERT(reg_name + ".CMD_FIFO_RST_FIELD_ALERT", *this,
                                 3, 1),
        reserved0(reg_name + ".reserved0", *this, 4, 8),
        EDN_BUS_CMP_ALERT(reg_name + ".EDN_BUS_CMP_ALERT", *this, 12, 1),
        CSRNG_ACK_ERR(reg_name + ".CSRNG_ACK_ERR", *this, 13, 1),
        reserved1(reg_name + ".reserved1", *this, 14, 18) {
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
  csml_bitfield<N> EDN_ENABLE_FIELD_ALERT;
  csml_bitfield<N> BOOT_REQ_MODE_FIELD_ALERT;
  csml_bitfield<N> AUTO_REQ_MODE_FIELD_ALERT;
  csml_bitfield<N> CMD_FIFO_RST_FIELD_ALERT;
  csml_bitfield<N> reserved0;
  csml_bitfield<N> EDN_BUS_CMP_ALERT;
  csml_bitfield<N> CSRNG_ACK_ERR;
  csml_bitfield<N> reserved1;
};

template <unsigned int N> class ERR_CODE_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  ERR_CODE_type(std::string reg_name, memory_type &memory, unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x70700003, 0x0, 0x00000000),
        SFIFO_RESCMD_ERR(reg_name + ".SFIFO_RESCMD_ERR", *this, 0, 1),
        SFIFO_GENCMD_ERR(reg_name + ".SFIFO_GENCMD_ERR", *this, 1, 1),
        reserved0(reg_name + ".reserved0", *this, 2, 18),
        EDN_ACK_SM_ERR(reg_name + ".EDN_ACK_SM_ERR", *this, 20, 1),
        EDN_MAIN_SM_ERR(reg_name + ".EDN_MAIN_SM_ERR", *this, 21, 1),
        EDN_CNTR_ERR(reg_name + ".EDN_CNTR_ERR", *this, 22, 1),
        reserved1(reg_name + ".reserved1", *this, 23, 5),
        FIFO_WRITE_ERR(reg_name + ".FIFO_WRITE_ERR", *this, 28, 1),
        FIFO_READ_ERR(reg_name + ".FIFO_READ_ERR", *this, 29, 1),
        FIFO_STATE_ERR(reg_name + ".FIFO_STATE_ERR", *this, 30, 1),
        reserved2(reg_name + ".reserved2", *this, 31, 1) {
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
  csml_bitfield<N> SFIFO_RESCMD_ERR;
  csml_bitfield<N> SFIFO_GENCMD_ERR;
  csml_bitfield<N> reserved0;
  csml_bitfield<N> EDN_ACK_SM_ERR;
  csml_bitfield<N> EDN_MAIN_SM_ERR;
  csml_bitfield<N> EDN_CNTR_ERR;
  csml_bitfield<N> reserved1;
  csml_bitfield<N> FIFO_WRITE_ERR;
  csml_bitfield<N> FIFO_READ_ERR;
  csml_bitfield<N> FIFO_STATE_ERR;
  csml_bitfield<N> reserved2;
};

template <unsigned int N> class ERR_CODE_TEST_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  ERR_CODE_TEST_type(std::string reg_name, memory_type &memory,
                     unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1f, 0x1f, 0x00000000),
        ERR_CODE_TEST(reg_name + ".ERR_CODE_TEST", *this, 0, 5),
        reserved0(reg_name + ".reserved0", *this, 5, 27) {
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
  csml_bitfield<N> ERR_CODE_TEST;
  csml_bitfield<N> reserved0;
};

template <unsigned int N> class MAIN_SM_STATE_type : public csml_reg<N> {
public:
  using typename csml_reg<N>::memory_type;
  typedef typename csml_word<N>::wordtype DT;
  MAIN_SM_STATE_type(std::string reg_name, memory_type &memory,
                     unsigned int offset)
      : csml_reg<N>(reg_name, memory, offset, 0x1ff, 0x0, 0x000000C1),
        MAIN_SM_STATE(reg_name + ".MAIN_SM_STATE", *this, 0, 9),
        reserved0(reg_name + ".reserved0", *this, 9, 23) {
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
  csml_bitfield<N> MAIN_SM_STATE;
  csml_bitfield<N> reserved0;
};

} // namespace edn