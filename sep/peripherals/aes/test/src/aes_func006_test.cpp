#include "testbench.h"
#include "aes_basetest.h"
#include <cstring>

// =============================================================================
// FUNC-AES-006: Shadowed Register Fault Detection Test Implementations
// =============================================================================

// =============================================================================
// Test Case 1: CTRL_SHADOWED Two-Write Protocol - Successful Matching Writes
// =============================================================================

void testbench::test_ctrl_shadowed_two_write_matching()
{
    report_test_start("test_ctrl_shadowed_two_write_matching");

    try {
        // Ensure AES is idle before attempting shadowed write
        wait_for_idle(1000);

        // Clear any existing alert from previous tests with a successful matching write
        uint32_t ctrl_clear = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);

        // Verify no alert is set after clearing
        uint32_t status_initial = read_status();
        bool alert_before = (status_initial & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_before) {
            report_test_fail("test_ctrl_shadowed_two_write_matching",
                           "Alert still set after clearing - cannot proceed with test");
            return;
        }

        // Configure CTRL_SHADOWED: AES-128 ECB Encryption, Automatic Mode
        // OPERATION=0x1 (ENC), MODE=0x01 (ECB), KEY_LEN=0x1 (128), MANUAL_OP=0
        uint32_t ctrl_value = (0x1) | (0x01 << 2) | (0x1 << 8) | (0x0 << 15);

        // First write
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value);
        wait(10, SC_NS);

        // Second write (matching)
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value);
        wait(10, SC_NS);

        // Verify register was successfully updated (read back value)
        uint32_t ctrl_readback;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_readback);
        wait(10, SC_NS);

        if (ctrl_readback != ctrl_value) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED not updated: expected 0x" << std::hex << ctrl_value
               << ", got 0x" << ctrl_readback;
            report_test_fail("test_ctrl_shadowed_two_write_matching", ss.str());
            return;
        }

        // Verify no alert triggered
        uint32_t status_after = read_status();
        bool alert_after = (status_after & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_after) {
            report_test_fail("test_ctrl_shadowed_two_write_matching",
                           "Recoverable alert triggered on matching writes");
            return;
        }

        // Verify alert signal on port remains low
        if (alert_recov_signal.read()) {
            report_test_fail("test_ctrl_shadowed_two_write_matching",
                           "alert_recov_ctrl_update_err asserted on matching writes");
            return;
        }

        report_test_pass("test_ctrl_shadowed_two_write_matching");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_shadowed_two_write_matching", e.what());
    }
}

// =============================================================================
// Test Case 2: CTRL_SHADOWED Two-Write Protocol - Mismatch Detection and Alert
// =============================================================================

void testbench::test_ctrl_shadowed_two_write_mismatch()
{
    report_test_start("test_ctrl_shadowed_two_write_mismatch");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Read initial CTRL_SHADOWED value
        uint32_t ctrl_initial;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_initial);
        wait(10, SC_NS);

        // First write: AES-128 ECB Encryption
        uint32_t ctrl_value1 = (0x1) | (0x01 << 2) | (0x1 << 8) | (0x0 << 15);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value1);
        wait(10, SC_NS);

        // Second write: AES-256 CBC Encryption (different value - mismatch)
        uint32_t ctrl_value2 = (0x1) | (0x02 << 2) | (0x4 << 8) | (0x0 << 15);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2);
        wait(10, SC_NS);

        // Verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR is set
        uint32_t status = read_status();
        bool alert_flag = (status & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert_flag) {
            report_test_fail("test_ctrl_shadowed_two_write_mismatch",
                           "STATUS.ALERT_RECOV_CTRL_UPDATE_ERR not set after mismatch");
            return;
        }

        // Verify alert signal on port is asserted
        wait(5, SC_NS);  // Allow signal propagation
        if (!alert_recov_signal.read()) {
            report_test_fail("test_ctrl_shadowed_two_write_mismatch",
                           "alert_recov_ctrl_update_err not asserted on port");
            return;
        }

        // Verify register was NOT updated (should retain original value)
        uint32_t ctrl_after;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_after);
        wait(10, SC_NS);

        if (ctrl_after != ctrl_initial) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED incorrectly updated on mismatch: was 0x" << std::hex << ctrl_initial
               << ", now 0x" << ctrl_after;
            report_test_fail("test_ctrl_shadowed_two_write_mismatch", ss.str());
            return;
        }

        report_test_pass("test_ctrl_shadowed_two_write_mismatch");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_shadowed_two_write_mismatch", e.what());
    }
}

// =============================================================================
// Test Case 3: CTRL_AUX_SHADOWED Two-Write Protocol - Successful Matching Writes
// =============================================================================

void testbench::test_ctrl_aux_shadowed_two_write_matching()
{
    report_test_start("test_ctrl_aux_shadowed_two_write_matching");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Clear any existing alert from previous tests with a successful matching write
        uint32_t ctrl_clear = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);

        // Verify no alert is set after clearing
        uint32_t status_initial = read_status();
        bool alert_before = (status_initial & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_before) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_matching",
                           "Alert still set after clearing - cannot proceed with test");
            return;
        }

        // Verify CTRL_AUX_REGWEN is unlocked (should be 0x1 by default)
        uint32_t regwen;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen);
        wait(10, SC_NS);

        if ((regwen & 0x1) == 0) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_matching",
                           "CTRL_AUX_REGWEN already locked - cannot test");
            return;
        }

        // Configure CTRL_AUX_SHADOWED: Enable KEY_TOUCH_FORCES_RESEED
        uint32_t ctrl_aux_value = 0x00000001;  // KEY_TOUCH_FORCES_RESEED=1

        // First write
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value);
        wait(10, SC_NS);

        // Second write (matching)
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value);
        wait(10, SC_NS);

        // Verify register was successfully updated
        uint32_t ctrl_aux_readback;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_readback);
        wait(10, SC_NS);

        if (ctrl_aux_readback != ctrl_aux_value) {
            std::stringstream ss;
            ss << "CTRL_AUX_SHADOWED not updated: expected 0x" << std::hex << ctrl_aux_value
               << ", got 0x" << ctrl_aux_readback;
            report_test_fail("test_ctrl_aux_shadowed_two_write_matching", ss.str());
            return;
        }

        // Verify no alert triggered by the matching writes
        uint32_t status_after = read_status();
        bool alert_after = (status_after & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_after) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_matching",
                           "Recoverable alert triggered on matching writes");
            return;
        }

        report_test_pass("test_ctrl_aux_shadowed_two_write_matching");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_shadowed_two_write_matching", e.what());
    }
}

// =============================================================================
// Test Case 4: CTRL_AUX_SHADOWED Two-Write Protocol - Mismatch Detection and Alert
// =============================================================================

void testbench::test_ctrl_aux_shadowed_two_write_mismatch()
{
    report_test_start("test_ctrl_aux_shadowed_two_write_mismatch");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Verify CTRL_AUX_REGWEN is unlocked
        uint32_t regwen;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen);
        wait(10, SC_NS);

        if ((regwen & 0x1) == 0) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_mismatch",
                           "CTRL_AUX_REGWEN already locked - cannot test");
            return;
        }

        // Read initial CTRL_AUX_SHADOWED value
        uint32_t ctrl_aux_initial;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_initial);
        wait(10, SC_NS);

        // First write: KEY_TOUCH_FORCES_RESEED=1
        uint32_t ctrl_aux_value1 = 0x00000001;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value1);
        wait(10, SC_NS);

        // Second write: KEY_TOUCH_FORCES_RESEED=0 (mismatch)
        uint32_t ctrl_aux_value2 = 0x00000000;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value2);
        wait(10, SC_NS);

        // Verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR is set
        uint32_t status = read_status();
        bool alert_flag = (status & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert_flag) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_mismatch",
                           "STATUS.ALERT_RECOV_CTRL_UPDATE_ERR not set after mismatch");
            return;
        }

        // Verify alert signal on port is asserted
        wait(5, SC_NS);
        if (!alert_recov_signal.read()) {
            report_test_fail("test_ctrl_aux_shadowed_two_write_mismatch",
                           "alert_recov_ctrl_update_err not asserted on port");
            return;
        }

        // Verify register was NOT updated
        uint32_t ctrl_aux_after;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_after);
        wait(10, SC_NS);

        if (ctrl_aux_after != ctrl_aux_initial) {
            std::stringstream ss;
            ss << "CTRL_AUX_SHADOWED incorrectly updated on mismatch: was 0x" << std::hex << ctrl_aux_initial
               << ", now 0x" << ctrl_aux_after;
            report_test_fail("test_ctrl_aux_shadowed_two_write_mismatch", ss.str());
            return;
        }

        report_test_pass("test_ctrl_aux_shadowed_two_write_mismatch");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_shadowed_two_write_mismatch", e.what());
    }
}

// =============================================================================
// Test Case 5: Read Side-Effect - Reading Shadowed Register Resets Write Sequence
// =============================================================================

void testbench::test_shadowed_read_resets_sequence()
{
    report_test_start("test_shadowed_read_resets_sequence");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Clear any existing alert from previous tests
        uint32_t ctrl_clear = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_clear);
        wait(10, SC_NS);

        // Verify no alert is set after clearing
        uint32_t status_initial = read_status();
        bool alert_initial = (status_initial & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_initial) {
            report_test_fail("test_shadowed_read_resets_sequence",
                           "Alert still set after clearing - cannot proceed with test");
            return;
        }

        // First write to CTRL_SHADOWED
        uint32_t ctrl_value1 = (0x1) | (0x01 << 2) | (0x1 << 8);  // AES-128 ECB ENC
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value1);
        wait(10, SC_NS);

        // Read CTRL_SHADOWED (this resets the write sequence)
        uint32_t ctrl_readback;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_readback);
        wait(10, SC_NS);

        // Write a different value (should be treated as NEW first write, not second write)
        uint32_t ctrl_value2 = (0x1) | (0x02 << 2) | (0x4 << 8);  // AES-256 CBC ENC
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2);
        wait(10, SC_NS);

        // Verify no alert triggered (different value after read should not cause mismatch)
        uint32_t status = read_status();
        bool alert_flag = (status & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_flag) {
            report_test_fail("test_shadowed_read_resets_sequence",
                           "Alert triggered after read reset - read should reset write sequence");
            return;
        }

        // Complete the second write sequence with matching value to confirm reset
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2);
        wait(10, SC_NS);

        // Verify register was updated to ctrl_value2
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_readback);
        wait(10, SC_NS);

        if (ctrl_readback != ctrl_value2) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED not updated after read reset: expected 0x" << std::hex << ctrl_value2
               << ", got 0x" << ctrl_readback;
            report_test_fail("test_shadowed_read_resets_sequence", ss.str());
            return;
        }

        report_test_pass("test_shadowed_read_resets_sequence");

    } catch (const std::exception& e) {
        report_test_fail("test_shadowed_read_resets_sequence", e.what());
    }
}

// =============================================================================
// Test Case 6: Alert Clearing After Successful Write Following Mismatch
// =============================================================================

void testbench::test_alert_cleared_by_successful_write()
{
    report_test_start("test_alert_cleared_by_successful_write");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Trigger a mismatch to set alert flag
        uint32_t ctrl_value1 = (0x1) | (0x01 << 2) | (0x1 << 8);
        uint32_t ctrl_value2 = (0x1) | (0x02 << 2) | (0x4 << 8);

        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value1);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2);
        wait(10, SC_NS);

        // Verify alert is set
        uint32_t status_before = read_status();
        bool alert_before = (status_before & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert_before) {
            report_test_fail("test_alert_cleared_by_successful_write",
                           "Alert not set after mismatch - prerequisite failed");
            return;
        }

        // Verify alert signal asserted
        if (!alert_recov_signal.read()) {
            report_test_fail("test_alert_cleared_by_successful_write",
                           "Alert signal not asserted after mismatch");
            return;
        }

        // Perform successful matching write to clear alert
        uint32_t ctrl_value_correct = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value_correct);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value_correct);
        wait(10, SC_NS);

        // Verify alert flag is cleared
        uint32_t status_after = read_status();
        bool alert_after = (status_after & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (alert_after) {
            report_test_fail("test_alert_cleared_by_successful_write",
                           "Alert flag not cleared after successful write");
            return;
        }

        // Verify alert signal deasserted
        wait(5, SC_NS);
        if (alert_recov_signal.read()) {
            report_test_fail("test_alert_cleared_by_successful_write",
                           "Alert signal not deasserted after successful write");
            return;
        }

        report_test_pass("test_alert_cleared_by_successful_write");

    } catch (const std::exception& e) {
        report_test_fail("test_alert_cleared_by_successful_write", e.what());
    }
}

// =============================================================================
// Test Case 7: Multiple Consecutive Mismatches
// =============================================================================

void testbench::test_multiple_consecutive_mismatches()
{
    report_test_start("test_multiple_consecutive_mismatches");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Mismatch 1
        uint32_t ctrl_value1a = (0x1) | (0x01 << 2) | (0x1 << 8);
        uint32_t ctrl_value1b = (0x1) | (0x02 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value1a);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value1b);
        wait(10, SC_NS);

        uint32_t status1 = read_status();
        bool alert1 = (status1 & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert1) {
            report_test_fail("test_multiple_consecutive_mismatches",
                           "Alert not set after first mismatch");
            return;
        }

        // Mismatch 2 (without clearing alert)
        uint32_t ctrl_value2a = (0x1) | (0x01 << 2) | (0x4 << 8);
        uint32_t ctrl_value2b = (0x1) | (0x02 << 2) | (0x4 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2a);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value2b);
        wait(10, SC_NS);

        uint32_t status2 = read_status();
        bool alert2 = (status2 & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert2) {
            report_test_fail("test_multiple_consecutive_mismatches",
                           "Alert not maintained after second mismatch");
            return;
        }

        // Mismatch 3
        uint32_t ctrl_value3a = (0x2) | (0x01 << 2) | (0x1 << 8);
        uint32_t ctrl_value3b = (0x1) | (0x01 << 2) | (0x1 << 8);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value3a);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_value3b);
        wait(10, SC_NS);

        uint32_t status3 = read_status();
        bool alert3 = (status3 & (1 << STATUS_ALERT_RECOV_BIT)) != 0;

        if (!alert3) {
            report_test_fail("test_multiple_consecutive_mismatches",
                           "Alert not maintained after third mismatch");
            return;
        }

        // Verify alert remains set (sticky until cleared by successful write)
        if (!alert_recov_signal.read()) {
            report_test_fail("test_multiple_consecutive_mismatches",
                           "Alert signal not asserted after multiple mismatches");
            return;
        }

        report_test_pass("test_multiple_consecutive_mismatches");

    } catch (const std::exception& e) {
        report_test_fail("test_multiple_consecutive_mismatches", e.what());
    }
}

// =============================================================================
// Test Case 8: Write Protection When Locked (CTRL_AUX_REGWEN for CTRL_AUX_SHADOWED)
// =============================================================================

void testbench::test_ctrl_aux_write_protection_when_locked()
{
    report_test_start("test_ctrl_aux_write_protection_when_locked");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Read initial CTRL_AUX_SHADOWED value
        uint32_t ctrl_aux_initial;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_initial);
        wait(10, SC_NS);

        // Lock CTRL_AUX_SHADOWED by writing 0 to CTRL_AUX_REGWEN
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x00000000);
        wait(10, SC_NS);

        // Verify CTRL_AUX_REGWEN is locked (reads as 0)
        uint32_t regwen_readback;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen_readback);
        wait(10, SC_NS);

        if ((regwen_readback & 0x1) != 0) {
            report_test_fail("test_ctrl_aux_write_protection_when_locked",
                           "CTRL_AUX_REGWEN not locked after write 0");
            return;
        }

        // Attempt to write CTRL_AUX_SHADOWED (should be rejected)
        uint32_t ctrl_aux_new_value = 0x00000001;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_new_value);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_new_value);
        wait(10, SC_NS);

        // Verify CTRL_AUX_SHADOWED was NOT updated
        uint32_t ctrl_aux_after;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_after);
        wait(10, SC_NS);

        if (ctrl_aux_after != ctrl_aux_initial) {
            std::stringstream ss;
            ss << "CTRL_AUX_SHADOWED updated despite lock: was 0x" << std::hex << ctrl_aux_initial
               << ", now 0x" << ctrl_aux_after;
            report_test_fail("test_ctrl_aux_write_protection_when_locked", ss.str());
            return;
        }

        report_test_pass("test_ctrl_aux_write_protection_when_locked");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_write_protection_when_locked", e.what());
    }
}

// =============================================================================
// Test Case 12: Interaction Between Shadowed Protocol and Busy State (CTRL_SHADOWED)
// =============================================================================

void testbench::test_shadowed_write_rejected_when_busy()
{
    report_test_start("test_shadowed_write_rejected_when_busy");

    try {
        // Configure AES for operation
        uint32_t key_share0[8], key_share1[8], actual_key[4] = {
            0x2b7e1516, 0x28aed2a6, 0xabf71588, 0x09cf4f3c
        };
        generate_two_share_key(key_share0, key_share1, actual_key, 4);

        configure_aes(AES_ENC, AES_MODE_ECB, AES_128, false);  // automatic mode
        wait(10, SC_NS);

        write_key_shares(key_share0, key_share1, 4);
        wait(10, SC_NS);

        // Read CTRL_SHADOWED value before starting operation
        uint32_t ctrl_before;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_before);
        wait(10, SC_NS);

        // Start operation by writing DATA_IN (auto-start in automatic mode)
        uint32_t plaintext[4] = {0x32438596, 0x12345678, 0xaabbccdd, 0xeeff0011};
        write_data_in(plaintext);
        wait(5, SC_NS);  // Small delay to ensure operation starts

        // Verify AES is busy (IDLE=0)
        uint32_t status_busy = read_status();
        bool idle_during_op = (status_busy & (1 << STATUS_IDLE_BIT)) != 0;

        if (idle_during_op) {
            report_test_fail("test_shadowed_write_rejected_when_busy",
                           "AES not busy after starting operation");
            return;
        }

        // Attempt to write CTRL_SHADOWED while busy (should be rejected)
        uint32_t ctrl_new_value = (0x2) | (0x02 << 2) | (0x4 << 8);  // Different config
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_new_value);
        wait(10, SC_NS);
        m_test->register_write_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_new_value);
        wait(10, SC_NS);

        // Wait for operation to complete
        wait_for_output_valid(1000);
        uint32_t ciphertext[4];
        read_data_out(ciphertext);
        wait(10, SC_NS);

        // Verify CTRL_SHADOWED was NOT updated (busy write rejected)
        uint32_t ctrl_after;
        m_test->register_read_32(aes_basetest::CTRL_SHADOWED_OFFSET, ctrl_after);
        wait(10, SC_NS);

        if (ctrl_after != ctrl_before) {
            std::stringstream ss;
            ss << "CTRL_SHADOWED incorrectly updated while busy: was 0x" << std::hex << ctrl_before
               << ", now 0x" << ctrl_after;
            report_test_fail("test_shadowed_write_rejected_when_busy", ss.str());
            return;
        }

        report_test_pass("test_shadowed_write_rejected_when_busy");

    } catch (const std::exception& e) {
        report_test_fail("test_shadowed_write_rejected_when_busy", e.what());
    }
}

// =============================================================================
// Test Case 13: CTRL_AUX_REGWEN Cannot Be Unlocked
// =============================================================================

void testbench::test_ctrl_aux_regwen_cannot_unlock()
{
    report_test_start("test_ctrl_aux_regwen_cannot_unlock");

    try {
        // Ensure AES is idle
        wait_for_idle(1000);

        // Lock CTRL_AUX_REGWEN by writing 0
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x00000000);
        wait(10, SC_NS);

        // Verify locked
        uint32_t regwen_after_lock;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen_after_lock);
        wait(10, SC_NS);

        if ((regwen_after_lock & 0x1) != 0) {
            report_test_fail("test_ctrl_aux_regwen_cannot_unlock",
                           "CTRL_AUX_REGWEN not locked after write 0");
            return;
        }

        // Attempt to unlock by writing 1 (should be ignored)
        m_test->register_write_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, 0x00000001);
        wait(10, SC_NS);

        // Verify still locked (write 1 ignored)
        uint32_t regwen_after_unlock_attempt;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen_after_unlock_attempt);
        wait(10, SC_NS);

        if ((regwen_after_unlock_attempt & 0x1) != 0) {
            report_test_fail("test_ctrl_aux_regwen_cannot_unlock",
                           "CTRL_AUX_REGWEN unlocked by write 1 (should remain locked)");
            return;
        }

        report_test_pass("test_ctrl_aux_regwen_cannot_unlock");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_regwen_cannot_unlock", e.what());
    }
}

// =============================================================================
// Test Case 14: CTRL_AUX_SHADOWED Read Resets Write Sequence
// =============================================================================

void testbench::test_ctrl_aux_shadowed_read_resets_sequence()
{
    report_test_start("test_ctrl_aux_shadowed_read_resets_sequence");

    try {
        // Reset to ensure CTRL_AUX_REGWEN is unlocked (it's rw0c, so only reset can unlock)
        m_test->trigger_reset();
        wait(20, SC_NS);

        // Verify CTRL_AUX_REGWEN is now unlocked after reset
        uint32_t regwen_check;
        m_test->register_read_32(aes_basetest::CTRL_AUX_REGWEN_OFFSET, regwen_check);
        wait(10, SC_NS);

        if ((regwen_check & 0x1) == 0) {
            report_test_fail("test_ctrl_aux_shadowed_read_resets_sequence",
                           "CTRL_AUX_REGWEN still locked after reset - cannot proceed");
            return;
        }

        // Ensure AES is idle
        wait_for_idle(1000);

        // First write to CTRL_AUX_SHADOWED
        uint32_t ctrl_aux_value1 = 0x00000001;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value1);
        wait(10, SC_NS);

        // Read CTRL_AUX_SHADOWED (resets sequence)
        uint32_t ctrl_aux_readback;
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_readback);
        wait(10, SC_NS);

        // Write a different value (should be treated as new first write)
        uint32_t ctrl_aux_value2 = 0x00000000;
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value2);
        wait(10, SC_NS);

        // Complete second write sequence with matching value
        m_test->register_write_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_value2);
        wait(10, SC_NS);

        // Verify register was updated to ctrl_aux_value2
        m_test->register_read_32(aes_basetest::CTRL_AUX_SHADOWED_OFFSET, ctrl_aux_readback);
        wait(10, SC_NS);

        if (ctrl_aux_readback != ctrl_aux_value2) {
            std::stringstream ss;
            ss << "CTRL_AUX_SHADOWED not updated after read reset: expected 0x" << std::hex << ctrl_aux_value2
               << ", got 0x" << ctrl_aux_readback;
            report_test_fail("test_ctrl_aux_shadowed_read_resets_sequence", ss.str());
            return;
        }

        report_test_pass("test_ctrl_aux_shadowed_read_resets_sequence");

    } catch (const std::exception& e) {
        report_test_fail("test_ctrl_aux_shadowed_read_resets_sequence", e.what());
    }
}
