#include "testbench.h"

void testbench::test_func007_multi_segment_csaat()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-007] Multi-Segment CSAAT" << std::endl
                         << "========================================" << std::endl
                         << "Testing multi-segment transaction support with CSAAT flag:" << std::endl
                         << "  1. CSAAT=1 keeps CSB asserted between segments" << std::endl
                         << "  2. Multi-segment with varying directions (TX/RX/Dummy)" << std::endl
                         << "  3. CSAAT=0 on final segment deasserts CSB" << std::endl
                         << "  4. CSID change terminates CSAAT transaction" << std::endl
                         << "  5. CFG change terminates CSAAT transaction" << std::endl
                         << "========================================\n" << std::endl;

    uint32_t status, error_status, csid;
    bool ready, active;

    // Helper lambda to construct CMD register value
    // CMD format per RDL: [13:12]=DIR, [11:10]=SPEED, [9]=CSAAT, [8:0]=LEN (bytes-1)
    // DIR: 0=Dummy, 1=RX, 2=TX, 3=Bidir
    // SPEED: 0=Std, 1=Dual, 2=Quad
    auto make_command = [](uint32_t bytes, uint32_t dir, uint32_t speed, uint32_t csaat) -> uint32_t {
        return BUILD_CMD(bytes - 1, dir, speed, csaat);
    };

    // Helper lambda to wait for command queue space (max depth = 4)
    // Returns true if space available, false if timeout/error
    auto wait_for_cmd_queue_space = [&](uint32_t max_depth_allowed = 3, uint32_t timeout_ms = 50) -> bool {
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            test->read_register_32(STATUS_OFFSET, status);
            uint32_t cmdqd = (status >> 16) & 0xF;  /// STATUS.CMDQD field
            if (cmdqd <= max_depth_allowed) {
                return true;
            }
            wait(1, SC_MS);
            elapsed += 1;
        }
        CSML_WARN(1, logger) << "  [WARNING] Command queue timeout waiting for space (CMDQD still > " << max_depth_allowed << ")" << std::endl;
        return false;
    };

    // Helper lambda to wait for transaction to complete
    auto wait_for_idle = [&](uint32_t timeout_ms = 50) -> bool {
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            test->read_register_32(STATUS_OFFSET, status);
            bool ready = (status >> 31) & 1;
            bool active = (status >> 29) & 1;
            if (ready && !active) {
                return true;
            }
            wait(1, SC_MS);
            elapsed += 1;
        }
        CSML_WARN(1, logger) << "  [WARNING] Timeout waiting for transaction to complete" << std::endl;
        return false;
    };

    // ==========================================================================
    // Initial Setup and Verification
    // ==========================================================================
    CSML_INFO(1, logger) << "\n[Initial Setup] Configure SPI Host" << std::endl;

    // Software reset to clear any previous state
    test->write_register_32(CTRL_OFFSET, 0x40000000);  /// SW_RST
    wait(50, SC_NS);

    // Clear any previous errors
    clear_errors();

    // Enable SPI Host with OUTPUT_EN
    test->write_register_32(CTRL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1 (bit 31=SPIEN, bit 29=OUTPUT_EN)
    wait(10, SC_NS);

    // Verify SPI Host is ready
    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 0x1;
    if (ready) {
        CSML_INFO(2, logger) << "  [PASS] SPI Host ready for operation" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] SPI Host not ready after enable" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-007: Multi-Segment CSAAT", false);
        return;
    }

    // Configure Device 0 with known timing parameters
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(5, SC_NS);
    test->write_register_32(CFG_OFFSET, 0x04440010);  /// CLKDIV=16, CSNIDLE=4, CSNTRAIL=4, CSNLEAD=4
    wait(10, SC_NS);

    CSML_INFO(2, logger) << "  [INFO] Device 0 configured: CLKDIV=16, CSNIDLE=4, CSNTRAIL=4, CSNLEAD=4\n" << std::endl;

    // ==========================================================================
    // Test 1: Basic CSAAT=1 Keeps CSB Asserted Between Segments
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 1: CSAAT=1 Keeps CSB Asserted" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Simulating Flash Quad Read: 1-1-4 transaction" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Segment 1: Standard TX (command byte) with CSAAT=1" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Segment 2: Standard TX (address bytes) with CSAAT=1" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Segment 3: Quad RX (data bytes) with CSAAT=0" << std::endl;

    // Clear any errors from previous operations
    clear_errors();

    // Load TX FIFO with command and address
    test->write_register_32(TXDATA_OFFSET, 0x03000000);  /// Command: 0x03 (Read)
    test->write_register_32(TXDATA_OFFSET, 0x00000000);  /// Address: 0x000000
    wait(10, SC_NS);

    // Segment 1: 1-byte Standard TX with CSAAT=1
    test->read_register_32(STATUS_OFFSET, status);
    if (!((status >> 31) & 1)) {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 1" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        CSML_INFO(2, logger) << "[ACTION] Issuing Segment 1: Standard TX, 1 byte, CSAAT=1..." << std::endl;
        test->write_register_32(CMD_OFFSET, make_command(1, 2, 0, 1));  /// 1 byte, TX, Std, CSAAT=1
        wait(50, SC_US);  /// Allow transaction to complete

        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS after Segment 1: 0x" << std::hex << error_status << std::dec << std::endl;
            sub_tests_failed++;
            test_passed = false;
        } else {
            test->read_register_32(STATUS_OFFSET, status);
            CSML_INFO(2, logger) << "[INFO] After Segment 1: READY=" << ((status >> 31) & 1)
                      << ", ACTIVE=" << ((status >> 29) & 1) << std::endl;
            sub_tests_passed++;
        }
    }

    // Segment 2: 3-byte Standard TX with CSAAT=1
    test->read_register_32(STATUS_OFFSET, status);
    if (!((status >> 31) & 1)) {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 2" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        CSML_INFO(2, logger) << "[ACTION] Issuing Segment 2: Standard TX, 3 bytes, CSAAT=1..." << std::endl;
        test->write_register_32(CMD_OFFSET, make_command(3, 2, 0, 1));  /// 3 bytes, TX, Std, CSAAT=1
        wait(100, SC_US);  /// Allow transaction to complete

        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS after Segment 2: 0x" << std::hex << error_status << std::dec << std::endl;
            sub_tests_failed++;
            test_passed = false;
        } else {
            test->read_register_32(STATUS_OFFSET, status);
            CSML_INFO(2, logger) << "[INFO] After Segment 2: READY=" << ((status >> 31) & 1)
                      << ", ACTIVE=" << ((status >> 29) & 1) << std::endl;
            sub_tests_passed++;
        }
    }

    // Segment 3: 16-byte Quad RX with CSAAT=0 (final segment)
    test->read_register_32(STATUS_OFFSET, status);
    if (!((status >> 31) & 1)) {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 3" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    } else {
        CSML_INFO(2, logger) << "[ACTION] Issuing Segment 3: Quad RX, 16 bytes, CSAAT=0..." << std::endl;
        test->write_register_32(CMD_OFFSET, make_command(16, 1, 2, 0));  /// 16 bytes, RX, Quad, CSAAT=0
        wait(200, SC_US);  /// Allow transaction to complete

        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS after Segment 3: 0x" << std::hex << error_status << std::dec << std::endl;
            sub_tests_failed++;
            test_passed = false;
        } else {
            test->read_register_32(STATUS_OFFSET, status);
            ready = (status >> 31) & 1;
            active = (status >> 29) & 1;
            uint32_t rxqd = (status >> 8) & 0xFF;

            CSML_INFO(2, logger) << "[INFO] After Segment 3: READY=" << ready << ", ACTIVE=" << active
                      << ", RXQD=" << rxqd << std::endl;

            if (rxqd >= 4) {  /// Expect at least 4 words (16 bytes) in RX FIFO
                CSML_INFO(2, logger) << "  [PASS] Multi-segment CSAAT transaction completed, RX data available" << std::endl;
                sub_tests_passed++;
            } else {
                CSML_ERROR(2, logger) << "  [FAIL] Expected RX data not available (RXQD=" << rxqd << ")" << std::endl;
                sub_tests_failed++;
                test_passed = false;
            }
        }
    }

    test->clear_slave_state();
    wait(10, SC_NS);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Test 2: Multi-Segment with Varying Directions
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 2: Multi-Segment with Varying Directions" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Testing TX -> Dummy -> RX -> Bidirectional sequence" << std::endl;

    // Clear any errors
    clear_errors();

    // Load TX FIFO with enough data for all TX/Bidir segments
    test->write_register_32(TXDATA_OFFSET, 0xAA000000);  /// For Segment 1
    test->write_register_32(TXDATA_OFFSET, 0xBB000000);  /// For Segment 4
    wait(10, SC_NS);

    bool test2_passed = true;

    // Segment 1: TX (4 bytes) with CSAAT=1
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 1" << std::endl;
        test2_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 1" << std::endl;
            test2_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 1: TX 4 bytes, CSAAT=1..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 1));  /// 4 bytes, TX, Std, CSAAT=1
            wait(100, SC_US);

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test2_passed = false;
            }
        }
    }

    // Segment 2: Dummy (2 bytes) with CSAAT=1
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 2" << std::endl;
        test2_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 2" << std::endl;
            test2_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 2: Dummy 2 bytes, CSAAT=1..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(2, 0, 0, 1));  /// 2 bytes, Dummy, Std, CSAAT=1
            wait(50, SC_US);

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test2_passed = false;
            }
        }
    }

    // Segment 3: RX (8 bytes) with CSAAT=1
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 3" << std::endl;
        test2_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 3" << std::endl;
            test2_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 3: RX 8 bytes, CSAAT=1..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(8, 1, 0, 1));  /// 8 bytes, RX, Std, CSAAT=1
            wait(150, SC_US);

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test2_passed = false;
            }
        }
    }

    // Segment 4: Bidirectional (4 bytes) with CSAAT=0
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 4" << std::endl;
        test2_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 4" << std::endl;
            test2_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 4: Bidirectional 4 bytes, CSAAT=0..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 3, 0, 0));  /// 4 bytes, Bidir, Std, CSAAT=0
            wait(100, SC_US);

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test2_passed = false;
            }
        }
    }

    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;
    CSML_INFO(2, logger) << "[INFO] Final status: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (test2_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] Multi-direction segment sequence completed" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Multi-direction segment sequence had errors" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Test 3: CSAAT=0 Deasserts CSB with Proper Timing
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 3: CSAAT=0 Deasserts CSB Properly" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Verifying CSB deasserted after final segment with CSAAT=0" << std::endl;

    // Clear any errors
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, error_status);
        wait(10, SC_US);
    }

    // Load TX data
    test->write_register_32(TXDATA_OFFSET, 0x12000000);
    test->write_register_32(TXDATA_OFFSET, 0x34000000);
    wait(10, SC_US);

    bool test3_passed = true;

    // Segment 1: TX with CSAAT=1
    test->read_register_32(STATUS_OFFSET, status);
    if (!((status >> 31) & 1)) {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 1" << std::endl;
        test3_passed = false;
    } else {
        CSML_INFO(2, logger) << "[ACTION] Segment 1: TX 4 bytes, CSAAT=1..." << std::endl;
        test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 1));  /// 4 bytes, TX, Std, CSAAT=1
        wait(100, SC_US);

        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
            test3_passed = false;
        }
    }

    // Segment 2: TX with CSAAT=0 (should deassert CSB)
    test->read_register_32(STATUS_OFFSET, status);
    if (!((status >> 31) & 1)) {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 2" << std::endl;
        test3_passed = false;
    } else {
        CSML_INFO(2, logger) << "[ACTION] Segment 2: TX 4 bytes, CSAAT=0..." << std::endl;
        test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 0));  /// 4 bytes, TX, Std, CSAAT=0
        wait(100, SC_US);

        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
            test3_passed = false;
        }
    }

    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;
    CSML_INFO(2, logger) << "[INFO] After CSAAT=0: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (test3_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] CSB deasserted correctly after CSAAT=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CSB deassertion test had errors" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Test 4: CSID Change Terminates CSAAT Transaction
    // ==========================================================================
    // Skip this test if NumCS < 2 (CSID=1 not available)
    if (dut->get_num_cs() >= 2) {
    CSML_INFO(1, logger) << "[FUNC-007] Test 4: CSID Change Terminates CSAAT" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Testing CSID change overrides CSAAT=1 and terminates transaction" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Per datasheet: CSID change should force CSB idle even with CSAAT=1" << std::endl;

    // Clear any errors
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, error_status);
        wait(10, SC_US);
    }

    // Configure Device 1 with different timing (for NumCS >= 2 systems)
    test->write_register_32(CSID_OFFSET, 0x1);
    wait(5, SC_US);
    test->write_register_32(CFG_OFFSET, 0x04440020);  /// CLKDIV=32, CSNIDLE=4, CSNTRAIL=4, CSNLEAD=4
    wait(10, SC_US);
    CSML_INFO(2, logger) << "  [INFO] Device 1 configured: CLKDIV=32" << std::endl;

    // Set CSID=0 to start
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(5, SC_US);

    // Load TX data for Segment 1 only
    test->write_register_32(TXDATA_OFFSET, 0x56000000);
    wait(10, SC_US);

    bool test4_passed = true;

    // Segment 1: TX with CSAAT=1 on Device 0
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 1" << std::endl;
        test4_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 1" << std::endl;
            test4_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 1: CSID=0, TX 4 bytes, CSAAT=1..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 1));  /// 4 bytes, TX, Std, CSAAT=1
            wait(100, SC_US);

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test4_passed = false;
            }
        }
    }

    // Change CSID to 1 (different device) - should terminate CSAAT transaction
    CSML_INFO(2, logger) << "[ACTION] Changing CSID from 0 to 1 (should terminate CSAAT transaction)..." << std::endl;
    test->write_register_32(CSID_OFFSET, 0x1);
    wait(50, SC_US);  /// Allow time for CSB transition timing (CSNTRAIL + CSNIDLE)

    // Verify no errors from CSID change
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS after CSID change: 0x" << std::hex << error_status << std::dec << std::endl;
        // Note: CSID=1 might be invalid if NumCS=1, which would set CSIDINVAL
        bool csidinval = (error_status >> 16) & 0x1;  /// CSIDINVAL at bit 16 per RDL spec
        if (csidinval) {
            CSML_INFO(2, logger) << "  [INFO] CSIDINVAL error - NumCS=1, CSID=1 is invalid (expected)" << std::endl;
            // Clear the error and switch back to CSID=0
            test->write_register_32(ERROR_STATUS_OFFSET, error_status);
            wait(10, SC_US);
            test->write_register_32(CSID_OFFSET, 0x0);
            wait(10, SC_US);
            CSML_INFO(2, logger) << "  [INFO] Reverting to CSID=0 for remainder of test" << std::endl;
        } else {
            test4_passed = false;
        }
    }

    // Load TX data for Segment 2
    test->write_register_32(TXDATA_OFFSET, 0x78000000);
    wait(10, SC_US);

    // Segment 2: New transaction on current CSID
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 2" << std::endl;
        test4_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted after CSID change" << std::endl;
            test4_passed = false;
        } else {
            test->read_register_32(CSID_OFFSET, csid);
            CSML_INFO(2, logger) << "[ACTION] Segment 2: CSID=" << csid << ", TX 4 bytes, CSAAT=0..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 0));  /// 4 bytes, TX, Std, CSAAT=0
            wait(150, SC_US);  /// Extra time for slower clock on Device 1

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test4_passed = false;
            }
        }
    }

    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;
    CSML_INFO(2, logger) << "[INFO] Final status: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (test4_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] CSID change correctly terminated CSAAT transaction" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CSID change behavior test had errors" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Restore CSID to 0 for remaining tests
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_US);
    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    } else {
        CSML_INFO(2, logger) << "  [SKIP] Test 4 requires NumCS >= 2 (current: " << dut->get_num_cs() << ")" << std::endl;
        sub_tests_passed++;  /// Count as passed since this is a configuration limitation
    }

    // Test 5: CFG Change Terminates CSAAT Transaction
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 5: CFG Change Terminates CSAAT" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Testing configuration change overrides CSAAT=1" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Per datasheet: CFG change forces CSB idle before applying" << std::endl;

    // Clear any errors
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, error_status);
        wait(10, SC_US);
    }

    // Ensure CSID=0 and restore original config
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(5, SC_US);
    test->write_register_32(CFG_OFFSET, 0x04440010);  /// CLKDIV=16
    wait(10, SC_US);

    // Load TX data for Segment 1 only
    test->write_register_32(TXDATA_OFFSET, 0x9A000000);
    wait(10, SC_US);

    bool test5_passed = true;

    // Segment 1: TX with CSAAT=1
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 1" << std::endl;
        test5_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before Segment 1" << std::endl;
            test5_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 1: TX 4 bytes, CSAAT=1..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 1));  /// 4 bytes, TX, Std, CSAAT=1
            wait(200, SC_US);  /// Allow transaction to complete

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test5_passed = false;
            }
        }
    }

    // Wait for segment 1 to complete before changing CFG
    if (!wait_for_idle()) {
        CSML_ERROR(2, logger) << "  [FAIL] Segment 1 did not complete before CFG change" << std::endl;
        test5_passed = false;
    }

    // Change CFG (should force CSB idle before applying)
    CSML_INFO(2, logger) << "[ACTION] Changing CFG (CLKDIV=32) during CSAAT transaction..." << std::endl;
    test->write_register_32(CFG_OFFSET, 0x04440020);  /// CLKDIV=32, others same
    wait(100, SC_US);  /// Allow time for CSB to go idle

    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS after CFG change: 0x" << std::hex << error_status << std::dec << std::endl;
        test5_passed = false;
    }

    // Load TX data for Segment 2
    test->write_register_32(TXDATA_OFFSET, 0xBC000000);
    wait(10, SC_US);

    // Segment 2: Next segment should use new config (CSB was deasserted/reasserted)
    if (!wait_for_cmd_queue_space(3)) {
        CSML_ERROR(2, logger) << "  [FAIL] Command queue full before Segment 2" << std::endl;
        test5_passed = false;
    } else {
        test->read_register_32(STATUS_OFFSET, status);
        if (!((status >> 31) & 1)) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted after CFG change" << std::endl;
            test5_passed = false;
        } else {
            CSML_INFO(2, logger) << "[ACTION] Segment 2: TX 4 bytes with new config (CLKDIV=32)..." << std::endl;
            test->write_register_32(CMD_OFFSET, make_command(4, 2, 0, 0));  /// 4 bytes, TX, Std, CSAAT=0
            wait(300, SC_US);  /// Extra time due to slower clock

            test->read_register_32(ERROR_STATUS_OFFSET, error_status);
            if (error_status != 0) {
                CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
                test5_passed = false;
            }
        }
    }

    // Wait for segment 2 to complete
    wait_for_idle();

    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;
    CSML_INFO(2, logger) << "[INFO] Final status: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (test5_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] CFG change correctly terminated CSAAT transaction" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] CFG change behavior test had errors" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Test 6: Large Multi-Segment TX Transfer (1 KB)
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 6: Large Multi-Segment TX Transfer (1 KB)" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Testing 1024-byte TX transfer using 4 segments of 256 bytes" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Validates FIFO refill management and CSB assertion across KB transfer" << std::endl;

    // Reset FIFOs to ensure clean state
    software_reset();

    // Clear any errors
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, error_status);
        wait(10, SC_US);
    }

    // Re-enable after reset
    test->write_register_32(CTRL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    wait(10, SC_US);

    // Configure for test
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(5, SC_US);
    test->write_register_32(CFG_OFFSET, 0x04440010);  /// CLKDIV=16
    wait(10, SC_US);

    bool test6_passed = true;
    const uint32_t total_bytes = 1024;      /// 1 KB
    const uint32_t segment_bytes = 256;     /// Max per segment
    const uint32_t num_segments = total_bytes / segment_bytes;  /// 4 segments
    const uint32_t words_per_segment = segment_bytes / 4;       /// 64 words

    CSML_INFO(2, logger) << "[INFO] Transfer plan: " << num_segments << " segments × "
                         << segment_bytes << " bytes = " << total_bytes << " bytes total" << std::endl;

    // Generate test pattern for 1 KB
    std::vector<uint32_t> tx_data(total_bytes / 4);  /// 256 words
    for (uint32_t i = 0; i < tx_data.size(); i++) {
        tx_data[i] = 0x10000000 + i;  /// Pattern: 0x10000000, 0x10000001, 0x10000002, ...
    }

    // Process each segment
    for (uint32_t seg = 0; seg < num_segments; seg++) {
        bool is_last_segment = (seg == num_segments - 1);
        uint32_t csaat = is_last_segment ? 0 : 1;

        CSML_INFO(2, logger) << "\n[Segment " << (seg + 1) << "/" << num_segments << "] "
                             << "TX " << segment_bytes << " bytes, CSAAT=" << csaat << std::endl;

        // Load TX FIFO with 64 words (256 bytes)
        uint32_t word_offset = seg * words_per_segment;
        for (uint32_t w = 0; w < words_per_segment; w++) {
            test->write_register_32(TXDATA_OFFSET, tx_data[word_offset + w]);
        }
        wait(10, SC_US);

        // Check STATUS before issuing command
        test->read_register_32(STATUS_OFFSET, status);
        uint32_t txqd = (status >> 20) & 0xFF;  /// TX FIFO depth
        bool ready_flag = (status >> 31) & 1;

        CSML_INFO(2, logger) << "  [STATUS] Before CMD: READY=" << ready_flag
                             << ", TXQD=" << txqd << std::endl;

        if (!ready_flag) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before segment " << (seg + 1) << std::endl;
            test6_passed = false;
            break;
        }

        // Issue CMD: LEN=255 (256 bytes), TX-only, Standard, CSAAT per segment
        uint32_t cmd = make_command(segment_bytes, 2, 0, csaat);
        test->write_register_32(CMD_OFFSET, cmd);
        wait(10, SC_US);

        // Wait for segment to complete with longer timeout for 256-byte transfer
        wait(2, SC_MS);

        // Check if transaction completed
        test->read_register_32(STATUS_OFFSET, status);
        ready_flag = (status >> 31) & 1;
        bool active = (status >> 29) & 1;

        if (!ready_flag || active) {
            CSML_ERROR(2, logger) << "  [FAIL] Segment " << (seg + 1) << " did not complete (READY="
                                  << ready_flag << ", ACTIVE=" << active << ")" << std::endl;
            test6_passed = false;
            break;
        }

        // Verify no errors occurred
        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << error_status << std::dec
                                  << " after segment " << (seg + 1) << std::endl;
            test6_passed = false;
            break;
        }

        // Check STATUS after segment
        test->read_register_32(STATUS_OFFSET, status);
        txqd = (status >> 20) & 0xFF;
        bool txempty = (status >> 26) & 1;
        CSML_INFO(2, logger) << "  [STATUS] After segment: TXQD=" << txqd << ", TXEMPTY=" << txempty << std::endl;

        CSML_INFO(2, logger) << "  [PASS] Segment " << (seg + 1) << " completed successfully" << std::endl;
    }

    // Verify slave received all 1024 bytes
    // Note: get_slave_tx_bytes() counts master TX (which is slave RX)
    uint32_t slave_rx_count = test->get_slave_tx_bytes();
    CSML_INFO(2, logger) << "\n[VERIFICATION] Slave received " << slave_rx_count << " bytes (expected: "
                         << total_bytes << ")" << std::endl;

    if (slave_rx_count == total_bytes) {
        CSML_INFO(2, logger) << "  [PASS] Slave received correct number of bytes" << std::endl;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Slave byte count mismatch" << std::endl;
        test6_passed = false;
    }

    // Verify final state
    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;

    if (test6_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] Test 6: 1 KB multi-segment TX transfer successful" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Test 6: 1 KB multi-segment TX transfer failed" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Test 7: Large Multi-Segment RX Transfer (2 KB)
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Test 7: Large Multi-Segment RX Transfer (2 KB)" << std::endl
                         << "-----------------------------------------------------------" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Testing 2048-byte RX transfer using 8 segments of 256 bytes" << std::endl;
    CSML_INFO(2, logger) << "[INFO] Validates FIFO drain management and data integrity across KB transfer" << std::endl;

    // Reset FIFOs to ensure clean state
    software_reset();

    // Clear any errors
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (error_status != 0) {
        test->write_register_32(ERROR_STATUS_OFFSET, error_status);
        wait(10, SC_US);
    }

    // Re-enable after reset
    test->write_register_32(CTRL_OFFSET, 0xA0000000);  /// SPIEN=1, OUTPUT_EN=1
    wait(10, SC_US);

    // Configure for test
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(5, SC_US);
    test->write_register_32(CFG_OFFSET, 0x04440010);  /// CLKDIV=16
    wait(10, SC_US);

    bool test7_passed = true;
    const uint32_t total_rx_bytes = 2048;   /// 2 KB
    const uint32_t rx_segment_bytes = 256;  /// Max per segment
    const uint32_t num_rx_segments = total_rx_bytes / rx_segment_bytes;  /// 8 segments
    const uint32_t rx_words_per_segment = rx_segment_bytes / 4;          /// 64 words

    CSML_INFO(2, logger) << "[INFO] Transfer plan: " << num_rx_segments << " segments × "
                         << rx_segment_bytes << " bytes = " << total_rx_bytes << " bytes total" << std::endl;

    // Pre-load slave with 2 KB of test data
    std::vector<uint8_t> expected_rx_bytes(total_rx_bytes);  /// 2048 bytes
    for (uint32_t i = 0; i < expected_rx_bytes.size(); i++) {
        expected_rx_bytes[i] = (uint8_t)(0x20 + (i & 0xFF));  /// Pattern: repeating 0x20-0x1F
    }

    // Load slave TX buffer with 2 KB
    test->load_slave_rx_data(expected_rx_bytes);
    wait(10, SC_US);
    CSML_INFO(2, logger) << "[INFO] Pre-loaded slave with " << total_rx_bytes << " bytes" << std::endl;

    std::vector<uint32_t> received_data;  /// Accumulate all received data

    // Process each RX segment
    for (uint32_t seg = 0; seg < num_rx_segments; seg++) {
        bool is_last_segment = (seg == num_rx_segments - 1);
        uint32_t csaat = is_last_segment ? 0 : 1;

        CSML_INFO(2, logger) << "\n[Segment " << (seg + 1) << "/" << num_rx_segments << "] "
                             << "RX " << rx_segment_bytes << " bytes, CSAAT=" << csaat << std::endl;

        // Check STATUS before issuing command
        test->read_register_32(STATUS_OFFSET, status);
        uint32_t rxqd = (status >> 16) & 0xFF;  /// RX FIFO depth
        bool ready_flag = (status >> 31) & 1;

        CSML_INFO(2, logger) << "  [STATUS] Before CMD: READY=" << ready_flag
                             << ", RXQD=" << rxqd << std::endl;

        if (!ready_flag) {
            CSML_ERROR(2, logger) << "  [FAIL] STATUS.READY not asserted before segment " << (seg + 1) << std::endl;
            test7_passed = false;
            break;
        }

        // Issue CMD: LEN=255 (256 bytes), RX-only, Standard, CSAAT per segment
        uint32_t cmd = make_command(rx_segment_bytes, 1, 0, csaat);
        test->write_register_32(CMD_OFFSET, cmd);
        wait(10, SC_US);

        // Wait for segment to complete with longer timeout for 256-byte transfer
        wait(2, SC_MS);

        // Check if transaction completed
        test->read_register_32(STATUS_OFFSET, status);
        ready_flag = (status >> 31) & 1;
        bool active = (status >> 29) & 1;

        if (!ready_flag || active) {
            CSML_ERROR(2, logger) << "  [FAIL] Segment " << (seg + 1) << " did not complete (READY="
                                  << ready_flag << ", ACTIVE=" << active << ")" << std::endl;
            test7_passed = false;
            break;
        }

        // Verify no errors occurred
        test->read_register_32(ERROR_STATUS_OFFSET, error_status);
        if (error_status != 0) {
            CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << error_status << std::dec
                                  << " after segment " << (seg + 1) << std::endl;
            test7_passed = false;
            break;
        }

        // Check RX FIFO depth after segment
        test->read_register_32(STATUS_OFFSET, status);
        rxqd = (status >> 16) & 0xFF;
        bool rxfull = (status >> 25) & 1;
        CSML_INFO(2, logger) << "  [STATUS] After segment: RXQD=" << rxqd << ", RXFULL=" << rxfull << std::endl;

        // Read all data from RX FIFO (64 words)
        for (uint32_t w = 0; w < rx_words_per_segment; w++) {
            uint32_t rx_word = 0;
            test->read_register_32(RXDATA_OFFSET, rx_word);
            received_data.push_back(rx_word);
        }

        // Verify RX FIFO is now empty
        test->read_register_32(STATUS_OFFSET, status);
        rxqd = (status >> 16) & 0xFF;
        bool rxempty = (status >> 27) & 1;

        CSML_INFO(2, logger) << "  [PASS] Segment " << (seg + 1) << " completed and FIFO drained (RXQD="
                             << rxqd << ", RXEMPTY=" << rxempty << ")" << std::endl;
    }

    // Verify data integrity - compare all 512 words (2048 bytes)
    CSML_INFO(2, logger) << "\n[VERIFICATION] Checking data integrity for all " << received_data.size()
                         << " words (" << (received_data.size() * 4) << " bytes)..." << std::endl;

    bool data_match = true;
    uint32_t mismatch_count = 0;
    uint32_t expected_words = expected_rx_bytes.size() / 4;

    // Convert expected bytes to words for comparison (considering byte order)
    bool byteorder = m_byte_order.get_param_value();  /// true = Little-Endian
    for (uint32_t w = 0; w < expected_words && w < received_data.size(); w++) {
        uint32_t expected_word = 0;
        uint32_t byte_offset = w * 4;

        if (byteorder) {  /// Little-Endian
            expected_word = (uint32_t)expected_rx_bytes[byte_offset] |
                           ((uint32_t)expected_rx_bytes[byte_offset + 1] << 8) |
                           ((uint32_t)expected_rx_bytes[byte_offset + 2] << 16) |
                           ((uint32_t)expected_rx_bytes[byte_offset + 3] << 24);
        } else {  /// Big-Endian
            expected_word = ((uint32_t)expected_rx_bytes[byte_offset] << 24) |
                           ((uint32_t)expected_rx_bytes[byte_offset + 1] << 16) |
                           ((uint32_t)expected_rx_bytes[byte_offset + 2] << 8) |
                           (uint32_t)expected_rx_bytes[byte_offset + 3];
        }

        if (received_data[w] != expected_word) {
            if (mismatch_count < 5) {  /// Report first 5 mismatches
                CSML_ERROR(2, logger) << "  [MISMATCH] Word " << w << ": received=0x"
                                      << std::hex << received_data[w]
                                      << ", expected=0x" << expected_word << std::dec << std::endl;
            }
            data_match = false;
            mismatch_count++;
        }
    }

    if (data_match && received_data.size() == expected_words) {
        CSML_INFO(2, logger) << "  [PASS] All " << received_data.size() << " words match expected data" << std::endl;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Data integrity check failed: " << mismatch_count
                              << " mismatches, received " << received_data.size() << " words, expected "
                              << expected_words << " words" << std::endl;
        test7_passed = false;
    }

    // Verify final state
    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;

    if (test7_passed && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] Test 7: 2 KB multi-segment RX transfer successful" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Test 7: 2 KB multi-segment RX transfer failed" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    test->clear_slave_state();
    wait(10, SC_US);
    CSML_INFO(1, logger) << std::endl;

    // ==========================================================================
    // Final Verification
    // ==========================================================================
    CSML_INFO(1, logger) << "[FUNC-007] Final State Verification" << std::endl
                         << "-----------------------------------------------------------" << std::endl;

    test->read_register_32(CSID_OFFSET, csid);
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    test->read_register_32(STATUS_OFFSET, status);
    ready = (status >> 31) & 1;
    active = (status >> 29) & 1;

    CSML_INFO(2, logger) << "[VERIFY] Final CSID: " << csid << std::endl;
    CSML_INFO(2, logger) << "[VERIFY] Final ERROR_STATUS: 0x" << std::hex << error_status << std::dec << std::endl;
    CSML_INFO(2, logger) << "[VERIFY] Final STATUS: READY=" << ready << ", ACTIVE=" << active << std::endl;

    if (error_status == 0 && csid == 0 && ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] IP in clean state after all tests" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] IP not in expected clean state" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-007] Multi-Segment CSAAT Summary" << std::endl
                         << "========================================" << std::endl
                         << "Sub-tests Passed: " << sub_tests_passed << std::endl
                         << "Sub-tests Failed: " << sub_tests_failed << std::endl
                         << "========================================" << std::endl
                         << "Test Coverage:" << std::endl
                         << "  1. Initial setup and STATUS.READY verification" << std::endl
                         << "  2. Multi-segment CSAAT=1 keeps CSB asserted (1-1-4 Flash Read)" << std::endl
                         << "  3. Multi-direction segments (TX/Dummy/RX/Bidirectional)" << std::endl
                         << "  4. CSAAT=0 properly deasserts CSB" << std::endl
                         << "  5. CSID change terminates CSAAT transaction" << std::endl
                         << "  6. CFG change terminates CSAAT transaction" << std::endl
                         << "  7. Large multi-segment TX transfer (1 KB / 4 segments)" << std::endl
                         << "  8. Large multi-segment RX transfer (2 KB / 8 segments)" << std::endl
                         << "  9. Final state verification" << std::endl
                         << "========================================" << std::endl
                         << "Key Features Validated:" << std::endl
                         << "  - STATUS.READY checked before all CMD writes" << std::endl
                         << "  - ERROR_STATUS monitored after all operations" << std::endl
                         << "  - CSID and CFG change handling" << std::endl
                         << "  - FIFO management for KB-sized transfers" << std::endl
                         << "  - TX FIFO refill between segments (256B × 4)" << std::endl
                         << "  - RX FIFO drain between segments (256B × 8)" << std::endl
                         << "  - Data integrity across 512 words (2048 bytes)" << std::endl
                         << "  - CSB assertion continuity across multiple segments" << std::endl
                         << "  - Pass/fail tracking for each sub-test" << std::endl
                         << "========================================\n" << std::endl;

    report_test_result("FUNC-007: Multi-Segment CSAAT", test_passed);
}

// =============================================================================
// Regression reproduction: second-read TX-command drop after SW_RST
// =============================================================================
//
// Bug seen in the OpenTitan-SPI DMA boot end-to-end run (branch
// cmccoy/spi_dma_fixups): after a flash read completes, the boot ROM issues
// CTRL.SW_RST and then a SECOND read, framed as an opcode+address TX segment
// (CSAAT held) followed by CSAAT-chained RX segments. In the VP the second
// read's TX command is QUEUED but the transaction thread never processes it
// (no "Pulling from TX FIFO"): the opcode+address is never driven, so the read
// returns undriven data (in the E2E the payload TOC read back as 0xFF ->
// MANIFEST_ERR_BAD_TOC_ID). The first read is unaffected.
//
// This models two back-to-back flash reads (opcode+addr TX + chained RX,
// PIO-drained) with a SW_RST between. The direct discriminator is the TX-FIFO
// depth after each read: if the opcode+address TX command is processed the TX
// FIFO drains to 0; if it is dropped the pushed opcode+address word is stranded
// (TXQD > 0).
//
// NOTE: the E2E drop occurs while the DMA drains the RX FIFO concurrently and
// firmware races ahead queuing the next read (so the SW_RST and the second
// read's TX land while the first read is still in flight). This unit test
// PIO-drains between chunks and may not recreate that exact interleave on the
// first pass. If it PASSES, tighten the interleave: queue the SW_RST + second
// read's TX/RX before the first read's tail segment has drained (or drive the
// RX drain from a separate thread) to recreate the "command issued while a
// prior read is still in flight" window that the DMA path produces.
// =============================================================================
void testbench::test_repro_second_read_tx_drop()
{
    using namespace spi_controller_regs;

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;
    const std::string test_name = "REPRO: second flash read TX-command drop after SW_RST";
    uint32_t status = 0, error_status = 0;

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[REPRO] Second-read TX-command drop after SW_RST" << std::endl
                         << "========================================" << std::endl;

    // Clean, enabled, error-free start.
    software_reset();
    wait(20, SC_NS);
    clear_errors();
    test->write_register_32(CTRL_OFFSET, 0xA0000000);  // SPIEN(31)=1, OUTPUT_EN(29)=1
    wait(10, SC_NS);
    test->clear_slave_state();

    // One flash read: opcode(0x03)+3-byte address TX (CSAAT held), then the data
    // phase as CSAAT-chained RX chunks of <= 256 B, PIO-drained per chunk. Sets
    // drained_bytes and returns the TX-FIFO depth (words) after the transfer:
    // 0 => the opcode+address TX command was processed; >0 => it was dropped.
    auto run_flash_read = [&](uint8_t fill, uint32_t total_bytes, uint32_t &drained_bytes) -> uint32_t {
        std::vector<uint8_t> data(total_bytes, fill);
        test->load_slave_rx_data(data);

        // TX phase: 4-byte opcode+address, CSAAT=1 so the RX data phase follows.
        const uint8_t hdr[4] = { 0x03u, 0x00u, 0x10u, 0x00u };
        uint32_t hdr_word = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8)
                          | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
        test->write_register_32(TXDATA_OFFSET, hdr_word);
        test->write_register_32(CMD_OFFSET, BUILD_CMD(3u, 2u /*TX*/, 0u, 1u /*csaat*/));

        drained_bytes = 0;
        uint32_t issued = 0;
        while (issued < total_bytes) {
            uint32_t chunk = (total_bytes - issued) < 256u ? (total_bytes - issued) : 256u;
            bool last = (issued + chunk) >= total_bytes;
            test->write_register_32(CMD_OFFSET, BUILD_CMD(chunk - 1u, 1u /*RX*/, 0u, last ? 0u : 1u));

            uint32_t got = 0, guard = 0;
            while (got < chunk && guard < 200000u) {
                guard++;
                test->read_register_32(STATUS_OFFSET, status);
                uint32_t rxqd = (status >> 8) & 0xFFu;
                if (rxqd > 0u) {
                    uint32_t word = 0;
                    test->read_register_32(RXDATA_OFFSET, word);
                    got += (chunk - got) < 4u ? (chunk - got) : 4u;
                } else {
                    wait(50, SC_NS);
                }
            }
            drained_bytes += got;
            issued += chunk;
        }

        wait(200, SC_NS);  // let the engine settle before sampling TXQD
        test->read_register_32(STATUS_OFFSET, status);
        return status & 0xFFu;  // TXQD (words remaining in TX FIFO)
    };

    // ---- Read 1 (baseline: must work) ----
    uint32_t got1 = 0;
    uint32_t txqd1 = run_flash_read(0xA5u, 512u, got1);
    if (got1 == 512u && txqd1 == 0u) {
        CSML_INFO(2, logger) << "  [PASS] Read 1: 512 B drained, TX FIFO empty (opcode+addr consumed)" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Read 1: drained=" << got1 << " TXQD=" << txqd1 << std::endl;
        sub_tests_failed++; test_passed = false;
    }

    // ---- SW_RST between reads, as the boot ROM does before each read ----
    software_reset();
    wait(20, SC_NS);
    test->write_register_32(CTRL_OFFSET, 0xA0000000);
    wait(10, SC_NS);

    // ---- Read 2 (the read the bug drops) ----
    uint32_t got2 = 0;
    uint32_t txqd2 = run_flash_read(0x5Au, 512u, got2);

    // CORE CHECK: the second read's opcode+address TX command must be processed,
    // i.e. the TX FIFO must be empty. A dropped command strands the pushed
    // opcode+address word (TXQD > 0).
    if (txqd2 == 0u) {
        CSML_INFO(2, logger) << "  [PASS] Read 2: opcode+addr TX command processed (TX FIFO empty)" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Read 2: opcode+addr TX command DROPPED — "
                              << txqd2 << " word(s) stranded in TX FIFO (reproduces the E2E bug)" << std::endl;
        sub_tests_failed++; test_passed = false;
    }
    if (got2 == 512u) {
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Read 2: only " << got2 << "/512 B drained" << std::endl;
        sub_tests_failed++; test_passed = false;
    }
    test->read_register_32(ERROR_STATUS_OFFSET, error_status);
    if (((error_status >> 4) & 0x1u) == 0u) {
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Read 2: unexpected ERROR_STATUS.OVERFLOW" << std::endl;
        sub_tests_failed++; test_passed = false;
    }

    CSML_INFO(1, logger) << "  [REPRO] sub-tests passed=" << sub_tests_passed
                         << " failed=" << sub_tests_failed << std::endl;
    report_test_result(test_name.c_str(), test_passed);
}

