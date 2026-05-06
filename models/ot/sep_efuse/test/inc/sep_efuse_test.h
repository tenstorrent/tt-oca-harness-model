#pragma once
#include "sep_efuse_basetest.h"

class sep_efuse_test : public sep_efuse_basetest
{
public:
    sep_efuse_test(sc_module_name name) : sep_efuse_basetest(name) {}

    void register_read_32(unsigned int offset, uint32_t &read_value);
    void register_write_32(unsigned int offset, uint32_t write_value);

    ~sep_efuse_test() {}
};
