// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_dma_func_013.cpp
 * @brief FUNC-013: Zero fill (CONTROL.opcode = 0x4)
 *
 * A zero fill writes TOTAL_DATA_SIZE zero bytes at the destination and never
 * reads the source. The cases follow the RTL (secure_dma.sv):
 * - no read is issued and the destination reads zero (SRAM, ICCM, DCCM ranges);
 * - the fill never feeds the SHA-2 engine; an initial start clears the digest;
 * - handshake mode is refused with OPCODE_ERROR when the trigger arrives;
 * - a start with no bytes left is refused with SIZE_ERROR and writes nothing;
 * - every beat is checked against ENABLED_MEMORY_RANGE, whatever the ASIDs; a
 *   beat that would leave it is not issued and raises DST_ADDR_ERROR;
 * - an ICCM fill inside an ICCM window completes;
 * - the source is still validated though it is never read;
 * - STATUS.done from a previous transfer clears on start.
 */

#include "testbench.h"
#include <sstream>

namespace {

constexpr uint32_t kGo = 1U << 31;
constexpr uint32_t kInitialTransfer = 1U << 8;
constexpr uint32_t kHandshake = 1U << 4;
constexpr uint32_t kOpcodeZero = 0x4;
constexpr uint32_t kOpcodeSha256 = 0x1;

constexpr uint32_t kStatusBusy = 1U << 0;
constexpr uint32_t kStatusDone = 1U << 1;
constexpr uint32_t kStatusError = 1U << 3;
constexpr uint32_t kStatusDigestValid = 1U << 4;

constexpr uint32_t kErrSrcAddr = 1U << 0;
constexpr uint32_t kErrDstAddr = 1U << 1;
constexpr uint32_t kErrOpcode = 1U << 2;
constexpr uint32_t kErrSize = 1U << 3;
constexpr uint32_t kErrAsid = 1U << 7;

constexpr uint32_t kSramDst = 0x20004000;
constexpr uint32_t kIccmBase = 0xC0000000;
constexpr uint32_t kIccmLimit = 0xC003FFFF;
constexpr uint32_t kDccmBase = 0xC0040000;
constexpr uint32_t kDccmLimit = 0xC005FFFF;

constexpr uint8_t kSentinel = 0xA5;

struct fill_cfg {
  uint32_t dst = kSramDst;
  uint32_t total = 64;
  uint32_t base = kSramDst;
  uint32_t limit = kSramDst + 63;
  uint32_t src_lo = 0;
  uint32_t src_hi = 0;
  uint32_t asid = 0x77;
};

void program_fill(secure_dma_test *t, const fill_cfg &c) {
  t->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, c.src_lo);
  t->register_write_32(secure_dma_basetest::SRC_ADDR_HI_OFFSET, c.src_hi);
  t->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, c.dst);
  t->register_write_32(secure_dma_basetest::DST_ADDR_HI_OFFSET, 0);
  t->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, c.asid);
  t->register_write_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, c.base);
  t->register_write_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, c.limit);
  t->register_write_32(secure_dma_basetest::RANGE_VALID_OFFSET, 0x1);
  t->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, c.total);
  t->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, c.total);
  t->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  t->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x1);
  t->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x1);
}

void fill_dst(secure_dma_test *t, uint32_t addr, uint32_t len, uint8_t v) {
  for (uint32_t i = 0; i < len; ++i)
    t->write_ot_memory_w_byte(addr + i, v);
}

// First address in [addr, addr+len) whose byte differs from v, or addr+len.
uint32_t first_mismatch(secure_dma_test *t, uint32_t addr, uint32_t len, uint8_t v) {
  for (uint32_t i = 0; i < len; ++i)
    if (t->read_ot_memory_w_byte(addr + i) != v)
      return addr + i;
  return addr + len;
}

// Polls STATUS until done or error, or 2000 polls of 10 ns.
uint32_t wait_done_or_error(secure_dma_test *t) {
  uint32_t status = 0;
  for (int i = 0; i < 2000; ++i) {
    t->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & (kStatusDone | kStatusError))
      break;
    sc_core::wait(10, sc_core::SC_NS);
  }
  return status;
}

uint32_t read_error_code(secure_dma_test *t) {
  uint32_t ec = 0;
  t->register_read_32(secure_dma_basetest::ERROR_CODE_OFFSET, ec);
  return ec;
}

} // namespace

void testbench::run_func013_tests() {
  REG_INFO(1, logger) << "\n========================================\n"
                      << "FUNC-013: Zero Fill Tests\n"
                      << "========================================\n"
                      << std::endl;

  wait(20, SC_NS);

  test_func013_zero_fill_writes_zero_without_reading();
  test_func013_zero_fill_does_not_feed_sha();
  test_func013_zero_fill_handshake_opcode_error();
  test_func013_zero_fill_no_bytes_left_size_error();
  test_func013_zero_fill_beat_outside_range();
  test_func013_zero_fill_iccm_window();
  test_func013_zero_fill_source_still_validated();
  test_func013_zero_fill_done_clears_on_start();
}

void testbench::test_func013_zero_fill_writes_zero_without_reading() {
  const char *test_name = "FUNC-013 TC001: zero fill writes zeros and issues no read";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  struct region {
    const char *name;
    uint32_t dst, base, limit;
  };
  const region regions[] = {
      {"SRAM", kSramDst, kSramDst, kSramDst + 63},
      {"ICCM", kIccmBase + 0x100, kIccmBase, kIccmLimit},
      {"DCCM", kDccmBase + 0x100, kDccmBase, kDccmLimit},
  };

  for (const region &r : regions) {
    t->apply_reset(sc_time(100, SC_NS));
    wait(20, SC_NS);
    fill_dst(t, r.dst - 4, 72, kSentinel);

    fill_cfg c;
    c.dst = r.dst;
    c.base = r.base;
    c.limit = r.limit;
    program_fill(t, c);
    const unsigned reads_before = t->m_ot_reads;
    const unsigned writes_before = t->m_ot_writes;
    t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
    const uint32_t status = wait_done_or_error(t);
    const uint32_t ec = read_error_code(t);

    if ((status & kStatusDone) == 0 || (status & kStatusError) != 0 || ec != 0) {
      passed = false;
      msg << r.name << ": STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec
          << "; ";
    }
    if (t->m_ot_reads != reads_before) {
      passed = false;
      msg << r.name << ": " << (t->m_ot_reads - reads_before) << " reads issued; ";
    }
    if (t->m_ot_writes - writes_before != 16) {
      passed = false;
      msg << r.name << ": " << (t->m_ot_writes - writes_before) << " writes, expected 16; ";
    }
    if (first_mismatch(t, r.dst, 64, 0x00) != r.dst + 64) {
      passed = false;
      msg << r.name << ": destination not all zero; ";
    }
    if (first_mismatch(t, r.dst - 4, 4, kSentinel) != r.dst ||
        first_mismatch(t, r.dst + 64, 4, kSentinel) != r.dst + 68) {
      passed = false;
      msg << r.name << ": bytes outside the destination changed; ";
    }
  }

  if (passed)
    msg << "SRAM, ICCM and DCCM ranges zeroed with 16 writes and no reads";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_does_not_feed_sha() {
  const char *test_name = "FUNC-013 TC002: zero fill never feeds the SHA-2 engine";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // A SHA-256 copy first, so a digest is valid before the fill starts.
  for (uint32_t i = 0; i < 64; ++i)
    t->write_ot_memory_r_byte(0x10000000 + i, static_cast<uint8_t>(i));
  fill_cfg h;
  h.src_lo = 0x10000000;
  h.dst = kSramDst;
  program_fill(t, h);
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeSha256);
  uint32_t status = wait_done_or_error(t);
  if ((status & kStatusDigestValid) == 0) {
    passed = false;
    msg << "precondition: SHA-256 copy left no valid digest (STATUS=0x" << std::hex << status
        << std::dec << "); ";
  }

  fill_cfg c;
  program_fill(t, c);
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  status = wait_done_or_error(t);

  // An initial start clears sha2_digest_valid and the digest registers; a fill
  // computes no digest, so neither comes back.
  if ((status & kStatusDone) == 0 || (status & kStatusDigestValid) != 0) {
    passed = false;
    msg << "after fill STATUS=0x" << std::hex << status << std::dec << "; ";
  }
  for (unsigned i = 0; i < 16; ++i) {
    uint32_t word = 0;
    t->register_read_32(secure_dma_basetest::SHA2_DIGEST_OFFSET + 4 * i, word);
    if (word != 0) {
      passed = false;
      msg << "SHA2_DIGEST_" << i << "=0x" << std::hex << word << std::dec << "; ";
      break;
    }
  }

  if (passed)
    msg << "Fill cleared the digest on start and produced none";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_handshake_opcode_error() {
  const char *test_name = "FUNC-013 TC003: zero fill in handshake mode raises OPCODE_ERROR";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);
  t->set_lsio_trigger(0, false);
  fill_dst(t, kSramDst, 64, kSentinel);

  fill_cfg c;
  program_fill(t, c);
  t->register_write_32(secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, 0x1);
  const unsigned writes_before = t->m_ot_writes;
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET,
                       kGo | kInitialTransfer | kHandshake | kOpcodeZero);
  wait(50, SC_NS);

  if (read_error_code(t) != 0) {
    passed = false;
    msg << "error raised before the trigger; ";
  }

  t->set_lsio_trigger(0, true);
  const uint32_t status = wait_done_or_error(t);
  t->set_lsio_trigger(0, false);
  const uint32_t ec = read_error_code(t);

  if ((status & kStatusError) == 0 || (ec & kErrOpcode) == 0) {
    passed = false;
    msg << "STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec << "; ";
  }
  if (t->m_ot_writes != writes_before || first_mismatch(t, kSramDst, 64, kSentinel) != kSramDst + 64) {
    passed = false;
    msg << "destination written; ";
  }

  if (passed)
    msg << "OPCODE_ERROR on the trigger, nothing written";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_no_bytes_left_size_error() {
  const char *test_name = "FUNC-013 TC004: zero fill start with no bytes left raises SIZE_ERROR";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  fill_cfg c;
  program_fill(t, c);
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  uint32_t status = wait_done_or_error(t);
  if ((status & kStatusDone) == 0) {
    passed = false;
    msg << "precondition: first fill did not complete; ";
  }

  fill_dst(t, kSramDst, 64, kSentinel);
  const unsigned writes_before = t->m_ot_writes;
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kOpcodeZero);
  status = wait_done_or_error(t);
  const uint32_t ec = read_error_code(t);

  if ((status & kStatusError) == 0 || (ec & kErrSize) == 0) {
    passed = false;
    msg << "STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec << "; ";
  }
  if (t->m_ot_writes != writes_before || first_mismatch(t, kSramDst, 64, kSentinel) != kSramDst + 64) {
    passed = false;
    msg << "destination written; ";
  }

  if (passed)
    msg << "SIZE_ERROR, nothing written";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_beat_outside_range() {
  const char *test_name = "FUNC-013 TC005: zero fill beat outside the range raises DST_ADDR_ERROR";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);
  fill_dst(t, kSramDst, 64, kSentinel);

  fill_cfg c;
  c.limit = kSramDst + 31;
  program_fill(t, c);
  const unsigned writes_before = t->m_ot_writes;
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  const uint32_t status = wait_done_or_error(t);
  const uint32_t ec = read_error_code(t);

  if ((status & kStatusError) == 0 || (ec & kErrDstAddr) == 0) {
    passed = false;
    msg << "STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec << "; ";
  }
  if (t->m_ot_writes - writes_before != 8) {
    passed = false;
    msg << (t->m_ot_writes - writes_before) << " writes, expected 8; ";
  }
  if (first_mismatch(t, kSramDst, 32, 0x00) != kSramDst + 32 ||
      first_mismatch(t, kSramDst + 32, 32, kSentinel) != kSramDst + 64) {
    passed = false;
    msg << "expected 32 zero bytes then the untouched remainder; ";
  }

  if (passed)
    msg << "Eight in-range beats written; the ninth refused with DST_ADDR_ERROR";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_iccm_window() {
  const char *test_name = "FUNC-013 TC006: zero fill inside an ICCM window completes";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);
  fill_dst(t, kIccmBase, 64, kSentinel);

  fill_cfg c;
  c.dst = kIccmBase;
  c.base = kIccmBase;
  c.limit = kIccmLimit;
  program_fill(t, c);
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  const uint32_t status = wait_done_or_error(t);
  const uint32_t ec = read_error_code(t);

  if ((status & kStatusDone) == 0 || ec != 0 ||
      first_mismatch(t, kIccmBase, 64, 0x00) != kIccmBase + 64) {
    passed = false;
    msg << "STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec << "; ";
  }

  if (passed)
    msg << "ICCM fill inside an ICCM window completes";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_source_still_validated() {
  const char *test_name = "FUNC-013 TC007: zero fill still validates the source";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  struct src_case {
    const char *name;
    uint32_t lo, hi, asid, expect;
  };
  const src_case cases[] = {
      {"invalid source ASID", 0, 0, 0x7F, kErrAsid},
      {"misaligned source", 0x2, 0, 0x77, kErrSrcAddr},
      {"source high bits for ASID 7", 0, 0x1, 0x77, kErrSrcAddr},
  };

  for (const src_case &sc : cases) {
    t->apply_reset(sc_time(100, SC_NS));
    wait(20, SC_NS);
    fill_dst(t, kSramDst, 64, kSentinel);

    fill_cfg c;
    c.src_lo = sc.lo;
    c.src_hi = sc.hi;
    c.asid = sc.asid;
    program_fill(t, c);
    const unsigned writes_before = t->m_ot_writes;
    t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
    const uint32_t status = wait_done_or_error(t);
    const uint32_t ec = read_error_code(t);

    if ((status & kStatusError) == 0 || (ec & sc.expect) == 0) {
      passed = false;
      msg << sc.name << ": STATUS=0x" << std::hex << status << " ERROR_CODE=0x" << ec << std::dec
          << "; ";
    }
    if (t->m_ot_writes != writes_before) {
      passed = false;
      msg << sc.name << ": destination written; ";
    }
  }

  if (passed)
    msg << "Bad ASID, misaligned source and source high bits all refused before any write";
  report_test_result(test_name, passed, msg.str());
}

void testbench::test_func013_zero_fill_done_clears_on_start() {
  const char *test_name = "FUNC-013 TC008: STATUS.done from a previous transfer clears on start";
  REG_INFO(1, logger) << "Running: " << test_name << std::endl;
  bool passed = true;
  std::stringstream msg;
  secure_dma_test *t = m_test.get();

  t->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  fill_cfg c;
  c.total = 4096;
  c.limit = kSramDst + 4095;
  program_fill(t, c);
  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  uint32_t status = wait_done_or_error(t);
  if ((status & kStatusDone) == 0) {
    passed = false;
    msg << "precondition: first fill did not complete; ";
  }

  t->register_write_32(secure_dma_basetest::CONTROL_OFFSET, kGo | kInitialTransfer | kOpcodeZero);
  t->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & kStatusDone) != 0 || (status & kStatusBusy) == 0) {
    passed = false;
    msg << "STATUS right after go=0x" << std::hex << status << std::dec << "; ";
  }
  status = wait_done_or_error(t);
  if ((status & kStatusDone) == 0) {
    passed = false;
    msg << "second fill did not complete; ";
  }

  if (passed)
    msg << "DONE cleared on start and set again on completion";
  report_test_result(test_name, passed, msg.str());
}
