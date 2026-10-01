// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#pragma once
#include "sep_filter_ctrl_basetest.h"
#include "tlm_probe.h"

#include <string>

class sep_filter_ctrl_test : public sep_filter_ctrl_basetest
{
public:
    sep_filter_ctrl_test(sc_module_name name) : sep_filter_ctrl_basetest(name) {}

    // Transport-failure bookkeeping.
    //
    // The CSR helpers below used to answer a failed transaction with 0 — which
    // is a legal reset value for most of these fields, so a refused access was
    // indistinguishable from a correct one. They now leave the caller's value
    // alone and record the failure here; the testbench refuses to print
    // "ALL TESTS PASSED" while the count is non-zero.
    unsigned           transport_failures() const { return m_transport_failures; }
    const std::string& last_transport_error() const { return m_last_transport_error; }
    void               clear_transport_failures();

    /// Drive a deliberately malformed payload; the status is returned, not
    /// recorded, because a rejection is the expected outcome here.
    simtlm::access_result probe(simtlm::defect d, const simtlm::target_geometry& geo,
                                tlm::tlm_command cmd);

    // Raw register access
    void register_read_8(unsigned int offset, uint8_t& read_value);
    void register_write_8(unsigned int offset, uint8_t write_value);
    void register_read_64(unsigned int offset, uint64_t& read_value);
    void register_write_64(unsigned int offset, uint64_t write_value);

    // Per-instance CSR access (stride=0x20 B between entries)
    void     csr_write_64(uint32_t instance, uint32_t reg_offset, uint64_t value);
    uint64_t csr_read_64(uint32_t instance, uint32_t reg_offset);
    // RV32 firmware path: two 32-bit stores into a 64-bit CSR (wr64).
    void     csr_write_32_pair(uint32_t instance, uint32_t reg_offset, uint64_t value);

    /// Entry locked by test_woset_locked_field(). The lock is irreversible and
    /// (RTL #2480) every later CSR write to the entry is DECERR'd, so suites
    /// that run afterwards must stay away from it. Its neighbour
    /// (LOCKED_ENTRY + 1) is used as the "still writable" witness.
    static constexpr uint32_t LOCKED_ENTRY = 30;

    // Test methods (shared by both outbound and inbound instances)
    void test_reset_values(uint32_t num_instances);
    void test_register_access_basic();
    void test_woset_locked_field();
    void test_hw_readonly_data_bus_width();
    void test_passthrough_when_unconfigured(uint32_t num_instances);
    void test_comprehensive_filter_scenarios();
    void test_malformed_payloads(uint32_t num_instances);

    ~sep_filter_ctrl_test() {}

private:
    void note_transport(const simtlm::access_result& r, const char* op, uint64_t offset);

    unsigned    m_transport_failures = 0;
    std::string m_last_transport_error;
};
