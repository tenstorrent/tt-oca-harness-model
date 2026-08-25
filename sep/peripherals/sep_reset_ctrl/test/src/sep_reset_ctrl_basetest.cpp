// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_reset_ctrl_basetest.h"

sep_reset_ctrl_basetest::Register_Property_t reg_map[1] = {
{sep_reset_ctrl_basetest::SW_RESET_N_OFFSET, sep_reset_ctrl_basetest::SW_RESET_N_READ, sep_reset_ctrl_basetest::SW_RESET_N_WRITE, sep_reset_ctrl_basetest::SW_RESET_N_RESET, "SW_RESET_N"}};