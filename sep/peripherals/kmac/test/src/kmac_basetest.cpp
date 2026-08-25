// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "kmac_basetest.h"

kmac_basetest::Register_Property_t reg_map[19] = {
{kmac_basetest::INTR_STATE_OFFSET, kmac_basetest::INTR_STATE_READ, kmac_basetest::INTR_STATE_WRITE, kmac_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{kmac_basetest::INTR_ENABLE_OFFSET, kmac_basetest::INTR_ENABLE_READ, kmac_basetest::INTR_ENABLE_WRITE, kmac_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{kmac_basetest::INTR_TEST_OFFSET, kmac_basetest::INTR_TEST_READ, kmac_basetest::INTR_TEST_WRITE, kmac_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{kmac_basetest::ALERT_TEST_OFFSET, kmac_basetest::ALERT_TEST_READ, kmac_basetest::ALERT_TEST_WRITE, kmac_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{kmac_basetest::CFG_REGWEN_OFFSET, kmac_basetest::CFG_REGWEN_READ, kmac_basetest::CFG_REGWEN_WRITE, kmac_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"}, 
{kmac_basetest::CFG_SHADOWED_OFFSET, kmac_basetest::CFG_SHADOWED_READ, kmac_basetest::CFG_SHADOWED_WRITE, kmac_basetest::CFG_SHADOWED_RESET, "CFG_SHADOWED"}, 
{kmac_basetest::CMD_OFFSET, kmac_basetest::CMD_READ, kmac_basetest::CMD_WRITE, kmac_basetest::CMD_RESET, "CMD"}, 
{kmac_basetest::STATUS_OFFSET, kmac_basetest::STATUS_READ, kmac_basetest::STATUS_WRITE, kmac_basetest::STATUS_RESET, "STATUS"}, 
{kmac_basetest::ENTROPY_PERIOD_OFFSET, kmac_basetest::ENTROPY_PERIOD_READ, kmac_basetest::ENTROPY_PERIOD_WRITE, kmac_basetest::ENTROPY_PERIOD_RESET, "ENTROPY_PERIOD"}, 
{kmac_basetest::ENTROPY_REFRESH_HASH_CNT_OFFSET, kmac_basetest::ENTROPY_REFRESH_HASH_CNT_READ, kmac_basetest::ENTROPY_REFRESH_HASH_CNT_WRITE, kmac_basetest::ENTROPY_REFRESH_HASH_CNT_RESET, "ENTROPY_REFRESH_HASH_CNT"}, 
{kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_READ, kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_WRITE, kmac_basetest::ENTROPY_REFRESH_THRESHOLD_SHADOWED_RESET, "ENTROPY_REFRESH_THRESHOLD_SHADOWED"}, 
{kmac_basetest::ENTROPY_SEED_OFFSET, kmac_basetest::ENTROPY_SEED_READ, kmac_basetest::ENTROPY_SEED_WRITE, kmac_basetest::ENTROPY_SEED_RESET, "ENTROPY_SEED"}, 
{kmac_basetest::KEY_SHARE0_OFFSET, kmac_basetest::KEY_SHARE0_READ, kmac_basetest::KEY_SHARE0_WRITE, kmac_basetest::KEY_SHARE0_RESET, "KEY_SHARE0"}, 
{kmac_basetest::KEY_SHARE1_OFFSET, kmac_basetest::KEY_SHARE1_READ, kmac_basetest::KEY_SHARE1_WRITE, kmac_basetest::KEY_SHARE1_RESET, "KEY_SHARE1"}, 
{kmac_basetest::KEY_LEN_OFFSET, kmac_basetest::KEY_LEN_READ, kmac_basetest::KEY_LEN_WRITE, kmac_basetest::KEY_LEN_RESET, "KEY_LEN"}, 
{kmac_basetest::PREFIX_OFFSET, kmac_basetest::PREFIX_READ, kmac_basetest::PREFIX_WRITE, kmac_basetest::PREFIX_RESET, "PREFIX"}, 
{kmac_basetest::ERR_CODE_OFFSET, kmac_basetest::ERR_CODE_READ, kmac_basetest::ERR_CODE_WRITE, kmac_basetest::ERR_CODE_RESET, "ERR_CODE"},
{kmac_basetest::STATE_OFFSET, kmac_basetest::STATE_READ, kmac_basetest::STATE_WRITE, kmac_basetest::STATE_RESET, "STATE"},
{kmac_basetest::MSG_FIFO_OFFSET, kmac_basetest::MSG_FIFO_READ, kmac_basetest::MSG_FIFO_WRITE, kmac_basetest::MSG_FIFO_RESET, "MSG_FIFO"}};