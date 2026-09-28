// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include "csrng_basetest.h"
#include <iomanip>

testbench::testbench(sc_module_name name)
    : sc_module(name)
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
{
    // Initialize regmodel logger
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Instantiate DUT and test module
    // Using csrng_model which includes full FSM, callbacks, and command processing
    m_crng = std::make_unique<csrng_model>("csrng_dut");
    m_crng->entropy_port.bind(m_entropy_provider);

    // Sync testbench logger verbosity with DUT (CCI ini may override build default)
    logger.setMaxVerbosity(m_crng->verbosity.get_param_value());
    m_test = std::make_unique<csrng_test>("csrng_test");

    // Bind TLM sockets between test and DUT
    m_test->initiator_socket.bind(m_crng->target_socket);

    // Bind clock and reset ports
    m_crng->clk_i(clk_signal);
    m_crng->rst_ni(rst_signal);

    m_crng->otp_en_csrng_sw_app_read(otp_en_signal);

    // Bind interrupt ports
    m_crng->cs_cmd_req_done(cs_cmd_req_done_signal);
    m_crng->cs_entropy_req(cs_entropy_req_signal);
    m_crng->cs_hw_inst_exc(cs_hw_inst_exc_signal);
    m_crng->cs_fatal_err(cs_fatal_err_signal);

    // Bind alert output ports
    m_crng->recov_alert_o(recov_alert_signal);
    m_crng->fatal_alert_o(fatal_alert_signal);

    // Initialize signals
    clk_signal.write(false);
    rst_signal.write(true);  // Active-low reset, so true = not in reset
    otp_en_signal.write(0x6);  // 0x6 = enable, 0x9 = disable

    // Register test process
    SC_THREAD(run_tests);
}

// =============================================================================
// Test Helper Methods
// =============================================================================

/**
 * @brief Apply reset to DUT
 *
 * Asserts active-low reset signal, waits for reset propagation, then deasserts.
 * Follows AES/KMAC reset pattern for proper hardware initialization and to
 * unlock REGWEN-protected registers.
 *
 * This should be called at the beginning of test suites that require:
 * - Unlocked REGWEN (for writing CTRL, RESEED_INTERVAL, etc.)
 * - Clean register state
 * - Initialized FSM state
 */
void testbench::apply_reset()
{
    REG_INFO(2, logger) << "Asserting reset (rst_ni = 0)";
    rst_signal.write(false);  // Assert active-low reset
    wait(10, SC_NS);

    REG_INFO(2, logger) << "Deasserting reset (rst_ni = 1)";
    rst_signal.write(true);   // Deassert reset
    wait(30, SC_NS);          // Wait for reset completion + internal initialization

    REG_INFO(2, logger) << "Reset complete - DUT ready";
}

// =============================================================================
// Test Result Reporting Helpers
// =============================================================================
// =============================================================================
// Malformed generic payloads
//
// INT_STATE_NUM (0x40) is the target: a plain read/write selector with no
// command side effects, unlike CTRL or the CMD_REQ path. CTRL (0x14) is the
// witness, well clear of the 0x40..0x49 range the widest and unaligned defects
// can reach. What each defect should *return* is decode policy; this asserts
// only that a decision is made, nothing crashes, and nothing else moves.
// =============================================================================
void testbench::test_malformed_payloads()
{
    const char* test_name = "Malformed generic payloads";
    report_test_start(test_name);

    constexpr unsigned APERTURE      = 0x60;
    constexpr unsigned OFF_CTRL      = 0x14;
    constexpr unsigned OFF_STATE_NUM = 0x40;

    bool passed = true;

    uint32_t witness_before = 0;
    m_test->register_read_32(OFF_CTRL, witness_before);
    m_test->clear_transport_failures();

    simtlm::target_geometry geo;
    geo.valid_address  = OFF_STATE_NUM;
    geo.word_bytes     = 4;
    geo.aperture_bytes = APERTURE;

    for (simtlm::defect d : simtlm::all_defects()) {
        for (tlm::tlm_command cmd : {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            const auto r = m_test->probe(d, geo, cmd);
            if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
                REG_ERROR(0, logger) << simtlm::defect_name(d) << " ("
                                      << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                                      << ") left the payload INCOMPLETE";
                passed = false;
            }
        }
    }

    uint32_t witness_after = 0;
    m_test->register_read_32(OFF_CTRL, witness_after);
    if (witness_after != witness_before) {
        REG_ERROR(0, logger) << "CTRL corrupted by malformed traffic: was 0x"
                              << std::hex << witness_before << ", now 0x" << witness_after;
        passed = false;
    }

    // Still usable afterwards.
    m_test->register_write_32(OFF_STATE_NUM, 0x1u);
    uint32_t back = 0xFFFFFFFFu;
    m_test->register_read_32(OFF_STATE_NUM, back);

    report_test_result(test_name, passed);
}

// A test cannot pass on the strength of transactions the CSRNG refused. Both
// reporting entry points consult the recorded transport failures, so every
// existing scenario became transport-sensitive without being edited.
bool testbench::transport_clean(const char* test_name)
{
    const unsigned tf = m_test ? m_test->transport_failures() : 0u;
    if (tf == 0)
        return true;

    REG_ERROR(0, logger) << test_name << ": " << tf
                          << " transport error(s); last: "
                          << m_test->last_transport_error();
    m_test->clear_transport_failures();
    return false;
}

void testbench::report_test_result(const char* test_name, bool passed)
{
    m_tests_run++;

    if (!transport_clean(test_name))
        passed = false;

    if (passed) {
        m_tests_passed++;
        REG_INFO(1, logger) << "\n========================================\n"
                             << "[*** TEST PASSED ***] " << test_name << "\n"
                             << "========================================\n";
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        REG_ERROR(0, logger) << "\n========================================\n"
                              << "[XXX TEST FAILED XXX] " << test_name << "\n"
                              << "========================================\n";
    }
}

void testbench::report_test_start(const std::string& test_name)
{
    REG_INFO(1, logger) << "========================================\n"
                         << test_name << "\n"
                         << "========================================";
}

void testbench::report_test_pass(const std::string& test_name)
{
    if (!transport_clean(test_name.c_str())) {
        report_test_fail(test_name, "transport error(s) during the test");
        return;
    }
    m_tests_passed++;
    m_tests_run++;
    REG_INFO(1, logger) << test_name << ": PASS";
}

void testbench::report_test_fail(const std::string& test_name,
                                 const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    REG_ERROR(0, logger) << test_name << ": FAIL - " << reason;
}

void testbench::report_test_summary()
{
    std::string summary = "\n========================================\n" +
                          std::string("CRNG Test Summary\n") +
                          "========================================\n" +
                          "Total Tests: " + std::to_string(m_tests_passed + m_tests_failed) + "\n" +
                          "Passed: " + std::to_string(m_tests_passed) + "\n" +
                          "Failed: " + std::to_string(m_tests_failed) + "\n" +
                          "========================================";
    REG_INFO(1, logger) << summary;
}

// =============================================================================
// Basic Register Access Tests
// =============================================================================

void testbench::test_register_reset_values()
{
    report_test_start("Test: Register Reset Values");

    // Apply reset to ensure all registers are at their reset state
    apply_reset();

    bool all_passed = true;
    uint32_t read_val = 0;

    // Test all registers in offset order (0x00, 0x04, 0x08, ...)

    // 0x00: INTR_STATE
    m_test->register_read_32(csrng_basetest::INTR_STATE_OFFSET, read_val);
    if (read_val != csrng_basetest::INTR_STATE_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INTR_STATE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INTR_STATE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INTR_STATE reset value correct: 0x" << std::hex << read_val;
    }

    // 0x04: INTR_ENABLE
    m_test->register_read_32(csrng_basetest::INTR_ENABLE_OFFSET, read_val);
    if (read_val != csrng_basetest::INTR_ENABLE_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INTR_ENABLE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INTR_ENABLE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INTR_ENABLE reset value correct: 0x" << std::hex << read_val;
    }

    // 0x08: INTR_TEST
    m_test->register_read_32(csrng_basetest::INTR_TEST_OFFSET, read_val);
    if (read_val != csrng_basetest::INTR_TEST_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INTR_TEST reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INTR_TEST_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INTR_TEST reset value correct: 0x" << std::hex << read_val;
    }

    // 0x0c: ALERT_TEST
    m_test->register_read_32(csrng_basetest::ALERT_TEST_OFFSET, read_val);
    if (read_val != csrng_basetest::ALERT_TEST_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: ALERT_TEST reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::ALERT_TEST_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "ALERT_TEST reset value correct: 0x" << std::hex << read_val;
    }

    // 0x10: REGWEN
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if (read_val != csrng_basetest::REGWEN_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: REGWEN reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::REGWEN_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "REGWEN reset value correct: 0x" << std::hex << read_val;
    }

    // 0x14: CTRL
    m_test->register_read_32(csrng_basetest::CTRL_OFFSET, read_val);
    if (read_val != csrng_basetest::CTRL_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: CTRL reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::CTRL_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "CTRL reset value correct: 0x" << std::hex << read_val;
    }

    // 0x18: CMD_REQ
    m_test->register_read_32(csrng_basetest::CMD_REQ_OFFSET, read_val);
    if (read_val != csrng_basetest::CMD_REQ_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: CMD_REQ reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::CMD_REQ_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "CMD_REQ reset value correct: 0x" << std::hex << read_val;
    }

    // 0x1c: RESEED_INTERVAL
    m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, read_val);
    if (read_val != csrng_basetest::RESEED_INTERVAL_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: RESEED_INTERVAL reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::RESEED_INTERVAL_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_INTERVAL reset value correct: 0x" << std::hex << read_val;
    }

    // 0x20: RESEED_COUNTER_0
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_0_OFFSET, read_val);
    if (read_val != csrng_basetest::RESEED_COUNTER_0_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: RESEED_COUNTER_0 reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::RESEED_COUNTER_0_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_0 reset value correct: 0x" << std::hex << read_val;
    }

    // 0x24: RESEED_COUNTER_1
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_1_OFFSET, read_val);
    if (read_val != csrng_basetest::RESEED_COUNTER_1_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: RESEED_COUNTER_1 reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::RESEED_COUNTER_1_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_1 reset value correct: 0x" << std::hex << read_val;
    }

    // 0x28: RESEED_COUNTER_2
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_2_OFFSET, read_val);
    if (read_val != csrng_basetest::RESEED_COUNTER_2_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: RESEED_COUNTER_2 reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::RESEED_COUNTER_2_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_2 reset value correct: 0x" << std::hex << read_val;
    }

    // 0x2c: SW_CMD_STS
    m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, read_val);
    if (read_val != csrng_basetest::SW_CMD_STS_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: SW_CMD_STS reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::SW_CMD_STS_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "SW_CMD_STS reset value correct: 0x" << std::hex << read_val;
    }

    // 0x30: GENBITS_VLD
    m_test->register_read_32(csrng_basetest::GENBITS_VLD_OFFSET, read_val);
    if (read_val != csrng_basetest::GENBITS_VLD_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: GENBITS_VLD reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::GENBITS_VLD_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "GENBITS_VLD reset value correct: 0x" << std::hex << read_val;
    }

    // 0x34: GENBITS
    m_test->register_read_32(csrng_basetest::GENBITS_OFFSET, read_val);
    if (read_val != csrng_basetest::GENBITS_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: GENBITS reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::GENBITS_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "GENBITS reset value correct: 0x" << std::hex << read_val;
    }

    // 0x38: INT_STATE_READ_ENABLE
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, read_val);
    if (read_val != csrng_basetest::INT_STATE_READ_ENABLE_RESET)
    {
        REG_ERROR(0, logger) << "INT_STATE_READ_ENABLE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INT_STATE_READ_ENABLE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_READ_ENABLE reset value correct: 0x" << std::hex << read_val;
    }

    // 0x3c: INT_STATE_READ_ENABLE_REGWEN
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, read_val);
    if (read_val != csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_READ_ENABLE_REGWEN reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_READ_ENABLE_REGWEN reset value correct: 0x" << std::hex << read_val;
    }

    // 0x40: INT_STATE_NUM
    m_test->register_read_32(csrng_basetest::INT_STATE_NUM_OFFSET, read_val);
    if (read_val != csrng_basetest::INT_STATE_NUM_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_NUM reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INT_STATE_NUM_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_NUM reset value correct: 0x" << std::hex << read_val;
    }

    // 0x44: INT_STATE_VAL
    m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, read_val);
    if (read_val != csrng_basetest::INT_STATE_VAL_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_VAL reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::INT_STATE_VAL_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_VAL reset value correct: 0x" << std::hex << read_val;
    }

    // 0x48: FIPS_FORCE
    m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, read_val);
    if (read_val != csrng_basetest::FIPS_FORCE_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: FIPS_FORCE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::FIPS_FORCE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "FIPS_FORCE reset value correct: 0x" << std::hex << read_val;
    }

    // 0x4c: HW_EXC_STS
    m_test->register_read_32(csrng_basetest::HW_EXC_STS_OFFSET, read_val);
    if (read_val != csrng_basetest::HW_EXC_STS_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: HW_EXC_STS reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::HW_EXC_STS_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "HW_EXC_STS reset value correct: 0x" << std::hex << read_val;
    }

    // 0x50: RECOV_ALERT_STS
    m_test->register_read_32(csrng_basetest::RECOV_ALERT_STS_OFFSET, read_val);
    if (read_val != csrng_basetest::RECOV_ALERT_STS_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: RECOV_ALERT_STS reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::RECOV_ALERT_STS_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RECOV_ALERT_STS reset value correct: 0x" << std::hex << read_val;
    }

    // 0x54: ERR_CODE
    m_test->register_read_32(csrng_basetest::ERR_CODE_OFFSET, read_val);
    if (read_val != csrng_basetest::ERR_CODE_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: ERR_CODE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::ERR_CODE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "ERR_CODE reset value correct: 0x" << std::hex << read_val;
    }

    // 0x58: ERR_CODE_TEST
    m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, read_val);
    if (read_val != csrng_basetest::ERR_CODE_TEST_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: ERR_CODE_TEST reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::ERR_CODE_TEST_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "ERR_CODE_TEST reset value correct: 0x" << std::hex << read_val;
    }

    // 0x5c: MAIN_SM_STATE
    m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, read_val);
    if (read_val != csrng_basetest::MAIN_SM_STATE_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: MAIN_SM_STATE reset value mismatch: expected 0x"
                             << std::hex << csrng_basetest::MAIN_SM_STATE_RESET
                             << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "MAIN_SM_STATE reset value correct: 0x" << std::hex << read_val;
    }

    report_test_result("Register Reset Values", all_passed);
}

void testbench::test_register_read_write()
{
    report_test_start("Test: Register Read/Write Access");

    // Apply reset to ensure clean state and REGWEN is unlocked
    apply_reset();

    bool all_passed = true;
    uint32_t write_val = 0;
    uint32_t read_val = 0;
    uint32_t expected = 0;
    uint32_t original_val = 0;

    // Test all RW registers in offset order

    // 0x04: INTR_ENABLE - RW, write mask 0xf
    m_test->register_read_32(csrng_basetest::INTR_ENABLE_OFFSET, original_val);
    write_val = 0xA5A5A5A5 & csrng_basetest::INTR_ENABLE_WRITE;
    m_test->register_write_32(csrng_basetest::INTR_ENABLE_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INTR_ENABLE_OFFSET, read_val);
    wait(10, SC_NS);
    expected = (original_val & ~csrng_basetest::INTR_ENABLE_WRITE) | (write_val & csrng_basetest::INTR_ENABLE_WRITE);
    if (read_val != expected)
    {
        REG_ERROR(0, logger) << "ERROR: INTR_ENABLE read/write mismatch: expected 0x"
                             << std::hex << expected << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INTR_ENABLE read/write correct: 0x" << std::hex << read_val;
    }



    // 0x10: REGWEN - RW0C, write mask 0x0 (special: write 0 to lock, cannot set back to 1)
    // Read current value first
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, original_val);
    // REGWEN is RW0C - we can only write 0 to lock it, writing 1 has no effect
    // For read/write test, we'll just verify we can read it (should be 1 after reset)
    if (original_val != csrng_basetest::REGWEN_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: REGWEN read failed: expected 0x"
                             << std::hex << csrng_basetest::REGWEN_RESET << ", got 0x" << original_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "REGWEN read correct: 0x" << std::hex << original_val;
        REG_INFO(2, logger) << "REGWEN is RW0C (write-0-to-clear), write test skipped to preserve unlock state";
    }

    // 0x14: CTRL - RW, protected by REGWEN, full 32-bit but multi-bit encoded fields
    // Ensure REGWEN is still unlocked (should be after reset)
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) == 0)
    {
        REG_ERROR(0, logger) << "REGWEN is locked, cannot test CTRL write";
        all_passed = false;
    }
    else
    {
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, original_val);
        // Write valid multi-bit encoded value (0x6666 = all fields enabled)
        write_val = 0x6666;
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, read_val);
        wait(10, SC_NS);
        if (read_val != write_val)
        {
            REG_ERROR(0, logger) << "ERROR: CTRL read/write mismatch: expected 0x"
                                 << std::hex << write_val << ", got 0x" << read_val;
            all_passed = false;
        }
        else
        {
            REG_INFO(2, logger) << "CTRL read/write correct: 0x" << std::hex << read_val;
        }
        // Restore original value
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, original_val);
        wait(10, SC_NS);
    }

    // 0x18: CMD_REQ - WO (write-only, command register)
    // Write test value (this will trigger command processing, but we just test write)
    write_val = 0x00000901; // INSTANTIATE command with deterministic flag
    m_test->register_write_32(csrng_basetest::CMD_REQ_OFFSET, write_val);
    wait(10, SC_NS);
    REG_INFO(2, logger) << "CMD_REQ write completed (WO register, command processing may occur)";

    // 0x1c: RESEED_INTERVAL - RW, full 32-bit
    m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, original_val);
    write_val = 0x12345678;
    m_test->register_write_32(csrng_basetest::RESEED_INTERVAL_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::RESEED_INTERVAL_OFFSET, read_val);
    wait(10, SC_NS);
    if (read_val != write_val)
    {
        REG_ERROR(0, logger) << "ERROR: RESEED_INTERVAL read/write mismatch: expected 0x"
                             << std::hex << write_val << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_INTERVAL read/write correct: 0x" << std::hex << read_val;
    }
    // Restore original value
    m_test->register_write_32(csrng_basetest::RESEED_INTERVAL_OFFSET, original_val);
    wait(10, SC_NS);

    // 0x38: INT_STATE_READ_ENABLE - RW, write mask 0x7, protected by INT_STATE_READ_ENABLE_REGWEN
    // Ensure INT_STATE_READ_ENABLE_REGWEN is unlocked (should be after reset)
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) == 0)
    {
        REG_ERROR(0, logger) << "INT_STATE_READ_ENABLE_REGWEN is locked, cannot test INT_STATE_READ_ENABLE write";
        all_passed = false;
    }
    else
    {
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, original_val);
        write_val = 0x5 & csrng_basetest::INT_STATE_READ_ENABLE_WRITE;
        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, read_val);
        wait(10, SC_NS);
        expected = (original_val & ~csrng_basetest::INT_STATE_READ_ENABLE_WRITE) | (write_val & csrng_basetest::INT_STATE_READ_ENABLE_WRITE);
        if (read_val != expected)
        {
            REG_ERROR(0, logger) << "ERROR: INT_STATE_READ_ENABLE read/write mismatch: expected 0x"
                                 << std::hex << expected << ", got 0x" << read_val;
            all_passed = false;
        }
        else
        {
            REG_INFO(2, logger) << "INT_STATE_READ_ENABLE read/write correct: 0x" << std::hex << read_val;
        }
        // Restore original value
        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, original_val);
        wait(10, SC_NS);
    }

    // 0x3c: INT_STATE_READ_ENABLE_REGWEN - RW0C (write-0-to-clear)
    // Read current value (should be 1 after reset)
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, original_val);
    if (original_val != csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_RESET)
    {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_READ_ENABLE_REGWEN read failed: expected 0x"
                             << std::hex << csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_RESET << ", got 0x" << original_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_READ_ENABLE_REGWEN read correct: 0x" << std::hex << original_val;
        REG_INFO(2, logger) << "INT_STATE_READ_ENABLE_REGWEN is RW0C, write test skipped to preserve unlock state";
    }

    // 0x40: INT_STATE_NUM - RW, bits [3:0]
    m_test->register_read_32(csrng_basetest::INT_STATE_NUM_OFFSET, original_val);
    write_val = 0x2; // Select instance 2
    m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INT_STATE_NUM_OFFSET, read_val);
    wait(10, SC_NS);
    expected = (original_val & ~0xF) | (write_val & 0xF);
    if (read_val != expected)
    {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_NUM read/write mismatch: expected 0x"
                             << std::hex << expected << ", got 0x" << read_val;
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_NUM read/write correct: 0x" << std::hex << read_val;
    }
    // Restore original value
    m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, original_val);
    wait(10, SC_NS);

    // 0x48: FIPS_FORCE - RW, write mask 0x7, protected by REGWEN and requires CTRL.FIPS_FORCE_ENABLE
    // Ensure REGWEN is still unlocked
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) == 0)
    {
        REG_ERROR(0, logger) << "REGWEN is locked, cannot test FIPS_FORCE write";
        all_passed = false;
    }
    else
    {
        // Enable FIPS_FORCE_ENABLE in CTRL register (bits [15:12] = 0x6)
        uint32_t ctrl_original = 0;
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, ctrl_original);
        uint32_t ctrl_with_fips_enable = (ctrl_original & ~0xF000) | (0x6 << 12); // Set FIPS_FORCE_ENABLE to 0x6
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, ctrl_with_fips_enable);
        wait(10, SC_NS);

        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, original_val);
        write_val = 0x5 & csrng_basetest::FIPS_FORCE_WRITE;
        m_test->register_write_32(csrng_basetest::FIPS_FORCE_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, read_val);
        wait(10, SC_NS);
        expected = (original_val & ~csrng_basetest::FIPS_FORCE_WRITE) | (write_val & csrng_basetest::FIPS_FORCE_WRITE);
        if (read_val != expected)
        {
            REG_ERROR(0, logger) << "ERROR: FIPS_FORCE read/write mismatch: expected 0x"
                                 << std::hex << expected << ", got 0x" << read_val;
            all_passed = false;
        }
        else
        {
            REG_INFO(2, logger) << "FIPS_FORCE read/write correct: 0x" << std::hex << read_val;
        }
        // Restore original values
        m_test->register_write_32(csrng_basetest::FIPS_FORCE_OFFSET, original_val);
        wait(10, SC_NS);
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, ctrl_original);
        wait(10, SC_NS);
    }



    // 0x58: ERR_CODE_TEST - RW, bits [4:0], protected by REGWEN
    // Ensure REGWEN is still unlocked
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) == 0)
    {
        REG_ERROR(0, logger) << "REGWEN is locked, cannot test ERR_CODE_TEST write";
        all_passed = false;
    }
    else
    {
        m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, original_val);
        write_val = 0x1F & 0x1F; // Test value for bits [4:0]
        m_test->register_write_32(csrng_basetest::ERR_CODE_TEST_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, read_val);
        wait(10, SC_NS);
        expected = (original_val & ~0x1F) | (write_val & 0x1F);
        if (read_val != expected)
        {
            REG_ERROR(0, logger) << "ERROR: ERR_CODE_TEST read/write mismatch: expected 0x"
                                 << std::hex << expected << ", got 0x" << read_val;
            all_passed = false;
        }
        else
        {
            REG_INFO(2, logger) << "ERR_CODE_TEST read/write correct: 0x" << std::hex << read_val;
        }
        // Restore original value
        m_test->register_write_32(csrng_basetest::ERR_CODE_TEST_OFFSET, original_val);
        wait(10, SC_NS);
    }

    report_test_result("Register Read/Write Access", all_passed);
}

void testbench::test_read_only_registers()
{
    report_test_start("Test: Read-Only Register Protection");

    bool all_passed = true;
    uint32_t read_val_before = 0;
    uint32_t read_val_after = 0;
    uint32_t write_val = 0xFFFFFFFF;

    // Test that RESEED_COUNTER_0 is read-only
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_0_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::RESEED_COUNTER_0_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_0_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: RESEED_COUNTER_0 changed after write! Register is not read-only.";
        all_passed = false;
       
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_0 register protection verified (no change after write)";
    }

    // Test that RESEED_COUNTER_1 is read-only
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_1_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::RESEED_COUNTER_1_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_1_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: RESEED_COUNTER_1 changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_1 register protection verified (no change after write)";
    }

    // Test that RESEED_COUNTER_2 is read-only
    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_2_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::RESEED_COUNTER_2_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::RESEED_COUNTER_2_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: RESEED_COUNTER_2 changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "RESEED_COUNTER_2 register protection verified (no change after write)";
    }


    // Test that SW_CMD_STS is read-only
    m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::SW_CMD_STS_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::SW_CMD_STS_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: SW_CMD_STS changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "SW_CMD_STS register protection verified (no change after write)";
    }

    // Test that GENBITS_VLD is read-only
    m_test->register_read_32(csrng_basetest::GENBITS_VLD_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::GENBITS_VLD_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::GENBITS_VLD_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: GENBITS_VLD changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "GENBITS_VLD register protection verified (no change after write)";
    }

    // Test that GENBITS is read-only
    m_test->register_read_32(csrng_basetest::GENBITS_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::GENBITS_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::GENBITS_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: GENBITS changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "GENBITS register protection verified (no change after write)";
    }

    // Test that INT_STATE_VAL is read-only
    m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::INT_STATE_VAL_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::INT_STATE_VAL_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: INT_STATE_VAL changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "INT_STATE_VAL register protection verified (no change after write)";
    }

    // Test that ERR_CODE is read-only
    m_test->register_read_32(csrng_basetest::ERR_CODE_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::ERR_CODE_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::ERR_CODE_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: ERR_CODE changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "ERR_CODE register protection verified (no change after write)";
    }

    // Test that MAIN_SM_STATE is read-only
    m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, read_val_before);
    wait(10, SC_NS);

    // Try to write (should have no effect)
    m_test->register_write_32(csrng_basetest::MAIN_SM_STATE_OFFSET, write_val);
    wait(10, SC_NS);

    m_test->register_read_32(csrng_basetest::MAIN_SM_STATE_OFFSET, read_val_after);
    wait(10, SC_NS);

    // Check write mask - only bits with write_mask=1 should change
    if (read_val_after != read_val_before)
    {
        REG_ERROR(0, logger) <<  "ERROR: MAIN_SM_STATE changed after write! Register is not read-only.";
        all_passed = false;
    }
    else
    {
        REG_INFO(2, logger) << "MAIN_SM_STATE register protection verified (no change after write)";
    }

    report_test_result("Read-Only Register Protection", all_passed);
}




void testbench::test_write_only_registers()
{
    report_test_start("Test: Write-Only Register Protection");

    bool all_passed = true;
    uint32_t read_val = 0;
    uint32_t write_val = 0xA5A5A5A5;

    // Test that INTR_TEST is write-only
    m_test->register_write_32(csrng_basetest::INTR_TEST_OFFSET, write_val);
    wait(10, SC_NS);

    // Try to read back
    m_test->register_read_32(csrng_basetest::INTR_TEST_OFFSET, read_val);
    wait(10, SC_NS);

    // registers should NOT return the written value
    if (read_val != write_val)
    {
        REG_INFO(2, logger) << "INTR_TEST register protection verified (write-only, read blocked)";
    }
    else
    {
         REG_ERROR(0, logger) << "ERROR: INTR_TEST register read returned written value, protection failed";
         all_passed=false;
    }

    // Test that ALERT_TEST is write-only
    m_test->register_write_32(csrng_basetest::ALERT_TEST_OFFSET, write_val);
    wait(10, SC_NS);

    // Try to read back
    m_test->register_read_32(csrng_basetest::ALERT_TEST_OFFSET, read_val);
    wait(10, SC_NS);

    // registers should NOT return the written value
    if (read_val != write_val)
    {
        REG_INFO(2, logger) << "ALERT_TEST register protection verified (write-only, read blocked)";
    }
    else
    {
        REG_ERROR(0, logger) << "ERROR: ALERT_TEST register read returned written value, protection failed";
        all_passed=false;
    }

    // Test that CMD_REQ is write-only
    m_test->register_write_32(csrng_basetest::CMD_REQ_OFFSET, write_val);
    wait(10, SC_NS);

    // Try to read back
    m_test->register_read_32(csrng_basetest::CMD_REQ_OFFSET, read_val);
    wait(10, SC_NS);

    // registers should NOT return the written value
    if (read_val != write_val)
    {
        REG_INFO(2, logger) << "CMD_REQ register protection verified (write-only, read blocked)";
    }
    else{
        REG_ERROR(0, logger) << "ERROR: CMD_REQ register read returned written value, protection failed";
        all_passed=false;
    }

    report_test_result("Write-Only Register Protection", all_passed);
}



void testbench::test_reserved_bits_write_ignore()
{
    report_test_start("Test: Reserved Bits Write Ignore");

    // Apply reset to ensure clean state and REGWEN is unlocked
    apply_reset();

    bool all_passed = true;
    uint32_t write_val = 0xFFFFFFFF;
    uint32_t read_val = 0;
    uint32_t expected = 0;

    // Test INTR_STATE: Bits [31:4] Reserved, RW1C
    m_test->register_read_32(csrng_basetest::INTR_STATE_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::INTR_STATE_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INTR_STATE_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFFFFF0;  // Reserved bits [31:4]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: INTR_STATE reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test INTR_ENABLE: Bits [31:4] Reserved, RW
    m_test->register_read_32(csrng_basetest::INTR_ENABLE_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::INTR_ENABLE_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INTR_ENABLE_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFFFFF0;  // Reserved bits [31:4]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: INTR_ENABLE reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test INTR_TEST: Bits [31:4] Reserved, WO
    m_test->register_write_32(csrng_basetest::INTR_TEST_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INTR_STATE_OFFSET, read_val);
    wait(10, SC_NS);
    // INTR_TEST is write-only, so we verify through INTR_STATE that reserved bits don't affect behavior

    // Test ALERT_TEST: Bits [31:2] Reserved, WO
    m_test->register_write_32(csrng_basetest::ALERT_TEST_OFFSET, write_val);
    wait(10, SC_NS);
    // ALERT_TEST is write-only, reserved bits should be ignored

    // Test REGWEN: Bits [31:1] Reserved, RW0C
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::REGWEN_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFFFFFE;  // Reserved bits [31:1]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: REGWEN reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test CTRL: Bits [31:16] Reserved, RW, protected by REGWEN
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) != 0) {
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, read_val);
        wait(10, SC_NS);
        m_test->register_write_32(csrng_basetest::CTRL_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::CTRL_OFFSET, read_val);
        wait(10, SC_NS);
        expected = read_val & 0xFFFF0000;  // Reserved bits [31:16]
        if (expected != 0) {
            REG_ERROR(0, logger) << "ERROR: CTRL reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
            all_passed = false;
        }
    }

    // Test CMD_REQ: No reserved bits (full 32-bit register), WO
    // Skip reserved bits test as there are no reserved bits

    // Test RESEED_INTERVAL: No reserved bits (full 32-bit register), RW
    // Skip reserved bits test as there are no reserved bits

    // Test INT_STATE_READ_ENABLE: Bits [31:3] Reserved, RW, protected by INT_STATE_READ_ENABLE_REGWEN
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) != 0) {
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, read_val);
        wait(10, SC_NS);
        m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_OFFSET, read_val);
        wait(10, SC_NS);
        expected = read_val & 0xFFFFFFF8;  // Reserved bits [31:3]
        if (expected != 0) {
            REG_ERROR(0, logger) << "ERROR: INT_STATE_READ_ENABLE reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
            all_passed = false;
        }
    }

    // Test INT_STATE_READ_ENABLE_REGWEN: Bits [31:1] Reserved, RW0C
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INT_STATE_READ_ENABLE_REGWEN_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFFFFFE;  // Reserved bits [31:1]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_READ_ENABLE_REGWEN reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test INT_STATE_NUM: Bits [31:4] Reserved, RW
    m_test->register_read_32(csrng_basetest::INT_STATE_NUM_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::INT_STATE_NUM_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::INT_STATE_NUM_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFFFFF0;  // Reserved bits [31:4]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: INT_STATE_NUM reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test FIPS_FORCE: Bits [31:3] Reserved, RW, protected by REGWEN
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) != 0) {
        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, read_val);
        wait(10, SC_NS);
        m_test->register_write_32(csrng_basetest::FIPS_FORCE_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::FIPS_FORCE_OFFSET, read_val);
        wait(10, SC_NS);
        expected = read_val & 0xFFFFFFF8;  // Reserved bits [31:3]
        if (expected != 0) {
            REG_ERROR(0, logger) << "ERROR: FIPS_FORCE reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
            all_passed = false;
        }
    }

    // Test HW_EXC_STS: Bits [31:16] Reserved, RW0C
    m_test->register_read_32(csrng_basetest::HW_EXC_STS_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::HW_EXC_STS_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::HW_EXC_STS_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFF0000;  // Reserved bits [31:16]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: HW_EXC_STS reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test RECOV_ALERT_STS: Bits [31:16, 11:5] Reserved, RW0C
    m_test->register_read_32(csrng_basetest::RECOV_ALERT_STS_OFFSET, read_val);
    wait(10, SC_NS);
    m_test->register_write_32(csrng_basetest::RECOV_ALERT_STS_OFFSET, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(csrng_basetest::RECOV_ALERT_STS_OFFSET, read_val);
    wait(10, SC_NS);
    expected = read_val & 0xFFFF0FE0;  // Reserved bits [31:16, 11:5]
    if (expected != 0) {
        REG_ERROR(0, logger) << "ERROR: RECOV_ALERT_STS reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
        all_passed = false;
    }

    // Test ERR_CODE_TEST: Bits [31:5] Reserved, RW, protected by REGWEN
    m_test->register_read_32(csrng_basetest::REGWEN_OFFSET, read_val);
    if ((read_val & 0x1) != 0) {
        m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, read_val);
        wait(10, SC_NS);
        m_test->register_write_32(csrng_basetest::ERR_CODE_TEST_OFFSET, write_val);
        wait(10, SC_NS);
        m_test->register_read_32(csrng_basetest::ERR_CODE_TEST_OFFSET, read_val);
        wait(10, SC_NS);
        expected = read_val & 0xFFFFFFE0;  // Reserved bits [31:5]
        if (expected != 0) {
            REG_ERROR(0, logger) << "ERROR: ERR_CODE_TEST reserved bits not ignored: expected 0x0, got 0x" << std::hex << expected;
            all_passed = false;
        }
    }

    report_test_result("Reserved Bits Write Ignore", all_passed);
}


void testbench::test_reset_functionality()
{
    report_test_start("Test: Reset Functionality (Stub)");

    bool all_passed = true;

    REG_INFO(2, logger) << "NOTE: Reset functionality not available in csrng_base";
    REG_INFO(2, logger) << "This test will be implemented when CRNG class is complete";

    report_test_result("Reset Functionality (Stub)", all_passed);
}

void testbench::test_interrupt_connections()
{
    report_test_start("Test: Interrupt Connections (Stub)");

    bool all_passed = true;

    REG_INFO(2, logger) << "NOTE: Interrupt ports not available in csrng_base";
    REG_INFO(2, logger) << "This test will be implemented when CRNG class is complete";

    report_test_result("Interrupt Connections (Stub)", all_passed);
}

void testbench::test_entropy_interface()
{
    report_test_start("Test: Entropy Interface (Stub)");

    bool all_passed = true;

    REG_INFO(2, logger) << "NOTE: Entropy interface not available in csrng_base";
    REG_INFO(2, logger) << "This test will be implemented when CRNG class is complete";

    report_test_result("Entropy Interface (Stub)", all_passed);
}

void testbench::test_control_inputs()
{
    report_test_start("Test: Control Inputs (Stub)");

    bool all_passed = true;

    REG_INFO(2, logger) << "NOTE: Control input ports not available in csrng_base";
    REG_INFO(2, logger) << "This test will be implemented when CRNG class is complete";

    report_test_result("Control Inputs (Stub)", all_passed);
}

// =============================================================================
// CRNG_FUNC_014: Alert Test Functionality Tests
// =============================================================================

void testbench::test_alert_test_recov_alert_trigger()
{
    report_test_start("Test: ALERT_TEST Recoverable Alert Trigger");

    bool all_passed = true;

    // Apply reset to ensure clean state
    apply_reset();

    // Verify initial alert state is de-asserted
    if (recov_alert_signal.read() != false) {
        REG_ERROR(0, logger) << "ERROR: recov_alert_o not de-asserted after reset";
        all_passed = false;
    } else {
        REG_INFO(2, logger) << "Initial recov_alert_o state is de-asserted (correct)";
    }

    // Write to ALERT_TEST bit 0 to trigger recoverable alert
    REG_INFO(2, logger) << "Writing 0x1 to ALERT_TEST register (bit 0 = recov_alert)";
    m_test->register_write_32(csrng_basetest::ALERT_TEST_OFFSET, 0x1);
    wait(1, SC_NS);

    // At this point, due to pulse behavior, alert should have de-asserted
    if (recov_alert_signal.read() == false) {
        REG_INFO(2, logger) << "recov_alert_o pulse completed (de-asserted as expected)";
    }
    else {
        REG_ERROR(0, logger) << "ERROR: recov_alert_o did not de-assert after pulse duration";
        all_passed = false;
    }

    // Verify fatal_alert was not triggered
    if (fatal_alert_signal.read() != false) {
        REG_ERROR(0, logger) << "ERROR: fatal_alert_o was unexpectedly asserted";
        all_passed = false;
    } else {
        REG_INFO(2, logger) << "fatal_alert_o remained de-asserted (correct)";
    }

    report_test_result("ALERT_TEST Recoverable Alert Trigger", all_passed);
}

void testbench::test_alert_test_fatal_alert_trigger()
{
    report_test_start("Test: ALERT_TEST Fatal Alert Trigger");

    bool all_passed = true;

    // Apply reset to ensure clean state
    apply_reset();

    // Verify initial alert state is de-asserted
    if (fatal_alert_signal.read() != false) {
        REG_ERROR(0, logger) << "ERROR: fatal_alert_o not de-asserted after reset";
        all_passed = false;
    } else {
        REG_INFO(2, logger) << "Initial fatal_alert_o state is de-asserted (correct)";
    }

    // Write to ALERT_TEST bit 1 to trigger fatal alert
    REG_INFO(2, logger) << "Writing 0x2 to ALERT_TEST register (bit 1 = fatal_alert)";
    m_test->register_write_32(csrng_basetest::ALERT_TEST_OFFSET, 0x2);
    wait(1, SC_NS);

    // At this point, due to pulse behavior, alert should have de-asserted
    if (fatal_alert_signal.read() == false) {
        REG_INFO(2, logger) << "fatal_alert_o pulse completed (de-asserted as expected)";
    }
    else {
        REG_ERROR(0, logger) << "ERROR: fatal_alert_o did not de-assert after pulse duration";
        all_passed = false;
    }

    // Verify recov_alert was not triggered
    if (recov_alert_signal.read() != false) {
        REG_ERROR(0, logger) << "ERROR: recov_alert_o was unexpectedly asserted";
        all_passed = false;
    } else {
        REG_INFO(2, logger) << "recov_alert_o remained de-asserted (correct)";
    }

    report_test_result("ALERT_TEST Fatal Alert Trigger", all_passed);
}

// =============================================================================
// Main Test Execution
// =============================================================================

void testbench::run_tests()
{
    // Set global quantum for temporal decoupling
    tlm::tlm_global_quantum::instance().set(sc_time(100, SC_NS));

    wait(10, SC_NS);

    REG_INFO(1, logger) << "\n========================================"
                         << "\nCRNG IP TESTBENCH (Full Model)"
                         << "\n========================================";

    REG_INFO(1, logger) << "Testing csrng_model with full FSM, callbacks, and command processing";

    // Test 1: Basic Register Tests
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest 1: Register Access Tests"
                         << "\n========================================";

    test_register_reset_values();
    wait(20, SC_NS);

    test_register_read_write();
    wait(20, SC_NS);

    test_read_only_registers();
    wait(20, SC_NS);

    test_reserved_bits_read_zero();
    wait(20, SC_NS);

    // Test: Reserved Bits Write Ignore
    test_reserved_bits_write_ignore();
    wait(20, SC_NS);

    test_write_only_registers();
    wait(20, SC_NS);

    test_generate_single_block();
    wait(20, SC_NS);
    test_generate_maximum_blocks();
    wait(20, SC_NS);
    test_generate_genbits_fips_flag_compliant();
    wait(20, SC_NS);
    test_regwen_write_1_no_effect();
    wait(15, SC_NS);
    test_int_state_read_enable_regwen_lock();
    wait(20, SC_NS);
    test_int_state_num_invalid_range();
    wait(20, SC_NS);

    test_ctrl_invalid_encoding_alert();
    wait(15, SC_NS);
    test_regwen_lock_mechanism();
    wait(15, SC_NS);
    test_int_state_num_valid_range();
    wait(20, SC_NS);
    test_module_disable_command_rejection();

    test_module_enable_after_disable();
    wait(20, SC_NS);

    test_command_sequence_instantiate_generate_reseed_uninstantiate();
    wait(30, SC_NS);

    test_command_sequence_recovery_via_uninstantiate();
    wait(30, SC_NS);


    test_reseed_basic_with_entropy();
    wait(20, SC_NS);
    test_reseed_deterministic_mode();
    wait(20, SC_NS);
    test_reseed_with_additional_input();
    wait(20, SC_NS);
    test_reseed_counter_reset_verification();
    wait(20, SC_NS);
    test_reseed_entropy_request_interrupt();
    wait(20, SC_NS);
    test_reseed_uninstantiated_instance_error();
    wait(20, SC_NS);
    test_reseed_extends_seed_life();
    wait(30, SC_NS);

    test_instantiate_deterministic_mode();
    wait(20, SC_NS);
    // test_023_instantiate_max_additional_data();
    // wait(20, SC_NS);
    // test_024_instantiate_reseed_counter_zero();
    // wait(20, SC_NS);
    // test_025_instantiate_entropy_request_interrupt();
    // wait(20, SC_NS);
     test_instantiate_already_instantiated_error();
     wait(20, SC_NS);
    test_instantiate_invalid_flag0_encoding();
    wait(20, SC_NS);
    // test_028_instantiate_cmd_rdy_polling();
    // wait(20, SC_NS);
    // test_029_instantiate_cmd_ack_polling();
    // wait(30, SC_NS);
    test_combined_instantiate_polling_and_verification();
    wait(30, SC_NS);
    




    test_generate_genbits_fips_flag_deterministic();
    wait(20, SC_NS);
    test_generate_with_additional_input();
    wait(20, SC_NS);
    test_generate_uninstantiated_instance_error();
    wait(20, SC_NS);
    test_generate_reseed_cnt_exceeded_error();
    wait(20, SC_NS);
    // test_040_generate_non_blocking_interleaved();
    // wait(30, SC_NS);


    test_update_basic_with_additional_data();
    wait(20, SC_NS);
    test__update_reseed_counter_unchanged();
    wait(20, SC_NS);
    test_update_max_additional_data();
    wait(20, SC_NS);
    test_update_uninstantiated_instance_error();
    wait(20, SC_NS);
    test_update_no_entropy_request();
    wait(30, SC_NS);


    test_uninstantiate_instantiated_instance();
    wait(20, SC_NS);
    test_uninstantiate_state_cleared();
    wait(20, SC_NS);
    test_uninstantiate_already_uninstantiated();
    wait(20, SC_NS);
    test_uninstantiate_reseed_counter_cleared();
    wait(20, SC_NS);
    test_uninstantiate_hw_exc_sts_cleared();
    wait(30, SC_NS);

    // Interrupt test added

    test_interrupt_cs_cmd_req_done_assertion();
    wait(20, SC_NS);

    test_interrupt_cs_cmd_req_done_deassertion();
    wait(20, SC_NS);
    test_interrupt_cs_entropy_req_assertion();
    wait(20, SC_NS);
    test_interrupt_cs_entropy_req_deassertion();
    wait(20, SC_NS);
    test_interrupt_cs_hw_inst_exc_assertion();
    wait(20, SC_NS);
    test_interrupt_cs_hw_inst_exc_deassertion();
    wait(20, SC_NS);
    test_interrupt_cs_fatal_err_assertion();  
    wait(20, SC_NS);
    test_interrupt_cs_fatal_err_sticky();
    wait(20, SC_NS);
    test_interrupt_enable_gating();
    wait(20, SC_NS);
    test_interrupt_test_mode_cs_cmd_req_done();
    wait(20, SC_NS);
    test_interrupt_test_mode_cs_entropy_req();
    wait(20, SC_NS);
    test_interrupt_test_mode_cs_hw_inst_exc();
    wait(20, SC_NS);
    test_interrupt_test_mode_cs_fatal_err();
    wait(20, SC_NS);
    test_interrupt_multiple_simultaneous_sources();
    wait(20, SC_NS);
    test_interrupt_state_accumulation();
    wait(20, SC_NS);
    

    test_combined_invalid_acmd_values();
    wait(20, SC_NS);
    test_invalid_clen_greater_than_12();
    wait(20, SC_NS);
    test_invalid_glen_zero();
    wait(20, SC_NS);


    test_reset_clears_all_instance_states();    
    wait(20, SC_NS);
    test_reset_during_command_processing();

    test_error_code_fifo_write_error_injection();
    wait(20, SC_NS);
    test_error_code_fifo_read_error_injection();
    wait(20, SC_NS);
    test_error_code_fsm_illegal_state_main_sm();
    wait(20, SC_NS);
    test_error_code_fsm_illegal_state_cmd_stage();
    wait(20, SC_NS);
    test_error_code_sticky_behavior();
    wait(20, SC_NS);
    test_error_code_multiple_errors();
    wait(20, SC_NS);
    test_recov_alert_sts_clear_mechanism();
    wait(20, SC_NS);
    test_recov_alert_sts_multiple_alerts();
    wait(20, SC_NS);


    // // Test 2: Port Interface Stubs
    // REG_INFO(1, logger) << "\n========================================"
    //                      << "\nTest 2: Port Interface Tests (Stubs)"
    //                      << "\n========================================";

    // test_reset_functionality();
    // wait(20, SC_NS);

    // test_interrupt_connections();
    // wait(20, SC_NS);

    // test_entropy_interface();
    // wait(20, SC_NS);

    // test_control_inputs();
    // wait(20, SC_NS);

    // =========================================================================
    // Test Suite 3: CRNG_FUNC_008 - Register Callbacks (Priority 1)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 3: CRNG_FUNC_008 - Register Callbacks"
                         << "\n========================================";
    apply_reset();
    // Tests 001-016: Register Reset Values
    // test_001_intr_state_reset();
    // wait(15, SC_NS);
    // test_002_intr_enable_reset();
    // wait(15, SC_NS);
    // test_003_intr_test_reset();
    // wait(15, SC_NS);
    // test_004_alert_test_reset();
    // wait(15, SC_NS);
    // test_005_regwen_reset();
    // wait(15, SC_NS);
    // test_006_ctrl_reset();
    // wait(15, SC_NS);
    // test_007_cmd_req_reset();
    // wait(15, SC_NS);
    // test_008_reseed_interval_reset();
    // wait(15, SC_NS);
    // test_009_reseed_counter_0_reset();
    // wait(15, SC_NS);
    // test_010_sw_cmd_sts_reset();
    // wait(15, SC_NS);
    // test_011_genbits_vld_reset();
    // wait(15, SC_NS);
    // test_012_int_state_read_enable_reset();
    // wait(15, SC_NS);
    // test_013_hw_exc_sts_reset();
    // wait(15, SC_NS);
    // test_014_recov_alert_sts_reset();
    // wait(15, SC_NS);
    // test_015_err_code_reset();
    // wait(15, SC_NS);
    // test_016_main_sm_state_reset();
    // wait(20, SC_NS);

    // Test 034: Multi-bit Encoding
    // test_034_ctrl_multibit_encoding();
    // wait(20, SC_NS);

    // Tests 069-077: REGWEN Lock Mechanism
    // test_069_regwen_lock_basic();
    // wait(20, SC_NS);
    // test_070_ctrl_regwen_protection();
    // wait(20, SC_NS);
    // test_071_batch_reset_verification();
    // wait(20, SC_NS);
    // test_072_fips_force_regwen_protection();
    // wait(20, SC_NS);
    // test_073_err_code_test_regwen_protection();
    // wait(20, SC_NS);
    // test_074_regwen_lock_persistence();
    // wait(20, SC_NS);
    // test_075_multiple_locked_register_writes();
    // wait(20, SC_NS);
    // test_076_regwen_write_one_when_locked();
    // wait(20, SC_NS);
    // test_077_regwen_unlock_only_by_reset();
    // wait(20, SC_NS);

    // // Tests 085-093: Read/Write Access Masks
    // test_085_intr_enable_rw_mask();
    // wait(20, SC_NS);
    // test_086_sw_cmd_sts_readonly();
    // wait(20, SC_NS);
    // test_087_genbits_readonly();
    // wait(20, SC_NS);
    // test_088_alert_test_write_only();
    // wait(20, SC_NS);
    // test_089_reseed_counter_0_readonly();
    // wait(20, SC_NS);
    // test_090_hw_exc_sts_rw0c();
    // wait(20, SC_NS);
    // test_091_recov_alert_sts_rw0c();
    // wait(20, SC_NS);
    // test_092_err_code_readonly();
    // wait(20, SC_NS);
    // test_093_main_sm_state_readonly();
    // wait(20, SC_NS);

    test_genbits_no_repetition_normal_operation();
    wait(20, SC_NS);
    test_invalid_command_field_validation();
    wait(20, SC_NS);

    // Test 109: Comprehensive Access Masks
    // test_109_comprehensive_access_masks();
    // wait(20, SC_NS);

    // Test 183: Register Lock Comprehensive
    test_183_register_lock_comprehensive();
    wait(20, SC_NS);

    // =========================================================================
    // Test Suite 4: CRNG_FUNC_001 - DRBG Lifecycle Management (Priority 1)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 4: CRNG_FUNC_001 - DRBG Lifecycle"
                         << "\n========================================";

    // Apply reset to unlock REGWEN and ensure clean state
    apply_reset();
    wait(10, SC_NS);

    // Tests 020-029: INSTANTIATE Command Tests
    // test_020_instantiate_basic_no_additional_data();
    // wait(20, SC_NS);
    // test_021_instantiate_with_personalization_data();
    // wait(20, SC_NS);


    // Tests 030-040: GENERATE Command Tests
    //test_031_generate_multiple_blocks();
    //wait(30, SC_NS);
    //test_033_generate_reseed_counter_increment();
    //wait(20, SC_NS);
    // test_034_generate_genbits_vld_behavior();
    // wait(20, SC_NS );

    // Tests 058-063: Command Sequence Validation Tests
    // test_059_command_sequence_double_instantiate_error();
    // wait(20, SC_NS);
    // test_060_command_sequence_generate_before_instantiate_error();
    // wait(20, SC_NS);
    // test_061_command_sequence_reseed_before_instantiate_error();
    // wait(20, SC_NS);
    // test_062_command_sequence_update_before_instantiate_error();
    // wait(20, SC_NS);

    // =========================================================================
    // Test Suite 7: CRNG_FUNC_002 - Pseudorandom Bit Generation (Priority 1)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 7: CRNG_FUNC_002 - Pseudorandom Bit Generation"
                         << "\n========================================";

    // Apply reset to unlock REGWEN and ensure clean state
    apply_reset();
    wait(10, SC_NS);

    // Tests 030-040: GENERATE Command Tests (already called in Suite 4 above)
    // Note: Tests 030-040 are shared between FUNC_001 and FUNC_002

    // Tests 068: Invalid Parameter Tests

    // Tests 069-071: Access Control Tests (already called in Suite 5 below)

    // Tests 085-087: Sequential Read Pointer Tests
    test_genbits_sequential_read_4_words();
    wait(20, SC_NS);
    test_genbits_read_pointer_wrap_after_4th_read();
    wait(20, SC_NS);
    test_genbits_multiple_blocks_pointer_management();
    wait(20, SC_NS);

    // Tests 095-100: FIPS Compliance Tests
    // test_095_fips_compliance_entropy_mode();
    // wait(20, SC_NS);
    // test_096_fips_compliance_deterministic_mode_no_force();
    // wait(20, SC_NS);
    test_fips_force_deterministic_with_fips_assertion();
    wait(20, SC_NS);
    test_fips_force_per_instance_instance0();
    wait(20, SC_NS);
    test_fips_force_per_instance_instance1();
    wait(20, SC_NS);
    test_fips_compliance_after_reseed_entropy();
    wait(20, SC_NS);

    // Tests 149, 157, 193-194: Corner Cases and Boundary Values
    test_corner_case_genbits_read_without_generate();
    wait(20, SC_NS);
    test_corner_case_all_ctrl_fields_disabled_and_enabled();
    wait(20, SC_NS);
    test_corner_case_rapid_instantiate_uninstantiate_cycle();
    wait(20, SC_NS);
    test_corner_case_fips_force_all_instances();
    wait(15, SC_NS);
    // test_193_boundary_cmd_req_glen_min_1();
    // wait(20, SC_NS);
    // test_194_boundary_cmd_req_glen_max_4095();  // COMMENTED OUT - too slow (4095 blocks)
    // wait(30, SC_NS);

    // =========================================================================
    // Test Suite 5: CRNG_FUNC_009 - Control and Configuration (Priority 1)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 5: CRNG_FUNC_009 - Control and Configuration"
                         << "\n========================================";

    // Apply reset to unlock REGWEN and ensure clean state
    apply_reset();
    wait(10, SC_NS);

    // Tests 002-012: Register Access and Configuration
    ctrl_enable_field_write_read();
    wait(15, SC_NS);
    ctrl_sw_app_enable_field_write_read();
    wait(15, SC_NS);
    ctrl_read_int_state_field_write_read();
    wait(15, SC_NS);
    ctrl_fips_force_enable_field_write_read();
    wait(15, SC_NS);

    // Reset to unlock REGWEN for subsequent tests
    apply_reset();
    wait(10, SC_NS);

    // test_009_reseed_interval_boundary_values();
    // wait(15, SC_NS);
    // test_010_fips_force_per_instance_bits();
    // wait(15, SC_NS);
    // test_011_int_state_read_enable_per_instance();
    // wait(15, SC_NS);

    // Tests 017-019: Module Enable/Disable
    // test_017_module_enable_command_processing();
    // wait(15, SC_NS);

    // Tests 177-178: Reset and Boundary Value Tests
    // test_177_reset_unlocks_regwen();
    // wait(15, SC_NS);


    // =========================================================================
    // Test Suite 6: CRNG_FUNC_005 - Command Interface and FSM (Priority 1)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 6: CRNG_FUNC_005 - Command Interface and FSM"
                         << "\n========================================";

    // Apply reset to unlock REGWEN and ensure clean state
    apply_reset();
    wait(10, SC_NS);

    // Tests 078-097: Command Interface Protocol Tests (SW_CMD_STS behavior)
    // test_078_cmd_rdy_initial_state_after_enable();
    // wait(15, SC_NS);
    // test_079_cmd_rdy_clears_during_processing();
    // wait(15, SC_NS);
    // test_080_cmd_ack_initial_state();
    // wait(15, SC_NS);
    // test_081_cmd_ack_sets_on_completion();
    // wait(15, SC_NS);
    // test_082_cmd_ack_clears_on_new_command();
    // wait(15, SC_NS);
    // test_083_cmd_sts_success_code();
    // wait(15, SC_NS);
    // test_084_cmd_sts_invalid_acmd_code();
    // wait(15, SC_NS);
    // test_200_cmd_sts_invalid_cmd_seq_code();
    // wait(15, SC_NS);
    // test_201_sw_cmd_sts_field_persistence();
    // wait(15, SC_NS);
    // test_202_cmd_rdy_blocking_behavior();
    // wait(15, SC_NS);
    // test_088_multiple_consecutive_commands_cycling();
    // wait(20, SC_NS);
    // test_089_cmd_sts_encoding_completeness();
    // wait(15, SC_NS);
    // test_090_cmd_rdy_ack_across_disable_enable();
    // wait(15, SC_NS);
    // test_091_sw_cmd_sts_reserved_bits_zero();
    // wait(15, SC_NS);
    // test_092_sw_cmd_sts_readonly_verification();
    // wait(15, SC_NS);
    // test_093_cmd_ack_vs_interrupt_equivalence();
    // wait(15, SC_NS);
    // test_094_back_to_back_command_execution();
    // wait(20, SC_NS);
    // test_203_cmd_sts_persistence_until_next_command();
    // wait(15, SC_NS);
    // test_204_command_interface_after_error();
    // wait(15, SC_NS);
    // test_205_cmd_req_write_only_verification();
    // wait(20, SC_NS);

    // Tests 098-107: FSM State Transition Tests (MAIN_SM_STATE monitoring)
    // test_206_main_sm_state_idle_after_reset();
    // wait(15, SC_NS);
    // test_207_main_sm_state_during_command();
    // wait(15, SC_NS);
    // test_208_main_sm_state_returns_idle();
    // wait(15, SC_NS);
    // test_209_main_sm_state_readonly();
    // wait(15, SC_NS);
    // test_210_main_sm_state_reserved_bits();
    // wait(15, SC_NS);
    // test_211_main_sm_state_during_error();
    // wait(15, SC_NS);
    // test_212_main_sm_state_multiple_sampling();
    // wait(15, SC_NS);
    // test_213_main_sm_state_stability_idle();
    // wait(15, SC_NS);
    // test_214_main_sm_state_across_disable_enable();
    // wait(15, SC_NS);
    // test_215_main_sm_state_all_commands();
    // wait(20, SC_NS);

    // // Tests 216-221: Command Timing and Arbitration Tests
    // test_216_command_processing_latency();
    // wait(15, SC_NS);
    // test_217_cmd_rdy_ack_timing_relationship();
    // wait(15, SC_NS);
    // test_218_rapid_command_succession_timing();
    // wait(20, SC_NS);
    // test_219_fsm_state_transition_timing();
    // wait(15, SC_NS);
    // test_220_command_timeout_detection();
    // wait(15, SC_NS);
    // test_221_command_interface_synchronization();
    // wait(20, SC_NS);

    // =========================================================================
    // Test Suite 8: CRNG_FUNC_003 - Seed Life Management (Priority 2)
    // =========================================================================
    REG_INFO(1, logger) << "\n========================================"
                         << "\nTest Suite 8: CRNG_FUNC_003 - Seed Life Management"
                         << "\n========================================";

    // Apply reset to unlock REGWEN and ensure clean state
    apply_reset();
    wait(10, SC_NS);

    // Tests 101-107: Reseed Interval Enforcement Tests
    // test_101_reseed_interval_enforcement_exact_threshold();
    // wait(20, SC_NS);
    test_reseed_interval_enforcement_below_threshold();
    wait(20, SC_NS);
    test_reseed_interval_enforcement_disabled();
    wait(20, SC_NS);
    test_reseed_interval_enforcement_after_reseed_recovery();
    wait(20, SC_NS);
    test_reseed_interval_enforcement_after_instantiate_recovery();
    wait(20, SC_NS);
    test_reseed_interval_enforcement_per_instance_independent();
    wait(15, SC_NS);
    test_reseed_interval_alert_on_exceeded();
    wait(20, SC_NS);

    // Tests 146-147: Corner Case Interval Boundary Tests
    // test_146_corner_case_reseed_interval_zero();
    // wait(15, SC_NS);
    // test_147_corner_case_reseed_interval_one();
    // wait(20, SC_NS);

    // Tests 195-196: Boundary Value Interval Tests
    test_boundary_reseed_interval_min_0();
    wait(15, SC_NS);
    test_boundary_reseed_interval_max_0xFFFFFFFF();
    wait(20, SC_NS);
 
    test_cmd_req_flag0_invalid_encoding_comprehensive();
    wait(20, SC_NS);

    
    test_int_state_val_sequential_read_14_words();
    wait(15, SC_NS);
    test_int_state_val_pointer_wrap_after_14th_read();
    wait(15, SC_NS);

    test_sw_flow_initialization_sequence();
    wait(15, SC_NS);
    test_sw_flow_instantiate_generate_loop();
    wait(20, SC_NS);
    // test_158_sw_flow_reseed_interval_enforcement_recovery();
    // wait(20, SC_NS);
    test_sw_flow_error_recovery_sequence();
    wait(20, SC_NS);
    test_sw_flow_interrupt_driven_operation();
    wait(20, SC_NS);
    test_sw_flow_polling_operation();
    wait(20, SC_NS);
    test_sw_flow_shutdown_sequence();
    wait(15, SC_NS);
    test_sw_flow_internal_state_inspection();
    wait(15, SC_NS);
    // test_162_sw_flow_deterministic_kat_mode();
    // wait(20, SC_NS);

    test_boundary_int_state_num_min_0();
    wait(15, SC_NS);
    test_boundary_fips_force_all_bits_set();
    wait(20, SC_NS);
    test_boundary_fips_force_all_bits_clear();
    wait(20, SC_NS);


    // Tests 069-077: Access Control
    test_genbits_access_ctrl_sw_app_enable_disabled();
    wait(15, SC_NS);
    test_genbits_access_ctrl_otp_disabled();
    wait(15, SC_NS);
    test_genbits_access_ctrl_both_enabled();
    wait(15, SC_NS);
    test_int_state_val_access_ctrl_read_int_state_disabled();
    wait(15, SC_NS);
    test_int_state_val_access_ctrl_otp_disabled();
    wait(15, SC_NS);
    // test_074_int_state_val_access_ctrl_instance_disabled();
    // wait(15, SC_NS);
    // test_077_int_state_val_access_ctrl_all_conditions_met();
    // wait(20, SC_NS);

    test_reset_clears_interrupt_states();
    wait(20, SC_NS);
    test_reset_clears_error_codes();
    wait(20, SC_NS);
    test_reset_disables_module();
    wait(20, SC_NS);
    test_fatal_error_recovery_via_reset();
    wait(20, SC_NS);
    test_int_state_val_pointer_reset_on_int_state_num_write();
    wait(15, SC_NS);
    test_int_state_val_read_multiple_instances();
    wait(15, SC_NS);
    test_alert_test_recov_alert_trigger();
    wait(20, SC_NS);
    test_alert_test_fatal_alert_trigger();
    wait(20, SC_NS);
    test_int_state_val_reseed_status_fips();
    wait(20, SC_NS);

    REG_INFO(1, logger) << "\n========================================"
                         << "\nCoverage: uncovered model paths"
                         << "\n========================================";
    apply_reset();
    test_coverage_invalid_instance_and_helpers();
    wait(20, SC_NS);
    test_coverage_fsm_states_and_genbits_repeat();
    wait(20, SC_NS);
    test_coverage_int_state_and_regwen_denies();
    wait(20, SC_NS);
    test_coverage_invalid_instance_and_seed_life();
    wait(20, SC_NS);
    test_coverage_update_requires_additional_data();
    wait(20, SC_NS);
    test_coverage_int_state_num_out_of_range();
    wait(20, SC_NS);
    test_coverage_reseed_interval_locked_and_fsm_states();
    test_malformed_payloads();
    wait(20, SC_NS);

    report_test_start("HW app/genbits production interfaces");
    apply_reset();
    uint32_t ack = 0;
    const uint32_t instantiate = 0x1;
    m_crng->hw_app_export[0]->send_command(&instantiate, 1, ack);
    bool interface_ok = (ack != 0);
    m_test->register_write_32(csrng_basetest::CTRL_OFFSET, 0x9996);
    wait(20, SC_NS);
    m_crng->hw_app_export[0]->send_command(nullptr, 0, ack);
    interface_ok = interface_ok && (ack != 0);
    const uint32_t bad_clen = 0x11;
    m_crng->hw_app_export[0]->send_command(&bad_clen, 1, ack);
    interface_ok = interface_ok && (ack != 0);
    const uint32_t update_words[2] = {0x14, 0xA5A5A5A5};
    m_crng->hw_app_export[0]->send_command(update_words, 2, ack);
    interface_ok = interface_ok && (ack != 0) &&
                   (m_crng->hw_app_export[0]->get_ack_status() == ack);
    m_crng->hw_app_export[0]->send_command(&instantiate, 1, ack);
    interface_ok = interface_ok && (ack == 0) &&
                   m_crng->hw_app_export[0]->is_ready();
    const uint32_t generate = (2u << 12) | 0x3u;
    m_crng->hw_app_export[0]->send_command(&generate, 1, ack);
    interface_ok = interface_ok && (ack == 0) && m_crng->genbits_export->has_data();
    uint32_t intr_state = 0;
    m_test->register_read_32(csrng_basetest::INTR_STATE_OFFSET, intr_state);
    interface_ok = interface_ok && ((intr_state & 0x1u) == 0);
    uint32_t words[4] = {};
    bool fips = false;
    m_crng->genbits_export->receive_genbits(words, fips);
    interface_ok = interface_ok &&
                   ((words[0] | words[1] | words[2] | words[3]) != 0);
    m_crng->genbits_export->receive_genbits(words, fips);
    m_crng->genbits_export->receive_genbits(nullptr, fips);
    words[0] = 0xFFFFFFFFu;
    m_crng->genbits_export->receive_genbits(words, fips);
    interface_ok = interface_ok && (words[0] == 0) && !fips;
    m_crng->genbits_export->provide_genbits(words, false);
    if (interface_ok) {
        report_test_pass("HW app/genbits production interfaces");
    } else {
        report_test_fail("HW app/genbits production interfaces",
                         "command acknowledgment or generated block was invalid");
    }
    wait(20, SC_NS);

    // Print final test summary
    REG_INFO(1, logger) << "\n========================================"
                         << "\n       TEST SUITE SUMMARY"
                         << "\n========================================";

    std::stringstream ss;
    ss << "Total Tests:  " << m_tests_run;
    REG_INFO(1, logger) << ss.str();

    ss.str("");
    ss << "Passed:       " << m_tests_passed << " (PASS)";
    REG_INFO(1, logger) << ss.str();

    ss.str("");
    ss << "Failed:       " << m_tests_failed << " (FAIL)";
    REG_INFO(1, logger) << ss.str();

    if (m_tests_run > 0) {
        double success_rate = (100.0 * m_tests_passed) / m_tests_run;
        ss.str("");
        ss << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%";
        REG_INFO(1, logger) << ss.str();
    }

    REG_INFO(1, logger) << "========================================";

    // Show list of failed tests if any
    if (m_tests_failed > 0) {
        REG_ERROR(0, logger) << "\nFailed Tests:";
        for (const auto& test : m_failed_tests) {
            ss.str("");
            ss << "  - " << test;
            REG_ERROR(0, logger) << ss.str();
        }
        ss.str("");
        ss << "\n[OVERALL RESULT: FAILED - " << m_tests_failed << " test(s) failed]";
        REG_ERROR(0, logger) << ss.str();
    } else if (m_tests_passed > 0) {
        REG_INFO(1, logger) << "[OVERALL RESULT: PASSED - All tests passed]";
    } else {
        REG_ERROR(0, logger) << "[OVERALL RESULT: NO TESTS RUN]";
    }

    REG_INFO(1, logger) << "========================================\n";

    wait(100, SC_NS);
    sc_stop();
}
