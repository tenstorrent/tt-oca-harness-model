#pragma once
#include "lc_ctrl_basetest.h"

class lc_ctrl_test : public lc_ctrl_basetest
{
public:
    lc_ctrl_test(sc_module_name name) : lc_ctrl_basetest(name) {}

    void register_read_32(unsigned int offset, uint32_t &read_value);
    void register_write_32(unsigned int offset, uint32_t write_value);

    ~lc_ctrl_test() {}
};
