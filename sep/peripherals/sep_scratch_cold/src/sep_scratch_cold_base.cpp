// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_cold_base.h"

void sep_scratch_cold_base::reset_all_registers()
{
  for (unsigned int i = 0; i < 8; i++) 
  {
      SCRATCH[i].reset();
  }
}