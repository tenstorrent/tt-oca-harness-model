// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer_base.cpp
 * @brief AON Timer base model implementation.
 *
 * Implements the aon_timer_base member functions. Currently provides the
 * reset_all_registers() method which restores all 14 AON Timer registers
 * to their hardware reset values as specified in the register map CSV.
 */

#include "aon_timer_base.h"

/**
 * @brief Reset all 14 AON Timer registers to their hardware reset values.
 *
 * Invokes reset() on each register instance in ascending address order.
 * Post-condition: all registers reflect their CSV-defined reset values:
 *   - WDOG_REGWEN = 0x1 (unlocked, this is the only non-zero reset value)
 *   - All other registers = 0x0
 *
 * This function should be called whenever rst_n or rst_aon_n is asserted to
 * restore the AON Timer to its initial power-on state.
 */
void aon_timer_base::reset_all_registers()
{
  ALERT_TEST.reset();      ///< 0x00 - Alert test register reset to 0x0.
  WKUP_CTRL.reset();       ///< 0x04 - Wakeup timer control reset to 0x0 (disabled, prescaler=0).
  WKUP_THOLD_HI.reset();   ///< 0x08 - Wakeup threshold upper 32 bits reset to 0x0.
  WKUP_THOLD_LO.reset();   ///< 0x0C - Wakeup threshold lower 32 bits reset to 0x0.
  WKUP_COUNT_HI.reset();   ///< 0x10 - Wakeup counter upper 32 bits reset to 0x0.
  WKUP_COUNT_LO.reset();   ///< 0x14 - Wakeup counter lower 32 bits reset to 0x0.
  WDOG_REGWEN.reset();     ///< 0x18 - Watchdog write-enable reset to 0x1 (unlocked).
  WDOG_CTRL.reset();       ///< 0x1C - Watchdog timer control reset to 0x0 (disabled).
  WDOG_BARK_THOLD.reset(); ///< 0x20 - Watchdog bark threshold reset to 0x0.
  WDOG_BITE_THOLD.reset(); ///< 0x24 - Watchdog bite threshold reset to 0x0.
  WDOG_COUNT.reset();      ///< 0x28 - Watchdog counter reset to 0x0.
  INTR_STATE.reset();      ///< 0x2C - Interrupt state reset to 0x0 (no interrupts pending).
  INTR_TEST.reset();       ///< 0x30 - Interrupt test reset to 0x0.
  WKUP_CAUSE.reset();      ///< 0x34 - Wakeup cause reset to 0x0 (no wakeup request active).
}