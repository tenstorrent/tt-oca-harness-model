/**
 * @file aes_base.cpp
 * @brief Implementation of the aes_base register initialization and reset
 * 
 * This file provides the implementation for resetting all memory-mapped
 * registers of the AES peripheral to their hardware-defined default values.
 */

#include "aes_base.h"

/** 
 * @brief Resets all hardware registers to their default reset values
 * 
 * This method is called during the asynchronous reset process to ensure
 * all software-visible registers (CTRL, STATUS, etc.) are in their 
 * correct initial state. It also zeroes out sensitive data registers
 * like key shares and IVs.
 */
void aes_base::reset_all_registers()
{
  ALERT_TEST.reset();
  for (size_t i = 0; i < 8; i++) {
    KEY_SHARE0[i].reset();
  }
  for (size_t i = 0; i < 8; i++) {
    KEY_SHARE1[i].reset();
  }
  for (size_t i = 0; i < 4; i++) {
    IV[i].reset();
  }
  for (size_t i = 0; i < 4; i++) {
    DATA_IN[i].reset();
  }
  for (size_t i = 0; i < 4; i++) {
    DATA_OUT[i].reset();
  }
  CTRL_SHADOWED.reset();
  CTRL_AUX_SHADOWED.reset();
  CTRL_AUX_REGWEN.reset();
  TRIGGER.reset();
  STATUS.reset();
}