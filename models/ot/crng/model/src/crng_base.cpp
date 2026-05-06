#include "crng_base.h"

void crng_base::reset_all_registers()
{
  INTR_STATE.reset();
  INTR_ENABLE.reset();
  INTR_TEST.reset();
  ALERT_TEST.reset();
  REGWEN.reset();
  CTRL.reset();
  CMD_REQ.reset();
  RESEED_INTERVAL.reset();
  RESEED_COUNTER_0.reset();
  RESEED_COUNTER_1.reset();
  RESEED_COUNTER_2.reset();
  SW_CMD_STS.reset();
  GENBITS_VLD.reset();
  GENBITS.reset();
  INT_STATE_READ_ENABLE.reset();
  INT_STATE_READ_ENABLE_REGWEN.reset();
  INT_STATE_NUM.reset();
  INT_STATE_VAL.reset();
  FIPS_FORCE.reset();
  HW_EXC_STS.reset();
  RECOV_ALERT_STS.reset();
  ERR_CODE.reset();
  ERR_CODE_TEST.reset();
  MAIN_SM_STATE.reset();
}