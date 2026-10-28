// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "testbench.h"
#include <sstream>

void testbench::run_coverage_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                       << "Coverage: paths not hit by the FUNC suites\n"
                       << "========================================\n"
                       << std::endl;
  m_model->logger.setMaxVerbosity(3);
  wait(20, SC_NS);
  test_cov_handshake_trigger_already_high();
  test_cov_hash_reset_frees_context();
}

void testbench::test_cov_handshake_trigger_already_high() {
  std::string test_name = "Coverage: handshake trigger already HIGH";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  const uint32_t src_addr = 0x10000000;
  const uint32_t dst_addr = 0x20000000;
  const uint32_t total_size = 8;
  const uint32_t chunk_size = 4;
  const uint32_t trigger_index = 2;

  unsigned char data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  m_test->write_ot_memory_block(src_addr, data, total_size);

  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, total_size);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, chunk_size);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, src_addr);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, dst_addr);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET,
                            (0x7 << 0) | (0x7 << 4));
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,
                            (1u << trigger_index));

  // Assert trigger before arming so CONTROL.go takes the already-HIGH path.
  m_test->set_lsio_trigger(trigger_index, true);
  wait(5, SC_NS);

  const uint32_t control_val = (1u << 31) | (1u << 4); // go + handshake
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, control_val);

  bool transfer_done = false;
  uint32_t status = 0;
  for (int i = 0; i < 100 && !transfer_done; i++) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    transfer_done = (status & 0x2) != 0;
  }

  m_test->set_lsio_trigger(trigger_index, false);

  if (!transfer_done) {
    passed = false;
    msg << "STATUS.done not set with trigger already HIGH; ";
  }

  report_test_result(test_name, passed, msg.str());
}

void testbench::test_cov_hash_reset_frees_context() {
  std::string test_name = "Coverage: reset frees a live SHA-2 context";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // The first of two chunks leaves the SHA-512 context live between chunks.
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 16);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 8);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x77);
  m_test->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET,
                            (1u << 31) | (1u << 8) | 0x3);

  uint32_t status = 0;
  for (int i = 0; i < 100 && !(status & 0x20); i++) {
    wait(10, SC_NS);
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  }
  if (!(status & 0x20) || m_model->m_hash_ctx == nullptr) {
    passed = false;
    msg << "first chunk did not leave a live context (STATUS 0x" << std::hex << status << "); ";
  }

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  if (m_model->m_hash_ctx != nullptr) {
    passed = false;
    msg << "context still live after reset; ";
  }

  report_test_result(test_name, passed, msg.str());
}
