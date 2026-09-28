# SMC `reset_unit` test audit and remediation plan

## Audit scope

Audited:

- `smc/peripherals/reset_unit/include/reset_unit.h`
- `smc/peripherals/reset_unit/src/reset_unit.cpp`
- `smc/peripherals/reset_unit/test/reset_unit_tb.cpp`
- `smc/peripherals/reset_unit/test/reset_unit_neg_tb.cpp`
- `smc/peripherals/reset_unit/test/CMakeLists.txt`
- `smc/peripherals/reset_unit/CMakeLists.txt`
- `smc/peripherals/reset_unit/run_tests.sh`

The separately implemented `straps` target in `reset_unit.cpp` is included. No model or test was modified.

## Model behavior inventory

- Inputs: powergood; cold, cool, fuse, and external-WDT resets; first/second WDT timeout; isolate pin; FLR active; subsystem completion; captured straps; JTAG override API.
- Outputs: stable powergood; cold/primary/core/WDT/cool resets; skip-memory-repair; sync IRQ; isolate bitmap; subsystem configuration; per-subsystem reset-control bundles.
- Registers: subsystem config/locks/cold/warm resets/hold fields/completion/force-clock; sync; isolate SW/pin/SMC enables and visibility; FLR delay/duration counters. In-window holes are RAZ/WI.
- Register policy: sticky woset locks, lock-gated writes, RO no-op writes, reset-domain-specific clearing.
- Processes/events: input recomputation, start-of-simulation seeding, FLR assert/deassert events, sole output driver, primary/cold reset-edge detection.
- Temporal behavior: rising FLR sets isolate latch; optional delayed active-low cool pulse; cold reset cancels scheduled FLR work; cool reset clears primary-domain state but retains cold-domain state.
- TLM: word-aligned 32-bit target, explicit command/length/alignment/streaming/byte-enable/range responses, mutable access delay, debug transport.
- `straps`: two RO words exposing a masked 61-bit captured value, with a separate target socket.

## Existing test classification

| Existing test area | Classification | Rationale |
|---|---|---|
| Reset derivation, WDT2, fuse, cool reset, FLR pulse (`reset_unit_tb.cpp:240-478`) | Genuine | Drives public inputs and observes outputs/registers through the socket. |
| Register access and lock semantics (`reset_unit_tb.cpp:254-280, 580-597`) | Genuine | Specific frontdoor values prove woset and lock gating. |
| Per-subsystem controls (`reset_unit_tb.cpp:282-304`) | Partial | Checks some output signals but relies heavily on `dbg_ss_reset_ctrl`. |
| Isolate/sync/straps (`reset_unit_tb.cpp:319-339, 410-454, 478-486`) | Genuine | Mostly observable through public outputs and separate frontdoor target. |
| Debug API and dump tests (`reset_unit_tb.cpp:489-509, 539-547, 681-691`) | Coverage-only | Exercise backdoors, text formatting, and defensive guards rather than firmware behavior. |
| Full register sweep and header tracing (`reset_unit_tb.cpp:550-679`) | Coverage-only/Partial | Explicitly labeled coverage sweep; some round-trips are useful, but `operator<<`/VCD tracing adds no functional assurance. |
| Negative TLM bench (`reset_unit_neg_tb.cpp:151-196`) | Genuine/Partial | Good response-status matrix, but runs with unbound ports and suppresses SystemC errors. |
| Constructor fatal probes (`reset_unit_neg_tb.cpp:124-139`) | Partial | Reaches guards, but `expect_fatal` accepts any exception. |

No private-public macro or test-only model branch was found.

## Artificial coverage mechanisms

- Direct backdoors: `dbg_read`, `dbg_ss_reset_ctrl`, `jtag_ctrl`, and `dump_state` are exercised at `test/reset_unit_tb.cpp:289-303, 489-509, 539-547, 611-621, 681-691`.
- Explicit coverage sweep: `test/reset_unit_tb.cpp:550-578`.
- Header-only coverage: stream/VCD trace smoke at `test/reset_unit_tb.cpp:666-679`.
- Broad exception acceptance: `test/reset_unit_neg_tb.cpp:65-74`.
- Report demotion: both benches replace `SC_ERROR` actions with display-only behavior (`test/reset_unit_tb.cpp:703`, `test/reset_unit_neg_tb.cpp:116`).

## Findings

1. **High — tests can pass despite SystemC errors.** Display-only `SC_ERROR` handling removes stop/throw behavior and does not increment `g_failures` (`test/reset_unit_tb.cpp:703`, `test/reset_unit_neg_tb.cpp:116`). The negative bench intentionally leaves ports unbound (`test/reset_unit_neg_tb.cpp:142-149`), making this suppression material.
2. **High — valid-looking transactions can dereference a null buffer.** `b_transport` validates size/protocol but not `data_ptr` before `memcpy` (`src/reset_unit.cpp:488-538`). The same issue exists in `straps` (`src/reset_unit.cpp:664-699`).
3. **Medium — byte-enable rejection has a loophole.** A non-null byte-enable pointer with zero byte-enable length is accepted because the check uses `ptr != nullptr && length != 0` (`src/reset_unit.cpp:505-508`).
4. **Medium — debug transport treats an unknown command as a write.** `transport_dbg` uses an `else` rather than an explicit write-command check (`src/reset_unit.cpp:555-565`). `straps::transport_dbg` similarly returns four bytes for non-read commands (`src/reset_unit.cpp:701-714`).
5. **Medium — important reset sources and outputs are not covered.** External WDT reset, WDT first-timeout non-effect, powergood loss, all synchronized output mirrors, all hold fields, and simultaneous reset/JTAG combinations lack independent public-output checks.
6. **Medium — FLR temporal checks are permissive.** The test samples well inside/after windows (`test/reset_unit_tb.cpp:441-447`) rather than at exact delay and release boundaries; retrigger, zero delay, cold-reset cancellation, and simultaneous pin/JTAG cool reset are absent.
7. **Medium — access-delay mutation is not behaviorally asserted.** The CCI test changes the handle but never checks the transaction's annotated delay (`test/reset_unit_tb.cpp:515-528`).
8. **Closed (2026-09-24) — negative-bench ASan process failures are no longer discarded.** Combined child exits fail the ASan phase. Display-only `SC_ERROR` handling (finding 1) remains open.
9. **Low — DMI and AXI sideband behavior are untested.** The canonical extension is included but unused; no explicit DMI callback/denial test exists.
10. **Low — the coverage gate is aggregate source-line coverage.** Coverage-only backdoor/sweep sections materially contribute to passing it and do not prove branch or integration quality.

## Missing scenarios

- Each reset source alone and in combinations: powergood, cold, cool, fuse, external WDT, WDT second timeout, JTAG overrides.
- First WDT timeout must be explicitly shown not to alter reset outputs.
- Every output mirror, every hold field, all configured subsystem indices including first/last and a reduced CCI subsystem count.
- Initial reset asserted before `sc_start`, register writes attempted before start, and reset assertion coincident with FLR events.
- Exact FLR assertion/release times, zero delay, zero duration, retrigger while active, and cancellation by cold reset.
- Every isolate visibility bit and isolate pin-qualified reset case through public outputs/frontdoor reads.
- Null data pointer, non-null BE with zero length, streaming widths, DMI, AXI extension, and debug unknown command.
- `straps` streaming width, null pointer, byte enables, write-ignore, debug command, and exact access delay.

## Proposed testcases

| ID | Stimulus | Expected result | Path exercised | Feature proved |
|---|---|---|---|---|
| RST-FD-001 | Drive each physical reset source independently, including external WDT and powergood. | Only documented reset tree nodes assert; all domain mirrors agree. | `derive`, `input_method`, `output_method` | Reset equations |
| RST-FD-002 | Toggle first and second WDT timeout separately. | First is informational; second asserts core/WDT only. | `derive` | WDT integration |
| RST-FD-003 | Program every subsystem field for bit 0 and last configured bit; observe only `ss_reset_ctrl_o`. | Exact bundle fields, lock behavior, and index bounds. | register writes/output loop | Public subsystem interface |
| RST-DOM-001 | Populate both reset domains, apply cold/cool/FLR resets. | Exact primary/cold retention matrix. | `clear_primary_regs`, `clear_cold_regs` | Reset-domain ownership |
| RST-FLR-001 | Use event-boundary sampling at delay−epsilon, delay, release−epsilon, release. | Pulse edges occur exactly once at programmed times. | FLR events/methods | Temporal contract |
| RST-FLR-002 | Retrigger FLR and assert cold reset while events are pending. | Defined retrigger policy; cold reset cancels stale events and deasserts cool output. | `flr_kick`, event cancellation | Race handling |
| RST-ISO-001 | Cover SW, pin, SMC, and combined isolate sources for first/last bits. | OR logic, visibility bits, and skip-repair output match independent oracle. | isolate output logic | Isolation behavior |
| RST-TLM-001 | Protocol matrix including null pointer and BE pointer with zero BE length. | No dereference on reject; exact response classes. | `b_transport` | TLM safety |
| RST-DBG-001 | Debug read/write/ignore, malformed range/alignment, and straps debug matrix. | Only explicit read/write accepted; exact byte counts. | both `transport_dbg` methods | Debug protocol |
| RST-CCI-001 | Mutate access delay and compare annotated delays. | Next transaction reflects new mutable value exactly. | mutable CCI use | Timing configuration |
| RST-CFG-001 | Invalid constructor values with report-ID/message-specific expectations. | Only the intended fatal satisfies the test. | constructor guards | Reliable negative tests |

## Verdict

**Broad functional coverage, but test trustworthiness is weakened.** The frontdoor reset/FLR/isolation scenarios are substantial. The ASan `|| true` hole is **closed (2026-09-24)**. Display-only error suppression, backdoor-heavy coverage sections, permissive timing checks, and null-buffer/debug-command gaps still prevent a robust sign-off.
