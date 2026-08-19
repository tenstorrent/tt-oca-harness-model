#pragma once
#include "efuse_basetest.h"

class efuse_test : public efuse_basetest
{
public:
    efuse_test(sc_module_name name) : efuse_basetest(name) {}

    void register_read_32(unsigned int offset, uint32_t &read_value);
    void register_write_32(unsigned int offset, uint32_t write_value);

    void shim_read_32(unsigned int offset, uint32_t &read_value);
    void shim_write_32(unsigned int offset, uint32_t write_value);

    ~efuse_test() {}
};
