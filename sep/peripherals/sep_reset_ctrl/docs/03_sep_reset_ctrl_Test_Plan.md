# sep_reset_ctrl SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides the test plan for the `sep_reset_ctrl` SystemC TLM model — a small software reset controller with a single 5-bit `SW_RESET_N` register (offset `0x0`) gating five independent per-peripheral active-low reset outputs (KM, OTBN, AES, HMAC, KMAC), all additionally overridden by one active-low `global_rst_ni` input. Testing is split across the CSR-only harness functions (`sep_reset_ctrl_test.cpp`) and two signal-level checks that live in the testbench itself (`sep_reset_ctrl_testbench.cpp`), because only the testbench wires the DUT's `sc_out` reset ports to observable signals. This plan documents both halves and calls out where a harness function's name promises more than it actually checks (see §5.1).

### 1.1 Test Objectives

- Verify `SW_RESET_N` resets to its documented default `0x1E` (KM held in reset; OTBN/AES/HMAC/KMAC released)
- Verify basic CSR write/readback of the 5 reset-enable bits, independently and in combination
- Verify all 5 per-peripheral reset outputs (`km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni`) correctly mirror the corresponding `SW_RESET_N` bit at the signal level, for the reset-default pattern, all-in-reset, and all-released
- Verify `global_rst_ni` deasserted forces **all** five outputs low regardless of the current `SW_RESET_N` CSR value
- Verify that when `global_rst_ni` is re-asserted (released), the outputs return to the `SW_RESET_N` **default** pattern — and, critically, that the `SW_RESET_N` CSR itself reverts to `0x1E`, rather than the outputs merely reflecting a default while the CSR still reads back whatever firmware had last programmed

### 1.2 Test Environment

- SystemC TLM-2.0 testbench (`sep_reset_ctrl_testbench` in `sep_reset_ctrl_testbench.cpp`) owning the DUT, a CSR harness, one `global_rst_sig`, and five per-peripheral `sc_signal<bool>` outputs bound to the DUT's reset ports
- `sep_reset_ctrl_test` — CSR-path TLM helper (`csr_read_64`/`csr_write_64`) bound to `dut.target_socket`; also owns the four `test_*` functions called from the testbench (`test_reset_values`, `test_register_access`, `test_reset_output_updates`, `test_global_reset_behavior`)
- No reference model — expected values are the documented per-bit reset defaults and the direct bit-for-bit mapping between `SW_RESET_N` and the five output signals

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `include/sep_reset_ctrl.h`:

**Primary Ports:**
- `target_socket` — TLM target socket (inherited from `sep_reset_ctrl_base`) for CSR access to `SW_RESET_N`
- `global_rst_ni` — `sc_in<bool>`, active-low; when deasserted (`false`), overrides all five outputs to `false` regardless of `SW_RESET_N`
- `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` — `sc_out<bool>`, active-low per-peripheral reset outputs, one per `SW_RESET_N` bit (bits 0–4 respectively)

## 2. Test Plan Table

Rows 1–5 correspond to the four harness-level `test_*` functions (`sep_reset_ctrl_test.cpp`); rows 6–10 correspond to the two testbench-level signal checks (`sep_reset_ctrl_testbench.cpp`), split by sub-case where the source itself labels distinct scenarios.

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | --------------------- | ------------------- | --------- |
| **CSR-Only Tests (sep_reset_ctrl_test.cpp)** |
| 1 | test_reset_values | Verify `SW_RESET_N` reads back `0x1E` on a freshly-constructed DUT | SW_RESET_N | `target_socket` | Positive |
| 2 | test_register_access (pattern 0x15) | Write `0x15` (km=1, otbn=0, aes=1, hmac=0, kmac=1) and verify readback | SW_RESET_N | `target_socket` | Positive |
| 3 | test_register_access (pattern 0x0A) | Write `0x0A` (km=0, otbn=1, aes=0, hmac=1, kmac=0) and verify readback | SW_RESET_N | `target_socket` | Positive |
| 4 | test_reset_output_updates | Write and read back four documented bit patterns (`0x00`, `0x1F`, `0x01`, `0x1E`) — despite the name, this checks only CSR readback, not the actual output signals (see §5.1) | SW_RESET_N | `target_socket` | Positive |
| 5 | test_global_reset_behavior | Walk each of the 5 bits individually (`1<<0` .. `1<<4`), writing and verifying readback of each in isolation — despite the name, this does not touch `global_rst_ni` (see §5.1) | SW_RESET_N | `target_socket` | Positive |
| **Signal-Level Tests (sep_reset_ctrl_testbench.cpp)** |
| 6 | test_reset_signal_behavior — reset state (0x1E) | Write `SW_RESET_N=0x1E` and verify `km_rst_ni=0`, `otbn_rst_n=1`, `aes_rst_ni=1`, `hmac_rst_ni=1`, `kmac_rst_ni=1` | SW_RESET_N | `target_socket`, `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` | Positive |
| 7 | test_reset_signal_behavior — all in reset (0x00) | Write `SW_RESET_N=0x00` and verify all 5 outputs read `0` | SW_RESET_N | `target_socket`, `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` | Positive |
| 8 | test_reset_signal_behavior — all released (0x1F) | Write `SW_RESET_N=0x1F` and verify all 5 outputs read `1` | SW_RESET_N | `target_socket`, `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` | Positive |
| 9 | test_global_reset_override — global reset asserted | With `SW_RESET_N=0x1F` (all released) still programmed, deassert `global_rst_ni`; verify all 5 outputs are forced to `0` regardless of the CSR value | SW_RESET_N (pre-set, not rewritten here) | `global_rst_ni`, `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni` | Positive |
| 10 | test_global_reset_override — global reset released | Re-assert `global_rst_ni`; verify all 5 outputs return to the `SW_RESET_N` **default** pattern (`0x1E`, not the `0x1F` that was programmed before the global reset) **and** that `SW_RESET_N` itself reads back `0x1E` — confirms the register's software-visible value reverts on global reset rather than only the outputs | SW_RESET_N (readback only) | `global_rst_ni`, `km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni`, `target_socket` | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

1. **CSR-Only Tests** (rows 1–5): register reset value, basic RW, and two additional CSR sweeps whose function names describe signal-level behavior they do not actually check
2. **Signal-Level Tests** (rows 6–10): the actual output-signal verification, plus global-reset override and CSR-revert-on-global-reset

All ten checks run in one `sc_main`/testbench instance, in the fixed order: rows 1–5 (called first from `run_all_tests()`), then rows 6–8 (`test_reset_signal_behavior`), then rows 9–10 (`test_global_reset_override`).

### 3.2 Test Dependencies

- Row 9 depends on row 8 having left `SW_RESET_N=0x1F` programmed (it is not rewritten in row 9 — the point of the test is that the override happens despite that value)
- Row 10 depends on row 9 (global reset must already be asserted before it can be released)
- Rows 6–8 are independent of each other but must run after rows 1–5 have exercised the CSR write path at least once

### 3.3 Pass/Fail Criteria

- **CSR tests**: readback matches the written value (masked to the 5 valid bits) or the documented reset default (`0x1E`)
- **Signal tests**: each `sc_signal<bool>` bound to a DUT output reads the expected value after `wait(1, SC_NS)` for signal propagation
- **Global reset override**: while `global_rst_ni` is deasserted, outputs are `0` irrespective of `SW_RESET_N`; once released, outputs match the *default* bit pattern and the CSR itself reads back that default

### 3.4 Test Coverage Metrics

- **Register Coverage**: the sole register, `SW_RESET_N`, is exercised for its reset value, individual-bit writes, multi-bit patterns, and CSR-revert-on-global-reset
- **Output Signal Coverage**: all 5 per-peripheral outputs (`km_rst_ni`, `otbn_rst_n`, `aes_rst_ni`, `hmac_rst_ni`, `kmac_rst_ni`) are exercised for both `0` and `1` states, and under global-reset override
- **Global Reset Coverage**: both assertion (override) and release (revert-to-default, including CSR-level revert) are exercised

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- `sep_reset_ctrl_testbench` — top-level `sc_module` owning the DUT, `test_harness` (CSR access), `global_rst_sig`, and five per-peripheral output signals bound directly to the DUT's `sc_out` ports
- `sep_reset_ctrl_test` — CSR read/write helper plus the four harness-level `test_*` functions, reused by the testbench
- Plain `assert()` checks with `std::cout` progress/diagnostic printing (bit patterns and signal states); no `[FAIL]` path or pass/fail counter — a failed `assert()` aborts the process, so pass/fail is the process exit code, matching the `sep_cpu_ctrl_test.cpp` pattern rather than the counter-based pattern used by `local_master_alias_remap_ctrl`/`sep_output_remap_ctrl`
- `sc_main` calls `load_config_file()` and uses `std::quick_exit(0)` unconditionally on completion (there is no failure branch that returns a nonzero code beyond the `assert()` abort itself)

### 4.2 Reference Models

None — expected values are the register's documented per-bit reset defaults (`sep_reset_ctrl.h`'s header comment) and the direct 1:1 bit-to-output mapping.

## 5. Notes

### 5.1 Important Notes

- **Two harness function names do not match what they test.** `test_reset_output_updates` (row 4) and `test_global_reset_behavior` (row 5) both only exercise the CSR path (`csr_write_64`/`csr_read_64`) — neither reads any `sc_signal`, and `test_global_reset_behavior` never touches `global_rst_ni` at all. The actual output-signal and global-reset-override behavior these names describe is what rows 6–10 (the testbench-level `test_reset_signal_behavior`/`test_global_reset_override` functions) verify instead. This is called out explicitly here (as it was for `sep_filter_ctrl`'s `test_passthrough_when_unconfigured`) so the naming isn't mistaken for the actual test boundary.
- Row 10 is the most functionally significant check in the suite: it distinguishes "outputs happen to match the default because the override is still active" from "the `SW_RESET_N` register's software-visible state was actually reset," which a naive implementation could get wrong (e.g. only gating the outputs combinationally without resetting the backing register).

### 5.2 Important Exclusions

- **`otbn_rst_n`'s inverted-polarity naming**: the port name lacks the `i` suffix the other four active-low outputs have (`km_rst_ni` vs. `otbn_rst_n`), per the header comment. No test specifically probes for a polarity mismatch on this signal beyond its value matching the same active-low convention as the others.
- **Global reset asserted with `SW_RESET_N` in other patterns**: the override test (row 9) only checks the override against the `0x1F` (all-released) pattern; it is not repeated against `0x00` or a mixed pattern to confirm the override truly ignores the CSR value rather than coincidentally matching it.
- **Combined/simultaneous CSR write and global-reset assertion**: not tested — the two mechanisms are only exercised in sequence, never overlapping in the same delta cycle.
- **Cycle-accurate timing**: the model has no clock; each check waits a fixed `1 ns` for signal propagation, which is a testbench convenience, not a timing spec being validated.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Design doc**: `02_sep_reset_ctrl_HighLevel_Design.md` — `SW_RESET_N` register layout, global-reset override semantics
- **Source of truth**: `sep_reset_ctrl_test.cpp` (rows 1–5) and `sep_reset_ctrl_testbench.cpp` (rows 6–10)

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-08-05 | — | Initial test plan, derived from the existing 10-check `sep_reset_ctrl_test.cpp` / `sep_reset_ctrl_testbench.cpp` suite |
