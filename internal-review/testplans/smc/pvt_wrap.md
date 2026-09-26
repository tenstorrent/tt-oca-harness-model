# SMC `pvt_wrap` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/pvt_wrap/include/pvt_wrap.h`
- `smc/peripherals/pvt_wrap/src/pvt_wrap.cpp`
- `smc/peripherals/pvt_wrap/test/pvt_wrap_tb.cpp`
- `smc/peripherals/pvt_wrap/test/CMakeLists.txt`
- `smc/peripherals/pvt_wrap/CMakeLists.txt`
- `smc/peripherals/pvt_wrap/run_tests.sh`

No model or test was modified by this audit.

## Model behavior inventory

- Ports: 32-bit target socket; active-low reset; process-clock observation and enable outputs; 3-bit voltage-code output carried in `uint32_t`; temperature interrupt.
- Registers: `PROCESS_CTRL`, `PROCESS_STATUS`, 64-bit reference period split LO/HI, 64-bit process counter split LO/HI, `VOLTAGE_CTRL/STATUS`, `TEMP_CTRL/STATUS`, and `TEMP_INTERRUPT`.
- Register policy: masks on control/status registers; period halves are full RW; status/counter/interrupt registers accept writes as ignored RO writes; unmapped and out-of-window offsets fail decode.
- Processes/events: reset method, periodic `tick_event_`, zero-time `recompute_event_`, counter-valid generation, output recomputation, and status recomputation.
- Temporal behavior: enabling count schedules ticks; a nonzero programmed period eventually latches the reference count and valid bit; disabling count clears valid; reset cancels pending work.
- TLM: `b_transport`, `transport_dbg`, fixed cached access delay, 32-bit register data copies, and address-error response for decode failures.
- Configuration: mutable CCI `access_delay_ns` and `tick_period_ns`; constructor rejects negative initial values.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Reset defaults and reset during counting (`pvt_wrap_tb.cpp:184-199, 379-400`) | Genuine | Uses the target socket and observable registers/outputs. |
| RW masks and RO write-ignore (`pvt_wrap_tb.cpp:203-218, 339-350`) | Genuine | Frontdoor register behavior with explicit expected values. |
| Counter/status/interrupt flow (`pvt_wrap_tb.cpp:229-275`) | Partial | Frontdoor and output-visible, but timing is a loose 200 ns wait and only one small period is checked. |
| Process/voltage outputs (`pvt_wrap_tb.cpp:253-267`) | Genuine | Checks public output signals after frontdoor configuration. |
| Decode errors and malformed command/length (`pvt_wrap_tb.cpp:279-299, 354-375`) | Partial | Covers only out-of-window/unmapped and short write; read length, pointer, byte-enable, streaming, and alignment contracts are absent. |
| `transport_dbg` (`pvt_wrap_tb.cpp:302-336`) | Coverage-only | Backdoor round-trip largely duplicates storage implementation and does not establish software-visible behavior. |
| CCI introspection (`pvt_wrap_tb.cpp:405-419`) | Partial | Proves handles/defaults exist, not that runtime mutation changes delay/tick behavior. |

No `private`→`public` macro, touch-all-offset loop, test-only model branch, or copied model implementation was found.

## Findings

1. **High — a malformed read can overwrite or dereference caller memory.** `b_transport` checks write length only, then always copies four bytes for a successful read without validating `data_length` or `data_ptr` (`src/pvt_wrap.cpp:218-252`). A zero/short/null read can corrupt or crash the simulation instead of returning a protocol error.
2. **High — advertised mutable CCI timing is not mutable in behavior.** The constructor caches both CCI values (`src/pvt_wrap.cpp:56-71`), while transactions and ticks use only `access_delay_`/`tick_period_` (`src/pvt_wrap.cpp:150-153, 224-226`). The test merely reads handles (`test/pvt_wrap_tb.cpp:405-419`).
3. **Medium — the TLM contract is underspecified and weakly tested.** Neither streaming width nor byte enables are checked, alignment is only rejected incidentally by decode, DMI denial is not explicit, and the AXI extension is included but never observed (`src/pvt_wrap.cpp:218-270`).
4. **Medium — `transport_dbg` has unsafe/host-dependent accesses.** Debug reads copy four bytes regardless of length; writes dereference a `uint32_t*`, which can be unaligned or null (`src/pvt_wrap.cpp:256-271`).
5. **Medium — temporal correctness is not established.** The period-zero path, exact first-valid tick, repeated periods, disable/re-enable, LO/HI carry, and reset exactly adjacent to a scheduled tick are missing. The existing `counter_lo >= 5` assertion (`test/pvt_wrap_tb.cpp:240-244`) permits off-by-one behavior.
6. **Medium — output deassertion combinations are incomplete.** Tests do not independently disable process observation, voltage reset, or temperature enable after assertion, nor verify every output while reset is held.
7. **Low — constructor guard rails have no negative bench.** Negative CCI defaults are fatal in `src/pvt_wrap.cpp:63-68`, but no test reaches those branches.
8. **Low — coverage is model-line aggregate only.** `run_tests.sh:220-319` includes the test itself in the report and the shared gate measures aggregate `src/` line coverage. It does not require branch/protocol coverage.

## Missing scenarios

- Exact reset-level values on all four outputs while reset remains asserted.
- Every control-bit combination and deassertion transition.
- Period 0, 1, `0xffffffff`, and a period using the HI half; repeated rollover.
- Tick disabled then re-enabled; reset before, at, and after a scheduled tick.
- Exact annotated delay before and after mutable CCI changes.
- Read/write command, 1/2/4/8-byte lengths, null pointer where safely rejectable, alignment, byte enable, streaming width, ignore command, and last in-window/unmapped addresses.
- Debug transport malformed command/length/alignment/range behavior.
- Explicit DMI denial and AXI-extension-present transaction acceptance.
- Negative constructor values.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| PVT-FD-001 | Hold reset low, read all registers and sample every output. | All storage and outputs remain at reset values; no tick occurs. | `reset_proc`, `output_method` | Complete reset contract |
| PVT-FD-002 | Independently toggle process enable, observation enable, voltage reset, and temperature enable. | Each output/status asserts and deasserts only from its documented control. | `reg_write`, `update_status_regs`, `output_method` | Control/output matrix |
| PVT-TIME-001 | Program period 1 and count ticks at boundaries just before/at/after expiry. | Valid and captured count change on one precisely documented tick. | `tick_method` | No off-by-one |
| PVT-TIME-002 | Program HI:LO period crossing 32 bits, disable/re-enable, then reset near an event. | No stale valid/interrupt; restart begins from zero; reset cancels work. | `schedule_tick`, `reset_proc` | 64-bit and event behavior |
| PVT-TLM-001 | Frontdoor read/write matrix for command, length, alignment, byte enables, streaming width, and range. | Deterministic TLM error class; no buffer access on rejected requests. | `b_transport` validation | Protocol safety |
| PVT-TLM-002 | Send a legal request with an `smc_axi_extension`; query DMI. | Same functional result; DMI denied explicitly. | TLM sideband/DMI | Integration contract |
| PVT-TLM-003 | Exercise malformed and legal `transport_dbg` requests. | Exact byte count, no crash/unaligned dereference, no state change on reject. | `transport_dbg` | Debug protocol |
| PVT-CCI-001 | Mutate access delay between two reads and tick period before re-enabling count. | Next operations use the new values, or parameters are made immutable if runtime change is unsupported. | CCI/timing | Configuration truthfulness |
| PVT-CFG-001 | Construct with negative delay/tick values under a narrowly scoped fatal expectation. | Exact expected fatal report ID/message. | Constructor validation | Guard rails |

## Verdict

**Needs strengthening; not protocol-robust.** The basic register and output tests are meaningful, but malformed reads are unsafe, mutable CCI behavior is not real, and timing assertions are permissive. Frontdoor protocol and exact event-boundary tests should precede additional coverage work.
