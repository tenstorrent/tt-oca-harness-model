#include "sep_reset_ctrl_base.h"

void sep_reset_ctrl_base::reset_all_registers()
{
  SW_RESET_N.reset();
}