/******************************************************************************
 * @file kmac_func020_test.cpp
 * @brief Test cases for FUNC-KMAC-020 (Reset and Initialization)
 *
 * This file implements test cases for FUNC-KMAC-020, verifying proper reset
 * behavior and initialization of the KMAC hardware. Tests cover register
 * reset values, FSM initialization, FIFO state, and reset during active
 * operations.
 *
 * Test Coverage:
 * - TC-001: All registers reset to correct default values
 * - TC-062: KEY_SHARE0/1 cleared to 0 on reset
 * - TC-071: FSM initializes to IDLE state (STATUS.sha3_idle = 1)
 * - TC-121: STATUS.fifo_empty = 1 on reset
 * - TC-184: Reset during ABSORB state reinitializes properly
 * - TC-185: Reset during SQUEEZE state reinitializes properly
 *
 * Test Plan Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-test-plan.md
 * Architecture Reference: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-architecture-behaviour-map.json
 * Detailed Design: /home/shravanr/Documents/tvastaavp/kmac/docs/kmac-detailed-design.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "csml_logger.h"
#include <iostream>
#include <iomanip>

// Logger for test output
static CsmlLogger test_logger;

/******************************************************************************
 * Helper Functions
 ******************************************************************************/

/**
 * @brief Helper function to read STATUS register FSM state bits
 * @param test Pointer to test harness
 * @param sha3_idle Output: sha3_idle bit value (bit 0)
 * @param sha3_absorb Output: sha3_absorb bit value (bit 1)
 * @param sha3_squeeze Output: sha3_squeeze bit value (bit 2)
 */
static void read_status_fsm_bits(kmac_test* test, bool& sha3_idle, bool& sha3_absorb, bool& sha3_squeeze)
{
    uint32_t status_val = 0;
    test->register_read_32(test->STATUS_OFFSET, status_val);

    sha3_idle = (status_val & 0x1) != 0;
    sha3_absorb = (status_val & 0x2) != 0;
    sha3_squeeze = (status_val & 0x4) != 0;
}

/**
 * @brief Helper function to configure CFG_SHADOWED for SHA3-256 mode
 * @param test Pointer to test harness
 */
static void configure_sha3_256_mode(kmac_test* test)
{
    // CFG_SHADOWED: mode=0x0 (SHA3), kstrength=0x2 (L256), kmac_en=0
    uint32_t cfg_val = (0 << 0) | (0x2 << 1) | (0x0 << 4) | (0x1 << 16) | (0x1 << 24);

    // Shadow register duplicate write sequence
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
    test->register_write_32(test->CFG_SHADOWED_OFFSET, cfg_val);
    wait(5, SC_NS);
}

/**
 * @brief Helper function to issue CMD register write
 * @param test Pointer to test harness
 * @param cmd_value Command value to write
 */
static void write_cmd(kmac_test* test, uint32_t cmd_value)
{
    test->register_write_32(test->CMD_OFFSET, cmd_value);
    wait(10, SC_NS); // Allow time for command processing
}

/******************************************************************************
 * TC-001: Verify All Registers Reset to Correct Default Values
 *
 * Verifies that all registers in the KMAC hardware are properly initialized
 * to their documented reset values upon hardware reset assertion.
 ******************************************************************************/
void testbench::test_reset_all_registers_default_values()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-001: test_reset_all_registers_default_values");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        bool all_pass = true;
        std::stringstream failures;

        // Verify INTR_STATE = 0x00000000
        uint32_t intr_state = 0xFFFFFFFF;
        
        test->register_read_32(test->INTR_STATE_OFFSET, intr_state);
        if (intr_state != test->INTR_STATE_RESET) {
            failures << "INTR_STATE: expected 0x" << std::hex << test->INTR_STATE_RESET
                    << ", got 0x" << intr_state << std::dec << "; ";
            all_pass = false;
        }

        // Verify INTR_ENABLE = 0x00000000
        uint32_t intr_enable = 0xFFFFFFFF;
        test->register_read_32(test->INTR_ENABLE_OFFSET, intr_enable);
        if (intr_enable != test->INTR_ENABLE_RESET) {
            failures << "INTR_ENABLE: expected 0x" << std::hex << test->INTR_ENABLE_RESET
                    << ", got 0x" << intr_enable << std::dec << "; ";
            all_pass = false;
        }

        // Verify CFG_REGWEN = 0x00000001 (unlocked)
        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if (cfg_regwen != test->CFG_REGWEN_RESET) {
            failures << "CFG_REGWEN: expected 0x" << std::hex << test->CFG_REGWEN_RESET
                    << ", got 0x" << cfg_regwen << std::dec << "; ";
            all_pass = false;
        }

        // Verify CFG_SHADOWED = 0x00001000 (sideload=1 per RDL default)
        uint32_t cfg_shadowed = 0xFFFFFFFF;
        test->register_read_32(test->CFG_SHADOWED_OFFSET, cfg_shadowed);
        if (cfg_shadowed != test->CFG_SHADOWED_RESET) {
            failures << "CFG_SHADOWED: expected 0x" << std::hex << test->CFG_SHADOWED_RESET
                    << ", got 0x" << cfg_shadowed << std::dec << "; ";
            all_pass = false;
        }

        // Verify STATUS = 0x00004001 (sha3_idle=1, fifo_empty=1)
        uint32_t status = 0;
        test->register_read_32(test->STATUS_OFFSET, status);
        if (status != test->STATUS_RESET) {
            failures << "STATUS: expected 0x" << std::hex << test->STATUS_RESET
                    << ", got 0x" << status << std::dec << "; ";
            all_pass = false;
        }

        // Verify ENTROPY_PERIOD = 0x00000000
        uint32_t entropy_period = 0xFFFFFFFF;
        test->register_read_32(test->ENTROPY_PERIOD_OFFSET, entropy_period);
        if (entropy_period != test->ENTROPY_PERIOD_RESET) {
            failures << "ENTROPY_PERIOD: expected 0x" << std::hex << test->ENTROPY_PERIOD_RESET
                    << ", got 0x" << entropy_period << std::dec << "; ";
            all_pass = false;
        }

        // Verify ENTROPY_REFRESH_HASH_CNT = 0x00000000
        uint32_t hash_cnt = 0xFFFFFFFF;
        test->register_read_32(test->ENTROPY_REFRESH_HASH_CNT_OFFSET, hash_cnt);
        if (hash_cnt != test->ENTROPY_REFRESH_HASH_CNT_RESET) {
            failures << "ENTROPY_REFRESH_HASH_CNT: expected 0x" << std::hex
                    << test->ENTROPY_REFRESH_HASH_CNT_RESET << ", got 0x" << hash_cnt << std::dec << "; ";
            all_pass = false;
        }

        // Verify ENTROPY_REFRESH_THRESHOLD_SHADOWED = 0x00000000
        uint32_t threshold = 0xFFFFFFFF;
        test->register_read_32(test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_OFFSET, threshold);
        if (threshold != test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_RESET) {
            failures << "ENTROPY_REFRESH_THRESHOLD_SHADOWED: expected 0x" << std::hex
                    << test->ENTROPY_REFRESH_THRESHOLD_SHADOWED_RESET << ", got 0x" << threshold << std::dec << "; ";
            all_pass = false;
        }

        // Verify ERR_CODE = 0x00000000
        uint32_t err_code = 0xFFFFFFFF;
        test->register_read_32(test->ERR_CODE_OFFSET, err_code);
        if (err_code != test->ERR_CODE_RESET) {
            failures << "ERR_CODE: expected 0x" << std::hex << test->ERR_CODE_RESET
                    << ", got 0x" << err_code << std::dec << "; ";
            all_pass = false;
        }

        if (all_pass) {
            CSML_INFO(2, test_logger) << "All registers verified at correct reset values";
            report_test_pass("TC-001: test_reset_all_registers_default_values");
        } else {
            report_test_fail("TC-001: test_reset_all_registers_default_values", failures.str());
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-001: test_reset_all_registers_default_values", e.what());
    }
}

/******************************************************************************
 * TC-062: Verify KEY_SHARE0 and KEY_SHARE1 Cleared to 0 on Reset
 *
 * Verifies that all KEY_SHARE0 and KEY_SHARE1 registers (16 words each) are
 * properly zeroed upon hardware reset for security reasons.
 ******************************************************************************/
void testbench::test_reset_key_shares_cleared()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-062: test_reset_key_shares_cleared");

    try {
        // First, write non-zero values to KEY_SHARE registers
        CSML_INFO(2, test_logger) << "Writing non-zero values to KEY_SHARE registers";
        for (int i = 0; i < 16; i++) {
            uint32_t test_value = 0xDEADBEEF + i;
            test->register_write_32(test->KEY_SHARE0_OFFSET + i*4, test_value);
            test->register_write_32(test->KEY_SHARE1_OFFSET + i*4, test_value);
        }
        wait(10, SC_NS);

        // Apply reset
        CSML_INFO(2, test_logger) << "Applying hardware reset";
        apply_reset();
        wait(10, SC_NS);

        // Verify all KEY_SHARE0 registers are 0
        bool all_zero = true;
        for (int i = 0; i < 16; i++) {
            uint32_t key_val = 0xFFFFFFFF;
            test->register_read_32(test->KEY_SHARE0_OFFSET + i*4, key_val);
            if (key_val != 0x00000000) {
                CSML_ERROR(1, test_logger) << "KEY_SHARE0[" << i << "] not zero: 0x"
                                          << std::hex << key_val << std::dec;
                all_zero = false;
            }
        }

        // Verify all KEY_SHARE1 registers are 0
        for (int i = 0; i < 16; i++) {
            uint32_t key_val = 0xFFFFFFFF;
            test->register_read_32(test->KEY_SHARE1_OFFSET + i*4, key_val);
            if (key_val != 0x00000000) {
                CSML_ERROR(1, test_logger) << "KEY_SHARE1[" << i << "] not zero: 0x"
                                          << std::hex << key_val << std::dec;
                all_zero = false;
            }
        }

        if (all_zero) {
            CSML_INFO(2, test_logger) << "All KEY_SHARE0 and KEY_SHARE1 registers correctly zeroed";
            report_test_pass("TC-062: test_reset_key_shares_cleared");
        } else {
            report_test_fail("TC-062: test_reset_key_shares_cleared",
                           "Some KEY_SHARE registers not zeroed");
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-062: test_reset_key_shares_cleared", e.what());
    }
}

/******************************************************************************
 * TC-071: Verify FSM Initializes to IDLE State (STATUS.sha3_idle = 1)
 *
 * Verifies that the KMAC state machine correctly initializes to IDLE state
 * on reset, with STATUS.sha3_idle = 1 and other FSM status bits = 0.
 ******************************************************************************/
void testbench::test_reset_fsm_idle_state()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-071: test_reset_fsm_idle_state");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        // Read STATUS register
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);

        // Verify FSM state bits
        if (sha3_idle && !sha3_absorb && !sha3_squeeze) {
            CSML_INFO(2, test_logger) << "FSM correctly initialized to IDLE state";
            CSML_INFO(2, test_logger) << "  sha3_idle=1, sha3_absorb=0, sha3_squeeze=0";
            report_test_pass("TC-071: test_reset_fsm_idle_state");
        } else {
            std::stringstream reason;
            reason << "FSM not in IDLE state: sha3_idle=" << sha3_idle
                   << ", sha3_absorb=" << sha3_absorb
                   << ", sha3_squeeze=" << sha3_squeeze;
            report_test_fail("TC-071: test_reset_fsm_idle_state", reason.str());
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-071: test_reset_fsm_idle_state", e.what());
    }
}

/******************************************************************************
 * TC-121: Verify STATUS.fifo_empty = 1 on Reset
 *
 * Verifies that the MSG_FIFO empty status bit is correctly set to 1 upon
 * reset, indicating an empty FIFO with no pending message data.
 ******************************************************************************/
void testbench::test_reset_fifo_empty_status()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-121: test_reset_fifo_empty_status");

    try {
        // Apply reset
        apply_reset();
        wait(10, SC_NS);

        // Read STATUS register
        uint32_t status = 0;
        test->register_read_32(test->STATUS_OFFSET, status);

        // Extract FIFO status bits
        bool fifo_empty = (status & (1 << 14)) != 0;  // bit 14
        uint32_t fifo_depth = (status >> 8) & 0x3F;   // bits[13:8]

        // Verify FIFO empty status
        if (fifo_empty && fifo_depth == 0) {
            CSML_INFO(2, test_logger) << "FIFO correctly initialized: empty=1, depth=0";
            report_test_pass("TC-121: test_reset_fifo_empty_status");
        } else {
            std::stringstream reason;
            reason << "FIFO not empty on reset: fifo_empty=" << fifo_empty
                   << ", fifo_depth=" << fifo_depth;
            report_test_fail("TC-121: test_reset_fifo_empty_status", reason.str());
        }

    } catch (const std::exception& e) {
        report_test_fail("TC-121: test_reset_fifo_empty_status", e.what());
    }
}

/******************************************************************************
 * TC-184: Verify Reset During ABSORB State Reinitializes Properly
 *
 * Verifies that asserting reset during an active ABSORB state operation
 * properly reinitializes the hardware to IDLE state with all registers
 * reset to default values.
 ******************************************************************************/
void testbench::test_reset_during_absorb_state()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-184: test_reset_during_absorb_state");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Issue START command to enter ABSORB state
        CSML_INFO(2, test_logger) << "Entering ABSORB state via START command";
        write_cmd(test, 0x1D); // START command

        // Verify in ABSORB state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_absorb) {
            report_test_fail("TC-184: test_reset_during_absorb_state",
                           "Failed to enter ABSORB state");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM confirmed in ABSORB state";

        // Write some message data to MSG_FIFO
        test->register_write_32(0x800, 0x12345678); // MSG_FIFO address
        wait(5, SC_NS);

        // Assert reset while in ABSORB state
        CSML_INFO(2, test_logger) << "Asserting reset during ABSORB state";
        apply_reset();
        wait(10, SC_NS);

        // Verify FSM returned to IDLE
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle || sha3_absorb || sha3_squeeze) {
            report_test_fail("TC-184: test_reset_during_absorb_state",
                           "FSM not in IDLE state after reset");
            return;
        }

        // Verify FIFO empty
        uint32_t status = 0;
        test->register_read_32(test->STATUS_OFFSET, status);
        bool fifo_empty = (status & (1 << 14)) != 0;
        if (!fifo_empty) {
            report_test_fail("TC-184: test_reset_during_absorb_state",
                           "FIFO not empty after reset");
            return;
        }

        // Verify CFG_REGWEN unlocked
        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) == 0) {
            report_test_fail("TC-184: test_reset_during_absorb_state",
                           "CFG_REGWEN not unlocked after reset");
            return;
        }

        CSML_INFO(2, test_logger) << "Reset during ABSORB properly reinitialized hardware";
        report_test_pass("TC-184: test_reset_during_absorb_state");

    } catch (const std::exception& e) {
        report_test_fail("TC-184: test_reset_during_absorb_state", e.what());
    }
}

/******************************************************************************
 * TC-185: Verify Reset During SQUEEZE State Reinitializes Properly
 *
 * Verifies that asserting reset during SQUEEZE state (after digest generation)
 * properly reinitializes the hardware to IDLE state with all registers reset.
 ******************************************************************************/
void testbench::test_reset_during_squeeze_state()
{
    test_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    report_test_start("TC-185: test_reset_during_squeeze_state");

    try {
        // Initial reset
        apply_reset();
        wait(10, SC_NS);

        // Configure SHA3-256 mode
        configure_sha3_256_mode(test);

        // Issue START command
        CSML_INFO(2, test_logger) << "Starting hash operation";
        write_cmd(test, 0x1D); // START command

        // Issue PROCESS command to enter SQUEEZE state
        CSML_INFO(2, test_logger) << "Entering SQUEEZE state via PROCESS command";
        write_cmd(test, 0x2E); // PROCESS command
        wait(20, SC_NS); // Allow time for padding and Keccak rounds

        // Verify in SQUEEZE state
        bool sha3_idle, sha3_absorb, sha3_squeeze;
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_squeeze) {
            report_test_fail("TC-185: test_reset_during_squeeze_state",
                           "Failed to enter SQUEEZE state");
            return;
        }
        CSML_INFO(2, test_logger) << "FSM confirmed in SQUEEZE state";

        // Assert reset while in SQUEEZE state
        CSML_INFO(2, test_logger) << "Asserting reset during SQUEEZE state";
        apply_reset();
        wait(10, SC_NS);

        // Verify FSM returned to IDLE
        read_status_fsm_bits(test, sha3_idle, sha3_absorb, sha3_squeeze);
        if (!sha3_idle || sha3_absorb || sha3_squeeze) {
            report_test_fail("TC-185: test_reset_during_squeeze_state",
                           "FSM not in IDLE state after reset");
            return;
        }

        // Verify FIFO empty
        uint32_t status = 0;
        test->register_read_32(test->STATUS_OFFSET, status);
        bool fifo_empty = (status & (1 << 14)) != 0;
        if (!fifo_empty) {
            report_test_fail("TC-185: test_reset_during_squeeze_state",
                           "FIFO not empty after reset");
            return;
        }

        // Verify CFG_REGWEN unlocked
        uint32_t cfg_regwen = 0;
        test->register_read_32(test->CFG_REGWEN_OFFSET, cfg_regwen);
        if ((cfg_regwen & 0x1) == 0) {
            report_test_fail("TC-185: test_reset_during_squeeze_state",
                           "CFG_REGWEN not unlocked after reset");
            return;
        }

        // Verify STATE window returns zero
        uint32_t state_val = 0xFFFFFFFF;
        test->register_read_32(0x400, state_val); // STATE window base
        if (state_val != 0x00000000) {
            report_test_fail("TC-185: test_reset_during_squeeze_state",
                           "STATE window not zero after reset");
            return;
        }

        CSML_INFO(2, test_logger) << "Reset during SQUEEZE properly reinitialized hardware";
        report_test_pass("TC-185: test_reset_during_squeeze_state");

    } catch (const std::exception& e) {
        report_test_fail("TC-185: test_reset_during_squeeze_state", e.what());
    }
}

// End of file
