# sep_scratch_cold SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `sep_scratch_cold` SystemC TLM model — 8 identical 64-bit `SCRATCH[0..7]` registers (lower 32 bits `data`, RW; upper 32 bits `Reserved0`, masked to 0) that double as (a) plain firmware/CocoTB-VP handshake registers for three specific test flows, and (b) the backing store for two simulation-only virtual consoles — `VirtConsoleDecoder` (decodes `SCRATCH[2]` writes into `[SIM_OUT]` lines) and `StatusDecoder` (decodes `SCRATCH[1]` writes into `[SEP_STATUS]` lines), both of which you will recognize from `sep-vp`'s console output during a ROM/firmware boot. The existing testbench (`sep_scratch_cold_testbench.cpp`) already names each test case with a `FUNC-SCRATCH-NNN` requirement ID, so this plan uses those IDs directly rather than inventing new names.

### 1.1 Test Objectives

- Verify all 8 `SCRATCH` registers reset to 0
- Verify independent read/write access to each register's lower 32 bits
- Verify simultaneous, distinct values across all 8 registers do not interfere with each other
- Verify the upper 32 bits (`Reserved0`) of each register are hardwired to 0 regardless of what is written there
- Verify the three hardcoded VP-ack handshakes: `SCRATCH[0]=0x12345678 -> SCRATCH[1]=0x87654321`, `SCRATCH[4]=0x815 -> SCRATCH[5]=0x777`, `SCRATCH[6]=0xA1E50006 -> SCRATCH[7]=0x00100001`
- Verify writing a near-miss (non-magic) value to `SCRATCH[0]`/`[4]`/`[6]` does **not** trigger the corresponding ack on `SCRATCH[1]`/`[5]`/`[7]`
- Verify `VirtConsoleDecoder`'s opcode branches (`OP_ASCII` char-append and newline-flush, `OP_DEC24` decimal encode, and an unrecognized opcode being silently ignored) execute without disturbing `SCRATCH[2]`'s stored value
- Verify `StatusDecoder`'s type-label (`INFO`/`WARN`/`ERROR`/`INFO_EXT`/`DEBUG`/unknown) and stage-label (`BL0`/`BL1`/unknown `fw_id`) branches execute without disturbing `SCRATCH[1]`'s stored value
- Verify the CCI-backed parameters (`verbosity`, `sim_out_enable`, `sep_status_enable`) have sane, enabled-by-default values

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`sep_scratch_cold_testbench` in `sep_scratch_cold_testbench.cpp`), a single `sc_module` with one `SC_THREAD` and its own 32-bit `simple_initiator_socket` bound to `dut.target_socket`
- No separate CSR-helper class (unlike the CSML pattern used by other peripherals' `*_test.h`) — `b_read`/`b_write` are inline 4-byte `b_transport` helpers, with `scratch_read`/`scratch_write`/`scratch_read_upper`/`scratch_write_upper` convenience wrappers computing the `idx * 8` (lower half) / `idx * 8 + 4` (upper half) byte offset
- No reference model — expected values are the documented magic constants and reset/mask defaults; the decoder tests (010, 011) only assert that the register's stored value is unaffected by the decode side effect, they do not capture/compare the `[SIM_OUT]`/`[SEP_STATUS]` stdout text itself
- `dut.reset_all_registers()` is called at the start of every test function to guarantee a clean starting state, independent of what a prior test left behind

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/sep_scratch_cold.h` / `sep_scratch_cold.cpp`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `sep_scratch_cold_base`) for all `SCRATCH[0..7]` access, 8-byte stride per register

**CCI Parameters** (not TLM ports — accessed via `dut.<param>.get_param_value()`): `verbosity`, `sim_out_enable` (gates `VirtConsoleDecoder`), `sep_status_enable` (gates `StatusDecoder`)

**Simulation-only side effects** (not observable via TLM, only via stdout): the `[SIM_OUT]` line from `VirtConsoleDecoder`'s emit lambda (driven by `SCRATCH[2]` writes) and the `[SEP_STATUS]` line from `StatusDecoder`'s emit lambda (driven by `SCRATCH[1]` writes) — these are the same consoles visible in an actual `sep-vp` boot-ROM run.

## 2. Test Plan Table

Each row corresponds 1:1 to a named `test_*` function and its `FUNC-SCRATCH-NNN` requirement ID from the source; rows are listed in the order they actually run in `run_tests()` (which is not strictly numeric — `FUNC-SCRATCH-009` runs last).

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **Reset and Basic Register Tests** |
| 1 | FUNC-SCRATCH-001: Reset values | Verify all 8 `SCRATCH` registers' lower 32 bits read back as 0 after `reset_all_registers()` | SCRATCH[0-7] | `target_socket` | Positive |
| 2 | FUNC-SCRATCH-002: Basic read/write | Write a distinct pattern to each register's lower 32 bits (values chosen to avoid the VP-ack magic values) and verify readback | SCRATCH[0-7] | `target_socket` | Positive |
| 3 | FUNC-SCRATCH-003: Register independence | Write 8 distinct values across all registers in one pass, then verify each still holds its own value (no cross-register interference) | SCRATCH[0-7] | `target_socket` | Positive |
| 4 | FUNC-SCRATCH-004: Reserved bits masked out | Write `0xFFFFFFFF` to `SCRATCH[3]`'s upper 32 bits (`Reserved0`) and verify both the upper and lower halves still read 0 | SCRATCH[3] | `target_socket` | Positive |
| **VP-Ack Side-Effect Tests** |
| 5 | FUNC-SCRATCH-005: VP ack SCRATCH[0]=0x12345678 -> SCRATCH[1]=0x87654321 | Write the `global_alias_remap_sanity` magic value to `SCRATCH[0]` and verify `SCRATCH[1]` is set to the ack value | SCRATCH[0], SCRATCH[1] | `target_socket` | Positive |
| 6 | FUNC-SCRATCH-006: VP ack SCRATCH[4]=0x815 -> SCRATCH[5]=0x777 | Write the `ap_stee_output_remap_test` magic value to `SCRATCH[4]` and verify `SCRATCH[5]` is set to the ack value | SCRATCH[4], SCRATCH[5] | `target_socket` | Positive |
| 7 | FUNC-SCRATCH-007: VP ack SCRATCH[6]=0xA1E50006 -> SCRATCH[7]=0x00100001 | Write the `sep_aes_large_payload_test` (`FW_READY_MAGIC`) value to `SCRATCH[6]` and verify `SCRATCH[7]` is set to the documented `{blocks=16, seed=1}` response | SCRATCH[6], SCRATCH[7] | `target_socket` | Positive |
| **No-Spurious-Ack Test** |
| 8 | FUNC-SCRATCH-008: No spurious acks on non-magic writes | Write each of the three magic values off-by-one (`0x12345677`, `0x00000816`, `0xA1E50007`) to `SCRATCH[0]`/`[4]`/`[6]` respectively and verify the corresponding ack register (`SCRATCH[1]`/`[5]`/`[7]`) stays at 0 in all three cases | SCRATCH[0], SCRATCH[1], SCRATCH[4], SCRATCH[5], SCRATCH[6], SCRATCH[7] | `target_socket` | Negative |
| **Decoder Code-Path Coverage Tests** |
| 9 | FUNC-SCRATCH-010: Virtual console decoder paths | Drive `SCRATCH[2]` through `OP_ASCII` (char append + newline flush), `OP_DEC24` (24-bit decimal encode, buffered), a second `OP_ASCII` newline (flushing the buffered decimal text), and an unrecognized opcode (`3`, silently ignored); verify `SCRATCH[2]`'s stored value reflects only the last write | SCRATCH[2] | `target_socket` | Positive |
| 10 | FUNC-SCRATCH-011: Status decoder type and stage labels | Drive `SCRATCH[1]` through `INFO`/`WARN`/`ERROR`/`INFO_EXT`/`DEBUG` type codes, `BL0`/`BL1`/unknown `fw_id` stage codes, and an unknown type code; verify `SCRATCH[1]`'s stored value reflects only the last write | SCRATCH[1] | `target_socket` | Positive |
| **CCI Parameter Test** |
| 11 | FUNC-SCRATCH-009: CCI parameter defaults | Verify `verbosity >= 1`, `sim_out_enable == true`, and `sep_status_enable == true` by default | — (CCI parameters, not registers) | `verbosity`, `sim_out_enable`, `sep_status_enable` (CCI params) | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

1. **Reset and Basic Register Tests** (rows 1–4): power-on state, plain RW, cross-register independence, upper-half masking
2. **VP-Ack Side-Effect Tests** (rows 5–7): the three hardcoded firmware/CocoTB-VP handshake write-callbacks
3. **No-Spurious-Ack Test** (row 8): confirms the three handshakes are exact-match, not prefix/near-match triggered
4. **Decoder Code-Path Coverage Tests** (rows 9–10): exercise `VirtConsoleDecoder`/`StatusDecoder` branches for code coverage, without asserting on their stdout output
5. **CCI Parameter Test** (row 11): configuration defaults, independent of register state

Each test function calls `dut.reset_all_registers()` at its own start, so despite running sequentially in one `sc_main`, the tests are functionally independent of each other's leftover register state (unlike some of the other IPs' suites, where later tests deliberately depend on earlier ones).

### 3.2 Test Dependencies

None in the strict sense — every test resets the DUT's registers before driving its own scenario. (CCI parameters, tested last by row 11, are not touched by `reset_all_registers()` and are unaffected by any earlier test regardless.)

### 3.3 Pass/Fail Criteria

- **Register tests**: readback matches the documented reset value, the just-written value, or (for the upper half) always 0
- **VP-ack tests**: the ack register holds exactly the documented response value after the magic value is written to the trigger register
- **No-spurious-ack test**: the would-be ack register stays at 0 (its reset value) after a near-miss write to the trigger register
- **Decoder coverage tests**: no crash/exception across all exercised opcode/type/stage branches, and the register's own stored value is exactly the last word written (i.e. the decoder is a pure side-effect observer, not something that mutates the register's storage)
- **CCI parameter test**: each parameter's `get_param_value()` matches its documented default

### 3.4 Test Coverage Metrics

- **Register Coverage**: all 8 `SCRATCH` registers exercised for reset/basic-RW/independence; `SCRATCH[0]`/`[1]`/`[4]`/`[5]`/`[6]`/`[7]` additionally exercised for the VP-ack/no-spurious-ack pairs; `SCRATCH[1]`/`[2]` additionally exercised for decoder-path coverage; `SCRATCH[3]` specifically for reserved-bit masking
- **VP-Ack Coverage**: all 3 documented magic-value handshakes covered, each with both a triggering write (rows 5–7) and a non-triggering near-miss write (row 8)
- **Decoder Branch Coverage**: `VirtConsoleDecoder`'s `OP_ASCII`/`OP_DEC24`/unknown-opcode branches, and `StatusDecoder`'s `INFO`/`WARN`/`ERROR`/`INFO_EXT`/`DEBUG`/unknown-type and `BL0`/`BL1`/unknown-`fw_id` branches, are all exercised at least once
- **CCI Parameter Coverage**: all 3 parameters checked against their default values (no non-default/`.ini`-override configuration is tested)

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- Single `sc_module` (`sep_scratch_cold_testbench`) owning the DUT, one 32-bit `simple_initiator_socket`, and a `CsmlLogger` configured from the DUT's own `verbosity` parameter
- Inline `b_read`/`b_write` 4-byte `b_transport` helpers, plus `scratch_read`/`scratch_write`/`scratch_read_upper`/`scratch_write_upper` convenience wrappers over the `idx * 8` register stride
- `report_test_start`/`report_test_pass`/`report_test_fail` — `[ RUN ]`/`[ PASS ]`/`[ FAIL ]` console reporting with a running `m_tests_run`/`m_tests_passed`/`m_tests_failed` tally (unlike `assert()`-only suites, a failed check here is recorded and the test function continues/returns rather than aborting the process immediately)
- `sc_main` returns a nonzero exit code (via `std::quick_exit`) if `m_tests_failed > 0`, independent of the per-test console reporting

### 4.2 Reference Models

None — expected values are the documented magic constants (`sep_scratch_cold.cpp`'s handshake comments) and decoder-opcode encodings (documented inline in the `FUNC-SCRATCH-010`/`011` test function comments), copied into the assertions directly.

## 5. Notes

### 5.1 Important Notes

- **This IP is a simulation-only aid, not a hardware-faithful model of a real scratch register block.** The three VP-ack handshakes and the two virtual-console decoders exist purely to let firmware/CocoTB coordinate with the VP and to make ROM/firmware progress visible in the `sep-vp` console (`[SIM_OUT]`/`[SEP_STATUS]`) — none of this has a silicon equivalent beyond the plain scratch-storage behavior itself.
- Rows 9 and 10 (`FUNC-SCRATCH-010`/`011`) are **code-coverage tests, not functional-output tests** — they confirm the decoder logic runs without disturbing register state across all its branches, but do not capture or assert on the actual `[SIM_OUT]`/`[SEP_STATUS]` text produced. A regression that garbled the decoded text (but didn't crash or touch register storage) would not be caught here.
- The test run order in `run_tests()` is not strictly numeric (`FUNC-SCRATCH-010`/`011` run before `009`) — this plan lists rows in that actual run order, not requirement-ID order, to match what a test-log diff would show.

### 5.2 Important Exclusions

- **stdout content verification**: no test captures/parses the actual `[SIM_OUT]`/`[SEP_STATUS]` lines the decoders print — only the side-effect-free register readback is checked (see §5.1).
- **CCI parameter overrides**: `verbosity`/`sim_out_enable`/`sep_status_enable` are only checked against their compiled-in defaults; no test drives a `.ini` override to confirm `sim_out_enable=false`/`sep_status_enable=false` actually silences the corresponding decoder.
- **Byte/halfword access granularity**: every register access in the suite is a 4-byte (`uint32_t`) transaction at a natural offset; sub-word or misaligned access is not tested.
- **Simultaneous multi-register handshake triggering**: the three VP-ack handshakes are tested one at a time; a single transaction sequence that triggers more than one simultaneously is not tested.
- **Cycle-accurate timing**: the model has no clock; all transactions complete with `SC_ZERO_TIME` delay.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_sep_scratch_cold_HighLevel_Design.md` — register layout, VP-ack handshake list, decoder integration
- **Source of truth**: `sep_scratch_cold_testbench.cpp` — every row above corresponds 1:1 to a named `FUNC-SCRATCH-NNN` test function

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 11-test (`FUNC-SCRATCH-001`–`011`) `sep_scratch_cold_testbench.cpp` suite |
