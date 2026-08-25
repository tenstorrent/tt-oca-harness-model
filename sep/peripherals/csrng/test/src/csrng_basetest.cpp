// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "csrng_basetest.h"

csrng_basetest::Register_Property_t reg_map[24] = {
{csrng_basetest::INTR_STATE_OFFSET, csrng_basetest::INTR_STATE_READ, csrng_basetest::INTR_STATE_WRITE, csrng_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{csrng_basetest::INTR_ENABLE_OFFSET, csrng_basetest::INTR_ENABLE_READ, csrng_basetest::INTR_ENABLE_WRITE, csrng_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{csrng_basetest::INTR_TEST_OFFSET, csrng_basetest::INTR_TEST_READ, csrng_basetest::INTR_TEST_WRITE, csrng_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{csrng_basetest::ALERT_TEST_OFFSET, csrng_basetest::ALERT_TEST_READ, csrng_basetest::ALERT_TEST_WRITE, csrng_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{csrng_basetest::REGWEN_OFFSET, csrng_basetest::REGWEN_READ, csrng_basetest::REGWEN_WRITE, csrng_basetest::REGWEN_RESET, "REGWEN"}, 
{csrng_basetest::CTRL_OFFSET, csrng_basetest::CTRL_READ, csrng_basetest::CTRL_WRITE, csrng_basetest::CTRL_RESET, "CTRL"}, 
{csrng_basetest::CMD_REQ_OFFSET, csrng_basetest::CMD_REQ_READ, csrng_basetest::CMD_REQ_WRITE, csrng_basetest::CMD_REQ_RESET, "CMD_REQ"}, 
{csrng_basetest::RESEED_INTERVAL_OFFSET, csrng_basetest::RESEED_INTERVAL_READ, csrng_basetest::RESEED_INTERVAL_WRITE, csrng_basetest::RESEED_INTERVAL_RESET, "RESEED_INTERVAL"}, 
{csrng_basetest::RESEED_COUNTER_0_OFFSET, csrng_basetest::RESEED_COUNTER_0_READ, csrng_basetest::RESEED_COUNTER_0_WRITE, csrng_basetest::RESEED_COUNTER_0_RESET, "RESEED_COUNTER_0"}, 
{csrng_basetest::RESEED_COUNTER_1_OFFSET, csrng_basetest::RESEED_COUNTER_1_READ, csrng_basetest::RESEED_COUNTER_1_WRITE, csrng_basetest::RESEED_COUNTER_1_RESET, "RESEED_COUNTER_1"}, 
{csrng_basetest::RESEED_COUNTER_2_OFFSET, csrng_basetest::RESEED_COUNTER_2_READ, csrng_basetest::RESEED_COUNTER_2_WRITE, csrng_basetest::RESEED_COUNTER_2_RESET, "RESEED_COUNTER_2"}, 
{csrng_basetest::SW_CMD_STS_OFFSET, csrng_basetest::SW_CMD_STS_READ, csrng_basetest::SW_CMD_STS_WRITE, csrng_basetest::SW_CMD_STS_RESET, "SW_CMD_STS"}, 
{csrng_basetest::GENBITS_VLD_OFFSET, csrng_basetest::GENBITS_VLD_READ, csrng_basetest::GENBITS_VLD_WRITE, csrng_basetest::GENBITS_VLD_RESET, "GENBITS_VLD"}, 
{csrng_basetest::GENBITS_OFFSET, csrng_basetest::GENBITS_READ, csrng_basetest::GENBITS_WRITE, csrng_basetest::GENBITS_RESET, "GENBITS"}, 
{csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, csrng_basetest::INT_STATE_READ_ENABLE_READ, csrng_basetest::INT_STATE_READ_ENABLE_WRITE, csrng_basetest::INT_STATE_READ_ENABLE_RESET, "INT_STATE_READ_ENABLE"}, 
{csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_READ, csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_WRITE, csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_RESET, "INT_STATE_READ_ENABLE_REGWEN"}, 
{csrng_basetest::INT_STATE_NUM_OFFSET, csrng_basetest::INT_STATE_NUM_READ, csrng_basetest::INT_STATE_NUM_WRITE, csrng_basetest::INT_STATE_NUM_RESET, "INT_STATE_NUM"}, 
{csrng_basetest::INT_STATE_VAL_OFFSET, csrng_basetest::INT_STATE_VAL_READ, csrng_basetest::INT_STATE_VAL_WRITE, csrng_basetest::INT_STATE_VAL_RESET, "INT_STATE_VAL"}, 
{csrng_basetest::FIPS_FORCE_OFFSET, csrng_basetest::FIPS_FORCE_READ, csrng_basetest::FIPS_FORCE_WRITE, csrng_basetest::FIPS_FORCE_RESET, "FIPS_FORCE"}, 
{csrng_basetest::HW_EXC_STS_OFFSET, csrng_basetest::HW_EXC_STS_READ, csrng_basetest::HW_EXC_STS_WRITE, csrng_basetest::HW_EXC_STS_RESET, "HW_EXC_STS"}, 
{csrng_basetest::RECOV_ALERT_STS_OFFSET, csrng_basetest::RECOV_ALERT_STS_READ, csrng_basetest::RECOV_ALERT_STS_WRITE, csrng_basetest::RECOV_ALERT_STS_RESET, "RECOV_ALERT_STS"}, 
{csrng_basetest::ERR_CODE_OFFSET, csrng_basetest::ERR_CODE_READ, csrng_basetest::ERR_CODE_WRITE, csrng_basetest::ERR_CODE_RESET, "ERR_CODE"}, 
{csrng_basetest::ERR_CODE_TEST_OFFSET, csrng_basetest::ERR_CODE_TEST_READ, csrng_basetest::ERR_CODE_TEST_WRITE, csrng_basetest::ERR_CODE_TEST_RESET, "ERR_CODE_TEST"}, 
{csrng_basetest::MAIN_SM_STATE_OFFSET, csrng_basetest::MAIN_SM_STATE_READ, csrng_basetest::MAIN_SM_STATE_WRITE, csrng_basetest::MAIN_SM_STATE_RESET, "MAIN_SM_STATE"}};