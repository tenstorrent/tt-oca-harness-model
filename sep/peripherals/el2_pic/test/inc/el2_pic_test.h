// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "el2_pic_basetest.h"
#include "reg_logger.h"
#include "tlm_probe.h"
#include <systemc.h>
#include <cstdint>
#include <string>

// Test agent for el2_pic::el2_pic_model.
// Owns the TLM initiator socket and port drivers for rst_ni and irq_in[].
class el2_pic_test : public el2_pic_basetest {
public:
    RegLogger logger;

    // Ports driven by the test agent
    sc_core::sc_out<bool>                      rst_no;
    sc_core::sc_vector<sc_core::sc_out<bool>>  irq_o;   // drives DUT irq_in[]

    el2_pic_test(sc_module_name name, unsigned num_irq);

    // Register access (32-bit, byte-addressed).
    //
    // The signatures are unchanged so the existing scenarios read the same, but
    // the response status is no longer discarded: a transaction the PIC did not
    // service is recorded here and turned into a suite failure by
    // testbench::report_test_pass(). A failed read also leaves the returned
    // value at 0 only because the access never happened — callers should rely on
    // the recorded failure, not the value.
    void     reg_write_32(unsigned byte_offset, uint32_t value);
    uint32_t reg_read_32(unsigned byte_offset);

    // Raw access for negative testing: hands back the status instead of
    // recording it, so a test can assert that a bad payload *is* rejected.
    simtlm::access_result probe(simtlm::defect d, const simtlm::target_geometry& geo,
                                tlm::tlm_command cmd);

    // Transport-failure bookkeeping.
    unsigned           transport_failures() const { return m_transport_failures; }
    const std::string& last_transport_error() const { return m_last_transport_error; }
    void               clear_transport_failures();

    // Convenience helpers
    void drive_irq(unsigned src, bool level);
    void assert_reset();
    void deassert_reset();

    ~el2_pic_test() = default;

private:
    void note_transport(const simtlm::access_result& r, const char* op, unsigned byte_offset);

    unsigned    m_transport_failures = 0;
    std::string m_last_transport_error;
};
