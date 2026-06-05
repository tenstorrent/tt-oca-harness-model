/**
 * @file secure_dma_basetest.h
 * @brief DMA Controller base test infrastructure
 *
 * This file defines base test infrastructure including register offsets,
 * access masks, reset values, and TLM initiator socket for test transactions.
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class secure_dma_basetest
 * @brief Base class for DMA Controller test infrastructure
 *
 * Provides essential test infrastructure including:
 * - TLM initiator socket for register transactions
 * - Register offset enumerations
 * - Register access mask enumerations (read/write permissions)
 * - Register reset value enumerations
 * - Register property structure for test validation
 *
 * This class serves as the foundation for all DMA Controller test cases.
 */
class secure_dma_basetest : public sc_module
{
  public:
    /// TLM initiator socket for sending register access transactions (32-bit data width)
    tlm_utils::simple_initiator_socket<secure_dma_basetest, 32> initiator_socket;

    /**
     * @enum Register_offset
     * @brief Register offset addresses in DMA Controller address map
     */
    enum Register_offset
    {
      INTR_STATE_OFFSET = (0x0 + 0x00), 
      INTR_ENABLE_OFFSET = (0x4 + 0x00), 
      INTR_TEST_OFFSET = (0x8 + 0x00), 
      ALERT_TEST_OFFSET = (0xC + 0x00), 
      SRC_ADDR_LO_OFFSET = (0x10 + 0x00), 
      SRC_ADDR_HI_OFFSET = (0x14 + 0x00), 
      DST_ADDR_LO_OFFSET = (0x18 + 0x00), 
      DST_ADDR_HI_OFFSET = (0x1C + 0x00), 
      ADDR_SPACE_ID_OFFSET = (0x20 + 0x00), 
      ENABLED_MEMORY_RANGE_BASE_OFFSET = (0x24 + 0x00), 
      ENABLED_MEMORY_RANGE_LIMIT_OFFSET = (0x28 + 0x00), 
      RANGE_VALID_OFFSET = (0x2C + 0x00), 
      RANGE_REGWEN_OFFSET = (0x30 + 0x00), 
      CFG_REGWEN_OFFSET = (0x34 + 0x00), 
      TOTAL_DATA_SIZE_OFFSET = (0x38 + 0x00), 
      CHUNK_DATA_SIZE_OFFSET = (0x3C + 0x00), 
      TRANSFER_WIDTH_OFFSET = (0x40 + 0x00), 
      CONTROL_OFFSET = (0x44 + 0x00), 
      SRC_CONFIG_OFFSET = (0x48 + 0x00), 
      DST_CONFIG_OFFSET = (0x4C + 0x00), 
      STATUS_OFFSET = (0x50 + 0x00), 
      ERROR_CODE_OFFSET = (0x54 + 0x00), 
      SHA2_DIGEST_OFFSET = (0x58 + 0x00), 
      HANDSHAKE_INTR_ENABLE_OFFSET = (0x98 + 0x00), 
      CLEAR_INTR_SRC_OFFSET = (0x9C + 0x00), 
      CLEAR_INTR_BUS_OFFSET = (0xA0 + 0x00), 
      INTR_SRC_ADDR_OFFSET = (0xA4 + 0x00), 
      INTR_SRC_WR_VAL_OFFSET = (0x124 + 0x00)  
    };

    /**
     * @enum Register_Read_Access
     * @brief Read access masks for DMA Controller registers
     *
     * Defines which bits are readable for each register.
     * 0xffffffff = all bits readable
     * 0x0 = write-only register (reads return 0 or undefined)
     */
    enum Register_Read_Access
    {
      INTR_STATE_READ = (0xffffffff), 
      INTR_ENABLE_READ = (0xffffffff), 
      INTR_TEST_READ = (0x0), 
      ALERT_TEST_READ = (0x0), 
      SRC_ADDR_LO_READ = (0xffffffff), 
      SRC_ADDR_HI_READ = (0xffffffff), 
      DST_ADDR_LO_READ = (0xffffffff), 
      DST_ADDR_HI_READ = (0xffffffff), 
      ADDR_SPACE_ID_READ = (0xffffffff), 
      ENABLED_MEMORY_RANGE_BASE_READ = (0xffffffff), 
      ENABLED_MEMORY_RANGE_LIMIT_READ = (0xffffffff), 
      RANGE_VALID_READ = (0xffffffff), 
      RANGE_REGWEN_READ = (0xffffffff), 
      CFG_REGWEN_READ = (0xffffffff), 
      TOTAL_DATA_SIZE_READ = (0xffffffff), 
      CHUNK_DATA_SIZE_READ = (0xffffffff), 
      TRANSFER_WIDTH_READ = (0xffffffff), 
      CONTROL_READ = (0xf7ffffff), 
      SRC_CONFIG_READ = (0xffffffff), 
      DST_CONFIG_READ = (0xffffffff), 
      STATUS_READ = (0xffffffff), 
      ERROR_CODE_READ = (0xffffffff), 
      SHA2_DIGEST_READ = (0xffffffff), 
      HANDSHAKE_INTR_ENABLE_READ = (0xffffffff), 
      CLEAR_INTR_SRC_READ = (0xffffffff), 
      CLEAR_INTR_BUS_READ = (0xffffffff), 
      INTR_SRC_ADDR_READ = (0xffffffff), 
      INTR_SRC_WR_VAL_READ = (0xffffffff)  
    };

    /**
     * @enum Register_Write_Access
     * @brief Write access masks for DMA Controller registers
     *
     * Defines which bits are writable for each register.
     * 0xffffffff = all bits writable
     * 0x0 = read-only register (writes ignored)
     * Other values = mixed access (some bits RW, some RO/WO/W1C)
     */
    enum Register_Write_Access
    {
      INTR_STATE_WRITE = (0x0), 
      INTR_ENABLE_WRITE = (0xffffffff), 
      INTR_TEST_WRITE = (0xffffffff), 
      ALERT_TEST_WRITE = (0xffffffff), 
      SRC_ADDR_LO_WRITE = (0xffffffff), 
      SRC_ADDR_HI_WRITE = (0xffffffff), 
      DST_ADDR_LO_WRITE = (0xffffffff), 
      DST_ADDR_HI_WRITE = (0xffffffff), 
      ADDR_SPACE_ID_WRITE = (0xffffffff), 
      ENABLED_MEMORY_RANGE_BASE_WRITE = (0xffffffff), 
      ENABLED_MEMORY_RANGE_LIMIT_WRITE = (0xffffffff), 
      RANGE_VALID_WRITE = (0xffffffff), 
      RANGE_REGWEN_WRITE = (0xffffffff), 
      CFG_REGWEN_WRITE = (0x0), 
      TOTAL_DATA_SIZE_WRITE = (0xffffffff), 
      CHUNK_DATA_SIZE_WRITE = (0xffffffff), 
      TRANSFER_WIDTH_WRITE = (0xffffffff), 
      CONTROL_WRITE = (0xffffffff), 
      SRC_CONFIG_WRITE = (0xffffffff), 
      DST_CONFIG_WRITE = (0xffffffff), 
      STATUS_WRITE = (0xffffffee), 
      ERROR_CODE_WRITE = (0x0), 
      SHA2_DIGEST_WRITE = (0x0), 
      HANDSHAKE_INTR_ENABLE_WRITE = (0xffffffff), 
      CLEAR_INTR_SRC_WRITE = (0xffffffff), 
      CLEAR_INTR_BUS_WRITE = (0xffffffff), 
      INTR_SRC_ADDR_WRITE = (0xffffffff), 
      INTR_SRC_WR_VAL_WRITE = (0xffffffff)
    };

    /**
     * @enum Register_Reset_Val
     * @brief Reset values for DMA Controller registers
     *
     * Defines the power-on reset value for each register.
     * Most registers reset to 0x00000000.
     * Notable exceptions:
     * - ADDR_SPACE_ID: 0x00000077 (both ASIDs = OT_ADDR)
     * - RANGE_REGWEN: 0x00000006 (unlocked)
     * - CFG_REGWEN: 0x00000006 (unlocked/idle)
     * - TRANSFER_WIDTH: 0x00000002 (FOUR_BYTE default)
     * - HANDSHAKE_INTR_ENABLE: 0x000007FF (all triggers enabled)
     */
    enum Register_Reset_Val
    {
      INTR_STATE_RESET = (0x00000000), 
      INTR_ENABLE_RESET = (0x00000000), 
      INTR_TEST_RESET = (0x00000000), 
      ALERT_TEST_RESET = (0x00000000), 
      SRC_ADDR_LO_RESET = (0x00000000), 
      SRC_ADDR_HI_RESET = (0x00000000), 
      DST_ADDR_LO_RESET = (0x00000000), 
      DST_ADDR_HI_RESET = (0x00000000), 
      ADDR_SPACE_ID_RESET = (0x00000077), 
      ENABLED_MEMORY_RANGE_BASE_RESET = (0x00000000), 
      ENABLED_MEMORY_RANGE_LIMIT_RESET = (0x00000000), 
      RANGE_VALID_RESET = (0x00000000), 
      RANGE_REGWEN_RESET = (0x00000006), 
      CFG_REGWEN_RESET = (0x00000006), 
      TOTAL_DATA_SIZE_RESET = (0x00000000), 
      CHUNK_DATA_SIZE_RESET = (0x00000000), 
      TRANSFER_WIDTH_RESET = (0x00000002), 
      CONTROL_RESET = (0x00000000), 
      SRC_CONFIG_RESET = (0x00000000), 
      DST_CONFIG_RESET = (0x00000000), 
      STATUS_RESET = (0x00000000), 
      ERROR_CODE_RESET = (0x00000000), 
      SHA2_DIGEST_RESET = (0x00000000), 
      HANDSHAKE_INTR_ENABLE_RESET = (0x000007FF), 
      CLEAR_INTR_SRC_RESET = (0x00000000), 
      CLEAR_INTR_BUS_RESET = (0x00000000), 
      INTR_SRC_ADDR_RESET = (0x00000000), 
      INTR_SRC_WR_VAL_RESET = (0x00000000)
    };
     
    /**
     * @struct Register_Property_t
     * @brief Structure containing all properties of a DMA register
     *
     * Used for test validation and verification of register behavior.
     */
    struct Register_Property_t
    {
		  unsigned int reg_offset;   ///< Register offset address
		  unsigned int read_mask;    ///< Read access mask (readable bits)
		  unsigned int write_mask;   ///< Write access mask (writable bits)
		  unsigned int reg_reset;    ///< Power-on reset value
		  std::string reg_name;      ///< Register name string
    };

    /**
     * @brief Constructor for DMA base test infrastructure
     * @param name SystemC module name
     *
     * Initializes the test infrastructure and TLM initiator socket.
     */
    secure_dma_basetest(sc_module_name name) : sc_module(name)
    {

    }

};