// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mailbox_base.cpp
 * @brief Implementation of mailbox base register infrastructure
 *
 * Provides reset functionality for single-port mailbox register architecture.
 * TLM transport is handled by mailbox_ip calling memory.b_transport().
 */

#include "mailbox_base.h"

/**
 * @brief Reset all registers to their default values
 *
 * Resets all 10 registers to RDL-specified reset values:
 * - WRITE_DATA: 0x0
 * - READ_DATA: 0x0
 * - STATUS: 0x1 (empty)
 * - ERROR_FLAGS: 0x0
 * - WIRQT: 0x0
 * - RIRQT: 0x0
 * - IRQS: 0x0
 * - IRQEN: 0x0
 * - IRQP: 0x0
 * - CTRL: 0x0
 */
void mailbox_base::reset_registers()
{
  WRITE_DATA.reset();
  READ_DATA.reset();
  STATUS.reset();
  ERROR_FLAGS.reset();
  WIRQT.reset();
  RIRQT.reset();
  IRQS.reset();
  IRQEN.reset();
  IRQP.reset();
  CTRL.reset();
}
