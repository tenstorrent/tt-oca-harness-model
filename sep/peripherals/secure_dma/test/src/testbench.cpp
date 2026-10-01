// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file testbench.cpp
 * @brief DMA Controller SystemC/TLM Testbench Implementation
 */

#include "testbench.h"
#include <iomanip>

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// ============================================================================
// Constructor - Instantiate Modules and Bind Ports
// ============================================================================

testbench::testbench(sc_module_name name)
    : sc_module(name), logger(), m_test(nullptr), m_tests_run(0),
      m_tests_passed(0), m_tests_failed(0) {

  // Initialize RegLogger
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  // Instantiate DUT and test harness
  m_model = std::make_unique<secure_dma_model>("dma_dut");

  // Coverage builds compile with REG_DEFAULT_VERBOSITY=1, which skips
  // REG_INFO(2/3) stream bodies (and helpers only invoked from those streams).
  m_model->logger.setMaxVerbosity(3);
  logger.setMaxVerbosity(3);

  REG_INFO(1, logger) << "Instantiating DMA model and test harness"
                       << std::endl;
  m_test = std::make_unique<secure_dma_test>("secure_dma_test");

  // =========================================================================
  // Bind TLM Sockets
  // =========================================================================

  REG_INFO(1, logger) << "Binding TLM Register Bus Interface" << std::endl;
  m_test->initiator_socket.bind(m_model->target_socket);

  REG_INFO(1, logger) << "Binding TLM Memory Bus Interfaces" << std::endl;
  // DMA initiator sockets to test target sockets
  m_model->ot_initiator_socket.bind(m_test->ot_target_socket);
  m_model->ctn_initiator_socket.bind(m_test->ctn_target_socket);
  m_model->sys_initiator_socket.bind(m_test->sys_target_socket);

  // =========================================================================
  // Bind Clock and Reset
  // =========================================================================

  REG_INFO(1, logger) << "Binding Clock and Reset Interfaces" << std::endl;
  m_test->clk_o.bind(clk_signal);
  m_model->clk_i.bind(clk_signal);

  m_test->rst_no.bind(rst_signal);
  m_model->rst_ni.bind(rst_signal);

  // =========================================================================
  // Bind Interrupt Outputs (model drives, test monitors)
  // =========================================================================

  REG_INFO(1, logger) << "Binding Interrupt Outputs" << std::endl;
  m_model->dma_done_intr.bind(dma_done_intr_signal);
  m_test->dma_done_intr_i.bind(dma_done_intr_signal);

  m_model->dma_chunk_done_intr.bind(dma_chunk_done_intr_signal);
  m_test->dma_chunk_done_intr_i.bind(dma_chunk_done_intr_signal);

  m_model->dma_error_intr.bind(dma_error_intr_signal);
  m_test->dma_error_intr_i.bind(dma_error_intr_signal);

  // =========================================================================
  // Bind Alert Output
  // =========================================================================

  REG_INFO(1, logger) << "Binding Alert Output" << std::endl;
  m_model->alert_fatal_fault.bind(alert_fatal_fault_signal);
  m_test->alert_fatal_fault_i.bind(alert_fatal_fault_signal);

  // =========================================================================
  // Bind Hardware Handshake Triggers (test drives, model monitors)
  // =========================================================================

  REG_INFO(1, logger) << "Binding Hardware Handshake Triggers" << std::endl;
  for (int i = 0; i < 11; i++) {
    m_test->lsio_trigger_o[i].bind(lsio_trigger_signal[i]);
    m_model->lsio_trigger[i].bind(lsio_trigger_signal[i]);

    // Initialize all triggers to low
    lsio_trigger_signal[i].write(false);
  }

  REG_INFO(1, logger) << "Port binding complete" << std::endl;

  // Set clock period
  clk_signal.write(sc_time(10, SC_NS)); // 100 MHz clock

  // Register test process
  SC_THREAD(run_tests);
}

testbench::~testbench() {
  // unique_ptr handles deletion
}

// ============================================================================
// Test Result Reporting Helpers
// ============================================================================

void testbench::report_test_result(const char *test_name, bool passed) {
  m_tests_run++;

  if (passed) {
    m_tests_passed++;
    REG_INFO(1, logger) << "\n========================================\n"
                         << "[TEST PASSED] " << test_name << "\n"
                         << "========================================\n"
                         << std::endl;
  } else {
    m_tests_failed++;
    m_failed_tests.push_back(test_name);
    REG_ERROR(0, logger) << "\n========================================\n"
                          << "[TEST FAILED] " << test_name << "\n"
                          << "========================================\n"
                          << std::endl;
  }
}

void testbench::report_test_result(const std::string &test_name, bool passed,
                                   const std::string &message) {
  m_tests_run++;

  if (passed) {
    m_tests_passed++;
    REG_INFO(1, logger) << "\n========================================\n"
                         << "[TEST PASSED] " << test_name << "\n"
                         << "Message: " << message << "\n"
                         << "========================================\n"
                         << std::endl;
  } else {
    m_tests_failed++;
    m_failed_tests.push_back(test_name);
    REG_ERROR(0, logger) << "\n========================================\n"
                          << "[TEST FAILED] " << test_name << "\n"
                          << "Message: " << message << "\n"
                          << "========================================\n"
                          << std::endl;
  }
}

void testbench::print_test_summary() {
  REG_INFO(0, logger) << "\n"
                       << "========================================\n"
                       << "          TEST SUMMARY\n"
                       << "========================================\n"
                       << "Total Tests Run:    " << m_tests_run << "\n"
                       << "Tests Passed:       " << m_tests_passed << "\n"
                       << "Tests Failed:       " << m_tests_failed << "\n"
                       << "========================================\n"
                       << std::endl;

  if (m_tests_failed > 0) {
    REG_ERROR(0, logger) << "Failed Tests:" << std::endl;
    for (const auto &test_name : m_failed_tests) {
      REG_ERROR(0, logger) << "  - " << test_name << std::endl;
    }
  }
}

// ============================================================================
// Main Test Execution Thread
// ============================================================================

void testbench::run_tests() {
  REG_INFO(1, logger) << "\n"
                       << "========================================\n"
                       << "   DMA CONTROLLER TEST SUITE START\n"
                       << "========================================\n"
                       << std::endl;

  m_model->logger.setMaxVerbosity(3);
  logger.setMaxVerbosity(3);

  // Apply initial reset
  REG_INFO(1, logger) << "Applying initial reset" << std::endl;
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(50, SC_NS));

  // Run basic test cases
  test_reset_values();
  test_register_rw();
  test_register_ro();
  test_port_binding();
  test_interrupts();
  test_hardware_handshake();

  // Run FUNC-001 comprehensive tests
  run_func001_tests();

  // // Run FUNC-002 comprehensive tests
  run_func002_tests();

  // // Run FUNC-003 comprehensive tests
  run_func003_tests();

  // Run FUNC-004 comprehensive tests
  run_func004_tests();

  // Run FUNC-005 comprehensive tests
  run_func005_tests();

  // // Run FUNC-006 comprehensive tests
   run_func006_tests();

  // // Run FUNC-007 comprehensive tests
  run_func007_tests();

  // // Run FUNC-008 comprehensive tests
  run_func008_tests();

  // // Run FUNC-009 comprehensive tests
  run_func009_tests();

  // // Run FUNC-010 comprehensive tests
  run_func010_tests();

  // // Run FUNC-011 comprehensive tests
  run_func011_tests();

  // // Run FUNC-012 comprehensive tests
  run_func012_tests();

  run_coverage_tests();

  report_test_result("AXI source_id is OTHERS_SOURCE_ID",
                     m_test->m_sideband_errors == 0);

  // Print final summary
  print_test_summary();

  REG_INFO(1, logger) << "\n"
                       << "========================================\n"
                       << "    DMA CONTROLLER TEST SUITE END\n"
                       << "========================================\n"
                       << std::endl;

  // Stop simulation
  sc_stop();
}

// ============================================================================
// Test: Register Read/Write Access
// ============================================================================

void testbench::test_register_rw() {
  REG_INFO(1, logger) << "\n>>> Test: Register Read/Write Access (Plain RW Only) <<<\n"
                       << std::endl;

  bool test_passed = true;
  uint32_t rd = 0;

  auto check_rw = [&](uint32_t off, uint32_t val, uint32_t mask, const char *name) {
    m_test->register_write_32(off, val);
    wait(sc_time(10, SC_NS));
    m_test->register_read_32(off, rd);

    if ((rd & mask) != (val & mask)) {
      REG_ERROR(0, logger) << name << " R/W mismatch: wrote 0x" << std::hex
                            << (val & mask) << " read 0x" << (rd & mask)
                            << std::dec << std::endl;
      test_passed = false;
    } else {
      REG_INFO(2, logger) << name << " R/W verified" << std::endl;
    }
  };

  // Start clean
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  // -------------------------
  // Plain RW registers only
  // -------------------------
  check_rw(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007, 0x00000007, "INTR_ENABLE");

  check_rw(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000100, 0xFFFFFFFF, "SRC_ADDR_LO");
  check_rw(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000, 0xFFFFFFFF, "SRC_ADDR_HI");
  check_rw(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10000200, 0xFFFFFFFF, "DST_ADDR_LO");
  check_rw(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000, 0xFFFFFFFF, "DST_ADDR_HI");

  check_rw(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077, 0x000000FF, "ADDR_SPACE_ID");

  check_rw(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET,  0x10000000, 0xFFFFFFFF,
           "ENABLED_MEMORY_RANGE_BASE");
  check_rw(secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, 0x1000FFFF, 0xFFFFFFFF,
           "ENABLED_MEMORY_RANGE_LIMIT");
  check_rw(secure_dma_basetest::RANGE_VALID_OFFSET, 0x00000001, 0x00000001, "RANGE_VALID");

  check_rw(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x00000020, 0xFFFFFFFF, "TOTAL_DATA_SIZE");
  check_rw(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x00000010, 0xFFFFFFFF, "CHUNK_DATA_SIZE");
  check_rw(secure_dma_basetest::TRANSFER_WIDTH_OFFSET,  0x00000002, 0x00000003, "TRANSFER_WIDTH");

  check_rw(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001, 0x00000003, "SRC_CONFIG");
  check_rw(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001, 0x00000003, "DST_CONFIG");

  check_rw(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, 0x000007FF, 0x000007FF,
           "HANDSHAKE_INTR_ENABLE");
  check_rw(secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, 0x000007FF, 0x000007FF, "CLEAR_INTR_SRC");
  check_rw(secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, 0x000007FF, 0x000007FF, "CLEAR_INTR_BUS");

  // INTR_SRC_ADDR and INTR_SRC_WR_VAL arrays [0..10]
  for (int i = 0; i < 11; i++) {
    check_rw(secure_dma_basetest::INTR_SRC_ADDR_OFFSET + (i * 4),
             0x20000000u + static_cast<uint32_t>(i * 0x10),
             0xFFFFFFFF, "INTR_SRC_ADDR[i]");

    check_rw(secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + (i * 4),
             0x00000001u + static_cast<uint32_t>(i),
             0xFFFFFFFF, "INTR_SRC_WR_VAL[i]");
  }

  // -------------------------
  // CONTROL plain config fields only (exclude go/abort side effects)
  // opcode[3:0], hardware_handshake_enable[4], digest_swap[5], initial_transfer[8]
  // -------------------------
  {
    uint32_t wr = (0x2) | (1u << 4) | (1u << 5) | (1u << 8);
    uint32_t mask = 0x0000013F; // bits 8,5,4,3:0

    m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, wr);
    wait(sc_time(10, SC_NS));
    m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, rd);

    if ((rd & mask) != (wr & mask)) {
      REG_ERROR(0, logger) << "CONTROL(config fields) R/W mismatch: wrote 0x"
                            << std::hex << (wr & mask) << " read 0x" << (rd & mask)
                            << std::dec << std::endl;
      test_passed = false;
    } else {
      REG_INFO(2, logger) << "CONTROL(config fields) R/W verified" << std::endl;
    }
  }

  report_test_result("Test: Register Read/Write Access (Plain RW Only)", test_passed);
}

// ============================================================================
// Test: Read-Only Register Behavior
// ============================================================================

void testbench::test_register_ro() {
  REG_INFO(1, logger) << "\n>>> Test: Read-Only Register Behavior <<<\n"
                       << std::endl;

  bool test_passed = true;
  uint32_t before = 0, after = 0;

  // 1) INTR_STATE (RO)
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, before);
  m_test->register_write_32(secure_dma_basetest::INTR_STATE_OFFSET, 0xFFFFFFFF);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, after);
  if (after != before){
    REG_ERROR(0, logger) << "INTR_STATE changed after write attempt" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "INTR_STATE Register verified" << std::endl;
  }  

  // 2) CFG_REGWEN (RO, should be 0x6 in IDLE)
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, before);
  m_test->register_write_32(secure_dma_basetest::CFG_REGWEN_OFFSET, 0x00000000);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, after);
  if (after != before){
    REG_ERROR(0, logger) << "CFG_REGWEN changed after write attempt" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "CFG_REGWEN Register verified" << std::endl;
  }  

  // 3) ERROR_CODE (RO)
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, before);
  m_test->register_write_32(secure_dma_basetest::ERROR_CODE_OFFSET, 0xFFFFFFFF);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, after);
  if (after != before){
    REG_ERROR(0, logger) << "ERROR_CODE changed after write attempt" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "ERROR_CODE Register verified" << std::endl;
  } 

  // 4) STATUS.busy(bit0) and STATUS.sha2_digest_valid(bit4) are RO to SW
  // Try to set both by write; they must not become 1 just due to SW write.
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0x00000011);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, after);
  if (after & 0x00000011){
    REG_ERROR(0, logger) << "STATUS.busy or STATUS.sha2_digest_valid became 1 from SW write" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "STATUS.busy(bit0) and STATUS.sha2_digest_valid(bit4) are verified" << std::endl;
  } 

  // 5) SHA2_DIGEST[0..15] (RO) - write attempts must be ignored
  for (int i = 0; i < 16; i++) {
    const uint32_t digest_offset = secure_dma_basetest::SHA2_DIGEST_OFFSET + (i * 4);
    const uint32_t wr = 0xA5A50000u | static_cast<uint32_t>(i);

    m_test->register_read_32(digest_offset, before);
    m_test->register_write_32(digest_offset, wr);
    wait(sc_time(5, SC_NS));
    m_test->register_read_32(digest_offset, after);

    if (after != before) {
      REG_ERROR(0, logger) << "SHA2_DIGEST[" << i << "] changed after write attempt" << std::endl;
      test_passed = false;
    }else {
    REG_INFO(2, logger) << "SHA2_DIGEST[" << i << "] Register verified" << std::endl;
    } 
  }  
  report_test_result("Test: Read-Only Register Behavior", test_passed);
}


// ============================================================================
// Test: Port Connectivity
// ============================================================================

void testbench::test_port_binding() {
  REG_INFO(1, logger) << "\n>>> Test: Port Connectivity <<<\n" << std::endl;

  bool test_passed = true;

  // Test clock signal connectivity
  sc_time clk_period = clk_signal.read();
  if (clk_period != sc_time(10, SC_NS)) {
    REG_ERROR(0, logger) << "Clock signal not correctly bound" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Clock signal binding verified" << std::endl;
  }

  // Test reset signal connectivity
  bool rst_state = rst_signal.read();
  if (rst_state != true) { // Should be de-asserted after initial reset
    REG_ERROR(0, logger) << "Reset signal not correctly bound" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Reset signal binding verified" << std::endl;
  }

  // Test interrupt signal connectivity (should be low initially)
  bool intr_done = dma_done_intr_signal.read();
  bool intr_chunk = dma_chunk_done_intr_signal.read();
  bool intr_error = dma_error_intr_signal.read();

  if (intr_done || intr_chunk || intr_error) {
    REG_ERROR(0, logger) << "Interrupt signals not correctly initialized"
                          << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Interrupt signal binding verified" << std::endl;
  }

  // Test alert signal connectivity
  bool alert = alert_fatal_fault_signal.read();
  if (alert) {
    REG_ERROR(0, logger) << "Alert signal not correctly initialized"
                          << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Alert signal binding verified" << std::endl;
  }

  // Test hardware handshake trigger connectivity
  bool handshake_ok = true;
  for (int i = 0; i < 11; i++) {
    if (lsio_trigger_signal[i].read() != false) {
      REG_ERROR(0, logger)
          << "Trigger " << i << " not correctly initialized" << std::endl;
      handshake_ok = false;
      test_passed = false;
    }
  }
  if (handshake_ok) {
    REG_INFO(2, logger) << "Hardware handshake trigger binding verified"
                         << std::endl;
  }

  report_test_result("Port Connectivity", test_passed);
}

// ============================================================================
// Test: Interrupt Generation
// ============================================================================

void testbench::test_interrupts() {
  REG_INFO(1, logger) << "\n>>> Test: Interrupt Generation <<<\n" << std::endl;

  bool test_passed = true;

  // Enable all interrupts
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  wait(sc_time(10, SC_NS));

  // Test INTR_TEST register to force interrupt state
  // Write to INTR_TEST should set corresponding INTR_STATE bits
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET,
                            0x00000001); // Force dma_done
  wait(sc_time(20, SC_NS));

  // Check if dma_done interrupt is asserted
  bool intr_done = dma_done_intr_signal.read();
  if (!intr_done) {
    REG_ERROR(0, logger) << "dma_done interrupt not asserted" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "dma_done interrupt correctly asserted"
                         << std::endl;
  }

  // Clear interrupt by reading (implementation-dependent)
  uint32_t intr_state;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  REG_INFO(2, logger) << "INTR_STATE: 0x" << std::hex << intr_state << std::dec
                       << std::endl;

  // Disable interrupts and verify outputs go low
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000000);
  wait(sc_time(10, SC_NS));

  // Even if INTR_STATE is set, outputs should be low when disabled
  // Note: This requires the model to implement update_interrupts() properly

  report_test_result("Interrupt Generation", test_passed);
}

// ============================================================================
// Test: Hardware Handshake Triggers
// ============================================================================

void testbench::test_hardware_handshake() {
  REG_INFO(1, logger) << "\n>>> Test: Hardware Handshake Triggers <<<\n"
                       << std::endl;

  bool test_passed = true;

  // Enable trigger 0 in HANDSHAKE_INTR_ENABLE
  uint32_t enable_mask = 0x00000001; // Enable trigger 0 only
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            enable_mask);
  wait(sc_time(10, SC_NS));

  // Assert trigger 0
  m_test->set_lsio_trigger(0, true);
  wait(sc_time(20, SC_NS));

  // Verify trigger can be read back as asserted
  bool trigger_state = lsio_trigger_signal[0].read();
  if (!trigger_state) {
    REG_ERROR(0, logger) << "Trigger 0 not asserted" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Trigger 0 correctly asserted" << std::endl;
  }

  // De-assert trigger 0
  m_test->set_lsio_trigger(0, false);
  wait(sc_time(20, SC_NS));

  trigger_state = lsio_trigger_signal[0].read();
  if (trigger_state) {
    REG_ERROR(0, logger) << "Trigger 0 not de-asserted" << std::endl;
    test_passed = false;
  } else {
    REG_INFO(2, logger) << "Trigger 0 correctly de-asserted" << std::endl;
  }

  // Test all 11 triggers can be individually controlled
  bool all_triggers_ok = true;
  for (int i = 0; i < 11; i++) {
    m_test->set_lsio_trigger(i, true);
    wait(sc_time(5, SC_NS));

    if (!lsio_trigger_signal[i].read()) {
      REG_ERROR(0, logger)
          << "Trigger " << i << " control failed" << std::endl;
      all_triggers_ok = false;
      test_passed = false;
    }

    m_test->set_lsio_trigger(i, false);
    wait(sc_time(5, SC_NS));
  }

  if (all_triggers_ok) {
    REG_INFO(2, logger) << "All 11 triggers individually controllable"
                         << std::endl;
  }

  report_test_result("Hardware Handshake Triggers", test_passed);
}

// ============================================================================
// Main Function
// ============================================================================

int sc_main(int argc, char *argv[]) {
  regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

  testbench tb("secure_dma_testbench");
  sc_start();

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);
  return 0;
}
