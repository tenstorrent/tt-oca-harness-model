#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-008: Alert Generation and Error Reporting Test Implementations
// =============================================================================
//
// This test file covers the two-tier alert mechanism:
// - Recoverable alerts: Shadowed register write mismatches
// - Fatal alerts: Critical faults requiring system reset
//
// Key test coverage:
// - ALERT_TEST register triggering test alerts
// - Fatal alert behavior and terminal ERROR state
// - Life cycle escalation interface
// - Register clearing on fatal alert
// - Alert status visibility in STATUS register
//
// =============================================================================

// =============================================================================
// Test Case 1: ALERT_TEST Register - Recoverable Alert Trigger
// =============================================================================

void testbench::test_alert_test_register_recoverable_trigger()
{
    report_test_start("test_alert_test_register_recoverable_trigger");

    try {
        // Ensure AES is idle and no alerts are active
        wait_for_idle(1000);

        // Verify alert signal is not asserted before test
        if (alert_recov_signal.read()) {
            report_test_fail("test_alert_test_register_recoverable_trigger",
                           "alert_recov_ctrl_update_err already asserted before test");
            return;
        }

        // Write ALERT_TEST bit 0 to trigger recoverable alert
        // ALERT_TEST.recov_ctrl_update_err = 1 (bit 0)
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000001);
        wait(10, SC_NS);

        // Verify recoverable alert signal is asserted
        if (!alert_recov_signal.read()) {
            report_test_fail("test_alert_test_register_recoverable_trigger",
                           "alert_recov_ctrl_update_err not asserted after ALERT_TEST write");
            return;
        }

        // Verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR is set
        uint32_t status_after = read_status();
        bool alert_recov_after = (status_after & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert_recov_after) {
            report_test_fail("test_alert_test_register_recoverable_trigger",
                           "STATUS.ALERT_RECOV_CTRL_UPDATE_ERR not set after test alert");
            return;
        }

        report_test_pass("test_alert_test_register_recoverable_trigger");

    } catch (const std::exception& e) {
        report_test_fail("test_alert_test_register_recoverable_trigger", e.what());
    }
}

// =============================================================================
// Test Case 2: ALERT_TEST Register - Fatal Alert Trigger and Terminal ERROR State
// =============================================================================

void testbench::test_alert_test_register_fatal_trigger()
{
    report_test_start("test_alert_test_register_fatal_trigger");

    try {
        // Ensure AES is idle before test
        wait_for_idle(1000);

        // Verify fatal alert signal is not asserted before test
        if (alert_fatal_signal.read()) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "alert_fatal_fault already asserted before test");
            return;
        }

        // Read initial STATUS
        uint32_t status_before = read_status();
        bool alert_fatal_before = (status_before & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (alert_fatal_before) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "STATUS.ALERT_FATAL_FAULT already set before test");
            return;
        }

        // Write ALERT_TEST bit 1 to trigger fatal alert
        // ALERT_TEST.fatal_fault = 1 (bit 1)
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
        wait(20, SC_NS);  // Allow time for fatal alert processing

        // Verify fatal alert signal is asserted
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "alert_fatal_fault not asserted after ALERT_TEST write");
            return;
        }

        // Verify STATUS.ALERT_FATAL_FAULT is set
        uint32_t status_after = read_status();
        bool alert_fatal_after = (status_after & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!alert_fatal_after) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "STATUS.ALERT_FATAL_FAULT not set after fatal alert");
            return;
        }

        // Verify AES transitions to non-IDLE state (ERROR state)
        bool is_idle = (status_after & (1 << STATUS_IDLE_BIT)) != 0;

        if (is_idle) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "AES should not be IDLE after fatal alert (should be in ERROR state)");
            return;
        }

        // Verify idle output port reflects ERROR state
        if (idle_signal.read()) {
            report_test_fail("test_alert_test_register_fatal_trigger",
                           "idle_o should be false after fatal alert");
            return;
        }

        report_test_pass("test_alert_test_register_fatal_trigger");

    } catch (const std::exception& e) {
        report_test_fail("test_alert_test_register_fatal_trigger", e.what());
    }
}

// =============================================================================
// Test Case 3: Terminal ERROR State - Register Writes Ignored
// =============================================================================

void testbench::test_fatal_alert_terminal_error_state()
{
    report_test_start("test_fatal_alert_terminal_error_state");

    try {
        // This test assumes AES is already in ERROR state from previous test
        // If not, trigger fatal alert first
        uint32_t status = read_status();
        bool in_error = (status & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!in_error) {
            // Trigger fatal alert to enter ERROR state
            m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
            wait(20, SC_NS);

            status = read_status();
            in_error = (status & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

            if (!in_error) {
                report_test_fail("test_fatal_alert_terminal_error_state",
                               "Failed to enter ERROR state for testing");
                return;
            }
        }

        // Verify AES is in terminal ERROR state
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_fatal_alert_terminal_error_state",
                           "alert_fatal_fault not asserted - not in ERROR state");
            return;
        }

        // Attempt to write CTRL_SHADOWED (should be ignored)
        uint32_t ctrl_before;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_before);
        wait(10, SC_NS);

        uint32_t new_ctrl_value = 0x12345678;
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, new_ctrl_value);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, new_ctrl_value);
        wait(10, SC_NS);

        uint32_t ctrl_after;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_after);
        wait(10, SC_NS);

        if (ctrl_after != ctrl_before) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED incorrectly updated in ERROR state: was 0x"
               << std::hex << ctrl_before << ", now 0x" << ctrl_after;
            report_test_fail("test_fatal_alert_terminal_error_state", ss.str());
            return;
        }

        // Read IV and DATA_IN before attempting writes
        uint32_t iv_before[4], data_in_before[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_before[i]);
            m_test->register_read_32(aes_basetest::DATA_IN_OFFSET + i*4, data_in_before[i]);
        }
        wait(10, SC_NS);

        // Attempt to write IV_0 (should be ignored)
        uint32_t test_iv_value = 0xCAFEBABE;
        m_test->register_write_32(aes_basetest::IV_OFFSET, test_iv_value);
        wait(10, SC_NS);

        // Attempt to write DATA_IN_0 (should be ignored)
        uint32_t test_data_value = 0xBAADF00D;
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET, test_data_value);
        wait(10, SC_NS);


        // Verify IV and DATA_IN were NOT modified
        uint32_t iv_after[4], data_in_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + i*4, iv_after[i]);
            m_test->register_read_32(aes_basetest::DATA_IN_OFFSET + i*4, data_in_after[i]);
        }
        wait(10, SC_NS);

        // Check IV was not modified
        for (int i = 0; i < 4; i++) {
            if (iv_after[i] != iv_before[i]) {
                std::stringstream ss;
                ss << "IV[" << i << "] incorrectly modified in ERROR state: was 0x"
                   << std::hex << iv_before[i] << ", now 0x" << iv_after[i];
                report_test_fail("test_fatal_alert_terminal_error_state", ss.str());
                return;
            }
        }

        // Check DATA_IN was not modified
        for (int i = 0; i < 4; i++) {
            if (data_in_after[i] != data_in_before[i]) {
                std::stringstream ss;
                ss << "DATA_IN[" << i << "] incorrectly modified in ERROR state: was 0x"
                   << std::hex << data_in_before[i] << ", now 0x" << data_in_after[i];
                report_test_fail("test_fatal_alert_terminal_error_state", ss.str());
                return;
            }
        }

        // Verify STATUS register is still readable
        uint32_t status_readable = read_status();
        bool fatal_still_set = (status_readable & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_still_set) {
            report_test_fail("test_fatal_alert_terminal_error_state",
                           "STATUS.ALERT_FATAL_FAULT cleared unexpectedly");
            return;
        }

        // Verify AES remains in ERROR state (not IDLE)
        bool is_idle = (status_readable & (1 << STATUS_IDLE_BIT)) != 0;

        if (is_idle) {
            report_test_fail("test_fatal_alert_terminal_error_state",
                           "AES incorrectly transitioned to IDLE from ERROR state");
            return;
        }

        report_test_pass("test_fatal_alert_terminal_error_state");

    } catch (const std::exception& e) {
        report_test_fail("test_fatal_alert_terminal_error_state", e.what());
    }
}

// =============================================================================
// Test Case 4: Fatal Alert Recovery Requires Reset
// =============================================================================

void testbench::test_fatal_alert_recovery_requires_reset()
{
    report_test_start("test_fatal_alert_recovery_requires_reset");

    try {
        // Verify AES is in ERROR state from previous tests
        if (!alert_fatal_signal.read()) {
            // Trigger fatal alert if not already in ERROR state
            m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
            wait(20, SC_NS);
        }

        // Verify fatal alert is active
        uint32_t status_before_reset = read_status();
        bool fatal_before = (status_before_reset & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_before) {
            report_test_fail("test_fatal_alert_recovery_requires_reset",
                           "AES not in ERROR state - cannot test recovery");
            return;
        }

        // Assert reset signal (active-low)
        rst_signal.write(false);
        wait(50, SC_NS);

        // De-assert reset
        rst_signal.write(true);
        wait(100, SC_NS);  // Allow time for reset processing and pseudo-random clearing

        // Wait for AES to become idle after reset
        wait_for_idle(2000);

        // Verify fatal alert signal is de-asserted after reset
        if (alert_fatal_signal.read()) {
            report_test_fail("test_fatal_alert_recovery_requires_reset",
                           "alert_fatal_fault still asserted after reset");
            return;
        }

        // Verify STATUS.ALERT_FATAL_FAULT is cleared
        uint32_t status_after_reset = read_status();
        bool fatal_after = (status_after_reset & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (fatal_after) {
            report_test_fail("test_fatal_alert_recovery_requires_reset",
                           "STATUS.ALERT_FATAL_FAULT not cleared after reset");
            return;
        }

        // Verify AES is IDLE and operational
        bool is_idle = (status_after_reset & (1 << STATUS_IDLE_BIT)) != 0;

        if (!is_idle) {
            report_test_fail("test_fatal_alert_recovery_requires_reset",
                           "AES not IDLE after reset");
            return;
        }

        // Verify idle output port is high
        if (!idle_signal.read()) {
            report_test_fail("test_fatal_alert_recovery_requires_reset",
                           "idle_o not asserted after reset");
            return;
        }

        // Verify AES is operational: attempt a simple configuration write
        uint32_t test_ctrl_value = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_ctrl_value);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_ctrl_value);
        wait(10, SC_NS);

        uint32_t ctrl_readback;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_readback);
        wait(10, SC_NS);

        if (ctrl_readback != test_ctrl_value) {
            std::stringstream ss;
            ss << "AES not operational after reset: CTRL write failed (expected 0x"
               << std::hex << test_ctrl_value << ", got 0x" << ctrl_readback << ")";
            report_test_fail("test_fatal_alert_recovery_requires_reset", ss.str());
            return;
        }

        report_test_pass("test_fatal_alert_recovery_requires_reset");

    } catch (const std::exception& e) {
        report_test_fail("test_fatal_alert_recovery_requires_reset", e.what());
    }
}

// =============================================================================
// Test Case 5: Life Cycle Escalation Triggers Fatal Alert
// =============================================================================

void testbench::test_life_cycle_escalation_fatal_alert()
{
    report_test_start("test_life_cycle_escalation_fatal_alert");

    try {
        // Ensure AES is operational and idle
        wait_for_idle(1000);

        // Verify no fatal alert before escalation
        uint32_t status_before = read_status();
        bool fatal_before = (status_before & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (fatal_before) {
            // Reset AES to clear previous fatal alert
            rst_signal.write(false);
            wait(50, SC_NS);
            rst_signal.write(true);
            wait(100, SC_NS);
            wait_for_idle(2000);

            status_before = read_status();
            fatal_before = (status_before & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

            if (fatal_before) {
                report_test_fail("test_life_cycle_escalation_fatal_alert",
                               "Cannot clear previous fatal alert");
                return;
            }
        }

        // Assert lc_escalate_en signal
        lc_escalate_signal.write(true);
        wait(20, SC_NS);  // Allow time for escalation processing

        // Verify fatal alert is triggered immediately
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_life_cycle_escalation_fatal_alert",
                           "alert_fatal_fault not asserted after lc_escalate_en");
            return;
        }

        // Verify STATUS.ALERT_FATAL_FAULT is set
        uint32_t status_after = read_status();
        bool fatal_after = (status_after & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_after) {
            report_test_fail("test_life_cycle_escalation_fatal_alert",
                           "STATUS.ALERT_FATAL_FAULT not set after escalation");
            return;
        }

        // Verify AES enters ERROR state (not IDLE)
        bool is_idle = (status_after & (1 << STATUS_IDLE_BIT)) != 0;

        if (is_idle) {
            report_test_fail("test_life_cycle_escalation_fatal_alert",
                           "AES should not be IDLE after escalation");
            return;
        }

        // De-assert lc_escalate_en (ERROR state should persist)
        lc_escalate_signal.write(false);
        wait(20, SC_NS);

        // Verify fatal alert remains active
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_life_cycle_escalation_fatal_alert",
                           "alert_fatal_fault cleared prematurely");
            return;
        }

        report_test_pass("test_life_cycle_escalation_fatal_alert");

    } catch (const std::exception& e) {
        report_test_fail("test_life_cycle_escalation_fatal_alert", e.what());
    }
}

// =============================================================================
// Test Case 6: Life Cycle Escalation - Register Writes Ignored
// =============================================================================

void testbench::test_life_cycle_escalation_register_writes_ignored()
{
    report_test_start("test_life_cycle_escalation_register_writes_ignored");

    try {
        // Verify AES is in ERROR state from previous escalation test
        if (!alert_fatal_signal.read()) {
            // Trigger escalation if not already in ERROR state
            lc_escalate_signal.write(true);
            wait(20, SC_NS);
        }

        uint32_t status = read_status();
        bool in_error = (status & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!in_error) {
            report_test_fail("test_life_cycle_escalation_register_writes_ignored",
                           "AES not in ERROR state - cannot test register lockout");
            return;
        }

        // Read readable registers before attempting writes
        uint32_t ctrl_before, ctrl_aux_before, data_in_before;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_before);
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_before);
        m_test->register_read_32(aes_basetest::DATA_IN_OFFSET, data_in_before);
        wait(10, SC_NS);

        // Attempt various register writes (all should be ignored)

        // Try to write CTRL_SHADOWED
        uint32_t test_value_1 = 0x11111111;
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_value_1);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_value_1);
        wait(10, SC_NS);

        // Try to write DATA_IN_0
        uint32_t test_value_3 = 0x33333333;
        m_test->register_write_32(aes_basetest::DATA_IN_OFFSET, test_value_3);
        wait(10, SC_NS);

        // Try to write CTRL_AUX_SHADOWED
        uint32_t test_value_4 = 0x44444444;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, test_value_4);
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, test_value_4);
        wait(10, SC_NS);

        // Read registers after writes to verify they were ignored
        uint32_t ctrl_after, ctrl_aux_after, data_in_after;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_after);
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_after);
        m_test->register_read_32(aes_basetest::DATA_IN_OFFSET, data_in_after);
        wait(10, SC_NS);

        // Verify CTRL_SHADOWED was not modified
        if (ctrl_after != ctrl_before) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED incorrectly modified in ERROR state: was 0x"
               << std::hex << ctrl_before << ", now 0x" << ctrl_after;
            report_test_fail("test_life_cycle_escalation_register_writes_ignored", ss.str());
            return;
        }

        // Verify CTRL_AUX_SHADOWED was not modified
        if (ctrl_aux_after != ctrl_aux_before) {
            std::stringstream ss;
            ss << "CTRL_AUX_SHADOWED incorrectly modified in ERROR state: was 0x"
               << std::hex << ctrl_aux_before << ", now 0x" << ctrl_aux_after;
            report_test_fail("test_life_cycle_escalation_register_writes_ignored", ss.str());
            return;
        }

        // Verify DATA_IN was not modified
        if (data_in_after != data_in_before) {
            std::stringstream ss;
            ss << "DATA_IN incorrectly modified in ERROR state: was 0x"
               << std::hex << data_in_before << ", now 0x" << data_in_after;
            report_test_fail("test_life_cycle_escalation_register_writes_ignored", ss.str());
            return;
        }

        // Verify AES remains in ERROR state
        uint32_t status_after_writes = read_status();
        bool still_in_error = (status_after_writes & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!still_in_error) {
            report_test_fail("test_life_cycle_escalation_register_writes_ignored",
                           "ERROR state cleared unexpectedly");
            return;
        }

        // Verify fatal alert still asserted
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_life_cycle_escalation_register_writes_ignored",
                           "alert_fatal_fault de-asserted unexpectedly");
            return;
        }

        report_test_pass("test_life_cycle_escalation_register_writes_ignored");

    } catch (const std::exception& e) {
        report_test_fail("test_life_cycle_escalation_register_writes_ignored", e.what());
    }
}

// =============================================================================
// Test Case 7: Register Clearing on Fatal Alert
// =============================================================================

void testbench::test_register_clearing_on_fatal_alert()
{
    report_test_start("test_register_clearing_on_fatal_alert");

    try {
        // Reset AES to start with clean state
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        // Configure AES with known key, IV, and data
        uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
        generate_two_share_key(key_share0, key_share1, actual_key, 4);
        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        uint32_t iv[4] = {0x01010101, 0x02020202, 0x03030303, 0x04040404};
        write_iv(iv);
        wait(10, SC_NS);

        uint32_t data_in[4] = {0x10101010, 0x20202020, 0x30303030, 0x40404040};
        write_data_in(data_in);
        wait(10, SC_NS);

        // Read back IV to verify it was written
        uint32_t iv_before[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + (i * 4), iv_before[i]);
            wait(1, SC_NS);
        }

        // Trigger fatal alert
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
        wait(50, SC_NS);  // Allow time for fatal alert processing and register clearing

        // Verify fatal alert is active
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_register_clearing_on_fatal_alert",
                           "Fatal alert not triggered");
            return;
        }

        // Read IV registers after fatal alert
        uint32_t iv_after[4];
        for (int i = 0; i < 4; i++) {
            m_test->register_read_32(aes_basetest::IV_OFFSET + (i * 4), iv_after[i]);
            wait(1, SC_NS);
        }

        // Verify IV registers were cleared (should be different from original values)
        // Note: Registers are cleared with pseudo-random data, not zeros
        bool all_same = true;
        for (int i = 0; i < 4; i++) {
            if (iv_after[i] != iv_before[i]) {
                all_same = false;
                break;
            }
        }

        if (all_same) {
            report_test_fail("test_register_clearing_on_fatal_alert",
                           "IV registers not cleared after fatal alert");
            return;
        }

        // Verify registers were not cleared to zero (pseudo-random clearing)
        bool all_zero = true;
        for (int i = 0; i < 4; i++) {
            if (iv_after[i] != 0) {
                all_zero = false;
                break;
            }
        }

        // Note: It's statistically possible (but unlikely) for pseudo-random data to be zero
        // This check is informational rather than strict
        if (all_zero) {
            CSML_INFO(1, logger) << "Warning: IV registers cleared to zero (expected pseudo-random data)" << std::endl;
        }

        report_test_pass("test_register_clearing_on_fatal_alert");

    } catch (const std::exception& e) {
        report_test_fail("test_register_clearing_on_fatal_alert", e.what());
    }
}

// =============================================================================
// Test Case 8: Status Register Visibility in ERROR State
// =============================================================================

void testbench::test_status_register_readable_in_error_state()
{
    report_test_start("test_status_register_readable_in_error_state");

    try {
        // Ensure AES is in ERROR state
        if (!alert_fatal_signal.read()) {
            // Trigger fatal alert
            m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
            wait(20, SC_NS);
        }

        // Verify fatal alert is active
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_status_register_readable_in_error_state",
                           "AES not in ERROR state - cannot test STATUS readability");
            return;
        }

        // Read STATUS register multiple times
        for (int i = 0; i < 5; i++) {
            uint32_t status = read_status();
            wait(10, SC_NS);

            // Verify ALERT_FATAL_FAULT bit is consistently set
            bool fatal_flag = (status & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

            if (!fatal_flag) {
                std::stringstream ss;
                ss << "STATUS.ALERT_FATAL_FAULT not set on read " << i + 1;
                report_test_fail("test_status_register_readable_in_error_state", ss.str());
                return;
            }

            // Verify IDLE bit is consistently clear (ERROR state)
            bool idle_flag = (status & (1 << STATUS_IDLE_BIT)) != 0;

            if (idle_flag) {
                std::stringstream ss;
                ss << "STATUS.IDLE incorrectly set in ERROR state on read " << i + 1;
                report_test_fail("test_status_register_readable_in_error_state", ss.str());
                return;
            }
        }

        report_test_pass("test_status_register_readable_in_error_state");

    } catch (const std::exception& e) {
        report_test_fail("test_status_register_readable_in_error_state", e.what());
    }
}

// =============================================================================
// Test Case 9: Multiple Fatal Alert Triggers 
// =============================================================================

void testbench::test_multiple_fatal_alert_triggers()
{
    report_test_start("test_multiple_fatal_alert_triggers");

    try {
        // Reset AES to clear ERROR state
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        // Trigger first fatal alert
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
        wait(20, SC_NS);

        // Verify first fatal alert
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_multiple_fatal_alert_triggers",
                           "First fatal alert not triggered");
            return;
        }

        uint32_t status_1 = read_status();
        bool fatal_1 = (status_1 & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_1) {
            report_test_fail("test_multiple_fatal_alert_triggers",
                           "STATUS.ALERT_FATAL_FAULT not set after first alert");
            return;
        }

        // Attempt to trigger another fatal alert (should have no additional effect)
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
        wait(20, SC_NS);

        // Verify fatal alert remains active
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_multiple_fatal_alert_triggers",
                           "Fatal alert de-asserted unexpectedly");
            return;
        }

        uint32_t status_2 = read_status();
        bool fatal_2 = (status_2 & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_2) {
            report_test_fail("test_multiple_fatal_alert_triggers",
                           "STATUS.ALERT_FATAL_FAULT cleared unexpectedly");
            return;
        }

        // Attempt to trigger via lc_escalate_en (should be idempotent)
        lc_escalate_signal.write(true);
        wait(20, SC_NS);

        // Verify fatal alert still active
        if (!alert_fatal_signal.read()) {
            report_test_fail("test_multiple_fatal_alert_triggers",
                           "Fatal alert de-asserted after escalation");
            return;
        }

        lc_escalate_signal.write(false);
        wait(10, SC_NS);

        report_test_pass("test_multiple_fatal_alert_triggers");

    } catch (const std::exception& e) {
        report_test_fail("test_multiple_fatal_alert_triggers", e.what());
    }
}

// =============================================================================
// Test Case 10: Persistent ERROR State Across Multiple Operations
// =============================================================================

void testbench::test_persistent_error_state()
{
    report_test_start("test_persistent_error_state");

    try {
        // Ensure AES is in ERROR state
        if (!alert_fatal_signal.read()) {
            m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
            wait(20, SC_NS);
        }

        // Verify ERROR state
        uint32_t status_initial = read_status();
        bool fatal_initial = (status_initial & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (!fatal_initial) {
            report_test_fail("test_persistent_error_state",
                           "AES not in ERROR state");
            return;
        }

        // Attempt multiple configuration sequences (all should be rejected)
        for (int attempt = 0; attempt < 3; attempt++) {
            // Try full AES configuration
            configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);
            wait(10, SC_NS);

            uint32_t key_share0[8], key_share1[8], actual_key[8] = {0};
            generate_two_share_key(key_share0, key_share1, actual_key, 4);
            write_key_shares(key_share0, key_share1, 4);
            wait(10, SC_NS);

            uint32_t data_in[4] = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};
            write_data_in(data_in);
            wait(10, SC_NS);

            // Verify ERROR state persists
            uint32_t status_check = read_status();
            bool fatal_check = (status_check & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

            if (!fatal_check) {
                std::stringstream ss;
                ss << "ERROR state cleared unexpectedly on attempt " << attempt + 1;
                report_test_fail("test_persistent_error_state", ss.str());
                return;
            }

            // Verify IDLE never becomes true
            bool idle_check = (status_check & (1 << STATUS_IDLE_BIT)) != 0;

            if (idle_check) {
                std::stringstream ss;
                ss << "AES incorrectly reported IDLE in ERROR state on attempt " << attempt + 1;
                report_test_fail("test_persistent_error_state", ss.str());
                return;
            }
        }

        report_test_pass("test_persistent_error_state");

    } catch (const std::exception& e) {
        report_test_fail("test_persistent_error_state", e.what());
    }
}

// =============================================================================
// Test Case 11: Recoverable Alert Does Not Enter ERROR State
// =============================================================================

void testbench::test_recoverable_alert_no_error_state()
{
    report_test_start("test_recoverable_alert_no_error_state");

    try {
        // Reset AES to clear any ERROR state
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        // Trigger recoverable alert via ALERT_TEST
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000001);
        wait(10, SC_NS);

        // Verify recoverable alert is asserted
        if (!alert_recov_signal.read()) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "Recoverable alert not asserted");
            return;
        }

        // Read STATUS register
        uint32_t status = read_status();

        // Verify ALERT_RECOV_CTRL_UPDATE_ERR is set
        bool recov_flag = (status & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!recov_flag) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "STATUS.ALERT_RECOV_CTRL_UPDATE_ERR not set");
            return;
        }

        // Verify ALERT_FATAL_FAULT is NOT set (recoverable alert only)
        bool fatal_flag = (status & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (fatal_flag) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "Fatal alert incorrectly triggered by recoverable alert test");
            return;
        }

        // Verify AES is still IDLE (not in ERROR state)
        bool idle_flag = (status & (1 << STATUS_IDLE_BIT)) != 0;

        if (!idle_flag) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "AES not IDLE after recoverable alert");
            return;
        }

        // Verify idle output port is high
        if (!idle_signal.read()) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "idle_o not asserted after recoverable alert");
            return;
        }

        // Verify AES is still operational: attempt configuration write
        uint32_t test_ctrl = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_ctrl);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, test_ctrl);
        wait(10, SC_NS);

        uint32_t ctrl_readback;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_readback);
        wait(10, SC_NS);

        if (ctrl_readback != test_ctrl) {
            report_test_fail("test_recoverable_alert_no_error_state",
                           "AES not operational after recoverable alert");
            return;
        }

        report_test_pass("test_recoverable_alert_no_error_state");

    } catch (const std::exception& e) {
        report_test_fail("test_recoverable_alert_no_error_state", e.what());
    }
}

// =============================================================================
// Test Case 12: Alert Outputs Match STATUS Register
// =============================================================================

void testbench::test_alert_outputs_match_status_register()
{
    report_test_start("test_alert_outputs_match_status_register");

    try {
        // Reset AES to start clean
        rst_signal.write(false);
        wait(50, SC_NS);
        rst_signal.write(true);
        wait(100, SC_NS);
        wait_for_idle(2000);

        // Verify no alerts initially
        uint32_t status_initial = read_status();
        bool recov_status = (status_initial & (1 << STATUS_ALERT_RECOV_BIT)) != 0;
        bool fatal_status = (status_initial & (1 << STATUS_ALERT_FATAL_BIT)) != 0;

        if (recov_status || alert_recov_signal.read()) {
            report_test_fail("test_alert_outputs_match_status_register",
                           "Recoverable alert active at start");
            return;
        }

        if (fatal_status || alert_fatal_signal.read()) {
            report_test_fail("test_alert_outputs_match_status_register",
                           "Fatal alert active at start");
            return;
        }

        // Trigger recoverable alert
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000001);
        wait(10, SC_NS);

        // Verify recoverable alert matches
        uint32_t status_recov = read_status();
        bool recov_status_set = (status_recov & (1 << STATUS_ALERT_RECOV_BIT)) != 0;
        bool recov_signal_set = alert_recov_signal.read();

        if (recov_status_set != recov_signal_set) {
            std::stringstream ss;
            ss << "Recoverable alert mismatch: STATUS bit=" << recov_status_set
               << ", signal=" << recov_signal_set;
            report_test_fail("test_alert_outputs_match_status_register", ss.str());
            return;
        }

        // Trigger fatal alert
        m_test->register_write_32(aes_basetest::ALERT_TEST_OFFSET, 0x00000002);
        wait(20, SC_NS);

        // Verify fatal alert matches
        uint32_t status_fatal = read_status();
        bool fatal_status_set = (status_fatal & (1 << STATUS_ALERT_FATAL_BIT)) != 0;
        bool fatal_signal_set = alert_fatal_signal.read();

        if (fatal_status_set != fatal_signal_set) {
            std::stringstream ss;
            ss << "Fatal alert mismatch: STATUS bit=" << fatal_status_set
               << ", signal=" << fatal_signal_set;
            report_test_fail("test_alert_outputs_match_status_register", ss.str());
            return;
        }

        report_test_pass("test_alert_outputs_match_status_register");

    } catch (const std::exception& e) {
        report_test_fail("test_alert_outputs_match_status_register", e.what());
    }
}
