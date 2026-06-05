#include "testbench.h"



/// FUNC-001: Flash Fast Read Sequence
void testbench::test_func001_flash_fast_read_sequence()
{
    using namespace spi_controller_regs;  /// Use common register offsets

    bool test_passed = true;
    int sub_tests_passed = 0;
    int sub_tests_failed = 0;

    CSML_INFO(1, logger) << "\n========================================" << std::endl
                         << "[TEST FUNC-001] Flash Fast Read Sequence" << std::endl
                         << "========================================" << std::endl;

    uint32_t status_val = 0;

    // =======================================================================
    // Step 1: Initial Configuration
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 1] Initial Configuration" << std::endl;

    // CTRL: SPIEN=1, OUTPUT_EN=1, SW_RST=0, RX_WATERMARK=0x00, TX_WATERMARK=0x00
    test->write_register_32(CTRL_OFFSET, 0xE0000000);
    wait(10, SC_NS);

    // CFG: CLKDIV=10 (SCK = clk_i/22), CPOL=0, CPHA=0, FULLCYC=0, timing margins=0
    test->write_register_32(CFG_OFFSET, 0x0000000A);
    wait(10, SC_NS);

    // CSID: Select device 0
    test->write_register_32(CSID_OFFSET, 0x0);
    wait(10, SC_NS);

    // Verify ERROR_STATUS is clear
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors after configuration" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << " after configuration" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify STATUS.READY=1 before proceeding
    test->read_register_32(STATUS_OFFSET, status_val);
    bool ready = (status_val >> 31) & 0x1;
    bool active = (status_val >> 30) & 0x1;
    bool byteorder = (status_val >> 22) & 0x1;

    if (ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] STATUS.READY=1, ACTIVE=0 (ready for commands)" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] STATUS: READY=" << ready << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-001: Flash Fast Read Sequence", test_passed);
        return;
    }

    // =======================================================================
    // Step 2: Load TX FIFO with Flash Fast Read Command (0x0B) + 24-bit Address
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 2] Load TX FIFO with Command+Address" << std::endl;

    // Flash Fast Read: 0x0B (command) + 0x123456 (address) = 0x0B123456
    // With Little-Endian (default): transmitted as 0x56, 0x34, 0x12, 0x0B
    uint32_t cmd_addr = 0x0B123456;
    test->write_register_32(TXDATA_OFFSET, cmd_addr);
    wait(10, SC_NS);

    // Verify TX FIFO depth = 1 word
    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t txqd = status_val & 0xFF;
    bool txempty = (status_val >> 28) & 0x1;

    if (txqd == 1 && !txempty) {
        CSML_INFO(2, logger) << "  [PASS] TX FIFO loaded: TXQD=1, TXEMPTY=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] TX FIFO state: TXQD=" << txqd << ", TXEMPTY=" << txempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors occurred
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors after TX FIFO load" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 3: Pre-load Slave with Expected Flash Data
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 3] Pre-load Slave with Test Data" << std::endl;

    std::vector<uint8_t> flash_data(256);
    for (int i = 0; i < 256; i++) {
        flash_data[i] = 0x40 + (i & 0xFF);
    }
    test->load_slave_rx_data(flash_data);
    CSML_INFO(2, logger) << "  [INFO] Loaded 256 bytes into slave" << std::endl;

    // =======================================================================
    // Step 4: Segment 1 - TX 4 bytes (command+address), Standard SPI, CSAAT=1
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 4] Segment 1: TX 4 bytes, CSAAT=1" << std::endl;

    // CMD: LEN=3 (4 bytes), DIRECTION=2 (TX-only), SPEED=0 (Standard), CSAAT=1
    // Bit layout per RDL: LEN[8:0]=3, CSAAT[9]=1, SPEED[11:10]=0, DIRECTION[13:12]=2
    uint32_t cmd1 = BUILD_CMD(3, 2, 0, 1);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before Segment 1" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-001: Flash Fast Read Sequence", test_passed);
        return;
    }

    test->write_register_32(CMD_OFFSET, cmd1);
    wait(20, SC_US);

    // Verify segment completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    txempty = (status_val >> 28) & 0x1;
    bool txstall = (status_val >> 27) & 0x1;

    if (!active && txempty && !txstall) {
        CSML_INFO(2, logger) << "  [PASS] Segment 1 completed: ACTIVE=0, TXEMPTY=1, TXSTALL=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Segment 1 status: ACTIVE=" << active << ", TXEMPTY=" << txempty << ", TXSTALL=" << txstall << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors after Segment 1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 5: Segment 2 - Dummy 1 byte, CSAAT=1
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 5] Segment 2: Dummy 1 byte, CSAAT=1" << std::endl;

    // CMD: LEN=0 (1 byte), DIRECTION=0 (Dummy), SPEED=0 (Standard), CSAAT=1
    uint32_t cmd2 = BUILD_CMD(0, 0, 0, 1);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before Segment 2" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-001: Flash Fast Read Sequence", test_passed);
        return;
    }

    test->write_register_32(CMD_OFFSET, cmd2);
    wait(10, SC_US);

    // Verify segment completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;

    if (!active) {
        CSML_INFO(2, logger) << "  [PASS] Segment 2 completed: ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Segment 2 still active" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors after Segment 2" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 6: Segment 3 - RX 256 bytes, CSAAT=0 (final segment)
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 6] Segment 3: RX 256 bytes, CSAAT=0" << std::endl;

    // CMD: LEN=255 (256 bytes), DIRECTION=1 (RX-only), SPEED=0 (Standard), CSAAT=0
    uint32_t cmd3 = BUILD_CMD(255, 1, 0, 0);

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    if (!ready) {
        CSML_ERROR(2, logger) << "  [FAIL] Not READY before Segment 3" << std::endl;
        sub_tests_failed++;
        test_passed = false;
        report_test_result("FUNC-001: Flash Fast Read Sequence", test_passed);
        return;
    }

    test->write_register_32(CMD_OFFSET, cmd3);
    wait(1000, SC_US);

    // Verify segment completed
    test->read_register_32(STATUS_OFFSET, status_val);
    active = (status_val >> 30) & 0x1;
    bool rxstall = (status_val >> 23) & 0x1;

    if (!active && !rxstall) {
        CSML_INFO(2, logger) << "  [PASS] Segment 3 completed: ACTIVE=0, RXSTALL=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Segment 3 status: ACTIVE=" << active << ", RXSTALL=" << rxstall << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Check no errors
    test->read_register_32(ERROR_STATUS_OFFSET, status_val);
    if (status_val == 0) {
        CSML_INFO(2, logger) << "  [PASS] No errors after Segment 3" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] ERROR_STATUS=0x" << std::hex << status_val << std::dec << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 7: Verify RX FIFO Contains Expected Data
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 7] Verify RX FIFO Status" << std::endl;

    test->read_register_32(STATUS_OFFSET, status_val);
    uint32_t rxqd = (status_val >> 8) & 0xFF;
    bool rxfull = (status_val >> 25) & 0x1;
    bool rxempty = (status_val >> 24) & 0x1;

    // RX FIFO depth should be 64 words (256 bytes / 4 bytes per word)
    if (rxqd == 64 && !rxempty) {
        CSML_INFO(2, logger) << "  [PASS] RX FIFO contains expected data: RXQD=64, RXEMPTY=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] RX FIFO status: RXQD=" << rxqd << ", RXEMPTY=" << rxempty << ", RXFULL=" << rxfull << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 8: Read and Verify All 256 Bytes from RX FIFO
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 8] Read and Verify RX Data" << std::endl;

    std::vector<uint8_t> received_data;
    received_data.reserve(256);

    // Read all 64 words from RX FIFO
    for (int i = 0; i < 64; i++) {
        uint32_t rx_word;
        test->read_register_32(RXDATA_OFFSET, rx_word);

        // Unpack based on byte order (Little-Endian by default)
        if (byteorder) {  /// Little-Endian
            received_data.push_back(rx_word & 0xFF);
            received_data.push_back((rx_word >> 8) & 0xFF);
            received_data.push_back((rx_word >> 16) & 0xFF);
            received_data.push_back((rx_word >> 24) & 0xFF);
        } else {  /// Big-Endian
            received_data.push_back((rx_word >> 24) & 0xFF);
            received_data.push_back((rx_word >> 16) & 0xFF);
            received_data.push_back((rx_word >> 8) & 0xFF);
            received_data.push_back(rx_word & 0xFF);
        }

        wait(5, SC_NS);
    }

    // Verify all 256 bytes match expected flash data
    bool data_match = true;
    int mismatch_count = 0;
    int first_mismatch = -1;

    for (int i = 0; i < 256; i++) {
        if (received_data[i] != flash_data[i]) {
            data_match = false;
            mismatch_count++;
            if (first_mismatch == -1) {
                first_mismatch = i;
            }
        }
    }

    if (data_match) {
        CSML_INFO(2, logger) << "  [PASS] All 256 bytes match expected flash data" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Data mismatch: " << mismatch_count << " bytes differ, first at byte " << first_mismatch
                  << " (expected=0x" << std::hex << (int)flash_data[first_mismatch]
                  << ", received=0x" << (int)received_data[first_mismatch] << std::dec << ")" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // Verify RX FIFO is now empty
    test->read_register_32(STATUS_OFFSET, status_val);
    rxqd = (status_val >> 8) & 0xFF;
    rxempty = (status_val >> 24) & 0x1;

    if (rxqd == 0 && rxempty) {
        CSML_INFO(2, logger) << "  [PASS] RX FIFO empty after read: RXQD=0, RXEMPTY=1" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] RX FIFO not empty: RXQD=" << rxqd << ", RXEMPTY=" << rxempty << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 9: Verify FSM Returned to IDLE State
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 9] Verify FSM State" << std::endl;

    test->read_register_32(STATUS_OFFSET, status_val);
    ready = (status_val >> 31) & 0x1;
    active = (status_val >> 30) & 0x1;

    if (ready && !active) {
        CSML_INFO(2, logger) << "  [PASS] FSM in IDLE: READY=1, ACTIVE=0" << std::endl;
        sub_tests_passed++;
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] FSM not in IDLE: READY=" << ready << ", ACTIVE=" << active << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Step 10: Verify Slave Received Correct TX Data
    // =======================================================================
    CSML_INFO(1, logger) << "\n[Step 10] Verify Slave Captured TX Data" << std::endl;

    const std::vector<uint8_t>& captured_tx = test->get_slave_captured_tx_data();

    // Expected TX data (Little-Endian byte order for 0x0B123456)
    std::vector<uint8_t> expected_tx = {0x56, 0x34, 0x12, 0x0B};

    if (captured_tx.size() >= 4) {
        bool tx_match = true;
        for (int i = 0; i < 4; i++) {
            if (captured_tx[i] != expected_tx[i]) {
                tx_match = false;
                CSML_ERROR(2, logger) << "  [FAIL] TX byte[" << i << "] mismatch: expected=0x"
                          << std::hex << (int)expected_tx[i]
                          << ", captured=0x" << (int)captured_tx[i] << std::dec << std::endl;
            }
        }

        if (tx_match) {
            CSML_INFO(2, logger) << "  [PASS] Slave received correct TX data (0x0B 0x12 0x34 0x56)" << std::endl;
            sub_tests_passed++;
        } else {
            sub_tests_failed++;
            test_passed = false;
        }
    } else {
        CSML_ERROR(2, logger) << "  [FAIL] Slave captured only " << captured_tx.size() << " bytes (expected 4)" << std::endl;
        sub_tests_failed++;
        test_passed = false;
    }

    // =======================================================================
    // Test Summary
    // =======================================================================
    print_test_summary(sub_tests_passed, sub_tests_failed);

    test->clear_slave_state();

    // Report final test result
    report_test_result("FUNC-001: Flash Fast Read Sequence", test_passed);
}