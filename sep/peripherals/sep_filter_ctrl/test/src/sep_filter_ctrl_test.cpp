#include "sep_filter_ctrl_test.h"
#include <cassert>
#include <tlm.h>
#include <iostream>
#include <iomanip>

using namespace tlm;
using namespace sc_core;

static constexpr uint32_t CSR_STRIDE = 0x20;

// =============================================================================
// Raw register access
// =============================================================================

void sep_filter_ctrl_test::register_read_64(unsigned int offset, uint64_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
    if (trans.is_response_error()) read_value = 0;
}

void sep_filter_ctrl_test::register_write_64(unsigned int offset, uint64_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
}

void sep_filter_ctrl_test::register_read_8(unsigned int offset, uint8_t& read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
    if (trans.is_response_error()) read_value = 0;
}

void sep_filter_ctrl_test::register_write_8(unsigned int offset, uint8_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
}

// =============================================================================
// Per-instance CSR access
// =============================================================================

void sep_filter_ctrl_test::csr_write_64(uint32_t instance, uint32_t reg_offset, uint64_t value)
{
    uint64_t addr = (instance * CSR_STRIDE) + reg_offset;
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
}

uint64_t sep_filter_ctrl_test::csr_read_64(uint32_t instance, uint32_t reg_offset)
{
    uint64_t addr = (instance * CSR_STRIDE) + reg_offset;
    uint64_t read_value = 0;
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
    if (trans.is_response_error()) return 0;
    return read_value;
}

void sep_filter_ctrl_test::csr_write_32_pair(uint32_t instance, uint32_t reg_offset, uint64_t value)
{
    const uint32_t addr = (instance * CSR_STRIDE) + reg_offset;
    uint32_t lo = static_cast<uint32_t>(value);
    uint32_t hi = static_cast<uint32_t>(value >> 32);

    auto beat32 = [this](uint32_t byte_addr, uint32_t word) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(byte_addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&word));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator_socket->b_transport(trans, delay);
    };

    beat32(addr, lo);
    beat32(addr + 4, hi);
}

// =============================================================================
// Test methods
// =============================================================================

void sep_filter_ctrl_test::test_reset_values(uint32_t num_instances)
{
    std::cout << "\n=== Testing Reset Values (num_instances=" << num_instances << ") ===" << std::endl;

    uint32_t check_count = (num_instances < 4) ? num_instances : 4;
    for (uint32_t i = 0; i < check_count; ++i) {
        uint64_t config = csr_read_64(i, 0x00);
        std::cout << "  Instance " << i << " FILTER_CONFIG: 0x" << std::hex << config << std::dec << std::endl;
        // data_bus_width=3 at [14:12] is hw=w; stored value is 0 at reset but read callback returns 3
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always read as 3");
        assert(((config & ~0x7000ULL) == 0x0ULL) && "Other FILTER_CONFIG bits should reset to 0");

        uint64_t start = csr_read_64(i, 0x08);
        assert((start == 0x0ULL) && "START_ADDR should reset to 0");

        uint64_t end = csr_read_64(i, 0x10);
        assert((end == 0x7ULL) && "END_ADDR should reset to 0x7 (minimum 8-byte granularity)");
    }
    std::cout << "Reset values test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_register_access_basic()
{
    std::cout << "\n=== Testing Basic Register Access ===" << std::endl;

    uint32_t instance = 0;

    // Write basic fields (not data_bus_width which is hw=w)
    uint64_t test_config = 0x01F0013ULL; // read_allowed, write_allowed, entry_enabled, allow_ns, allow_burst
    csr_write_64(instance, 0x00, test_config);
    uint64_t read_config = csr_read_64(instance, 0x00);

    uint64_t basic_mask = 0x01F0013ULL;
    assert(((read_config & basic_mask) == test_config) && "FILTER_CONFIG basic fields failed");
    assert(((read_config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");

    uint64_t test_start = 0x12345000ULL;
    csr_write_64(instance, 0x08, test_start);
    assert((csr_read_64(instance, 0x08) == test_start) && "START_ADDR access failed");

    uint64_t test_end = 0x56789000ULL;
    csr_write_64(instance, 0x10, test_end);
    assert((csr_read_64(instance, 0x10) == test_end) && "END_ADDR access failed");

    std::cout << "Basic register access test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_woset_locked_field()
{
    std::cout << "\n=== Testing WOSET Locked Field ===" << std::endl;

    uint32_t instance = 1;
    uint64_t config = csr_read_64(instance, 0x00);
    assert(((config & (1ULL << 63)) == 0) && "Locked should start as 0");

    const uint64_t start_before = csr_read_64(instance, 0x08);
    const uint64_t end_before   = csr_read_64(instance, 0x10);

    // Set locked bit
    csr_write_64(instance, 0x00, 1ULL << 63);
    config = csr_read_64(instance, 0x00);
    assert(((config & (1ULL << 63)) != 0) && "Locked bit should be set");

    // Try to clear — should remain set (WOSET)
    csr_write_64(instance, 0x00, 0x0ULL);
    config = csr_read_64(instance, 0x00);
    assert(((config & (1ULL << 63)) != 0) && "Locked bit should remain set (WOSET)");

    // START/END freeze with the lock (RDL write-once). A locked entry must
    // ignore later START/END stores — the same path RV32 wr64 would take.
    csr_write_64(instance, 0x08, 0x11110000ULL);
    csr_write_64(instance, 0x10, 0x22220000ULL);
    assert((csr_read_64(instance, 0x08) == start_before) && "START_ADDR must freeze when locked");
    assert((csr_read_64(instance, 0x10) == end_before) && "END_ADDR must freeze when locked");

    std::cout << "WOSET locked field test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_hw_readonly_data_bus_width()
{
    std::cout << "\n=== Testing HW-readonly data_bus_width ===" << std::endl;

    uint32_t instance = 2;
    uint64_t test_values[] = {0x0000ULL, 0x1000ULL, 0x2000ULL, 0x4000ULL, 0x7000ULL};
    for (auto test_val : test_values) {
        csr_write_64(instance, 0x00, test_val);
        uint64_t config = csr_read_64(instance, 0x00);
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");
    }
    std::cout << "HW-readonly data_bus_width test PASSED" << std::endl;
}

// Note: this only checks CSR readback (entry_enabled clears to 0). It does NOT
// mean the data path passes through when unconfigured — BlockByDefault=1
// means an unconfigured filter denies all data-path transactions (see A7 in
// sep_filter_ctrl_testbench.cpp).
void sep_filter_ctrl_test::test_passthrough_when_unconfigured(uint32_t num_instances)
{
    std::cout << "\n=== Testing CSR Clears to Unconfigured (entry_enabled=0) ===" << std::endl;

    uint32_t check_count = (num_instances < 8) ? num_instances : 8;
    for (uint32_t i = 0; i < check_count; ++i) {
        csr_write_64(i, 0x00, 0x0ULL);
        uint64_t config = csr_read_64(i, 0x00);
        assert(((config & (1ULL << 4)) == 0) && "entry_enabled should be 0 when cleared");
        assert(((config & 0x7000ULL) == 0x3000ULL) && "data_bus_width should always be 3");
    }
    std::cout << "CSR unconfigured state test PASSED" << std::endl;
}

void sep_filter_ctrl_test::test_comprehensive_filter_scenarios()
{
    std::cout << "\n=== Testing Comprehensive Filter Scenarios ===" << std::endl;

    // Entry 0: read+write, [0x1000, 0x2000)
    csr_write_64(0, 0x08, 0x1000ULL);
    csr_write_64(0, 0x10, 0x2000ULL);
    csr_write_64(0, 0x00, 0x00000013ULL); // read_allowed=1, write_allowed=1, entry_enabled=1
    assert((csr_read_64(0, 0x08) == 0x1000ULL) && "START_ADDR[0] failed");
    assert((csr_read_64(0, 0x10) == 0x2000ULL) && "END_ADDR[0] failed");
    uint64_t cfg0 = csr_read_64(0, 0x00);
    assert(((cfg0 & 0x13ULL) == 0x13ULL) && "FILTER_CONFIG[0] basic bits failed");
    assert(((cfg0 & 0x3000ULL) == 0x3000ULL) && "data_bus_width should remain 3");

    // Entry 1: read-only, [0x3000, 0x4000)
    csr_write_64(1, 0x08, 0x3000ULL);
    csr_write_64(1, 0x10, 0x4000ULL);
    csr_write_64(1, 0x00, 0x00000011ULL); // read_allowed=1, write_allowed=0, entry_enabled=1

    // Entry 2: write-only, [0x5000, 0x6000)
    csr_write_64(2, 0x08, 0x5000ULL);
    csr_write_64(2, 0x10, 0x6000ULL);
    csr_write_64(2, 0x00, 0x00000012ULL); // read_allowed=0, write_allowed=1, entry_enabled=1

    std::cout << "Comprehensive filter scenarios test PASSED" << std::endl;
}
