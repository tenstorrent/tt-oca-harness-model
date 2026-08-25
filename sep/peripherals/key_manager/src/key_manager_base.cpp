// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "key_manager_base.h"

void key_manager_base::reset_all_registers()
{
    MB_WDATA.reset();
    MB_WSEP.reset();
    MB_RDATA.reset();
    MB_STATUS.reset();
    MB_IRQEN.reset();
    MB_IRQS.reset();
    MB_CTRL.reset();
}
