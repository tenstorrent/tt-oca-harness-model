#include "sep_filter_ctrl_base.h"

void sep_filter_ctrl_base::reset_all_registers()
{
    for (unsigned int i = 0; i < num_instances_; i++)
    {
        FILTER_CONFIG[i].reset();
        START_ADDR[i].reset();
        END_ADDR[i].reset();
    }
}
