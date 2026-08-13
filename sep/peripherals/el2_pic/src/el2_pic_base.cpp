/**
 * @file el2_pic_base.cpp
 * @brief Reset implementation for the el2_pic register infrastructure.
 */
#include "el2_pic_base.h"

namespace el2_pic {

void el2_pic_base::reset_all_registers()
{
    MPICCFG.reset();
    for (unsigned i = 0; i < NUM_INTERRUPTS; ++i) {
        MEIPL[i].reset();
        MEIE[i].reset();
        MEIGWCTRL[i].reset();
        MEIGWCLR[i].reset();
    }
    for (unsigned w = 0; w < NUM_PEND_WORDS; ++w) {
        MEIP[w].reset();
    }
}

} // namespace el2_pic
