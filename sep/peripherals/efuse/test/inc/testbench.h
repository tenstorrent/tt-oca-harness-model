// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc.h>
#include <memory>
#include <vector>
#include <string>
#include "efuse.h"
#include "efuse_test.h"
#include "reg_logger.h"

class testbench : public sc_module
{
public:
    RegLogger logger;

    std::unique_ptr<efuse_model> m_dut;
    std::unique_ptr<efuse_test>  m_test;

    /// locked_field_access_irq_o, plus a count of its rising edges. The pulse is
    /// too short to catch by polling, so it is counted as it happens.
    sc_signal<bool> locked_field_irq;
    unsigned int    m_locked_field_pulses = 0;
    void count_locked_field_pulse() { m_locked_field_pulses++; }

    /// Number of pulses since the last call — the form every assertion wants.
    unsigned int take_locked_field_pulses() {
        const unsigned int n = m_locked_field_pulses;
        m_locked_field_pulses = 0;
        return n;
    }

    int m_tests_run;
    int m_tests_passed;
    int m_tests_failed;
    std::vector<std::string> m_failed_tests;

    SC_HAS_PROCESS(testbench);

    testbench(sc_module_name name);
    ~testbench() = default;

    void run_tests();

    void report_test_start(const std::string &name);
    void report_test_pass(const std::string &name);
    void report_test_fail(const std::string &name, const std::string &reason);
    void report_test_summary();

    // Test cases
    void test_fuse_load_ro_registers();
    void test_fuse_load_array_registers();
    void test_ro_write_protection();
    void test_woset_locks();
    void test_woset_sip_dis();
    void test_woset_sys_dis();
    void test_efuse_sense_done();
    void test_efuse_read_ctrl_rw();

    // Coverage tests (test_coverage.cpp)
    void run_coverage_tests();
    void test_woset_locks_hi();
    void test_woset_lc_state();
    void test_woset_sip_dis_hi();
    void test_woset_sys_dis_hi();
    void test_woset_chiplet_pubk_revoke();
    void test_woset_bl_version();
    void test_efuse_write_ctrl_go();
    void test_efuse_read_ctrl_go();
    void test_otp_accessors();
    void test_shim_ctrl_window();
    void test_fuse_array_program_read();
    void test_fuse_preload_file();
    void test_fuse_program_out_of_range();
    void test_lock_enforcement();
    void test_token_matching();
    void test_lc_state_transitions();
    void test_locked_field_interrupt();
    void test_transient_rma();
    void test_consumer_accessors();

    /// The whole of the secure_tm run — see the comment on its definition.
    void test_secure_tm_mode();
};
