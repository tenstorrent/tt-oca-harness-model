
#pragma once
#include "sep_filter_ctrl_basetest.h"

class sep_filter_ctrl_test : public sep_filter_ctrl_basetest
{
public:
    sep_filter_ctrl_test(sc_module_name name) : sep_filter_ctrl_basetest(name) {}

    // Raw register access
    void register_read_8(unsigned int offset, uint8_t& read_value);
    void register_write_8(unsigned int offset, uint8_t write_value);
    void register_read_64(unsigned int offset, uint64_t& read_value);
    void register_write_64(unsigned int offset, uint64_t write_value);

    // Per-instance CSR access (stride=0x20 B between entries)
    void     csr_write_64(uint32_t instance, uint32_t reg_offset, uint64_t value);
    uint64_t csr_read_64(uint32_t instance, uint32_t reg_offset);

    // Test methods (shared by both outbound and inbound instances)
    void test_reset_values(uint32_t num_instances);
    void test_register_access_basic();
    void test_woset_locked_field();
    void test_hw_readonly_data_bus_width();
    void test_passthrough_when_unconfigured(uint32_t num_instances);
    void test_comprehensive_filter_scenarios();

    ~sep_filter_ctrl_test() {}
};
