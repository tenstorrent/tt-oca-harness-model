// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_dma_func_014.cpp
 * @brief FUNC-014: Control state machine and bus beats of secure_dma.sv
 *
 * - every beat is a word-aligned 4-byte transaction with the RTL's byte
 *   enables, and a sub-word read is steered to every lane;
 * - a memory-to-memory chunk ends in idle with go and busy clear, and the next
 *   go continues the transfer: byte count and addresses carry over;
 * - a fixed-address side advances its address register by a chunk;
 * - an error holds busy, go and the configuration lock until STATUS.error is
 *   cleared, and any STATUS write while it is set clears ERROR_CODE;
 * - abort leaves any state and keeps the error flag and the digest;
 * - interrupts are status type: INTR_STATE follows STATUS and INTR_TEST;
 * - handshake triggers are levels, the clearing writes go out in index order,
 *   and the final chunk clears go;
 * - initial_transfer is cleared by hardware; the range check, the SoC chunk
 *   bound and the address-width rules match the RTL;
 * - inline hashing masks a partial final word, and the digest swap is taken
 *   only by the initial start;
 * - a zero fill stops between chunks and resumes like a copy;
 * - RANGE_REGWEN stores the MuBi4 AND of its value and the write.
 */

#include "testbench.h"
#include <openssl/sha.h>
#include <sstream>

namespace {

constexpr uint32_t kGo = 1U << 31;
constexpr uint32_t kAbort = 1U << 27;
constexpr uint32_t kInitial = 1U << 8;
constexpr uint32_t kHandshake = 1U << 4;
constexpr uint32_t kDigestSwap = 1U << 5;

constexpr uint32_t kBusy = 1U << 0;
constexpr uint32_t kDone = 1U << 1;
constexpr uint32_t kAborted = 1U << 2;
constexpr uint32_t kError = 1U << 3;
constexpr uint32_t kDigestValid = 1U << 4;
constexpr uint32_t kChunkDone = 1U << 5;

constexpr uint32_t kErrSrcAddr = 1U << 0;
constexpr uint32_t kErrDstAddr = 1U << 1;
constexpr uint32_t kErrSize = 1U << 3;
constexpr uint32_t kErrBaseLimit = 1U << 5;

constexpr uint32_t kSrc = 0x10000000;
constexpr uint32_t kDst = 0x20000000;
constexpr uint8_t kSentinel = 0xEE;

struct xfer {
  uint32_t src = kSrc;
  uint32_t src_hi = 0;
  uint32_t dst = kDst;
  uint32_t total = 16;
  uint32_t chunk = 16;
  uint32_t width = 0x2;
  uint32_t src_cfg = 0x1;
  uint32_t dst_cfg = 0x1;
  uint32_t asid = 0x77;
  uint32_t base = 0;
  uint32_t limit = 0xFFFFFFFF;
};

using tb = secure_dma_basetest;

void program(secure_dma_test *t, const xfer &x) {
  t->register_write_32(tb::SRC_ADDR_LO_OFFSET, x.src);
  t->register_write_32(tb::SRC_ADDR_HI_OFFSET, x.src_hi);
  t->register_write_32(tb::DST_ADDR_LO_OFFSET, x.dst);
  t->register_write_32(tb::DST_ADDR_HI_OFFSET, 0);
  t->register_write_32(tb::ADDR_SPACE_ID_OFFSET, x.asid);
  t->register_write_32(tb::ENABLED_MEMORY_RANGE_BASE_OFFSET, x.base);
  t->register_write_32(tb::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, x.limit);
  t->register_write_32(tb::RANGE_VALID_OFFSET, 0x1);
  t->register_write_32(tb::TOTAL_DATA_SIZE_OFFSET, x.total);
  t->register_write_32(tb::CHUNK_DATA_SIZE_OFFSET, x.chunk);
  t->register_write_32(tb::TRANSFER_WIDTH_OFFSET, x.width);
  t->register_write_32(tb::SRC_CONFIG_OFFSET, x.src_cfg);
  t->register_write_32(tb::DST_CONFIG_OFFSET, x.dst_cfg);
}

void fill_ot(secure_dma_test *t, uint32_t src, uint32_t dst, uint32_t len) {
  for (uint32_t i = 0; i < len; ++i) {
    t->write_ot_memory_r_byte(src + i, static_cast<uint8_t>(0x40 + i));
    t->write_ot_memory_w_byte(dst + i, kSentinel);
  }
}

uint32_t reg(secure_dma_test *t, unsigned offset) {
  uint32_t v = 0;
  t->register_read_32(offset, v);
  return v;
}

// Polls STATUS until any of @p bits is set, or 1000 polls of 10 ns.
uint32_t wait_status(secure_dma_test *t, uint32_t bits) {
  uint32_t status = 0;
  for (int i = 0; i < 1000; ++i) {
    status = reg(t, tb::STATUS_OFFSET);
    if (status & bits)
      break;
    sc_core::wait(10, sc_core::SC_NS);
  }
  return status;
}

std::string hex(uint64_t v) {
  std::stringstream s;
  s << "0x" << std::hex << v;
  return s.str();
}

// Checks one logged beat, appending to @p msg on a mismatch.
void expect_beat(const secure_dma_test::beat &b, char port, bool write, uint64_t addr, uint8_t be,
                 std::stringstream &msg, bool &passed, const char *what) {
  if (b.port != port || b.write != write || b.addr != addr || b.len != 4 || b.be != be) {
    passed = false;
    msg << what << ": got " << b.port << (b.write ? " W " : " R ") << hex(b.addr) << " len "
        << b.len << " be " << hex(b.be) << ", expected " << port << (write ? " W " : " R ")
        << hex(addr) << " len 4 be " << hex(be) << "; ";
  }
}

} // namespace

void testbench::run_func014_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                      << "FUNC-014: Control State Machine and Bus Beats\n"
                      << "========================================\n"
                      << std::endl;
  wait(20, SC_NS);

  test_func014_one_byte_beats();
  test_func014_two_byte_beats();
  test_func014_four_byte_partial_word();
  test_func014_chunk_pause_and_continue();
  test_func014_fixed_address_writeback();
  test_func014_error_state_holds_busy();
  test_func014_abort_from_idle_and_error();
  test_func014_abort_keeps_digest();
  test_func014_status_type_interrupts();
  test_func014_handshake_level_and_clear_order();
  test_func014_initial_transfer_cleared();
  test_func014_base_limit_every_transfer();
  test_func014_soc_chunk_bound();
  test_func014_address_width_rules();
  test_func014_sha_partial_word();
  test_func014_digest_swap_captured();
  test_func014_zero_fill_chunk_pause();
  test_func014_range_regwen_mubi_and();
}

void testbench::test_func014_one_byte_beats() {
  const char *test_name = "FUNC-014 TC001: one-byte beats are word aligned with steered data";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.src = kSrc + 1;
  x.dst = kDst + 2;
  x.total = 3;
  x.chunk = 3;
  x.width = 0x0;
  fill_ot(t, kSrc, kDst, 8);
  program(t, x);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  const uint32_t status = wait_status(t, kDone | kError);
  if ((status & (kDone | kError)) != kDone) {
    passed = false;
    msg << "STATUS " << hex(status) << "; ";
  }

  if (t->m_beats.size() != 6) {
    passed = false;
    msg << t->m_beats.size() << " beats, expected 6; ";
  } else {
    expect_beat(t->m_beats[0], 'O', false, kSrc, 0x2, msg, passed, "read 1");
    expect_beat(t->m_beats[1], 'O', true, kDst, 0x4, msg, passed, "write 1");
    expect_beat(t->m_beats[2], 'O', false, kSrc, 0x4, msg, passed, "read 2");
    expect_beat(t->m_beats[3], 'O', true, kDst, 0x8, msg, passed, "write 2");
    expect_beat(t->m_beats[4], 'O', false, kSrc, 0x8, msg, passed, "read 3");
    expect_beat(t->m_beats[5], 'O', true, kDst + 4, 0x1, msg, passed, "write 3");
    if (t->m_beats[1].data != 0x41414141u) {
      passed = false;
      msg << "write 1 data " << hex(t->m_beats[1].data) << " not steered to every lane; ";
    }
  }
  const uint8_t expect[8] = {kSentinel, kSentinel, 0x41, 0x42, 0x43, kSentinel, kSentinel, kSentinel};
  for (uint32_t i = 0; i < 8; ++i)
    if (t->read_ot_memory_w_byte(kDst + i) != expect[i]) {
      passed = false;
      msg << "dst byte " << i << " = " << hex(t->read_ot_memory_w_byte(kDst + i)) << "; ";
    }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_two_byte_beats() {
  const char *test_name = "FUNC-014 TC002: two-byte beats, the last one narrowed to a byte";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.src = kSrc + 2;
  x.total = 3;
  x.chunk = 3;
  x.width = 0x1;
  fill_ot(t, kSrc, kDst, 8);
  program(t, x);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  wait_status(t, kDone | kError);

  if (t->m_beats.size() != 4) {
    passed = false;
    msg << t->m_beats.size() << " beats, expected 4; ";
  } else {
    expect_beat(t->m_beats[0], 'O', false, kSrc, 0xC, msg, passed, "read 1");
    expect_beat(t->m_beats[1], 'O', true, kDst, 0x3, msg, passed, "write 1");
    expect_beat(t->m_beats[2], 'O', false, kSrc + 4, 0x1, msg, passed, "read 2");
    expect_beat(t->m_beats[3], 'O', true, kDst, 0x4, msg, passed, "write 2");
  }
  const uint8_t expect[4] = {0x42, 0x43, 0x44, kSentinel};
  for (uint32_t i = 0; i < 4; ++i)
    if (t->read_ot_memory_w_byte(kDst + i) != expect[i]) {
      passed = false;
      msg << "dst byte " << i << " = " << hex(t->read_ot_memory_w_byte(kDst + i)) << "; ";
    }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_four_byte_partial_word() {
  const char *test_name = "FUNC-014 TC003: a four-byte transfer masks its final partial word";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 6;
  x.chunk = 6;
  fill_ot(t, kSrc, kDst, 8);
  program(t, x);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  wait_status(t, kDone | kError);

  if (t->m_beats.size() != 4) {
    passed = false;
    msg << t->m_beats.size() << " beats, expected 4; ";
  } else {
    expect_beat(t->m_beats[1], 'O', true, kDst, 0xF, msg, passed, "write 1");
    expect_beat(t->m_beats[2], 'O', false, kSrc + 4, 0x3, msg, passed, "read 2");
    expect_beat(t->m_beats[3], 'O', true, kDst + 4, 0x3, msg, passed, "write 2");
  }
  for (uint32_t i = 0; i < 8; ++i) {
    const uint8_t want = i < 6 ? static_cast<uint8_t>(0x40 + i) : kSentinel;
    if (t->read_ot_memory_w_byte(kDst + i) != want) {
      passed = false;
      msg << "dst byte " << i << " = " << hex(t->read_ot_memory_w_byte(kDst + i)) << "; ";
    }
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_chunk_pause_and_continue() {
  const char *test_name = "FUNC-014 TC004: a chunk ends in idle and the next go continues it";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 16;
  x.chunk = 8;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  uint32_t status = wait_status(t, kChunkDone | kDone | kError);
  wait(50, SC_NS);
  status = reg(t, tb::STATUS_OFFSET);
  if ((status & (kBusy | kDone | kError | kChunkDone)) != kChunkDone) {
    passed = false;
    msg << "after chunk 1 STATUS " << hex(status) << ", expected only chunk_done; ";
  }
  if (reg(t, tb::CONTROL_OFFSET) & kGo) {
    passed = false;
    msg << "go still set after chunk 1; ";
  }
  if (reg(t, tb::CFG_REGWEN_OFFSET) != 0x6) {
    passed = false;
    msg << "CFG_REGWEN locked between chunks; ";
  }
  if (t->read_ot_memory_w_byte(kDst + 8) != kSentinel) {
    passed = false;
    msg << "chunk 2 moved before the second go; ";
  }

  // In increment mode the beat address carries over: the new SRC_ADDR is not used.
  t->register_write_32(tb::SRC_ADDR_LO_OFFSET, kSrc + 0x100);
  t->register_write_32(tb::CONTROL_OFFSET, kGo);
  status = reg(t, tb::STATUS_OFFSET);
  if (status & kChunkDone) {
    passed = false;
    msg << "chunk_done not cleared by the start; ";
  }
  status = wait_status(t, kDone | kError);
  if ((status & (kDone | kError | kBusy)) != kDone) {
    passed = false;
    msg << "after chunk 2 STATUS " << hex(status) << "; ";
  }
  for (uint32_t i = 0; i < 16; ++i)
    if (t->read_ot_memory_w_byte(kDst + i) != static_cast<uint8_t>(0x40 + i)) {
      passed = false;
      msg << "dst byte " << i << " = " << hex(t->read_ot_memory_w_byte(kDst + i)) << "; ";
      break;
    }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_fixed_address_writeback() {
  const char *test_name = "FUNC-014 TC005: a fixed address advances its register by a chunk";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 16;
  x.chunk = 8;
  x.src_cfg = 0x0;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  wait_status(t, kChunkDone | kError);
  if (reg(t, tb::SRC_ADDR_LO_OFFSET) != kSrc + 8 || reg(t, tb::DST_ADDR_LO_OFFSET) != kDst) {
    passed = false;
    msg << "after chunk 1 SRC " << hex(reg(t, tb::SRC_ADDR_LO_OFFSET)) << " DST "
        << hex(reg(t, tb::DST_ADDR_LO_OFFSET)) << "; ";
  }
  t->register_write_32(tb::CONTROL_OFFSET, kGo);
  wait_status(t, kDone | kError);
  if (reg(t, tb::SRC_ADDR_LO_OFFSET) != kSrc + 16) {
    passed = false;
    msg << "after chunk 2 SRC " << hex(reg(t, tb::SRC_ADDR_LO_OFFSET)) << "; ";
  }
  const uint64_t expect_src[4] = {kSrc, kSrc, kSrc + 8, kSrc + 8};
  unsigned reads = 0;
  for (const auto &b : t->m_beats)
    if (!b.write) {
      if (reads < 4 && b.addr != expect_src[reads]) {
        passed = false;
        msg << "read " << reads << " at " << hex(b.addr) << "; ";
      }
      ++reads;
    }
  if (reads != 4) {
    passed = false;
    msg << reads << " reads, expected 4; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_error_state_holds_busy() {
  const char *test_name = "FUNC-014 TC006: the error state holds busy, go and the lock";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.width = 0x3;
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  uint32_t status = wait_status(t, kError);
  if ((status & (kBusy | kError)) != (kBusy | kError) || reg(t, tb::ERROR_CODE_OFFSET) != kErrSize ||
      !(reg(t, tb::CONTROL_OFFSET) & kGo) || reg(t, tb::CFG_REGWEN_OFFSET) != 0x9) {
    passed = false;
    msg << "in error STATUS " << hex(status) << " ERROR_CODE " << hex(reg(t, tb::ERROR_CODE_OFFSET))
        << " CONTROL " << hex(reg(t, tb::CONTROL_OFFSET)) << " CFG_REGWEN "
        << hex(reg(t, tb::CFG_REGWEN_OFFSET)) << "; ";
  }
  t->register_write_32(tb::TRANSFER_WIDTH_OFFSET, 0x2);
  if (reg(t, tb::TRANSFER_WIDTH_OFFSET) != 0x3) {
    passed = false;
    msg << "configuration writable in the error state; ";
  }

  // Any STATUS write while the error is set clears ERROR_CODE.
  t->register_write_32(tb::STATUS_OFFSET, kDone);
  status = reg(t, tb::STATUS_OFFSET);
  if (reg(t, tb::ERROR_CODE_OFFSET) != 0 || (status & (kBusy | kError)) != (kBusy | kError)) {
    passed = false;
    msg << "after STATUS.done write ERROR_CODE " << hex(reg(t, tb::ERROR_CODE_OFFSET))
        << " STATUS " << hex(status) << "; ";
  }

  t->register_write_32(tb::STATUS_OFFSET, kError);
  status = reg(t, tb::STATUS_OFFSET);
  if ((status & (kBusy | kError)) != 0 || (reg(t, tb::CONTROL_OFFSET) & kGo) ||
      reg(t, tb::CFG_REGWEN_OFFSET) != 0x6) {
    passed = false;
    msg << "after STATUS.error clear STATUS " << hex(status) << " CONTROL "
        << hex(reg(t, tb::CONTROL_OFFSET)) << "; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_abort_from_idle_and_error() {
  const char *test_name = "FUNC-014 TC007: abort leaves idle and the error state";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  t->register_write_32(tb::CONTROL_OFFSET, kAbort);
  uint32_t status = reg(t, tb::STATUS_OFFSET);
  if ((status & (kAborted | kBusy)) != kAborted) {
    passed = false;
    msg << "abort in idle STATUS " << hex(status) << "; ";
  }

  xfer x;
  x.width = 0x3;
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  status = reg(t, tb::STATUS_OFFSET);
  if (status & kAborted) {
    passed = false;
    msg << "aborted not cleared by the start; ";
  }
  wait_status(t, kError);
  t->register_write_32(tb::CONTROL_OFFSET, kAbort);
  status = reg(t, tb::STATUS_OFFSET);
  if ((status & (kAborted | kBusy | kError)) != (kAborted | kError) ||
      reg(t, tb::ERROR_CODE_OFFSET) != kErrSize || (reg(t, tb::CONTROL_OFFSET) & kGo)) {
    passed = false;
    msg << "abort in error STATUS " << hex(status) << " ERROR_CODE "
        << hex(reg(t, tb::ERROR_CODE_OFFSET)) << "; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_abort_keeps_digest() {
  const char *test_name = "FUNC-014 TC008: abort keeps a computed digest";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial | 0x1);
  wait_status(t, kDone | kError);
  const uint32_t digest0 = reg(t, tb::SHA2_DIGEST_OFFSET);
  t->register_write_32(tb::CONTROL_OFFSET, kAbort);
  const uint32_t status = reg(t, tb::STATUS_OFFSET);
  if (!(status & kDigestValid) || !(status & kAborted) || reg(t, tb::SHA2_DIGEST_OFFSET) != digest0 ||
      digest0 == 0) {
    passed = false;
    msg << "after abort STATUS " << hex(status) << " digest[0] "
        << hex(reg(t, tb::SHA2_DIGEST_OFFSET)) << " (was " << hex(digest0) << "); ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_status_type_interrupts() {
  const char *test_name = "FUNC-014 TC009: INTR_STATE follows STATUS and INTR_TEST";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  t->register_write_32(tb::INTR_ENABLE_OFFSET, 0x7);
  t->register_write_32(tb::INTR_TEST_OFFSET, 0x4);
  wait(10, SC_NS);
  if (reg(t, tb::INTR_STATE_OFFSET) != 0x4 || !dma_error_intr_signal.read()) {
    passed = false;
    msg << "INTR_TEST did not raise dma_error; ";
  }
  t->register_write_32(tb::INTR_STATE_OFFSET, 0x4);
  if (reg(t, tb::INTR_STATE_OFFSET) != 0x4) {
    passed = false;
    msg << "INTR_STATE write cleared a test interrupt; ";
  }
  t->register_write_32(tb::INTR_TEST_OFFSET, 0x0);
  wait(10, SC_NS);
  if (reg(t, tb::INTR_STATE_OFFSET) != 0 || dma_error_intr_signal.read()) {
    passed = false;
    msg << "INTR_TEST=0 did not drop dma_error; ";
  }

  xfer x;
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  wait_status(t, kDone | kError);
  t->register_write_32(tb::INTR_STATE_OFFSET, 0x1);
  if (!(reg(t, tb::INTR_STATE_OFFSET) & 0x1)) {
    passed = false;
    msg << "INTR_STATE write cleared dma_done while STATUS.done is set; ";
  }
  t->register_write_32(tb::STATUS_OFFSET, kDone);
  wait(10, SC_NS);
  if (reg(t, tb::INTR_STATE_OFFSET) != 0 || dma_done_intr_signal.read()) {
    passed = false;
    msg << "clearing STATUS.done did not drop dma_done; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_handshake_level_and_clear_order() {
  const char *test_name =
      "FUNC-014 TC010: a held trigger runs every chunk, clearing writes in index order";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 16;
  x.chunk = 4;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->register_write_32(tb::HANDSHAKE_INTR_ENABLE_OFFSET, 1u << 5);
  t->register_write_32(tb::CLEAR_INTR_SRC_OFFSET, (1u << 3) | (1u << 1));
  t->register_write_32(tb::CLEAR_INTR_BUS_OFFSET, 1u << 1);
  t->register_write_32(tb::INTR_SRC_ADDR_OFFSET + 4 * 1, 0x10005000);
  t->register_write_32(tb::INTR_SRC_ADDR_OFFSET + 4 * 3, 0x30006000);
  t->register_write_32(tb::INTR_SRC_WR_VAL_OFFSET + 4 * 1, 0x11);
  t->register_write_32(tb::INTR_SRC_WR_VAL_OFFSET + 4 * 3, 0x33);
  t->set_lsio_trigger(5, true);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial | kHandshake);
  const uint32_t status = wait_status(t, kDone | kError);
  t->set_lsio_trigger(5, false);

  if ((status & (kDone | kBusy | kError | kChunkDone)) != kDone ||
      (reg(t, tb::CONTROL_OFFSET) & kGo)) {
    passed = false;
    msg << "STATUS " << hex(status) << " CONTROL " << hex(reg(t, tb::CONTROL_OFFSET)) << "; ";
  }
  // Each chunk: the clearing writes for sources 1 then 3, then one read/write pair.
  if (t->m_beats.size() != 4 * 4) {
    passed = false;
    msg << t->m_beats.size() << " beats, expected 16; ";
  } else {
    for (unsigned c = 0; c < 4; ++c) {
      const auto &first = t->m_beats[4 * c];
      const auto &second = t->m_beats[4 * c + 1];
      if (first.port != 'O' || !first.write || first.addr != 0x10005000 || first.data != 0x11 ||
          second.port != 'C' || !second.write || second.addr != 0x30006000 || second.data != 0x33) {
        passed = false;
        msg << "chunk " << c << " clearing writes out of order; ";
        break;
      }
    }
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_initial_transfer_cleared() {
  const char *test_name = "FUNC-014 TC011: hardware clears initial_transfer on start";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  if (reg(t, tb::CONTROL_OFFSET) & kInitial) {
    passed = false;
    msg << "CONTROL " << hex(reg(t, tb::CONTROL_OFFSET)) << "; ";
  }
  wait_status(t, kDone | kError);
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_base_limit_every_transfer() {
  const char *test_name = "FUNC-014 TC012: BASE above LIMIT fails any transfer";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.base = 0x30000000;
  x.limit = 0x2FFFFFFF;
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
  wait_status(t, kDone | kError);
  if (reg(t, tb::ERROR_CODE_OFFSET) != kErrBaseLimit) {
    passed = false;
    msg << "ERROR_CODE " << hex(reg(t, tb::ERROR_CODE_OFFSET)) << "; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_soc_chunk_bound() {
  const char *test_name = "FUNC-014 TC013: a SoC-to-OT chunk must end below LIMIT";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  for (const uint32_t limit : {kDst + 16, kDst + 15}) {
    t->apply_reset(sc_time(100, SC_NS));
    wait(20, SC_NS);
    xfer x;
    x.asid = 0x79;
    x.base = kDst;
    x.limit = limit;
    program(t, x);
    t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
    wait_status(t, kDone | kError);
    const uint32_t want = limit == kDst + 16 ? 0 : kErrDstAddr;
    if (reg(t, tb::ERROR_CODE_OFFSET) != want) {
      passed = false;
      msg << "LIMIT " << hex(limit) << ": ERROR_CODE " << hex(reg(t, tb::ERROR_CODE_OFFSET)) << "; ";
    }
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_address_width_rules() {
  const char *test_name = "FUNC-014 TC014: only the system bus takes upper address bits";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  struct row {
    uint32_t asid;
    uint32_t want;
  };
  for (const row r : {row{0x7A, kErrSrcAddr}, row{0x77, kErrSrcAddr}, row{0x79, 0u}}) {
    t->apply_reset(sc_time(100, SC_NS));
    wait(20, SC_NS);
    xfer x;
    x.asid = r.asid;
    x.src_hi = 0x1;
    program(t, x);
    t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial);
    wait_status(t, kDone | kError);
    if (reg(t, tb::ERROR_CODE_OFFSET) != r.want) {
      passed = false;
      msg << "ASID " << hex(r.asid) << ": ERROR_CODE " << hex(reg(t, tb::ERROR_CODE_OFFSET)) << "; ";
    }
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_sha_partial_word() {
  const char *test_name = "FUNC-014 TC015: inline SHA-256 hashes only the enabled bytes";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 6;
  x.chunk = 6;
  fill_ot(t, kSrc, kDst, 8);
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial | 0x1);
  wait_status(t, kDone | kError);

  unsigned char data[6];
  for (unsigned i = 0; i < 6; ++i)
    data[i] = static_cast<unsigned char>(0x40 + i);
  unsigned char ref[SHA256_DIGEST_LENGTH];
  SHA256(data, sizeof(data), ref);
  for (unsigned i = 0; i < 8; ++i) {
    const uint32_t want = (uint32_t{ref[4 * i]} << 24) | (uint32_t{ref[4 * i + 1]} << 16) |
                          (uint32_t{ref[4 * i + 2]} << 8) | ref[4 * i + 3];
    if (reg(t, tb::SHA2_DIGEST_OFFSET + 4 * i) != want) {
      passed = false;
      msg << "digest[" << i << "] " << hex(reg(t, tb::SHA2_DIGEST_OFFSET + 4 * i)) << " want "
          << hex(want) << "; ";
      break;
    }
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_digest_swap_captured() {
  const char *test_name = "FUNC-014 TC016: the digest swap is taken by the initial start only";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 16;
  x.chunk = 8;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial | 0x1);
  wait_status(t, kChunkDone | kError);
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kDigestSwap | 0x1);
  wait_status(t, kDone | kError);

  unsigned char data[16];
  for (unsigned i = 0; i < 16; ++i)
    data[i] = static_cast<unsigned char>(0x40 + i);
  unsigned char ref[SHA256_DIGEST_LENGTH];
  SHA256(data, sizeof(data), ref);
  const uint32_t want =
      (uint32_t{ref[0]} << 24) | (uint32_t{ref[1]} << 16) | (uint32_t{ref[2]} << 8) | ref[3];
  if (reg(t, tb::SHA2_DIGEST_OFFSET) != want) {
    passed = false;
    msg << "digest[0] " << hex(reg(t, tb::SHA2_DIGEST_OFFSET)) << " want unswapped " << hex(want)
        << "; ";
  }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_zero_fill_chunk_pause() {
  const char *test_name = "FUNC-014 TC017: a two-chunk zero fill stops between chunks and resumes";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();
  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  xfer x;
  x.total = 16;
  x.chunk = 8;
  x.base = kDst;
  x.limit = kDst + 15;
  fill_ot(t, kSrc, kDst, 16);
  program(t, x);
  t->m_beats.clear();
  t->register_write_32(tb::CONTROL_OFFSET, kGo | kInitial | 0x4);
  uint32_t status = wait_status(t, kChunkDone | kDone | kError);
  if ((status & (kBusy | kDone | kError | kChunkDone)) != kChunkDone) {
    passed = false;
    msg << "after chunk 1 STATUS " << hex(status) << "; ";
  }
  for (uint32_t i = 0; i < 16; ++i) {
    const uint8_t want = i < 8 ? 0 : kSentinel;
    if (t->read_ot_memory_w_byte(kDst + i) != want) {
      passed = false;
      msg << "after chunk 1 dst byte " << i << " = " << hex(t->read_ot_memory_w_byte(kDst + i))
          << "; ";
      break;
    }
  }
  t->register_write_32(tb::CONTROL_OFFSET, kGo);
  status = wait_status(t, kDone | kError);
  if ((status & (kBusy | kDone | kError)) != kDone) {
    passed = false;
    msg << "after chunk 2 STATUS " << hex(status) << "; ";
  }
  for (uint32_t i = 0; i < 16; ++i)
    if (t->read_ot_memory_w_byte(kDst + i) != 0) {
      passed = false;
      msg << "after chunk 2 dst byte " << i << " nonzero; ";
      break;
    }
  for (const auto &b : t->m_beats)
    if (!b.write) {
      passed = false;
      msg << "the fill read at " << hex(b.addr) << "; ";
      break;
    }
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func014_range_regwen_mubi_and() {
  const char *test_name = "FUNC-014 TC018: RANGE_REGWEN stores the MuBi4 AND of old and written";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  struct row {
    uint32_t first, second, want;
  };
  for (const row r : {row{0x0, 0x6, 0x0}, row{0x9, 0x6, 0x9}, row{0x0, 0xF, 0x9}, row{0x6, 0x6, 0x6}}) {
    t->apply_reset(sc_time(100, SC_NS));
    wait(20, SC_NS);
    t->register_write_32(tb::RANGE_REGWEN_OFFSET, r.first);
    t->register_write_32(tb::RANGE_REGWEN_OFFSET, r.second);
    if (reg(t, tb::RANGE_REGWEN_OFFSET) != r.want) {
      passed = false;
      msg << hex(r.first) << " then " << hex(r.second) << ": "
          << hex(reg(t, tb::RANGE_REGWEN_OFFSET)) << ", expected " << hex(r.want) << "; ";
    }
  }
  report_test_result(test_name, passed, msg.str());
}
