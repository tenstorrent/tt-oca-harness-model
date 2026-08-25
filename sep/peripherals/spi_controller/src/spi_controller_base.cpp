// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_controller_base.cpp
 * @brief Implementation of SPI Controller base class
 *
 * Implements register reset functionality for all SPI Controller hardware registers.
 */

#include "spi_controller_base.h"

/**
 * @brief Reset all hardware registers to their default values
 *
 * Calls reset() on each register instance, restoring all register fields
 * to their hardware-specified reset values as defined in the register
 * definitions.
 */
void spi_controller_base::reset_all_registers()
{
  INTR_STATUS.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  CTRL.reset();
  STATUS.reset();
  CFG.reset();
  CSID.reset();
  CMD.reset();
  RXDATA.reset();
  TXDATA.reset();
  ERROR_ENABLE.reset();
  ERROR_STATUS.reset();
  EVENT_ENABLE.reset();
}
