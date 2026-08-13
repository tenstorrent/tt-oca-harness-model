# sep_output_remap_ctrl SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `sep_output_remap_ctrl` SystemC TLM model — a fixed 16-region, index-selected address remapper, instantiated twice (`InstanceType::AP` and `InstanceType::STEE`) from the same class with different `REGION_BASE` constants. Unlike `local_master_alias_remap_ctrl` (range-matching, additive offset, first-match priority), this model selects a region directly from address bits and substitutes the upper bits wholesale from a per-region 56-bit offset table — a smaller, simpler algorithm, and the test plan below is sized accordingly: it covers the 13 cases the existing testbench implements (10 for the AP instance, 3 for STEE) and does not add categories the model has no room for (no priority/overlap logic exists — regions are addressed by index, not by range).

### 1.1 Test Objectives

- Verify all 16 `REGION_ATTRS` entries reset to 0
- Verify single-region and all-16-region CSR programming and readback
- Verify the 56-bit write mask on `REGION_ATTRS.offset` (bits `[63:56]` discarded)
- Verify data-path remap on a read (region 0) and a write (region 3), matching the index-select-and-substitute algorithm exactly
- Verify the caller's original transaction address is restored after the DUT forwards the (temporarily rewritten) payload
- Verify the CSR write path enforces the 56-bit mask even when byte-enables are supplied (`csml_memory` ignores byte enables)
- Verify `transport_dbg` applies the identical remap as `b_transport`
- Verify `rst_ni` clears previously-programmed, non-zero region state (not just the power-on-reset state)
- Verify the AP and STEE instances (different `REGION_BASE`, independently constructed) do not share register or remap state

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`sep_output_remap_ctrl_testbench.cpp`) instantiating **two** DUTs — `dut_ap` (`InstanceType::AP`, `REGION_BASE=0x11000000`) and `dut_stee` (`InstanceType::STEE`, `REGION_BASE=0x11800000`) — each with its own CSR harness, data-path initiator, and remap-observing stub
- `sep_output_remap_ctrl_test` — thin CSR-path TLM helper (`register_read/write_64/8`) bound to `dut.target_socket`
- `StubTarget` — records the last address seen on the data-path output (`dut.remapped_socket`), used as the remap oracle for both instances
- No reference model — expected remapped addresses are computed inline per test from the documented algorithm (`adjusted = addr - REGION_BASE`; `idx = adjusted[IDX_START+log2(NUM_REGIONS)-1:IDX_START]`; `remapped = {REGION_ATTRS[idx][55:IDX_START], adjusted[IDX_START-1:0]}`)

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/sep_output_remap_ctrl.h`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `sep_output_remap_ctrl_base`) for CSR access to the 16-entry `REGION_ATTRS` table (`0x8`-byte stride per entry)
- `data_socket` — AXI data-path slave; incoming transactions to be remapped
- `remapped_socket` — AXI data-path master; remapped transactions forwarded to the fabric
- `rst_ni` — active-low asynchronous reset; clears all `REGION_ATTRS` entries when asserted

There is no clock port — like `local_master_alias_remap_ctrl`, this is a combinational, quantum-free LT model.

## 2. Test Plan Table

Test cases are inline blocks inside `run_tests()` (there are no discrete `test_*` functions); the "TestCase Name" column uses the exact `report(...)` label string from the source.

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **Suite A — AP Instance: Reset and Register Access Tests** |
| 1 | T1: All region offsets reset to 0 | Verify all 16 `REGION_ATTRS` entries read back as 0 on a freshly-constructed AP DUT | REGION_ATTRS[0-15] (AP) | `target_socket` (AP) | Positive |
| 2 | T2: Region 0 offset write/readback | Write a 56-bit-aligned offset to `REGION_ATTRS[0]` and verify readback matches (masked) | REGION_ATTRS[0] (AP) | `target_socket` (AP) | Positive |
| 3 | T3: All 16 region offsets written and verified | Program a distinct offset into every one of the 16 entries and verify each reads back its own masked value | REGION_ATTRS[0-15] (AP) | `target_socket` (AP) | Positive |
| 4 | T4: 56-bit mask enforced on REGION_ATTRS.offset | Write all-ones to `REGION_ATTRS[0]` and verify only the low 56 bits (`0x00FFFFFFFFFFFFFF`) are retained | REGION_ATTRS[0] (AP) | `target_socket` (AP) | Negative |
| **Suite A — AP Instance: Data-Path Remap Tests** |
| 5 | T5: Data-path remap region 0 (READ) | Program `REGION_ATTRS[0].offset = 0x0040_0000_0000`; a read at `AP_BASE + 0x1234` (index bits select region 0) remaps to `0x4000001234` — upper bits substituted from the table, lower `IDX_START` (19) bits of the adjusted address preserved | REGION_ATTRS[0] (AP) | `data_socket`, `remapped_socket` (AP) | Positive |
| 6 | T6: Data-path remap region 3 (WRITE) | Program `REGION_ATTRS[3].offset = 0xF0000000`; a write at `AP_BASE + (3<<19) + 0xABC` (index bits select region 3) remaps to `0xF0000ABC` | REGION_ATTRS[3] (AP) | `data_socket`, `remapped_socket` (AP) | Positive |
| 7 | T7: Original address restored on payload after remap | After `b_transport` returns, the caller's transaction `get_address()` still reads the *input* address — confirms the DUT rewrites the address only on the forwarded copy, not on the caller's payload | — (region 0, AP) | `data_socket` (AP) | Positive |
| 8 | T8: 56-bit field mask enforced (byte enables not honoured by csml) | Write all-ones to `REGION_ATTRS[5]` with only the low 4 bytes marked `TLM_BYTE_ENABLED`; verify the full 56-bit mask is still applied on readback (`csml_memory` performs a full 8-byte write regardless of byte-enable content) | REGION_ATTRS[5] (AP) | `target_socket` (AP) | Negative |
| 9 | T9: transport_dbg applies same remap | Re-program `REGION_ATTRS[0]` (offset from T5) and confirm a `transport_dbg` read at the same input address produces the identical remapped address as `b_transport` did in T5 | REGION_ATTRS[0] (AP) | `data_socket`, `remapped_socket` (AP) | Positive |
| **Suite A — AP Instance: Reset-Clears-State Test** |
| 10 | T10: rst_ni pulse clears previously-programmed regions | Confirm `REGION_ATTRS[0]` is still non-zero from T9, drive a real `rst_ni` pulse (assert 10 ns, deassert), verify all 16 entries read back as 0 afterward | REGION_ATTRS[0-15] (AP) | `target_socket` (AP), `rst_ni` | Positive |
| **Suite B — STEE Instance: Dual-Instantiation Sanity** |
| 11 | B1: STEE instance reset state independent of AP instance | Verify all 16 `REGION_ATTRS` entries on the separately-constructed STEE DUT read back as 0, independent of whatever Suite A left in the AP DUT | REGION_ATTRS[0-15] (STEE) | `target_socket` (STEE) | Positive |
| 12 | B2: STEE data-path remap region 0 (READ) | Program `REGION_ATTRS[0].offset` on the STEE DUT and verify a read at `STEE_BASE + 0x1234` remaps correctly using `STEE_REGION_BASE`/`STEE_IDX_START` — confirms the same algorithm works with a different per-instance base address | REGION_ATTRS[0] (STEE) | `data_socket`, `remapped_socket` (STEE) | Positive |
| 13 | B3: AP CSR write does not affect STEE register | Write a distinct value to `REGION_ATTRS[2]` on the AP DUT and verify `REGION_ATTRS[2]` on the STEE DUT still reads 0 — confirms the two instances do not share backing storage | REGION_ATTRS[2] (AP), REGION_ATTRS[2] (STEE) | `target_socket` (AP and STEE) | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

1. **Suite A — Reset and Register Access** (rows 1–4): CSR reset value, single/all-region programming, 56-bit mask
2. **Suite A — Data-Path Remap** (rows 5–9): index-select-and-substitute algorithm on read, write, address-restoration, byte-enable-vs-mask interaction, and `transport_dbg` parity
3. **Suite A — Reset-Clears-State** (row 10): live reset pulse against previously-programmed, non-zero state
4. **Suite B — STEE Instance** (rows 11–13): independent reset state, remap with a different base address, cross-instance isolation

Suite A runs to completion (T1–T10) before Suite B starts; T10 deliberately runs last within Suite A so it can assert that a real reset pulse clears state left behind by T2/T3/T5/T6/T9.

### 3.2 Test Dependencies

- Row 6 depends on row 5 having already exercised the algorithm once (not a data dependency, but the same derivation pattern)
- Row 7 depends on region 0 still being configured from row 5
- Row 9 re-programs `REGION_ATTRS[0]` itself, so it does not strictly depend on row 5's programming surviving, but reuses row 5's expected-value derivation
- Row 10 depends on rows 2/3/5/6/9 having left `REGION_ATTRS[0]` (and others) non-zero and valid before the reset pulse
- Row 13 depends on the AP and STEE DUTs being genuinely separate instances (constructor-level, not data-dependent on any prior row)

### 3.3 Pass/Fail Criteria

- **Register Access**: readback matches the written value, masked to the documented 56-bit write mask
- **Remap**: `StubTarget::last_addr` (the address observed on `remapped_socket`) matches the hand-computed expected address from the index-select-and-substitute algorithm
- **Address restoration**: the caller-side transaction's `get_address()` is unchanged by the call
- **Reset**: all 16 entries (on the instance that was reset) read back as 0

### 3.4 Test Coverage Metrics

- **Register Coverage**: `REGION_ATTRS` exercised on entries 0, 2, 3, 5 individually plus a full 0–15 sweep (T3) on the AP instance; entry 0 on the STEE instance
- **Remap Path Coverage**: region-0 read, region-3 write, address restoration, byte-enable/mask interaction, `transport_dbg`, and a second base address (STEE) are all exercised
- **Reset Coverage**: power-on state (T1, B1) and mid-run reset-after-programming (T10) both exercised — T10 only on the AP instance, not repeated on STEE
- **Instance Coverage**: both `InstanceType::AP` and `InstanceType::STEE` exercised, plus cross-instance isolation (B3)

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- SystemC TLM-2.0 testbench with, per instance, two independent socket paths: a CSR path (`harness_{ap,stee}.initiator_socket -> dut.target_socket`) and a data path (`data_init_{ap,stee} -> dut.data_socket -> dut.remapped_socket -> stub_{ap,stee}.socket`)
- `sep_output_remap_ctrl_test` — generic 64-bit/8-bit TLM register read/write helper (shared code between AP and STEE harnesses)
- `StubTarget` — minimal target recording the last address it received
- `report(...)` pass/fail counters (`m_tests_run`/`m_tests_passed`/`m_tests_failed`); `sc_main` returns a nonzero exit code if any test failed, via `std::quick_exit`

### 4.2 Reference Models

None — expected remap addresses are computed by hand from the index-select-and-substitute algorithm and hardcoded per test as `expected`/`expected_addr`.

## 5. Notes

### 5.1 Important Notes

- The IP has **no runtime-configurable parameters** for region count/base — `REGION_BASE`/`NUM_REGIONS`/`IDX_START` are fixed per `InstanceType` and passed as constructor arguments (`const` members), not CCI/`csml_param`-backed.
- Unlike `local_master_alias_remap_ctrl`, there is **no range matching or first-match priority** in this model — every address is unconditionally assigned to a region by its index bits, so there is no "no region match / passthrough" case to test.
- T8 specifically documents a `csml_memory` implementation detail (byte enables on the CSR write path are not honoured — the register always does a full-width write) rather than a hardware behavior per se; it is retained here because it is part of the model's actual, observable write semantics.

### 5.2 Important Exclusions

- **Regions 1, 2 (except as a write-only isolation target in B3), 4, and 6–15 on the data path**: only regions 0 and 3 are driven through `data_socket`/`remapped_socket`; the remaining regions are only ever checked via CSR readback (T1, T3, T10), not through an actual remap.
- **STEE reset-clears-state**: T10's live `rst_ni` pulse is only exercised on the AP instance; the STEE instance's reset-after-programming behavior is not separately re-verified (B1 only checks its initial power-on state).
- **Boundary/edge addresses at the region-index bit boundary** (e.g. one address below/above an index transition) are not specifically probed — T5/T6/B2 each pick one address well inside a given region's window.
- **Cycle-accurate timing**: the model has no clock and returns `SC_ZERO_TIME` delay on every transaction; no timing behavior is validated.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_sep_output_remap_ctrl_HighLevel_Design.md` — remap algorithm (index-select-and-substitute, `IDX_START`/`NUM_REGIONS` granularity) and register layout
- **Source of truth**: `sep_output_remap_ctrl_testbench.cpp` — every row above corresponds 1:1 to a `report(...)` call in `run_tests()`

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 13-test (T1–T10, B1–B3) `sep_output_remap_ctrl_testbench.cpp` suite |
