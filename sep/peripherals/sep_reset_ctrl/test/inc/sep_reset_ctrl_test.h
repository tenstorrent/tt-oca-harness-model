// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

/**
 * @file sep_reset_ctrl_test.h
 * @brief Test harness for SEP Reset Controller
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

class sep_reset_ctrl_test : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(sep_reset_ctrl_test);

    // TLM socket for CSR access
    tlm_utils::simple_initiator_socket<sep_reset_ctrl_test> initiator_socket;

    explicit sep_reset_ctrl_test(sc_core::sc_module_name n);

    // Test functions
    void test_reset_values();
    void test_register_access();
    void test_reset_output_updates();
    void test_global_reset_behavior();

    // Helper functions for register access (public for testbench)
    void csr_write_64(uint64_t offset, uint64_t value);
    uint64_t csr_read_64(uint64_t offset);
};