// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "lifecycle_ctrl_basetest.h"

lifecycle_ctrl_basetest::Register_Property_t reg_map[4] = {
    {lifecycle_ctrl_basetest::FEAT_CTRL_LO_OFFSET, lifecycle_ctrl_basetest::FEAT_CTRL_LO_READ, lifecycle_ctrl_basetest::FEAT_CTRL_LO_WRITE, lifecycle_ctrl_basetest::FEAT_CTRL_LO_RESET, "FEAT_CTRL_LO"},
    {lifecycle_ctrl_basetest::FEAT_CTRL_HI_OFFSET, lifecycle_ctrl_basetest::FEAT_CTRL_HI_READ, lifecycle_ctrl_basetest::FEAT_CTRL_HI_WRITE, lifecycle_ctrl_basetest::FEAT_CTRL_HI_RESET, "FEAT_CTRL_HI"},
    {lifecycle_ctrl_basetest::DEMOTE_1_OFFSET,     lifecycle_ctrl_basetest::DEMOTE_1_READ,     lifecycle_ctrl_basetest::DEMOTE_1_WRITE,     lifecycle_ctrl_basetest::DEMOTE_1_RESET,     "DEMOTE_1"},
    {lifecycle_ctrl_basetest::DEMOTE_2_OFFSET,     lifecycle_ctrl_basetest::DEMOTE_2_READ,     lifecycle_ctrl_basetest::DEMOTE_2_WRITE,     lifecycle_ctrl_basetest::DEMOTE_2_RESET,     "DEMOTE_2"},
};
