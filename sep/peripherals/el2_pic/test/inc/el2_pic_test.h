// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "el2_pic_basetest.h"
#include "csml_logger.h"
#include <systemc.h>
#include <cstdint>

// Test agent for el2_pic::el2_pic_model.
// Owns the TLM initiator socket and port drivers for rst_ni and irq_in[].
class el2_pic_test : public el2_pic_basetest {
public:
    CsmlLogger logger;

    // Ports driven by the test agent
    sc_core::sc_out<bool>                      rst_no;
    sc_core::sc_vector<sc_core::sc_out<bool>>  irq_o;   // drives DUT irq_in[]

    el2_pic_test(sc_module_name name, unsigned num_irq);

    // Register access (32-bit, byte-addressed)
    void     reg_write_32(unsigned byte_offset, uint32_t value);
    uint32_t reg_read_32(unsigned byte_offset);

    // Convenience helpers
    void drive_irq(unsigned src, bool level);
    void assert_reset();
    void deassert_reset();

    ~el2_pic_test() = default;
};
