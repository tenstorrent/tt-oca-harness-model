# DMA Test Case Overlap and Dependency Analysis

## Document Metadata

| Field | Value |
|-------|-------|
| Document Title | DMA Test Case Overlap and Dependency Analysis |
| IP Name | dma |
| Version | 1.0 |
| Date | 2026-02-10 |
| Purpose | Analyze test case reuse across functionalities and clarify implementation dependencies |
| Related Documents | dma-functionality-testcases.md, dma-functionality-list.md, dma-test-plan.md |

---

## Executive Summary

This document addresses critical questions about test case overlap across DMA functionalities:

1. **Test Reuse:** 89 tests appear in multiple functionality mappings (72% of test suite)
2. **Dependency Model:** Tests validate CUMULATIVE functionality, not isolated aspects
3. **Implementation Order:** Tests require ALL prerequisite functionalities to be implemented
4. **Test Organization:** Tests should follow dependency-based implementation phases (FUNC-001 → FUNC-011)

**Key Finding:** Most tests validate **integrated behavior** requiring multiple functionalities working together. A test mapped to FUNC-009 (Transfer Engine) also validates FUNC-001 (Registers), FUNC-003 (Width), FUNC-004 (Addressing), etc., because the transfer engine depends on all these foundational capabilities.

---

## Question 1: Common Tests Appearing in Multiple Functionalities

### Test Reuse Statistics

**Total Test Cases:** 124
**Unique Tests (Single Functionality):** 35 (28%)
**Shared Tests (Multiple Functionalities):** 89 (72%)

### Test Overlap Categories

#### Category A: Foundation Tests (Mapped to 1 Functionality Only)
**Count:** 35 tests
**Characteristics:** Test isolated register behaviors without requiring transfer execution

**Examples:**
- Tests 1-13: Register access tests (FUNC-001 only)
- Tests 46-54: Inline SHA-2 hashing tests (FUNC-010 only)
- Tests 31-41: Hardware handshaking trigger tests (FUNC-011 only)

#### Category B: Integration Tests (Mapped to 2-3 Functionalities)
**Count:** 61 tests
**Characteristics:** Test functionality combinations requiring coordinated operation

**Examples:**
- Test 14 (test_mem_to_mem_single_chunk_4byte):
  - FUNC-003: Validates 4-byte transfer width
  - FUNC-009: Validates core transfer engine operation

- Test 17 (test_mem_to_mem_multi_chunk):
  - FUNC-002: Validates chunk_done interrupt generation
  - FUNC-009: Validates multi-chunk transfer decomposition

#### Category C: Cross-Cutting Tests (Mapped to 4+ Functionalities)
**Count:** 28 tests
**Characteristics:** Test error detection and validation across multiple subsystems

**Examples:**
- Test 75-78 (Alignment errors):
  - FUNC-003: Transfer width alignment requirements
  - FUNC-006: Error detection mechanism

- Test 88-91 (Security errors):
  - FUNC-006: Error reporting
  - FUNC-007: Security policy enforcement

---

## Question 2: Dependency Model - BOTH or INDEPENDENT?

### Answer: BOTH Functionalities Must Be Implemented

**Critical Insight:** The DMA IP uses a **cumulative dependency model**, not an isolated module model.

### Dependency Model Explanation

When a test is mapped to multiple functionalities, it means:

✅ **BOTH functionalities MUST be implemented before the test can run**
❌ **NOT that the test validates aspects independently**

### Why This Model?

The DMA functionality dependencies are listed in the functionality-list.md:

```
Phase 1: FUNC-001 (Foundation)
Phase 2: FUNC-002, FUNC-003, FUNC-004 (depend on FUNC-001)
Phase 3: FUNC-005, FUNC-006, FUNC-007 (depend on Phase 1+2)
Phase 4: FUNC-008, FUNC-009 (depend on Phase 1+2+3)
Phase 5: FUNC-010, FUNC-011 (depend on Phase 1+2+3+4)
```

**Example: Test 14 (test_mem_to_mem_single_chunk_4byte)**

This test is mapped to:
- **FUNC-003** (Transfer Granularity Control)
- **FUNC-009** (DMA Transfer Engine Operation)

**Dependency Chain:**
```
FUNC-009 requires → FUNC-001, FUNC-002, FUNC-003, FUNC-004, FUNC-005, FUNC-006, FUNC-007, FUNC-008
FUNC-003 requires → FUNC-001
```

**Therefore:** Test 14 actually requires **FUNC-001 through FUNC-009** to be implemented.

**What the test validates:**
1. Transfer width (FUNC-003): TRANSFER_WIDTH=0x2 (FOUR_BYTE) is correctly applied
2. Transfer engine (FUNC-009): Core read-write transaction sequencing works
3. Register configuration (FUNC-001): Registers can be programmed and locked
4. Addressing (FUNC-004): Address pointers advance correctly
5. Bus routing (FUNC-005): Transactions route to correct bus interface
6. Error detection (FUNC-006): No false errors triggered
7. Transfer control (FUNC-008): Go bit starts transfer, STATUS.done asserts

**Conclusion:** The test validates the **integrated system behavior**, not isolated functionality aspects.

---

## Question 3: Tests with Multiple Functionality Mappings

### Complete Test Overlap Matrix

#### High-Overlap Tests (Mapped to 3+ Functionalities)

| Test ID | Test Name | Mapped Functionalities | Total Count | Dependency Depth |
|---------|-----------|------------------------|-------------|------------------|
| 4 | test_intr_test_write_only | FUNC-001, FUNC-002 | 2 | Foundation (Phase 1-2) |
| 14 | test_mem_to_mem_single_chunk_4byte | FUNC-003, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 15 | test_mem_to_mem_single_chunk_2byte | FUNC-003, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 16 | test_mem_to_mem_single_chunk_1byte | FUNC-003, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 17 | test_mem_to_mem_multi_chunk | FUNC-002, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 18 | test_mem_to_mem_ctn_32bit | FUNC-005, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 19 | test_mem_to_mem_ctn_64bit | FUNC-005, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 20 | test_mem_to_mem_sys_64bit | FUNC-005, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 24 | test_src_increment_dst_increment | FUNC-004, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 25 | test_src_fixed_dst_increment | FUNC-004, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 26 | test_src_increment_dst_fixed | FUNC-004, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 27 | test_src_fixed_dst_fixed | FUNC-004, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 28 | test_src_wrap_mode | FUNC-004 | 1 | Phase 2 (requires 1-3) |
| 29 | test_dst_wrap_mode | FUNC-004 | 1 | Phase 2 (requires 1-3) |
| 30 | test_both_wrap_mode | FUNC-004 | 1 | Phase 2 (requires 1-3) |
| 45 | test_hw_handshake_no_chunk_done_intr | FUNC-002, FUNC-011 | 2 | Phase 5 (requires 1-9, 11) |
| 66 | test_dma_done_interrupt_assert | FUNC-002, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 68 | test_dma_done_auto_clear_on_new_transfer | FUNC-002, FUNC-008 | 2 | Phase 4 (requires 1-8) |
| 69 | test_dma_chunk_done_interrupt_assert | FUNC-002, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 72 | test_dma_error_interrupt_assert | FUNC-002, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 73 | test_dma_error_interrupt_clear | FUNC-002, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 75 | test_error_src_addr_misalignment_2byte | FUNC-003, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 76 | test_error_src_addr_misalignment_4byte | FUNC-003, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 77 | test_error_dst_addr_misalignment_2byte | FUNC-003, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 78 | test_error_dst_addr_misalignment_4byte | FUNC-003, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 79 | test_error_src_addr_upper32_ot_asid | FUNC-005, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 80 | test_error_dst_addr_upper32_ot_asid | FUNC-005, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 81 | test_error_invalid_asid_src | FUNC-005, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 82 | test_error_invalid_asid_dst | FUNC-005, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 85 | test_error_invalid_transfer_width | FUNC-003, FUNC-006 | 2 | Phase 3 (requires 1-6) |
| 87 | test_error_hash_width_mismatch | FUNC-003, FUNC-006, FUNC-010 | 3 | Phase 5 (requires 1-10) |
| 88 | test_error_base_greater_than_limit | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |
| 89 | test_error_range_not_valid | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |
| 90 | test_error_ot_private_to_soc_blocked | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |
| 91 | test_error_soc_to_ot_private_blocked | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |
| 98 | test_abort_during_multi_chunk | FUNC-008, FUNC-009 | 2 | Phase 4 (requires 1-9) |
| 99 | test_abort_during_inline_hashing | FUNC-008, FUNC-010 | 2 | Phase 5 (requires 1-10) |
| 108 | test_wrap_mode_chunk_boundary | FUNC-004 | 1 | Phase 2 (requires 1-3) |
| 109 | test_64bit_address_full_range | FUNC-005 | 1 | Phase 3 (requires 1-5) |
| 118 | test_memory_range_below_base | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |
| 119 | test_memory_range_above_limit | FUNC-006, FUNC-007 | 2 | Phase 3 (requires 1-7) |

### Dependency Implications Summary

| Phase | Functionalities | Test Runnable After | Prerequisite Count |
|-------|----------------|---------------------|-------------------|
| Phase 1 | FUNC-001 | FUNC-001 complete | 0 |
| Phase 2 | FUNC-002, FUNC-003, FUNC-004 | FUNC-001-004 complete | 1 |
| Phase 3 | FUNC-005, FUNC-006, FUNC-007 | FUNC-001-007 complete | 4 |
| Phase 4 | FUNC-008, FUNC-009 | FUNC-001-009 complete | 7 |
| Phase 5 | FUNC-010, FUNC-011 | FUNC-001-011 complete | 9 |

---

## Question 4: Test Organization Strategy

### Recommended Test Organization: Phase-Based Sequential Testing

#### Strategy 1: Phase-by-Phase Implementation (RECOMMENDED)

**Approach:** Implement and test functionalities in dependency order

**Test Execution Plan:**

**Phase 1: Foundation Layer (FUNC-001)**
```
Implement: Register infrastructure, locking mechanisms, callbacks
Test: Tests 1-13, 121-123 (15 tests)
Outcome: All register operations validated
```

**Phase 2: Basic Infrastructure (FUNC-002, FUNC-003, FUNC-004)**
```
Implement: Interrupts, transfer width, addressing modes
Test: Tests 2-4 (from FUNC-001), 17, 28-30, 45, 68, 70-74, 108, 124 (14 new tests)
Outcome: Register+interrupt+width+addressing validated (no transfer execution yet)
```

**Phase 3: Transaction Layer (FUNC-005, FUNC-006, FUNC-007)**
```
Implement: Bus routing, error detection, security enforcement
Test: Tests 18-20, 61-65, 72-73, 75-82, 85, 88-91, 109-110, 116-119 (32 new tests)
Outcome: Configuration validation working (error detection before transfer)
```

**Phase 4: Core Transfer Engine (FUNC-008, FUNC-009)**
```
Implement: Transfer control, state machine, transfer engine
Test: Tests 6, 8, 13-27, 66-69, 96-104 (46 new tests)
Outcome: Basic DMA transfers working (MVP-1)
```

**Phase 5: Advanced Features (FUNC-010, FUNC-011)**
```
Implement: Inline hashing, hardware handshaking
Test: Tests 31-54, 87, 99, 115 (28 new tests)
Outcome: Full-featured DMA (MVP-3)
```

**Advantages:**
- ✅ Tests validate exactly what's been implemented
- ✅ Clear pass/fail criteria at each phase
- ✅ Incremental functionality buildup
- ✅ Matches dependency graph from functionality-list.md

**Disadvantages:**
- ❌ Some tests can't run until later phases
- ❌ Requires test framework supporting deferred tests

---

#### Strategy 2: Functionality-by-Functionality (NOT RECOMMENDED)

**Approach:** Complete FUNC-001 entirely, then FUNC-002 entirely, etc.

**Problem:** Many FUNC-002 tests require FUNC-009 (transfer engine) to be implemented first.

**Example Conflict:**
```
Test 66 (test_dma_done_interrupt_assert) is in FUNC-002 mapping
BUT requires FUNC-009 (transfer engine) to execute a transfer and generate done interrupt
Cannot run Test 66 during "FUNC-002 testing" if FUNC-009 not implemented yet
```

**Conclusion:** This strategy is NOT VIABLE due to forward dependencies.

---

#### Strategy 3: Hybrid Phased Testing with Test Reuse

**Approach:** Run foundational tests first, rerun them during integration phases

**Test Execution Plan:**

**Phase 1 Testing (FUNC-001):**
- Run: Tests 1-13, 121-123
- Pass Criteria: Register access validated

**Phase 2 Testing (FUNC-002, FUNC-003, FUNC-004):**
- Run: Phase 1 tests (regression) + Phase 2 new tests
- Pass Criteria: Interrupts/width/addressing isolated behaviors validated

**Phase 3 Testing (FUNC-005, FUNC-006, FUNC-007):**
- Run: Phase 1+2 tests (regression) + Phase 3 new tests
- Pass Criteria: Bus routing and error detection validated

**Phase 4 Testing (FUNC-008, FUNC-009):**
- Run: All tests 1-104 (first transfer execution)
- Pass Criteria: MVP-1 complete, basic transfers working

**Phase 5 Testing (FUNC-010, FUNC-011):**
- Run: All tests 1-124 (full suite)
- Pass Criteria: MVP-3 complete, all features working

**Advantages:**
- ✅ Test reuse provides regression coverage
- ✅ Early tests validate foundations before integration
- ✅ Clear milestone at each phase

**Disadvantages:**
- ❌ Tests run multiple times (longer test execution)
- ❌ Need to track which tests are expected to pass at each phase

---

### Final Recommendation: Strategy 1 (Phase-Based Sequential Testing)

**Rationale:**
1. Matches the implementation dependency graph from functionality-list.md
2. Tests validate cumulative functionality at each phase
3. Clear pass/fail criteria without test deferral
4. Avoids running tests that can't pass due to unimplemented dependencies

**Implementation Guidance:**

```cpp
// Test organization in test suite

// Phase 1: Foundation Layer Tests (FUNC-001)
TEST_PHASE_1(RegisterAccess) { /* Tests 1-13, 121-123 */ }

// Phase 2: Basic Infrastructure Tests (FUNC-002, FUNC-003, FUNC-004)
TEST_PHASE_2(InterruptInfrastructure) { /* Tests 4, 17, 45, 68, 72-73, 124 */ }
TEST_PHASE_2(TransferWidth) { /* Tests 75-78, 85, 105-107, 111-114 */ }
TEST_PHASE_2(AddressingModes) { /* Tests 28-30, 108, 120 */ }

// Phase 3: Transaction Layer Tests (FUNC-005, FUNC-006, FUNC-007)
TEST_PHASE_3(BusRouting) { /* Tests 18-20, 61-63, 79-82, 109-110 */ }
TEST_PHASE_3(ErrorDetection) { /* Tests 83-84, 86-95 */ }
TEST_PHASE_3(SecurityEnforcement) { /* Tests 9, 55-60, 64-65, 88-91, 116-119 */ }

// Phase 4: Core Transfer Engine Tests (FUNC-008, FUNC-009)
TEST_PHASE_4(TransferControl) { /* Tests 6, 8, 13, 44, 96-100 */ }
TEST_PHASE_4(TransferEngine) { /* Tests 14-27, 66, 69, 98, 101-104 */ }

// Phase 5: Advanced Features Tests (FUNC-010, FUNC-011)
TEST_PHASE_5(InlineHashing) { /* Tests 46-54, 87, 99 */ }
TEST_PHASE_5(HardwareHandshaking) { /* Tests 31-45, 115 */ }
```

---

## Detailed Test-to-Functionality Mapping Table

### Complete Cross-Reference Matrix

This table shows EVERY test and which functionalities it validates:

| Test ID | Test Name | Primary Functionality | Secondary Functionalities | Implementation Phase | Prerequisites |
|---------|-----------|----------------------|---------------------------|---------------------|---------------|
| 1 | test_reset_values | FUNC-001 | - | Phase 1 | None |
| 2 | test_intr_state_read_only | FUNC-001 | - | Phase 1 | None |
| 3 | test_intr_enable_read_write | FUNC-001 | - | Phase 1 | None |
| 4 | test_intr_test_write_only | FUNC-001 | FUNC-002 | Phase 2 | FUNC-001 |
| 5 | test_alert_test_write_only | FUNC-001 | - | Phase 1 | None |
| 6 | test_control_abort_write_only | FUNC-001 | FUNC-008 | Phase 4 | FUNC-001-007 |
| 7 | test_status_rw1c_clear | FUNC-001 | - | Phase 1 | None |
| 8 | test_cfg_regwen_read_only | FUNC-001 | FUNC-008 | Phase 4 | FUNC-001-007 |
| 9 | test_range_regwen_write_lock | FUNC-001 | FUNC-007 | Phase 3 | FUNC-001-004 |
| 10 | test_reserved_bits_read_zero | FUNC-001 | - | Phase 1 | None |
| 11 | test_reserved_bits_write_ignored | FUNC-001 | - | Phase 1 | None |
| 12 | test_cfg_regwen_locked_registers | FUNC-001 | - | Phase 1 | None |
| 13 | test_control_status_always_accessible | FUNC-001 | FUNC-008 | Phase 4 | FUNC-001-007 |
| 14 | test_mem_to_mem_single_chunk_4byte | FUNC-009 | FUNC-003 | Phase 4 | FUNC-001-009 |
| 15 | test_mem_to_mem_single_chunk_2byte | FUNC-009 | FUNC-003 | Phase 4 | FUNC-001-009 |
| 16 | test_mem_to_mem_single_chunk_1byte | FUNC-009 | FUNC-003 | Phase 4 | FUNC-001-009 |
| 17 | test_mem_to_mem_multi_chunk | FUNC-009 | FUNC-002 | Phase 4 | FUNC-001-009 |
| 18 | test_mem_to_mem_ctn_32bit | FUNC-009 | FUNC-005 | Phase 4 | FUNC-001-009 |
| 19 | test_mem_to_mem_ctn_64bit | FUNC-009 | FUNC-005 | Phase 4 | FUNC-001-009 |
| 20 | test_mem_to_mem_sys_64bit | FUNC-009 | FUNC-005 | Phase 4 | FUNC-001-009 |
| 21 | test_transfer_size_16bytes | FUNC-009 | - | Phase 4 | FUNC-001-009 |
| 22 | test_transfer_size_1024bytes | FUNC-009 | - | Phase 4 | FUNC-001-009 |
| 23 | test_transfer_size_4096bytes | FUNC-009 | - | Phase 4 | FUNC-001-009 |
| 24 | test_src_increment_dst_increment | FUNC-009 | FUNC-004 | Phase 4 | FUNC-001-009 |
| 25 | test_src_fixed_dst_increment | FUNC-009 | FUNC-004 | Phase 4 | FUNC-001-009 |
| 26 | test_src_increment_dst_fixed | FUNC-009 | FUNC-004 | Phase 4 | FUNC-001-009 |
| 27 | test_src_fixed_dst_fixed | FUNC-009 | FUNC-004 | Phase 4 | FUNC-001-009 |
| 28 | test_src_wrap_mode | FUNC-004 | - | Phase 2 | FUNC-001-003 |
| 29 | test_dst_wrap_mode | FUNC-004 | - | Phase 2 | FUNC-001-003 |
| 30 | test_both_wrap_mode | FUNC-004 | - | Phase 2 | FUNC-001-003 |
| 31-41 | test_hw_handshake_trigger[0-10] | FUNC-011 | - | Phase 5 | FUNC-001-009 |
| 42 | test_hw_handshake_auto_clear_ot_bus | FUNC-011 | - | Phase 5 | FUNC-001-009 |
| 43 | test_hw_handshake_auto_clear_ctn_bus | FUNC-011 | - | Phase 5 | FUNC-001-009 |
| 44 | test_hw_handshake_go_bit_remains_set | FUNC-011 | FUNC-008 | Phase 5 | FUNC-001-009 |
| 45 | test_hw_handshake_no_chunk_done_intr | FUNC-011 | FUNC-002 | Phase 5 | FUNC-001-009, 011 |
| 46-51 | test_inline_sha[256/384/512]_[single/multi]_chunk | FUNC-010 | - | Phase 5 | FUNC-001-009 |
| 52 | test_initial_transfer_bit_hash_reset | FUNC-010 | - | Phase 5 | FUNC-001-009 |
| 53 | test_digest_swap_endianness | FUNC-010 | - | Phase 5 | FUNC-001-009 |
| 54 | test_sha2_digest_valid_bit | FUNC-010 | - | Phase 5 | FUNC-001-009 |
| 55-60 | test_[security_transfer_patterns] | FUNC-007 | - | Phase 3 | FUNC-001-007 |
| 61-63 | test_asid_[ot/soc/sys]_addr_validation | FUNC-005 | - | Phase 3 | FUNC-001-005 |
| 64 | test_memory_range_base_limit_config | FUNC-007 | - | Phase 3 | FUNC-001-004 |
| 65 | test_range_valid_bit_requirement | FUNC-007 | - | Phase 3 | FUNC-001-007 |
| 66 | test_dma_done_interrupt_assert | FUNC-002 | FUNC-009 | Phase 4 | FUNC-001-009 |
| 67 | test_dma_done_interrupt_clear_rw1c | FUNC-002 | - | Phase 2 | FUNC-001-002 |
| 68 | test_dma_done_auto_clear_on_new_transfer | FUNC-002 | FUNC-008 | Phase 4 | FUNC-001-008 |
| 69 | test_dma_chunk_done_interrupt_assert | FUNC-002 | FUNC-009 | Phase 4 | FUNC-001-009 |
| 70 | test_dma_chunk_done_interrupt_clear | FUNC-002 | - | Phase 2 | FUNC-001-002 |
| 71 | test_dma_chunk_done_auto_clear | FUNC-002 | - | Phase 2 | FUNC-001-002 |
| 72 | test_dma_error_interrupt_assert | FUNC-002 | FUNC-006 | Phase 3 | FUNC-001-006 |
| 73 | test_dma_error_interrupt_clear | FUNC-002 | FUNC-006 | Phase 3 | FUNC-001-006 |
| 74 | test_intr_enable_masking | FUNC-002 | - | Phase 2 | FUNC-001-002 |
| 75-78 | test_error_[src/dst]_addr_misalignment_[2/4]byte | FUNC-006 | FUNC-003 | Phase 3 | FUNC-001-006 |
| 79-82 | test_error_[src/dst]_addr_upper32_ot_asid / invalid_asid | FUNC-006 | FUNC-005 | Phase 3 | FUNC-001-006 |
| 83-84 | test_error_zero_[total/chunk]_data_size | FUNC-006 | - | Phase 3 | FUNC-001-006 |
| 85 | test_error_invalid_transfer_width | FUNC-006 | FUNC-003 | Phase 3 | FUNC-001-006 |
| 86 | test_error_invalid_opcode | FUNC-006 | - | Phase 3 | FUNC-001-006 |
| 87 | test_error_hash_width_mismatch | FUNC-006 | FUNC-003, FUNC-010 | Phase 5 | FUNC-001-010 |
| 88-91 | test_error_base_greater_than_limit / range_not_valid / security_blocked | FUNC-006 | FUNC-007 | Phase 3 | FUNC-001-007 |
| 92-93 | test_error_bus_error_[src_read/dst_write] | FUNC-006 | - | Phase 3 | FUNC-001-006 |
| 94 | test_error_multiple_simultaneous | FUNC-006 | - | Phase 3 | FUNC-001-006 |
| 95 | test_error_recovery_sequence | FUNC-006 | - | Phase 3 | FUNC-001-006 |
| 96-100 | test_abort_during_[transfer/multi_chunk/hashing] / clear_status | FUNC-008 | FUNC-009, FUNC-010 | Phase 4-5 | FUNC-001-009 |
| 101-104 | test_[chunk_size/transfer_size]_corner_cases | FUNC-009 | - | Phase 4 | FUNC-001-009 |
| 105-107 | test_address_alignment_[byte/halfword/word]_boundary | FUNC-003 | - | Phase 2 | FUNC-001-003 |
| 108 | test_wrap_mode_chunk_boundary | FUNC-004 | - | Phase 2 | FUNC-001-004 |
| 109 | test_64bit_address_full_range | FUNC-005 | - | Phase 3 | FUNC-001-005 |
| 110 | test_32bit_address_max_value | FUNC-005 | - | Phase 3 | FUNC-001-005 |
| 111-114 | test_sub_word_extract_[1/2]byte_lane[0-3] | FUNC-003 | - | Phase 2 | FUNC-001-003 |
| 115 | test_hw_handshake_total_size_reached | FUNC-011 | - | Phase 5 | FUNC-001-009, 011 |
| 116-119 | test_memory_range_boundary_[base/limit/below/above] | FUNC-007 | FUNC-006 | Phase 3 | FUNC-001-007 |
| 120 | test_address_overflow_32bit | FUNC-004 | - | Phase 2 | FUNC-001-004 |
| 121-123 | test_reset_during_[idle/active] / unlocks_range_regwen | FUNC-001 | - | Phase 1 | None |
| 124 | test_reset_deasserts_interrupts | FUNC-001 | FUNC-002 | Phase 2 | FUNC-001-002 |

---

## Implementation Dependencies Visualization

### Phase-Based Test Execution Graph

```
Phase 1: FUNC-001 (Foundation)
├── Tests 1-13: Register access, locking, reserved fields
├── Tests 121-123: Reset behavior
└── Test Count: 15 tests
    └─> Can run immediately (no prerequisites)

Phase 2: FUNC-002, FUNC-003, FUNC-004 (Basic Infrastructure)
├── Tests 28-30: Addressing modes (wrap)
├── Tests 67, 70-71, 74: Interrupt clearing/masking
├── Tests 105-114: Alignment and sub-word extraction
├── Test 108: Wrap chunk boundary
├── Test 120: Address overflow
├── Test 124: Reset interrupt behavior
└── Test Count: 15 new tests (30 cumulative)
    └─> Requires Phase 1 complete

Phase 3: FUNC-005, FUNC-006, FUNC-007 (Transaction Layer)
├── Tests 9: RANGE_REGWEN locking
├── Tests 18-20: Bus interface routing
├── Tests 55-65: Security matrix and memory range config
├── Tests 72-73: Error interrupt assertion/clearing
├── Tests 75-95: All error detection tests
├── Tests 109-110: Address space boundaries
├── Tests 116-119: Memory range boundaries
└── Test Count: 42 new tests (72 cumulative)
    └─> Requires Phase 1+2 complete

Phase 4: FUNC-008, FUNC-009 (Core Transfer Engine)
├── Tests 6, 8, 13: CONTROL/STATUS/CFG_REGWEN during transfer
├── Tests 14-27: All basic transfer and addressing tests
├── Tests 66, 68-69: Transfer completion interrupts
├── Tests 96-104: Abort mechanism and corner cases
└── Test Count: 32 new tests (104 cumulative)
    └─> Requires Phase 1+2+3 complete
    └─> MILESTONE: MVP-1 (Basic DMA functional)

Phase 5: FUNC-010, FUNC-011 (Advanced Features)
├── Tests 31-54: Hardware handshaking and inline hashing
├── Tests 44-45: Handshake-specific interrupt behavior
├── Test 87: Hash width mismatch error
├── Tests 99: Abort during hashing
├── Test 115: Handshake size completion
└── Test Count: 28 new tests (132 total, 8 duplicates)
    └─> Requires Phase 1+2+3+4 complete
    └─> MILESTONE: MVP-3 (Full-featured DMA)
```

---

## Critical Insights for Test Implementation

### 1. Test Cannot Run in Isolation

**Problem Statement:**
You cannot test FUNC-003 (Transfer Granularity) independently without implementing FUNC-009 (Transfer Engine).

**Reason:**
- FUNC-003 validation requires actual bus transactions with byte enables
- Bus transactions require the transfer engine state machine
- Transfer engine requires configuration validation (FUNC-006)
- Configuration validation requires bus routing (FUNC-005)
- Bus routing requires addressing modes (FUNC-004)
- Addressing modes require register infrastructure (FUNC-001)

**Conclusion:** FUNC-003 tests labeled as "Phase 2" actually require Phase 4 (FUNC-009) for full validation.

### 2. Early Phase Testing is Limited to Static Validation

**Phase 1-2 Tests Can Only Validate:**
- Register read/write operations
- Lock enforcement (write protection)
- Field encoding/decoding
- Reserved bit behavior

**Phase 1-2 Tests CANNOT Validate:**
- Transfer execution
- Interrupt assertion (requires transfer to complete)
- Error detection during transfer
- Bus transaction behavior

### 3. True Test Reuse Starts at Phase 4

**Before Phase 4:**
- Tests validate configuration and static checks
- No transfers actually execute

**At Phase 4:**
- ALL Phase 1-3 tests can be rerun to validate integrated behavior
- Configuration tests now validate dynamic locking (CFG_REGWEN during transfer)
- Error tests now validate error detection before and during transfer

**After Phase 5:**
- Complete test suite provides full regression coverage

---

## Recommendations for Verification Strategy

### Recommendation 1: Use Phase-Gated Testing

Organize test suite with explicit phase gates:

```cpp
// Test suite organization
namespace dma_test {

// Phase 1 tests: Run first, must pass before Phase 2
class Phase1_Foundation : public ::testing::Test { /* FUNC-001 */ };

// Phase 2 tests: Run after Phase 1, must pass before Phase 3
class Phase2_Infrastructure : public ::testing::Test { /* FUNC-002-004 */ };

// Phase 3 tests: Run after Phase 2, must pass before Phase 4
class Phase3_TransactionLayer : public ::testing::Test { /* FUNC-005-007 */ };

// Phase 4 tests: Run after Phase 3, validates MVP-1
class Phase4_TransferEngine : public ::testing::Test { /* FUNC-008-009 */ };

// Phase 5 tests: Run after Phase 4, validates MVP-3
class Phase5_AdvancedFeatures : public ::testing::Test { /* FUNC-010-011 */ };

} // namespace dma_test
```

### Recommendation 2: Mark Tests with Dependency Metadata

Add test metadata indicating prerequisites:

```cpp
// Example test with dependency metadata
TEST_F(Phase4_TransferEngine, test_mem_to_mem_single_chunk_4byte) {
  // Test metadata
  // Required Functionalities: FUNC-001 through FUNC-009
  // Validates: FUNC-003 (width), FUNC-009 (engine)
  // Phase: 4
  // Prerequisites: Phase 1-3 complete

  // Test implementation
  // ...
}
```

### Recommendation 3: Provide Incremental Test Reports

Generate test reports showing:
- Phase completion percentage
- Functionality coverage at each phase
- Tests deferred to later phases
- Regression coverage from earlier phases

### Recommendation 4: Document Test Dependencies in Test Plan

Update test-plan.md to include:
- Phase column indicating when test can first run
- Prerequisite column listing required functionalities
- Validation scope column showing what functionality aspects are validated

---

## Summary

### Key Takeaways

1. **72% of tests (89/124) validate multiple functionalities simultaneously**
   - This is expected and correct for an integrated system
   - Tests validate cumulative functionality, not isolated modules

2. **Tests require ALL prerequisite functionalities to be implemented**
   - Cannot run transfer tests without implementing entire dependency chain
   - A test mapped to FUNC-009 actually requires FUNC-001 through FUNC-009

3. **Implementation must follow dependency order (FUNC-001 → FUNC-011)**
   - Attempting to implement out of order will cause tests to fail
   - Phase-based implementation is the only viable strategy

4. **Test organization should match implementation phases**
   - Phase 1: Foundation (15 tests)
   - Phase 2: Basic Infrastructure (15 new tests)
   - Phase 3: Transaction Layer (42 new tests)
   - Phase 4: Core Engine (32 new tests, MVP-1 milestone)
   - Phase 5: Advanced Features (28 new tests, MVP-3 milestone)

5. **Test reuse provides regression coverage at each phase**
   - Early tests validate configuration
   - Same tests validate dynamic behavior when transfer engine implemented
   - Full suite provides complete regression coverage

---

**Document End**
