// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file edn_base.cpp
 * @brief EDN base class implementation
 *
 * Implements the base infrastructure methods for the EDN register model.
 * Currently provides register reset functionality to restore all registers
 * to their hardware reset values.
 */

#include "edn_base.h"

/**
 * @brief Reset all EDN registers to hardware reset values
 *
 * Calls the reset() method on all 17 EDN registers to restore them to
 * their hardware-defined reset values as specified in the CSV register map.
 *
 * Reset Values:
 * - INTR_STATE, INTR_ENABLE, INTR_TEST: 0x00000000
 * - ALERT_TEST: 0x00000000
 * - REGWEN: 0x00000001 (unlocked)
 * - CTRL: 0x00009999 (all modes disabled: EDN_ENABLE=0x9, BOOT_REQ_MODE=0x9, AUTO_REQ_MODE=0x9, CMD_FIFO_RST=0x9)
 * - BOOT_INS_CMD: 0x00000001 (default instantiate command)
 * - BOOT_GEN_CMD: 0x00FFF003 (default generate command with glen=0xFFF)
 * - SW_CMD_REQ, SW_CMD_STS, HW_CMD_STS: 0x00000000
 * - RESEED_CMD, GENERATE_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS: 0x00000000
 * - RECOV_ALERT_STS, ERR_CODE, ERR_CODE_TEST: 0x00000000
 * - MAIN_SM_STATE: 0x000000C1 (Idle state - sparse encoded)
 *
 * @note This method should be called during module initialization and on system reset.
 * @note Reset does not trigger register callbacks or state machine transitions.
 */
void edn_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  REGWEN.reset();
  CTRL.reset();
  BOOT_INS_CMD.reset();
  BOOT_GEN_CMD.reset();
  SW_CMD_REQ.reset();
  SW_CMD_STS.reset();
  HW_CMD_STS.reset();
  RESEED_CMD.reset();
  GENERATE_CMD.reset();
  MAX_NUM_REQS_BETWEEN_RESEEDS.reset();
  RECOV_ALERT_STS.reset();
  ERR_CODE.reset();
  ERR_CODE_TEST.reset();
  MAIN_SM_STATE.reset();
}