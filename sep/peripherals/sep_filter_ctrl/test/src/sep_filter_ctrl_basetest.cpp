// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_filter_ctrl_basetest.h"

sep_filter_ctrl_basetest::Register_Property_t reg_map[3] = {
    {sep_filter_ctrl_basetest::FILTER_CONFIG_OFFSET,
     sep_filter_ctrl_basetest::FILTER_CONFIG_READ,
     sep_filter_ctrl_basetest::FILTER_CONFIG_WRITE,
     sep_filter_ctrl_basetest::FILTER_CONFIG_RESET,
     "FILTER_CONFIG"},
    {sep_filter_ctrl_basetest::START_ADDR_OFFSET,
     sep_filter_ctrl_basetest::START_ADDR_READ,
     sep_filter_ctrl_basetest::START_ADDR_WRITE,
     sep_filter_ctrl_basetest::START_ADDR_RESET,
     "START_ADDR"},
    {sep_filter_ctrl_basetest::END_ADDR_OFFSET,
     sep_filter_ctrl_basetest::END_ADDR_READ,
     sep_filter_ctrl_basetest::END_ADDR_WRITE,
     sep_filter_ctrl_basetest::END_ADDR_RESET,
     "END_ADDR"}
};
