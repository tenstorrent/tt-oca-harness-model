#include "sep_scratch_warm_base.h"

void sep_scratch_warm_base::reset_all_registers()
{
  for(unsigned int i = 0; i < 8; i++)
  {
    SCRATCH[i].reset();
  }
}