/**
 * @file dma_base.cpp
 * @brief DMA Controller base register infrastructure implementation
 *
 * Implements reset functionality for all DMA Controller registers.
 */

#include "dma_base.h"

/**
 * @brief Resets all DMA Controller registers to default values
 *
 * This function resets all registers including:
 * - Interrupt control registers to 0x00000000
 * - Address registers to 0x00000000
 * - ADDR_SPACE_ID to 0x00000077 (both ASIDs default to OT_ADDR)
 * - RANGE_REGWEN to 0x00000006 (unlocked)
 * - CFG_REGWEN to 0x00000006 (unlocked/idle)
 * - TRANSFER_WIDTH to 0x00000002 (FOUR_BYTE default)
 * - HANDSHAKE_INTR_ENABLE to 0x000007FF (all 11 triggers enabled)
 * - All SHA-2 digest registers to 0x00000000
 * - All interrupt source address and value arrays to 0x00000000
 */
void dma_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  SRC_ADDR_LO.reset();
  SRC_ADDR_HI.reset();
  DST_ADDR_LO.reset();
  DST_ADDR_HI.reset();
  ADDR_SPACE_ID.reset();
  ENABLED_MEMORY_RANGE_BASE.reset();
  ENABLED_MEMORY_RANGE_LIMIT.reset();
  RANGE_VALID.reset();
  RANGE_REGWEN.reset();
  CFG_REGWEN.reset();
  TOTAL_DATA_SIZE.reset();
  CHUNK_DATA_SIZE.reset();
  TRANSFER_WIDTH.reset();
  CONTROL.reset();
  SRC_CONFIG.reset();
  DST_CONFIG.reset();
  STATUS.reset();
  ERROR_CODE.reset();
  for (size_t i = 0; i < 16; i++) {
    SHA2_DIGEST[i].reset();
  }
  HANDSHAKE_INTR_ENABLE.reset();
  CLEAR_INTR_SRC.reset();
  CLEAR_INTR_BUS.reset();
  for (size_t i = 0; i < 11; i++) {
    INTR_SRC_ADDR[i].reset();
  }
  for (size_t i = 0; i < 11; i++) {
    INTR_SRC_WR_VAL[i].reset();
  }
}