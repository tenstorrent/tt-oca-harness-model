// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "lifecycle_ctrl_base.h"

void lifecycle_ctrl_base::reset_all_registers()
{
    FEAT_CTRL_LO.reset();
    FEAT_CTRL_HI.reset();
    DEMOTE_1.reset();
    DEMOTE_1_HI.reset();
    DEMOTE_2.reset();
    DEMOTE_2_HI.reset();
}
