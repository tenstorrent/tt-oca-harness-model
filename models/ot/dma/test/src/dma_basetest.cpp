/**
 * @file dma_basetest.cpp
 * @brief DMA Controller base test infrastructure implementation
 *
 * Implements the register property map for DMA Controller test validation.
 */

#include "dma_basetest.h"

/**
 * @brief Register property map for DMA Controller
 *
 * This array contains all register properties (offset, read mask, write mask,
 * reset value, name) for all 28 DMA Controller registers. Used by test cases
 * to validate register access behavior and reset values.
 */
dma_basetest::Register_Property_t reg_map[28] = {
{dma_basetest::INTR_STATE_OFFSET, dma_basetest::INTR_STATE_READ, dma_basetest::INTR_STATE_WRITE, dma_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{dma_basetest::INTR_ENABLE_OFFSET, dma_basetest::INTR_ENABLE_READ, dma_basetest::INTR_ENABLE_WRITE, dma_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{dma_basetest::INTR_TEST_OFFSET, dma_basetest::INTR_TEST_READ, dma_basetest::INTR_TEST_WRITE, dma_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{dma_basetest::ALERT_TEST_OFFSET, dma_basetest::ALERT_TEST_READ, dma_basetest::ALERT_TEST_WRITE, dma_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{dma_basetest::SRC_ADDR_LO_OFFSET, dma_basetest::SRC_ADDR_LO_READ, dma_basetest::SRC_ADDR_LO_WRITE, dma_basetest::SRC_ADDR_LO_RESET, "SRC_ADDR_LO"}, 
{dma_basetest::SRC_ADDR_HI_OFFSET, dma_basetest::SRC_ADDR_HI_READ, dma_basetest::SRC_ADDR_HI_WRITE, dma_basetest::SRC_ADDR_HI_RESET, "SRC_ADDR_HI"}, 
{dma_basetest::DST_ADDR_LO_OFFSET, dma_basetest::DST_ADDR_LO_READ, dma_basetest::DST_ADDR_LO_WRITE, dma_basetest::DST_ADDR_LO_RESET, "DST_ADDR_LO"}, 
{dma_basetest::DST_ADDR_HI_OFFSET, dma_basetest::DST_ADDR_HI_READ, dma_basetest::DST_ADDR_HI_WRITE, dma_basetest::DST_ADDR_HI_RESET, "DST_ADDR_HI"}, 
{dma_basetest::ADDR_SPACE_ID_OFFSET, dma_basetest::ADDR_SPACE_ID_READ, dma_basetest::ADDR_SPACE_ID_WRITE, dma_basetest::ADDR_SPACE_ID_RESET, "ADDR_SPACE_ID"}, 
{dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, dma_basetest::ENABLED_MEMORY_RANGE_BASE_READ, dma_basetest::ENABLED_MEMORY_RANGE_BASE_WRITE, dma_basetest::ENABLED_MEMORY_RANGE_BASE_RESET, "ENABLED_MEMORY_RANGE_BASE"}, 
{dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_READ, dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_WRITE, dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_RESET, "ENABLED_MEMORY_RANGE_LIMIT"}, 
{dma_basetest::RANGE_VALID_OFFSET, dma_basetest::RANGE_VALID_READ, dma_basetest::RANGE_VALID_WRITE, dma_basetest::RANGE_VALID_RESET, "RANGE_VALID"}, 
{dma_basetest::RANGE_REGWEN_OFFSET, dma_basetest::RANGE_REGWEN_READ, dma_basetest::RANGE_REGWEN_WRITE, dma_basetest::RANGE_REGWEN_RESET, "RANGE_REGWEN"}, 
{dma_basetest::CFG_REGWEN_OFFSET, dma_basetest::CFG_REGWEN_READ, dma_basetest::CFG_REGWEN_WRITE, dma_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"}, 
{dma_basetest::TOTAL_DATA_SIZE_OFFSET, dma_basetest::TOTAL_DATA_SIZE_READ, dma_basetest::TOTAL_DATA_SIZE_WRITE, dma_basetest::TOTAL_DATA_SIZE_RESET, "TOTAL_DATA_SIZE"}, 
{dma_basetest::CHUNK_DATA_SIZE_OFFSET, dma_basetest::CHUNK_DATA_SIZE_READ, dma_basetest::CHUNK_DATA_SIZE_WRITE, dma_basetest::CHUNK_DATA_SIZE_RESET, "CHUNK_DATA_SIZE"}, 
{dma_basetest::TRANSFER_WIDTH_OFFSET, dma_basetest::TRANSFER_WIDTH_READ, dma_basetest::TRANSFER_WIDTH_WRITE, dma_basetest::TRANSFER_WIDTH_RESET, "TRANSFER_WIDTH"}, 
{dma_basetest::CONTROL_OFFSET, dma_basetest::CONTROL_READ, dma_basetest::CONTROL_WRITE, dma_basetest::CONTROL_RESET, "CONTROL"}, 
{dma_basetest::SRC_CONFIG_OFFSET, dma_basetest::SRC_CONFIG_READ, dma_basetest::SRC_CONFIG_WRITE, dma_basetest::SRC_CONFIG_RESET, "SRC_CONFIG"}, 
{dma_basetest::DST_CONFIG_OFFSET, dma_basetest::DST_CONFIG_READ, dma_basetest::DST_CONFIG_WRITE, dma_basetest::DST_CONFIG_RESET, "DST_CONFIG"}, 
{dma_basetest::STATUS_OFFSET, dma_basetest::STATUS_READ, dma_basetest::STATUS_WRITE, dma_basetest::STATUS_RESET, "STATUS"}, 
{dma_basetest::ERROR_CODE_OFFSET, dma_basetest::ERROR_CODE_READ, dma_basetest::ERROR_CODE_WRITE, dma_basetest::ERROR_CODE_RESET, "ERROR_CODE"}, 
{dma_basetest::SHA2_DIGEST_OFFSET, dma_basetest::SHA2_DIGEST_READ, dma_basetest::SHA2_DIGEST_WRITE, dma_basetest::SHA2_DIGEST_RESET, "SHA2_DIGEST"}, 
{dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, dma_basetest::HANDSHAKE_INTR_ENABLE_READ, dma_basetest::HANDSHAKE_INTR_ENABLE_WRITE, dma_basetest::HANDSHAKE_INTR_ENABLE_RESET, "HANDSHAKE_INTR_ENABLE"}, 
{dma_basetest::CLEAR_INTR_SRC_OFFSET, dma_basetest::CLEAR_INTR_SRC_READ, dma_basetest::CLEAR_INTR_SRC_WRITE, dma_basetest::CLEAR_INTR_SRC_RESET, "CLEAR_INTR_SRC"}, 
{dma_basetest::CLEAR_INTR_BUS_OFFSET, dma_basetest::CLEAR_INTR_BUS_READ, dma_basetest::CLEAR_INTR_BUS_WRITE, dma_basetest::CLEAR_INTR_BUS_RESET, "CLEAR_INTR_BUS"}, 
{dma_basetest::INTR_SRC_ADDR_OFFSET, dma_basetest::INTR_SRC_ADDR_READ, dma_basetest::INTR_SRC_ADDR_WRITE, dma_basetest::INTR_SRC_ADDR_RESET, "INTR_SRC_ADDR"}, 
{dma_basetest::INTR_SRC_WR_VAL_OFFSET, dma_basetest::INTR_SRC_WR_VAL_READ, dma_basetest::INTR_SRC_WR_VAL_WRITE, dma_basetest::INTR_SRC_WR_VAL_RESET, "INTR_SRC_WR_VAL"}};