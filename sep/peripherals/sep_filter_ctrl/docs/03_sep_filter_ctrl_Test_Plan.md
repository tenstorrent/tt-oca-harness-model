# sep_filter_ctrl SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `sep_filter_ctrl` SystemC TLM model — the generic AXI address-range filter used for both the SEP outbound filter (32 entries) and inbound filter (16 entries), instantiated twice from the same class via `InstanceType::OUTBOUND`/`INBOUND`. The existing suite (`sep_filter_ctrl_testbench.cpp` driving shared test methods in `sep_filter_ctrl_test.cpp`) is organized as two suites — Suite A (outbound, 32 entries) and Suite B (inbound, 16 entries) — and this plan documents exactly what each covers. It does not add rows for behavior the current tests do not exercise; those gaps are listed in §5.2.

### 1.1 Test Objectives

- Verify `FILTER_CONFIG`/`START_ADDR`/`END_ADDR` reset values across multiple entries, including the hardware-fixed `data_bus_width` field
- Verify basic CSR read/write on the configurable `FILTER_CONFIG` bits (`read_allowed`, `write_allowed`, `entry_enabled`, `allow_ns`, `allow_burst`) and on `START_ADDR`/`END_ADDR`
- Verify the `locked` bit (bit 63) is WOSET — settable but never clearable by software
- Verify `data_bus_width` (bits `[14:12]`) is hardware-read-only and always reads `3` regardless of what software writes there
- Verify clearing `FILTER_CONFIG` clears `entry_enabled` (CSR-level "unconfigured" state) — while separately verifying (data-path tests) that "unconfigured" does **not** mean passthrough
- Verify multiple simultaneously-configured entries retain independent CSR state (read-only, write-only, read+write combinations)
- Verify the data-path `BlockByDefault` behavior: with no entries enabled, every transaction is denied
- Verify default-deny when an entry is enabled but the accessed address falls outside its range
- Verify a permitted read within a configured entry's range is forwarded to `filtered_socket`
- Verify `transport_dbg` (the debug/GDB path) applies the identical filter logic as `b_transport`, both for permit and deny
- Verify the inbound instance's reset state and per-entry enforcement are independent of the outbound instance, and that writing the outbound instance's CSRs has no effect on the inbound instance

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`SepFilterCtrlTestbench` in `sep_filter_ctrl_testbench.cpp`) instantiating **two** DUTs — `outbound_dut` (`InstanceType::OUTBOUND`, 32 entries) and `inbound_dut` (`InstanceType::INBOUND`, 16 entries) — each with its own CSR harness, data-path initiator, and filtered-output stub
- `sep_filter_ctrl_test` (extends `sep_filter_ctrl_basetest`) — CSR-path TLM helper (`register_read/write_8/64`, plus `csr_read_64`/`csr_write_64` which add the per-instance `0x20`-byte stride) bound to `dut.target_socket`; also owns the six shared `test_*` methods used by both suites
- `DataInitiator` — generates data-path `read`/`write`/`dbg_read` transactions into `dut.data_socket`
- `FilteredStub` — records the last address/data/command forwarded to `dut.filtered_socket`, and whether anything was received at all (used to distinguish "denied" from "permitted but address 0")
- No reference model — expected values/behaviors are hardcoded per test (masks, reset values, and permit/deny outcomes come from the RTL-derived comments in `sep_filter_ctrl.h`)

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/sep_filter_ctrl.h`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `sep_filter_ctrl_base`) for CSR access to the per-entry `FILTER_CONFIG`/`START_ADDR`/`END_ADDR` table (`0x20`-byte stride per entry)
- `data_socket` — AXI data-path slave; incoming transactions to be filtered
- `filtered_socket` — AXI data-path master; transactions forwarded here only if permitted
- `rst_ni` — active-low asynchronous reset. **Note:** the testbench writes `rst_n_sig` high once in its constructor and never pulses it low — so no test in this suite exercises a live reset; "reset values" here means the freshly-constructed DUT's power-on state, not a reset-clears-programmed-state check (contrast with `local_master_alias_remap_ctrl`'s T11 or `sep_cpu_ctrl`'s reset pulse, which do test this).

## 2. Test Plan Table

Suite A tests (A1–A6) call named `test_*` methods on `sep_filter_ctrl_test` (shared code, reused verbatim by Suite B); A7–A10 and B2–B3 are inline blocks in the testbench. The "TestCase Name" column uses the label from the source comment/console string.

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **Suite A — Outbound Filter (32 entries): CSR Tests** |
| 1 | A1: test_reset_values(32) | Verify entries 0–3's `FILTER_CONFIG` (`data_bus_width` field reads `3`, all other bits `0`), `START_ADDR` (`0`), and `END_ADDR` (`0x7`, the minimum 8-byte granularity) on a freshly-constructed DUT | FILTER_CONFIG[0-3], START_ADDR[0-3], END_ADDR[0-3] | `target_socket` | Positive |
| 2 | A2: test_register_access_basic | Write `read_allowed`/`write_allowed`/`entry_enabled`/`allow_ns`/`allow_burst` plus `START_ADDR`/`END_ADDR` on entry 0 and verify readback; confirm `data_bus_width` is unaffected | FILTER_CONFIG[0], START_ADDR[0], END_ADDR[0] | `target_socket` | Positive |
| 3 | A3: test_woset_locked_field | Verify entry 1's `locked` bit (bit 63) starts at 0, can be set, and cannot be cleared by a subsequent write of 0 (WOSET) | FILTER_CONFIG[1] | `target_socket` | Negative |
| 4 | A4: test_hw_readonly_data_bus_width | Write several different `data_bus_width` field values (`0x0000`–`0x7000`) to entry 2 and verify the field always reads back `3` regardless | FILTER_CONFIG[2] | `target_socket` | Negative |
| 5 | A5: test_passthrough_when_unconfigured(32) | Clear `FILTER_CONFIG` on entries 0–7 and verify `entry_enabled` reads back 0 and `data_bus_width` still reads 3 — a CSR-level check only; despite the method's name, this does **not** verify data-path passthrough (see A7) | FILTER_CONFIG[0-7] | `target_socket` | Positive |
| 6 | A6: test_comprehensive_filter_scenarios | Program entry 0 (read+write, `[0x1000,0x2000)`), entry 1 (read-only, `[0x3000,0x4000)`), entry 2 (write-only, `[0x5000,0x6000)`); verify entry 0's CSR readback (`START_ADDR`/`END_ADDR`/`FILTER_CONFIG` bits and `data_bus_width`) — entries 1/2 are programmed but their CSR content is not independently re-read/asserted | FILTER_CONFIG[0-2], START_ADDR[0-2], END_ADDR[0-2] | `target_socket` | Positive |
| **Suite A — Outbound Filter: Data-Path Tests** |
| 7 | A7: data path BlockByDefault | Clear all 32 entries, attempt a data-path write; verify it is denied (`TLM_ADDRESS_ERROR_RESPONSE`) and never reaches `filtered_socket` — confirms "no entries enabled" denies all, it does not pass through | FILTER_CONFIG[0-31] | `data_socket`, `filtered_socket` | Negative |
| 8 | A8: default deny | Configure entry 0 to `[0x10000,0x20000)`, read+write enabled; attempt a write at `0x30000` (outside the range); verify denied and not forwarded | FILTER_CONFIG[0], START_ADDR[0], END_ADDR[0] | `data_socket`, `filtered_socket` | Negative |
| 9 | A9: data path read permitted | Using the same entry 0 range from A8, read `0x15000` (inside range); verify permitted and forwarded to `filtered_socket` | — (entry 0 from A8) | `data_socket`, `filtered_socket` | Positive |
| 10 | A10: data_transport_dbg | Using entry 0's range, `dbg_read` (via `transport_dbg`) at `0x15000` (inside) returns nonzero and is forwarded; `dbg_read` at `0x30000` (outside) returns `0` and is not forwarded — confirms `transport_dbg` applies the same filter logic as `b_transport` | — (entry 0 from A8/A9) | `data_socket`, `filtered_socket` | Positive |
| **Suite B — Inbound Filter (16 entries)** |
| 11 | B1: test_reset_values(16) | Same reset-value check as A1, run against `inbound_dut` — confirms the inbound instance's independent 16-entry table starts in the same reset state | FILTER_CONFIG[0-3], START_ADDR[0-3], END_ADDR[0-3] (inbound) | `target_socket` (inbound) | Positive |
| 12 | B2: inbound per-entry enforcement | Configure inbound entry 0 as write-only (`write_allowed=1`, `read_allowed=0`) over `[0x80000,0x90000)`; verify a read inside the range is denied and a write inside the range is permitted and forwarded | FILTER_CONFIG[0], START_ADDR[0], END_ADDR[0] (inbound) | `data_socket`, `filtered_socket` (inbound) | Positive/Negative (mixed) |
| 13 | B3: outbound write does not affect inbound | Program outbound entry 5 (read+write, `[0x99999000,0xAAAA0000)`); verify inbound entry 5's `entry_enabled` bit still reads 0 — confirms the two instances do not share backing storage | FILTER_CONFIG[5] (outbound), FILTER_CONFIG[5] (inbound) | `target_socket` (outbound and inbound) | Positive |
| 13a | A11: read-only entry denies writes | Configure entry 3 read-only over `[0x40000,0x50000)`; verify a write inside the range is denied and not forwarded, and a read is permitted and forwarded. Uses entry 3 because A3 locks entry 1 | FILTER_CONFIG[3], START_ADDR[3], END_ADDR[3] | `data_socket`, `filtered_socket` | Positive/Negative (mixed) |
| **Suite C — `filter_skip_i` bypass (`skip_dut`, skip strapped high)** |
| 14 | C1: skip bypasses BlockByDefault | With no entry enabled, write `0xABCD1000` and verify it is permitted and forwarded. A7 proves the same access is denied when skip is low, so the outcome is attributable to the skip input | — (none configured) | `data_socket`, `filtered_socket` (skip) | Positive |
| 15 | C2: skip bypasses permission denial | Configure entry 0 write-only over `[0x80000,0x90000)`, then read `0x85000`; verify permitted and forwarded — skip defeats the per-entry permission check, not just the match | FILTER_CONFIG[0], START_ADDR[0], END_ADDR[0] (skip) | `data_socket`, `filtered_socket` (skip) | Positive |
| 16 | C3: skip bypasses the dbg path | `dbg_read` at `0x30000` (outside every entry) returns nonzero and is forwarded — confirms `transport_dbg` honours skip identically | — (entry 0 from C2) | `data_socket`, `filtered_socket` (skip) | Positive |
| **Suite D — Reset (the only suite that runs the scheduler; must run last)** |
| 17 | D1: `rst_ni` clears configuration | Start the scheduler to commit reset deasserted and confirm configuration survives, then drive `rst_ni` low and confirm `FILTER_CONFIG` returns to `0x3000` (`data_bus_width` is hw=w), `START_ADDR` to 0 and `END_ADDR` to `0x7`. Exercises `reset_handler`, which no other suite can reach because an `SC_METHOD` needs the scheduler | FILTER_CONFIG[0], START_ADDR[0], END_ADDR[0] | `rst_ni`, `target_socket` | Positive |
| 18 | D2: filtering denies after reset | Re-attempt the A9 read at `0x15000`, previously permitted, and confirm it is now denied and not forwarded — reset restores deny-by-default rather than leaving the datapath open | — (all entries cleared) | `data_socket`, `filtered_socket` | Negative |

## 3. Test Execution Strategy

### 3.1 Test Grouping

1. **Suite A — CSR Tests** (rows 1–6): reset values, basic access, WOSET lock, HW-readonly field, CSR-clear-to-unconfigured, multi-entry programming — all via the CSR (`target_socket`) path only
2. **Suite A — Data-Path Tests** (rows 7–10): `BlockByDefault` deny-all, default deny outside range, permitted read, `transport_dbg` parity — via `data_socket`/`filtered_socket`
3. **Suite B — Inbound** (rows 11–13): reset independence, per-entry enforcement, cross-instance isolation

Suite A runs to completion before Suite B starts (`run_suite_a()` then `run_suite_b()`), and B3 deliberately writes to the *outbound* DUT to prove isolation from the already-tested inbound state.

### 3.2 Test Dependencies

- Row 8 depends on row 7 (all entries cleared first)
- Rows 9 and 10 depend on row 8's entry-0 configuration (`[0x10000, 0x20000)`, read+write enabled) still being active
- Row 13 depends on row 12 having left inbound entry 0 configured (unaffected, since row 13 checks entry 5) and on the outbound/inbound DUTs being genuinely separate instances

### 3.3 Pass/Fail Criteria

- **CSR tests**: readback matches the written value (masked to the field's write mask) or the documented reset value
- **Locked/HW-readonly fields**: value is unaffected by the software write that should be rejected (WOSET or hw=w)
- **Data-path permit**: `DataInitiator::write`/`read`/`dbg_read` returns success/nonzero, and `FilteredStub::received` is `true` with the expected address
- **Data-path deny**: the call returns failure/zero, and `FilteredStub::received` remains `false` (i.e. the denial happens before forwarding, not after)

### 3.4 Test Coverage Metrics

- **Register Coverage**: `FILTER_CONFIG`, `START_ADDR`, `END_ADDR` all exercised, on both the outbound (entries 0, 1, 2, 5, and the 0–7/0–31 bulk-reset checks) and inbound (entries 0, 5, and the 0–3 bulk-reset check) instances
- **Field Coverage**: `read_allowed`, `write_allowed`, `entry_enabled`, `allow_ns`, `allow_burst`, `data_bus_width` (hw=w), `locked` (WOSET) all exercised
- **Filter Outcome Coverage**: deny-by-BlockByDefault, deny-by-out-of-range, permit-by-range-match, the `read_allowed`/`write_allowed` command-gating combination (via B2's write-only entry), and bypass-by-`filter_skip_i` (Suite C) are all exercised
- **API Coverage**: both `b_transport` and `transport_dbg` data-path entry points are exercised, each with skip low and skip high
- **Instance Coverage**: both `InstanceType::OUTBOUND` (32 entries) and `InstanceType::INBOUND` (16 entries) are exercised, plus cross-instance isolation
- **Port Coverage**: `filter_skip_i` is exercised both unbound (Suites A and B — reads 0) and strapped high (Suite C); `rst_ni` is exercised deasserted and asserted (Suite D)
- **Line/Function Coverage**: 95.2% of lines and 97.8% of functions across the four
  hand-written model sources (`--coverage` excludes the generated
  `sep_filter_ctrl_register.h`). Everything still uncovered is a defensive
  `idx >= num_instances_` guard or the constructor's `num_instances` fatal — all
  unreachable through the model's normal API

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- `SepFilterCtrlTestbench` — top-level `sc_module` owning both DUT instances, their harnesses, data initiators, and filtered stubs
- `sep_filter_ctrl_test`/`sep_filter_ctrl_basetest` — CSR access helpers, shared unmodified between the outbound and inbound harness instances
- `DataInitiator` — `write`/`read` (via `b_transport`, returns success bool) and `dbg_read` (via `transport_dbg`, returns forwarded byte count)
- `FilteredStub` — target-side recorder (`last_addr`, `last_data`, `last_cmd`, `received`), with an explicit `reset()` called between data-path checks so each check's `received` flag reflects only that check's transaction
- `assert()`-based checks with `[PASS]`/section-banner console output; `sc_main` wraps `run_all_tests()` in a `try`/`catch` and returns a nonzero exit code on any thrown exception (in addition to `assert()` aborting on failure) — two different failure-reporting mechanisms coexist here, unlike the plain-`assert()`-only pattern in `sep_cpu_ctrl_test.cpp`

### 4.2 Reference Models

None — expected values and permit/deny outcomes are transcribed from the RTL-derived behavior documented in the `sep_filter_ctrl.h` header comment (`BlockByDefault`, page/word-granularity range masking, WOSET lock, hw=w `data_bus_width`).

## 5. Notes

### 5.1 Important Notes

- **`BlockByDefault=1` is the key behavior distinguishing this IP from a "passthrough when unconfigured" filter** — every test in this plan that touches the data path (A7–A10, B2) explicitly confirms denial-by-default rather than assuming it; A5's method name (`test_passthrough_when_unconfigured`) is a naming artifact from an earlier design and is called out with a code comment (and in row 5 above) so it isn't misread as testing passthrough.
- Address-range matching is masked to page (`allow_burst`, 4 KB) or word (8-byte) granularity, not exact byte bounds — none of the current tests probe a boundary address that would expose this widening (e.g. one byte below/above a range edge that is not already page/word aligned).
- No test in this suite pulses `rst_ni` — see §1.3.

### 5.2 Important Exclusions

- **Reset-clears-programmed-state**: unlike `sep_cpu_ctrl`/`local_master_alias_remap_ctrl`, there is no test that programs entries and then asserts a live `rst_ni` pulse to confirm they clear.
- **Entry 1 is locked by A3 and cannot be programmed afterwards.** A3 sets its WOSET
  `locked` bit, so A6's writes to entry 1 (`[0x3000,0x4000)`, read-only) are silently
  discarded and the entry stays disabled for the rest of the run. A6's entry 2 is
  programmed but not driven through the data path. Both arms of command gating are now
  covered regardless — A11 for a write denied by a read-only entry, B2 for a read denied
  by a write-only one — but A11 deliberately uses entry 3 for this reason.
- **Priority/overlap between multiple enabled entries** (analogous to `local_master_alias_remap_ctrl`'s first-match test): not tested here — A6/A8/A9/A10 only ever have one entry (entry 0) active on the data path at a time.
- **`allow_burst`/`allow_ns` functional effect**: both fields are written and read back via CSR (A2), but no data-path test exercises a burst-length (>8B) transaction or a non-secure-attribute transaction to confirm these fields actually gate behavior.
- **`transport_dbg` write path**: A10 only exercises `dbg_read`; a debug-path *write* is not tested.
- **Cycle-accurate timing**: the model has no clock; all transactions complete with `SC_ZERO_TIME` delay.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_sep_filter_ctrl_HighLevel_Design.md` — filter algorithm, `BlockByDefault`, register layout
- **Source of truth**: `sep_filter_ctrl_testbench.cpp` (suite structure, A7–A10/B2–B3) and `sep_filter_ctrl_test.cpp` (A1–A6, B1's shared `test_*` methods)

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 13-test (A1–A10, B1–B3) `sep_filter_ctrl_testbench.cpp` / `sep_filter_ctrl_test.cpp` suite |
