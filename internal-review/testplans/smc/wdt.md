# SMC `wdt` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/wdt/include/wdt.h`
- `smc/peripherals/wdt/src/wdt.cpp`
- `smc/peripherals/wdt/test/wdt_tb.cpp`
- `smc/peripherals/wdt/test/wdt_tick_tb.cpp`
- `smc/peripherals/wdt/test/CMakeLists.txt`
- `smc/peripherals/wdt/CMakeLists.txt`
- `smc/peripherals/wdt/run_tests.sh`

No model or test was modified.

## Model behavior inventory

- Ports: MMIO target, active-low module reset, active-high core-reset gate, IRQ, sticky reset output.
- Registers: `CTRL`, `COUNT`, `SCALED_COUNT`, `FEED`, `KEY`, `CMP`; 1 KiB window with holes RAZ/WI.
- Access sizes: 4- and 8-byte normal MMIO; natural alignment by transfer length; 64-bit reads combine adjacent words; 64-bit writes split into words except special FEED+KEY handling.
- Locking: exact KEY magic unlocks; known protected writes consume lock; SCALED_COUNT write locks; holes leave lock unchanged; successful/failed feed semantics.
- Counter: 31-bit wrapping count, 4-bit scale, 16-bit scaled value/compare, always/awake/core-reset enable.
- Timeout effects: elapsed sets IP; optional reset-sticky; zero-compare clears count; feed clears count/sticky but not elapsed IP.
- Processes/events: reset, periodic tick event, zero-time output recompute; optional disabled auto-tick; debug-controlled tick/count.
- TLM: command/size/pointer/BE/streaming/range validation, mutable annotated delay, debug transport.
- CCI: immutable tick period, mutable access delay.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Reset/register lock/feed/CTRL semantics (`wdt_tb.cpp:201-438`) | Genuine/Partial | Frontdoor register accesses are real, but timeout setup relies heavily on direct count/tick hooks. |
| Timeout/scale/zero/awake behavior (`wdt_tb.cpp:233-335`) | Partial | Assertions are meaningful; `dbg_tick` and `dbg_set_count` bypass the production event path. |
| 64-bit MMIO and TLM negatives (`wdt_tb.cpp:442-497`) | Genuine | Uses target socket and explicit statuses, though same-beat FEED+KEY expected behavior needs external validation. |
| Debug transport (`wdt_tb.cpp:499-520`) | Coverage-only | Backdoor path and defensive rejection. |
| Direct register helpers (`wdt_tb.cpp:522-535`) | Coverage-only | Explicitly bypasses TLM size/window checks through public wrappers. |
| Auto-tick bench (`wdt_tick_tb.cpp`) | Genuine/Partial | Uses real tick events, but reads count through a debug accessor and samples exact event multiples loosely. |
| Dump (`wdt_tb.cpp:537`) | Coverage-only | Text path only. |

No private-public macro or source test-only branch was found.

## Artificial coverage mechanisms

- The header explicitly exposes direct `dbg_reg_read/write` “for unit tests” (`include/wdt.h:137-146`).
- Most timing behavior is forced by `dbg_tick` and `dbg_set_count` (`test/wdt_tb.cpp:233-335, 352-359, 427-438`).
- Defensive internal helper paths are tested directly (`test/wdt_tb.cpp:522-535`), including an 8-byte read at `0x3fc` that no valid frontdoor transfer permits.
- CCI reports are demoted to display-only (`test/wdt_tb.cpp:552-553`, `test/wdt_tick_tb.cpp:167-168`).

## Findings

1. **High — the default and ASan runners omit the auto-tick bench.** `run_tests.sh` locates only `wdt_tb` for normal/ASan execution (`run_tests.sh:190-194, 218-267, 418-420`). `wdt_tick_tb` runs only through CTest or coverage. Production temporal behavior is therefore absent from the standard and sanitizer gates.
2. **High — coverage is inflated by direct register internals.** Public wrappers call private `reg_read/reg_write` without TLM validation (`include/wdt.h:137-146`), and the test explicitly uses them for defensive coverage (`test/wdt_tb.cpp:522-535`).
3. **High — debug transport accepts invalid boundary/alignment cases.** It checks only starting address, pointer, and length (`src/wdt.cpp:484-515`); an 8-byte debug transfer can cross the window and misaligned 4/8-byte transfers can succeed.
4. **Medium — streaming width greater than transfer length is accepted.** Normal validation rejects only `streaming_width < len` (`src/wdt.cpp:405-417`), unlike a fixed register transaction contract requiring equality.
5. **Medium — 64-bit FEED+KEY behavior is asserted from the implementation, not an independent architectural oracle.** The source special-cases this pair (`src/wdt.cpp:456-473`), and the test expects the same-beat KEY not to authorize FEED (`test/wdt_tb.cpp:442-469`). This must be checked against the actual TileLink/register adapter semantics.
6. **Medium — counter boundary behavior is missing.** No test covers 31-bit wrap, scale 15, scaled 16-bit truncation, compare `0xffff`, compare zero over repeated ticks, or sticky behavior across wrap/feed races.
7. **Medium — auto-tick races are weakly tested.** Reset, core-reset, feed, and control writes coincident with tick events are not sampled before/at/after the event boundary.
8. **Medium — negative access delay is not rejected and delay behavior is untested.** Only tick period is validated (`src/wdt.cpp:55-57`); the mutable access-delay handle is never introspected or measured.
9. **Low — no explicit DMI denial or AXI-extension test exists.** The canonical extension is included but not consumed; successful transactions do not explicitly set DMI false.
10. **Low — output initialization without an initial reset pulse is not tested.** The output method runs only when recompute is scheduled.

## Missing scenarios

- Auto-tick under ASan/default runner, with always and awake/core-reset gating.
- Exact event-boundary reset/feed/control races and no double tick after reset rearm.
- 31-bit count wrap, all scale values, scaled truncation, compare min/max, repeated zerocmp.
- IP set/clear while currently elapsed versus no longer elapsed; sticky and feed precedence.
- Correct lock consumption for every known register, bad key/feed, holes, 32-/64-bit split writes.
- Independent RTL-derived 64-bit FEED+KEY semantics.
- Normal/debug full range, boundary crossing, alignment, BE, null, both streaming mismatch directions, command, DMI, AXI extension.
- Exact mutable delay and invalid CCI parameters.
- Initial outputs when reset starts high/low.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| WDT-TIME-001 | Enable always mode and advance actual simulation over N−1, N, N+1 tick boundaries. | Count and outputs change exactly on documented edges. | `tick_event_`, `tick_method` | Production ticking |
| WDT-TIME-002 | Toggle core reset in awake mode around a tick; repeat with always set. | Awake pauses only in core reset; always overrides without lost/double ticks. | `counting_enabled` | Gating |
| WDT-RACE-001 | Feed, reset, CTRL write, and CMP write immediately before/at an elapsed tick. | Deterministic IP/sticky/count precedence matching RTL. | event/register side effects | Race semantics |
| WDT-COUNT-001 | Load `0x7ffffffe`, tick through wrap at scales 0 and 15. | 31-bit wrap and scaled 16-bit value are exact. | counter/scaler | Boundaries |
| WDT-CMP-001 | Compare 0, 1, and `0xffff` with zero-compare/rsten combinations. | IP, sticky, and reset-to-zero behavior repeats exactly. | elapsed side effects | Compare extremes |
| WDT-LOCK-001 | For each protected register, perform locked, good-key, bad-key, and hole sequences. | Only authorized writes commit; lock consumption matches independent truth table. | register decode | Lock protocol |
| WDT-64-001 | Replay RTL/bus-adapter-derived 64-bit accesses over CTRL, COUNT, FEED+KEY, and CMP pairs. | Per-lane ordering and authorization match hardware, not current source assumptions. | 64-bit split path | Bus-width semantics |
| WDT-TLM-001 | Normal protocol matrix including SW greater/less than length, null, BE, alignment, and end crossing. | Exact responses; no state/delay side effects on rejects. | `b_transport` | Protocol safety |
| WDT-DBG-001 | Equivalent debug matrix, especially 8 bytes at `0x3f8/0x3fc` and misalignment. | Reject crossing/misaligned transfers; exact byte count for legal requests. | `transport_dbg` | Debug boundaries |
| WDT-RST-001 | Start simulation with reset high and low, then pulse during active IRQ/sticky. | Outputs always initialized and reset clears all state/event residue. | reset/output methods | Reset lifecycle |
| WDT-CCI-001 | Invalid tick/access delays; mutate access delay and measure requests. | Narrow fatal errors; next request uses exact new delay. | constructor/CCI | Configuration |
| WDT-INT-001 | Attach canonical AXI extension and query DMI. | Functional behavior unchanged; DMI explicitly denied. | TLM integration | AXI/DMI |
| WDT-BUILD-001 | Run both binaries in Release, ASan, and coverage phases and propagate either exit code. | Every gate covers real auto-tick behavior. | `run_tests.sh` | Test orchestration |

## Verdict

**Functional semantics are substantially exercised, but the quality gate overstates confidence.** Most timeout tests bypass time, direct register helpers inflate coverage, debug boundaries are permissive, and the real auto-tick bench is skipped by default and ASan runs. Temporal and protocol sign-off should remain open.
