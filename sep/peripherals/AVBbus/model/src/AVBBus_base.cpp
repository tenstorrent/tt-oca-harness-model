#include "AVBBus_base.h"

void AVBBus_base::reset_all_registers()
{
  AVS_FSM_RESET.reset();
  AVS_CTRL.reset();
  AVS_CMD.reset();
  for (size_t i = 0; i < 15; i++) {
    AVS_READBACK[i].reset();
  }
  AVS_TARGET_ACKS.reset();
  for (size_t i = 0; i < 15; i++) {
    AVS_LATEST_TARGET_SUBFRAME[i].reset();
  }
  AVS_LATEST_TARGET_ACKS.reset();
  AVS_NORMAL_STATUS.reset();
  AVS_CONTROLLER_STATUS.reset();
  AVS_TOTAL_RETRIES.reset();
  AVS_FIFOS_STATUS.reset();
  AVS_INTERRUPT.reset();
  AVS_INTERRUPT_MASK.reset();
  AVS_INTERRUPT_TEST.reset();
  AVS_TARGET_ISSUED_INTERRUPT_IDS.reset();
  AVS_TARGET_UNRESPONSIVE_BAD_FRAME_INT_IDS.reset();
  AVS_TARGET_BAD_CRC_INT_IDS.reset();
  AVS_RETRY_CFG.reset();
  AVS_CLK_CFG.reset();
  AVS_THROTTLE_CFG.reset();
  AVS_CONFIG.reset();
}