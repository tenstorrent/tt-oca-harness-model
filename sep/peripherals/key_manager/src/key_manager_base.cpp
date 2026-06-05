#include "key_manager_base.h"

void key_manager_base::reset_all_registers()
{
    MB_WDATA.reset();
    MB_WSEP.reset();
    MB_RDATA.reset();
    MB_STATUS.reset();
    MB_IRQEN.reset();
    MB_IRQS.reset();
    MB_CTRL.reset();

    for (unsigned int i = 0; i < 512; i++)
        KPVLP_KEY[i].reset();

    for (unsigned int i = 0; i < 32; i++)
        KPVLP_CTRL[i].reset();

    KPVLP_STATUS.reset();
}
