#include "lc_ctrl_base.h"

void lc_ctrl_base::reset_all_registers()
{
    FEAT_CTRL_LO.reset();
    FEAT_CTRL_HI.reset();
    DEMOTE_1.reset();
    DEMOTE_2.reset();
}
