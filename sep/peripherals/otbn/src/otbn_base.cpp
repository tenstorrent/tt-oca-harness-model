// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_base.cpp
 * @brief Implementation of OTBN base class reset functionality
 */

#include "otbn_base.h"

/**
 * @brief Reset all OTBN registers to their default values
 * 
 * Iterates through all register instances and memory arrays (IMEM, DMEM)
 * calling their reset() method to restore default values.
 */
void otbn_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  CMD.reset();
  CTRL.reset();
  STATUS.reset();
  ERR_BITS.reset();
  FATAL_ALERT_CAUSE.reset();
  INSN_CNT.reset();
  LOAD_CHECKSUM.reset();
  for (size_t i = 0; i < 2048; i++) {
    IMEM[i].reset();
  }
  for (size_t i = 0; i < 768; i++) {
    DMEM[i].reset();
  }
}