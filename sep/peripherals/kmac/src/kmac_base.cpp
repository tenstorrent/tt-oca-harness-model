// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "kmac_base.h"

void kmac_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  CFG_REGWEN.reset();
  CFG_SHADOWED.reset();
  CMD.reset();
  STATUS.reset();
  ENTROPY_PERIOD.reset();
  ENTROPY_REFRESH_HASH_CNT.reset();
  ENTROPY_REFRESH_THRESHOLD_SHADOWED.reset();
  ENTROPY_SEED.reset();
  for (size_t i = 0; i < 16; i++) {
    KEY_SHARE0[i].reset();
  }
  for (size_t i = 0; i < 16; i++) {
    KEY_SHARE1[i].reset();
  }
  KEY_LEN.reset();
  for (size_t i = 0; i < 11; i++) {
    PREFIX[i].reset();
  }
  ERR_CODE.reset();

  // Reset STATE register array (128 entries)
  for (size_t i = 0; i < 128; i++) {
    STATE[i].reset();
  }

  // Reset MSG_FIFO register array (512 entries)
  for (size_t i = 0; i < 512; i++) {
    MSG_FIFO[i].reset();
  }
}