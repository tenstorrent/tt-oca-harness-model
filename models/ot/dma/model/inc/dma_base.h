/**
 * @file dma_base.h
 * @brief DMA Controller base register infrastructure class
 *
 * This file defines the base class containing all DMA Controller registers,
 * register arrays, memory infrastructure, and TLM target socket for register access.
 */

#pragma once
#include "dma_register.h"
#include <string.h>

/**
 * @class dma_base
 * @brief Base class providing DMA Controller register infrastructure
 *
 * This class instantiates all DMA Controller registers including:
 * - Interrupt control registers (INTR_STATE, INTR_ENABLE, INTR_TEST)
 * - Alert test register (ALERT_TEST)
 * - Address configuration (SRC_ADDR_LO/HI, DST_ADDR_LO/HI, ADDR_SPACE_ID)
 * - Memory range security (ENABLED_MEMORY_RANGE_BASE/LIMIT, RANGE_VALID)
 * - Transfer configuration (TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH)
 * - Control and status (CONTROL, STATUS, ERROR_CODE)
 * - SHA-2 digest output array (SHA2_DIGEST[0-15])
 * - Hardware handshake configuration
 * - Interrupt source address and write value arrays
 *
 * Provides TLM target socket for register access and manages register memory.
 */
class dma_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;

    /**
     * @brief Constructor for DMA base register infrastructure
     * @param name SystemC module name
     * @param memory_size Total size of register address space in bytes (default calculated from register map)
     *
     * Initializes all DMA registers with their reset values and configures
     * the TLM target socket for register access transactions.
     */
    dma_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x0 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x4 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x8 + 0x00)/sizeof(unsigned int)), 
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0xC + 0x00)/sizeof(unsigned int)), 
       SRC_ADDR_LO(std::string(name) + ".SRC_ADDR_LO", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       SRC_ADDR_HI(std::string(name) + ".SRC_ADDR_HI", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       DST_ADDR_LO(std::string(name) + ".DST_ADDR_LO", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       DST_ADDR_HI(std::string(name) + ".DST_ADDR_HI", memory, (0x1C + 0x00)/sizeof(unsigned int)), 
       ADDR_SPACE_ID(std::string(name) + ".ADDR_SPACE_ID", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       ENABLED_MEMORY_RANGE_BASE(std::string(name) + ".ENABLED_MEMORY_RANGE_BASE", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       ENABLED_MEMORY_RANGE_LIMIT(std::string(name) + ".ENABLED_MEMORY_RANGE_LIMIT", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       RANGE_VALID(std::string(name) + ".RANGE_VALID", memory, (0x2C + 0x00)/sizeof(unsigned int)), 
       RANGE_REGWEN(std::string(name) + ".RANGE_REGWEN", memory, (0x30 + 0x00)/sizeof(unsigned int)), 
       CFG_REGWEN(std::string(name) + ".CFG_REGWEN", memory, (0x34 + 0x00)/sizeof(unsigned int)), 
       TOTAL_DATA_SIZE(std::string(name) + ".TOTAL_DATA_SIZE", memory, (0x38 + 0x00)/sizeof(unsigned int)), 
       CHUNK_DATA_SIZE(std::string(name) + ".CHUNK_DATA_SIZE", memory, (0x3C + 0x00)/sizeof(unsigned int)), 
       TRANSFER_WIDTH(std::string(name) + ".TRANSFER_WIDTH", memory, (0x40 + 0x00)/sizeof(unsigned int)), 
       CONTROL(std::string(name) + ".CONTROL", memory, (0x44 + 0x00)/sizeof(unsigned int)), 
       SRC_CONFIG(std::string(name) + ".SRC_CONFIG", memory, (0x48 + 0x00)/sizeof(unsigned int)), 
       DST_CONFIG(std::string(name) + ".DST_CONFIG", memory, (0x4C + 0x00)/sizeof(unsigned int)), 
       STATUS(std::string(name) + ".STATUS", memory, (0x50 + 0x00)/sizeof(unsigned int)), 
       ERROR_CODE(std::string(name) + ".ERROR_CODE", memory, (0x54 + 0x00)/sizeof(unsigned int)), 
       SHA2_DIGEST(std::string(name) + ".SHA2_DIGEST", memory, (0x58 + 0x00)/sizeof(unsigned int), 1), 
       HANDSHAKE_INTR_ENABLE(std::string(name) + ".HANDSHAKE_INTR_ENABLE", memory, (0x98 + 0x00)/sizeof(unsigned int)), 
       CLEAR_INTR_SRC(std::string(name) + ".CLEAR_INTR_SRC", memory, (0x9C + 0x00)/sizeof(unsigned int)), 
       CLEAR_INTR_BUS(std::string(name) + ".CLEAR_INTR_BUS", memory, (0xA0 + 0x00)/sizeof(unsigned int)), 
       INTR_SRC_ADDR(std::string(name) + ".INTR_SRC_ADDR", memory, (0xA4 + 0x00)/sizeof(unsigned int), 1), 
       INTR_SRC_WR_VAL(std::string(name) + ".INTR_SRC_WR_VAL", memory, (0x124 + 0x00)/sizeof(unsigned int), 1)
       {
         memory.bind_to_socket(target_socket);
       }

      /// Register memory storage (32-bit word width)
      csml_memory<32> memory;

      /// TLM target socket for register access (32-bit data width)
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;


      /// Interrupt State Register (offset 0x00, RO) - Reflects current interrupt status
      dma::INTR_STATE_type<32> INTR_STATE;

      /// Interrupt Enable Register (offset 0x04, RW) - Masks for interrupt signals
      dma::INTR_ENABLE_type<32> INTR_ENABLE;

      /// Interrupt Test Register (offset 0x08, WO) - Forces interrupt state bits for testing
      dma::INTR_TEST_type<32> INTR_TEST;

      /// Alert Test Register (offset 0x0C, WO) - Triggers fatal_fault alert
      dma::ALERT_TEST_type<32> ALERT_TEST;

      /// Source Address Low Register (offset 0x10, RW) - Lower 32 bits of source address
      dma::SRC_ADDR_LO_type<32> SRC_ADDR_LO;

      /// Source Address High Register (offset 0x14, RW) - Upper 32 bits of source address
      dma::SRC_ADDR_HI_type<32> SRC_ADDR_HI;

      /// Destination Address Low Register (offset 0x18, RW) - Lower 32 bits of destination address
      dma::DST_ADDR_LO_type<32> DST_ADDR_LO;

      /// Destination Address High Register (offset 0x1C, RW) - Upper 32 bits of destination address
      dma::DST_ADDR_HI_type<32> DST_ADDR_HI;

      /// Address Space ID Register (offset 0x20, RW) - Source and destination ASID configuration (reset 0x77)
      dma::ADDR_SPACE_ID_type<32> ADDR_SPACE_ID;

      /// Enabled Memory Range Base Register (offset 0x24, RW) - DMA-enabled memory range start address
      dma::ENABLED_MEMORY_RANGE_BASE_type<32> ENABLED_MEMORY_RANGE_BASE;

      /// Enabled Memory Range Limit Register (offset 0x28, RW) - DMA-enabled memory range end address
      dma::ENABLED_MEMORY_RANGE_LIMIT_type<32> ENABLED_MEMORY_RANGE_LIMIT;

      /// Range Valid Register (offset 0x2C, RW) - Indicates memory range configuration validity
      dma::RANGE_VALID_type<32> RANGE_VALID;

      /// Range Register Write Enable (offset 0x30, RW, W0C) - Software lock for memory range registers (reset 0x6)
      dma::RANGE_REGWEN_type<32> RANGE_REGWEN;

      /// Configuration Register Write Enable (offset 0x34, RO) - Hardware-managed lock reflecting DMA busy state (reset 0x6)
      dma::CFG_REGWEN_type<32> CFG_REGWEN;

      /// Total Data Size Register (offset 0x38, RW) - Total transfer size in bytes
      dma::TOTAL_DATA_SIZE_type<32> TOTAL_DATA_SIZE;

      /// Chunk Data Size Register (offset 0x3C, RW) - Chunk size in bytes for handshake/multi-chunk transfers
      dma::CHUNK_DATA_SIZE_type<32> CHUNK_DATA_SIZE;

      /// Transfer Width Register (offset 0x40, RW) - Transaction width configuration (reset 0x2 = 4 bytes)
      dma::TRANSFER_WIDTH_type<32> TRANSFER_WIDTH;

      /// Control Register (offset 0x44, Mixed) - DMA control with go, abort, opcode, and mode bits
      dma::CONTROL_type<32> CONTROL;

      /// Source Config Register (offset 0x48, RW) - Source addressing mode (increment, wrap)
      dma::SRC_CONFIG_type<32> SRC_CONFIG;

      /// Destination Config Register (offset 0x4C, RW) - Destination addressing mode (increment, wrap)
      dma::DST_CONFIG_type<32> DST_CONFIG;

      /// Status Register (offset 0x50, Mixed) - DMA status with busy, done, error, digest_valid flags
      dma::STATUS_type<32> STATUS;

      /// Error Code Register (offset 0x54, RO) - Error source indicators cleared by STATUS.error
      dma::ERROR_CODE_type<32> ERROR_CODE;

      /// SHA-2 Digest Array (offset 0x58-0x94, RO) - 16x32-bit words for SHA-256/384/512 output
      csml_reg_vector<dma::SHA2_DIGEST_type<32>, 16> SHA2_DIGEST;

      /// Handshake Interrupt Enable Register (offset 0x98, RW) - Enable mask for 11 lsio_trigger inputs (reset 0x7FF)
      dma::HANDSHAKE_INTR_ENABLE_type<32> HANDSHAKE_INTR_ENABLE;

      /// Clear Interrupt Source Register (offset 0x9C, RW) - Enable automatic interrupt clearing per source
      dma::CLEAR_INTR_SRC_type<32> CLEAR_INTR_SRC;

      /// Clear Interrupt Bus Register (offset 0xA0, RW) - Bus selection for interrupt clearing (0=CTN/SYS, 1=OT)
      dma::CLEAR_INTR_BUS_type<32> CLEAR_INTR_BUS;

      /// Interrupt Source Address Array (offset 0xA4-0xCC, RW) - 11 addresses for interrupt clearing writes
      csml_reg_vector<dma::INTR_SRC_ADDR_type<32>, 11> INTR_SRC_ADDR;

      /// Interrupt Source Write Value Array (offset 0x124-0x14C, RW) - 11 write values for interrupt clearing
      csml_reg_vector<dma::INTR_SRC_WR_VAL_type<32>, 11> INTR_SRC_WR_VAL;

      /**
       * @brief Resets all DMA registers to their default values
       *
       * This method restores all registers to their power-on reset values as specified
       * in the register map. Should be called during system reset or initialization.
       */
      void reset_all_registers();
};
