# Lifecycle Controller model/test audit

## Scope and evidence

Audited model files: `include/lifecycle_ctrl.h`,
`include/lifecycle_ctrl_base.h`, `include/lifecycle_ctrl_register.h`,
`src/lifecycle_ctrl.cpp`, and `src/lifecycle_ctrl_base.cpp`.

Audited all test sources/headers, `CMakeLists.txt`, `run_tests.sh`,
configuration files, and `doc/test_plan.adoc`.

No measured coverage percentage was claimed at the original audit. **Revalidated 2026-09-24:** `--coverage` is fail-closed and passed at **98.8%** (170/172 lines) with all eight CCI configs succeeding.

## Closure status (2026-09-24)

1. **Closed — coverage ignored failed binaries.** Workers now return the child exit; background `wait` failures abort merge (`run_tests.sh`).
2. **Closed — skip-as-pass.** Tests 3, 4, 8, 9, 10, 12 (and Test 5's TEST_DEV FEAT_CTRL check) log a skip without `report_test_pass`. Test 4 also skips when `security_disable` is true (Test 12 covers all-ones).
3–8 remain **open** (no modeled reset, hidden TLM status, untested callback, weak lock duplicates, getter-not-port outputs).

## Behavior inventory

- Six-register MMIO bank: RO FEAT_CTRL low/high; two low demote words and two
  upper reserved words with write-one-to-set semantics.
- Differential lifecycle-state decode and integrity error.
- FEAT_CTRL formulas for TEST_DEV, PROD, PROD_END, RMA_SIP,
  RMA_CHIPLET, invalid state, `security_disable`, secure-test-mode gating, and
  SIP/SYS disable masks.
- Demote bits, lock scope, reserved W1S fields, PROD_DBG priority, differential
  demotion-state output, SEP-debug and product-debug outputs.
- Live eFuse input bundle and feature-vector change callback.
- CCI parameters seed standalone inputs at elaboration.
- Blocking transport is inherited from `regmodel::Memory`; no explicit
  nonblocking or DMI path.

## Existing scenario matrix

| Area | Existing evidence | Class | Rationale |
|---|---|---|---|
| Register reset/RO | Tests 1-2 | Genuine/Partial | MMIO checks are exact, but only the low demote words are checked at reset. |
| CCI-selected state tests | Tests 3-4, 8-10, 12 | Genuine when config matches | Mismatched configs log skip (not pass). Test 4 also skips under `security_disable`. |
| Demote W1S/lock | Tests 5-7, 11, 17-19 | Mostly Genuine | Transport checks W1S, upper words, lock scope and PROD priority. |
| Full state truth table | Test 13 | Genuine for public input API | `set_inputs()` is the actual platform-facing input interface and register outputs are read through MMIO. |
| Integrity/error behavior | Test 14 | Genuine | Bad differential encodings and override priority are asserted. |
| Live feature disables | Test 15 | Genuine | Public input updates and exact outputs are checked. |
| Derived outputs | Tests 16/20 | Partial | Public getters are checked directly; no external signal/consumer integration exists. |
| Reset isolation | `clear_demote()` | Coverage-only for reset | Calls `reset_all_registers()` directly; there is no modeled reset input/process. |
| Coverage harness | Eight CCI combinations | Genuine (fail-closed) | Nonzero child exit aborts merge (2026-09-24). |
| TLM protocol | Fixed 32-bit helpers | Partial | Response status is not asserted; malformed payloads are absent. |

## Shortcut and integrity findings

1. **Closed (2026-09-24) — coverage no longer ignores failed binaries.**
   Workers return the child exit; a failed config aborts merge before
   `peripheral_enforce_coverage_gate`. Measured `--coverage`: 98.8%.

2. **Closed (2026-09-24) — skipped tests are not recorded as passes.**
   Tests 3, 4, 8, 9, 10, 12 log a skip via `REG_INFO` and return. Test 4
   additionally skips when `security_disable` is true.

3. **High — no modeled reset path is tested.**
   `clear_demote()` calls public base helper `reset_all_registers()` directly
   (`test/src/testbench.cpp:453-457`). The model has no `rst_ni` port or reset
   process. Tests prove helper behavior, not asynchronous/synchronous hardware
   reset, output recomputation, or callback behavior on reset.

4. **High — TLM outcomes are hidden.**
   `test/src/lifecycle_ctrl_test.cpp:6-44` does not expose response status.
   Reads update the caller only on OK; writes ignore status entirely.

5. **Medium — feature-change callback is untested.**
   The callback is public at `include/lifecycle_ctrl.h:107-117` and is invoked
   unconditionally after recomputation (`src/lifecycle_ctrl.cpp:128-134`).
   No test checks invocation count, ordering, reentrancy, unchanged-value
   suppression policy, demote writes, integrity errors, or reset.

6. **Medium — one lock check has no discriminating stimulus.**
   Tests 6 and 11 write zero after locking
   (`test/src/testbench.cpp:254-263`, `315-325`); zero would leave a W1S
   register unchanged even without the lock. Test 18 supplies a stronger
   discriminating case, so the duplicate “post-lock ignored” passes should not
   be counted as independent lock proof.

7. **Medium — outputs are getters, not ports.**
   `get_lc_sigint_err`, `get_sep_debug`, `get_prod_dbg_active`, and
   `get_demote_state` are tested directly. This validates combinational values
   but not temporal propagation into inbound filtering, eFuse freeze, or key
   manager consumers.

8. **No `#define private public` or private-friend test access was found.**
   `set_inputs()` and output getters are production integration APIs.

## Missing scenarios

- A real reset input/domain and reset during demote writes/input changes; reset
  of FEAT_CTRL, both low/high demote words, integrity output and consumers.
- Feature callback exact semantics: only-on-change versus every recompute,
  old/new ordering, input update, demote update, secure-test gating, invalid
  state, and callback removal/replacement.
- Exhaustive raw state encodings 0x0-0xF with multiple SIP/SYS masks and both
  secure_tm values; current table samples invalid states.
- Lock and W1S simultaneous writes for demote/lock/reserved bits, all-zero/all-
  one/repeated writes, both domains, and upper-word persistence across lock.
- CCI preset path for every parameter with a strict “executed” count; skips
  are now honest but still do not prove the selected-state behavior.
- Platform consumer tests for SEP filter bypass, eFuse product-debug freeze,
  and live key-manager demotion reads.
- Invalid/hole/unaligned addresses, command, null pointer, lengths, byte
  enables, streaming width, delay, debug transport, DMI and extensions.

## Proposed testcase matrix

| ID | Setup | Stimulus | Expected | Model path | Protocol feature |
|---|---|---|---|---|---|
| LC-F-01 | Table over raw states 0-15 and mask patterns | `set_inputs()` each row | Exact FEAT_CTRL/error/SEP-debug truth table | State decode | Public input + MMIO |
| LC-F-02 | Callback records timestamp/count/value | Change each input and demote field; repeat same value | Explicit callback policy and post-update value | Recompute callback | Event/temporal |
| LC-F-03 | Each demote domain unlocked/locked | Combined demote+lock+reserved writes, then repeated writes | W1S and field-scoped lock exact | Demote callbacks | MMIO boundary |
| LC-F-04 | PROD with both demotes and distinct masks | Toggle demotes in both orders | Domain-1 priority and live derived outputs exact | PROD_DBG branches | MMIO + input |
| LC-R-01 | Add/bind modeled reset | Reset after every state/lock combination | All registers/internal outputs/callbacks return to defined reset | Reset path | Reset/event |
| LC-I-01 | Bind filter/eFuse/key-manager consumer stubs | Live input/demote changes | Consumer-visible values update without direct getter polling | Integration APIs | Ports/callback |
| LC-T-01 | Raw payload helper | Valid/hole/out-of-range/unaligned access | Exact response and no mutation on reject | Regmodel memory | Command/address/response |
| LC-T-02 | Raw payload helper | Null pointer; lengths/BE/streaming variants | Exact support or rejection | Regmodel memory | Pointer/length/BE/streaming |
| LC-T-03 | Nonzero delay | Valid register access | Delay contract asserted | Regmodel memory | Delay |
| LC-T-04 | Debug/DMI initiator | `transport_dbg`, DMI request | Explicit debug and DMI policy | Target socket | Debug/DMI |
| LC-H-01 | Coverage runner with deliberately failing config | Run `--coverage` | Overall command fails and identifies config | Harness | **Closed 2026-09-24** — fail-closed merge |

## Verdict

**Functional truth-table coverage is good, and the coverage gate is now
failure-safe (2026-09-24).** Public input behavior and demote semantics have
meaningful checks; skip-as-pass inflation is removed. Still open: no real
reset/consumer propagation, hidden TLM status, and untested feature callback.
