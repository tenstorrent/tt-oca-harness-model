// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "edn_basetest.h"

edn_basetest::Register_Property_t reg_map[18] = {
{edn_basetest::INTR_STATE_OFFSET, edn_basetest::INTR_STATE_READ, edn_basetest::INTR_STATE_WRITE, edn_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{edn_basetest::INTR_ENABLE_OFFSET, edn_basetest::INTR_ENABLE_READ, edn_basetest::INTR_ENABLE_WRITE, edn_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{edn_basetest::INTR_TEST_OFFSET, edn_basetest::INTR_TEST_READ, edn_basetest::INTR_TEST_WRITE, edn_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{edn_basetest::ALERT_TEST_OFFSET, edn_basetest::ALERT_TEST_READ, edn_basetest::ALERT_TEST_WRITE, edn_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{edn_basetest::REGWEN_OFFSET, edn_basetest::REGWEN_READ, edn_basetest::REGWEN_WRITE, edn_basetest::REGWEN_RESET, "REGWEN"}, 
{edn_basetest::CTRL_OFFSET, edn_basetest::CTRL_READ, edn_basetest::CTRL_WRITE, edn_basetest::CTRL_RESET, "CTRL"}, 
{edn_basetest::BOOT_INS_CMD_OFFSET, edn_basetest::BOOT_INS_CMD_READ, edn_basetest::BOOT_INS_CMD_WRITE, edn_basetest::BOOT_INS_CMD_RESET, "BOOT_INS_CMD"}, 
{edn_basetest::BOOT_GEN_CMD_OFFSET, edn_basetest::BOOT_GEN_CMD_READ, edn_basetest::BOOT_GEN_CMD_WRITE, edn_basetest::BOOT_GEN_CMD_RESET, "BOOT_GEN_CMD"}, 
{edn_basetest::SW_CMD_REQ_OFFSET, edn_basetest::SW_CMD_REQ_READ, edn_basetest::SW_CMD_REQ_WRITE, edn_basetest::SW_CMD_REQ_RESET, "SW_CMD_REQ"}, 
{edn_basetest::SW_CMD_STS_OFFSET, edn_basetest::SW_CMD_STS_READ, edn_basetest::SW_CMD_STS_WRITE, edn_basetest::SW_CMD_STS_RESET, "SW_CMD_STS"}, 
{edn_basetest::HW_CMD_STS_OFFSET, edn_basetest::HW_CMD_STS_READ, edn_basetest::HW_CMD_STS_WRITE, edn_basetest::HW_CMD_STS_RESET, "HW_CMD_STS"}, 
{edn_basetest::RESEED_CMD_OFFSET, edn_basetest::RESEED_CMD_READ, edn_basetest::RESEED_CMD_WRITE, edn_basetest::RESEED_CMD_RESET, "RESEED_CMD"}, 
{edn_basetest::GENERATE_CMD_OFFSET, edn_basetest::GENERATE_CMD_READ, edn_basetest::GENERATE_CMD_WRITE, edn_basetest::GENERATE_CMD_RESET, "GENERATE_CMD"}, 
{edn_basetest::MAX_NUM_REQS_BETWEEN_RESEEDS_OFFSET, edn_basetest::MAX_NUM_REQS_BETWEEN_RESEEDS_READ, edn_basetest::MAX_NUM_REQS_BETWEEN_RESEEDS_WRITE, edn_basetest::MAX_NUM_REQS_BETWEEN_RESEEDS_RESET, "MAX_NUM_REQS_BETWEEN_RESEEDS"}, 
{edn_basetest::RECOV_ALERT_STS_OFFSET, edn_basetest::RECOV_ALERT_STS_READ, edn_basetest::RECOV_ALERT_STS_WRITE, edn_basetest::RECOV_ALERT_STS_RESET, "RECOV_ALERT_STS"}, 
{edn_basetest::ERR_CODE_OFFSET, edn_basetest::ERR_CODE_READ, edn_basetest::ERR_CODE_WRITE, edn_basetest::ERR_CODE_RESET, "ERR_CODE"}, 
{edn_basetest::ERR_CODE_TEST_OFFSET, edn_basetest::ERR_CODE_TEST_READ, edn_basetest::ERR_CODE_TEST_WRITE, edn_basetest::ERR_CODE_TEST_RESET, "ERR_CODE_TEST"}, 
{edn_basetest::MAIN_SM_STATE_OFFSET, edn_basetest::MAIN_SM_STATE_READ, edn_basetest::MAIN_SM_STATE_WRITE, edn_basetest::MAIN_SM_STATE_RESET, "MAIN_SM_STATE"}};