#pragma once

#include <systemc.h>
#include <memory>
#include <vector>
#include <string>
#include "lifecycle_ctrl.h"
#include "lifecycle_ctrl_test.h"
#include "csml_logger.h"

class testbench : public sc_module
{
public:
    CsmlLogger logger;

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
};
