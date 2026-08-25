// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_cpu_ctrl_base.h"

void sep_cpu_ctrl_base::reset_all_registers()
{
  CLOCK_GATE_CTRL.reset();
  REFERENCE_COUNTER.reset();
  TIMEOUT_INTERRUPT.reset();
  PKA_CTRL.reset();
  TIMEOUT_COUNT_DMA.reset();
  TIMEOUT_COUNT_SYS_IN.reset();
  TIMEOUT_COUNT_MAILBOX_INBOUND.reset();
  TIMEOUT_COUNT_MAILBOX_OUTBOUND.reset();
  TIMEOUT_COUNT_ENTROPY_WRITE.reset();
  TIMEOUT_COUNT_ENTROPY_READ.reset();
  TIMEOUT_COUNT_FILTER_OUT.reset();
  TIMEOUT_COUNT_ALIAS_REMAP.reset();
  TIMEOUT_ENABLE.reset();
  TIMEOUT_CLEAR.reset();
  TIMEOUT_MODE.reset();
  SEP_TEST_CTRL.reset();
  SEP_GLOBAL_BASE_ADDR.reset();
  SEP_LOCAL_BASE_ADDR.reset();
  SEP_REGION_SIZE.reset();
  SMU_GLOBAL_BASE_ADDR.reset();
  SMU_REGION_SIZE.reset();
  SMC_FUSE_SENSE_STATUS.reset();
  SEP_FUSE_SENSE_STATUS.reset();
  SEP_STRAPS.reset();
  RAS_BANK_INFO.reset();
  SEP_SW_DEBUG.reset();
  SEP_NMI_VEC.reset();
  SEP_NMI_VEC_LOCK.reset();
  EXT_TRNG_SRC_SEL.reset();
  EXT_TRNG_SRC_SEL_LOCK.reset();
  KM_WIPE_CTRL.reset();
  SEP_VERSION_ID.reset();
}