// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_basetest.cpp
 * @brief OTBN base test register map initialization
 * 
 * Initializes the register property map used for automated register testing.
 */

#include "otbn_basetest.h"

/// @brief Global register property map for automated testing
otbn_basetest::Register_Property_t reg_map[13] = {
{otbn_basetest::INTR_STATE_OFFSET, otbn_basetest::INTR_STATE_READ, otbn_basetest::INTR_STATE_WRITE, otbn_basetest::INTR_STATE_RESET, "INTR_STATE"}, 
{otbn_basetest::INTR_ENABLE_OFFSET, otbn_basetest::INTR_ENABLE_READ, otbn_basetest::INTR_ENABLE_WRITE, otbn_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"}, 
{otbn_basetest::INTR_TEST_OFFSET, otbn_basetest::INTR_TEST_READ, otbn_basetest::INTR_TEST_WRITE, otbn_basetest::INTR_TEST_RESET, "INTR_TEST"}, 
{otbn_basetest::ALERT_TEST_OFFSET, otbn_basetest::ALERT_TEST_READ, otbn_basetest::ALERT_TEST_WRITE, otbn_basetest::ALERT_TEST_RESET, "ALERT_TEST"}, 
{otbn_basetest::CMD_OFFSET, otbn_basetest::CMD_READ, otbn_basetest::CMD_WRITE, otbn_basetest::CMD_RESET, "CMD"}, 
{otbn_basetest::CTRL_OFFSET, otbn_basetest::CTRL_READ, otbn_basetest::CTRL_WRITE, otbn_basetest::CTRL_RESET, "CTRL"}, 
{otbn_basetest::STATUS_OFFSET, otbn_basetest::STATUS_READ, otbn_basetest::STATUS_WRITE, otbn_basetest::STATUS_RESET, "STATUS"}, 
{otbn_basetest::ERR_BITS_OFFSET, otbn_basetest::ERR_BITS_READ, otbn_basetest::ERR_BITS_WRITE, otbn_basetest::ERR_BITS_RESET, "ERR_BITS"}, 
{otbn_basetest::FATAL_ALERT_CAUSE_OFFSET, otbn_basetest::FATAL_ALERT_CAUSE_READ, otbn_basetest::FATAL_ALERT_CAUSE_WRITE, otbn_basetest::FATAL_ALERT_CAUSE_RESET, "FATAL_ALERT_CAUSE"}, 
{otbn_basetest::INSN_CNT_OFFSET, otbn_basetest::INSN_CNT_READ, otbn_basetest::INSN_CNT_WRITE, otbn_basetest::INSN_CNT_RESET, "INSN_CNT"}, 
{otbn_basetest::LOAD_CHECKSUM_OFFSET, otbn_basetest::LOAD_CHECKSUM_READ, otbn_basetest::LOAD_CHECKSUM_WRITE, otbn_basetest::LOAD_CHECKSUM_RESET, "LOAD_CHECKSUM"}, 
{otbn_basetest::IMEM_OFFSET, otbn_basetest::IMEM_READ, otbn_basetest::IMEM_WRITE, otbn_basetest::IMEM_RESET, "IMEM"}, 
{otbn_basetest::DMEM_OFFSET, otbn_basetest::DMEM_READ, otbn_basetest::DMEM_WRITE, otbn_basetest::DMEM_RESET, "DMEM"}};