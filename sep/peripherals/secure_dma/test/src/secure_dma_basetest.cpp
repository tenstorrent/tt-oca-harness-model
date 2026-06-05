/**
 * @file secure_dma_basetest.cpp
 * @brief DMA Controller base test infrastructure implementation
 *
 * Implements the register property map for DMA Controller test validation.
 */

#include "secure_dma_basetest.h"

/**
 * @brief Register property map for DMA Controller
 *
 * This array contains all register properties (offset, read mask, write mask,
 * reset value, name) for all 28 DMA Controller registers. Used by test cases
 * to validate register access behavior and reset values.
 */
secure_dma_basetest::Register_Property_t reg_map[28] = {
{secure_dma_basetest::INTR_STATE_OFFSET, secure_dma_basetest::INTR_STATE_READ, secure_dma_basetest::INTR_STATE_WRITE, secure_dma_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{secure_dma_basetest::INTR_ENABLE_OFFSET, secure_dma_basetest::INTR_ENABLE_READ, secure_dma_basetest::INTR_ENABLE_WRITE, secure_dma_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{secure_dma_basetest::INTR_TEST_OFFSET, secure_dma_basetest::INTR_TEST_READ, secure_dma_basetest::INTR_TEST_WRITE, secure_dma_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{secure_dma_basetest::ALERT_TEST_OFFSET, secure_dma_basetest::ALERT_TEST_READ, secure_dma_basetest::ALERT_TEST_WRITE, secure_dma_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{secure_dma_basetest::SRC_ADDR_LO_OFFSET, secure_dma_basetest::SRC_ADDR_LO_READ, secure_dma_basetest::SRC_ADDR_LO_WRITE, secure_dma_basetest::SRC_ADDR_LO_RESET, "SRC_ADDR_LO"}, 
{secure_dma_basetest::SRC_ADDR_HI_OFFSET, secure_dma_basetest::SRC_ADDR_HI_READ, secure_dma_basetest::SRC_ADDR_HI_WRITE, secure_dma_basetest::SRC_ADDR_HI_RESET, "SRC_ADDR_HI"}, 
{secure_dma_basetest::DST_ADDR_LO_OFFSET, secure_dma_basetest::DST_ADDR_LO_READ, secure_dma_basetest::DST_ADDR_LO_WRITE, secure_dma_basetest::DST_ADDR_LO_RESET, "DST_ADDR_LO"}, 
{secure_dma_basetest::DST_ADDR_HI_OFFSET, secure_dma_basetest::DST_ADDR_HI_READ, secure_dma_basetest::DST_ADDR_HI_WRITE, secure_dma_basetest::DST_ADDR_HI_RESET, "DST_ADDR_HI"}, 
{secure_dma_basetest::ADDR_SPACE_ID_OFFSET, secure_dma_basetest::ADDR_SPACE_ID_READ, secure_dma_basetest::ADDR_SPACE_ID_WRITE, secure_dma_basetest::ADDR_SPACE_ID_RESET, "ADDR_SPACE_ID"}, 
{secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_READ, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_WRITE, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_RESET, "ENABLED_MEMORY_RANGE_BASE"}, 
{secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_READ, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_WRITE, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_RESET, "ENABLED_MEMORY_RANGE_LIMIT"}, 
{secure_dma_basetest::RANGE_VALID_OFFSET, secure_dma_basetest::RANGE_VALID_READ, secure_dma_basetest::RANGE_VALID_WRITE, secure_dma_basetest::RANGE_VALID_RESET, "RANGE_VALID"}, 
{secure_dma_basetest::RANGE_REGWEN_OFFSET, secure_dma_basetest::RANGE_REGWEN_READ, secure_dma_basetest::RANGE_REGWEN_WRITE, secure_dma_basetest::RANGE_REGWEN_RESET, "RANGE_REGWEN"}, 
{secure_dma_basetest::CFG_REGWEN_OFFSET, secure_dma_basetest::CFG_REGWEN_READ, secure_dma_basetest::CFG_REGWEN_WRITE, secure_dma_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"}, 
{secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, secure_dma_basetest::TOTAL_DATA_SIZE_READ, secure_dma_basetest::TOTAL_DATA_SIZE_WRITE, secure_dma_basetest::TOTAL_DATA_SIZE_RESET, "TOTAL_DATA_SIZE"}, 
{secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, secure_dma_basetest::CHUNK_DATA_SIZE_READ, secure_dma_basetest::CHUNK_DATA_SIZE_WRITE, secure_dma_basetest::CHUNK_DATA_SIZE_RESET, "CHUNK_DATA_SIZE"}, 
{secure_dma_basetest::TRANSFER_WIDTH_OFFSET, secure_dma_basetest::TRANSFER_WIDTH_READ, secure_dma_basetest::TRANSFER_WIDTH_WRITE, secure_dma_basetest::TRANSFER_WIDTH_RESET, "TRANSFER_WIDTH"}, 
{secure_dma_basetest::CONTROL_OFFSET, secure_dma_basetest::CONTROL_READ, secure_dma_basetest::CONTROL_WRITE, secure_dma_basetest::CONTROL_RESET, "CONTROL"}, 
{secure_dma_basetest::SRC_CONFIG_OFFSET, secure_dma_basetest::SRC_CONFIG_READ, secure_dma_basetest::SRC_CONFIG_WRITE, secure_dma_basetest::SRC_CONFIG_RESET, "SRC_CONFIG"}, 
{secure_dma_basetest::DST_CONFIG_OFFSET, secure_dma_basetest::DST_CONFIG_READ, secure_dma_basetest::DST_CONFIG_WRITE, secure_dma_basetest::DST_CONFIG_RESET, "DST_CONFIG"}, 
{secure_dma_basetest::STATUS_OFFSET, secure_dma_basetest::STATUS_READ, secure_dma_basetest::STATUS_WRITE, secure_dma_basetest::STATUS_RESET, "STATUS"}, 
{secure_dma_basetest::ERROR_CODE_OFFSET, secure_dma_basetest::ERROR_CODE_READ, secure_dma_basetest::ERROR_CODE_WRITE, secure_dma_basetest::ERROR_CODE_RESET, "ERROR_CODE"}, 
{secure_dma_basetest::SHA2_DIGEST_OFFSET, secure_dma_basetest::SHA2_DIGEST_READ, secure_dma_basetest::SHA2_DIGEST_WRITE, secure_dma_basetest::SHA2_DIGEST_RESET, "SHA2_DIGEST"}, 
{secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_READ, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_WRITE, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_RESET, "HANDSHAKE_INTR_ENABLE"}, 
{secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, secure_dma_basetest::CLEAR_INTR_SRC_READ, secure_dma_basetest::CLEAR_INTR_SRC_WRITE, secure_dma_basetest::CLEAR_INTR_SRC_RESET, "CLEAR_INTR_SRC"}, 
{secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, secure_dma_basetest::CLEAR_INTR_BUS_READ, secure_dma_basetest::CLEAR_INTR_BUS_WRITE, secure_dma_basetest::CLEAR_INTR_BUS_RESET, "CLEAR_INTR_BUS"}, 
{secure_dma_basetest::INTR_SRC_ADDR_OFFSET, secure_dma_basetest::INTR_SRC_ADDR_READ, secure_dma_basetest::INTR_SRC_ADDR_WRITE, secure_dma_basetest::INTR_SRC_ADDR_RESET, "INTR_SRC_ADDR"}, 
{secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET, secure_dma_basetest::INTR_SRC_WR_VAL_READ, secure_dma_basetest::INTR_SRC_WR_VAL_WRITE, secure_dma_basetest::INTR_SRC_WR_VAL_RESET, "INTR_SRC_WR_VAL"}};