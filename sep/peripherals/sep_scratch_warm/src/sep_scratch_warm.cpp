#include "sep_scratch_warm.h"

sep_scratch_warm_ip::sep_scratch_warm_ip(sc_module_name n)
    : sep_scratch_warm_base(n, "sep_scratch_warm", 8 * sizeof(unsigned long long))
    , rst_ni("rst_ni")
{
    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    reset_all_registers();
}

void sep_scratch_warm_ip::reset_handler()
{
    if (!rst_ni.read())
        reset_all_registers();
}
