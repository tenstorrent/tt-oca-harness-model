// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_reset_ctrl_base.h"

void sep_reset_ctrl_base::reset_all_registers()
{
  SW_RESET_N.reset();
}