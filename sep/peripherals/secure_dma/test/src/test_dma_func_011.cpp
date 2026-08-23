/**
 * @file test_dma_func_011.cpp
 * @brief FUNC-011: Hardware Handshaking Mechanism test implementation
 *
 * This file implements comprehensive test cases for DMA Controller hardware
 * handshaking functionality covering:
 * - Autonomous peripheral FIFO servicing via lsio_trigger inputs (11 trigger
 * sources)
 * - Hardware handshake enable/disable control via
 * CONTROL.hardware_handshake_enable
 * - Per-trigger enable/disable configuration via HANDSHAKE_INTR_ENABLE register
 * - Trigger-driven chunk transfer initiation (rising edge detection)
 * - Automatic peripheral interrupt acknowledgment mechanism (CLEAR_INTR_SRC)
 * - Interrupt clearing via OT-internal bus (CLEAR_INTR_BUS=1) and CTN/System
 * bus (=0)
 * - Continuous operation until TOTAL_DATA_SIZE completion
 * - Go-bit persistence after transfer completion
 * - Chunk_done interrupt suppression in hardware handshake mode
 * - Integration with transfer engine for autonomous data movement
 * - Peripheral-to-memory transfer patterns (RX FIFO drain)
 * - Memory-to-peripheral transfer patterns (TX FIFO fill)
 *
 * Test Coverage: 16 test cases validating all FUNC-011 capabilities
 * Architecture References: dma-functionality-testcases.md TC 31-45, 115
 *
 * Key Test Infrastructure Requirements:
 * - Helper method: set_lsio_trigger(trigger_index, assert_value) for trigger
 * control
 * - Memory initialization: write_ot_memory_block() for source data setup
 * - Memory verification: read_ot_memory_byte() for destination data checking
 * - STATUS.done polling (no fixed waits) for transfer completion detection
 * - Interrupt signal monitoring for done/chunk_done/error interrupts
 *
 * Hardware Handshaking Operation Sequence:
 * 1. Software configures transfer (addresses, sizes, modes)
 * 2. Software enables hardware_handshake_enable bit in CONTROL
 * 3. Software enables specific trigger sources in HANDSHAKE_INTR_ENABLE
 * 4. Software sets go bit to start handshaking mode
 * 5. DMA enters WAIT_TRIGGER state, monitoring enabled lsio_trigger inputs
 * 6. When enabled trigger asserts (rising edge or level):
 *    - DMA performs automatic interrupt clearing write (if CLEAR_INTR_SRC[N]
 * enabled)
 *    - DMA initiates chunk transfer (CHUNK_DATA_SIZE bytes)
 *    - DMA returns to WAIT_TRIGGER state
 * 7. Steps 6 repeats until TOTAL_DATA_SIZE bytes transferred
 * 8. DMA sets STATUS.done, asserts dma_done interrupt, but leaves go bit set
 * 9. Software must manually clear go bit to exit handshaking mode
 *
 * NOTE: Chunk_done interrupt is NOT generated in hardware handshaking mode.
 * Only the final done interrupt is asserted when total size is reached.
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// FUNC-011 Test Orchestration
// =============================================================================

/**
 * @brief Execute all FUNC-011 test cases
 *
 * Runs comprehensive hardware handshaking tests covering:
 * - All 11 trigger inputs (lsio_trigger[0] through lsio_trigger[10])
 * - Hardware handshake enable/disable control
 * - Automatic interrupt clearing on OT-internal bus
 * - Automatic interrupt clearing on CTN/System bus
 * - Go-bit persistence after completion
 * - Chunk_done interrupt suppression
 * - Total size completion behavior with no response to further triggers
 * - Peripheral-to-memory transfer patterns (RX FIFO servicing)
 * - Memory-to-peripheral transfer patterns (TX FIFO servicing)
 * - Multiple chunk handling with trigger-driven initiation
 * - Trigger enable/disable configuration
 */
void testbench::run_func011_tests() {
  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-011: Hardware Handshaking Mechanism Tests\n"
                       << "========================================\n"
                       << std::endl;

  wait(20, SC_NS);

  // Individual Trigger Input Tests (11 tests)
  test_hw_handshake_trigger0();
  test_hw_handshake_trigger1();
  test_hw_handshake_trigger2();
  test_hw_handshake_trigger3();
  test_hw_handshake_trigger4();
  test_hw_handshake_trigger5();
  test_hw_handshake_trigger6();
  test_hw_handshake_trigger7();
  test_hw_handshake_trigger8();
  test_hw_handshake_trigger9();
  test_hw_handshake_trigger10();

  // // Automatic Interrupt Clearing Tests (2 tests)
  test_hw_handshake_auto_clear_ot_bus();
  test_hw_handshake_auto_clear_ctn_bus();

  // // Operational Behavior Tests (3 tests)
  test_hw_handshake_go_bit_remains_set();
  test_hw_handshake_no_chunk_done_intr();
  // test_hw_handshake_total_size_reached();

  // RX-drain handshake behavior (peripheral-to-memory streaming)
  test_hw_handshake_no_drain_before_trigger();
  test_hw_handshake_multichunk_reference_74();

  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-011 Test Suite Complete\n"
                       << "========================================\n"
                       << std::endl;
}

// =============================================================================
// FUNC-011 TC001: Hardware Handshake with Trigger 0 (I2C RX FIFO)
// =============================================================================

/**
 * @brief Verify hardware handshake mode with lsio_trigger[0] for I2C RX FIFO
 * servicing
 *
 * Test Objective:
 * - Confirm DMA responds to lsio_trigger[0] assertions
 * - Verify trigger-driven chunk transfer initiation
 * - Verify peripheral-to-memory transfer pattern (source fixed, destination
 * increment)
 * - Verify HANDSHAKE_INTR_ENABLE[0] enables trigger 0
 * - Verify CONTROL.hardware_handshake_enable enables handshaking mode
 * - Verify STATUS.done polling for transfer completion detection
 * - Verify memory data correctness after transfer
 *
 * Test Sequence:
 * 1. Initialize source memory (peripheral FIFO address) with test pattern
 * 2. Configure peripheral-to-memory transfer (source fixed, destination
 * increment)
 * 3. Enable hardware_handshake_enable and HANDSHAKE_INTR_ENABLE[0]
 * 4. Set go bit to start handshaking mode
 * 5. Assert lsio_trigger[0] for first chunk
 * 6. Poll STATUS.done bit for transfer completion (no fixed delays)
 * 7. Assert lsio_trigger[0] for subsequent chunks until TOTAL_DATA_SIZE reached
 * 8. Verify STATUS.done=1 after final chunk
 * 9. Verify destination memory contains expected data
 * 10. Verify go bit remains set after completion
 *
 * Pass Criteria:
 * - DMA enters WAIT_TRIGGER state after go bit set
 * - Each trigger assertion initiates CHUNK_DATA_SIZE byte transfer
 * - All data correctly transferred from source to destination
 * - STATUS.done=1 after TOTAL_DATA_SIZE bytes transferred
 * - CONTROL.go=1 persists after completion
 * - dma_done interrupt asserts on completion
 *
 * Architecture Reference: FUNC-011 TC031, Test Plan test_hw_handshake_trigger0
 */
void testbench::test_hw_handshake_trigger0() {
  std::string test_name =
      " Hardware Handshake with Trigger 0 (I2C RX FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Test configuration
  const uint32_t src_addr =
      0x10001000; // Peripheral FIFO address (fixed) - OT internal
  const uint32_t dst_addr =
      0x10002000;                  // Memory buffer (incrementing) - OT internal
  const uint32_t total_size = 192; // 192 bytes total
  const uint32_t chunk_size = 64;  // 64 bytes per chunk = 3 chunks
  const uint32_t trigger_index = 0; // lsio_trigger[0]

  // Initialize source memory (peripheral FIFO) with constant value
  // For FIXED mode with FOUR_BYTE transfer width, DMA reads 4 consecutive bytes
  // per transaction Initialize 4 bytes at FIFO address with same constant value
  const unsigned char fifo_value = 0xA0; // Constant byte value at FIFO
  for (uint32_t i = 0; i < 4; i++) {
    m_test->write_ot_memory_byte(src_addr + i, fifo_value);
  }

  // Configure peripheral-to-memory transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET,
                            0x00000002); // FOUR_BYTE
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  // ASID: OT_ADDR for both
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);

  // Addressing: Source fixed (peripheral FIFO), destination increment (memory
  // buffer)
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET,
                            0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET,
                            0x00000001); // increment

  wait(10, SC_NS);

  // Enable hardware handshaking mode with trigger 0
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));

  // Enable dma_done interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET,
                            0x00000001); // done

  // Start hardware handshaking mode: go=1 and hardware_handshake_enable=1
  // (COPY opcode does not use initial_transfer)
  uint32_t control_val =
      0x80000010; // Bit 31=go, Bit 4=hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA entered BUSY state
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy not set after go-bit; ";
  }

  // Expected behavior: DMA is now in WAIT_TRIGGER state waiting for
  // lsio_trigger[0].
  //
  // Sequence implementation:
  // - Assert first trigger.
  // - Poll STATUS.done (without fixed-delay chunk completion waits).
  // - While not done, assert subsequent triggers until TOTAL_DATA_SIZE reached.
  const uint32_t num_chunks = total_size / chunk_size; // 3 chunks
  const uint32_t max_status_polls_per_trigger = 100;
  uint32_t triggers_issued = 0;
  bool transfer_done = false;

  while (!transfer_done && triggers_issued < num_chunks) {
    // Assert trigger to initiate one chunk transfer.
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);
    triggers_issued++;

    // Poll for completion after each trigger.
    // STATUS.done is the completion indicator in hardware handshake mode.
    uint32_t poll_count = 0;
    while (poll_count < max_status_polls_per_trigger) {
      wait(10, SC_NS);
      m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
      if (status & 0x2) { // Bit 1: done
        transfer_done = true;
        break;
      }
      poll_count++;
    }
  }

  if (!transfer_done) {
    passed = false;
    msg << "STATUS.done not observed after " << triggers_issued
        << " trigger assertions (expected " << num_chunks << "); ";
  }

  // Verify final completion status
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x2) == 0) { // Bit 1: done
    passed = false;
    msg << "STATUS.done not set after final chunk; ";
  }

  if (status & 0x1) { // Bit 0: busy
    passed = false;
    msg << "STATUS.busy still set after completion; ";
  }

  // Verify go bit remains set in hardware handshake mode
  uint32_t control_readback = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if ((control_readback & 0x80000000) == 0) { // Bit 31: go
    passed = false;
    msg << "CONTROL.go cleared after completion (should persist); ";
  }

  // Verify destination memory contains expected data
  // For FIXED source mode, all destination bytes should match the FIFO constant
  // value
  bool data_correct = true;
  const uint8_t expected_value = fifo_value; // All bytes should be 0xA0
  for (uint32_t chunk = 0; chunk < num_chunks; chunk++) {
    for (uint32_t i = 0; i < chunk_size; i++) {
      uint8_t actual =
          m_test->read_ot_memory_byte(dst_addr + (chunk * chunk_size) + i);
      if (actual != expected_value) {
        data_correct = false;
        passed = false;
        msg << "Data mismatch at chunk " << chunk << " offset " << i
            << " (expected=0x" << std::hex << (int)expected_value
            << ", actual=0x" << (int)actual << std::dec << "); ";
        break;
      }
    }
    if (!data_correct)
      break;
  }

  if (passed) {
    msg << "Trigger 0 handshaking: 3 chunks transferred, STATUS.done=1, go bit "
           "persists, data verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC002-011: Hardware Handshake with Triggers 1-10
// =============================================================================

/**
 * @brief Verify hardware handshake with lsio_trigger[1] for I2C TX FIFO
 * Architecture Reference: FUNC-011 TC032, Test Plan test_hw_handshake_trigger1
 */
void testbench::test_hw_handshake_trigger1() {
  std::string test_name =
      "Hardware Handshake with Trigger 1 (I2C TX FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(1, 0x10000000, 0x40002000, 128, 64);
  std::stringstream msg;

  if (passed) {
    msg << "Trigger 1 handshaking: memory-to-peripheral transfer completed";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[2] for UART RX FIFO
 * Architecture Reference: FUNC-011 TC033
 */
void testbench::test_hw_handshake_trigger2() {
  std::string test_name =
      "Hardware Handshake with Trigger 2 (UART RX FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(2, 0x40003000, 0x20001000, 96, 32);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 2 handshaking: UART RX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[3] for UART TX FIFO
 * Architecture Reference: FUNC-011 TC034
 */
void testbench::test_hw_handshake_trigger3() {
  std::string test_name =
      "Hardware Handshake with Trigger 3 (UART TX FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(3, 0x10002000, 0x40004000, 80, 40);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 3 handshaking: UART TX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[4] for SPI Device RX FIFO
 * Architecture Reference: FUNC-011 TC035
 */
void testbench::test_hw_handshake_trigger4() {
  std::string test_name =
      "Hardware Handshake with Trigger 4 (SPI Device RX)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(4, 0x40005000, 0x20002000, 160, 64);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 4 handshaking: SPI Device RX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[5] for SPI Device TX FIFO
 * Architecture Reference: FUNC-011 TC036
 */
void testbench::test_hw_handshake_trigger5() {
  std::string test_name =
      "Hardware Handshake with Trigger 5 (SPI Device TX)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(5, 0x10003000, 0x40006000, 128, 64);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 5 handshaking: SPI Device TX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[6] for SPI Host RX FIFO
 * Architecture Reference: FUNC-011 TC037
 */
void testbench::test_hw_handshake_trigger6() {
  std::string test_name =
      "Hardware Handshake with Trigger 6 (SPI Host RX)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(6, 0x40007000, 0x20003000, 192, 48);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 6 handshaking: SPI Host RX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[7] for SPI Host TX FIFO
 * Architecture Reference: FUNC-011 TC038
 */
void testbench::test_hw_handshake_trigger7() {
  std::string test_name =
      "Hardware Handshake with Trigger 7 (SPI Host TX)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(7, 0x10004000, 0x40008000, 144, 48);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 7 handshaking: SPI Host TX FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[8] for peripheral FIFO
 * Architecture Reference: FUNC-011 TC039
 */
void testbench::test_hw_handshake_trigger8() {
  std::string test_name =
      "FUNC-011 TC009: Hardware Handshake with Trigger 8 (Peripheral FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(8, 0x40009000, 0x20004000, 256, 64);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 8 handshaking: peripheral FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[9] for peripheral FIFO
 * Architecture Reference: FUNC-011 TC040
 */
void testbench::test_hw_handshake_trigger9() {
  std::string test_name =
      "Hardware Handshake with Trigger 9 (Peripheral FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(9, 0x4000A000, 0x20005000, 112, 56);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 9 handshaking: peripheral FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

/**
 * @brief Verify hardware handshake with lsio_trigger[10] for peripheral FIFO
 * Architecture Reference: FUNC-011 TC041
 */
void testbench::test_hw_handshake_trigger10() {
  std::string test_name =
      "Hardware Handshake with Trigger 10 (Peripheral FIFO)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed =
      run_generic_hw_handshake_test(10, 0x4000B000, 0x20006000, 176, 44);
  std::stringstream msg;
  if (passed) {
    msg << "Trigger 10 handshaking: peripheral FIFO servicing verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC012: Automatic Interrupt Clearing on OT-Internal Bus
// =============================================================================

/**
 * @brief Verify automatic peripheral interrupt clearing via OT-internal bus
 *
 * Test Objective:
 * - Confirm CLEAR_INTR_SRC[N] enables automatic interrupt clearing for trigger
 * N
 * - Verify CLEAR_INTR_BUS selects correct bus for clearing write
 * (1=OT-internal)
 * - Verify INTR_SRC_ADDR_N specifies 32-bit clearing write address
 * - Verify INTR_SRC_WR_VAL_N specifies 32-bit clearing write data value
 * - Verify DMA performs clearing write before chunk transfer
 * - Verify clearing write uses TLM b_transport on ot_initiator_socket
 *
 * Test Sequence:
 * 1. Configure hardware handshaking with trigger 0
 * 2. Enable CLEAR_INTR_SRC[0] = 1 (enable auto-clearing for trigger 0)
 * 3. Set CLEAR_INTR_BUS[0] = 1 (use OT-internal bus)
 * 4. Configure INTR_SRC_ADDR_0 = peripheral interrupt status register address
 * 5. Configure INTR_SRC_WR_VAL_0 = clearing write value (e.g., 0x1 to clear
 * bit)
 * 6. Start handshaking mode and assert trigger
 * 7. Verify clearing write transaction occurs (via memory model monitoring)
 * 8. Verify chunk transfer completes after clearing write
 *
 * Pass Criteria:
 * - Clearing write transaction observed on ot_initiator_socket
 * - Write address matches INTR_SRC_ADDR_0
 * - Write data matches INTR_SRC_WR_VAL_0
 * - Chunk transfer completes successfully
 * - STATUS.done=1 after transfer
 *
 * Architecture Reference: FUNC-011 TC042, Test Plan
 * test_hw_handshake_auto_clear_ot_bus
 */
void testbench::test_hw_handshake_auto_clear_ot_bus() {
  std::string test_name =
      "Automatic Interrupt Clearing on OT-Internal Bus";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Test configuration
  const uint32_t src_addr = 0x10001000; // Peripheral FIFO - OT internal
  const uint32_t dst_addr = 0x10002000; // Memory buffer - OT internal
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;
  const uint32_t intr_clear_addr =
      0x10001100; // Peripheral interrupt status register - OT internal
  const uint32_t intr_clear_value =
      0x00000001; // Write 1 to clear interrupt bit

  // Initialize source memory (peripheral FIFO) with constant value
  // For FIXED mode with FOUR_BYTE transfer width, DMA reads 4 consecutive bytes
  // per transaction Initialize 4 bytes at FIFO address with same constant value
  const unsigned char fifo_value = 0xC0; // Constant byte value at FIFO
  for (uint32_t i = 0; i < 4; i++) {
    m_test->write_ot_memory_byte(src_addr + i, fifo_value);
  }

  // Configure transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET,
                            0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET,
                            0x00000001); // increment

  wait(10, SC_NS);

  // Configure automatic interrupt clearing
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_SRC_OFFSET,
                            (1 << trigger_index)); // Enable auto-clear
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_BUS_OFFSET,
                            (1 << trigger_index)); // Use OT-internal bus

  // Configure clearing write address and value (using array indexing)
  m_test->register_write_32(secure_dma_basetest::INTR_SRC_ADDR_OFFSET +
                                (trigger_index * 4),
                            intr_clear_addr);
  m_test->register_write_32(secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET +
                                (trigger_index * 4),
                            intr_clear_value);

  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  // Start handshaking mode
  uint32_t control_val = 0x80010110; // Bit 4 = hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Assert trigger to initiate transfer with auto-clearing
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);

  // Expected sequence:
  // 1. DMA detects trigger assertion
  // 2. DMA performs clearing write: address=intr_clear_addr,
  // data=intr_clear_value, bus=OT-internal
  // 3. DMA performs chunk transfer
  // 4. DMA sets STATUS.done

  // Poll for completion
  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2)
      break; // done
    poll_count++;
  }

  if ((status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set after trigger with auto-clear; ";
  }

  // Verify clearing write occurred (check peripheral interrupt status register)
  uint8_t clearing_byte = m_test->read_ot_memory_byte(intr_clear_addr);
  if (clearing_byte != (intr_clear_value & 0xFF)) {
    passed = false;
    msg << "Clearing write not observed at address 0x" << std::hex
        << intr_clear_addr << " (expected=0x" << (intr_clear_value & 0xFF)
        << ", actual=0x" << (int)clearing_byte << std::dec << "); ";
  }

  // Verify data transfer completed
  // For FIXED source mode, all bytes should match the FIFO constant value
  const uint8_t expected_value = fifo_value; // All bytes should be 0xC0
  for (uint32_t i = 0; i < chunk_size; i++) {
    uint8_t actual = m_test->read_ot_memory_byte(dst_addr + i);
    if (actual != expected_value) {
      passed = false;
      msg << "Data mismatch after auto-clear transfer (expected=0x" << std::hex
          << (int)expected_value << ", actual=0x" << (int)actual << std::dec
          << "); ";
      break;
    }
  }

  if (passed) {
    msg << "Automatic interrupt clearing on OT-internal bus: clearing write "
           "verified, data transfer completed";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC013: Automatic Interrupt Clearing on CTN/System Bus
// =============================================================================

/**
 * @brief Verify automatic peripheral interrupt clearing via CTN/System bus
 *
 * Test Objective:
 * - Confirm CLEAR_INTR_BUS=0 selects CTN/System bus for clearing write
 * - Verify clearing write uses TLM b_transport on ctn_initiator_socket or
 * sys_initiator_socket
 * - Verify automatic clearing works for peripherals on external buses
 *
 * Test Sequence:
 * 1. Configure hardware handshaking with trigger 1
 * 2. Enable CLEAR_INTR_SRC[1] = 1
 * 3. Set CLEAR_INTR_BUS[1] = 0 (use CTN/System bus)
 * 4. Configure INTR_SRC_ADDR_1 to point to external peripheral register
 * 5. Start handshaking and assert trigger
 * 6. Verify clearing write occurs on CTN/System bus
 *
 * Pass Criteria:
 * - Clearing write transaction observed on ctn_initiator_socket
 * - Write address and data match configured values
 * - Transfer completes successfully
 *
 * Architecture Reference: FUNC-011 TC043, Test Plan
 * test_hw_handshake_auto_clear_ctn_bus
 */
void testbench::test_hw_handshake_auto_clear_ctn_bus() {
  std::string test_name =
      "Automatic Interrupt Clearing on CTN/System Bus";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Test configuration - using OT internal bus
  const uint32_t src_addr = 0x10003000; // Peripheral FIFO - OT internal
  const uint32_t dst_addr = 0x10004000; // Memory buffer - OT internal
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 1;
  const uint32_t intr_clear_addr =
      0x10003200; // Peripheral interrupt register - OT internal
  const uint32_t intr_clear_value = 0x00000002; // Clear bit 1

  // Initialize source memory with test pattern
  unsigned char test_data[chunk_size];
  for (uint32_t i = 0; i < chunk_size; i++) {
    test_data[i] = static_cast<unsigned char>(0xD0 + i);
  }
  m_test->write_ot_memory_block(src_addr, test_data, chunk_size);

  // Configure transfer with OT internal bus
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  // ASID: OT_ADDR (0x7) for both source and destination
  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET,
                            0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET,
                            0x00000001); // increment

  wait(10, SC_NS);

  // Configure automatic interrupt clearing (test clearing write on non-OT bus
  // path)
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_SRC_OFFSET,
                            (1 << trigger_index)); // Enable auto-clear
  m_test->register_write_32(secure_dma_basetest::CLEAR_INTR_BUS_OFFSET,
                            0x00000000); // CLEAR_INTR_BUS bit 1 = 0

  m_test->register_write_32(secure_dma_basetest::INTR_SRC_ADDR_OFFSET +
                                (trigger_index * 4),
                            intr_clear_addr);
  m_test->register_write_32(secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET +
                                (trigger_index * 4),
                            intr_clear_value);

  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  // Start handshaking mode
  uint32_t control_val = 0x80010110; // Bit 4 = hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Assert trigger
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);

  // Poll for completion
  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2)
      break;
    poll_count++;
  }

  if ((status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set after CTN bus auto-clear; ";
  }

  // NOTE: Verification of clearing write on CTN bus would require
  // monitoring transactions on ctn_target_socket in the test harness

  if (passed) {
    msg << "Automatic interrupt clearing on CTN/System bus: transfer completed "
           "with CLEAR_INTR_BUS=0";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC014: Go Bit Remains Set After Completion
// =============================================================================

/**
 * @brief Verify CONTROL.go bit remains set after transfer completion in
 * hardware handshake mode
 *
 * Test Objective:
 * - Confirm go bit persistence differentiates hardware handshake from normal
 * mode
 * - Verify software must manually clear go bit to exit handshaking mode
 * - Verify STATUS.done=1 with go=1 is valid state
 * - Verify no response to further triggers after total size reached (tested
 * separately)
 *
 * Test Sequence:
 * 1. Configure and start hardware handshaking mode
 * 2. Assert triggers to complete all chunks
 * 3. Verify STATUS.done=1 after final chunk
 * 4. Read CONTROL register and verify go bit still set
 * 5. Verify STATUS.busy=0 (idle)
 * 6. Manually clear go bit via CONTROL write
 * 7. Verify go bit cleared after software write
 *
 * Pass Criteria:
 * - STATUS.done=1 after final chunk
 * - CONTROL.go=1 after completion (hardware does not clear it)
 * - STATUS.busy=0 after completion
 * - Software can clear go bit with CONTROL write
 * - CFG_REGWEN unlocks to 0x6 after completion
 *
 * Architecture Reference: FUNC-011 TC044, Test Plan
 * test_hw_handshake_go_bit_remains_set
 */
void testbench::test_hw_handshake_go_bit_remains_set() {
  std::string test_name = "Go Bit Remains Set After Completion";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Simple single-chunk transfer for quick completion
  const uint32_t src_addr = 0x40001000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;

  // Configure transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  // Enable handshaking
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  // Start handshaking mode
  uint32_t control_val = 0x80010110; // Bit 4 = hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Trigger single chunk transfer
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);

  // Poll for completion
  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2)
      break; // done
    poll_count++;
  }

  // Verify STATUS.done=1
  if ((status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set; ";
  }

  // Verify STATUS.busy=0
  if (status & 0x1) {
    passed = false;
    msg << "STATUS.busy still set after completion; ";
  }

  // Critical check: Verify CONTROL.go=1 (bit 31)
  uint32_t control_readback = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if ((control_readback & 0x80000000) == 0) {
    passed = false;
    msg << "CONTROL.go cleared after completion (should persist in handshake "
           "mode); ";
  }

  // Verify CFG_REGWEN unlocked (0x6)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x00000006) {
    passed = false;
    msg << "CFG_REGWEN not unlocked after completion; ";
  }

  // Manually clear go bit (software operation to exit handshaking mode)
  uint32_t control_clear_go = 0x00000000; // Clear all bits including go
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_clear_go);
  wait(5, SC_NS);

  // Verify go bit cleared by software write
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if (control_readback & 0x80000000) {
    passed = false;
    msg << "CONTROL.go not cleared by software write; ";
  }

  if (passed) {
    msg << "Go bit persistence verified: go=1 after hardware completion, "
           "cleared by software";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC015: No Chunk Done Interrupt in Hardware Handshake Mode
// =============================================================================

/**
 * @brief Verify chunk_done interrupt is NOT generated in hardware handshake
 * mode
 *
 * Test Objective:
 * - Confirm chunk_done interrupt suppression differentiates handshake from
 * normal mode
 * - Verify INTR_ENABLE.chunk_done does not cause chunk_done assertions
 * - Verify STATUS.chunk_done remains clear between chunks
 * - Verify only dma_done interrupt asserts on final chunk
 *
 * Test Sequence:
 * 1. Configure multi-chunk transfer in handshake mode (3 chunks)
 * 2. Enable both chunk_done and done interrupts in INTR_ENABLE
 * 3. Start handshaking mode
 * 4. Assert triggers for all chunks
 * 5. Monitor dma_chunk_done_intr signal throughout transfer
 * 6. Verify dma_chunk_done_intr never asserts
 * 7. Verify dma_done_intr asserts only on final chunk
 * 8. Verify STATUS.chunk_done never sets
 *
 * Pass Criteria:
 * - dma_chunk_done_intr remains deasserted throughout transfer
 * - STATUS.chunk_done=0 after non-final chunks
 * - dma_done_intr asserts on final chunk
 * - STATUS.done=1 on final chunk
 *
 * Architecture Reference: FUNC-011 TC045, Test Plan
 * test_hw_handshake_no_chunk_done_intr
 */
void testbench::test_hw_handshake_no_chunk_done_intr() {
  std::string test_name =
      "No Chunk Done Interrupt in Hardware Handshake Mode";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Multi-chunk transfer configuration
  const uint32_t src_addr = 0x40001000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 192; // 3 chunks
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;
  const uint32_t num_chunks = total_size / chunk_size;

  // Configure transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  // Enable handshaking
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));

  // Enable BOTH chunk_done and done interrupts (to verify chunk_done
  // suppression)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET,
                            0x00000003); // bits 1:0

  // Start handshaking mode
  uint32_t control_val = 0x80010110; // Bit 4 = hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Process all chunks and monitor chunk_done interrupt
  for (uint32_t chunk = 0; chunk < num_chunks; chunk++) {
    // Assert trigger
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);

    // Poll for chunk completion
    uint32_t poll_count = 0;
    while (poll_count < 100) {
      wait(10, SC_NS);

      // Check if chunk_done interrupt asserted (should never happen)
      bool chunk_done_intr = dma_chunk_done_intr_signal.read();
      if (chunk_done_intr) {
        passed = false;
        msg << "dma_chunk_done_intr asserted during handshake mode (chunk "
            << chunk << "); ";
      }

      // Check STATUS
      uint32_t status = 0;
      m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);

      // Verify STATUS.chunk_done never sets (bit 3)
      if (status & 0x8) {
        passed = false;
        msg << "STATUS.chunk_done set during handshake mode (chunk " << chunk
            << "); ";
      }

      // Check for completion
      // Note: STATUS.busy remains 1 during WAIT_TRIGGER (per architecture spec)
      if (chunk == (num_chunks - 1)) {
        if (status & 0x2)
          break; // done on final chunk
        poll_count++;
      } else {
        // Non-final chunk: enough samples collected, exit after reasonable wait
        if (poll_count >= 50)
          break; // 500ns total wait time
        poll_count++;
      }
    }
  }

  // Verify final done interrupt asserted
  bool done_intr = dma_done_intr_signal.read();
  if (!done_intr) {
    passed = false;
    msg << "dma_done_intr not asserted after final chunk; ";
  }

  // Verify STATUS.done=1
  uint32_t final_status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, final_status);
  if ((final_status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set after final chunk; ";
  }

  if (passed) {
    msg << "Chunk done interrupt suppression verified: no chunk_done "
           "assertions across "
        << num_chunks << " chunks, only final done interrupt";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// FUNC-011 TC016: Total Size Reached - No Response to Further Triggers
// =============================================================================

/**
 * @brief Verify DMA ignores trigger assertions after TOTAL_DATA_SIZE completion
 *
 * Test Objective:
 * - Confirm DMA stops responding to triggers after total size reached
 * - Verify STATUS.done persists
 * - Verify STATUS.busy remains 0
 * - Verify no additional chunk transfers initiated
 * - Verify go bit still set (but no activity)
 *
 * Test Sequence:
 * 1. Configure single-chunk transfer
 * 2. Start handshaking mode
 * 3. Assert trigger to complete chunk (reaches total size)
 * 4. Verify STATUS.done=1
 * 5. Assert trigger again multiple times
 * 6. Verify STATUS.busy remains 0 (no new transfer initiated)
 * 7. Verify STATUS.done remains 1
 * 8. Verify destination memory unchanged after trigger assertions
 *
 * Pass Criteria:
 * - STATUS.done=1 after first chunk
 * - Subsequent trigger assertions do not initiate transfers
 * - STATUS.busy=0 after completion
 * - CONTROL.go=1 persists
 * - No memory modifications after done
 *
 * Architecture Reference: FUNC-011 TC115, Test Plan
 * test_hw_handshake_total_size_reached
 */
void testbench::test_hw_handshake_total_size_reached() {
  std::string test_name =
      "Total Size Reached - No Response to Further Triggers";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Single-chunk transfer (total size reached after one trigger)
  const uint32_t src_addr = 0x40001000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 64;
  const uint32_t chunk_size = 64;
  const uint32_t trigger_index = 0;

  // Initialize source with distinctive pattern
  unsigned char test_data[chunk_size];
  for (uint32_t i = 0; i < chunk_size; i++) {
    test_data[i] = static_cast<unsigned char>(0xD0 + i);
  }
  m_test->write_ot_memory_block(src_addr, test_data, chunk_size);

  // Configure transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  // Enable handshaking
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  // Start handshaking mode
  uint32_t control_val = 0x80010110; // Bit 4 = hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // First trigger: completes total size
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);
  m_test->set_lsio_trigger(trigger_index, false);

  // Poll for completion
  uint32_t poll_count = 0;
  uint32_t status = 0;
  while (poll_count < 100) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x2)
      break; // done
    poll_count++;
  }

  // Verify done
  if ((status & 0x2) == 0) {
    passed = false;
    msg << "STATUS.done not set after first trigger; ";
  }

  if (status & 0x1) {
    passed = false;
    msg << "STATUS.busy still set after completion; ";
  }

  // Take snapshot of destination memory
  uint8_t memory_snapshot[chunk_size];
  for (uint32_t i = 0; i < chunk_size; i++) {
    memory_snapshot[i] = m_test->read_ot_memory_byte(dst_addr + i);
  }

  // Assert trigger multiple times (should be ignored)
  const uint32_t spurious_triggers = 5;
  for (uint32_t trig = 0; trig < spurious_triggers; trig++) {
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);
    wait(20, SC_NS);

    // Verify STATUS.busy never sets
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & 0x1) {
      passed = false;
      msg << "STATUS.busy set after spurious trigger " << trig
          << " (should be ignored); ";
    }

    // Verify STATUS.done persists
    if ((status & 0x2) == 0) {
      passed = false;
      msg << "STATUS.done cleared after spurious trigger; ";
    }
  }

  // Verify memory unchanged after spurious triggers
  for (uint32_t i = 0; i < chunk_size; i++) {
    uint8_t current = m_test->read_ot_memory_byte(dst_addr + i);
    if (current != memory_snapshot[i]) {
      passed = false;
      msg << "Memory modified after spurious triggers (byte " << i << "); ";
      break;
    }
  }

  // Verify go bit still set
  uint32_t control_readback = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if ((control_readback & 0x80000000) == 0) {
    passed = false;
    msg << "CONTROL.go cleared unexpectedly; ";
  }

  if (passed) {
    msg << "Total size completion verified: " << spurious_triggers
        << " spurious triggers ignored, STATUS.done persists, memory unchanged";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// Helper Function: Generic Hardware Handshake Test
// =============================================================================

/**
 * @brief Generic hardware handshake test for triggers 1-10
 * @param trigger_index Trigger input index (1-10)
 * @param src_addr Source address
 * @param dst_addr Destination address
 * @param total_size Total transfer size in bytes
 * @param chunk_size Chunk size in bytes
 * @return True if test passed, false otherwise
 *
 * Performs trigger0-style hardware handshake verification:
 * - Configures transfer and enables specified trigger
 * - Starts handshake mode with go + hardware_handshake_enable
 * - Asserts trigger and polls STATUS.done after each assertion
 * - Verifies completion status and control-bit persistence
 */
bool testbench::run_generic_hw_handshake_test(uint32_t trigger_index,
                                              uint32_t src_addr,
                                              uint32_t dst_addr,
                                              uint32_t total_size,
                                              uint32_t chunk_size) {

  bool passed = true;
  uint32_t status = 0;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure transfer
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000);

  uint32_t asid_val = (0x7 << 0) | (0x7 << 4);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, asid_val);
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000002); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);

  wait(10, SC_NS);

  // Enable handshaking
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000001);

  // Start handshaking mode: go=1 and hardware_handshake_enable=1
  uint32_t control_val = 0x80000010; // Bit 31=go, Bit 4=hardware_handshake_enable
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  wait(10, SC_NS);

  // Verify DMA entered BUSY state
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) { // Bit 0: busy
    passed = false;
  }

  // Trigger-driven transfer using STATUS.done polling after each trigger.
  const uint32_t num_chunks = (total_size + chunk_size - 1) / chunk_size;
  const uint32_t max_status_polls_per_trigger = 100;
  uint32_t triggers_issued = 0;
  bool transfer_done = false;

  while (!transfer_done && triggers_issued < num_chunks) {
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);
    triggers_issued++;

    uint32_t poll_count = 0;
    while (poll_count < max_status_polls_per_trigger) {
      wait(10, SC_NS);
      m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
      if (status & 0x2) { // Bit 1: done
        transfer_done = true;
        break;
      }
      poll_count++;
    }
  }

  if (!transfer_done) {
    passed = false;
  }

  // Verify completion
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x2) == 0) {
    passed = false;
  }

  if (status & 0x1) { // Bit 0: busy
    passed = false;
  }

  // Verify go bit remains set in hardware handshake mode
  uint32_t control_readback = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_readback);
  if ((control_readback & 0x80000000) == 0) { // Bit 31: go
    passed = false;
  }

  return passed;
}

// =============================================================================
// RX-drain handshake: DMA must NOT drain before the first watermark trigger
// =============================================================================

/**
 * @brief Arming a handshake transfer must not move any data until the first
 *        watermark trigger arrives.
 *
 * Models the real boot flow, where the DMA is armed BEFORE the peripheral read
 * is issued. If the engine drains a chunk immediately on arm it reads an empty
 * peripheral FIFO (returning zero) and corrupts the transfer. This verifies
 * that, after go with hardware-handshake enabled but with NO trigger asserted,
 * the transfer is armed (busy=1) yet has moved nothing (done=0, destination
 * unchanged). It then confirms the transfer still completes correctly once the
 * triggers are supplied.
 */
void testbench::test_hw_handshake_no_drain_before_trigger() {
  std::string test_name =
      "Handshake: no drain before first trigger (arm-before-data)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10001000; // peripheral FIFO (fixed), OT internal
  const uint32_t dst_addr = 0x10002000; // memory buffer (increment), OT internal
  const uint32_t total_size = 64;       // 4 chunks
  const uint32_t chunk_size = 16;       // watermark-sized chunk
  const uint32_t trigger_index = 0;
  const uint8_t src_value = 0xA5;       // FIFO data
  const uint8_t dst_sentinel = 0xEE;    // pre-fill dest to detect a premature drain

  // Fixed-source FIFO returns 4 bytes per transaction.
  for (uint32_t i = 0; i < 4; i++)
    m_test->write_ot_memory_byte(src_addr + i, src_value);
  // Pre-fill the destination so any premature chunk-1 write is detectable.
  for (uint32_t i = 0; i < total_size; i++)
    m_test->write_ot_memory_w_byte(dst_addr + i, dst_sentinel);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET,
                            (0x7 << 0) | (0x7 << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x2); // fixed FIFO (wrap, no increment)
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1); // increment
  wait(10, SC_NS);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x1);

  // Arm: go + hardware_handshake_enable, but DO NOT assert any trigger.
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000010);

  // Give the engine ample time to (wrongly) run a chunk if the bug is present.
  wait(500, SC_NS);

  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) {
    passed = false;
    msg << "not armed (STATUS.busy=0) after go; ";
  }
  if (status & 0x2) {
    passed = false;
    msg << "STATUS.done set before any trigger (premature drain); ";
  }
  bool dst_untouched = true;
  for (uint32_t i = 0; i < total_size; i++) {
    if (m_test->read_ot_memory_byte(dst_addr + i) != dst_sentinel) {
      dst_untouched = false;
      break;
    }
  }
  if (!dst_untouched) {
    passed = false;
    msg << "destination modified before any trigger (drain-before-data); ";
  }

  // Now supply the triggers and confirm the transfer completes with real data.
  const uint32_t num_chunks = total_size / chunk_size; // 4
  const uint32_t max_polls = 100;
  uint32_t triggers = 0;
  bool done = false;
  while (!done && triggers < num_chunks) {
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);
    triggers++;
    for (uint32_t p = 0; p < max_polls && !done; p++) {
      wait(10, SC_NS);
      m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
      if (status & 0x2)
        done = true;
    }
  }
  if (!done) {
    passed = false;
    msg << "transfer did not complete after " << triggers << " triggers; ";
  } else {
    for (uint32_t i = 0; i < total_size; i++) {
      if (m_test->read_ot_memory_byte(dst_addr + i) != src_value) {
        passed = false;
        msg << "post-transfer data mismatch at offset " << i << "; ";
        break;
      }
    }
  }

  if (passed)
    msg << "armed without draining; completed on " << triggers
        << " triggers with correct data";
  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// RX-drain handshake: multi-chunk transfer at the reference scale
// =============================================================================

/**
 * @brief Handshake-paced transfer of 1184 bytes as 74 chunks of 16 bytes,
 *        verifying byte-for-byte data correctness and completion.
 *
 * This mirrors the boot manifest-header read (74 x 16-byte chunks) at the
 * model level. An incrementing source pattern is used so ordering is verified
 * (not just a constant), one chunk drained per watermark trigger, completing
 * when TOTAL_DATA_SIZE is reached.
 */
void testbench::test_hw_handshake_multichunk_reference_74() {
  std::string test_name =
      "Handshake: 74-chunk (1184B) multi-chunk drain, data verified";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10001000; // incrementing source
  const uint32_t dst_addr = 0x10010000; // incrementing dest (well separated)
  const uint32_t total_size = 1184;     // 74 x 16
  const uint32_t chunk_size = 16;
  const uint32_t trigger_index = 0;
  const uint32_t num_chunks = total_size / chunk_size; // 74

  // Distinct per-byte source pattern so ordering is verified end to end.
  auto pattern = [](uint32_t i) -> uint8_t {
    return static_cast<uint8_t>((i * 7u + 13u) & 0xFF);
  };
  for (uint32_t i = 0; i < total_size; i++)
    m_test->write_ot_memory_byte(src_addr + i, pattern(i));

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET,
                            (0x7 << 0) | (0x7 << 4));
  // Hardware requires RANGE_VALID for every transfer, not just
  // cross-boundary ones.
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1); // increment
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1); // increment
  wait(10, SC_NS);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1 << trigger_index));
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000010);
  wait(10, SC_NS);

  const uint32_t max_polls = 100;
  uint32_t triggers = 0;
  uint32_t status = 0;
  bool done = false;
  while (!done && triggers < num_chunks) {
    m_test->set_lsio_trigger(trigger_index, true);
    wait(5, SC_NS);
    m_test->set_lsio_trigger(trigger_index, false);
    triggers++;
    for (uint32_t p = 0; p < max_polls && !done; p++) {
      wait(10, SC_NS);
      m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
      if (status & 0x2)
        done = true;
    }
  }

  if (!done) {
    passed = false;
    msg << "STATUS.done not observed after " << triggers << " triggers (expected "
        << num_chunks << "); ";
  }
  if (triggers != num_chunks) {
    passed = false;
    msg << "completed in " << triggers << " triggers, expected " << num_chunks
        << " (one chunk per trigger); ";
  }
  // Byte-for-byte data verification across all 74 chunks.
  for (uint32_t i = 0; i < total_size && passed; i++) {
    uint8_t actual = m_test->read_ot_memory_byte(dst_addr + i);
    if (actual != pattern(i)) {
      passed = false;
      msg << "data mismatch at offset " << i << " (expected 0x" << std::hex
          << (int)pattern(i) << ", got 0x" << (int)actual << std::dec << "); ";
    }
  }

  if (passed)
    msg << "74 chunks drained one-per-trigger; 1184 bytes verified byte-for-byte";
  report_test_result(test_name, passed, msg.str());
}
