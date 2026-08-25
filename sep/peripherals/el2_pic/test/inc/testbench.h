// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include <systemc.h>
#include "el2_pic.h"
#include "VeeR-ISSTlm.hpp"
#include "el2_pic_test.h"
#include "csml_logger.h"
#include <memory>
#include <vector>
#include <string>

class testbench : public sc_module {
public:
    CsmlLogger logger;

    std::unique_ptr<el2_pic::el2_pic_model> m_dut;
    std::unique_ptr<el2_pic_test>           m_test;

    // Second instance with every irq_in left deliberately unbound, standing in
    // for an integration that drives only some of the 256 sources. Its mere
    // existence is most of the test: without the tie-off in
    // before_end_of_elaboration this aborts during elaboration.
    std::unique_ptr<el2_pic::el2_pic_model> m_dut_unbound;

    // Owned directly by the testbench: el2_pic_model only holds a hart_
    // pointer (set via bind_hart()), it doesn't own the hart itself.
    VeeRISSTlm mock_hart;

    sc_core::sc_signal<bool>                      rst_n_sig;
    sc_core::sc_signal<bool>                      clk_sig;
    sc_core::sc_vector<sc_core::sc_signal<bool>>  irq_sigs;

    // CSR path to m_dut_unbound. A target socket must still be bound to
    // something even when the test never transacts on it, so this doubles as the
    // way to read that instance's pending words.
    tlm_utils::simple_initiator_socket<testbench> unbound_isock;
    uint32_t unbound_read_32(unsigned byte_offset);

    // Test counters
    int m_tests_run    = 0;
    int m_tests_passed = 0;
    int m_tests_failed = 0;
    std::vector<std::string> m_failed_tests;

    SC_HAS_PROCESS(testbench);
    testbench(sc_module_name name);
    ~testbench() = default;

    // Test reporting
    void report_test_start(const std::string& test_name);
    void report_test_pass(const std::string& test_name);
    void report_test_fail(const std::string& test_name, const std::string& reason);
    void report_test_summary();

    // Main test sequence (SC_THREAD)
    void run_tests();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-001: Register Reset Values
    // -------------------------------------------------------------------------
    void test_reset_values();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-002: Register Read / Write with Masks
    // -------------------------------------------------------------------------
    void test_mpiccfg_rw();
    void test_meipl_rw();
    void test_meie_rw();
    void test_meigwctrl_rw();
    void test_meip_readonly();
    void test_meigwclr_writeonly();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-003: Level-mode Gateway (active-high)
    // -------------------------------------------------------------------------
    void test_level_gateway_active_high();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-004: Level-mode Gateway (active-low polarity)
    // -------------------------------------------------------------------------
    void test_level_gateway_active_low();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-005: Edge-mode Gateway and MEIGWCLR Clear
    // -------------------------------------------------------------------------
    void test_edge_gateway_and_meigwclr();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-006: Priority Arbitration (highest priority wins)
    // -------------------------------------------------------------------------
    void test_arbitration_highest_priority_wins();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-007: Interrupt Enable Gate (MEIE)
    // -------------------------------------------------------------------------
    void test_enable_gate();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-008: Priority Order (priord inversion)
    // -------------------------------------------------------------------------
    void test_priord_inversion();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-009: Reset Clears All State
    // -------------------------------------------------------------------------
    void test_reset_clears_state();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-010: Highest source ID (255) is fully functional
    // -------------------------------------------------------------------------
    void test_high_source_end_to_end();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-011: meip pending words map sources across all 8 words
    // -------------------------------------------------------------------------
    void test_meip_word_mapping();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-013: Edge-mode meigwclr against a still-asserted source
    // -------------------------------------------------------------------------
    void test_edge_clear_while_asserted();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-014: Under priord=1, raw priority 0 is the highest
    // -------------------------------------------------------------------------
    void test_priord_raw_zero_is_highest();

    // -------------------------------------------------------------------------
    // FUNC-EL2PIC-012: Unbound irq_in sources are tied low
    // -------------------------------------------------------------------------
    void test_unbound_sources_tied_low();
};
