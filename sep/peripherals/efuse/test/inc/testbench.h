#pragma once

#include <systemc.h>
#include <memory>
#include <vector>
#include <string>
#include "efuse.h"
#include "efuse_test.h"
#include "csml_logger.h"

class testbench : public sc_module
{
public:
    CsmlLogger logger;

    std::unique_ptr<efuse_model> m_dut;
    std::unique_ptr<efuse_test>  m_test;

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
};
