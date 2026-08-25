// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl_test.cpp
 * @brief Test implementation for SEP Reset Controller
 */

#include "sep_reset_ctrl_test.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

sep_reset_ctrl_test::sep_reset_ctrl_test(sc_core::sc_module_name n)
    : sc_module(n), initiator_socket("initiator_socket")
{
}

void sep_reset_ctrl_test::csr_write_64(uint64_t offset, uint64_t value)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    
    assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE && "CSR write failed");
}

uint64_t sep_reset_ctrl_test::csr_read_64(uint64_t offset)
{
    uint64_t value = 0;
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    initiator_socket->b_transport(trans, delay);
    
    assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE && "CSR read failed");
    return value;
}

void sep_reset_ctrl_test::test_reset_values()
{
    std::cout << "\n=== Testing Reset Values ===" << std::endl;
    
    // SW_RESET_N should reset to 0x1E per RDL specification
    // km_sw_rst_n=0, otbn_sw_rst_n=1, aes_sw_rst_n=1, hmac_sw_rst_n=1, kmac_sw_rst_n=1
    uint64_t sw_reset_val = csr_read_64(0x0);
    std::cout << "  SW_RESET_N reads: 0x" << std::hex << sw_reset_val << std::dec << std::endl;
    
    assert(sw_reset_val == 0x1E && "SW_RESET_N should reset to 0x1E");
    
    std::cout << "Reset values test PASSED" << std::endl;
}

void sep_reset_ctrl_test::test_register_access()
{
    std::cout << "\n=== Testing Basic Register Access ===" << std::endl;
    
    // Test write and read back
    csr_write_64(0x0, 0x15); // km=1, otbn=0, aes=1, hmac=0, kmac=1
    uint64_t readback = csr_read_64(0x0);
    assert((readback & 0x1F) == 0x15 && "SW_RESET_N readback must match written value");
    std::cout << "  Wrote: 0x15, Read: 0x" << std::hex << readback << std::dec << std::endl;

    // Test another pattern
    csr_write_64(0x0, 0x0A); // km=0, otbn=1, aes=0, hmac=1, kmac=0
    readback = csr_read_64(0x0);
    assert((readback & 0x1F) == 0x0A && "SW_RESET_N readback must match written value");
    
    std::cout << "Basic register access test PASSED" << std::endl;
}

void sep_reset_ctrl_test::test_reset_output_updates()
{
    std::cout << "\n=== Testing Reset Output Updates ===" << std::endl;
    
    // This test verifies the register access works correctly
    // The actual reset output signal testing would require access to the DUT signals
    // which is handled in the testbench level
    
    // Test different bit patterns
    struct TestCase { 
        uint64_t sw_reset_val; 
        const char* description; 
    };
    
    TestCase test_cases[] = {
        {0x00, "All peripherals in reset"},
        {0x1F, "All peripherals released"}, 
        {0x01, "Only KM released"},
        {0x1E, "Reset state (KM in reset, others released)"}
    };
    
    for (const auto& tc : test_cases) {
        csr_write_64(0x0, tc.sw_reset_val);
        uint64_t readback = csr_read_64(0x0);
        
        assert((readback & 0x1F) == (tc.sw_reset_val & 0x1F) && "SW_RESET_N readback must match written value");
        std::cout << "  " << tc.description << " - wrote: 0x" << std::hex << tc.sw_reset_val
                  << ", read: 0x" << readback << std::dec << std::endl;
    }
    
    std::cout << "Reset output updates test PASSED" << std::endl;
}

void sep_reset_ctrl_test::test_global_reset_behavior()
{
    std::cout << "\n=== Testing Global Reset Behavior ===" << std::endl;
    
    // This test verifies register behavior - the signal-level testing
    // is done in the testbench where we have access to the signals
    
    // Verify that register access works correctly for different values
    // Each bit should be independently controllable
    for (int bit = 0; bit < 5; bit++) {
        uint64_t value = 1ULL << bit;
        csr_write_64(0x0, value);
        uint64_t readback = csr_read_64(0x0);
        
        assert((readback & 0x1F) == (value & 0x1F) && "SW_RESET_N bit readback must match written value");
        std::cout << "  Bit " << bit << " test - wrote: 0x" << std::hex << value
                  << ", read: 0x" << readback << std::dec << std::endl;
    }
    
    std::cout << "Global reset behavior test PASSED" << std::endl;
}