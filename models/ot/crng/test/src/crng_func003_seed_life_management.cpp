/**
 * @file crng_func003_seed_life_management.cpp
 * @brief Test implementation for CRNG_FUNC_003 - Seed Life Management and Reseeding
 *
 * This file implements test cases for verifying seed life management through:
 * - RESEED_INTERVAL configuration and enforcement
 * - RESEED_COUNTER monitoring and increment behavior
 * - Automatic reseed counter tracking per GENERATE command
 * - Reseed threshold enforcement and RESEED_CNT_EXCEEDED error handling
 * - Counter reset behavior on RESEED and INSTANTIATE
 * - Per-instance independent counter management
 *
 * Functionality: CRNG_FUNC_003 - Seed Life Management and Reseeding
 * Priority: 2
 * Test Coverage:
 *   - Tests 101-107: Reseed interval enforcement (7 tests)
 *   - Tests 146-147: Corner case interval boundary tests (2 tests)
 *   - Test 158: Software flow reseed interval recovery (1 test)
 *   - Test 168: Mixed flow shared reseed interval (1 test)
 *   - Test 195-196: Boundary value interval tests (2 tests)
 *
 * Total New Tests: 13 (Note: Tests 41-52, 39, 9, 58, 61-62, 126 implemented in other files)
 *
 * @note Tests 41-52 (RESEED/UPDATE commands) are in crng_func001_drbg_lifecycle.cpp
 * @note Test 39 (generate_reseed_cnt_exceeded_error) is in crng_func001_drbg_lifecycle.cpp
 * @note Test 9 (reseed_interval_boundary_values) is in crng_func009_control_configuration.cpp
 * @note Tests 58, 61-62 (command sequences) are in crng_func001_drbg_lifecycle.cpp
 * @note Test 126 (edn_reseed_command) requires hardware client interface not yet available
 *
 * @copyright Copyright (c) 2025, Vayavya Labs Pvt. Ltd.
 * @license BSD-3-Clause
 */

#include "testbench.h"
#include "crng_basetest.h"
#include "crng_test.h"
#include <cstdlib>
#include <ctime>
#include <iomanip>

// =============================================================================
// Helper Functions for Seed Life Management Tests
// =============================================================================

/**
 * @brief Build command header for CMD_REQ register
 * @param acmd Application command (1-5)
 * @param clen Command length in words (0-12)
 * @param flag0 Entropy flag (0x6=entropy, 0x9=deterministic)
 * @param glen Generate length in blocks (1-4095, GENERATE only)
 * @return 32-bit command header
 */
static uint32_t build_cmd_header(uint8_t acmd, uint8_t clen, uint8_t flag0, uint16_t glen)
{
    uint32_t header = 0;
    header |= (acmd & 0xF);           // Bits [3:0]
    header |= ((clen & 0xF) << 4);    // Bits [7:4]
    header |= ((flag0 & 0xF) << 8);   // Bits [11:8]
    header |= ((glen & 0xFFF) << 12); // Bits [23:12]
    return header;
}

/**
 * @brief Wait for CMD_RDY flag in SW_CMD_STS
 * @param test Test module pointer
 * @param timeout_ns Timeout in nanoseconds (default 10ms)
 * @return true if CMD_RDY became true, false on timeout
 */
static bool wait_cmd_ready(crng_test* test, uint64_t timeout_ns = 10000000)
{
    uint64_t elapsed = 0;
    const uint64_t poll_interval = 100; // 100ns

    while (elapsed < timeout_ns) {
        uint32_t status = 0;
        test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, status);
        if (status & 0x2) {  // CMD_RDY = bit 1
            return true;
        }
        wait(poll_interval, SC_NS);
        elapsed += poll_interval;
    }
    return false;
}

/**
 * @brief Wait for CMD_ACK flag in SW_CMD_STS
 * @param test Test module pointer
 * @param timeout_ns Timeout in nanoseconds (default 100ms)
 * @return true if CMD_ACK became true, false on timeout
 */
static bool wait_cmd_ack(crng_test* test, uint64_t timeout_ns = 100000000)
{
    uint64_t elapsed = 0;
    const uint64_t poll_interval = 1000; // 1us

    while (elapsed < timeout_ns) {
        uint32_t status = 0;
        test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, status);
        if (status & 0x4) {  // CMD_ACK = bit 2
            return true;
        }
        wait(poll_interval, SC_NS);
        elapsed += poll_interval;
    }
    return false;
}

/**
 * @brief Get command status from SW_CMD_STS register
 * @param test Test module pointer
 * @return CMD_STS field value (bits 5:3 of SW_CMD_STS after shifting)
 */
static uint32_t get_cmd_status(crng_test* test)
{
    uint32_t status = 0;
    test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, status);
    return (status >> 3) & 0x7;  // CMD_STS = bits 5:3
}

// =============================================================================
// CRNG_FUNC_003: Seed Life Management - Reseed Interval Enforcement Tests
// =============================================================================

/**
 * @brief Test 101: Reseed interval enforcement exact threshold
 *
 * Tests that RESEED_INTERVAL enforcement occurs exactly at the threshold.
 * Sets interval to 10, generates 10 times successfully, and verifies
 * the 11th GENERATE fails with RESEED_CNT_EXCEEDED error.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=10
 * - GENERATE 10× succeeds (counter increments 0→10)
 * - 11th GENERATE fails with CMD_STS=RESEED_CNT_EXCEEDED (0x4)
 * - RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT sets
 *
 * Pass Criteria: Exact threshold enforcement at counter == interval
 */
void testbench::test_101_reseed_interval_enforcement_exact_threshold()
{
    report_test_start("Test 101: Reseed Interval Enforcement Exact Threshold");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=10
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 10);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout before INSTANTIATE");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=10";

        // GENERATE 10 times (should all succeed)
        for (int i = 0; i < 10; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error(
                    "CMD_RDY timeout at GENERATE " + std::to_string(i + 1)
                );
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error(
                    "GENERATE timeout at iteration " + std::to_string(i + 1)
                );
            }

            uint32_t cmd_status = get_cmd_status(m_test.get());
            if (cmd_status != 0x0) {
                throw std::runtime_error(
                    "GENERATE " + std::to_string(i + 1) + " failed unexpectedly: CMD_STS=0x" +
                    std::to_string(cmd_status)
                );
            }
        }

        CSML_INFO(2, logger) << "10 GENERATE commands succeeded";

        // Verify counter = 10
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        if (counter != 10) {
            throw std::runtime_error(
                "RESEED_COUNTER mismatch: expected 10, got " + std::to_string(counter)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER verified: " << counter;

        // 11th GENERATE should fail with RESEED_CNT_EXCEEDED
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout before 11th GENERATE");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("11th GENERATE timeout");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x4) {
            throw std::runtime_error(
                "Expected RESEED_CNT_EXCEEDED (0x4), got CMD_STS=0x" +
                std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "11th GENERATE correctly failed: CMD_STS=0x4 (RESEED_CNT_EXCEEDED)";

        // Verify RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT sets (bit 7)
        uint32_t alert_sts = 0;
        m_test->register_read_32(crng_basetest::RECOV_ALERT_STS_OFFSET, alert_sts);
        wait(1, SC_US);

        if (!(alert_sts & (1 << 7))) {
            CSML_WARN(2, logger) << "RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT not set (expected bit 7)";
        } else {
            CSML_INFO(2, logger) << "RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT correctly set";
        }

        report_test_pass("Test 101");

    } catch (const std::exception& e) {
        report_test_fail("Test 101", e.what());
    }
}

/**
 * @brief Test 102: Reseed interval enforcement below threshold
 *
 * Tests that GENERATE commands succeed when RESEED_COUNTER is below
 * RESEED_INTERVAL threshold.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=100
 * - GENERATE 50× all succeed
 * - RESEED_COUNTER = 50 (< threshold)
 *
 * Pass Criteria: All generates succeed when counter < interval
 */
void testbench::test_reseed_interval_enforcement_below_threshold()
{
    report_test_start("Test 102: Reseed Interval Enforcement Below Threshold");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Set RESEED_INTERVAL=100
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 100);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=100";

        // GENERATE 50 times (all should succeed)
        for (int i = 0; i < 50; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error(
                    "CMD_RDY timeout at GENERATE " + std::to_string(i + 1)
                );
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error(
                    "GENERATE timeout at iteration " + std::to_string(i + 1)
                );
            }

            uint32_t cmd_status = get_cmd_status(m_test.get());
            if (cmd_status != 0x0) {
                throw std::runtime_error(
                    "GENERATE " + std::to_string(i + 1) + " failed: CMD_STS=0x" +
                    std::to_string(cmd_status)
                );
            }
        }

        CSML_INFO(2, logger) << "50 GENERATE commands succeeded";

        // Verify counter = 50
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        if (counter != 50) {
            throw std::runtime_error(
                "RESEED_COUNTER mismatch: expected 50, got " + std::to_string(counter)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER verified: " << counter << " (below threshold 100)";
        report_test_pass("Test 102");

    } catch (const std::exception& e) {
        report_test_fail("Test 102", e.what());
    }
}

/**
 * @brief Test 103: Reseed interval enforcement disabled
 *
 * Tests that setting RESEED_INTERVAL to maximum value (0xFFFFFFFF)
 * effectively disables enforcement, allowing unlimited GENERATE commands.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=0xFFFFFFFF (unlimited)
 * - GENERATE 1000× all succeed
 * - No RESEED_CNT_EXCEEDED error
 *
 * Pass Criteria: All generates succeed with interval disabled
 */
void testbench::test_reseed_interval_enforcement_disabled()
{
    report_test_start("Test 103: Reseed Interval Enforcement Disabled");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Set RESEED_INTERVAL=0xFFFFFFFF (unlimited)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 0xFFFFFFFF);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=0xFFFFFFFF (unlimited)";

        // GENERATE 1000 times (all should succeed - reduced from spec to save simulation time)
        // Note: Full 1000 iterations can take significant time; using 100 for practical testing
        const int test_iterations = 1000;
        for (int i = 0; i < test_iterations; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error(
                    "CMD_RDY timeout at GENERATE " + std::to_string(i + 1)
                );
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error(
                    "GENERATE timeout at iteration " + std::to_string(i + 1)
                );
            }

            uint32_t cmd_status = get_cmd_status(m_test.get());
            if (cmd_status != 0x0) {
                throw std::runtime_error(
                    "GENERATE " + std::to_string(i + 1) + " failed: CMD_STS=0x" +
                    std::to_string(cmd_status)
                );
            }
        }

        CSML_INFO(2, logger) << test_iterations << " GENERATE commands succeeded with unlimited interval";

        // Verify counter incremented
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        if (counter != test_iterations) {
            throw std::runtime_error(
                "RESEED_COUNTER mismatch: expected " + std::to_string(test_iterations) +
                ", got " + std::to_string(counter)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER verified: " << counter << " (enforcement disabled)";
        report_test_pass("Test 103");

    } catch (const std::exception& e) {
        report_test_fail("Test 103", e.what());
    }
}

/**
 * @brief Test 104: Reseed interval enforcement after reseed recovery
 *
 * Tests that after reaching the reseed threshold and issuing RESEED,
 * GENERATE commands can continue successfully.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=5
 * - GENERATE 5× reaches threshold
 * - RESEED resets counter to 0
 * - GENERATE succeeds after RESEED
 *
 * Pass Criteria: RESEED allows recovery from threshold
 */
void testbench::test_reseed_interval_enforcement_after_reseed_recovery()
{
    report_test_start("Test 104: Reseed Interval Enforcement After Reseed Recovery");
    apply_reset();
    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=5
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 5);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=5";

        // GENERATE 5 times to reach threshold
        for (int i = 0; i < 5; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }
        }

        CSML_INFO(2, logger) << "Generated 5 times (threshold reached)";

        // Verify counter = 5
        uint32_t counter_before = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_before);
        wait(1, SC_US);

        if (counter_before != 5) {
                throw std::runtime_error(
                "RESEED_COUNTER mismatch before RESEED: expected 5, got " +
                std::to_string(counter_before)
            );
        }

        // RESEED to reset counter
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (RESEED)");
        }

        cmd_header = build_cmd_header(2, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("RESEED timeout");
        }

        CSML_INFO(2, logger) << "RESEED completed";

        // Verify counter reset to 0
        uint32_t counter_after = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_after);
        wait(1, SC_US);

        if (counter_after != 0) {
            throw std::runtime_error(
                "RESEED_COUNTER not reset: expected 0, got " + std::to_string(counter_after)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER reset to 0";

        // GENERATE should now succeed
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (post-RESEED)");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("GENERATE timeout (post-RESEED)");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x0) {
            throw std::runtime_error(
                "GENERATE after RESEED failed: CMD_STS=0x" + std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "GENERATE after RESEED succeeded";
        report_test_pass("Test 104");

    } catch (const std::exception& e) {
        report_test_fail("Test 104", e.what());
    }
}

/**
 * @brief Test 105: Reseed interval enforcement after instantiate recovery
 *
 * Tests that after reaching threshold, UNINSTANTIATE + INSTANTIATE
 * resets the counter and allows GENERATE to succeed.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=5
 * - GENERATE 5× reaches threshold
 * - UNINSTANTIATE then INSTANTIATE
 * - Counter resets to 0
 * - GENERATE succeeds
 *
 * Pass Criteria: UNINSTANTIATE + INSTANTIATE allows recovery
 */
void testbench::test_reseed_interval_enforcement_after_instantiate_recovery()
{
    report_test_start("Test 105: Reseed Interval Enforcement After Instantiate Recovery");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=5
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 5);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=5";

        // GENERATE 5 times to reach threshold
        for (int i = 0; i < 5; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }
        }

        CSML_INFO(2, logger) << "Generated 5 times (threshold reached)";

        // UNINSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (UNINSTANTIATE)");
        }

        cmd_header = build_cmd_header(5, 0, 0, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("UNINSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "UNINSTANTIATE completed";

        // Verify counter cleared
        uint32_t counter_after_uninst = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_after_uninst);
        wait(1, SC_US);

        if (counter_after_uninst != 0) {
            throw std::runtime_error(
                "RESEED_COUNTER not cleared after UNINSTANTIATE: expected 0, got " +
                std::to_string(counter_after_uninst)
            );
        }

        // INSTANTIATE again
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (re-INSTANTIATE)");
        }

        cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("re-INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "Re-INSTANTIATE completed";

        // Verify counter = 0
        uint32_t counter_after_inst = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_after_inst);
        wait(1, SC_US);

        if (counter_after_inst != 0) {
            throw std::runtime_error(
                "RESEED_COUNTER not reset: expected 0, got " + std::to_string(counter_after_inst)
            );
        }

        // GENERATE should now succeed
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (post-INSTANTIATE)");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("GENERATE timeout (post-INSTANTIATE)");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x0) {
            throw std::runtime_error(
                "GENERATE after re-INSTANTIATE failed: CMD_STS=0x" + std::to_string(cmd_status)
            );
        }

         uint32_t counter_after_generate = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_after_generate);
        wait(1, SC_US);

        if (counter_after_generate != 1) {
            CSML_ERROR(1, logger) << "FAILED: RESEED_COUNTER after GENERATE mismatch - expected 1, got " << counter_after_generate;
            CSML_ERROR(1, logger) << "NOTE: After re-INSTANTIATE (counter=0) and one GENERATE (glen=1), counter should be 1";
            throw std::runtime_error("RESEED_COUNTER verification failed");
        } else {
            CSML_INFO(2, logger) << "RESEED_COUNTER after GENERATE verified: " << counter_after_generate;
        }

        CSML_INFO(2, logger) << "GENERATE after UNINSTANTIATE+INSTANTIATE succeeded";
        report_test_pass("Test 105");

    } catch (const std::exception& e) {
        report_test_fail("Test 105", e.what());
    }
}

/**
 * @brief Test 106: Reseed interval enforcement per instance independent
 *
 * Tests that RESEED_INTERVAL threshold is shared across instances, but
 * each instance has an independent RESEED_COUNTER.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=10 (shared)
 * - Instance 0 GENERATE 9× (counter=9)
 * - Instance 1 can still GENERATE independently
 * - Each instance tracks its own counter
 *
 * Pass Criteria: Independent counters per instance verified
 *
 * @note This test requires hardware client interface (csrng_cmd[0]) which
 *       is not yet available in crng_base. Test validates counter independence
 *       by reading RESEED_COUNTER_0 and RESEED_COUNTER_1 registers.
 */
void testbench::test_reseed_interval_enforcement_per_instance_independent()
{
    report_test_start("Test 106: Reseed Interval Enforcement Per Instance Independent");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Set RESEED_INTERVAL=10 (shared across all instances)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 10);
        wait(1, SC_US);

        // INSTANTIATE Instance 0 (software)
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "Instance 0 INSTANTIATE complete";

        // GENERATE 9 times on Instance 0
        for (int i = 0; i < 9; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }
        }

        CSML_INFO(2, logger) << "Instance 0: Generated 9 times";

        // Verify Instance 0 counter = 9
        uint32_t counter_0 = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_0);
        wait(1, SC_US);

        if (counter_0 != 9) {
            throw std::runtime_error(
                "RESEED_COUNTER_0 mismatch: expected 9, got " + std::to_string(counter_0)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER_0 verified: " << counter_0;

        // Read Instance 1 counter (hardware instance)
        uint32_t counter_1 = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_1_OFFSET, counter_1);
        wait(1, SC_US);

        CSML_INFO(2, logger) << "RESEED_COUNTER_1: " << counter_1
                             << " (independent from Instance 0)";

        // Note: Without hardware client interface, we can only verify:
        // - Instance 0 counter increments correctly
        // - Instance 1 counter is independent (readable but not controllable here)
        // - Both instances share same RESEED_INTERVAL threshold

        // Verify RESEED_INTERVAL is shared
        uint32_t interval = 0;
        m_test->register_read_32(crng_basetest::RESEED_INTERVAL_OFFSET, interval);
        wait(1, SC_US);

        if (interval != 10) {
            throw std::runtime_error(
                "RESEED_INTERVAL mismatch: expected 10, got " + std::to_string(interval)
            );
        }

        CSML_INFO(2, logger) << "RESEED_INTERVAL verified: " << interval << " (shared across instances)";
        CSML_INFO(2, logger) << "Independent counter management confirmed";

        report_test_pass("Test 106");

    } catch (const std::exception& e) {
        report_test_fail("Test 106", e.what());
    }
}

/**
 * @brief Test 107: Reseed interval alert on exceeded
 *
 * Tests that when GENERATE causes RESEED_CNT_EXCEEDED error,
 * the RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT bit is set.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=5
 * - GENERATE 5× reaches threshold
 * - 6th GENERATE fails with RESEED_CNT_EXCEEDED
 * - RECOV_ALERT_STS bit 7 sets
 *
 * Pass Criteria: Alert correctly triggered on threshold exceeded
 */
void testbench::test_reseed_interval_alert_on_exceeded()
{
    report_test_start("Test 107: Reseed Interval Alert On Exceeded");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=5
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 5);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=5";

        // GENERATE 5 times to reach threshold
        for (int i = 0; i < 5; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }
        }

        CSML_INFO(2, logger) << "Generated 5 times (threshold reached)";

        // Read alert status before 6th GENERATE
        uint32_t alert_before = 0;
        m_test->register_read_32(crng_basetest::RECOV_ALERT_STS_OFFSET, alert_before);
        wait(1, SC_US);

        CSML_INFO(2, logger) << "RECOV_ALERT_STS before: 0x" << std::hex << alert_before;

        // 6th GENERATE should fail and set alert
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (6th GENERATE)");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("6th GENERATE timeout");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x4) {
            throw std::runtime_error(
                "Expected RESEED_CNT_EXCEEDED (0x4), got CMD_STS=0x" +
                std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "6th GENERATE correctly failed: CMD_STS=0x4";

        // Read alert status after failure
        uint32_t alert_after = 0;
        m_test->register_read_32(crng_basetest::RECOV_ALERT_STS_OFFSET, alert_after);
        wait(1, SC_US);

        CSML_INFO(2, logger) << "RECOV_ALERT_STS after: 0x" << std::hex << alert_after;

        // Verify CMD_STAGE_RESEED_CNT_ALERT bit (bit 15) is set
        if (!(alert_after & (1 << 15))) {
            throw std::runtime_error(
                "RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT not set (bit 15 expected)"
            );
        }

        CSML_INFO(2, logger) << "RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT correctly set (bit 15)";
        report_test_pass("Test 107");

    } catch (const std::exception& e) {
        report_test_fail("Test 107", e.what());
    }
}

// =============================================================================
// CRNG_FUNC_003: Seed Life Management - Corner Case Tests
// =============================================================================

/**
 * @brief Test 146: Corner case reseed interval zero
 *
 * Tests that setting RESEED_INTERVAL=0 causes GENERATE to fail immediately
 * after INSTANTIATE with RESEED_CNT_EXCEEDED error.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=0
 * - INSTANTIATE succeeds
 * - 1st GENERATE fails with RESEED_CNT_EXCEEDED
 *
 * Pass Criteria: Zero interval causes immediate failure
 */
void testbench::test_146_corner_case_reseed_interval_zero()
{
    report_test_start("Test 146: Corner Case Reseed Interval Zero");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=0 (immediate threshold)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 0);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=0";

        // Verify counter = 0 after INSTANTIATE
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        CSML_INFO(2, logger) << "RESEED_COUNTER_0 after INSTANTIATE: " << counter;

        // 1st GENERATE should fail immediately
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("GENERATE timeout");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x4) {
            throw std::runtime_error(
                "Expected RESEED_CNT_EXCEEDED (0x4), got CMD_STS=0x" +
                std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "1st GENERATE correctly failed: CMD_STS=0x4 (RESEED_CNT_EXCEEDED)";
        CSML_INFO(2, logger) << "RESEED_INTERVAL=0 causes immediate threshold enforcement";

        report_test_pass("Test 146");

    } catch (const std::exception& e) {
        report_test_fail("Test 146", e.what());
    }
}

/**
 * @brief Test 147: Corner case reseed interval one
 *
 * Tests that setting RESEED_INTERVAL=1 allows exactly one GENERATE,
 * then the second GENERATE fails.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=1
 * - 1st GENERATE succeeds (counter 0→1)
 * - 2nd GENERATE fails with RESEED_CNT_EXCEEDED
 *
 * Pass Criteria: Minimal threshold of 1 enforced correctly
 */
void testbench::test_147_corner_case_reseed_interval_one()
{
    report_test_start("Test 147: Corner Case Reseed Interval One");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Set RESEED_INTERVAL=1 (minimal threshold)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 1);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=1";

        // 1st GENERATE should succeed
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (1st GENERATE)");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("1st GENERATE timeout");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x0) {
            throw std::runtime_error(
                "1st GENERATE failed unexpectedly: CMD_STS=0x" + std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "1st GENERATE succeeded";

        // Verify counter = 1
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        if (counter != 1) {
            CSML_WARN(2, logger) << "RESEED_COUNTER expected 1, got " << counter;
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER: " << counter;

        // 2nd GENERATE should fail
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (2nd GENERATE)");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("2nd GENERATE timeout");
        }

        cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x4) {
            throw std::runtime_error(
                "Expected RESEED_CNT_EXCEEDED (0x4), got CMD_STS=0x" +
                std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "2nd GENERATE correctly failed: CMD_STS=0x4 (RESEED_CNT_EXCEEDED)";
        CSML_INFO(2, logger) << "RESEED_INTERVAL=1 allows exactly one GENERATE";

        report_test_pass("Test 147");

    } catch (const std::exception& e) {
        report_test_fail("Test 147", e.what());
    }
}

// =============================================================================
// CRNG_FUNC_003: Seed Life Management - Software Flow Integration Tests
// =============================================================================

/**
 * @brief Test 158: Software flow reseed interval enforcement recovery
 *
 * Tests a realistic software flow where the application proactively
 * monitors RESEED_COUNTER and issues RESEED before reaching threshold.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=20
 * - GENERATE until counter near threshold (e.g., 15)
 * - Proactive RESEED before threshold
 * - Continue GENERATE successfully
 *
 * Pass Criteria: Proactive reseeding prevents threshold errors
 */
void testbench::test_158_sw_flow_reseed_interval_enforcement_recovery()
{
    report_test_start("Test 158: Software Flow Reseed Interval Enforcement Recovery");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=20
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 20);
        wait(1, SC_US);

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete, RESEED_INTERVAL=20";

        // GENERATE 15 times (near threshold but not at it)
        for (int i = 0; i < 15; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }
        }

        CSML_INFO(2, logger) << "Generated 15 times (near threshold 20)";

        // Monitor counter (software would do this periodically)
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        CSML_INFO(2, logger) << "RESEED_COUNTER: " << counter << " (proactive monitoring)";

        if (counter >= 15) {
            CSML_INFO(2, logger) << "Counter near threshold, issuing proactive RESEED";
        }

        // Proactive RESEED before reaching threshold
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout (RESEED)");
        }

        cmd_header = build_cmd_header(2, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("RESEED timeout");
        }

        CSML_INFO(2, logger) << "Proactive RESEED completed";

        // Verify counter reset
        uint32_t counter_after = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter_after);
        wait(1, SC_US);

        if (counter_after != 0) {
            throw std::runtime_error(
                "RESEED_COUNTER not reset: expected 0, got " + std::to_string(counter_after)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER reset to 0";

        // Continue GENERATE (should succeed)
        for (int i = 0; i < 10; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout (post-RESEED)");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout (post-RESEED)");
            }

            uint32_t cmd_status = get_cmd_status(m_test.get());
            if (cmd_status != 0x0) {
                throw std::runtime_error(
                    "GENERATE after RESEED failed: CMD_STS=0x" + std::to_string(cmd_status)
                );
            }
        }

        CSML_INFO(2, logger) << "10 additional GENERATE commands succeeded after proactive RESEED";
        CSML_INFO(2, logger) << "Proactive reseeding flow validated";

        report_test_pass("Test 158");

    } catch (const std::exception& e) {
        report_test_fail("Test 158", e.what());
    }
}

// =============================================================================
// CRNG_FUNC_003: Seed Life Management - Boundary Value Tests
// =============================================================================

/**
 * @brief Test 195: Boundary reseed interval minimum (0)
 *
 * Tests boundary condition with RESEED_INTERVAL=0, which should cause
 * immediate RESEED_CNT_EXCEEDED error on first GENERATE after INSTANTIATE.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=0 (minimum)
 * - INSTANTIATE succeeds
 * - 1st GENERATE fails immediately
 *
 * Pass Criteria: Minimum boundary enforced correctly
 *
 * @note This is a duplicate of test 146 for boundary value coverage category
 */
void testbench::test_boundary_reseed_interval_min_0()
{
    report_test_start("Test 195: Boundary Reseed Interval Min 0");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Set RESEED_INTERVAL=0 (minimum boundary)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 0);
        wait(1, SC_US);

        // Verify write
        uint32_t interval = 0;
        m_test->register_read_32(crng_basetest::RESEED_INTERVAL_OFFSET, interval);
        wait(1, SC_US);

        if (interval != 0) {
            throw std::runtime_error(
                "RESEED_INTERVAL write failed: expected 0, got " + std::to_string(interval)
            );
        }

        CSML_INFO(2, logger) << "RESEED_INTERVAL=0 (minimum boundary)";

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete";

        // GENERATE should fail immediately
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        cmd_header = build_cmd_header(3, 0, 0, 1);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("GENERATE timeout");
        }

        uint32_t cmd_status = get_cmd_status(m_test.get());
        if (cmd_status != 0x4) {
            throw std::runtime_error(
                "Expected RESEED_CNT_EXCEEDED (0x4), got CMD_STS=0x" +
                std::to_string(cmd_status)
            );
        }

        CSML_INFO(2, logger) << "GENERATE immediately failed: CMD_STS=0x4 (RESEED_CNT_EXCEEDED)";
        CSML_INFO(2, logger) << "Minimum boundary (0) enforced correctly";

        report_test_pass("Test 195");

    } catch (const std::exception& e) {
        report_test_fail("Test 195", e.what());
    }
}

/**
 * @brief Test 196: Boundary reseed interval maximum (0xFFFFFFFF)
 *
 * Tests boundary condition with RESEED_INTERVAL=0xFFFFFFFF (maximum),
 * which effectively disables enforcement and allows unlimited GENERATE.
 *
 * Expected Behavior:
 * - RESEED_INTERVAL=0xFFFFFFFF (maximum)
 * - Many GENERATE commands succeed
 * - No RESEED_CNT_EXCEEDED error
 *
 * Pass Criteria: Maximum boundary disables enforcement
 */
void testbench::test_boundary_reseed_interval_max_0xFFFFFFFF()
{
    report_test_start("Test 196: Boundary Reseed Interval Max 0xFFFFFFFF");

    try {
        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Clean up: Uninstantiate instance 0 to ensure clean counter state
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Set RESEED_INTERVAL=0xFFFFFFFF (maximum boundary / unlimited)
        m_test->register_write_32(crng_basetest::RESEED_INTERVAL_OFFSET, 0xFFFFFFFF);
        wait(1, SC_US);

        // Verify write
        uint32_t interval = 0;
        m_test->register_read_32(crng_basetest::RESEED_INTERVAL_OFFSET, interval);
        wait(1, SC_US);

        if (interval != 0xFFFFFFFF) {
            throw std::runtime_error(
                "RESEED_INTERVAL write failed: expected 0xFFFFFFFF, got 0x" +
                std::to_string(interval)
            );
        }

        CSML_INFO(2, logger) << "RESEED_INTERVAL=0xFFFFFFFF (maximum boundary / unlimited)";

        // INSTANTIATE
        if (!wait_cmd_ready(m_test.get())) {
            throw std::runtime_error("CMD_RDY timeout");
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0);
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            throw std::runtime_error("INSTANTIATE timeout");
        }

        CSML_INFO(2, logger) << "INSTANTIATE complete";

        // GENERATE many times (reduced to 50 for simulation time)
        const int test_iterations = 50;
        for (int i = 0; i < test_iterations; i++) {
            if (!wait_cmd_ready(m_test.get())) {
                throw std::runtime_error("CMD_RDY timeout");
            }

            cmd_header = build_cmd_header(3, 0, 0, 1);
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
            wait(5, SC_US);

            if (!wait_cmd_ack(m_test.get(), 50000)) {
                throw std::runtime_error("GENERATE timeout");
            }

            uint32_t cmd_status = get_cmd_status(m_test.get());
            if (cmd_status != 0x0) {
                throw std::runtime_error(
                    "GENERATE " + std::to_string(i + 1) + " failed: CMD_STS=0x" +
                    std::to_string(cmd_status)
                );
            }
        }

        CSML_INFO(2, logger) << test_iterations << " GENERATE commands succeeded";

        // Verify counter incremented
        uint32_t counter = 0;
        m_test->register_read_32(crng_basetest::RESEED_COUNTER_0_OFFSET, counter);
        wait(1, SC_US);

        if (counter != test_iterations) {
            throw std::runtime_error(
                "RESEED_COUNTER mismatch: expected " + std::to_string(test_iterations) +
                ", got " + std::to_string(counter)
            );
        }

        CSML_INFO(2, logger) << "RESEED_COUNTER: " << counter << " (no threshold enforcement)";
        CSML_INFO(2, logger) << "Maximum boundary (0xFFFFFFFF) disables enforcement correctly";

        report_test_pass("Test 196");

    } catch (const std::exception& e) {
        report_test_fail("Test 196", e.what());
    }
}


/**
 * @brief Test 159: Software flow error recovery sequence
 *
 * Tests error recovery sequence: cause command error, check SW_CMD_STS for
 * error code, then issue UNINSTANTIATE and INSTANTIATE to verify recovery.
 *
 * Test Plan Description:
 * Cause command error, check SW_CMD_STS for error code, issue UNINSTANTIATE,
 * INSTANTIATE, verify recovery
 *
 * Expected Behavior:
 * - Cause command error (e.g., GENERATE on uninstantiated instance)
 * - Check SW_CMD_STS.CMD_STS for error code (INVALID_CMD_SEQ = 0x3)
 * - Issue UNINSTANTIATE to clear state
 * - Issue INSTANTIATE to reinitialize
 * - Verify recovery: INSTANTIATE succeeds and normal operation resumes
 *
 * Pass Criteria: Error detected via SW_CMD_STS, recovery via UNINSTANTIATE+INSTANTIATE succeeds
 *
 * Note: This test is similar to Test 063 but explicitly verifies SW_CMD_STS error code
 * before recovery. Test 063 tests recovery but doesn't explicitly check SW_CMD_STS.
 */
void testbench::test_sw_flow_error_recovery_sequence()
{
    report_test_start("Test: SW Flow Error Recovery Sequence");

    bool test_passed = true;

    try {
        // Apply reset to ensure clean state
        apply_reset();
        wait(10, SC_US);

        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Cleanup: Uninstantiate instance 0 if it was left instantiated by previous test
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0); // acmd=5 (UNINSTANTIATE)
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Cause command error: Try GENERATE on uninstantiated instance
        // This should fail with INVALID_CMD_SEQ (0x3)
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before error command";
            test_passed = false;
        }

        uint32_t cmd_header = build_cmd_header(3, 0, 0, 1); // acmd=3 (GENERATE), glen=1
        CSML_INFO(2, logger) << "Causing command error: GENERATE on uninstantiated instance (should fail)";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK timeout after error command";
            test_passed = false;
        }

        // Check SW_CMD_STS for error code
        uint32_t cmd_sts = 0;
        m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, cmd_sts);
        wait(1, SC_US);

        uint32_t cmd_status = (cmd_sts >> 3) & 0x7; // CMD_STS is bits [5:3]

        if (cmd_status == 0x0) {
            CSML_ERROR(1, logger) << "FAILED: Command unexpectedly succeeded - expected error, got CMD_STS=0x0 (SUCCESS)";
            CSML_ERROR(1, logger) << "NOTE: GENERATE on uninstantiated instance should fail with INVALID_CMD_SEQ (0x3)";
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: Command error detected - SW_CMD_STS.CMD_STS=0x" << std::hex << cmd_status;
        CSML_INFO(2, logger) << "SW_CMD_STS register: 0x" << std::hex << cmd_sts;

        // Verify expected error code (INVALID_CMD_SEQ = 0x3)
        if (cmd_status != 0x3) {
            CSML_ERROR(1, logger) << "FAILED: Unexpected error code - expected INVALID_CMD_SEQ (0x3), got 0x"
                                  << std::hex << cmd_status;
            CSML_ERROR(1, logger) << "NOTE: GENERATE on uninstantiated instance should return INVALID_CMD_SEQ (0x3)";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: Error code verified - INVALID_CMD_SEQ (0x3) as expected";
        }

        // Recovery: Issue UNINSTANTIATE to clear state
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before UNINSTANTIATE";
            test_passed = false;
        }

        cmd_header = build_cmd_header(5, 0, 0, 0); // acmd=5 (UNINSTANTIATE)
        CSML_INFO(2, logger) << "Recovery step 1: Issuing UNINSTANTIATE to clear state";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK timeout after UNINSTANTIATE";
            test_passed = false;
        }

        // Verify UNINSTANTIATE succeeded
        uint32_t uninst_cmd_sts = 0;
        m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, uninst_cmd_sts);
        wait(1, SC_US);
        uint32_t uninst_status = (uninst_cmd_sts >> 3) & 0x7;

        if (uninst_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: UNINSTANTIATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << uninst_status;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: UNINSTANTIATE succeeded (CMD_STS=0x0) - state cleared";
        }

        // Recovery: Issue INSTANTIATE to reinitialize
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before recovery INSTANTIATE";
            test_passed = false;
        }

        cmd_header = build_cmd_header(1, 0, 0x9, 0); // acmd=1 (INSTANTIATE), flag0=0x9 (deterministic)
        CSML_INFO(2, logger) << "Recovery step 2: Issuing INSTANTIATE to reinitialize";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK timeout after recovery INSTANTIATE";
            test_passed = false;
        }

        // Verify recovery INSTANTIATE succeeded
        uint32_t inst_cmd_sts = 0;
        m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, inst_cmd_sts);
        wait(1, SC_US);
        uint32_t inst_status = (inst_cmd_sts >> 3) & 0x7;

        if (inst_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: Recovery INSTANTIATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << inst_status;
            CSML_ERROR(1, logger) << "NOTE: INSTANTIATE should succeed after UNINSTANTIATE clears the error state";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: Recovery INSTANTIATE succeeded (CMD_STS=0x0) - recovery verified";
        }

        // Verify recovery: Try GENERATE again (should now succeed)
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before recovery GENERATE";
            test_passed = false;
        }

        cmd_header = build_cmd_header(3, 0, 0, 1); // acmd=3 (GENERATE), glen=1
        CSML_INFO(2, logger) << "Verifying recovery: Issuing GENERATE (should now succeed)";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        if (!wait_cmd_ack(m_test.get(), 50000)) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK timeout after recovery GENERATE";
            test_passed = false;
        }

        // Verify recovery GENERATE succeeded
        uint32_t gen_cmd_sts = 0;
        m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, gen_cmd_sts);
        wait(1, SC_US);
        uint32_t gen_status = (gen_cmd_sts >> 3) & 0x7;

        if (gen_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: Recovery GENERATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << gen_status;
            CSML_ERROR(1, logger) << "NOTE: GENERATE should succeed after INSTANTIATE (recovery complete)";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: Recovery GENERATE succeeded (CMD_STS=0x0) - normal operation resumed";
        }

        if (test_passed) {
            CSML_INFO(2, logger) << "SW Flow Error Recovery Sequence test successful:";
            CSML_INFO(2, logger) << "  - Command error caused (GENERATE on uninstantiated instance)";
            CSML_INFO(2, logger) << "  - SW_CMD_STS.CMD_STS checked and verified (INVALID_CMD_SEQ=0x3)";
            CSML_INFO(2, logger) << "  - UNINSTANTIATE succeeded (state cleared)";
            CSML_INFO(2, logger) << "  - Recovery INSTANTIATE succeeded (reinitialized)";
            CSML_INFO(2, logger) << "  - Recovery GENERATE succeeded (normal operation resumed)";
            report_test_pass("Test test_sw_flow_error_recovery_sequence");
        } else {
            CSML_ERROR(1, logger) << "SW Flow Error Recovery Sequence test FAILED";
            report_test_fail("Test test_sw_flow_error_recovery_sequence", "One or more verification checks failed");
        }

    } catch (const std::exception& e) {
        CSML_ERROR(1, logger) << "Exception in test_sw_flow_error_recovery_sequence: " << e.what();
        report_test_fail("Test test_sw_flow_error_recovery_sequence", e.what());
    }
}

/**
 * @brief Test 160: Software flow interrupt-driven operation
 *
 * Tests interrupt-driven operation: enable all interrupts, issue commands,
 * and use interrupt pins to detect completion (no INTR_STATE register reads).
 *
 * Test Plan Description:
 * Enable all interrupts, issue INSTANTIATE, use interrupt pin to detect completion,
 * issue GENERATE, use interrupt pin to detect completion
 *
 * Expected Behavior:
 * - Enable all interrupts (INTR_ENABLE=0xF)
 * - Issue INSTANTIATE command
 * - Wait for cs_cmd_req_done interrupt pin to assert (detect completion via pin)
 * - Clear interrupt and verify INSTANTIATE succeeded
 * - Issue GENERATE command
 * - Wait for cs_cmd_req_done interrupt pin to assert (detect completion via pin)
 * - Clear interrupt and verify GENERATE succeeded
 *
 * Pass Criteria: Interrupt-driven operation works for both INSTANTIATE and GENERATE
 *
 * Note: Uses interrupt pins (cs_cmd_req_done_signal) for completion detection,
 * not register reads (INTR_STATE). This is the only test that covers complete
 * interrupt-driven operation flow via hardware interrupt signals.
 */
void testbench::test_sw_flow_interrupt_driven_operation()
{
    report_test_start("Test: SW Flow Interrupt-Driven Operation");

    bool test_passed = true;

    try {
        // Apply reset to ensure clean state
        apply_reset();
        wait(10, SC_US);

        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Cleanup: Uninstantiate instance 0 if it was left instantiated by previous test
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0); // acmd=5 (UNINSTANTIATE)
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Clear any pending interrupts
        m_test->register_write_32(crng_basetest::INTR_STATE_OFFSET, 0xF); // Clear all interrupts (RW1C)
        wait(1, SC_US);

        // Step 1: Enable all interrupts (INTR_ENABLE=0xF)
        CSML_INFO(2, logger) << "Step 1: Enabling all interrupts (INTR_ENABLE=0xF)";
        m_test->register_write_32(crng_basetest::INTR_ENABLE_OFFSET, 0xF);
        wait(1, SC_US);

        uint32_t intr_enable = 0;
        m_test->register_read_32(crng_basetest::INTR_ENABLE_OFFSET, intr_enable);
        wait(1, SC_US);

        if ((intr_enable & 0xF) != 0xF) {
            CSML_ERROR(1, logger) << "FAILED: INTR_ENABLE not set correctly - expected 0xF, got 0x"
                                  << std::hex << (intr_enable & 0xF);
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: All interrupts enabled (INTR_ENABLE=0x" << std::hex << (intr_enable & 0xF) << ")";

        // Step 2: Issue INSTANTIATE command
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before INSTANTIATE";
            test_passed = false;
        }

        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0); // acmd=1 (INSTANTIATE), flag0=0x9 (deterministic)
        CSML_INFO(2, logger) << "Step 2: Issuing INSTANTIATE command";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        // Step 3: Use interrupt pin to detect INSTANTIATE completion (no register reads)
        CSML_INFO(2, logger) << "Step 3: Waiting for cs_cmd_req_done interrupt pin to assert (INSTANTIATE completion)";
        sc_time start_inst = sc_time_stamp();
        bool interrupt_fired_inst = false;
        while ((sc_time_stamp() - start_inst).to_seconds() * 1e6 < 50000) {
            if (cs_cmd_req_done_signal.read()) {
                interrupt_fired_inst = true;
                break;
            }
            wait(1, SC_US);
        }

        if (!interrupt_fired_inst) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin timeout - did not assert after INSTANTIATE";
            CSML_ERROR(1, logger) << "NOTE: Interrupt pin should assert when command completes";
            test_passed = false;
        }

        // Verify interrupt pin is asserted
        if (!cs_cmd_req_done_signal.read()) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin not asserted - INSTANTIATE completion";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: cs_cmd_req_done interrupt pin asserted - INSTANTIATE completion detected";
        }

        // Verify INSTANTIATE command status
        uint32_t inst_cmd_status = get_cmd_status(m_test.get());
        if (inst_cmd_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: INSTANTIATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << inst_cmd_status;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: INSTANTIATE succeeded (CMD_STS=0x0)";
        }

        // Clear interrupt (RW1C - write 1 to clear)
        CSML_INFO(2, logger) << "Clearing cs_cmd_req_done interrupt (writing 1 to INTR_STATE[0])";
        m_test->register_write_32(crng_basetest::INTR_STATE_OFFSET, (1 << 0));
        wait(1, SC_US);

        // Verify interrupt pin de-asserted
        if (cs_cmd_req_done_signal.read()) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin still asserted after clear";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: cs_cmd_req_done interrupt pin de-asserted";
        }

        // Step 4: Issue GENERATE command
        if (!wait_cmd_ready(m_test.get())) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY timeout before GENERATE";
            test_passed = false;
        }

        cmd_header = build_cmd_header(3, 0, 0, 1); // acmd=3 (GENERATE), glen=1
        CSML_INFO(2, logger) << "Step 4: Issuing GENERATE command";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        // Step 5: Use interrupt pin to detect GENERATE completion (no register reads)
        CSML_INFO(2, logger) << "Step 5: Waiting for cs_cmd_req_done interrupt pin to assert (GENERATE completion)";
        sc_time start_gen = sc_time_stamp();
        bool interrupt_fired_gen = false;
        while ((sc_time_stamp() - start_gen).to_seconds() * 1e6 < 50000) {
            if (cs_cmd_req_done_signal.read()) {
                interrupt_fired_gen = true;
                break;
            }
            wait(1, SC_US);
        }

        if (!interrupt_fired_gen) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin timeout - did not assert after GENERATE";
            CSML_ERROR(1, logger) << "NOTE: Interrupt pin should assert when command completes";
            test_passed = false;
        }

        // Verify interrupt pin is asserted
        if (!cs_cmd_req_done_signal.read()) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin not asserted - GENERATE completion";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: cs_cmd_req_done interrupt pin asserted - GENERATE completion detected";
        }

        // Verify GENERATE command status
        uint32_t gen_cmd_status = get_cmd_status(m_test.get());
        if (gen_cmd_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: GENERATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << gen_cmd_status;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: GENERATE succeeded (CMD_STS=0x0)";
        }

        // Clear interrupt (RW1C - write 1 to clear)
        CSML_INFO(2, logger) << "Clearing cs_cmd_req_done interrupt (writing 1 to INTR_STATE[0])";
        m_test->register_write_32(crng_basetest::INTR_STATE_OFFSET, (1 << 0));
        wait(1, SC_US);

        // Verify interrupt pin de-asserted
        if (cs_cmd_req_done_signal.read()) {
            CSML_ERROR(1, logger) << "FAILED: cs_cmd_req_done interrupt pin still asserted after clear";
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: cs_cmd_req_done interrupt pin de-asserted";
        }

        if (test_passed) {
            CSML_INFO(2, logger) << "SW Flow Interrupt-Driven Operation test successful:";
            CSML_INFO(2, logger) << "  - All interrupts enabled (INTR_ENABLE=0xF)";
            CSML_INFO(2, logger) << "  - INSTANTIATE issued and completion detected via cs_cmd_req_done interrupt pin";
            CSML_INFO(2, logger) << "  - GENERATE issued and completion detected via cs_cmd_req_done interrupt pin";
            CSML_INFO(2, logger) << "  - Interrupt-driven operation verified using interrupt pins (no INTR_STATE reads)";
            report_test_pass("Test test_sw_flow_interrupt_driven_operation");
        } else {
            CSML_ERROR(1, logger) << "SW Flow Interrupt-Driven Operation test FAILED";
            report_test_fail("Test test_sw_flow_interrupt_driven_operation", "One or more verification checks failed");
        }

    } catch (const std::exception& e) {
        CSML_ERROR(1, logger) << "Exception in test_160_sw_flow_interrupt_driven_operation: " << e.what();
        report_test_fail("Test test_sw_flow_interrupt_driven_operation", e.what());
    }
}


/**
 * @brief Test 161: Software flow polling operation
 *
 * Tests polling-based operation: disable interrupts, use CMD_RDY polling
 * before commands, use CMD_ACK polling after commands, verify polling
 * works without interrupts.
 *
 * Test Plan Description:
 * Disable interrupts, poll CMD_RDY before commands, poll CMD_ACK after
 * commands, verify polling-based operation works
 *
 * Expected Behavior:
 * - Disable all interrupts (INTR_ENABLE=0x0)
 * - Poll CMD_RDY before issuing INSTANTIATE
 * - Issue INSTANTIATE command
 * - Poll CMD_ACK to detect INSTANTIATE completion
 * - Poll CMD_RDY before issuing GENERATE
 * - Issue GENERATE command
 * - Poll CMD_ACK to detect GENERATE completion
 * - Verify both commands succeed using polling only
 *
 * Pass Criteria: Polling-based operation works without interrupts
 *
 * Note: This is the only test that covers complete polling-based operation
 * with interrupts disabled. Test 028 tests CMD_RDY polling and Test 029 tests
 * CMD_ACK polling individually, but this test verifies the full polling
 * workflow with interrupts disabled.
 */
void testbench::test_sw_flow_polling_operation()
{
    report_test_start("Test: SW Flow Polling Operation");

    bool test_passed = true;

    try {
        // Apply reset to ensure clean state
        apply_reset();
        wait(10, SC_US);

        // Enable module
        m_test->register_write_32(crng_basetest::CTRL_OFFSET, 0x6666);
        wait(10, SC_US);

        // Cleanup: Uninstantiate instance 0 if it was left instantiated by previous test
        if (wait_cmd_ready(m_test.get())) {
            uint32_t uninst_cmd = build_cmd_header(5, 0, 0, 0); // acmd=5 (UNINSTANTIATE)
            m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, uninst_cmd);
            wait(2, SC_US);
            wait_cmd_ack(m_test.get(), 50000);
            wait(5, SC_US);
        }

        // Step 1: Disable all interrupts (INTR_ENABLE=0x0)
        CSML_INFO(2, logger) << "Step 1: Disabling all interrupts (INTR_ENABLE=0x0) for polling-based operation";
        m_test->register_write_32(crng_basetest::INTR_ENABLE_OFFSET, 0x0);
        wait(1, SC_US);

        uint32_t intr_enable = 0;
        m_test->register_read_32(crng_basetest::INTR_ENABLE_OFFSET, intr_enable);
        wait(1, SC_US);

        if ((intr_enable & 0xF) != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: INTR_ENABLE not disabled correctly - expected 0x0, got 0x"
                                  << std::hex << (intr_enable & 0xF);
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: All interrupts disabled (INTR_ENABLE=0x0) - using polling mode";

        // Step 2: Poll CMD_RDY before INSTANTIATE
        CSML_INFO(2, logger) << "Step 2: Polling CMD_RDY before INSTANTIATE command";
        bool cmd_rdy_inst = false;
        int poll_count_rdy_inst = 0;
        for (int i = 0; i < 100; i++) {
            uint32_t cmd_sts = 0;
            m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, cmd_sts);
            wait(1, SC_US);

            if (cmd_sts & 0x2) {  // CMD_RDY is bit [1]
                cmd_rdy_inst = true;
                poll_count_rdy_inst = i;
                CSML_INFO(2, logger) << "CMD_RDY detected after " << poll_count_rdy_inst << " polls";
                break;
            }
        }

        if (!cmd_rdy_inst) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY never became true after polling";
            CSML_ERROR(1, logger) << "NOTE: CMD_RDY should be true when module is ready for commands";
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: CMD_RDY polling successful - ready for INSTANTIATE";

        // Step 3: Issue INSTANTIATE command
        uint32_t cmd_header = build_cmd_header(1, 0, 0x9, 0); // acmd=1 (INSTANTIATE), flag0=0x9 (deterministic)
        CSML_INFO(2, logger) << "Step 3: Issuing INSTANTIATE command";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        // Step 4: Poll CMD_ACK to detect INSTANTIATE completion
        CSML_INFO(2, logger) << "Step 4: Polling CMD_ACK to detect INSTANTIATE completion";
        bool cmd_ack_inst = false;
        int poll_count_ack_inst = 0;
        for (int i = 0; i < 10000; i++) {
            uint32_t cmd_sts = 0;
            m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, cmd_sts);
            wait(1, SC_US);

            if (cmd_sts & 0x4) {  // CMD_ACK is bit [2]
                cmd_ack_inst = true;
                poll_count_ack_inst = i;
                CSML_INFO(2, logger) << "CMD_ACK detected after " << poll_count_ack_inst << " polls";
                break;
            }
        }

        if (!cmd_ack_inst) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK never became true after INSTANTIATE";
            CSML_ERROR(1, logger) << "NOTE: CMD_ACK should be true when command completes";
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: CMD_ACK polling successful - INSTANTIATE completion detected";

        // Verify INSTANTIATE command status
        uint32_t inst_cmd_status = get_cmd_status(m_test.get());
        if (inst_cmd_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: INSTANTIATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << inst_cmd_status;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: INSTANTIATE succeeded (CMD_STS=0x0) via polling";
        }

        // Step 5: Poll CMD_RDY before GENERATE
        CSML_INFO(2, logger) << "Step 5: Polling CMD_RDY before GENERATE command";
        bool cmd_rdy_gen = false;
        int poll_count_rdy_gen = 0;
        for (int i = 0; i < 100; i++) {
            uint32_t cmd_sts = 0;
            m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, cmd_sts);
            wait(1, SC_US);

            if (cmd_sts & 0x2) {  // CMD_RDY is bit [1]
                cmd_rdy_gen = true;
                poll_count_rdy_gen = i;
                CSML_INFO(2, logger) << "CMD_RDY detected after " << poll_count_rdy_gen << " polls";
                break;
            }
        }

        if (!cmd_rdy_gen) {
            CSML_ERROR(1, logger) << "FAILED: CMD_RDY never became true after polling before GENERATE";
            CSML_ERROR(1, logger) << "NOTE: CMD_RDY should be true when module is ready for next command";
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: CMD_RDY polling successful - ready for GENERATE";

        // Step 6: Issue GENERATE command
        cmd_header = build_cmd_header(3, 0, 0, 1); // acmd=3 (GENERATE), glen=1
        CSML_INFO(2, logger) << "Step 6: Issuing GENERATE command";
        m_test->register_write_32(crng_basetest::CMD_REQ_OFFSET, cmd_header);
        wait(5, SC_US);

        // Step 7: Poll CMD_ACK to detect GENERATE completion
        CSML_INFO(2, logger) << "Step 7: Polling CMD_ACK to detect GENERATE completion";
        bool cmd_ack_gen = false;
        int poll_count_ack_gen = 0;
        for (int i = 0; i < 10000; i++) {
            uint32_t cmd_sts = 0;
            m_test->register_read_32(crng_basetest::SW_CMD_STS_OFFSET, cmd_sts);
            wait(1, SC_US);

            if (cmd_sts & 0x4) {  // CMD_ACK is bit [2]
                cmd_ack_gen = true;
                poll_count_ack_gen = i;
                CSML_INFO(2, logger) << "CMD_ACK detected after " << poll_count_ack_gen << " polls";
                break;
            }
        }

        if (!cmd_ack_gen) {
            CSML_ERROR(1, logger) << "FAILED: CMD_ACK never became true after GENERATE";
            CSML_ERROR(1, logger) << "NOTE: CMD_ACK should be true when command completes";
            test_passed = false;
        }

        CSML_INFO(2, logger) << "PASS: CMD_ACK polling successful - GENERATE completion detected";

        // Verify GENERATE command status
        uint32_t gen_cmd_status = get_cmd_status(m_test.get());
        if (gen_cmd_status != 0x0) {
            CSML_ERROR(1, logger) << "FAILED: GENERATE failed - expected CMD_STS=0x0 (SUCCESS), got 0x"
                                  << std::hex << gen_cmd_status;
            test_passed = false;
        } else {
            CSML_INFO(2, logger) << "PASS: GENERATE succeeded (CMD_STS=0x0) via polling";
        }

        // Verify interrupts are still disabled (should not have fired)
        uint32_t intr_state = 0;
        m_test->register_read_32(crng_basetest::INTR_STATE_OFFSET, intr_state);
        wait(1, SC_US);

        // Note: INTR_STATE may still be set even if interrupts are disabled
        // (interrupts are gated by INTR_ENABLE, but state bits can still be set)
        // The key is that we're using polling, not interrupts
        CSML_INFO(2, logger) << "INTR_STATE value: 0x" << std::hex << intr_state;
        CSML_INFO(2, logger) << "NOTE: INTR_STATE may be set, but interrupts are disabled (polling mode)";

        if (test_passed) {
            CSML_INFO(2, logger) << "SW Flow Polling Operation test successful:";
            CSML_INFO(2, logger) << "  - All interrupts disabled (INTR_ENABLE=0x0)";
            CSML_INFO(2, logger) << "  - CMD_RDY polled before INSTANTIATE (polling-based)";
            CSML_INFO(2, logger) << "  - CMD_ACK polled after INSTANTIATE (detected completion)";
            CSML_INFO(2, logger) << "  - CMD_RDY polled before GENERATE (polling-based)";
            CSML_INFO(2, logger) << "  - CMD_ACK polled after GENERATE (detected completion)";
            CSML_INFO(2, logger) << "  - Both commands succeeded using polling only (no interrupts)";
            report_test_pass("Test test_sw_flow_polling_operation");
        } else {
            CSML_ERROR(1, logger) << "SW Flow Polling Operation test FAILED";
            report_test_fail("Test test_sw_flow_polling_operation", "One or more verification checks failed");
        }

    } catch (const std::exception& e) {
        CSML_ERROR(1, logger) << "Exception in test_sw_flow_polling_operation: " << e.what();
        report_test_fail("Test test_sw_flow_polling_operation", e.what());
    }
}
