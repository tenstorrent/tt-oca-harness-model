#include "sep_output_remap_ctrl_base.h"

void sep_output_remap_ctrl_base::reset_all_registers()
{
    for (unsigned i = 0; i < 16; i++)
    {
        REGION_ATTRS[i].reset();
    }
}
