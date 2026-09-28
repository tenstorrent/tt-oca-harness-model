# AON Timer SystemC/TLM Test Audit and Test Plan

## Scope and evidence basis

Static audit only. Runtime pass rate and measured coverage were not established.

Audited files:

- Model: `include/aon_timer.h`, `aon_timer_base.h`, `aon_timer_interface.h`, `aon_timer_register.h`, `src/aon_timer.cpp`, `src/aon_timer_base.cpp`
- Tests: `test/inc/testbench.h`, `aon_timer_basetest.h`, `aon_timer_test.h`, `test/src/testbench.cpp`, `aon_timer_basetest.cpp`, `aon_timer_test.cpp`
- Wiring: `CMakeLists.txt`, `run_tests.sh`, `config/accellera_config.ini`
- Shared transport: `common/include/reg_file.h`

## Model behavior inventory

- Fourteen-register block: WO alert/test registers, RW wakeup/watchdog controls/thresholds/counters, RW0C lock/cause and RW1C interrupt state.
- 64-bit wakeup counter with 12-bit prescaler, software HI/LO access, threshold edge latch and wraparound.
- 32-bit watchdog counter with bark/bite thresholds, enable, sleep pause, software preload, edge-latched bark and reset-request latch.
- Independent SYS and AON active-low resets. SYS reset clears all state/registers/locks; AON reset clears AON outputs/cause/bite while preserving SYS state.
- Interrupt, NMI, wakeup, reset request, fatal alert and RACL output driving through one deferred `SC_METHOD`.
- Wakeup and watchdog `SC_THREAD`s with timed waits interrupted by control/reset/escalation/sleep events.
- Lifecycle escalation halts both timers; sleep only pauses watchdog when configured.
- CDC annotation: writes add two AON cycles to a quantum keeper; reads synchronize pending local time.
- `EnableRacl` constructor flag and ports exist, but no access-control implementation updates `m_racl_error_active`.
- Inherited register transport supports partial accesses, byte enables and debug transport, returns OK even on unsupported/reserved operations, ignores streaming width, and has no DMI.

## Existing scenario-to-function matrix

| Existing scenario group | Model paths reached | Quality |
|---|---|---|
| Binding, reset values, RW/WO and all register reset/access semantics | base register map and callbacks | Genuine, though binding checks are mostly “reached here” |
| Reserved masks, W1C/RW0C, REGWEN lock and CDC readback | callback layer and quantum keeper | Genuine for values; Partial for CDC timing |
| Full/AON-only reset and bite-driven reset response | inline reset thread and outputs | Genuine |
| Wakeup enable/prescaler/64-bit threshold/write/read/wrap/carry | wakeup thread and threshold latch | Genuine/Partial |
| Watchdog enable/bark/bite/preload/pause/wrap/volatile reads | watchdog thread and bark/bite latches | Genuine |
| Interrupt test, W1C, retrigger, NMI coupling and wakeup-cause independence | interrupt/output helpers | Genuine |
| Wakeup request persistence and bite latch | cause/bite state | Genuine |
| Escalation halt/preservation/no-bite/register access | escalation method and both threads | Genuine |
| RACL suites | only disabled/no-enforcement behavior | Partial; EnableRacl=true is not tested |
| Invalid AON clock edge case | zero-frequency branches | Coverage-only |

## Artificial coverage shortcuts and weaknesses

1. **Critical — test codifies an event/timer bug as desired behavior.** `test/src/testbench.cpp:5029-5055` says a same-value WKUP_CTRL write wakes `wait(tick_delay, event)` and causes an immediate increment. The implementation at `src/aon_timer.cpp:1533-1555` cannot distinguish timeout from event and increments whenever enable remains true. A prescaler reset should restart the period without a count; `testbench.cpp:5100-5124` explicitly requires the erroneous immediate `+1`.
2. **High — stale watchdog expectations are tolerated.** `test/src/testbench.cpp:4844-4860` says the autonomous watchdog thread is not implemented although it is. At `:4920-4930`, nonzero watchdog progress is only logged as a NOTE and does not fail, weakening the claimed concurrency check.
3. **High — fatal alert “pulse” requires an unrelated second bus operation to deassert.** `testbench.cpp:10310-10418` documents and accepts that ALERT_TEST leaves `fatal_fault` high until INTR_TEST happens to notify the shared output event. This is not a self-contained pulse and makes the test adapt to the implementation defect instead of checking pulse width.
4. **High — RACL coverage is mislabeled and does not instantiate enabled RACL.** `testbench.cpp:10505-10603` names an EnableRacl=1 test but explicitly runs `EnableRacl=0`. The model states the active flag is always false at `include/aon_timer.h:731-740`; no denial path exists.
5. **Medium — invalid-clock test has no assertion.** `testbench.cpp:11455-11500` toggles zero frequency and unconditionally passes if execution reaches the end. It does not prove counters remain stable, threads wake after frequency restoration, or response timing.
6. **Medium — CDC test checks immediate readback, not annotated time.** `testbench.cpp:2185-2254` compares values only. It never measures `sc_time_stamp`, local quantum time or expected two-cycle latency, so removing timing while retaining synchronous storage would pass.
7. **Medium — broad timing ranges accept multiple outcomes.** Numerous counter tests use `>=`, small ranges or startup offsets; e.g. wrap accepts any low word `<=20` at `testbench.cpp:4748-4765`. These are reasonable scheduler tolerances but do not prove exact event cancellation/order.
8. **Medium — tests explicitly split writes around an alleged quantum-keeper throw.** `testbench.cpp:1524-1537` structures INTR_TEST writes to avoid a callback throwing during `m_qk.sync()`. A test should expose and fail a transport-context synchronization problem, not route around it.
9. **Low — CMake had to override `NDEBUG` because tests use `assert`.** `CMakeLists.txt:165-169` keeps assertions live, which is correct wiring, but demonstrates that test verdicts depend on build flags and should migrate to explicit failure accounting.
10. **Low — ASan runner has no log/leak gate.** `run_tests.sh:66-73` runs the ASan binary like a normal test.

No private-public macro or direct callback call was found.

## Missing or inadequately proven scenarios

- Correct event-vs-timeout handling for both timer threads when same-value controls, sleep, escalation and reset interrupt a pending period.
- A self-deasserting fatal alert pulse with measured width and no unrelated event.
- EnableRacl=true allowed/denied accesses, policy changes, error output and reset.
- TLM unsupported command, null pointer, zero length, out-of-range/cross-map accesses, exact response status and no mutation.
- Partial/unaligned accesses and byte-enable semantics for W1C, RW0C, split 64-bit values and side-effect registers. The 8-bit helper exists but is never called.
- `transport_dbg` side-effect policy and DMI behavior.
- Streaming width smaller than transfer length.
- Exact annotated CDC delay, zero-frequency behavior, and dynamic frequency changes while a timer is pending.
- Reset, disable, escalation or sleep at one delta before timeout; prove no extra count.
- Simultaneous wakeup and bark source setting/clearing, exact delta ordering of all ports, and merged events.
- Interrupt/cause behavior over wrap and repeated crossings for prescaler zero and nonzero.
- Thread behavior when AON frequency stays zero and is later restored without another control write.
- SYS reset and AON reset asserted/deasserted in different orders and simultaneously during pending bite/bark.

## Proposed testcases

| ID | Setup / stimulus | Required assertions | Paths | Feature |
|---|---|---|---|---|
| AON-TMR-001 | Enable prescaler=9, wait half period, rewrite same value | No immediate increment; next count exactly one full restarted period later | wakeup thread/control event | Event vs timeout |
| AON-TMR-002 | Interrupt pending wakeup/watchdog waits with disable, reset, escalation and sleep at `period-epsilon` | No ghost count/event; resume starts documented remaining/new period | both timer threads | Concurrency/temporal |
| AON-ALR-001 | Write ALERT_TEST once and sample deltas/time | One bounded pulse that deasserts without another transaction | alert callback/output driver | Port/event |
| AON-RACL-001 | Instantiate `EnableRacl=true`; issue allowed and denied reads/writes | Denied access response/data/no side effects; `racl_error` assertion/clear/reset | missing RACL path | Security/TLM |
| AON-TLM-001 | IGNORE/null/zero-length/OOB/cross-boundary payloads | Exact error response and no mutation/events | inherited transport | Command/address/length |
| AON-TLM-002 | Byte writes and byte enables to CTRL, counters, INTR_STATE, CAUSE, REGWEN | Independent byte-lane expected state; no clearing from disabled lanes | callbacks | Byte enable/unaligned |
| AON-TLM-003 | Debug accesses to side-effect and volatile registers; DMI request | Explicit debug side-effect policy and returned byte count; DMI false | inherited debug | Debug/DMI |
| AON-CDC-001 | Timestamp writes and immediate reads at several AON frequencies | Exactly two AON cycles annotated/synchronized; zero-frequency is zero delay | `compute_cdc_delay`, reads | Annotated delay |
| AON-CLK-001 | Enable at 0 Hz, later set 200 kHz without rewriting CTRL | Defined restart behavior; no count while stopped; exact first tick after restart | zero-frequency wait branches | Dynamic clock port |
| AON-RST-001 | Assert SYS/AON resets in every ordering during active bark/bite/wakeup | Exact domain-specific state/ports; no stale tick after reset | reset thread/output driver | Reset domains |
| AON-INTR-001 | Cross wakeup and bark thresholds same tick, clear sources independently | Exact INTR_STATE/NMI/wkup cause and persistence | threshold helpers | Simultaneous events |

## Coverage quality verdict

**Verdict: Extensive scenario count but materially compromised temporal coverage.** Most register, reset, interrupt and timer semantics have real assertions. The suite nevertheless bakes an event-interruption bug into its expected result, tolerates stale watchdog assumptions, works around a possible transport-context sync failure, and has assertion-free invalid-clock coverage. EnableRacl=true is completely absent. Coverage wiring uses the shared target and explicitly preserves assertions, but no measured coverage was proven.
