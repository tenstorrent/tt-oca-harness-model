# local_master_alias_remap_ctrl SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `local_master_alias_remap_ctrl` SystemC TLM model — a fixed 16-region, address-range-based additive alias remapper for SEP local masters (DMA, SPAcc, PKA). The model is intentionally small (three CSRs per region, first-match-wins combinational remap, no clock, no interrupts, no CCI-configurable parameters), so the test plan below covers exactly the behavior the model implements — it does not pad coverage with test categories (interrupts, entropy, key management, FSM, application interfaces, etc.) that do not apply to this IP.

### 1.1 Test Objectives

- Verify all region registers (`REGION_START`, `REGION_END`, `REGION_ATTRS`) reset to zero
- Verify region programming and readback via the CSR (`target_socket`) path
- Verify additive address remap on a region hit, with lower 12 bits (4 KB page offset) preserved
- Verify passthrough (address unchanged) when no region matches
- Verify first-match priority when two programmed regions overlap
- Verify remap is applied identically on write transactions, `b_transport`, and `transport_dbg`
- Verify the `cacheable` attribute bit is stored/read back correctly alongside `offset`/`valid`
- Verify the matched region's `cacheable` bit overrides the outgoing cache attribute in both directions, passes through untouched on a miss, and is restored on the caller's payload
- Verify `rst_ni` clears previously-programmed, valid region state (not just power-on-reset state)

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`local_master_alias_remap_ctrl_testbench.cpp`)
- `local_alias_remap_test` — CSR-path TLM initiator helper (`register_read_64/8`, `register_write_64/8`) bound to `dut.target_socket`
- `StubTarget` — records the last address and `sep_axi_extension::cacheable` value seen on the data-path output (`dut.remapped_socket`), used as the remap oracle
- `data_access_ext()` — sends a data-path access carrying a stack-allocated `sep::sep_axi_extension` with a chosen incoming `cacheable` value, over either `b_transport` or `transport_dbg`, and returns the value the extension holds afterwards (the initiator's own view)
- No reference model / no OpenSSL — remap results are checked against hand-computed expected addresses (additive offset arithmetic is simple enough to verify by inspection)

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/local_alias_remap.h` / `local_alias_remap_base.h`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `local_alias_remap_base`) for CSR access to the 16 region tables
- `data_socket` — AXI data-path slave; incoming transactions from SEP local masters to be remapped
- `remapped_socket` — AXI data-path master; remapped transactions forwarded to the SEP system crossbar
- `rst_ni` — active-low asynchronous reset; clears all region registers when asserted

There is no clock port — the model is a combinational, quantum-free LT model (`b_transport` remaps and forwards within the same call).

## 2. Test Plan Table

Test cases are inline blocks inside `run_tests()` (there are no discrete `test_*` functions per case); the "TestCase Name" column below uses the exact `report(...)` label string from the source as the identifier.

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **Reset and Register Access Tests** |
| 1 | T1: All region registers reset to 0 | Verify all 16 regions' `start_addr`/`end_addr`/`offset`/`cacheable`/`valid` read back as 0 on a freshly-constructed DUT | REGION_START[0-15], REGION_END[0-15], REGION_ATTRS[0-15] | `target_socket` (via `dut.get_region()`) | Positive |
| 2 | T2: Region 0 program and readback | Program region 0 (`[0x10000, 0x11000) -> +0x2000000`) via three 64-bit CSR writes and verify readback matches | REGION_START[0], REGION_END[0], REGION_ATTRS[0] | `target_socket` | Positive |
| 3 | T10: Cacheable bit field handling | Program region 2 with the `cacheable` attribute bit set (bit 62) alongside `offset`/`valid`, verify all fields read back correctly | REGION_START[2], REGION_END[2], REGION_ATTRS[2] | `target_socket` | Positive |
| 4 | T11: rst_ni pulse clears previously-programmed regions | Confirm region 0 is still valid/programmed from T2, drive a real `rst_ni` pulse (assert 10ns, deassert), verify all 16 regions read back as 0 afterward | REGION_START[0-15], REGION_END[0-15], REGION_ATTRS[0-15] | `target_socket`, `rst_ni` | Positive |
| **Address Remap Tests (data path)** |
| 5 | T3: Address remap region 0 hit | Data-path read at `0x10100` (inside region 0's range) is remapped to `0x10100 + 0x2000000 = 0x2010100` on `remapped_socket` | REGION_START[0], REGION_END[0], REGION_ATTRS[0] (pre-programmed by T2) | `data_socket`, `remapped_socket` | Positive |
| 6 | T4: Passthrough - no region match | Data-path read at `0x20000` (outside all programmed regions) is forwarded unchanged | — (relies on no region covering `0x20000`) | `data_socket`, `remapped_socket` | Positive |
| 7 | T5: First-match priority (region 0 wins) | Program overlapping region 1 (`[0x10800, 0x10900) -> +0x3000000`); a read at `0x10850` (inside both region 0 and region 1) resolves to region 0's remap (`+0x2000000`), confirming lower-index priority | REGION_START[1], REGION_END[1], REGION_ATTRS[1] | `data_socket`, `remapped_socket` | Positive |
| 8 | T6: Lower 12 bits preserved | Read at `0x10ABC` (region 0, lower 12 bits = `0xABC`) remaps to `0x2010ABC` — confirms the 4 KB page offset passes through unmodified | — (region 0) | `data_socket`, `remapped_socket` | Positive |
| 9 | T7: Write transaction remap | A `TLM_WRITE_COMMAND` at `0x10200` is remapped identically to a read (`0x2010200`) — confirms remap logic is command-independent | — (region 0) | `data_socket`, `remapped_socket` | Positive |
| 10 | T8: Address restoration after forward | After `b_transport` returns, the original transaction's `get_address()` on the caller side still reads the *input* address (`0x10300`), i.e. the DUT does not mutate the caller's payload address by reference | — (region 0) | `data_socket` | Positive |
| 11 | T9: transport_dbg applies same remap | A debug-transport (`transport_dbg`) read at `0x10400` is remapped identically to `b_transport` (`0x2010400`) — confirms both TLM paths share the same remap logic | — (region 0) | `data_socket`, `remapped_socket` | Positive |
| 12 | T18: get_region rejects an out-of-range index | `dut.get_region(NUM_REGIONS)` returns an all-zero `Region` rather than reading past `REGION_ATTRS[15]` | — | `dut.get_region()` | Negative |
| **`cacheable` Override Tests (data path)** |
| 13 | T12: cacheable region drives outgoing cache attribute high | Region 0 reprogrammed with `cacheable=1`; a read at `0x10100` carrying an extension with `cacheable=false` reaches the stub with the bit **set** and the address remapped | REGION_START[0], REGION_END[0], REGION_ATTRS[0] | `data_socket`, `remapped_socket`, `sep_axi_extension` | Positive |
| 14 | T13: non-cacheable region overrides an incoming set bit | Region 1 programmed with `cacheable=0`; a read at `0x40100` carrying `cacheable=true` reaches the stub with the bit **clear** — proves an override rather than an OR | REGION_START[1], REGION_END[1], REGION_ATTRS[1] | `data_socket`, `remapped_socket`, `sep_axi_extension` | Positive |
| 15 | T14: passthrough on miss leaves cache attribute unchanged | An unmapped address (`0x90000`) is sent twice, once with `cacheable=true` and once `false`; both reach the stub with the incoming value and the unremapped address | — (relies on no region covering `0x90000`) | `data_socket`, `remapped_socket`, `sep_axi_extension` | Positive |
| 16 | T15: initiator's cache attribute restored after forward | After a hit that overrode the bit, the caller's extension reads back its original value; after a miss it is likewise untouched — the `cacheable` counterpart to T8 | — (regions 0/1) | `data_socket`, `sep_axi_extension` | Positive |
| 17 | T16: transport_dbg applies the same cacheable override | The override is checked on the debug path in both directions (set via region 0, cleared via region 1), so a GDB access cannot observe a different attribute than the functional path | — (regions 0/1) | `data_socket`, `remapped_socket`, `sep_axi_extension` | Positive |
| 18 | T17: transport_dbg passthrough on miss | A debug read at unmapped `0x90000` forwards address and attribute unchanged — the debug path's own no-hit branch, distinct from T4/T14 | — | `data_socket`, `remapped_socket`, `sep_axi_extension` | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

1. **Reset and Register Access Tests** (T1, T2, T10, T11, T18): CSR reset value, programming, readback, attribute fields, reset-clears-state, backdoor bounds guard
2. **Address Remap Tests** (T3–T9): data-path hit/miss/priority/boundary/command/API-equivalence behavior
3. **`cacheable` Override Tests** (T12–T17): override on hit in both polarities, passthrough on miss, restore, and debug-path equivalence

Tests run in source order within a single `sc_main`/testbench instance (`local_master_alias_remap_ctrl_testbench`) — later tests (T3–T10) depend on region state programmed by earlier tests (T2, T5), and T11 deliberately runs mid-sequence so it can assert that a real reset pulse clears state left behind by T2/T5/T10.

### 3.2 Test Dependencies

- T3, T6–T9 depend on region 0 being programmed by T2
- T5 depends on region 0 (T2) and additionally programs region 1
- T11 depends on T2/T5/T10 having left non-zero, valid region state to clear
- T12–T17 run *after* T11 has cleared every region, so they reprogram from scratch via `program_region()`: region 0 as `[0x10000, 0x11000) -> +0x2000000` with `cacheable=1`, region 1 as `[0x40000, 0x41000) -> +0x5000000` with `cacheable=0`. Both polarities are needed because a test using only `cacheable=1` cannot tell an override apart from an OR.

### 3.3 Pass/Fail Criteria

- **Register Access**: `dut.get_region(i)` fields match the programmed `start_addr`/`end_addr`/`offset`/`cacheable`/`valid` values
- **Remap**: `StubTarget::last_addr` (the address observed on `remapped_socket`) matches the hand-computed expected address
- **Cache attribute**: `StubTarget::last_cacheable` matches the matched region's bit on a hit and the incoming value on a miss; `data_access_ext()`'s return value shows the caller's own copy restored
- **Reset**: all 16 regions read back as all-zero fields after a `rst_ni` pulse

Results are reported through `report()`, which counts pass/fail and sets the process exit code, rather than through `assert()` — so these checks hold in the default Release build, where `NDEBUG` would otherwise compile assertions away.

### 3.4 Test Coverage Metrics

- **Register Coverage**: `REGION_START`/`REGION_END`/`REGION_ATTRS` all exercised (region 0 fully; regions 1 and 2 exercised for overlap/attribute/`cacheable` cases — regions 3–15 are only covered by the reset/all-zero checks in T1/T11, not individually programmed)
- **Field Coverage**: `start_addr`, `end_addr`, `offset`, `cacheable`, `valid` all exercised; the `Reserved0`/`Reserved1` bitfields (masked out by the register's read/write masks) are not separately tested
- **Remap Path Coverage**: hit, miss/passthrough, overlap priority, page-offset preservation, read, write, and `transport_dbg` all exercised
- **Cache Attribute Coverage**: hit with region `cacheable=1` and `=0`, incoming attribute both set and clear, miss passthrough in both polarities, restore-after-forward, and all of it repeated on the debug path
- **Reset Coverage**: power-on state (T1) and mid-run reset-after-programming (T11) both exercised
- **Code Coverage**: 18/18 tests pass, giving 100.0% line coverage (107/107) and 91.9% function coverage (34/37) across the four hand-written sources (`src/local_alias_remap.cpp`, `src/local_alias_remap_base.cpp`, `include/local_alias_remap.h`, `include/local_alias_remap_base.h`). The three uncovered functions are compiler-generated deleting destructors (`~local_alias_remap_ip`/`~local_alias_remap_base` `D0` variants), never reached because the DUT is not destroyed through a base pointer. The generated `include/local_alias_remap_register.h` is excluded from the report — its csml template instantiations otherwise dominate the totals without saying anything about this model.

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- SystemC TLM-2.0 testbench with two independent socket paths: a CSR path (`test_harness.initiator_socket -> dut.target_socket`) and a data path (`data_initiator -> dut.data_socket -> dut.remapped_socket -> stub.socket`)
- `local_alias_remap_test` — generic 64-bit/8-bit TLM register read/write helper
- `StubTarget` — minimal target recording the last address and cache attribute it received, used to observe the remap result
- Simple pass/fail counters (`m_tests_run`/`m_tests_passed`/`m_tests_failed`) and `[PASS]`/`[FAIL]` console output per test; process exit code reflects overall pass/fail

### 4.2 Reference Models

None — expected remap addresses are computed by hand from the additive-offset algorithm (`addr[55:12] + REGION_ATTRS[r].offset[55:12]`, lower 12 bits preserved) and hardcoded per test as `expected_addr`.

## 5. Notes

### 5.1 Important Notes

- The IP has **no runtime-configurable parameters** — `NUM_REGIONS` (16) and `IDX_START` (12, i.e. 4 KB granularity) are `static constexpr`, not CCI/`csml_param`-backed, so there is no configuration-sweep testing to do.
- The IP has **no interrupts, no clock, no application interfaces, and no keys/entropy** — the corresponding test categories present in larger CSML peripherals (e.g. KMAC) do not apply here and are intentionally absent from this plan.
- Only regions 0–2 are individually programmed across the suite; regions 3–15 are only ever checked in their reset (all-zero) state.

### 5.2 Important Exclusions

- **Byte/halfword data-path granularity**: all data-path transactions in the suite use 8-byte (`uint64_t`) accesses; sub-word remap behavior is not separately tested.
- **CSR-side reserved-bit/access-type checks**: unlike register-heavy peripherals, there is no dedicated test for reserved-bit masking or read-only/write-only enforcement at the CSR level — the register masks are exercised implicitly (T2/T10 readback), not directly probed with out-of-mask writes.
- **Three or more simultaneously overlapping regions**: only the two-region overlap case (T5) is tested; N-way overlap priority beyond first-vs-second match is not exercised.
- **Cycle-accurate timing**: the model has no clock and returns `SC_ZERO_TIME` delay on every transaction; no timing behavior is validated.
- **Downstream reaction to the cache attribute**: the suite checks that the override reaches `remapped_socket` with the right value, not that anything acts on it — no VP target consumes the attribute (see HLD §2.2.1). The four `AxCACHE` bits are also not tested individually, since this IP's RTL drives all four from the single region bit.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_local_master_alias_remap_ctrl_HighLevel_Design.md` — remap algorithm (first-match, additive offset, 4 KB granularity) and register layout
- **Source of truth**: `local_master_alias_remap_ctrl_testbench.cpp` — every row above corresponds 1:1 to a `report(...)` call in `run_tests()`

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 11-test `local_master_alias_remap_ctrl_testbench.cpp` suite |
| 1.1 | 2026-08-11 | — | Added T12–T18 for the `cacheable` data-path override (`axi_alias_remap.sv:122,147`), the debug-path miss branch, and the `get_region()` bounds guard; recorded code-coverage figures |
