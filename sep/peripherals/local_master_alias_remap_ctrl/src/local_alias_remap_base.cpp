// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "local_alias_remap_base.h"

void local_alias_remap_base::reset_all_registers()
{
  for(unsigned i = 0; i < 16; i++) 
  {
      REGION_START[i].reset();
      REGION_END[i].reset();
      REGION_ATTRS[i].reset();
  }
}