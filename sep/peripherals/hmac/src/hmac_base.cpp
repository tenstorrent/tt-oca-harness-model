// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file hmac_base.cpp
 * @brief Implementation of hmac_base class methods
 * 
 * This file contains the implementation of the base HMAC IP class methods,
 * primarily the register reset functionality.
 */

#include "hmac_base.h"
#include <tlm.h>
#include <systemc.h>

/**
 * @brief Reset all registers to their default values
 * 
 * This function resets all registers in the HMAC IP to their reset values
 * as defined in the register definitions. This includes:
 * - Interrupt control registers
 * - Configuration and command registers
 * - Status and error registers
 * - Key, digest, and message length registers
 * - Message FIFO registers
 */
void hmac_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  CFG.reset();
  CMD.reset();
  STATUS.reset();
  ERR_CODE.reset();
  WIPE_SECRET.reset();
  for (size_t i = 0; i < 32; i++) {
    KEY[i].reset();
  }
  for (size_t i = 0; i < 16; i++) {
    DIGEST[i].reset();
  }
  MSG_LENGTH_LOWER.reset();
  MSG_LENGTH_UPPER.reset();
  for (size_t i = 0; i < 1024; i++) {
    MSG_FIFO[i].reset();
  }
}