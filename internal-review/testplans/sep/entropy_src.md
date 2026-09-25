# Entropy Source model/test audit

## Scope and evidence

Audited model files: `include/entropy_src.h`,
`include/entropy_src_interface.h`, `include/entropy_src_base.h`,
`include/entropy_src_register.h`, `src/entropy_src.cpp`, and
`src/entropy_src_base.cpp`.

Audited all test sources under `test/src`, test headers under `test/inc`,
`CMakeLists.txt`, `run_tests.sh`, and `doc/test_plan.adoc`.

No measured coverage percentage is claimed. The generic coverage target and
gate are wired at `CMakeLists.txt:149-167` and `run_tests.sh:69-72`; no report
was used as evidence in this audit.

## Behavior inventory

- Regmodel-backed MMIO register bank with access masks, FIPS lock gating,
  W1C health status, interrupt status/enable/test, configuration/status words,
  observer controls, and generator configuration.
- Active-low hardware reset drains the FIFO, resets registers/pointers/internal
  mirrors, wakes the generation thread, and deasserts IRQ.
- Background entropy generation using `RAND_bytes`, a 64-word FIFO, 6-bit
  pointers, FIFO overflow/underflow status, and quantum-keeper pacing.
- FIFO enable/disable, destructive FIFO_RDATA reads, live FIFO_STATUS, and
  combined `irq_o`.
- Boot-phase status from MODULE_ENABLE plus ring-oscillator enable.
- Health-test enable is mirrored, but health tests/counters are explicitly
  functional stubs; counter thresholds are not evaluated
  (`src/entropy_src.cpp:346-373`, `625-633`).
- Blocking transport is inherited from `regmodel::Memory`; nonblocking and DMI
  paths are explicitly absent (`src/entropy_src.cpp:60-63`).

## Existing scenario matrix

| Area | Existing evidence | Class | Rationale |
|---|---|---|---|
| Register access/masks | FUNC-001/003/005/008 | Genuine for storage; Partial for status behavior | Most cases use MMIO and exact masks/defaults; several status/counter tests only prove reset/stub storage. |
| Interrupts | FUNC-002 and COV-008 | Genuine | Inject, enable, W1C, masking, IRQ, overflow and underflow are observed. |
| FIFO behavior | FUNC-004/006 and COV-002/008 | Mostly Genuine | Occupancy, pointers, disable/re-enable, overflow, underflow and destructive reads are exercised through MMIO/ports. |
| Reset | hardware-reset cases and FUNC-007 | Genuine for `rst_ni`; Partial for legacy “software reset” labels | Current implementation uses `rst_ni`; several comments/names still describe retired CTRL reset behavior. |
| Timing/startup | FUNC-004 plus COV-001/002 | Partial | Observable waits are checked, but setup writes private `m_startup_delay_ns` directly. |
| FIPS lock/W1C status | COV-006 | Partial | Lock behavior is transport-driven; sticky status is seeded by direct register assignment. |
| Recovery/thread branches | COV-003/004/009/010 | Coverage-only | Private flags, events, FIFO, quantum keeper, and private helpers are directly manipulated. |
| TLM protocol | 32-bit and 8-bit helpers | Partial | Valid accesses are used; response errors are logged but do not fail, and malformed payloads are absent. |

## Shortcut and integrity findings

1. **Critical — an assertion is explicitly disabled.**
   `test/src/testbench.cpp:333-340` guards the FIFO_STATUS RO check with
   `if (false && read_val != fifo_status_reset)`. A broken writable
   FIFO_STATUS therefore passes `test_ro()`.

2. **High — the coverage suite is white-box and reaches impossible external
   states.** `include/entropy_src.h:93-98` grants the testbench friendship.
   `test/src/test_coverage.cpp:116-135` synthesizes reset by changing
   `m_reset_in_progress` and notifying private events, then returns `true`
   without checking an architectural result. `test_coverage.cpp:549-580`
   directly edits reset flags, events, the FIFO and quantum keeper, invokes the
   private recovery helper, and checks only two internal postconditions.

3. **High — functional timing tests bypass the programmed register path.**
   `test/src/func004_tests.cpp:763-767`, `973-1002`, and `1063` write
   `dut->m_startup_delay_ns` directly. These tests cannot catch a broken
   STARTUP_CTRL decode/callback-to-thread connection.

4. **High — W1C tests seed model registers directly.**
   `test/src/test_coverage.cpp:307-329` assigns HEALTH_TEST_STATUS and
   GENERATOR_0_HEALTH_STATUS directly before testing clear behavior. This is a
   valid callback unit seam only if labeled as such; it does not prove a
   hardware event can set those bits.

5. **High — TLM errors are non-fatal.**
   `test/src/entropy_src_test.cpp:34-59`, `70-93`, `105-129`, and `141-164`
   only log response errors. Helpers return no status, so callers can pass on a
   stale/unchanged read buffer.

6. **Medium — random data values are used as occupancy evidence.**
   For example `test/src/test_coverage.cpp:40-49` declares filling started only
   when FIFO_RDATA is nonzero. A valid random zero is indistinguishable from
   empty. Loops reduce but do not eliminate this oracle defect.

7. **Medium — health-test coverage overstates behavior.**
   The model explicitly leaves health-test execution as a no-op
   (`src/entropy_src.cpp:625-633`), while FUNC-005 mainly checks register
   defaults/masks. The suite does not validate repetition/APT/Markov failure
   detection, counters, alerts, or interrupt generation.

8. **No `#define private public` was found**, but `friend class testbench`
   provides equivalent private-state access for the cited coverage tests.

## Missing scenarios

- Health-test algorithms, thresholds, counter increments/saturation, per-test
  status, alert summary/fail counts, failure interrupts, enable/disable during a
  window, and reset mid-window.
- RAND failure injection and recovery; deterministic entropy source for
  non-flaky value checks.
- FIPS-lock boundary coverage for every lock-gated register and partial fields,
  including attempted writes before/after reset.
- FIFO simultaneous push/pop, pointer wrap at 63→0, full-to-read transition,
  repeated overflow/underflow, interrupt enable changes while status is pending,
  and reset at each thread wait point through public inputs only.
- Boot-phase truth table for module enabled/disabled and zero/nonzero generator
  masks, including FIPS lock.
- Observer control/status/RDATA functional behavior rather than reset-zero
  storage.
- Invalid/hole/unaligned addresses, unsupported command, null data pointer,
  zero/short/oversized length, byte-enable masks, streaming-width mismatch,
  response status, delay behavior, debug transport, DMI, and extensions.

## Proposed testcase matrix

| ID | Setup | Stimulus | Expected | Model path | Protocol feature |
|---|---|---|---|---|---|
| ES-F-01 | Deterministic entropy provider, FIFO enabled | Generate 65 iterations | Level caps at 64; exact overflow bit/IRQ; no extra push | Generation thread | Thread/event/IRQ |
| ES-F-02 | FIFO at levels 0,1,63,64 | Interleave timed push and FIFO_RDATA | Atomic LEVEL/WPTR/RPTR and wrap behavior | FIFO status/pop | Temporal boundary |
| ES-F-03 | Program startup delay via architectural register/input | Disable/re-enable FIFO | No data before deadline; data after; no private mutation | Startup-delay path | MMIO + time |
| ES-F-04 | Each health-test mode enabled | Feed deterministic pass/fail streams | Exact counters/status/alert/interrupt | Health-test logic | Functional negative |
| ES-F-05 | Pending status with IRQ disabled | Enable, disable, W1C, re-enable | Level IRQ follows status&enable without losing status | Interrupt method | Port/event |
| ES-F-06 | FIPS lock clear/set | Write every protected register before/after lock and reset | Only permitted bits move; reset unlocks | Lock callbacks | MMIO boundary |
| ES-R-01 | Thread in boot gate, enable wait, pacing, and full wait | Toggle `rst_ni` in each state | FIFO/register/IRQ reset and clean restart | Reset process/thread | Reset/event race |
| ES-T-01 | Raw payload helper | Valid/hole/unaligned reads and writes | Exact response and no mutation on reject | Regmodel memory | Command/address/response |
| ES-T-02 | Raw payload helper | Null pointer; lengths 0/1/2/3/5/8 | Defined responses and no overrun | Regmodel memory | Pointer/length |
| ES-T-03 | Raw payload helper | Byte enables and streaming widths | Exact supported partial access or rejection | Regmodel memory | BE/streaming |
| ES-T-04 | Debug/DMI initiator | `transport_dbg`, DMI request | Explicit side-effect/debug policy and DMI result | Regmodel memory | Debug/DMI |
| ES-T-05 | Nonzero incoming delay | Valid register access | Delay contract asserted | Regmodel memory | Delay |

## Verdict

**Material false-green risk.** The transport-driven FIFO and interrupt cases
are extensive, but one assertion is literally disabled and the coverage suite
uses friendship to force private flags/events/helpers. Health-test functionality
is largely unimplemented and consequently untested. Coverage-only cases should
not be counted as functional scenarios.
