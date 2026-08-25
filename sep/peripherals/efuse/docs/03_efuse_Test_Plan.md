# SEP eFuse SystemC TLM Test Plan

**Document Version:** 1.0
**Date:** 2026-04-27
**IP Module:** SEP eFuse Controller (sep_efuse)
**Modeling Approach:** SystemC TLM2.0, fuse-shadow register model with WOSET semantics

---

## Executive Summary

This test plan provides comprehensive test coverage for the `sep_efuse` SystemC TLM model. The model loads fuse content from CCI parameters at `end_of_elaboration()` and exposes it as MMIO-accessible shadow registers. Writable registers follow either WOSET (accumulating-OR) or unrestricted RW semantics.

### Key Testing Focus Areas

1. **Fuse Load — Scalar Registers** — RO shadow registers contain the values specified by CCI params after elaboration
2. **Fuse Load — Array Registers** — 256-bit (8 × 32-bit) array registers correctly loaded word-by-word from vector params
3. **RO Write Protection** — read-only shadow registers silently ignore write attempts
4. **WOSET — LOCKS_LO/HI** — lock register bits accumulate via OR; once set, cannot be cleared
5. **WOSET — SIP_DIS_LO** — feature-disable bits accumulate; write-0 has no effect
6. **WOSET — SYS_DIS_LO** — independent WOSET channel with same semantics as SIP_DIS
7. **Interface Control — efuse_sense_done** — always reads 1; write attempt silently ignored
8. **Interface Control — EFUSE_READ_CTRL R/W** — unrestricted read-write access; overwrites are allowed

### Critical Architectural Context

- All fuse content comes from CCI parameters set via ini file — no compile-time configuration
- Tests 1 and 2 verify that what the ini file specifies appears in the shadow registers at runtime
- WOSET tests (4–6) are self-contained: they write fresh bit patterns and verify OR accumulation, independent of the ini preset value
- The model has no reset port — test state carries forward; tests are designed to be non-interfering across their own register domains

---

## Test Plan Summary

| Test # | Test Method | Sub-tests | Description | Registers Accessed |
|--------|-------------|-----------|-------------|-------------------|
| 1 | `test_fuse_load_ro_registers` | 4 | Verify scalar RO shadow registers loaded from CCI params | LC_STATE, SBOOT_DIS, CHIPLET_PUBK_REVOKE, STATUS_RPT |
| 2 | `test_fuse_load_array_registers` | 2 | Verify array shadow registers loaded word-by-word from vector params | SIP_PUBK[0:7], CHIPLET_UID[0:7] |
| 3 | `test_ro_write_protection` | 2 | Verify RO registers ignore write attempts | SBOOT_DIS, CHIPLET_UID[0] |
| 4 | `test_woset_locks` | 3 | Verify LOCKS_LO WOSET accumulation and non-clearability | LOCKS_LO |
| 5 | `test_woset_sip_dis` | 3 | Verify SIP_DIS_LO WOSET accumulation | SIP_DIS_LO |
| 6 | `test_woset_sys_dis` | 2 | Verify SYS_DIS_LO WOSET accumulation | SYS_DIS_LO |
| 7 | `test_efuse_sense_done` | 2 | Verify EFUSE_INTERFACE_CTRL_STATUS.efuse_sense_done is always 1 | EFUSE_INTERFACE_CTRL_STATUS |
| 8 | `test_efuse_read_ctrl_rw` | 2 | Verify EFUSE_READ_CTRL is unrestricted RW | EFUSE_READ_CTRL |
| **TOTAL** | **8 methods** | **20 sub-tests** | **Full coverage of all register access categories** | |

---

## Test Case Details

| Sl. No. | Test Method | Sub-test | Description | Registers / Params Used | Pass Condition | Test Type |
|---------|-------------|----------|-------------|-------------------------|----------------|-----------|
| **FUSE LOAD — SCALAR REGISTERS** | | | | | | |
| 1 | `test_fuse_load_ro_registers` | 1a | Read `LC_STATE` at offset 0x008. Compare against `m_dut->lc_state.get_param_value()`. Verifies `load_fuses()` correctly stored the ini-configured life-cycle state in the shadow register. | `LC_STATE` (read); param: `lc_state` | `val == lc_state.get_param_value()` | Positive |
| 2 | `test_fuse_load_ro_registers` | 1b | Read `SBOOT_DIS` at offset 0x00C. Compare against `m_dut->sboot_dis.get_param_value()`. Verifies secure-boot-disable fuse is correctly reflected in shadow. | `SBOOT_DIS` (read); param: `sboot_dis` | `val == sboot_dis.get_param_value()` | Positive |
| 3 | `test_fuse_load_ro_registers` | 1c | Read `CHIPLET_PUBK_REVOKE` at offset 0x084. Compare against `m_dut->chiplet_pubk_revoke.get_param_value()`. Verifies WOSET register initial value matches ini preset (before any firmware writes). | `CHIPLET_PUBK_REVOKE` (read); param: `chiplet_pubk_revoke` | `val == chiplet_pubk_revoke.get_param_value()` | Positive |
| 4 | `test_fuse_load_ro_registers` | 1d | Read `STATUS_RPT` at offset 0x168. Compare against `m_dut->status_rpt.get_param_value()`. Verifies RO boot-status field is loaded correctly. Default ini has `status_rpt: 1`. | `STATUS_RPT` (read); param: `status_rpt` | `val == status_rpt.get_param_value()` | Positive |
| **FUSE LOAD — ARRAY REGISTERS** | | | | | | |
| 5 | `test_fuse_load_array_registers` | 2a | For each `i` in 0..7: read `SIP_PUBK[i]` at offset `(SIP_PUBK_OFFSET + i*4)`. Compare against `sip_pubk_v[i]` where `sip_pubk_v = m_dut->sip_pubk.get_param_value()`. Use `vi(v, i)` lambda for bounds-safe access. Report per-word FAIL if mismatch; report single PASS if all 8 words match. | `SIP_PUBK[0:7]` (8 reads); param: `sip_pubk` vector | All 8 words match param vector | Positive |
| 6 | `test_fuse_load_array_registers` | 2b | For each `i` in 0..7: read `CHIPLET_UID[i]` at offset `(CHIPLET_UID_OFFSET + i*4)`. Compare against `uid_v[i]` where `uid_v = m_dut->chiplet_uid.get_param_value()`. Report per-word FAIL if mismatch; report single PASS if all 8 words match. | `CHIPLET_UID[0:7]` (8 reads); param: `chiplet_uid` vector | All 8 words match param vector | Positive |
| **RO WRITE PROTECTION** | | | | | | |
| 7 | `test_ro_write_protection` | 3a | Read `SBOOT_DIS` (store `before`). Write 0xDEADBEEF to `SBOOT_DIS`. Wait 1 ns. Read `SBOOT_DIS` (store `after`). Verify `before == after`. Confirms `write_mask=0x0` — software writes to this RO shadow are silently discarded. | `SBOOT_DIS` (read, write 0xDEADBEEF, read) | `before == after` | Positive |
| 8 | `test_ro_write_protection` | 3b | Read `CHIPLET_UID[0]` at offset `CHIPLET_UID_OFFSET` (store `before`). Write 0xCAFEBABE to same offset. Wait 1 ns. Read again (store `after`). Verify `before == after`. Confirms RO protection on array registers. | `CHIPLET_UID[0]` (read, write 0xCAFEBABE, read) | `before == after` | Positive |
| **WOSET — LOCKS_LO** | | | | | | |
| 9 | `test_woset_locks` | 4a | Write 0x00000003 to `LOCKS_LO`. Wait 1 ns. Read `LOCKS_LO`. Verify `val == 0x00000003`. First WOSET write sets bits[0:1] (LC_STATE write-lock and read-lock). | `LOCKS_LO` (write 0x3, read) | `val == 0x00000003` | Positive |
| 10 | `test_woset_locks` | 4b | Write 0x00000002 to `LOCKS_LO` (attempt to "clear" bit[0] by writing only bit[1]). Wait 1 ns. Read `LOCKS_LO`. Verify `val == 0x00000003` (bit[0] not cleared — WOSET prevents clearing). | `LOCKS_LO` (write 0x2, read) | `val == 0x00000003` (unchanged) | Positive |
| 11 | `test_woset_locks` | 4c | Write 0x000000F0 to `LOCKS_LO` (set bits[4:7]). Wait 1 ns. Read `LOCKS_LO`. Verify `val == 0x000000F3` (OR of 0x03 from sub-test 4a and 0xF0 from this write). Confirms multi-write accumulation. | `LOCKS_LO` (write 0xF0, read) | `val == 0x000000F3` | Positive |
| **WOSET — SIP_DIS_LO** | | | | | | |
| 12 | `test_woset_sip_dis` | 5a | Write 0x1 to `SIP_DIS_LO`. Wait 1 ns. Read `SIP_DIS_LO`. Verify `val == 0x1` (bit[0]=sep_debug disabled). | `SIP_DIS_LO` (write 0x1, read) | `val == 0x1` | Positive |
| 13 | `test_woset_sip_dis` | 5b | Write 0x0 to `SIP_DIS_LO` (attempt to clear bit[0]). Wait 1 ns. Read `SIP_DIS_LO`. Verify `val == 0x1` (WOSET: write-0 has no effect; bit[0] persists). | `SIP_DIS_LO` (write 0x0, read) | `val == 0x1` | Positive |
| 14 | `test_woset_sip_dis` | 5c | Write 0x6 to `SIP_DIS_LO` (set bits[1:2] = soc_debug and ap_debug disable). Wait 1 ns. Read `SIP_DIS_LO`. Verify `val == 0x7` (OR of 0x1 from 5a and 0x6 from this write). | `SIP_DIS_LO` (write 0x6, read) | `val == 0x7` | Positive |
| **WOSET — SYS_DIS_LO** | | | | | | |
| 15 | `test_woset_sys_dis` | 6a | Write 0x3 to `SYS_DIS_LO` (set bits[0:1]). Wait 1 ns. Read `SYS_DIS_LO`. Verify `val == 0x3`. | `SYS_DIS_LO` (write 0x3, read) | `val == 0x3` | Positive |
| 16 | `test_woset_sys_dis` | 6b | Write 0x0 to `SYS_DIS_LO`. Wait 1 ns. Read `SYS_DIS_LO`. Verify `val == 0x3` (WOSET holds; write-0 has no effect). | `SYS_DIS_LO` (write 0x0, read) | `val == 0x3` | Positive |
| **EFUSE INTERFACE CONTROL** | | | | | | |
| 17 | `test_efuse_sense_done` | 7a | Read `EFUSE_INTERFACE_CTRL_STATUS` at offset 0x400. Verify `val == 0x00000001` (bit[0]=efuse_sense_done is always 1 in VP; the model resets this register to 1 to indicate sense completion at power-on). | `EFUSE_INTERFACE_CTRL_STATUS` (read) | `val == 0x00000001` | Positive |
| 18 | `test_efuse_sense_done` | 7b | Write 0x00000000 to `EFUSE_INTERFACE_CTRL_STATUS` (attempt to clear sense-done). Wait 1 ns. Read register. Verify `val == 0x00000001` (write silently ignored; `write_mask=0x0`). | `EFUSE_INTERFACE_CTRL_STATUS` (write 0x0, read) | `val == 0x00000001` | Positive |
| **EFUSE READ CTRL** | | | | | | |
| 19 | `test_efuse_read_ctrl_rw` | 8a | Write 0x00001234 to `EFUSE_READ_CTRL` at offset 0x408. Wait 1 ns. Read register. Verify `val == 0x00001234`. Confirms EFUSE_READ_CTRL is fully R/W with no access restrictions. | `EFUSE_READ_CTRL` (write 0x1234, read) | `val == 0x00001234` | Positive |
| 20 | `test_efuse_read_ctrl_rw` | 8b | Write 0xABCD5678 to `EFUSE_READ_CTRL` (overwrite prior value). Wait 1 ns. Read register. Verify `val == 0xABCD5678`. Confirms the register is fully overwritable (no WOSET, no lock). | `EFUSE_READ_CTRL` (write 0xABCD5678, read) | `val == 0xABCD5678` | Positive |

---

## Coverage Goals and Metrics

### Functional Coverage Targets

1. **Register Access Type Coverage: 100%**

   | Access Type | Example Registers | Tests | Coverage |
   |-------------|-----------------|-------|---------|
   | RO (shadow fuse) | `SBOOT_DIS`, `CHIPLET_UID` | 1, 2, 3 | Load verified + write rejected |
   | RW WOSET (scalar) | `LOCKS_LO`, `SIP_DIS_LO`, `SYS_DIS_LO` | 4, 5, 6 | Multi-write OR accumulation; write-0 no-effect |
   | RW WOSET (array) | `BL1_VERSION`, `BL2_VERSION` | Not tested | Gap — see below |
   | RO w/ hw=1 | `EFUSE_INTERFACE_CTRL_STATUS` | 7 | Always-1 verified; write rejected |
   | RW unrestricted | `EFUSE_READ_CTRL` | 8 | Multi-overwrite verified |

2. **Shadow Register Load Coverage**

   | Category | Registers Tested | Coverage |
   |----------|-----------------|---------|
   | Scalar RO | LC_STATE, SBOOT_DIS, STATUS_RPT | Verified against param |
   | WOSET initial (scalar) | CHIPLET_PUBK_REVOKE | Verified against param |
   | Array 256-bit | SIP_PUBK[0:7], CHIPLET_UID[0:7] | All 8 words verified |
   | Array WOSET | BL1_VERSION, BL2_VERSION | Not tested — gap |
   | Token arrays | RMA_SIP_TOKEN, CLASS_KEY | Not tested — gap |

3. **WOSET Channels Coverage**

   | Register | OR accumulation | Write-0 no-effect | Multi-write | Coverage |
   |----------|----------------|-------------------|-------------|---------|
   | LOCKS_LO | ✓ (4c) | ✓ (4b) | ✓ (4a→4c) | Full |
   | SIP_DIS_LO | ✓ (5c) | ✓ (5b) | ✓ (5a→5c) | Full |
   | SYS_DIS_LO | ✓ (6a) | ✓ (6b) | ✓ (6a→6b) | Partial (no 3-step accumulation) |
   | LC_STATE | Initial load verified | Not tested | — | Partial |
   | SIP_DIS_HI | — | — | — | Gap |
   | SYS_DIS_HI | — | — | — | Gap |
   | CHIPLET_PUBK_REVOKE | Initial load verified | — | — | Partial |
   | BL1_VERSION | — | — | — | Gap |
   | BL2_VERSION | — | — | — | Gap |
   | LOCKS_HI | — | — | — | Gap |

### Pass/Fail Criteria

**Each sub-test passes if:**
1. The read register value exactly matches the expected value (no partial match accepted)
2. Write-protected registers return the same value before and after a write attempt
3. WOSET registers hold accumulated bits and do not clear on write-0

**Overall test plan passes if:**
- All 20 sub-tests across 8 test methods pass with the default ini configuration
- Test 1 and 2 values are consistent with what is in `config/accellera_config.ini`

---

## Known Test Gaps (Recommended Future Tests)

| Gap | Priority | Proposed Test |
|-----|----------|---------------|
| `BL1_VERSION[0:7]` WOSET — array WOSET per-word | High | Write 0x1 to `BL1_VERSION[0]`, verify 0x1; write 0x0, verify 0x1; write 0x2 to `BL1_VERSION[0]`, verify 0x3; write 0x1 to `BL1_VERSION[1]`, verify word 1=0x1, word 0=0x3 |
| `BL2_VERSION[0:7]` WOSET | Medium | Same as BL1_VERSION test |
| `LC_STATE` WOSET runtime write | High | Write 0x01 to LC_STATE (set bit[0]); verify accumulated value equals `lc_state.get_param_value() \| 0x1` |
| `SIP_DIS_HI` WOSET | Medium | Write 0x1 (fuse_test disable), verify; write 0x0, verify hold |
| `SYS_DIS_HI` WOSET | Medium | Same as SIP_DIS_HI |
| `LOCKS_HI` WOSET | Low | Write 0x3 (STATUS_RPT write/read lock), verify OR accumulation |
| `CHIPLET_PUBK_REVOKE` WOSET runtime | Medium | Write 0x5 to CHIPLET_PUBK_REVOKE; verify `ini_value \| 0x5` |
| `RMA_SIP_TOKEN[0:7]` fuse load | Medium | Verify all 8 words of RMA_SIP_TOKEN match `rma_sip_token` param vector |
| `CLASS_KEY[0:7]` fuse load | Low | Verify all 8 words match `class_key` param vector |
| `TRANSIENT_RMA_EN` fuse load | Low | Verify `TRANSIENT_RMA_EN == transient_rma_en.get_param_value()` |
| `SIP_DIS_LO/HI` fuse load | Medium | Set non-zero `sip_dis_lo: 0xFF` in ini, verify Test 1 reads 0xFF |
| `TOKEN_EOP` is write-only | Low | Read `TOKEN_EOP`, verify 0x0 (WO: read returns 0); write 0x1, wait, re-read 0x0 |
| `RMA_SIP_TOKEN_MATCH` preset | Medium | Set `rma_sip_token_match: 21` in ini, verify register reads 0x15 |
| `TOKEN_I[0:7]` are RW | Low | Write pattern to `RMA_SIP_TOKEN_I[0]`, verify read-back |
| `EFUSE_PROGRAM_CTRL` RW stub | Low | Write and read back pattern to `EFUSE_PROGRAM_CTRL` |
| RO write-protection on `RMA_SIP_TOKEN[0]` | Low | Read, write, re-read; verify unchanged |
| Full ini combo: all-zero fuses | Medium | Set all params to 0, verify all shadow registers read 0 |

---

## Implementation Notes

### Test Environment Architecture

The testbench uses the shared framework in `testbench.h` / `testbench.cpp`:

- `sep_efuse_model` instantiated as `"sep_efuse_dut"` using CCI params from ini file
- `sep_efuse_test` instance providing `register_read_32()` and `register_write_32()` helpers
- TLM initiator-to-target socket binding for all register transactions
- TLM quantum set to 100 ns; tests begin after 10 ns wait
- SC_THREAD runs all 8 test methods sequentially then calls `sc_stop()`

### Register Access Helpers (`sep_efuse_test.cpp`)

All tests use shared transport helpers:
- `register_read_32(offset, &value)` — TLM READ with `SC_ZERO_TIME` delay, returns 32-bit value
- `register_write_32(offset, value)` — TLM WRITE with `SC_ZERO_TIME` delay

### Register Offset Constants (`sep_efuse_basetest.h`)

All register offsets are defined as enums in `sep_efuse_basetest`:

| Constant | Value | Register |
|----------|-------|---------|
| `LC_STATE_OFFSET` | 0x008 | LC_STATE |
| `SBOOT_DIS_OFFSET` | 0x00C | SBOOT_DIS |
| `SIP_DIS_LO_OFFSET` | 0x014 | SIP_DIS_LO |
| `SYS_DIS_LO_OFFSET` | 0x01C | SYS_DIS_LO |
| `CHIPLET_PUBK_REVOKE_OFFSET` | 0x084 | CHIPLET_PUBK_REVOKE |
| `CHIPLET_UID_OFFSET` | 0x0C8 | CHIPLET_UID[0] |
| `SIP_PUBK_OFFSET` | 0x0E8 | SIP_PUBK[0] |
| `STATUS_RPT_OFFSET` | 0x168 | STATUS_RPT |
| `LOCKS_LO_OFFSET` | 0x000 | LOCKS_LO |
| `EFUSE_INTF_STATUS_OFFSET` | 0x400 | EFUSE_INTERFACE_CTRL_STATUS |
| `EFUSE_READ_CTRL_OFFSET` | 0x408 | EFUSE_READ_CTRL |

### Test Execution Order

```
1. test_fuse_load_ro_registers     — 4 sub-tests
2. test_fuse_load_array_registers  — 2 sub-tests
3. test_ro_write_protection        — 2 sub-tests
4. test_woset_locks                — 3 sub-tests (LOCKS_LO ends at 0xF3)
5. test_woset_sip_dis              — 3 sub-tests (SIP_DIS_LO ends at 0x7)
6. test_woset_sys_dis              — 2 sub-tests (SYS_DIS_LO ends at 0x3)
7. test_efuse_sense_done           — 2 sub-tests
8. test_efuse_read_ctrl_rw         — 2 sub-tests
```

> **Ordering note:** WOSET tests accumulate state across sub-tests within the same method (e.g., 4a sets bits that 4b tries to clear, and 4c then adds more). Tests in different methods operate on different registers, so method ordering does not affect correctness.

### Running the Tests

```bash
./sep_efuse_test                                  # default ini (all-zero fuses + test data)
./sep_efuse_test config/accellera_config.ini      # explicit ini (same defaults, all combos documented)
```

For Tests 1 and 2 to produce interesting non-zero values, the ini file must set:
```ini
lc_state: 1
status_rpt: 1
chiplet_uid: [2684354560, 2684354561, ...]   # 0xA0000000+i
sip_pubk:    [2952790016, 2952790017, ...]   # 0xB0000000+i
```

---

## Conclusion

This test plan covers all 8 implemented `sep_efuse` test methods through 20 sub-tests. The plan is derived from the implemented model and describes what is currently tested.

Key characteristics:
- **Tests 1–2** verify the ini-to-register pipeline: CCI params → `load_fuses()` → CSML shadow registers
- **Test 3** verifies the access-control boundary: RO registers reject all writes
- **Tests 4–6** verify WOSET semantics for three independent register groups
- **Tests 7–8** verify the interface-control register access types (permanently-1 RO and unrestricted RW)

Identified gaps (array WOSET registers `BL1/BL2_VERSION`, `LC_STATE` runtime WOSET, upper-word dis vectors `SIP/SYS_DIS_HI`, token-related registers) should be addressed as the firmware integration test matrix expands.

---

**End of Test Plan**
