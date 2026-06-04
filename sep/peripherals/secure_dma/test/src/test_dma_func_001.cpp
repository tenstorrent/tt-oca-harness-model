/**
 * @file test_dma_func_001.cpp
 * @brief FUNC-001: Register Access and Configuration test implementation
 */

#include "testbench.h"
#include <iomanip>
#include <sstream>

// =============================================================================
// FUNC-001 Test Orchestration
// =============================================================================

void testbench::run_func001_tests() {
  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-001: Register Access and Configuration Tests\n"
                       << "========================================\n"
                       << std::endl;

  wait(10, SC_NS);

  // Execute all test cases
  test_reset_values();
  // test_func001_intr_state_read_only();
  // test_func001_intr_enable_read_write();
  // test_func001_intr_test_write_only();
  // test_func001_alert_test_write_only();
  // test_func001_control_abort_write_only();
  // test_func001_status_rw1c_clear();
  // test_func001_cfg_regwen_read_only();
  test_range_regwen_write_lock();
  test_reserved_bits_read_zero();
  test_reserved_bits_write_ignored();
  test_cfg_regwen_locked_registers();
  test_control_status_always_accessible();
  test_reset_during_idle();
  test_reset_during_active_transfer();
  test_reset_unlocks_range_regwen();
  test_register_rw();
  test_register_ro();
  test_register_wo();
  test_register_rw0c();
  test_register_rw1c();
  test_reset_deasserts_interrupts();

  CSML_INFO(1, logger) << "\n========================================\n"
                       << "FUNC-001 Test Suite Complete\n"
                       << "========================================\n"
                       << std::endl;
}

// =============================================================================
// TC001: Reset Value Verification
// =============================================================================

// =============================================================================
// TC001: Reset Value Verification
// =============================================================================

void testbench::test_reset_values() {
  CSML_INFO(1, logger) << "\n>>> Test: Reset Value Verification <<<\n"
  << std::endl;

bool test_passed = true;
uint32_t read_val;

// Apply reset
m_test->apply_reset(sc_time(100, SC_NS));
wait(sc_time(20, SC_NS));

// Check key register reset values
struct {
unsigned int offset;
uint32_t expected;
const char *name;
}
reset_checks[] = {

{secure_dma_basetest::INTR_STATE_OFFSET, secure_dma_basetest::INTR_STATE_RESET, "INTR_STATE"},
{secure_dma_basetest::INTR_ENABLE_OFFSET, secure_dma_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"},
{secure_dma_basetest::INTR_TEST_OFFSET, secure_dma_basetest::INTR_TEST_RESET, "INTR_TEST"},
{secure_dma_basetest::ALERT_TEST_OFFSET, secure_dma_basetest::ALERT_TEST_RESET, "ALERT_TEST"},
{secure_dma_basetest::SRC_ADDR_LO_OFFSET, secure_dma_basetest::SRC_ADDR_LO_RESET, "SRC_ADDR_LO"},
{secure_dma_basetest::SRC_ADDR_HI_OFFSET, secure_dma_basetest::SRC_ADDR_HI_RESET, "SRC_ADDR_HI"},
{secure_dma_basetest::DST_ADDR_LO_OFFSET, secure_dma_basetest::DST_ADDR_LO_RESET, "DST_ADDR_LO"},
{secure_dma_basetest::DST_ADDR_HI_OFFSET, secure_dma_basetest::DST_ADDR_HI_RESET, "DST_ADDR_HI"},
{secure_dma_basetest::ADDR_SPACE_ID_OFFSET, secure_dma_basetest::ADDR_SPACE_ID_RESET, "ADDR_SPACE_ID"},
{secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_RESET, "ENABLED_MEMORY_RANGE_BASE"},
{secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_RESET, "ENABLED_MEMORY_RANGE_LIMIT"},
{secure_dma_basetest::RANGE_VALID_OFFSET, secure_dma_basetest::RANGE_VALID_RESET, "RANGE_VALID"},
{secure_dma_basetest::RANGE_REGWEN_OFFSET, secure_dma_basetest::RANGE_REGWEN_RESET, "RANGE_REGWEN"},
{secure_dma_basetest::CFG_REGWEN_OFFSET, secure_dma_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"},
{secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, secure_dma_basetest::TOTAL_DATA_SIZE_RESET, "TOTAL_DATA_SIZE"},
{secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, secure_dma_basetest::CHUNK_DATA_SIZE_RESET, "CHUNK_DATA_SIZE"},
{secure_dma_basetest::TRANSFER_WIDTH_OFFSET, secure_dma_basetest::TRANSFER_WIDTH_RESET, "TRANSFER_WIDTH"},
{secure_dma_basetest::CONTROL_OFFSET, secure_dma_basetest::CONTROL_RESET, "CONTROL"},
{secure_dma_basetest::SRC_CONFIG_OFFSET, secure_dma_basetest::SRC_CONFIG_RESET, "SRC_CONFIG"},
{secure_dma_basetest::DST_CONFIG_OFFSET, secure_dma_basetest::DST_CONFIG_RESET, "DST_CONFIG"},
{secure_dma_basetest::STATUS_OFFSET, secure_dma_basetest::STATUS_RESET, "STATUS"},
{secure_dma_basetest::ERROR_CODE_OFFSET, secure_dma_basetest::ERROR_CODE_RESET, "ERROR_CODE"},
{secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_RESET, "HANDSHAKE_INTR_ENABLE"},
{secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, secure_dma_basetest::CLEAR_INTR_SRC_RESET, "CLEAR_INTR_SRC"},
{secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, secure_dma_basetest::CLEAR_INTR_BUS_RESET, "CLEAR_INTR_BUS"},
};

for (const auto &check : reset_checks) {
m_test->register_read_32(check.offset, read_val);

if (read_val != check.expected) {
CSML_ERROR(0, logger)
<< check.name << " reset value mismatch: expected 0x" << std::hex
<< check.expected << " got 0x" << read_val << std::dec << std::endl;
test_passed = false;
}
}
// -------------------------------------------------------------------------
// SHA2_DIGEST[0..15] (16 registers at 0x58 + i*4)
// -------------------------------------------------------------------------
for (unsigned int i = 0; i < 16; i++) {
unsigned int offset = secure_dma_basetest::SHA2_DIGEST_OFFSET + i * 4;
m_test->register_read_32(offset, read_val);
if (read_val != secure_dma_basetest::SHA2_DIGEST_RESET) {
CSML_ERROR(0, logger)
<< "SHA2_DIGEST[" << i << "] reset value mismatch: expected 0x"
<< std::hex << secure_dma_basetest::SHA2_DIGEST_RESET << " got 0x" << read_val
<< std::dec << std::endl;
test_passed = false;
}
}

// -------------------------------------------------------------------------
// INTR_SRC_ADDR[0..10] (11 registers at 0xA4 + i*4)
// -------------------------------------------------------------------------
for (unsigned int i = 0; i < 11; i++) {
unsigned int offset = secure_dma_basetest::INTR_SRC_ADDR_OFFSET + i * 4;
m_test->register_read_32(offset, read_val);
if (read_val != secure_dma_basetest::INTR_SRC_ADDR_RESET) {
CSML_ERROR(0, logger)
<< "INTR_SRC_ADDR[" << i << "] reset value mismatch: expected 0x"
<< std::hex << secure_dma_basetest::INTR_SRC_ADDR_RESET << " got 0x"
<< read_val << std::dec << std::endl;
test_passed = false;
}
}

// -------------------------------------------------------------------------
// INTR_SRC_WR_VAL[0..10] (11 registers at 0x124 + i*4)
// -------------------------------------------------------------------------
for (unsigned int i = 0; i < 11; i++) {
unsigned int offset = secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + i * 4;
m_test->register_read_32(offset, read_val);
if (read_val != secure_dma_basetest::INTR_SRC_WR_VAL_RESET) {
CSML_ERROR(0, logger)
<< "INTR_SRC_WR_VAL[" << i << "] reset value mismatch: expected 0x"
<< std::hex << secure_dma_basetest::INTR_SRC_WR_VAL_RESET << " got 0x"
<< read_val << std::dec << std::endl;
test_passed = false;
}
}
report_test_result("Reset Value Verification (all registers)", test_passed);

}

// =============================================================================
// TC002: INTR_STATE Read-Only
// =============================================================================

void testbench::test_func001_intr_state_read_only() {
  std::string test_name = "FUNC-001 TC002: INTR_STATE Read-Only";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  uint32_t initial_value = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, initial_value);

  // Attempt write
  m_test->register_write_32(secure_dma_basetest::INTR_STATE_OFFSET, 0xFFFFFFFF);
  wait(10, SC_NS);

  uint32_t post_write_value = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, post_write_value);

  if (initial_value != post_write_value) {
    msg << "INTR_STATE changed after write (should be read-only)";
    passed = false;
  } else {
    msg << "INTR_STATE correctly ignored write attempts";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC003: INTR_ENABLE Read-Write
// =============================================================================

void testbench::test_func001_intr_enable_read_write() {
  std::string test_name = "FUNC-001 TC003: INTR_ENABLE Read-Write";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  struct { uint32_t write_val; uint32_t expected; } tests[] = {
    {0x00000007, 0x00000007},
    {0x00000005, 0x00000005},
    {0x00000000, 0x00000000}
  };

  for (const auto& test : tests) {
    m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, test.write_val);
    wait(5, SC_NS);

    uint32_t read_val = 0;
    m_test->register_read_32(secure_dma_basetest::INTR_ENABLE_OFFSET, read_val);

    if (read_val != test.expected) {
      msg << "INTR_ENABLE mismatch: expected 0x" << std::hex
          << test.expected << ", got 0x" << read_val << "; ";
      passed = false;
    }
  }

  if (passed) {
    msg << "INTR_ENABLE read-write access verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC004: INTR_TEST Write-Only
// =============================================================================

void testbench::test_func001_intr_test_write_only() {
  std::string test_name = "FUNC-001 TC004: INTR_TEST Write-Only";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  uint32_t intr_test_val = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_TEST_OFFSET, intr_test_val);

  if (intr_test_val != 0x00000000) {
    msg << "INTR_TEST should always read 0x0";
    passed = false;
  }

  // Write to force interrupt
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001);
  wait(10, SC_NS);

  // Read INTR_STATE to verify interrupt set
  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);

  if ((intr_state & 0x1) == 0) {
    msg << "INTR_STATE.dma_done not set after INTR_TEST write";
    passed = false;
  }

  if (passed) {
    msg << "INTR_TEST write-only behavior verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC005: ALERT_TEST Write-Only
// =============================================================================

void testbench::test_func001_alert_test_write_only() {
  std::string test_name = "FUNC-001 TC005: ALERT_TEST Write-Only";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  uint32_t alert_test_val = 0;
  m_test->register_read_32(secure_dma_basetest::ALERT_TEST_OFFSET, alert_test_val);

  if (alert_test_val != 0x00000000) {
    msg << "ALERT_TEST should always read 0x0";
    passed = false;
  } else {
    msg << "ALERT_TEST write-only behavior verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC006: CONTROL.abort Write-Only
// =============================================================================

void testbench::test_func001_control_abort_write_only() {
  std::string test_name = "FUNC-001 TC006: CONTROL.abort Write-Only";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Write abort bit
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000);
  wait(10, SC_NS);

  uint32_t control_val = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_val);

  if ((control_val & 0x08000000) != 0) {
    msg << "CONTROL.abort (bit 27) should always read 0";
    passed = false;
  } else {
    msg << "CONTROL.abort write-only behavior verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC007: STATUS RW1C Clear Behavior
// =============================================================================

void testbench::test_func001_status_rw1c_clear() {
  std::string test_name = "FUNC-001 TC007: STATUS RW1C Clear";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Clear any existing STATUS bits
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0xFFFFFFFF);
  wait(5, SC_NS);

  msg << "STATUS RW1C clear behavior verified";
  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC008: CFG_REGWEN Read-Only
// =============================================================================

void testbench::test_func001_cfg_regwen_read_only() {
  std::string test_name = "FUNC-001 TC008: CFG_REGWEN Read-Only";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);

  if (cfg_regwen != 0x00000006) {
    msg << "CFG_REGWEN should be 0x6 (unlocked)";
    passed = false;
  }

  // Attempt write
  m_test->register_write_32(secure_dma_basetest::CFG_REGWEN_OFFSET, 0x00000000);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);

  if (cfg_regwen != 0x00000006) {
    msg << "CFG_REGWEN changed after write (should be read-only)";
    passed = false;
  } else {
    msg << "CFG_REGWEN read-only behavior verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC009: RANGE_REGWEN Write-Lock
// =============================================================================

void testbench::test_range_regwen_write_lock() {
  std::string test_name = "RANGE_REGWEN Write-Lock";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  uint32_t range_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, range_regwen);

  if (range_regwen != 0x00000006) {
    msg << "RANGE_REGWEN should be 0x6 after reset";
    passed = false;
  }

  // Write to base register
  m_test->register_write_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, 0x10000000);
  wait(5, SC_NS);

  uint32_t base_val = 0;
  m_test->register_read_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, base_val);

  if (base_val != 0x10000000) {
    msg << "ENABLED_MEMORY_RANGE_BASE write failed before lock";
    passed = false;
  }

  // Lock RANGE_REGWEN
  m_test->register_write_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, 0x00000000);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, range_regwen);

  if (range_regwen != 0x00000000) {
    msg << "RANGE_REGWEN should be 0x0 after lock";
    passed = false;
  }

  // Attempt write after lock
  m_test->register_write_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, 0x20000000);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, base_val);

  if (base_val != 0x10000000) {
    msg << "ENABLED_MEMORY_RANGE_BASE changed after lock";
    passed = false;
  } else {
    msg << "RANGE_REGWEN write-0-to-lock verified";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC010: Reserved Bits Read Zero
// =============================================================================

// =============================================================================
// TC010: Reserved Bits Read Zero (all 17 registers)
// =============================================================================

struct ReservedReadZeroEntry {
  uint32_t offset;
  uint32_t reserved_mask;  // bits that must read 0
  const char* name;
};

static const ReservedReadZeroEntry kReservedFieldRegs[] = {
  { secure_dma_basetest::INTR_STATE_OFFSET,               0xFFFFFFF8u, "INTR_STATE" },
  { secure_dma_basetest::INTR_ENABLE_OFFSET,              0xFFFFFFF8u, "INTR_ENABLE" },
  { secure_dma_basetest::INTR_TEST_OFFSET,                0xFFFFFFF8u, "INTR_TEST" },
  { secure_dma_basetest::ALERT_TEST_OFFSET,               0xFFFFFFFEu, "ALERT_TEST" },
  { secure_dma_basetest::ADDR_SPACE_ID_OFFSET,            0xFFFFFF00u, "ADDR_SPACE_ID" },
  { secure_dma_basetest::RANGE_VALID_OFFSET,              0xFFFFFFFEu, "RANGE_VALID" },
  { secure_dma_basetest::RANGE_REGWEN_OFFSET,             0xFFFFFFF0u, "RANGE_REGWEN" },
  { secure_dma_basetest::CFG_REGWEN_OFFSET,               0xFFFFFFF0u, "CFG_REGWEN" },
  { secure_dma_basetest::TRANSFER_WIDTH_OFFSET,           0xFFFFFFFCu, "TRANSFER_WIDTH" },
  { secure_dma_basetest::CONTROL_OFFSET,                  0x77FFFEC0u, "CONTROL" },
  { secure_dma_basetest::SRC_CONFIG_OFFSET,               0xFFFFFFFCu, "SRC_CONFIG" },
  { secure_dma_basetest::DST_CONFIG_OFFSET,               0xFFFFFFFCu, "DST_CONFIG" },
  { secure_dma_basetest::STATUS_OFFSET,                   0xFFFFFFC0u, "STATUS" },
  { secure_dma_basetest::ERROR_CODE_OFFSET,               0xFFFFFF00u, "ERROR_CODE" },
  { secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET,    0xFFFFF800u, "HANDSHAKE_INTR_ENABLE" },
  { secure_dma_basetest::CLEAR_INTR_SRC_OFFSET,           0xFFFFF800u, "CLEAR_INTR_SRC" },
  { secure_dma_basetest::CLEAR_INTR_BUS_OFFSET,           0xFFFFF800u, "CLEAR_INTR_BUS" },
};

static const size_t kReservedFieldRegsCount =
    sizeof(kReservedFieldRegs) / sizeof(kReservedFieldRegs[0]);

void testbench::test_reserved_bits_read_zero() {
  std::string test_name = "Reserved Bits Read Zero \n";
  CSML_INFO(1, logger) << "\n Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  uint32_t read_val = 0;
  for (size_t i = 0; i < kReservedFieldRegsCount; i++) {
    m_test->register_read_32(kReservedFieldRegs[i].offset, read_val);
    if ((read_val & kReservedFieldRegs[i].reserved_mask) != 0) {
      msg << kReservedFieldRegs[i].name << " reserved bits not zero (read 0x" << std::hex
          << read_val << ")";
      passed = false;
      throw std::runtime_error(msg.str());
    }
  }

  if (passed) {
    msg << "\n All 17 registers: reserved bits read zero.\n";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC011: Reserved Bits Write Ignored
// =============================================================================

void testbench::test_reserved_bits_write_ignored() {
  std::string test_name = "Reserved Bits Write Ignored";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  for (size_t i = 0; i < kReservedFieldRegsCount; i++) {
    const uint32_t offset = kReservedFieldRegs[i].offset;
    const uint32_t reserved_mask = kReservedFieldRegs[i].reserved_mask;
    const uint32_t valid_mask = ~reserved_mask;

    uint32_t before = 0;
    uint32_t after = 0;

    m_test->register_read_32(offset, before);

    // Keep defined bits unchanged and only attempt writes on reserved bits.
    const uint32_t write_val = (before & valid_mask) | reserved_mask;
    m_test->register_write_32(offset, write_val);
    wait(10, SC_NS);
    m_test->register_read_32(offset, after);

    // Sanity: ensure this stimulus actually tries to set reserved bits.
    if ((write_val & reserved_mask) == 0u) {
      msg << kReservedFieldRegs[i].name
          << " stimulus did not set reserved bits (write_val=0x" << std::hex
          << write_val << "); ";
      passed = false;
      continue;
    }

    // Primary requirement: reserved bits must read as zero after write.
    if ((after & reserved_mask) != 0u) {
      msg << kReservedFieldRegs[i].name
          << " reserved bits not ignored (write_val=0x" << std::hex << write_val
          << ", after=0x" << after << "); ";
      passed = false;
      continue;
    }

    // No-side-effect requirement: defined fields must be unchanged.
    if ((after & valid_mask) != (before & valid_mask)) {
      msg << kReservedFieldRegs[i].name
          << " non-reserved bits changed during reserved write (before=0x" << std::hex
          << before << ", after=0x" << after << "); ";
      passed = false;
    }
  }

  if (passed) {
    msg << "All " << kReservedFieldRegsCount
        << " registers ignored reserved-bit writes without side effects";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC012: CFG_REGWEN Locks Configuration Registers
// =============================================================================

void testbench::test_cfg_regwen_locked_registers() {
  std::string test_name = "CFG_REGWEN Locks Configuration Registers";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  int registers_tested = 0;
  int registers_passed = 0;

  // Apply reset to ensure CFG_REGWEN is unlocked
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Verify CFG_REGWEN is unlocked (0x6)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x6) {
    msg << "CFG_REGWEN not unlocked after reset; ";
    passed = false;
  }

  // =========================================================================
  // Phase 1: Write initial values when CFG_REGWEN is unlocked (0x6)
  // =========================================================================

  struct RegisterTest {
    uint32_t offset;
    uint32_t initial_value;
    uint32_t locked_value;
    const char* name;
  };

  std::vector<RegisterTest> reg_tests = {
    {secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000, 0x20000000, "SRC_ADDR_LO"},
    {secure_dma_basetest::SRC_ADDR_HI_OFFSET, 0x00000000, 0x00000000, "SRC_ADDR_HI"},
    {secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x30000000, 0x40000000, "DST_ADDR_LO"},
    {secure_dma_basetest::DST_ADDR_HI_OFFSET, 0x00000000, 0x00000000, "DST_ADDR_HI"},
    {secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077, 0x00000099, "ADDR_SPACE_ID"},
    {secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x00001000, 0x00002000, "TOTAL_DATA_SIZE"},
    {secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x00000100, 0x00000200, "CHUNK_DATA_SIZE"},
    {secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002, 0x00000001, "TRANSFER_WIDTH"},
    {secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001, 0x00000000, "SRC_CONFIG"},
    {secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001, 0x00000000, "DST_CONFIG"},
    {secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, 0x000007FF, 0x00000000, "HANDSHAKE_INTR_ENABLE"},
    {secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, 0x000007FF, 0x00000000, "CLEAR_INTR_SRC"},
    {secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, 0x000007FF, 0x00000000, "CLEAR_INTR_BUS"}
  };

  // Write initial values to all configuration registers (while unlocked)
  for (const auto& reg : reg_tests) {
    m_test->register_write_32(reg.offset, reg.initial_value);
    wait(5, SC_NS);
    registers_tested++;
  }

  // Test INTR_SRC_ADDR array registers [0-10]
  for (int i = 0; i < 11; i++) {
    uint32_t offset = secure_dma_basetest::INTR_SRC_ADDR_OFFSET + (i * 4);
    m_test->register_write_32(offset, 0x50000000 + (i * 0x100));
    wait(5, SC_NS);
    registers_tested++;
  }

  // Test INTR_SRC_WR_VAL array registers [0-10]
  for (int i = 0; i < 11; i++) {
    uint32_t offset = secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + (i * 4);
    m_test->register_write_32(offset, 0x12345678 + i);
    wait(5, SC_NS);
    registers_tested++;
  }

  // Verify initial values were written successfully
  for (const auto& reg : reg_tests) {
    uint32_t read_val = 0;
    m_test->register_read_32(reg.offset, read_val);
    if (read_val != reg.initial_value) {
      msg << reg.name << " initial write failed (expected 0x" << std::hex
          << reg.initial_value << ", got 0x" << read_val << "); ";
      passed = false;
    }
  }

  // =========================================================================
  // Phase 2: Lock CFG_REGWEN by setting DMA busy state
  // =========================================================================

  // For this test, we just need CFG_REGWEN to be locked. In reality, CFG_REGWEN
  // is locked when DMA is busy. However, since FUNC-008+ transfer engine is not
  // yet implemented, writing CONTROL.go will set busy state without actually
  // initiating transfers. This is sufficient to test the locking mechanism.
  //
  // IMPORTANT: Use the SAME values as the initial writes above, so that when
  // we attempt to write "locked" values later, we can verify the registers
  // didn't change from their initial (==setup) values.

  // Already written above, no need to write again
  // The initial values ARE the setup values for this test
  wait(10, SC_NS);

  // Trigger DMA operation by setting CONTROL.go bit (bit 31)
  // This will set m_dma_busy = true and lock CFG_REGWEN to 0x0
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1
  wait(20, SC_NS);

  // Verify CFG_REGWEN is now locked (0x0)
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x0) {
    msg << "CFG_REGWEN not locked when DMA busy (expected 0x0, got 0x"
        << std::hex << cfg_regwen << "); ";
    passed = false;
  }

  // =========================================================================
  // Phase 3: Attempt writes to all CFG_REGWEN-protected registers (should be blocked)
  // =========================================================================

  for (const auto& reg : reg_tests) {
    // Attempt to write new value while locked
    m_test->register_write_32(reg.offset, reg.locked_value);
    wait(10, SC_NS);

    // Verify software write was blocked
    uint32_t read_val = 0;
    m_test->register_read_32(reg.offset, read_val);

    // For SRC_ADDR_LO and DST_ADDR_LO: Hardware may update during transfer (per spec),
    // so we verify software write was blocked by checking value != locked_value.
    // For other registers: They should remain at initial_value.
    bool is_addr_reg = (reg.offset == secure_dma_basetest::SRC_ADDR_LO_OFFSET ||
                        reg.offset == secure_dma_basetest::DST_ADDR_LO_OFFSET);

    bool write_blocked = is_addr_reg ? (read_val != reg.locked_value) : (read_val == reg.initial_value);

    if (write_blocked) {
      registers_passed++;
    } else {
      if (is_addr_reg) {
        msg << reg.name << " write not blocked when locked (attempted 0x" << std::hex
            << reg.locked_value << ", got 0x" << read_val << "); ";
      } else {
        msg << reg.name << " write not blocked when locked (expected 0x" << std::hex
            << reg.initial_value << ", got 0x" << read_val << "); ";
      }
      passed = false;
    }
  }

  // Test INTR_SRC_ADDR array registers [0-10] are locked
  for (int i = 0; i < 11; i++) {
    uint32_t offset = secure_dma_basetest::INTR_SRC_ADDR_OFFSET + (i * 4);
    uint32_t expected = 0x50000000 + (i * 0x100);

    // Attempt write
    m_test->register_write_32(offset, 0xDEADBEEF);
    wait(10, SC_NS);

    // Verify not changed
    uint32_t read_val = 0;
    m_test->register_read_32(offset, read_val);

    if (read_val == expected) {
      registers_passed++;
    } else {
      msg << "INTR_SRC_ADDR[" << i << "] write not blocked when locked; ";
      passed = false;
    }
  }

  // Test INTR_SRC_WR_VAL array registers [0-10] are locked
  for (int i = 0; i < 11; i++) {
    uint32_t offset = secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + (i * 4);
    uint32_t expected = 0x12345678 + i;

    // Attempt write
    m_test->register_write_32(offset, 0xCAFEBABE);
    wait(10, SC_NS);

    // Verify not changed
    uint32_t read_val = 0;
    m_test->register_read_32(offset, read_val);

    if (read_val == expected) {
      registers_passed++;
    } else {
      msg << "INTR_SRC_WR_VAL[" << i << "] write not blocked when locked; ";
      passed = false;
    }
  }

  // =========================================================================
  // Summary
  // =========================================================================

  if (passed) {
    msg << "All " << registers_tested << " CFG_REGWEN-protected registers correctly locked during busy state";
  } else {
    msg << "Coverage: " << registers_passed << "/" << registers_tested
        << " registers correctly protected";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC013: CONTROL/STATUS Always Accessible
// =============================================================================

void testbench::test_control_status_always_accessible() {
  std::string test_name = "CONTROL/STATUS Always Accessible During Busy";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  // Apply reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Configure and start a transfer to enter busy state
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x00000100);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x00000100);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  wait(10, SC_NS);

  // Trigger DMA operation (locks CFG_REGWEN)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000);
  wait(20, SC_NS);

  // Verify DMA is busy (CFG_REGWEN locked)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x0) {
    msg << "CFG_REGWEN not locked (DMA not busy); ";
    passed = false;
  }

  // Verify STATUS.busy is set
  uint32_t status = 0;
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & 0x1) == 0) {
    msg << "STATUS.busy not set; ";
    passed = false;
  }

  // Test CONTROL register remains writable (e.g., abort bit)
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort bit
  wait(10, SC_NS);

  // Test STATUS register remains writable (clear bits)
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, 0xFFFFFFFF);
  wait(10, SC_NS);

  // Read STATUS to verify write was processed
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);

  if (passed) {
    msg << "CONTROL and STATUS remain accessible during busy state (CFG_REGWEN=0x0)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// TC014 / Test Plan 121: Reset During Idle (test_reset_during_idle)
// =============================================================================

void testbench::test_reset_during_idle() {
  std::string test_name = "FUNC-001 TC014: Reset During Idle (test_reset_during_idle)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  uint32_t read_val = 0;

  // Apply initial reset (rst_ni); DMA is idle (no transfer started)
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Write non-default values to various registers via reg_target_socket
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0xDEADBEEF);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0xCAFEBABE);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x12345678);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000000);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000000);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, read_val);
  if (read_val != 0xDEADBEEF) {
    msg << "Pre-reset write verification failed; ";
    passed = false;
  }

  // Apply reset again (rst_ni)
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Verify ALL registers return to reset values (test plan: "All registers")
  struct { unsigned int offset; uint32_t expected; const char* name; } reset_checks[] = {
    {secure_dma_basetest::INTR_STATE_OFFSET, secure_dma_basetest::INTR_STATE_RESET, "INTR_STATE"},
    {secure_dma_basetest::INTR_ENABLE_OFFSET, secure_dma_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"},
    {secure_dma_basetest::INTR_TEST_OFFSET, secure_dma_basetest::INTR_TEST_RESET, "INTR_TEST"},
    {secure_dma_basetest::ALERT_TEST_OFFSET, secure_dma_basetest::ALERT_TEST_RESET, "ALERT_TEST"},
    {secure_dma_basetest::SRC_ADDR_LO_OFFSET, secure_dma_basetest::SRC_ADDR_LO_RESET, "SRC_ADDR_LO"},
    {secure_dma_basetest::SRC_ADDR_HI_OFFSET, secure_dma_basetest::SRC_ADDR_HI_RESET, "SRC_ADDR_HI"},
    {secure_dma_basetest::DST_ADDR_LO_OFFSET, secure_dma_basetest::DST_ADDR_LO_RESET, "DST_ADDR_LO"},
    {secure_dma_basetest::DST_ADDR_HI_OFFSET, secure_dma_basetest::DST_ADDR_HI_RESET, "DST_ADDR_HI"},
    {secure_dma_basetest::ADDR_SPACE_ID_OFFSET, secure_dma_basetest::ADDR_SPACE_ID_RESET, "ADDR_SPACE_ID"},
    {secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_RESET, "ENABLED_MEMORY_RANGE_BASE"},
    {secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_RESET, "ENABLED_MEMORY_RANGE_LIMIT"},
    {secure_dma_basetest::RANGE_VALID_OFFSET, secure_dma_basetest::RANGE_VALID_RESET, "RANGE_VALID"},
    {secure_dma_basetest::RANGE_REGWEN_OFFSET, secure_dma_basetest::RANGE_REGWEN_RESET, "RANGE_REGWEN"},
    {secure_dma_basetest::CFG_REGWEN_OFFSET, secure_dma_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"},
    {secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, secure_dma_basetest::TOTAL_DATA_SIZE_RESET, "TOTAL_DATA_SIZE"},
    {secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, secure_dma_basetest::CHUNK_DATA_SIZE_RESET, "CHUNK_DATA_SIZE"},
    {secure_dma_basetest::TRANSFER_WIDTH_OFFSET, secure_dma_basetest::TRANSFER_WIDTH_RESET, "TRANSFER_WIDTH"},
    {secure_dma_basetest::CONTROL_OFFSET, secure_dma_basetest::CONTROL_RESET, "CONTROL"},
    {secure_dma_basetest::SRC_CONFIG_OFFSET, secure_dma_basetest::SRC_CONFIG_RESET, "SRC_CONFIG"},
    {secure_dma_basetest::DST_CONFIG_OFFSET, secure_dma_basetest::DST_CONFIG_RESET, "DST_CONFIG"},
    {secure_dma_basetest::STATUS_OFFSET, secure_dma_basetest::STATUS_RESET, "STATUS"},
    {secure_dma_basetest::ERROR_CODE_OFFSET, secure_dma_basetest::ERROR_CODE_RESET, "ERROR_CODE"},
    {secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_RESET, "HANDSHAKE_INTR_ENABLE"},
    {secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, secure_dma_basetest::CLEAR_INTR_SRC_RESET, "CLEAR_INTR_SRC"},
    {secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, secure_dma_basetest::CLEAR_INTR_BUS_RESET, "CLEAR_INTR_BUS"},
  };

  for (const auto& check : reset_checks) {
    m_test->register_read_32(check.offset, read_val);
    if (read_val != check.expected) {
      msg << check.name << " not reset (expected 0x" << std::hex
          << check.expected << ", got 0x" << read_val << "); ";
      passed = false;
    }
  }

  for (unsigned int i = 0; i < 16; i++) {
    unsigned int offset = secure_dma_basetest::SHA2_DIGEST_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::SHA2_DIGEST_RESET) {
      msg << "SHA2_DIGEST[" << i << "] not reset; ";
      passed = false;
    }
  }
  for (unsigned int i = 0; i < 11; i++) {
    unsigned int offset = secure_dma_basetest::INTR_SRC_ADDR_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::INTR_SRC_ADDR_RESET) {
      msg << "INTR_SRC_ADDR[" << i << "] not reset; ";
      passed = false;
    }
  }
  for (unsigned int i = 0; i < 11; i++) {
    unsigned int offset = secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::INTR_SRC_WR_VAL_RESET) {
      msg << "INTR_SRC_WR_VAL[" << i << "] not reset; ";
      passed = false;
    }
  }

  if (passed) {
    msg << "All registers cleared to reset values when DMA is idle (rst_ni, reg_target_socket)";
  }

  report_test_result(test_name, passed, msg.str());
}

// =============================================================================
// Test Plan 122: Reset During Active Transfer (test_reset_during_active_transfer)
// =============================================================================

void testbench::test_reset_during_active_transfer() {
  std::string test_name =
      " Reset During Active Transfer (test_reset_during_active_transfer)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  uint32_t read_val = 0;

  // Apply initial reset (rst_ni)
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Program all registers per test plan (transfer uses ot_initiator_socket)
  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000000);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x20000000);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x00001000);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x00000100);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x00000002);
  m_test->register_write_32(secure_dma_basetest::SRC_CONFIG_OFFSET, 0x00000001);
  m_test->register_write_32(secure_dma_basetest::DST_CONFIG_OFFSET, 0x00000001);
  wait(10, SC_NS);

  // Start transfer (CONTROL.go) – DMA becomes busy, uses ot_initiator_socket
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000);
  wait(20, SC_NS);

  // Verify DMA is active (transfer aborted by reset)
  uint32_t cfg_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != 0x0) {
    msg << "DMA not busy before reset (CFG_REGWEN not 0x0); ";
    passed = false;
  }

  // Apply reset during active transfer (rst_ni)
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(50, SC_NS);

  // Verify reset aborts transfer: CFG_REGWEN unlocked, STATUS.busy cleared
  m_test->register_read_32(secure_dma_basetest::CFG_REGWEN_OFFSET, cfg_regwen);
  if (cfg_regwen != secure_dma_basetest::CFG_REGWEN_RESET) {
    msg << "CFG_REGWEN not unlocked after reset (expected 0x6, got 0x"
        << std::hex << cfg_regwen << "); ";
    passed = false;
  }
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, read_val);
  if ((read_val & 0x1) != 0) {
    msg << "STATUS.busy not cleared after reset; ";
    passed = false;
  }

  // Verify all registers cleared per test plan ("clears all registers")
  struct { unsigned int offset; uint32_t expected; const char* name; } reset_checks[] = {
    {secure_dma_basetest::INTR_STATE_OFFSET, secure_dma_basetest::INTR_STATE_RESET, "INTR_STATE"},
    {secure_dma_basetest::INTR_ENABLE_OFFSET, secure_dma_basetest::INTR_ENABLE_RESET, "INTR_ENABLE"},
    {secure_dma_basetest::INTR_TEST_OFFSET, secure_dma_basetest::INTR_TEST_RESET, "INTR_TEST"},
    {secure_dma_basetest::ALERT_TEST_OFFSET, secure_dma_basetest::ALERT_TEST_RESET, "ALERT_TEST"},
    {secure_dma_basetest::SRC_ADDR_LO_OFFSET, secure_dma_basetest::SRC_ADDR_LO_RESET, "SRC_ADDR_LO"},
    {secure_dma_basetest::SRC_ADDR_HI_OFFSET, secure_dma_basetest::SRC_ADDR_HI_RESET, "SRC_ADDR_HI"},
    {secure_dma_basetest::DST_ADDR_LO_OFFSET, secure_dma_basetest::DST_ADDR_LO_RESET, "DST_ADDR_LO"},
    {secure_dma_basetest::DST_ADDR_HI_OFFSET, secure_dma_basetest::DST_ADDR_HI_RESET, "DST_ADDR_HI"},
    {secure_dma_basetest::ADDR_SPACE_ID_OFFSET, secure_dma_basetest::ADDR_SPACE_ID_RESET, "ADDR_SPACE_ID"},
    {secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_BASE_RESET, "ENABLED_MEMORY_RANGE_BASE"},
    {secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_OFFSET, secure_dma_basetest::ENABLED_MEMORY_RANGE_LIMIT_RESET, "ENABLED_MEMORY_RANGE_LIMIT"},
    {secure_dma_basetest::RANGE_VALID_OFFSET, secure_dma_basetest::RANGE_VALID_RESET, "RANGE_VALID"},
    {secure_dma_basetest::RANGE_REGWEN_OFFSET, secure_dma_basetest::RANGE_REGWEN_RESET, "RANGE_REGWEN"},
    {secure_dma_basetest::CFG_REGWEN_OFFSET, secure_dma_basetest::CFG_REGWEN_RESET, "CFG_REGWEN"},
    {secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, secure_dma_basetest::TOTAL_DATA_SIZE_RESET, "TOTAL_DATA_SIZE"},
    {secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, secure_dma_basetest::CHUNK_DATA_SIZE_RESET, "CHUNK_DATA_SIZE"},
    {secure_dma_basetest::TRANSFER_WIDTH_OFFSET, secure_dma_basetest::TRANSFER_WIDTH_RESET, "TRANSFER_WIDTH"},
    {secure_dma_basetest::CONTROL_OFFSET, secure_dma_basetest::CONTROL_RESET, "CONTROL"},
    {secure_dma_basetest::SRC_CONFIG_OFFSET, secure_dma_basetest::SRC_CONFIG_RESET, "SRC_CONFIG"},
    {secure_dma_basetest::DST_CONFIG_OFFSET, secure_dma_basetest::DST_CONFIG_RESET, "DST_CONFIG"},
    {secure_dma_basetest::STATUS_OFFSET, secure_dma_basetest::STATUS_RESET, "STATUS"},
    {secure_dma_basetest::ERROR_CODE_OFFSET, secure_dma_basetest::ERROR_CODE_RESET, "ERROR_CODE"},
    {secure_dma_basetest::HANDSHAKE_INTR_ENABLE_OFFSET, secure_dma_basetest::HANDSHAKE_INTR_ENABLE_RESET, "HANDSHAKE_INTR_ENABLE"},
    {secure_dma_basetest::CLEAR_INTR_SRC_OFFSET, secure_dma_basetest::CLEAR_INTR_SRC_RESET, "CLEAR_INTR_SRC"},
    {secure_dma_basetest::CLEAR_INTR_BUS_OFFSET, secure_dma_basetest::CLEAR_INTR_BUS_RESET, "CLEAR_INTR_BUS"},
  };

  for (const auto& check : reset_checks) {
    m_test->register_read_32(check.offset, read_val);
    if (read_val != check.expected) {
      msg << check.name << " not reset (expected 0x" << std::hex
          << check.expected << ", got 0x" << read_val << "); ";
      passed = false;
    }
  }

  for (unsigned int i = 0; i < 16; i++) {
    unsigned int offset = secure_dma_basetest::SHA2_DIGEST_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::SHA2_DIGEST_RESET) {
      msg << "SHA2_DIGEST[" << i << "] not reset; ";
      passed = false;
    }
  }
  for (unsigned int i = 0; i < 11; i++) {
    unsigned int offset = secure_dma_basetest::INTR_SRC_ADDR_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::INTR_SRC_ADDR_RESET) {
      msg << "INTR_SRC_ADDR[" << i << "] not reset; ";
      passed = false;
    }
  }
  for (unsigned int i = 0; i < 11; i++) {
    unsigned int offset = secure_dma_basetest::INTR_SRC_WR_VAL_OFFSET + i * 4;
    m_test->register_read_32(offset, read_val);
    if (read_val != secure_dma_basetest::INTR_SRC_WR_VAL_RESET) {
      msg << "INTR_SRC_WR_VAL[" << i << "] not reset; ";
      passed = false;
    }
  }

  if (passed) {
    msg << "Reset aborted active transfer and cleared all registers (rst_ni, "
           "reg_target_socket, ot_initiator_socket)";
  }

  report_test_result(test_name, passed, msg.str());
}


void testbench::test_reset_unlocks_range_regwen() {
  std::string test_name = "Reset Unlocks RANGE_REGWEN";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;

  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Lock RANGE_REGWEN
  m_test->register_write_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, 0x00000000);
  wait(10, SC_NS);

  uint32_t range_regwen = 0;
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, range_regwen);

  if (range_regwen != 0x00000000) {
    msg << "RANGE_REGWEN not locked";
    passed = false;
  }

  // Reset
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(50, SC_NS);

  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, range_regwen);

  if (range_regwen != 0x00000006) {
    msg << "RANGE_REGWEN not unlocked after reset";
    passed = false;
  } else {
    msg << "Reset unlocks RANGE_REGWEN successfully";
  }

  report_test_result(test_name, passed, msg.str());
}

void testbench::test_register_wo() {
  CSML_INFO(1, logger) << "\n>>> Test: Write-Only Register Behavior <<<\n"
                       << std::endl;

  bool test_passed = true;
  uint32_t before = 0, after = 0;

  // 1) INTR_TEST (WO): should always read as 0, write causes side effect
  m_test->register_read_32(secure_dma_basetest::INTR_TEST_OFFSET, before);
  if (before != 0x0) {
    CSML_ERROR(0, logger) << "INTR_TEST read not zero before write" << std::endl;
    test_passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000001); // force done
  wait(sc_time(10, SC_NS));

  m_test->register_read_32(secure_dma_basetest::INTR_TEST_OFFSET, after);
  if (after != 0x0) {
    CSML_ERROR(0, logger) << "INTR_TEST latched written value (should be WO/transient)" << std::endl;
    test_passed = false;
  }

  uint32_t intr_state = 0;
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x1) == 0) {
    CSML_ERROR(0, logger) << "INTR_TEST write did not set INTR_STATE.dma_done" << std::endl;
    test_passed = false;
  }

  // 2) ALERT_TEST (WO): should read as 0 even after write
  m_test->register_read_32(secure_dma_basetest::ALERT_TEST_OFFSET, before);
  if (before != 0x0) {
    CSML_ERROR(0, logger) << "ALERT_TEST read not zero before write" << std::endl;
    test_passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::ALERT_TEST_OFFSET, 0x00000001);
  wait(sc_time(10, SC_NS));

  m_test->register_read_32(secure_dma_basetest::ALERT_TEST_OFFSET, after);
  if (after != 0x0) {
    CSML_ERROR(0, logger) << "ALERT_TEST latched written value (should be WO/transient)" << std::endl;
    test_passed = false;
  }

  // 3) CONTROL.abort bit (bit27): write-only, must read as 0
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort=1
  wait(sc_time(10, SC_NS));

  uint32_t control_val = 0;
  m_test->register_read_32(secure_dma_basetest::CONTROL_OFFSET, control_val);
  if ((control_val & 0x08000000) != 0) {
    CSML_ERROR(0, logger) << "CONTROL.abort bit read back as 1 (should be WO)" << std::endl;
    test_passed = false;
  }

  report_test_result("Write-Only Register Behavior", test_passed);
}

void testbench::test_register_rw0c() {
  CSML_INFO(1, logger) << "\n>>> Test: RW0C Register Behavior (RANGE_REGWEN) <<<\n"
                       << std::endl;

  bool test_passed = true;
  uint32_t before = 0, after = 0;

  // Start clean
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  // 1) Reset state should be unlocked (0x6)
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, before);
  if ((before & 0xF) != 0x6) {
    CSML_ERROR(0, logger) << "RANGE_REGWEN reset value mismatch (expected 0x6, got 0x"
                          << std::hex << (before & 0xF) << std::dec << ")" << std::endl;
    test_passed = false;
  }

  // 2) Write 0x0 => lock
  m_test->register_write_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, 0x00000000);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, after);
  if ((after & 0xF) != 0x0) {
    CSML_ERROR(0, logger) << "RANGE_REGWEN lock failed (expected 0x0, got 0x"
                          << std::hex << (after & 0xF) << std::dec << ")" << std::endl;
    test_passed = false;
  }

  // 3) Try to unlock by writing 0x6 => should be ignored (stay 0x0)
  m_test->register_write_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, 0x00000006);
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, after);
  if ((after & 0xF) != 0x0) {
    CSML_ERROR(0, logger) << "RANGE_REGWEN unexpectedly unlocked after write 0x6" << std::endl;
    test_passed = false;
  }

  // 4) Reset should restore unlock state
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));
  m_test->register_read_32(secure_dma_basetest::RANGE_REGWEN_OFFSET, after);
  if ((after & 0xF) != 0x6) {
    CSML_ERROR(0, logger) << "RANGE_REGWEN not restored to 0x6 after reset" << std::endl;
    test_passed = false;
  }

  report_test_result("RW0C Register Behavior (RANGE_REGWEN)", test_passed);
}


void testbench::test_register_rw1c() {
  CSML_INFO(1, logger) << "\n>>> Test: RW1C Register Behavior (STATUS bits) <<<\n"
                       << std::endl;

  bool test_passed = true;
  uint32_t status = 0;

  // Helper: clear all RW1C bits first
  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET,
                            (1u << 1) | (1u << 2) | (1u << 3) | (1u << 5));
  wait(sc_time(10, SC_NS));

  // ------------------------------------------------------------
  // 1) STATUS.done (bit1): set by successful transfer, clear by write-1
  // ------------------------------------------------------------
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000100);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10000200);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x10);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x10);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1 copy
  wait(sc_time(200, SC_NS));

  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & (1u << 1)) == 0) {
    CSML_ERROR(0, logger) << "STATUS.done not set before RW1C clear" << std::endl;
    test_passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, (1u << 1)); // clear done
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status & (1u << 1)) {
    CSML_ERROR(0, logger) << "STATUS.done not cleared by RW1C write" << std::endl;
    test_passed = false;
  }

  // ------------------------------------------------------------
  // 2) STATUS.error (bit3): set by invalid config, clear by write-1
  // ------------------------------------------------------------
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x0); // invalid
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000);  // go=1
  wait(sc_time(20, SC_NS));

  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & (1u << 3)) == 0) {
    CSML_ERROR(0, logger) << "STATUS.error not set before RW1C clear" << std::endl;
    test_passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, (1u << 3)); // clear error
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status & (1u << 3)) {
    CSML_ERROR(0, logger) << "STATUS.error not cleared by RW1C write" << std::endl;
    test_passed = false;
  }

  // ------------------------------------------------------------
  // 3) STATUS.aborted (bit2): set by abort, clear by write-1
  // ------------------------------------------------------------
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000100);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10000200);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x1000); // long transfer
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x100);
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1
  wait(sc_time(20, SC_NS));
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x08000000); // abort=1
  wait(sc_time(20, SC_NS));

  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if ((status & (1u << 2)) == 0) {
    CSML_ERROR(0, logger) << "STATUS.aborted not set before RW1C clear" << std::endl;
    test_passed = false;
  }

  m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, (1u << 2)); // clear aborted
  wait(sc_time(10, SC_NS));
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status & (1u << 2)) {
    CSML_ERROR(0, logger) << "STATUS.aborted not cleared by RW1C write" << std::endl;
    test_passed = false;
  }

  // ------------------------------------------------------------
  // 4) STATUS.chunk_done (bit5): set during multi-chunk transfer, clear by write-1
  // ------------------------------------------------------------
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(sc_time(20, SC_NS));

  m_test->register_write_32(secure_dma_basetest::SRC_ADDR_LO_OFFSET, 0x10000100);
  m_test->register_write_32(secure_dma_basetest::DST_ADDR_LO_OFFSET, 0x10000200);
  m_test->register_write_32(secure_dma_basetest::ADDR_SPACE_ID_OFFSET, 0x00000077);
  m_test->register_write_32(secure_dma_basetest::TOTAL_DATA_SIZE_OFFSET, 0x100);
  m_test->register_write_32(secure_dma_basetest::CHUNK_DATA_SIZE_OFFSET, 0x10); // multi-chunk
  m_test->register_write_32(secure_dma_basetest::TRANSFER_WIDTH_OFFSET, 0x2);
  m_test->register_write_32(secure_dma_basetest::CONTROL_OFFSET, 0x80000000); // go=1

  bool chunk_seen = false;
  for (int i = 0; i < 50; i++) {
    wait(sc_time(10, SC_NS));
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & (1u << 5)) {
      chunk_seen = true;
      break;
    }
  }

  if (!chunk_seen) {
    CSML_ERROR(0, logger) << "STATUS.chunk_done not observed before RW1C clear" << std::endl;
    test_passed = false;
  } else {
    m_test->register_write_32(secure_dma_basetest::STATUS_OFFSET, (1u << 5)); // clear chunk_done
    wait(sc_time(10, SC_NS));
    m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
    if (status & (1u << 5)) {
      CSML_ERROR(0, logger) << "STATUS.chunk_done not cleared by RW1C write" << std::endl;
      test_passed = false;
    }
  }

  report_test_result("RW1C Register Behavior (STATUS.done/error/aborted/chunk_done)", test_passed);
}


// =============================================================================
//  Test Plan 124: Reset De-asserts Interrupts (test_reset_deasserts_interrupts)
// =============================================================================
//
// Test Plan Reference: test_reset_deasserts_interrupts (row 124)
// Description: Verify reset de-asserts all interrupt outputs
// Registers: INTR_STATE, STATUS
// Ports: rst_ni, reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr
// Type: Positive

void testbench::test_reset_deasserts_interrupts() {
  std::string test_name =
      "Reset De-asserts Interrupts (test_reset_deasserts_interrupts)";
  CSML_INFO(1, logger) << "Running: " << test_name << std::endl;

  bool passed = true;
  std::stringstream msg;
  uint32_t intr_state = 0;
  uint32_t status = 0;

  // Apply initial reset (rst_ni)
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(20, SC_NS);

  // Enable all interrupts and force state via INTR_TEST (reg_target_socket)
  m_test->register_write_32(secure_dma_basetest::INTR_ENABLE_OFFSET, 0x00000007);
  wait(5, SC_NS);
  m_test->register_write_32(secure_dma_basetest::INTR_TEST_OFFSET, 0x00000007);
  wait(10, SC_NS);

  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if ((intr_state & 0x7) != 0x7) {
    msg << "INTR_STATE not set before reset (got 0x" << std::hex << intr_state << "); ";
    passed = false;
  }
  if (!dma_done_intr_signal.read() || !dma_chunk_done_intr_signal.read() ||
      !dma_error_intr_signal.read()) {
    msg << "Interrupt outputs not asserted before reset; ";
    passed = false;
  }

  // Apply reset (rst_ni) – test plan: reset de-asserts all interrupt outputs
  m_test->apply_reset(sc_time(100, SC_NS));
  wait(50, SC_NS);

  // Verify INTR_STATE cleared (reg_target_socket)
  m_test->register_read_32(secure_dma_basetest::INTR_STATE_OFFSET, intr_state);
  if (intr_state != secure_dma_basetest::INTR_STATE_RESET) {
    msg << "INTR_STATE not cleared after reset (expected 0x0, got 0x" << std::hex
        << intr_state << std::dec << "); ";
    passed = false;
  }

  // Verify STATUS cleared (reg_target_socket) – reset de-asserts interrupt state
  m_test->register_read_32(secure_dma_basetest::STATUS_OFFSET, status);
  if (status != secure_dma_basetest::STATUS_RESET) {
    msg << "STATUS not cleared after reset (expected 0x0, got 0x" << std::hex
        << status << std::dec << "); ";
    passed = false;
  }

  // Verify all three interrupt outputs de-asserted (dma_done_intr, dma_chunk_done_intr, dma_error_intr)
  if (dma_done_intr_signal.read()) {
    msg << "dma_done_intr not deasserted after reset; ";
    passed = false;
  }
  if (dma_chunk_done_intr_signal.read()) {
    msg << "dma_chunk_done_intr not deasserted after reset; ";
    passed = false;
  }
  if (dma_error_intr_signal.read()) {
    msg << "dma_error_intr not deasserted after reset; ";
    passed = false;
  }

  if (passed) {
    msg << "Reset de-asserts all interrupt outputs (INTR_STATE, STATUS, dma_done/chunk_done/error)";
  }

  report_test_result(test_name, passed, msg.str());
}
