#include "lc_ctrl_basetest.h"

lc_ctrl_basetest::Register_Property_t reg_map[4] = {
    {lc_ctrl_basetest::FEAT_CTRL_LO_OFFSET, lc_ctrl_basetest::FEAT_CTRL_LO_READ, lc_ctrl_basetest::FEAT_CTRL_LO_WRITE, lc_ctrl_basetest::FEAT_CTRL_LO_RESET, "FEAT_CTRL_LO"},
    {lc_ctrl_basetest::FEAT_CTRL_HI_OFFSET, lc_ctrl_basetest::FEAT_CTRL_HI_READ, lc_ctrl_basetest::FEAT_CTRL_HI_WRITE, lc_ctrl_basetest::FEAT_CTRL_HI_RESET, "FEAT_CTRL_HI"},
    {lc_ctrl_basetest::DEMOTE_1_OFFSET,     lc_ctrl_basetest::DEMOTE_1_READ,     lc_ctrl_basetest::DEMOTE_1_WRITE,     lc_ctrl_basetest::DEMOTE_1_RESET,     "DEMOTE_1"},
    {lc_ctrl_basetest::DEMOTE_2_OFFSET,     lc_ctrl_basetest::DEMOTE_2_READ,     lc_ctrl_basetest::DEMOTE_2_WRITE,     lc_ctrl_basetest::DEMOTE_2_RESET,     "DEMOTE_2"},
};
