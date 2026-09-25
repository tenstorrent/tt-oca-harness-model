// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc.h>
#include <memory>
#include <vector>
#include <string>
#include "lifecycle_ctrl.h"
#include "lifecycle_ctrl_test.h"
#include "reg_logger.h"

class testbench : public sc_module
{
public:
    RegLogger logger;

    std::unique_ptr<lifecycle_ctrl_model> m_dut;
    std::unique_ptr<lifecycle_ctrl_test>  m_test;

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
    void test_reset_values();
    void test_feat_ctrl_ro();
    void test_test_dev_state();
    void test_prod_state_no_demote();
    void test_demote_1_w1s();
    void test_demote_1_lock();
    void test_demote_2_w1s();
    void test_demote_2_lock();
    void test_invalid_state();
    void test_rma_chiplet_state();
    void test_secure_tm();
    void test_security_disable();

    // Tests driving the input bundle directly, the way the eFuse model does on a
    // platform. These do not depend on the config, so they cover every state arm in one
    // run rather than one arm per ini.
    void test_all_state_arms();
    void test_lc_sigint_fail_safe();
    void test_live_feature_disable();
    void test_demote_diff_encoding();
    void test_demote_upper_words();
    void test_lock_scope();
    void test_prod_dbg_priority();
    void test_outputs_to_sep();
    void test_feat_ctrl_callback();

private:
    /// Apply an input bundle, as the eFuse model does through set_inputs().
    void drive_inputs(uint32_t lc_code, uint64_t sip_dis = 0, uint64_t sys_dis = 0,
                      bool security_disable = false, bool secure_tm = true);

    /// Clear the W1S demote registers so a test can start un-demoted. Stands in for a
    /// reset, which is the only thing that clears them in hardware either.
    void clear_demote();

    /// Recompute FEAT_CTRL from the CCI parameters after clear_demote().
    void restore_configured_inputs();

    /// Read FEAT_CTRL as one 64-bit value.
    uint64_t read_feat_ctrl();

    void check_feat_ctrl(const std::string &name, uint64_t expected);
};
