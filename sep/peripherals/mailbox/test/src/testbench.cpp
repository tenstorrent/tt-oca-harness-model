// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/**
 * @file testbench.cpp
 * @brief Mailbox SystemC testbench implementation
 *
 * This file implements the testbench constructor with complete port binding
 * between mailbox model and test harness, including dual TLM sockets for
 * slave ports, interrupt signals, clock, and reset.
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 */

#include "testbench.h"
#include "reg_logger.h"
#include "reg_param.h"

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

/**
 * @brief Testbench constructor
 *
 * Instantiates mailbox model and dual test harnesses (one per port),
 * then binds all ports.
 */
testbench::testbench(sc_module_name name, int log_verbosity)
    : sc_module(name), m_tests_failed(0), m_tests_run(0), m_tests_passed(0) {

  // Instantiate mailbox model (DUT)
  // Memory size: 0x50 bytes (10 registers × 8 bytes)
  dut = new mailbox_ip("mailbox_dut", log_verbosity);

  // Sync loggers with DUT verbosity (CCI preset may have overridden the build default)
  int resolved_verbosity = dut->verbosity.get_param_value();

  // Configure logger
  logger.setMaxVerbosity(resolved_verbosity);
  logger.setLogFormat(
      "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
  logger.setFunctionTrace(false);

  REG_INFO(2, logger) << "Constructing mailbox testbench";

  // Instantiate test harnesses for both ports
  test_port0 = new mailbox_test("mailbox_test_port0");
  test_port1 = new mailbox_test("mailbox_test_port1");

  // Perform port binding
  bind_ports();

  // Initialize testbench
  initialize();

  // Initialize clock and reset signals
  clk_sig.write(100000000.0); // 100 MHz abstract clock frequency
  rst_ni_sig.write(true);     // Reset inactive (active-low)

  // Register test execution thread
  SC_THREAD(run_tests);

  REG_INFO(2, logger) << "Mailbox testbench construction complete";
}

/**
 * @brief Testbench destructor
 *
 * Cleans up allocated component instances.
 */
testbench::~testbench() {
  delete dut;
  delete test_port0;
  delete test_port1;
}

/**
 * @brief Bind all ports between model and test harness
 *
 * Performs comprehensive port binding following SystemC TLM-2.0 best practices:
 * - Dual TLM target sockets (model) ← TLM initiator sockets (test)
 * - sc_out/sc_in signal connections for interrupts, clock, reset
 */
void testbench::bind_ports() {
  REG_INFO(2, logger) << "Binding ports...";

  // =========================================================================
  // 1. TLM Target Socket Binding (Dual AXI4-Lite Slave Ports)
  // =========================================================================
  // Test's initiator socket → Model's target socket for port 0
  test_port0->initiator_socket.bind(dut->socket0);
  REG_INFO(2, logger) << "  [BOUND] test_port0 initiator_socket → socket0";

  // Test's initiator socket → Model's target socket for port 1
  test_port1->initiator_socket.bind(dut->socket1);
  REG_INFO(2, logger) << "  [BOUND] test_port1 initiator_socket → socket1";

  // =========================================================================
  // 2. Interrupt Signal Binding
  // =========================================================================
  // Model interrupt outputs → Testbench signals (for monitoring)
  dut->irq_o[0].bind(irq_port0_sig);
  REG_INFO(2, logger) << "  [BOUND] mailbox_dut irq_o[0] → irq_port0_sig";

  dut->irq_o[1].bind(irq_port1_sig);
  REG_INFO(2, logger) << "  [BOUND] mailbox_dut irq_o[1] → irq_port1_sig";

  // =========================================================================
  // 3. Clock and Reset Signal Binding
  // =========================================================================
  // Testbench signals → Model inputs (abstract frequency and async reset)
  dut->clk_i.bind(clk_sig);
  REG_INFO(2, logger) << "  [BOUND] clk_sig → mailbox_dut clk_i (100 MHz)";

  dut->rst_ni.bind(rst_ni_sig);
  REG_INFO(2, logger)
      << "  [BOUND] rst_ni_sig → mailbox_dut rst_ni (active-low)";

  // =========================================================================
  // Port Binding Complete
  // =========================================================================
  REG_INFO(2, logger) << "- Port binding complete";
}

/**
 * @brief Initialize testbench environment
 */
void testbench::initialize() {
  REG_INFO(2, logger) << "Initializing testbench environment";
  m_tests_run = 0;
  m_tests_passed = 0;
  m_tests_failed = 0;
  m_failed_tests.clear();
}

/**
 * @brief Apply reset to DUT
 *
 * Asserts active-low reset (rst_ni=0), waits for reset duration,
 * then deasserts (rst_ni=1).
 */
void testbench::apply_reset() {
  REG_INFO(2, logger) << "Applying reset to DUT";

  // Assert reset (active-low)
  rst_ni_sig.write(false);
  wait(10, SC_NS);

  // Deassert reset
  rst_ni_sig.write(true);
  wait(5, SC_NS);

  REG_INFO(2, logger) << "Reset complete";
}

/**
 * @brief Read register with TLM response status return
 * @param port Port number (0 or 1)
 * @param offset Register address offset
 * @param read_value Reference to store read data
 * @return TLM response status (TLM_OK_RESPONSE or TLM_GENERIC_ERROR_RESPONSE)
 */
tlm::tlm_response_status testbench::mailbox_read(unsigned int port,
                                                 unsigned int offset,
                                                 uint64_t &read_value) {
  if (port > 1) {
    SC_REPORT_ERROR("testbench::mailbox_read",
                    "Invalid port number (must be 0 or 1)");
    return tlm::TLM_GENERIC_ERROR_RESPONSE;
  }

  mailbox_test *test_port = (port == 0) ? test_port0 : test_port1;

  // Create TLM transaction
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  // Setup transaction parameters
  trans.set_command(tlm::TLM_READ_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&read_value));
  trans.set_data_length(8); // 64-bit register
  trans.set_streaming_width(8);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  // Execute blocking transport
  test_port->initiator_socket->b_transport(trans, delay);

  // Return response status
  return trans.get_response_status();
}

/**
 * @brief Write register with TLM response status return
 * @param port Port number (0 or 1)
 * @param offset Register address offset
 * @param write_value Data to write
 * @return TLM response status (TLM_OK_RESPONSE or TLM_GENERIC_ERROR_RESPONSE)
 */
tlm::tlm_response_status testbench::mailbox_write(unsigned int port,
                                                  unsigned int offset,
                                                  uint64_t write_value) {
  if (port > 1) {
    SC_REPORT_ERROR("testbench::mailbox_write",
                    "Invalid port number (must be 0 or 1)");
    return tlm::TLM_GENERIC_ERROR_RESPONSE;
  }

  mailbox_test *test_port = (port == 0) ? test_port0 : test_port1;

  // Create TLM transaction
  tlm::tlm_generic_payload trans;
  sc_time delay = SC_ZERO_TIME;

  // Setup transaction parameters
  trans.set_command(tlm::TLM_WRITE_COMMAND);
  trans.set_address(offset);
  trans.set_data_ptr(reinterpret_cast<unsigned char *>(&write_value));
  trans.set_data_length(8); // 64-bit register
  trans.set_streaming_width(8);
  trans.set_byte_enable_ptr(0);
  trans.set_dmi_allowed(false);
  trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

  // Execute blocking transport
  test_port->initiator_socket->b_transport(trans, delay);

  // Return response status
  return trans.get_response_status();
}

/**
 * @brief Main test execution entry point
 *
 * Executes all register access validation tests.
 */
void testbench::run_tests() {
  REG_INFO(2, logger) << "\n"
                       << "========================================\n"
                       << "   MAILBOX IP TEST SUITE START\n"
                       << "========================================\n";

  // Apply initial reset
  REG_INFO(2, logger) << "Applying initial reset";
  apply_reset();
  wait(50, SC_NS);

  // Run basic test cases
  test_register_reset_values();
  test_read_only_register_protection();
  test_write_only_register_protection();
  test_read_write_register_access();
  test_asynchronous_reset_behavior();

  // Run FUNC-001 comprehensive tests
  run_func001_tests();

  // Run FUNC-002 callback-level tests
  run_func002_comprehensive_tests();

  // Run FUNC-003 comprehensive tests
  run_func003_tests();

  // Run FUNC-004 comprehensive tests
  run_func004_tests();

  // Run FUNC-005 comprehensive tests
  run_func005_tests();

  // Run FUNC-006 comprehensive tests
  run_func006_tests();

  // Run FUNC-007 comprehensive tests
  run_func007_tests();

  // Print final summary
  report_test_summary();

  REG_INFO(2, logger) << "\n"
                       << "========================================\n"
                       << "    MAILBOX IP TEST SUITE END\n"
                       << "========================================\n";

  // Stop simulation
  sc_stop();
}

// =============================================================================
// Test Result Reporting Functions
// =============================================================================

void testbench::report_test_result(const char *test_name, bool passed) {
  m_tests_run++;

  if (passed) {
    m_tests_passed++;
    REG_INFO(1, logger) << "\n========================================\n"
                         << "[TEST PASSED] " << test_name << "\n"
                         << "========================================\n";
  } else {
    m_tests_failed++;
    m_failed_tests.push_back(test_name);
    REG_ERROR(0, logger) << "\n========================================\n"
                          << "[TEST FAILED] " << test_name << "\n"
                          << "========================================\n";
  }
}

void testbench::report_test_summary() {
  REG_INFO(2, logger)
      << "==========================================================";
  REG_INFO(2, logger) << "Test Execution Summary";
  REG_INFO(2, logger)
      << "==========================================================";
  REG_INFO(2, logger) << "Total tests run: " << m_tests_run;
  REG_INFO(2, logger) << "Tests passed:    " << m_tests_passed;
  REG_INFO(2, logger) << "Tests failed:    " << m_tests_failed;

  if (m_tests_failed > 0) {
    REG_ERROR(0, logger) << "Failed tests:";
    for (const auto &test : m_failed_tests) {
      REG_ERROR(0, logger) << "  - " << test;
    }
  }

  double pass_rate =
      (m_tests_run > 0) ? (100.0 * m_tests_passed / m_tests_run) : 0.0;
  REG_INFO(2, logger) << "Pass rate: " << std::fixed << std::setprecision(1)
                       << pass_rate << "%";
  REG_INFO(2, logger)
      << "==========================================================";

  if (m_tests_failed == 0) {
    REG_INFO(2, logger) << "ALL TESTS PASSED";
  } else {
    REG_ERROR(0, logger) << "SOME TESTS FAILED";
  }
  REG_INFO(2, logger)
      << "==========================================================";
}

// =============================================================================
// Test Case Implementations
// =============================================================================

/**
 * @brief Test register reset values for all 10 registers
 */
void testbench::test_register_reset_values() {
  REG_INFO(1, logger) << "\n>>> Test: Register Reset Values <<<";

  uint64_t read_val;
  bool all_pass = true;

  // Apply reset
  apply_reset();

  // Test all 10 registers
  struct {
    unsigned int offset;
    uint64_t expected_reset;
    const char *name;
  } registers[] = {{mailbox_basetest::WRITE_DATA_OFFSET, 0x0, "WRITE_DATA"},
                   {mailbox_basetest::READ_DATA_OFFSET, 0x0, "READ_DATA"},
                   {mailbox_basetest::STATUS_OFFSET, 0x1, "STATUS"},
                   {mailbox_basetest::ERROR_FLAGS_OFFSET, 0x0, "ERROR_FLAGS"},
                   {mailbox_basetest::WIRQT_OFFSET, 0x0, "WIRQT"},
                   {mailbox_basetest::RIRQT_OFFSET, 0x0, "RIRQT"},
                   {mailbox_basetest::IRQS_OFFSET, 0x0, "IRQS"},
                   {mailbox_basetest::IRQEN_OFFSET, 0x0, "IRQEN"},
                   {mailbox_basetest::IRQP_OFFSET, 0x0, "IRQP"},
                   {mailbox_basetest::CTRL_OFFSET, 0x0, "CTRL"}};

  for (const auto &reg : registers) {
    // Skip write-only registers (cannot read)
    if (reg.offset == mailbox_basetest::WRITE_DATA_OFFSET ||
        reg.offset == mailbox_basetest::READ_DATA_OFFSET) {
      REG_INFO(2, logger) << "  " << reg.name << ": SKIPPED (write-only)";
      continue;
    }

    test_port0->register_read_64(reg.offset, read_val);

    if (read_val == reg.expected_reset) {
      REG_INFO(2, logger) << "  " << reg.name << ": 0x" << std::hex << read_val
                           << " (expected 0x" << reg.expected_reset
                           << ") - PASS" << std::dec;
    } else {
      REG_ERROR(0, logger)
          << "  " << reg.name << ": 0x" << std::hex << read_val
          << " (expected 0x" << reg.expected_reset << ") - FAIL" << std::dec;
      all_pass = false;
    }
  }

  report_test_result("TC001: Register Reset Values", all_pass);
}

/**
 * @brief Test read-only register write protection
 */
void testbench::test_read_only_register_protection() {
  REG_INFO(1, logger) << "\n>>> Test: Read-Only Register Protection <<<";

  uint64_t read_before, read_after;
  bool all_pass = true;

  // Test READ_DATA, STATUS, ERROR_FLAGS, IRQP (all RO)
  unsigned int ro_registers[] = {
      mailbox_basetest::STATUS_OFFSET,
      mailbox_basetest::ERROR_FLAGS_OFFSET,
      mailbox_basetest::IRQP_OFFSET,
      mailbox_basetest::READ_DATA_OFFSET};

  const char *ro_names[] = {"STATUS", "ERROR_FLAGS", "IRQP", "READ_DATA"};

  for (size_t i = 0; i < sizeof(ro_registers) / sizeof(ro_registers[0]); i++) {
    // Read original value
    test_port0->register_read_64(ro_registers[i], read_before);

    // Attempt write (should be rejected)
    test_port0->register_write_64(ro_registers[i], 0xDEADBEEFCAFEBABE);

    // Read again
    test_port0->register_read_64(ro_registers[i], read_after);

    if (read_before == read_after) {
      REG_INFO(2, logger) << "  " << ro_names[i]
                           << ": Write rejected (value unchanged) - PASS";
    } else {
      REG_ERROR(0, logger)
          << "  " << ro_names[i] << ": Write accepted (value changed) - FAIL\n read_before: " << read_before << " read_after: " << read_after;
      all_pass = false;
    }
  }

  report_test_result("TC002: Read-Only Register Protection", all_pass);
}

/**
 * @brief Test write-only register read protection
 *
 * Verification strategy:
 * - Read protection: pre-load the read variable with a sentinel value, attempt
 *   a read, and confirm the sentinel is unchanged. Because the regmodel
 *   handle_read_restriction_error callback returns false, regmodel::Memory skips
 *   the buffer-copy step, leaving the caller's variable untouched.
 * - Write acceptance: confirm that writes ARE processed via observable
 *   side-effects (STATUS register reflects FIFO/flush state changes).
 */
void testbench::test_write_only_register_protection() {
  REG_INFO(1, logger) << "\n>>> Test: Write-Only Register Protection <<<";

  const uint64_t sentinel = 0xDEADBEEFCAFEBABE;
  uint64_t read_val;
  uint64_t status_val;
  bool all_pass = true;

  // -------------------------------------------------------------------------
  // WRITE_DATA (WO)
  // -------------------------------------------------------------------------
  // 1. Read behaviour: this is the one write-only register the RTL lets you
  //    read. axi_lite_mailbox.sv answers OKAY and returns a fixed sentinel
  //    (MBOXW: r_chan = '{data: 32'hFEEDC0DE, resp: RESP_OKAY}).
  read_val = sentinel;
  {
    tlm::tlm_response_status st =
        mailbox_read(0, mailbox_basetest::WRITE_DATA_OFFSET, read_val);
    if (st == tlm::TLM_OK_RESPONSE && read_val == 0xFEEDC0DEULL) {
      REG_INFO(2, logger) << "  WRITE_DATA: Read returns 0xFEEDC0DE with OKAY - PASS";
    } else {
      REG_ERROR(0, logger) << "  WRITE_DATA: expected 0xFEEDC0DE/OK, got 0x"
                            << std::hex << read_val << std::dec
                            << " status=" << st << " - FAIL";
      all_pass = false;
    }
  }

  // 2. Write acceptance: write a word and verify STATUS[0] (empty flag)
  //    transitions from 1 (empty) to 0 (data available on peer port).
  mailbox_read(0, mailbox_basetest::STATUS_OFFSET, status_val);
  bool was_empty = (status_val & 0x1) != 0;  // STATUS[0] = empty

  mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xCAFEBABEDEAD1234ULL);

  wait(1, SC_NS);
  // Read STATUS from port 1 — it sees the data written by port 0.
  mailbox_read(1, mailbox_basetest::STATUS_OFFSET, status_val);
  bool peer_sees_data = (status_val & 0x1) == 0;  // empty flag cleared on peer

  if (was_empty && peer_sees_data) {
    REG_INFO(2, logger) << "  WRITE_DATA: Write accepted (peer STATUS empty→not-empty) - PASS";
  } else {
    REG_ERROR(0, logger) << "  WRITE_DATA: Write not reflected in peer STATUS - FAIL" << was_empty << " " << peer_sees_data;
    all_pass = false;
  }

  // -------------------------------------------------------------------------
  // CTRL (WO)
  // -------------------------------------------------------------------------
  // 1. Read protection: CTRL is write-only with no read path, so the RTL
  //    decodes the access as an error and returns zero data with SLVERR.
  read_val = sentinel;
  {
    tlm::tlm_response_status st =
        mailbox_read(0, mailbox_basetest::CTRL_OFFSET, read_val);
    if (st != tlm::TLM_OK_RESPONSE && read_val == 0) {
      REG_INFO(2, logger) << "  CTRL: Read rejected with error response - PASS";
    } else {
      REG_ERROR(0, logger) << "  CTRL: expected 0x0 with error, got 0x"
                            << std::hex << read_val << std::dec
                            << " status=" << st << " - FAIL";
      all_pass = false;
    }
  }

  // 2. Write acceptance: issue wflush (bit[0]=1) on port 0 to drain the word
  //    written above. wflush drains write_fifo_for(port=0) = fifo_0_to_1,
  //    which is port 1's inbound FIFO. After flush, port 1 STATUS[0] (empty)
  //    must return to 1.
  mailbox_write(0, mailbox_basetest::CTRL_OFFSET, 0x1);  // wflush = drain port 0 outbound (= port 1 inbound)

  mailbox_read(1, mailbox_basetest::STATUS_OFFSET, status_val);
  bool peer_empty_after_flush = (status_val & 0x1) != 0;  // empty flag set again

  if (peer_empty_after_flush) {
    REG_INFO(2, logger) << "  CTRL: Write accepted (wflush drained FIFO, peer STATUS not-empty→empty) - PASS";
  } else {
    REG_ERROR(0, logger) << "  CTRL: Write not reflected in STATUS after flush - FAIL";
    all_pass = false;
  }

  report_test_result("TC003: Write-Only Register Protection", all_pass);
}

/**
 * @brief Test read-write register access
 */
void testbench::test_read_write_register_access() {
  REG_INFO(1, logger) << "\n>>> Test: Read-Write Register Access <<<";

  uint64_t read_val;
  bool all_pass = true;

  // -------------------------------------------------------------------------
  // WIRQT (RW, 8-bit threshold with saturation)
  // Must use a value < mailbox depth (default = 8). Values >= depth saturate
  // to (depth - 1), so the readback would not match the written value.
  // -------------------------------------------------------------------------
  {
    const uint64_t wirqt_val = 0x05;  // < 8, no saturation
    test_port0->register_write_64(mailbox_basetest::WIRQT_OFFSET, wirqt_val);
    test_port0->register_read_64(mailbox_basetest::WIRQT_OFFSET, read_val);
    if (read_val == wirqt_val) {
      REG_INFO(2, logger) << "  WIRQT: Write/Read successful (0x"
                           << std::hex << read_val << std::dec << ") - PASS";
    } else {
      REG_ERROR(0, logger) << "  WIRQT: Write/Read mismatch - FAIL"
                            << " written=0x" << std::hex << wirqt_val
                            << " readback=0x" << read_val << std::dec;
      all_pass = false;
    }
  }

  // -------------------------------------------------------------------------
  // RIRQT (RW, 8-bit threshold with saturation) — same saturation constraint
  // -------------------------------------------------------------------------
  {
    const uint64_t rirqt_val = 0x03;  // < 8, no saturation
    test_port0->register_write_64(mailbox_basetest::RIRQT_OFFSET, rirqt_val);
    test_port0->register_read_64(mailbox_basetest::RIRQT_OFFSET, read_val);
    if (read_val == rirqt_val) {
      REG_INFO(2, logger) << "  RIRQT: Write/Read successful (0x"
                           << std::hex << read_val << std::dec << ") - PASS";
    } else {
      REG_ERROR(0, logger) << "  RIRQT: Write/Read mismatch - FAIL"
                            << " written=0x" << std::hex << rirqt_val
                            << " readback=0x" << read_val << std::dec;
      all_pass = false;
    }
  }

  // -------------------------------------------------------------------------
  // IRQS (RW, write-1-to-clear)
  // Simple write→readback does NOT work: writing 1 CLEARS the bit, so IRQS
  // always reads back 0 after a write of 0x7 if no interrupts are pending.
  // Two-phase test:
  //   Phase 1 — Set:   write WRITE_DATA from port 0 with WIRQT=0 so the fill
  //             level (1) > threshold (0), which sets IRQS[0] (wtirq sticky).
  //   Phase 2 — Clear: write 0x7 to IRQS (W1C) and verify it reads 0.
  // -------------------------------------------------------------------------
  {
    // Phase 1: set WIRQT=0, write one word to trigger WTIRQ
    test_port0->register_write_64(mailbox_basetest::WIRQT_OFFSET, 0x0);
    mailbox_write(0, mailbox_basetest::WRITE_DATA_OFFSET, 0xABCD1234ABCD1234ULL);

    test_port0->register_read_64(mailbox_basetest::IRQS_OFFSET, read_val);
    bool wtirq_set = (read_val & 0x1) != 0;
    if (wtirq_set) {
      REG_INFO(2, logger) << "  IRQS: WTIRQ set after WRITE_DATA (0x"
                           << std::hex << read_val << std::dec << ") - OK";
    } else {
      REG_ERROR(0, logger) << "  IRQS: WTIRQ not set after WRITE_DATA - FAIL";
      all_pass = false;
    }

    // Phase 2: W1C — clear all IRQS bits and verify
    test_port0->register_write_64(mailbox_basetest::IRQS_OFFSET, 0x7);
    test_port0->register_read_64(mailbox_basetest::IRQS_OFFSET, read_val);
    if (read_val == 0x0) {
      REG_INFO(2, logger) << "  IRQS: W1C clear successful - PASS";
    } else {
      REG_ERROR(0, logger) << "  IRQS: W1C clear failed (readback=0x"
                            << std::hex << read_val << std::dec << ") - FAIL";
      all_pass = false;
    }

    // Cleanup: flush the FIFO word written above so later tests start clean.
    // wait() is required: sc_fifo's nb_write() increments m_num_written (pending)
    // but update() only commits it to m_num_readable at the next delta cycle.
    // handle_write_CTRL's drain loop uses num_available() = m_num_readable, so
    // without a wait the word appears invisible and survives the wflush.
    wait(1, SC_NS);
    mailbox_write(0, mailbox_basetest::CTRL_OFFSET, 0x1);  // wflush port 0
  }

  // -------------------------------------------------------------------------
  // IRQEN (RW, standard read-write register)
  // -------------------------------------------------------------------------
  {
    const uint64_t irqen_val = 0x5;  // bits [2:0]
    test_port0->register_write_64(mailbox_basetest::IRQEN_OFFSET, irqen_val);
    test_port0->register_read_64(mailbox_basetest::IRQEN_OFFSET, read_val);
    if (read_val == irqen_val) {
      REG_INFO(2, logger) << "  IRQEN: Write/Read successful (0x"
                           << std::hex << read_val << std::dec << ") - PASS";
    } else {
      REG_ERROR(0, logger) << "  IRQEN: Write/Read mismatch - FAIL"
                            << " written=0x" << std::hex << irqen_val
                            << " readback=0x" << read_val << std::dec;
      all_pass = false;
    }
  }

  report_test_result("TC004: Read-Write Register Access", all_pass);
}


/**
 * @brief Test asynchronous reset behavior
 */
void testbench::test_asynchronous_reset_behavior() {
  REG_INFO(1, logger) << "\n>>> Test: Asynchronous Reset Behavior <<<";

  // Write non-zero values to RW registers
  test_port0->register_write_64(mailbox_basetest::WIRQT_OFFSET, 0xFF);
  test_port0->register_write_64(mailbox_basetest::RIRQT_OFFSET, 0xAA);
  test_port0->register_write_64(mailbox_basetest::IRQEN_OFFSET, 0x7);

  REG_INFO(2, logger) << "  Written non-zero values to registers";

  // Apply reset
  apply_reset();

  // Verify registers return to reset values
  uint64_t read_val;
  bool all_pass = true;

  test_port0->register_read_64(mailbox_basetest::WIRQT_OFFSET, read_val);
  bool wirqt_reset = (read_val == 0x0);

  test_port0->register_read_64(mailbox_basetest::RIRQT_OFFSET, read_val);
  bool rirqt_reset = (read_val == 0x0);

  test_port0->register_read_64(mailbox_basetest::IRQEN_OFFSET, read_val);
  bool irqen_reset = (read_val == 0x0);

  if (wirqt_reset && rirqt_reset && irqen_reset) {
    REG_INFO(2, logger) << "  All registers returned to reset values - PASS";
  } else {
    REG_ERROR(0, logger) << "  Some registers did not reset properly - FAIL";
    all_pass = false;
  }

  report_test_result("TC005: Asynchronous Reset Behavior", all_pass);
}

void testbench::run_func001_tests() {
  REG_INFO(2, logger) << "\n========================================\n"
                       << "FUNC-001: System Reset and Initialization Behavior\n"
                       << "========================================\n";

  wait(10, SC_NS);

  // Apply reset before running test suite
  apply_reset();
  wait(20, SC_NS);

  // Execute FUNC_001 specific test cases (Test IDs 1, 61, 62)
  test_reset_fifo_interrupt_state();    // TC006

  REG_INFO(2, logger) << "\n========================================\n"
                       << "FUNC-001 Test Suite Complete\n"
                       << "========================================\n";
}

/**
 * @brief Main function - SystemC entry point
 */
int sc_main(int argc, char *argv[]) {
  // Suppress IEEE 1666 deprecation warnings
  sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);

  // Initialize CCI broker and optionally load INI config file.
  regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);

  testbench tb("mailbox_testbench");
  sc_start();

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(tb.m_tests_failed > 0 ? 1 : 0);
  return 0;
}
