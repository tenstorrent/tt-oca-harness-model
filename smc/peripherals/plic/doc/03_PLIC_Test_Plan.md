# SMC PLIC — Detailed Test Plan

**Document**: `03_PLIC_Test_Plan.md`
**Module under test (MUT)**: `smc::plic` (`peripherals/plic/`)
**Reference test bench**: `peripherals/plic/test/plic_tb.cpp`
**Status**: Active — all 20 unit tests pass on SystemC 3.0.2 / Apple Clang 17
**Companion docs**:
  - `01_PLIC_Specification.md` — externally-observable behaviour
  - `02_PLIC_LowLevel_Design.md` — implementation contract

---

## Contents

1. [Objectives](#1-objectives)
2. [Scope (in / out)](#2-scope-in--out)
3. [Verification strategy](#3-verification-strategy)
4. [Environment](#4-environment)
5. [Test bench architecture](#5-test-bench-architecture)
6. [Stimulus & checking primitives](#6-stimulus--checking-primitives)
7. [Feature → test traceability matrix](#7-feature--test-traceability-matrix)
8. [Detailed test cases (TC-1 … TC-14)](#8-detailed-test-cases-tc-1--tc-14)
9. [Coverage plan](#9-coverage-plan)
10. [Negative-test catalogue](#10-negative-test-catalogue)
11. [Regression workflow](#11-regression-workflow)
12. [Pass / fail criteria & exit conditions](#12-pass--fail-criteria--exit-conditions)
13. [Future tests & open work](#13-future-tests--open-work)
14. [Test results — current baseline](#14-test-results--current-baseline)

---

## 1. Objectives

The PLIC test plan must demonstrate that the SystemC model:

1. **Conforms** to the RISC-V PLIC v1.0 specification and the OCAH
   `plic.rdl` register layout.
2. **Implements correctly** every flow described in
   `01_PLIC_Specification.md` §7 (source latching, arbitration, claim/
   complete, threshold gating, cross-context isolation, reset).
3. **Honours** the TLM-2.0 contract documented in
   `01_PLIC_Specification.md` §9 (alignment, width, error responses,
   debug back door).
4. **Is robust** against negative stimulus (misaligned, oversize,
   out-of-window, spurious EOI, bad commands).
5. Is **integration-ready** — the unmodified firmware driver
   (`fw/smc/common/drivers/riscv_plic0.c`) runs against it through the
   full fabric path.

---

## 2. Scope (in / out)

### 2.1 In scope

- Self-checking SystemC unit tests for every register and every
  observable behavioural rule.
- Test-bench-driven stimulus through `b_transport` (the production
  access path).
- Back-door inspection through `dbg_*` and `transport_dbg`.
- Negative path testing (misaligned, oversize, out-of-window, bad
  commands).
- Reset behaviour, including from a non-trivial state.

### 2.2 Out of scope (covered by other plans)

| Aspect                                        | Owner / document                          |
|-----------------------------------------------|-------------------------------------------|
| Cycle-accurate timing of MEIP/SEIP delivery   | RTL verification (Chipyard)               |
| Access-control / `axi_filter` enforcement     | `axi_filter` test plan                    |
| Multi-master arbitration on the SMC fabric    | `smc_fabric` test plan                    |
| `riscv_plic0.c` driver-level scenarios        | SMC firmware test plan                    |
| Whisper ↔ Rocket commit-log diff              | Co-simulation test plan (future)          |
| Power / DFT / MBIST                           | RTL DV plan                               |

---

## 3. Verification strategy

### 3.1 Tier model

| Tier              | Goal                                       | Where                                |
|-------------------|--------------------------------------------|--------------------------------------|
| **Unit (this plan)** | Per-register R/W, per-flow correctness     | `plic_tb.cpp`                        |
| Integration       | Fabric → filter → PLIC → CPU cluster       | `smc_top` integration test (future)  |
| System            | Firmware running real PLIC driver under Whisper | `smc_top` boot test (future)    |
| Co-simulation     | Whisper vs Rocket commit-log diff with PLIC events | future                        |

### 3.2 Methods

| Method                       | Purpose                                                                        |
|------------------------------|--------------------------------------------------------------------------------|
| Directed tests               | Trigger a specific spec requirement and check the observable response.         |
| Back-door assertions (`dbg_*`) | Inspect internal state with no perturbation.                                  |
| Stress against in-window holes | Confirm reserved bits decode to RAZ/WI rather than corrupting state.          |
| Negative stimulus            | Confirm the bus-error contract holds for misaligned / oversize / OOR.          |
| Reset replay                 | Drive non-trivial state, then assert reset, then re-check from scratch.        |
| Two-phase settling           | After every state mutation, run two delta cycles to let `recompute_event_` propagate before checking outputs. |

### 3.3 Determinism guarantees

The test bench is fully deterministic:

- No random stimulus.
- No wall-clock dependence; settles via `wait(SC_ZERO_TIME)` instead of
  real time.
- Single-threaded SystemC scheduler, single TLM initiator.

A failing test is a hard error — no retries, no flaky regression flags.

---

## 4. Environment

| Component             | Required version                                    |
|-----------------------|-----------------------------------------------------|
| Accellera SystemC     | ≥ 2.3.4 (3.0.2 verified)                            |
| OSCI CCI              | cci-1.0.0 (Accellera reference implementation)      |
| C++ compiler          | C++17 — Apple Clang 17 / Clang 10+ / GCC 9+         |
| CMake                 | ≥ 3.16                                              |
| OS (host)             | macOS / Linux                                       |
| Optional              | `ctest` for CI integration (already wired)          |

Reproducible commands:

```bash
SYSTEMC_HOME=/opt/homebrew/opt/systemc \
  cmake -S peripherals/plic -B peripherals/plic/build -DCMAKE_BUILD_TYPE=Release
cmake --build peripherals/plic/build -j
ctest --test-dir peripherals/plic/build --output-on-failure
```

Or, equivalently:

```bash
peripherals/plic/run_tests.sh           # configure (if needed) + build + run
peripherals/plic/run_tests.sh --clean   # wipe build/ first
peripherals/plic/run_tests.sh --ctest   # run via ctest with PASS/FAIL regex
```

The CMake CTest entry asserts on the regex:

```
PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
FAIL_REGULAR_EXPRESSION "FAIL"
```

so any test bench run that does not print `ALL TESTS PASSED` is a hard
CI failure.

---

## 5. Test bench architecture

```
                      ┌──────────────────────┐
                      │       sc_main        │
                      └──────────┬───────────┘
                                 ▼
                          ┌────────────┐
                          │   tb       │  SC_THREAD: run()
                          │ (sc_module)│
                          └─┬────────┬─┘
                            │        │
              src_sig[N] ───┘        └─── ctx_sig[C]
                  │                          ▲
                  │ src_in[i]   ctx_out[c]  │
                  ▼                          │
              ┌─────────────────────────────────┐
              │            DUT: smc::plic       │
              │  reg_socket  ◄──── b_transport  │
              │  rst_n_i     ◄──── tb.rst_n     │
              └─────────────────────────────────┘
                            ▲
                            │ TLM b_transport / transport_dbg
                            │
                      ┌─────┴──────┐
                      │  driver    │  (sc_module with simple_initiator_socket)
                      │  read32    │
                      │  write32   │
                      │  raw_xfer  │
                      └────────────┘
```

`tb` instantiates:

- `dut` — the PLIC under test, with default config (336 sources, 8 contexts).
- `drv` — a TLM initiator that exposes `read32 / write32 / raw_xfer`.
- `rst_n` and `sc_vector<sc_signal<bool>>` arrays for sources and outputs.

The `run()` thread executes the test sequence; on completion it calls
`sc_stop()` and returns the failure count to the OS.

---

## 6. Stimulus & checking primitives

| Primitive           | Purpose                                                                        |
|---------------------|--------------------------------------------------------------------------------|
| `EXPECT_EQ(a, b)`   | Hard equality check; logs `FAIL` on mismatch and increments `g_failures`.      |
| `EXPECT_TRUE(c)`    | Boolean-truth check; same failure semantics as `EXPECT_EQ`.                    |
| `pulse_reset()`     | Drives `rst_n` low for 20 ns, then high for 20 ns, then yields enough delta cycles for state to settle. |
| `raise(src, level)` | Drives `src_sig[src - 1]` to the requested level and settles.                  |
| `settle()`          | Two `wait(SC_ZERO_TIME)` calls — enough for `src_method`/`b_transport` → `recompute_event_` → `output_method` to propagate to `ctx_out`. |
| `drv.read32(addr)`  | TLM read; auto-fails on non-`TLM_OK_RESPONSE`.                                 |
| `drv.write32(addr, value)` | TLM write; auto-fails on non-`TLM_OK_RESPONSE`.                         |
| `drv.raw_xfer(cmd, addr, len, data)` | Returns the raw `tlm_response_status` for negative-path tests. |

Address arithmetic helpers:

```cpp
prio_addr(src)            = PRIORITY_BASE  + 4 * src
pending_addr(word)        = PENDING_BASE   + 4 * word
enable_addr(ctx, word)    = ENABLE_BASE    + ENABLE_STRIDE   * ctx + 4 * word
thr_addr(ctx)             = CONTEXT_BASE   + CONTEXT_STRIDE  * ctx + CONTEXT_THR_OFF
cc_addr(ctx)              = CONTEXT_BASE   + CONTEXT_STRIDE  * ctx + CONTEXT_CC_OFF
```

---

## 7. Feature → test traceability matrix

| #   | Specification requirement (`01_..._Specification.md`)                          | Verified by    |
|-----|-------------------------------------------------------------------------------|----------------|
| F1  | Reset clears priority / enable / threshold / pending / claim_in_flight        | TC-1, TC-14    |
| F2  | `PRIORITY[*]` is 3-bit (writes truncated)                                     | TC-2           |
| F3  | Source 0 is reserved (RAZ/WI on PRIORITY)                                     | TC-3           |
| F4  | Pending bit set on rising edge of source line                                 | TC-4           |
| F5  | `PENDING` register reflects per-source pending state, packed 32 per word      | TC-4           |
| F6  | Disabled (enable=0) source does not drive `ctx_out`                           | TC-5           |
| F7  | Per-context enable independence                                               | TC-6, TC-13    |
| F8  | Threshold gates priority strictly (`priority > threshold`)                    | TC-7           |
| F9  | Best-pending arbitration — highest priority wins                              | TC-8           |
| F10 | Tie-break — lowest source ID wins                                             | TC-8           |
| F11 | Claim returns top-pending, clears pending, sets `claim_in_flight`             | TC-9           |
| F12 | Complete with line still high re-arms pending immediately                     | TC-9           |
| F13 | Complete after line de-asserted does **not** re-arm pending                   | TC-10          |
| F14 | Misaligned access → `TLM_BURST_ERROR_RESPONSE`                                | TC-11          |
| F15 | Oversize access → `TLM_BURST_ERROR_RESPONSE`                                  | TC-11          |
| F16 | Out-of-window access → `TLM_ADDRESS_ERROR_RESPONSE`                           | TC-11          |
| F17 | `transport_dbg` claim-read does **not** claim                                 | TC-12          |
| F18 | Per-context threshold independence (raising one ctx threshold does not affect others) | TC-13  |
| F19 | Reset returns to clean state from non-trivial state                           | TC-14          |
| F20 | Source 0 is **never** returned as a claim result                              | Implicit in TC-8/TC-9 (best_pending iterates from 1) |
| F21 | DMI hint always cleared                                                       | Constructor/`b_transport` invariant — see `02_..._LowLevel_Design.md` §4.2 |
| F22 | `complete()` of out-of-range source ID is silently dropped                    | Implicit in spec; covered by `complete()` precondition |
| F23 | `dbg_priority` / `dbg_enable` / `dbg_threshold` / `dump_state` return correct values and safe defaults for OOB inputs | TC-15 |
| F24 | Write to `PENDING` region is silently discarded (SW=r)                        | TC-16          |
| F25 | Context-block reserved sub-offset reads as 0 (RAZ) and writes are ignored (WI) | TC-17        |
| F26 | Unknown TLM command → `TLM_COMMAND_ERROR_RESPONSE`                            | TC-18          |
| F27 | `transport_dbg` general read (non-CC) and write paths function correctly      | TC-19          |
| F28 | Enable last-word bits beyond `num_sources` are forced to 0 on write           | TC-20          |

Spec features F1–F22 were exercised before the TC-15..TC-20 additions;
F23–F28 are the newly covered requirements.

---

## 8. Detailed test cases (TC-1 … TC-20)

Each test case lists: *Spec ref → Setup → Stimulus → Expected → Notes*.

### TC-1. Reset clears all state

- **Spec**: F1
- **Setup**: Default `plic_cfg` (336 sources, 8 contexts).
- **Stimulus**: Drive `rst_n` low for 20 ns, then high; allow settle.
- **Expected**:
  - `read32(prio_addr(s))` returns `0` for every `s ∈ [1..N]`.
  - `read32(thr_addr(c))` returns `0` for every `c ∈ [0..C-1]`.
  - `ctx_sig[c]` reads `false` for every `c`.
- **Notes**: Reads are issued through the bus to confirm the register
  bank, not just internal state, is cleared.

### TC-2. Priority R/W with 3-bit truncation

- **Spec**: F2
- **Setup**: After reset.
- **Stimulus**:
  - `write32(prio_addr(7), 0x12345678)` — verify `[2:0]` survives,
    upper bits drop.
  - `write32(prio_addr(7), 5)` — verify exact value reads back.
- **Expected**: First read returns `0x12345678 & 0x7 == 0x0`; second
  read returns `5`.

### TC-3. Source 0 reserved

- **Spec**: F3
- **Setup**: After reset.
- **Stimulus**: `write32(prio_addr(0), 7)`, then `read32(prio_addr(0))`.
- **Expected**: Read returns `0` (RAZ; write was silently ignored).

### TC-4. Pending latched on rising edge

- **Spec**: F4, F5
- **Setup**: Source 7 not yet pending.
- **Stimulus**: `raise(7, true)` — drives `src_in[6]` high.
- **Expected**:
  - `dbg_pending(7)` is true.
  - Reading the pending word `pending_addr(7/32) == 0x1000`,
    bit `7 & 31 == 7` is set.

### TC-5. Disabled source does not drive `ctx_out`

- **Spec**: F6
- **Setup**: Continuation of TC-4 — pending without enable.
- **Stimulus**: settle.
- **Expected**: `ctx_sig[c]` is false for every `c`.

### TC-6. Per-context enable independence

- **Spec**: F7
- **Setup**: Source 7 pending.
- **Stimulus**: `write32(enable_addr(0, 0), 1u << 7)` — enable in ctx 0
  only.
- **Expected**: `ctx_sig[0]` rises; `ctx_sig[1..7]` stay low.

### TC-7. Threshold gating

- **Spec**: F8
- **Setup**: Source 7 priority = 5, enabled in ctx 0.
- **Stimulus**:
  - `write32(thr_addr(0), 6)` → `5 ≤ 6` → masked.
  - `write32(thr_addr(0), 4)` → `5 > 4` → visible.
- **Expected**: `ctx_sig[0]` falls then rises accordingly.

### TC-8. Best-pending arbitration (priority + tie-break)

- **Spec**: F9, F10
- **Setup**: Sources 7 / 11 / 13 enabled in ctx 0; thresholds = 4.
- **Stimulus**:
  - `priority[7] = 5`, `priority[11] = 6`, `priority[13] = 5`.
  - Raise sources 11 and 13.
  - Read `dbg_claim_top(0)` → expect 11 (higher priority).
  - Set `priority[11] = 0` → expect `dbg_claim_top(0)` returns 7
    (lowest ID at priority 5 — `7 < 13`).

### TC-9. Claim/complete with line still high

- **Spec**: F11, F12
- **Setup**: Source 7 priority 5, enabled in ctx 0, line high; source 13
  silenced via priority 0.
- **Stimulus**:
  - `read32(cc_addr(0))` → claim → returns 7.
  - `write32(cc_addr(0), 7)` → complete with line still high.
- **Expected**:
  - Claim returns 7, `dbg_pending(7)` becomes false, `ctx_sig[0]` falls.
  - After complete, `dbg_pending(7)` becomes true again, `ctx_sig[0]`
    rises.

### TC-10. Complete after line de-asserted — no re-pend

- **Spec**: F13
- **Setup**: Continuation of TC-9.
- **Stimulus**:
  - `read32(cc_addr(0))` → claim again → returns 7.
  - `raise(7, false)` → drop the line.
  - `write32(cc_addr(0), 7)` → complete.
- **Expected**: `dbg_pending(7)` stays false; `ctx_sig[0]` stays low.

### TC-11. TLM error responses

- **Spec**: F14, F15, F16
- **Stimulus** (via `raw_xfer`):
  - Read at address `0x2` (misaligned, length 4) → `TLM_BURST_ERROR_RESPONSE`.
  - Read at address `0x0` (length 8) → `TLM_BURST_ERROR_RESPONSE`.
  - Read at address `WINDOW_SIZE + 4` → `TLM_ADDRESS_ERROR_RESPONSE`.

### TC-12. `transport_dbg` has no side effects

- **Spec**: F17
- **Setup**: Source 7 pending, enabled, threshold 0 — claim would return 7.
- **Stimulus**: Issue a `transport_dbg` read of `cc_addr(0)`.
- **Expected**:
  - The debug call returns 4 bytes with value 7.
  - `dbg_pending(7)` remains true (no claim happened).

### TC-13. Cross-context isolation

- **Spec**: F7, F18
- **Setup**: Source 7 enabled and above threshold in ctx 0.
- **Stimulus**:
  - `write32(enable_addr(4, 0), 1u << 7)` and `write32(thr_addr(4), 0)`
    → ctx 4 also fires.
  - `write32(thr_addr(4), 7)` → only ctx 4 mutes.
- **Expected**: `ctx_sig[4]` follows the threshold change; `ctx_sig[0]`
  stays high throughout.

### TC-14. Reset returns to clean state from non-trivial state

- **Spec**: F1, F19
- **Setup**: After all previous tests have left the model in a
  multi-source, multi-context configured state.
- **Stimulus**: `pulse_reset()`.
- **Expected**:
  - All `ctx_sig[c]` low.
  - `read32(prio_addr(7)) == 0`, `read32(enable_addr(0, 0)) == 0`.

### TC-15. Debug back-door API

- **Spec**: F23
- **Setup**: Post TC-14 reset (clean state). Write `priority[3]=5`,
  `enable[0][word0]=1<<3`, `threshold[0]=2` via `b_transport`.
- **Stimulus**: Call all four back-door helpers with valid and
  out-of-range / reserved inputs.
- **Expected**:
  - `dbg_priority(3)` returns 5; `dbg_priority(0)` returns 0
    (src 0 reserved); `dbg_priority(num_sources+1)` returns 0 (OOB).
  - `dbg_enable(0, 3)` returns true; `dbg_enable(num_contexts, 3)`
    returns false (ctx OOB); `dbg_enable(0, 0)` returns false (src 0);
    `dbg_enable(0, num_sources+1)` returns false (src OOB).
  - `dbg_threshold(0)` returns 2; `dbg_threshold(num_contexts)`
    returns 0 (ctx OOB).
  - `dump_state(oss)` produces non-empty output without crashing.

### TC-16. PENDING write silently discarded

- **Spec**: F24
- **Setup**: After TC-14 reset (no pending bits set).
- **Stimulus**: `write32(pending_addr(0), 0xFFFFFFFF)`.
- **Expected**: `read32(pending_addr(0))` returns 0 — write had no effect.

### TC-17. Context-block reserved sub-offset (RAZ/WI)

- **Spec**: F25
- **Setup**: Any state.
- **Stimulus**:
  - `read32(CONTEXT_BASE + 0x8)` — sub-offset 8, neither THR nor CC.
  - `write32(CONTEXT_BASE + 0x8, 0xDEAD)`.
  - `read32(CONTEXT_BASE + 0x8)` again.
- **Expected**: Both reads return 0 (RAZ); write completes with
  `TLM_OK_RESPONSE` (WI, no error).

### TC-18. `b_transport` — TLM_COMMAND_ERROR_RESPONSE

- **Spec**: F26
- **Stimulus**: `raw_xfer(TLM_IGNORE_COMMAND, 0x0, 4, &scratch)`.
- **Expected**: Response status is `TLM_COMMAND_ERROR_RESPONSE`.
- **Notes**: Previous TC-11 only checked BURST and ADDRESS errors.

### TC-19. `transport_dbg` general read and write paths

- **Spec**: F27
- **Setup**: `priority[3] = 5` (set in TC-15).
- **Stimulus**:
  - `transport_dbg` READ of `prio_addr(3)` (non-CC address).
  - `transport_dbg` WRITE of `prio_addr(5)` with data 6.
- **Expected**:
  - Read returns 4 bytes with value 5 (priority[3]).
  - Write returns 4; `dbg_priority(5)` confirms the new value is 6.
- **Notes**: TC-12 only exercised the CC-peek path.

### TC-20. Enable last-word bit masking

- **Spec**: F28
- **Setup**: Default 336-source config: W=11 words (0..10); last word is
  word 10, containing sources 320–336 (17 valid bits [0..16]).
- **Stimulus**: `write32(enable_addr(0, 10), 0xFFFFFFFF)`.
- **Expected**: `read32(enable_addr(0, 10))` returns `0x0001FFFF`
  (bits 17–31 forced to 0 by the write mask; `valid_bits = 337 − 10×32 = 17`).

---

## 9. Coverage plan

### 9.1 Functional coverage (instrumentation roadmap)

The current test bench is directed; coverage instrumentation is planned
but not yet plumbed into the project's coverage harness. Hooks expected
in follow-up commits:

| Bin / cross                                         | Implementation hook                                 |
|-----------------------------------------------------|-----------------------------------------------------|
| Per-register R / W coverage                         | Increment counters at `reg_read` / `reg_write` exit |
| Per-source rising edges                             | Counter in `src_method`                             |
| Per-source `claim_in_flight` set/clear              | Counters in `claim()` / `complete()`                |
| Per-context `ctx_out` rising / falling transitions  | Counters in `output_method`                         |
| Cross: `(priority × threshold) ∈ {0..7}²`           | Sample in `output_method` after each recompute      |
| Cross: `(claim_in_flight × line_high_at_complete)`  | Sample in `complete()`                              |
| Reserved-region accesses                            | Counter in `reg_read` / `reg_write` reserved branch |

### 9.2 Code coverage targets and current results

Both `--asan` and `--coverage` modes are wired into `run_tests.sh` and
use separate build directories (`build_asan/`, `build_cov/`). Coverage
uses LLVM source-based instrumentation (Clang) or `--coverage` / gcov
(GCC), selected automatically by CMake based on `CMAKE_CXX_COMPILER_ID`.

| Metric              | Target  | Current (TC-1..TC-20) |
|---------------------|---------|------------------------|
| Function coverage   | 100 %   | **100 %** (20/20)      |
| Line coverage       | ≥ 95 %  | **97.7 %** (293/300)   |
| Region coverage     | ≥ 80 %  | 90.4 %                 |

The 7 lines that remain uncovered are structurally unreachable from the
test bench:
- 4 `SC_REPORT_FATAL` constructor guard lines (require a fatal abort to
  trigger; see TC-plan future item N13/N14).
- 3 `return false` / `return 0` defensive paths that are pre-empted by
  the `b_transport` window check before `reg_read` / `reg_write` can
  see an out-of-window address.

Untestable lines are candidates for `// LCOV_EXCL_LINE` annotations.

### 9.3 Coverage closure plan

A test is considered insufficient if any spec requirement in §7 is
covered by zero tests, or any covergroup bin in §9.1 has count 0 after
the regression. Closure is reached when:

- All spec rows (F1..F22) have at least one passing test.
- All covergroup bins are hit at least once.
- Negative-test catalogue (§10) is fully exercised.
- Code coverage targets in §9.2 are met.

---

## 10. Negative-test catalogue

| #   | Negative scenario                          | Expected response                          | Covered by              |
|-----|--------------------------------------------|--------------------------------------------|-------------------------|
| N1  | Misaligned read (e.g. addr = 0x2)          | `TLM_BURST_ERROR_RESPONSE`                 | TC-11                   |
| N2  | Misaligned write                           | `TLM_BURST_ERROR_RESPONSE`                 | TC-11 (symmetric path)  |
| N3  | Oversize access (length ≠ 4)               | `TLM_BURST_ERROR_RESPONSE`                 | TC-11                   |
| N4  | Undersize access (length < 4)              | `TLM_BURST_ERROR_RESPONSE`                 | TC-11 (path)            |
| N5  | Out-of-window access (≥ WINDOW_SIZE)       | `TLM_ADDRESS_ERROR_RESPONSE`               | TC-11                   |
| N6  | `TLM_IGNORE_COMMAND`                       | `TLM_COMMAND_ERROR_RESPONSE`               | **TC-18** ✓             |
| N7  | `priority[0]` write                        | RAZ/WI                                     | TC-3                    |
| N8  | `enable[c][0]` write of bit 0              | Bit 0 forced to 0                          | Implicit in TC-6        |
| N9  | Write to reserved bits (above 3 bits) of `priority[s]` | Truncated to 3 bits             | TC-2                    |
| N10 | Write to `pending[*]`                      | Silently discarded (RO)                    | **TC-16** ✓             |
| N11 | `complete(ctx, src=0)`                     | Silently dropped                           | Implicit in `complete()` precondition |
| N12 | `complete(ctx, src > num_sources)`         | Silently dropped                           | Implicit in `complete()` precondition |
| N13 | Construction with `num_sources = 0` or > 1023 | `SC_REPORT_FATAL` at elaboration         | Future (planned)        |
| N14 | Construction with `num_contexts = 0`       | `SC_REPORT_FATAL` at elaboration           | Future (planned)        |
| N15 | DMI request                                | `dmi_allowed = false` (no callback registered) | Implicit (callback never registered) |

Items marked `Future (planned)` are listed in §13.

---

## 11. Regression workflow

### 11.1 Local regression

```bash
peripherals/plic/run_tests.sh
```

The script:

1. Auto-locates SystemC (`SYSTEMC_HOME` or Homebrew default).
2. Configures CMake (only on first run; subsequent runs are incremental).
3. Builds with `-j<ncpus>`.
4. Runs the test bench binary `peripherals/plic/build/test/plic_tb`.

### 11.2 CI regression

```bash
peripherals/plic/run_tests.sh --ctest
```

`ctest` parses the test bench output and asserts on:

- `PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"` — must match.
- `FAIL_REGULAR_EXPRESSION "FAIL"` — must not match.

The command exits non-zero on either condition.

### 11.3 Triage protocol

On failure:

1. Read the first `FAIL` line from stderr — it carries
   `__FILE__:__LINE__` plus the expected/actual values.
2. Optionally re-run with `--clean` to rule out a stale build.
3. Add `dut.dump_state(std::cerr)` adjacent to the failing assertion to
   capture full PLIC state.
4. If the failure reproduces under a single test case but not in
   isolation, suspect cross-test state leakage — most likely an unreset
   `pending_` from a prior block.

---

## 12. Pass / fail criteria & exit conditions

| Outcome                | Criterion                                                                     |
|------------------------|-------------------------------------------------------------------------------|
| **PASS**               | The test bench prints `ALL TESTS PASSED` and exits with status `0`.           |
| **FAIL**               | Any `EXPECT_*` macro reports `FAIL`, or the bench prints any `FAIL` line, or it exits with non-zero status. |
| **BUILD FAIL**         | CMake configuration or compilation error — counts as FAIL for CI.             |
| **TIMEOUT** (CI only)  | Bench has not completed within 60 s — counts as FAIL (current actual: < 1 s). |

A green CI run requires every test case TC-1..TC-20 to pass on every
supported `(SystemC version × compiler)` matrix entry.

---

## 13. Future tests & open work

Items not yet covered by `plic_tb.cpp` but planned:

| Item                                                                       | Priority | Notes                                                |
|----------------------------------------------------------------------------|----------|------------------------------------------------------|
| ~~Negative cmd: `TLM_IGNORE_COMMAND` returns `TLM_COMMAND_ERROR_RESPONSE`~~ | —       | **Done** (TC-18)                                     |
| ~~Write to `pending[*]` is silently discarded~~                            | —        | **Done** (TC-16)                                     |
| Construction-time `SC_REPORT_FATAL` for bad `num_sources` / `num_contexts` (N13/N14) | Low | CCI preset path: set a bad value via broker preset before ctor; use `sc_report_handler::set_actions(SC_FATAL, SC_THROW)` to capture |
| CCI preset override test — verify that a broker preset for `num_sources` overrides the `plic_cfg` default | Medium | Requires a CCI broker fixture; confirms the dual-mechanism described in `01_PLIC_Specification.md §4` |
| CCI mutable `access_delay_ns` — change between transactions and verify annotated delay shifts | Medium | Read back `b_transport` delay before and after a broker write |
| Source 0 explicitly never returned as claim                                | Low      | Force `pending_[0] = true` via test-only hook        |
| Per-source coverage of `(claim, line low at complete)` cross               | Medium   | Coverage plumbing                                    |
| DMI request rejection                                                      | Low      | Verify the absence of a `get_direct_mem_ptr` handler |
| Multi-PLIC instantiation                                                   | Low      | Build a TB with two `plic` modules and disjoint sources |
| Random-stimulus fuzzer driving register accesses + edges                   | Medium   | Bound-checked random sequence; compare against reference model |
| Whisper+PLIC system test                                                   | High     | Boot SMC firmware against the model through the fabric |

---

## 14. Test results — current baseline

| Test                                                   | Status |
|--------------------------------------------------------|--------|
| TC-1  reset clears state                               | PASS   |
| TC-2  priority R/W + 3-bit truncation                  | PASS   |
| TC-3  source 0 reserved                                | PASS   |
| TC-4  pending latched on rising edge                   | PASS   |
| TC-5  disabled source does not drive ctx_out           | PASS   |
| TC-6  per-context enable independence                  | PASS   |
| TC-7  threshold gating                                 | PASS   |
| TC-8  best-pending arbitration                         | PASS   |
| TC-9  claim/complete with line still high              | PASS   |
| TC-10 complete after line de-asserted                  | PASS   |
| TC-11 TLM error responses                              | PASS   |
| TC-12 transport_dbg has no side effects                | PASS   |
| TC-13 cross-context isolation                          | PASS   |
| TC-14 reset returns to clean state                     | PASS   |
| TC-15 debug back-door API                              | PASS   |
| TC-16 PENDING write silently discarded                 | PASS   |
| TC-17 context-block reserved sub-offset (RAZ/WI)       | PASS   |
| TC-18 TLM_COMMAND_ERROR_RESPONSE                       | PASS   |
| TC-19 transport_dbg general read and write             | PASS   |
| TC-20 enable last-word bit masking                     | PASS   |

**Result**: 20 / 20 PASS — `ALL TESTS PASSED`
**Environment**: SystemC 3.0.2 Accellera, Apple Clang 17, macOS 24.3.0
**Wall-clock**: < 1 second on Apple Silicon.
**Coverage** (LLVM instrumented, `run_tests.sh --coverage`):
  function 100 % · line 97.7 % · region 90.4 %

---

*End of document.*
